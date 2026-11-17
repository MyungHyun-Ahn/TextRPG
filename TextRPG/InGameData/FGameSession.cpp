#include "pch.h"
#include "Common/GameDataEnums.g.h"
#include "InGameDataAll.h"
#include "InGameData.h"
#include "FSaveDataStore.h"
#include "FGameSession.h"
#include "Combat/FCombat.h"
#include "Scene/FDataScene.h"
#include "Scene/TSceneManager.h"
#include "Scene/FTextRPGSceneManager.h"

namespace TextRPG
{
	namespace
	{
		struct SPlainBuffer final
		{
			std::vector<std::uint8_t> Bytes;
			~SPlainBuffer()
			{
				if (!Bytes.empty()) SecureZeroMemory(Bytes.data(), Bytes.size());
			}
		};

		bool CanOverwriteProgressVersion(const std::vector<std::uint8_t>& bytes, std::string& error)
		{
			if (bytes.size() < 8) return true;
			const auto number = [&](std::size_t at)
			{
				std::uint32_t value = 0;
				for (std::size_t i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(bytes[at + i]) << (8 * i);
				return value;
			};
			const bool isFramed = bytes[0] == 'C' && bytes[1] == 'G' && bytes[2] == 'S' && bytes[3] == 'A';
			const bool isLegacy = bytes[0] == 'I' && bytes[1] == 'G' && bytes[2] == 'D' && bytes[3] == '1';
			if ((isFramed || isLegacy) && number(4) > 1)
			{
				error = "Refusing to overwrite a newer progress format version.";
				return false;
			}
			if (isFramed && bytes.size() >= 12 && number(8) > GameData::InGameData::kSchemaVersion)
			{
				error = "Refusing to overwrite a newer progress schema version.";
				return false;
			}
			return true;
		}

		void CopySceneError(const std::wstring& sceneError, std::string& error)
		{
			// 개발자 진단 경계만 UTF-8로 변환한다. 게임 화면에는 현지화된 wide 문구를 사용한다.
			if (sceneError.empty() || sceneError.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			{
				error = "Scene preparation or commit failed.";
				return;
			}
			const int characters = static_cast<int>(sceneError.size());
			const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sceneError.data(), characters,
				nullptr, 0, nullptr, nullptr);
			if (bytes <= 0)
			{
				error = "Could not encode the scene diagnostic.";
				return;
			}
			std::string encoded(static_cast<std::size_t>(bytes), '\0');
			if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sceneError.data(), characters,
				encoded.data(), bytes, nullptr, nullptr) != bytes)
			{
				error = "Could not encode the scene diagnostic.";
				return;
			}
			error.swap(encoded);
		}
	}

	FGameSession::FGameSession(const GameData::FGameDataStore& store)
		: m_store(store)
	{
	}

	FGameSession::~FGameSession() = default;

	const InGameData* FGameSession::GetProgress() const noexcept
	{
		return m_progress.get();
	}

	bool FGameSession::ReadCandidate(std::unique_ptr<InGameData>& candidate, std::string& error) const
	{
		std::filesystem::path directory;
		if (!ResolveSaveDataDirectory(directory, error)) return false;
		SPlainBuffer plain;
		return FSaveDataStore(directory).Load(plain.Bytes, error) &&
			InGameData::Deserialize(plain.Bytes, m_store, candidate, error);
	}

	bool FGameSession::CanContinue(FTextRPGSceneManager& scenes, FGameUiBridge& ui, std::string& error) const
	{
		try
		{
			std::unique_ptr<InGameData> candidate;
			if (!ReadCandidate(candidate, error)) return false;
			std::unique_ptr<FDataScene> scene;
			std::wstring sceneError;
			if (!scenes.PrepareSceneById(candidate->GetSceneId(), ui, candidate.get(), scene, sceneError))
			{
				CopySceneError(sceneError, error);
				return false;
			}
			error.clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}

	EGameStartResult FGameSession::NewGame(std::uint32_t sceneId, FTextRPGSceneManager& scenes, FGameUiBridge& ui,
		std::string& error)
	{
		if (m_hasPendingStart)
		{
			error = "A progress publication is already pending.";
			return EGameStartResult::Failed;
		}
		try
		{
			std::unique_ptr<InGameData> candidate;
			if (!InGameData::CreateNew(m_store, sceneId, candidate, error)) return EGameStartResult::Failed;
			std::unique_ptr<FDataScene> scene;
			std::wstring sceneError;
			if (!scenes.PrepareSceneById(sceneId, ui, candidate.get(), scene, sceneError) ||
				!scenes.CommitProgress(scene, m_progress, candidate, sceneError))
			{
				CopySceneError(sceneError, error);
				return EGameStartResult::Failed;
			}
			m_hasPendingStart = true;
			m_pendingAutosave = true;
			error.clear();
			return EGameStartResult::Queued;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return EGameStartResult::Failed;
		}
	}

	bool FGameSession::Continue(FTextRPGSceneManager& scenes, FGameUiBridge& ui, std::string& error)
	{
		if (m_hasPendingStart)
		{
			error = "A progress publication is already pending.";
			return false;
		}
		try
		{
			std::unique_ptr<InGameData> candidate;
			if (!ReadCandidate(candidate, error)) return false;
			std::unique_ptr<FDataScene> scene;
			std::wstring sceneError;
			if (!scenes.PrepareSceneById(candidate->GetSceneId(), ui, candidate.get(), scene, sceneError) ||
				!scenes.CommitProgress(scene, m_progress, candidate, sceneError))
			{
				CopySceneError(sceneError, error);
				return false;
			}
			m_hasPendingStart = true;
			m_pendingAutosave = false;
			error.clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}

	EGameStartResult FGameSession::TakeStartResult(FTextRPGSceneManager& scenes, std::string& error)
	{
		if (!m_hasPendingStart) return EGameStartResult::None;
		bool published = false;
		std::wstring sceneError;
		if (!scenes.TakeProgressResult(m_progress, published, sceneError)) return EGameStartResult::Queued;
		m_hasPendingStart = false;
		if (!published)
		{
			CopySceneError(sceneError, error);
			return EGameStartResult::Failed;
		}
		// 오직 실제 게시를 확인한 뒤 자동 저장한다. Continue는 기존 슬롯을 다시 쓰지 않는다.
		if (m_pendingAutosave && !Save(error)) return EGameStartResult::StartedSaveFailed;
		error.clear();
		return EGameStartResult::Started;
	}

	bool FGameSession::Travel(std::uint32_t sceneId, FTextRPGSceneManager& scenes, FGameUiBridge& ui, std::string& error)
	{
		if (!m_progress || m_hasPendingStart || scenes.HasPendingTransition() || scenes.IsQuitting())
		{
			error = "Travel conflicts with absent progress or a pending operation";
			return false;
		}
		try
		{
			SPlainBuffer plain;
			std::unique_ptr<InGameData> candidate;
			if (!m_progress->Serialize(plain.Bytes, error) || !InGameData::Deserialize(plain.Bytes, m_store, candidate, error) ||
				!FCombat::Travel(m_store, *candidate, sceneId, error)) return false;
			std::unique_ptr<FDataScene> scene;
			std::wstring sceneError;
			if (!scenes.PrepareSceneById(sceneId, ui, candidate.get(), scene, sceneError) ||
				!scenes.CommitProgress(scene, m_progress, candidate, sceneError))
			{
				CopySceneError(sceneError, error);
				return false;
			}
			m_hasPendingStart = true;
			m_pendingAutosave = false;
			error.clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}

	bool FGameSession::BasicAttack(FTextRPGSceneManager& scenes, std::string& error)
	{
		if (!m_progress || m_hasPendingStart || scenes.HasPendingTransition() || scenes.IsQuitting())
		{
			error = "Attack conflicts with absent progress or a pending operation";
			return false;
		}
		return FCombat::BasicAttack(m_store, *m_progress, error);
	}

	bool FGameSession::Save(std::string& error) const
	{
		if (!m_progress)
		{
			error.clear();
			return true;
		}
		try
		{
			std::filesystem::path directory;
			if (!ResolveSaveDataDirectory(directory, error)) return false;
			FSaveDataStore store(directory);
			SPlainBuffer existing;
			std::string probeError;
			// 인증된 미래 버전만 선행 거부한다. 누락/손상 슬롯의 명시 저장 정책은 기존 서비스가 유지한다.
			// 슬롯 쓰기는 게임 스레드에서 직렬화한다. 이 읽기 선행 검사는 다중 프로세스 CAS가 아니다.
			if (store.Load(existing.Bytes, probeError))
			{
				if (!CanOverwriteProgressVersion(existing.Bytes, error)) return false;
				const auto& bytes = existing.Bytes;
				const bool isLegacy = bytes.size() >= 8 && bytes[0] == 'I' && bytes[1] == 'G' &&
					bytes[2] == 'D' && bytes[3] == '1';
				const bool isOldFramed = bytes.size() >= 12 && bytes[0] == 'C' && bytes[1] == 'G' &&
					bytes[2] == 'S' && bytes[3] == 'A' && bytes[8] == 1 && bytes[9] == 0 && bytes[10] == 0 && bytes[11] == 0;
				// 인증된 과거 콘텐츠를 해석할 수 없으면 명시 저장도 이전 슬롯을 보호한다.
				if (isLegacy || isOldFramed)
				{
					std::unique_ptr<InGameData> migrated;
					if (!InGameData::Deserialize(bytes, m_store, migrated, error)) return false;
				}
			}
			SPlainBuffer plain;
			return m_progress->Serialize(plain.Bytes, error) && store.Save(plain.Bytes, error);
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}
}
