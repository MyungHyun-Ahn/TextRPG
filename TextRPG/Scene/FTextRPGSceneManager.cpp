#include "pch.h"
#include "Scene/TSceneManager.h"
#include "FTextRPGSceneManager.h"

#include "Common/GameDataEnums.g.h"
#include "UI/FUIManager.h"
#include "UI/FGameUiBridge.h"
#include "FDataScene.h"
#include "Rendering/FFrameBuffer.h"
#include "Scene/SceneData.g.h"
#include "GameData/FGameDataStore.h"
#include "InGameDataAll.h"
#include "InGameData/InGameData.h"
#include "Localization/Localization.h"

namespace TextRPG
{
	bool IScene::AllowInput(const SInputEvent& event, SSceneAction& response)
	{
		const bool dataScene = dynamic_cast<FDataScene*>(this) != nullptr;
		if (m_inputPolicy && !m_isHandlingInputFrame && !m_inputPolicy->FilterSceneInput(event, dataScene, response)) return false;
		return !IsTransitionRequested() || dataScene;
	}

	bool IScene::AllowInputFrame(const std::vector<SInputEvent>& events, std::vector<SInputEvent>& allowed,
		const std::vector<SInputEvent>*& selected, SSceneAction& response)
	{
		return !m_inputPolicy || m_inputPolicy->FilterSceneInputFrame(events, dynamic_cast<FDataScene*>(this) != nullptr,
			IsTransitionRequested(), allowed, selected, response);
	}

	FTextRPGSceneManager::FTextRPGSceneManager(const GameData::FGameDataStore& store)
		: m_store(store)
	{
	}
	FTextRPGSceneManager::~FTextRPGSceneManager()
	{
		// Core 소유 Scene/UI가 typed 후보/Session 문맥보다 먼저 소멸해야 한다.
		Shutdown();
	}

	bool FTextRPGSceneManager::LoadStartupScene(FGameUiBridge& uiManager, std::wstring& errorMessage)
	{
		SHostOperation operation(*this);
		const GameData::Scene::SSceneData* startupScene = nullptr;
		for (const auto& scene : m_store.GetScenes())
		{
			if (!scene.isStartupScene) continue;
			if (startupScene)
			{
				errorMessage = L"Scene data contains multiple startup scenes.";
				return false;
			}
			startupScene = &scene;
		}
		if (!startupScene)
		{
			errorMessage = L"Scene data does not define a startup scene.";
			return false;
		}
		return LoadSceneById(startupScene->sceneId, uiManager, errorMessage);
	}

	bool FTextRPGSceneManager::LoadSceneById(std::uint32_t sceneId, FGameUiBridge& uiManager, std::wstring& errorMessage)
	{
		SHostOperation operation(*this);
		if (IsQuitting() || HasPendingTransition() || m_publicationProgress.Owner || m_acceptProgress)
		{
			errorMessage = L"Scene request conflicts with pending publication or shutdown.";
			return false;
		}
		std::unique_ptr<FDataScene> scene;
		if (!PrepareSceneById(sceneId, uiManager, m_progress, scene, errorMessage)) return false;
		if (!SetScene(std::move(scene)))
		{
			errorMessage = L"Scene request conflicts with a pending operation or an invalid candidate.";
			return false;
		}
		errorMessage.clear();
		return true;
	}

	bool FTextRPGSceneManager::PrepareSceneById(std::uint32_t sceneId, FGameUiBridge& uiManager,
		const InGameData* progress, std::unique_ptr<FDataScene>& output, std::wstring& errorMessage) const
	{
		SHostOperation operation(*this);
		try
		{
			auto scene = std::make_unique<FDataScene>(m_store, uiManager, progress, m_canContinue);
			if (!scene->LoadFromGameData(sceneId, errorMessage)) return false;
			output = std::move(scene);
			errorMessage.clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			const std::string error = exception.what();
			errorMessage.assign(error.begin(), error.end());
			return false;
		}
	}

	bool FTextRPGSceneManager::ValidateDataScene(const FScene& scene, const InGameData* progress) const
	{
		const auto* next = dynamic_cast<const FDataScene*>(&scene);
		const auto* previous = dynamic_cast<const FDataScene*>(GetCurrentScene());
		return !next || (!next->m_isRetired && !next->m_uiLayers.empty() && !next->IsUiBusy() &&
			!next->m_isDispatching && !next->m_isHandlingFrame && !next->m_isRendering && !next->m_isCommitting &&
			next->m_progress == progress && &next->m_store == &m_store &&
			(!previous || &previous->m_uiManager == &next->m_uiManager));
	}

	bool FTextRPGSceneManager::ValidateCandidate(const FScene& scene) const
	{
		if (dynamic_cast<const FDataScene*>(&scene) && m_publicationProgress.Owner) return false;
		return ValidateDataScene(scene, m_acceptProgress ? m_acceptProgress : m_progress);
	}

