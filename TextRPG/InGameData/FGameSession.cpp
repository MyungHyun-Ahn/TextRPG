#include "pch.h"
#include "Common/GameDataEnums.g.h"
#include "InGameData.h"
#include "FSaveDataStore.h"
#include "FGameSession.h"
#include "Scene/FDataScene.h"
#include "Scene/FSceneManager.h"

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

	bool FGameSession::CanContinue(FSceneManager& scenes, FGameUiBridge& ui, std::string& error) const
	{
		try
		{
			std::unique_ptr<InGameData> candidate;
			if (!ReadCandidate(candidate, error)) return false;
			std::unique_ptr<FDataScene> scene;
			std::wstring sceneError;
			if (!scenes.PrepareSceneById(candidate->GetSceneId(), ui, {}, candidate.get(), scene, sceneError))
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

	EGameStartResult FGameSession::NewGame(const std::wstring& sceneId, FSceneManager& scenes, FGameUiBridge& ui,
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
			if (!scenes.PrepareSceneById(sceneId, ui, {}, candidate.get(), scene, sceneError) ||
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

	bool FGameSession::Continue(FSceneManager& scenes, FGameUiBridge& ui, std::string& error)
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
			if (!scenes.PrepareSceneById(candidate->GetSceneId(), ui, {}, candidate.get(), scene, sceneError) ||
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

	EGameStartResult FGameSession::TakeStartResult(FSceneManager& scenes, std::string& error)
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
			SPlainBuffer plain;
			return m_progress->Serialize(plain.Bytes, error) && FSaveDataStore(directory).Save(plain.Bytes, error);
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}
}