	bool FTextRPGSceneManager::ValidatePublication(const FScene& scene) const
	{
		const auto& request = m_publicationProgress;
		const auto* next = dynamic_cast<const FDataScene*>(&scene);
		return (!request.Owner || (request.Progress && next && request.Owner->get() == m_progress &&
			next->m_sceneId == request.Progress->GetSceneId())) &&
			ValidateDataScene(scene, request.Owner ? request.Progress.get() : m_progress);
	}

	bool FTextRPGSceneManager::CommitProgress(std::unique_ptr<FDataScene>& nextScene,
		std::unique_ptr<InGameData>& active, std::unique_ptr<InGameData>& candidate, std::wstring& errorMessage)
	{
		SHostOperation operation(*this);
		if (IsQuitting() || HasPendingTransition() || m_completedOwner || m_publicationProgress.Owner || m_acceptProgress ||
			!nextScene || !candidate || m_progress != active.get() || nextScene->m_sceneId != candidate->GetSceneId())
		{
			errorMessage = L"Progress request conflicts with a pending operation or has an invalid prepared candidate.";
			return false;
		}
		struct SAcceptProgressScope final
		{
			const InGameData*& Progress;
			~SAcceptProgressScope()
			{
				Progress = nullptr;
			}
		} preparing{m_acceptProgress};
		m_acceptProgress = candidate.get();
		if (!QueueScene(nextScene, [&]() noexcept
		{
			m_pendingProgress.Progress = std::move(candidate);
			m_pendingProgress.Owner = &active;
		}))
		{
			errorMessage = L"Progress request conflicts with a pending operation or has an invalid prepared candidate.";
			return false;
		}
		errorMessage.clear();
		return true;
	}

	bool FTextRPGSceneManager::TakeProgressResult(std::unique_ptr<InGameData>& active, bool& published, std::wstring& errorMessage)
	{
		if (IsExecutingTransition() || IsHostOperating() || m_completedOwner != &active) return false;
		published = m_progressPublished;
		errorMessage = std::move(m_publicationError);
		m_completedOwner = nullptr;
		return true;
	}

	void FTextRPGSceneManager::EnterPreparedScene(FScene& scene)
	{
		if (!dynamic_cast<FDataScene*>(&scene)) scene.OnEnter();
	}

	void FTextRPGSceneManager::OnSceneLeaving(FScene& scene) noexcept
	{
		if (auto* previous = dynamic_cast<FDataScene*>(&scene))
		{
			m_confirmHeld = previous->m_confirmHeld;
			m_primaryHeld = previous->m_primaryConfirmHeld;
			m_currentBatch = previous->m_currentBatch;
		}
	}

	void FTextRPGSceneManager::RetireScene(FScene& scene) noexcept
	{
		if (auto* data = dynamic_cast<FDataScene*>(&scene)) data->RetireForReplacement();
	}

	void FTextRPGSceneManager::RestoreSceneAfterFailedPublication(FScene& scene) noexcept
	{
		// 검증 실패가 OnExit 성공 뒤에 발생해도 살아 있는 이전 framework 연결만 복구한다.
		if (auto* data = dynamic_cast<FDataScene*>(&scene); data && m_restorePreviousBinding && !data->m_uiManager.GetBoundTarget())
			data->m_uiManager.Bind(*data);
	}

	void FTextRPGSceneManager::OnTransitionRequested(FScene* scene, bool requested) noexcept
	{
		if (auto* data = dynamic_cast<FDataScene*>(scene))
		{
			data->m_replacementRequested = requested;
			if (IsQuitting()) data->InvalidateUiInputForShutdown();
		}
	}

	void FTextRPGSceneManager::InvalidatePendingScene(FScene* scene) noexcept
	{
		if (auto* data = dynamic_cast<FDataScene*>(scene)) data->InvalidateUiInputForShutdown();
	}

	void FTextRPGSceneManager::OnTransitionSnapshot() noexcept
	{
		const auto* previous = dynamic_cast<const FDataScene*>(GetCurrentScene());
		m_restorePreviousBinding = previous && previous->m_uiManager.GetBoundTarget() == previous;
		m_publicationProgress.Progress = std::move(m_pendingProgress.Progress);
		m_publicationProgress.Owner = std::exchange(m_pendingProgress.Owner, nullptr);
	}

	void FTextRPGSceneManager::PublishScene(SPublication& publication) noexcept
	{
		if (auto* owner = m_publicationProgress.Owner)
		{
			owner->swap(m_publicationProgress.Progress);
			m_progress = owner->get();
		}
		publication.Publish();
	}

	void FTextRPGSceneManager::OnScenePublished(FScene& scene) noexcept
	{
		if (auto* gameScene = dynamic_cast<IScene*>(&scene)) gameScene->SetInputPolicy(*this);
		m_confirmGate = m_confirmHeld;
		m_primaryGate = m_primaryHeld;
		if (auto* next = dynamic_cast<FDataScene*>(&scene))
		{
			next->m_confirmHeld = m_confirmHeld;
			next->m_primaryConfirmHeld = m_primaryHeld;
			next->m_dropBatch = m_currentBatch;
			next->m_uiManager.Bind(*next);
		}
		m_messageKey = 0;
	}

	void FTextRPGSceneManager::OnTransitionComplete(ESceneTransitionResult result, std::wstring& error)
	{
		// 후보/이전 UI의 모든 destructor가 반환한 뒤에만 typed root를 정리하고 완료를 공개한다.
		m_publicationProgress.Progress.reset();
		if (auto* owner = std::exchange(m_publicationProgress.Owner, nullptr))
		{
			m_completedOwner = owner;
			m_progressPublished = result == ESceneTransitionResult::Published;
			m_publicationError = error;
		}
	}

	void FTextRPGSceneManager::SetContinueAvailable(bool available) noexcept
	{
		m_canContinue = available;
	}
	void FTextRPGSceneManager::SetSaveRetryAvailable(bool available) noexcept
	{
		if (auto* scene = dynamic_cast<FDataScene*>(GetCurrentScene())) scene->SetSaveRetryAvailable(available);
	}
	void FTextRPGSceneManager::ShowMessage(std::uint32_t textKey)
	{
		m_messageKey = textKey;
	}

	bool FTextRPGSceneManager::ObserveInput(const SInputEvent& event) noexcept
	{
		m_currentBatch = event.BatchId;
		if (event.Type == EInputEventType::FocusLost)
		{
			m_confirmHeld.fill(false);
			m_confirmGate.fill(false);
			m_primaryHeld = m_primaryGate = false;
		}
		const auto key = static_cast<std::size_t>(event.Key);
		const bool confirm = event.Key == EInputKey::Enter || (event.Key >= EInputKey::Digit1 && event.Key <= EInputKey::Digit4);
		bool blocked = false;
		if (confirm && key < m_confirmHeld.size())
		{
			blocked = event.Type == EInputEventType::KeyDown && m_confirmGate[key];
			if (event.Type == EInputEventType::KeyDown) m_confirmHeld[key] = true;
			if (event.Type == EInputEventType::KeyUp && event.IsLogicalTransition) m_confirmHeld[key] = m_confirmGate[key] = false;
		}
		if (event.MouseButton == EMouseButton::Left)
		{
			if (event.Type == EInputEventType::PointerDown)
			{
				blocked = m_primaryGate;
				m_primaryHeld = true;
			}
			if (event.Type == EInputEventType::PointerUp && event.IsLogicalTransition) m_primaryHeld = m_primaryGate = false;
		}
		return blocked;
	}

	bool FTextRPGSceneManager::FilterSceneInput(const SInputEvent& event, bool dataScene, SSceneAction& response)
	{
		const bool blocked = ObserveInput(event);
		if (event.Type == EInputEventType::KeyDown && event.Key == EInputKey::Escape)
		{
			CancelPendingTransitions();
			response = {ESceneAction::Quit, {}};
			return false;
		}
		return !IsExecutingTransition() && !IsQuitting() && (dataScene || !blocked);
	}

	bool FTextRPGSceneManager::FilterSceneInputFrame(const std::vector<SInputEvent>& events, bool dataScene,
		bool transitionRequested, std::vector<SInputEvent>& allowed, const std::vector<SInputEvent>*& selected, SSceneAction& response)
	{
		for (const auto& event : events)
		{
			if (event.Type == EInputEventType::KeyDown && event.Key == EInputKey::Escape)
			{
				CancelPendingTransitions();
				response = {ESceneAction::Quit, {}};
				return false;
			}
		}
		for (const auto& event : events) if (!ObserveInput(event)) allowed.push_back(event);
		if (IsExecutingTransition() || IsQuitting() || (!dataScene && transitionRequested)) return false;
		selected = dataScene ? &events : &allowed;
		return true;
	}

	bool FTextRPGSceneManager::AllowManagerInput(const SInputEvent& event, SSceneAction& response)
	{
		if (!GetCurrentScene() || IsExecutingTransition() || IsQuitting()) return FilterSceneInput(event, false, response);
		if (event.Type == EInputEventType::KeyDown && event.Key == EInputKey::Escape)
		{
			CancelPendingTransitions();
			response = {ESceneAction::Quit, {}};
			return false;
		}
		return true;
	}

	bool FTextRPGSceneManager::AllowManagerInputFrame(const std::vector<SInputEvent>& events, SSceneAction& response)
	{
		if (GetCurrentScene() && !IsExecutingTransition() && !IsQuitting()) return true;
		for (const auto& event : events) if (!FilterSceneInput(event, false, response) && response) return false;
		return false;
	}

	void FTextRPGSceneManager::RenderOverlay(FFrameBuffer& frame) const
	{
		if (m_messageKey != 0 && frame.GetWidth() >= 132 && frame.GetHeight() >= 43)
			frame.DrawText(88, 40, GetLocalizedText(m_store, m_messageKey), 7, 42);
	}
}
