#include "pch.h"
#include "FSceneManager.h"

#include "Common/GameDataEnums.g.h"
#include "UI/FUIManager.h"
#include "UI/FGameUiBridge.h"
#include "FDataScene.h"
#include "IScene.h"
#include "Rendering/FFrameBuffer.h"
#include "Scene/SceneData.g.h"
#include "GameData/FGameDataStore.h"
#include "InGameDataAll.h"
#include "InGameData/InGameData.h"
#include "Localization/Localization.h"

namespace TextRPG
{
	bool IScene::AllowInput(const SInputEvent& inputEvent, SSceneAction& response)
	{
		const bool dataScene = dynamic_cast<FDataScene*>(this) != nullptr;
		if (m_owner && !m_isHandlingInputFrame)
		{
			const bool blocked = m_owner->ObserveInput(inputEvent);
			if (inputEvent.Type == EInputEventType::KeyDown && inputEvent.Key == EInputKey::Escape)
			{
				m_owner->CancelPendingTransitions();
				response = {ESceneAction::Quit, {}};
				return false;
			}
			if (m_owner->m_isEndingFrame || m_owner->m_isQuitting || (!dataScene && blocked)) return false;
		}
		return !m_transitionRequested || dataScene;
	}
	bool IScene::AllowInputFrame(const std::vector<SInputEvent>& events, std::vector<SInputEvent>& allowed,
		const std::vector<SInputEvent>*& selected, SSceneAction& response)
	{
		if (!m_owner) return true;
		for (const auto& event : events)
		{
			if (event.Type == EInputEventType::KeyDown && event.Key == EInputKey::Escape)
			{
				m_owner->CancelPendingTransitions();
				response = {ESceneAction::Quit, {}};
				return false;
			}
		}
		const bool dataScene = dynamic_cast<FDataScene*>(this) != nullptr;
		for (const auto& event : events)
		{
			const bool blocked = m_owner->ObserveInput(event);
			if (!blocked) allowed.push_back(event);
		}
		if (m_owner->m_isEndingFrame || m_owner->m_isQuitting || (!dataScene && m_transitionRequested)) return false;
		selected = dataScene ? &events : &allowed;
		return true;
	}

	FSceneManager::FSceneManager(const GameData::FGameDataStore& store)
		: m_store(store)
	{
	}

	bool FSceneManager::LoadStartupScene(
		FGameUiBridge& uiManager,
		std::wstring& errorMessage)
	{
		const GameData::Scene::SSceneData* startupScene = nullptr;
		for (const GameData::Scene::SSceneData& scene : m_store.GetScenes())
		{
			if (!scene.isStartupScene)
			{
				continue;
			}

			if (startupScene != nullptr)
			{
				errorMessage = L"Scene data contains multiple startup scenes.";
				return false;
			}
			startupScene = &scene;
		}

		if (startupScene == nullptr)
		{
			errorMessage = L"Scene data does not define a startup scene.";
			return false;
		}

		return LoadSceneById(startupScene->sceneId, uiManager, errorMessage);
	}

	bool FSceneManager::LoadSceneById(
		const std::wstring& sceneId,
		FGameUiBridge& uiManager,
		std::wstring& errorMessage)
	{
		// 진행 게시의 옛 Scene 콜백은 곧 해제될 root로 다음 Scene을 준비할 수 없다.
		if (m_isQuitting || m_pendingScene || m_isPublishingProgress)
		{
			errorMessage = L"Scene request conflicts with pending publication or shutdown.";
			return false;
		}
		std::unique_ptr<FDataScene> nextScene;
		if (!PrepareSceneById(sceneId, uiManager, m_progress, nextScene, errorMessage))
		{
			return false;
		}

		if (!SetScene(std::move(nextScene)))
		{
			errorMessage = L"Scene request conflicts with a pending operation or an invalid candidate.";
			return false;
		}
		errorMessage.clear();
		return true;
	}

	bool FSceneManager::PrepareSceneById(const std::wstring& sceneId, FGameUiBridge& uiManager,
		const InGameData* progress,
		std::unique_ptr<FDataScene>& output, std::wstring& errorMessage) const
	{
		try
		{
			auto nextScene = std::make_unique<FDataScene>(m_store, uiManager, progress, m_canContinue);
			if (!nextScene->LoadFromGameData(sceneId, errorMessage)) return false;
			output = std::move(nextScene);
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

	void FSceneManager::DiscardPreparedScene(std::unique_ptr<IScene>& scene) noexcept
	{
		if (auto* data = dynamic_cast<FDataScene*>(scene.get())) data->RetireForReplacement();
		scene.reset();
	}

	FSceneManager::~FSceneManager()
	{
		CancelPendingTransitions();
		if (IsSafePointEligible()) DiscardPendingTransitions();
		try { if (m_currentScene) m_currentScene->OnExit(); }
		catch (...) { std::cerr << "Scene shutdown failed.\n"; }
		DiscardPreparedScene(m_currentScene);
	}

	void FSceneManager::MarkTransitionRequested() noexcept
	{
		if (m_currentScene) m_currentScene->m_transitionRequested = true;
		if (auto* previous = dynamic_cast<FDataScene*>(m_currentScene.get())) previous->m_replacementRequested = true;
	}

	bool FSceneManager::CommitProgress(std::unique_ptr<FDataScene>& nextScene,
		std::unique_ptr<InGameData>& active, std::unique_ptr<InGameData>& candidate, std::wstring& errorMessage)
	{
		auto* previous = dynamic_cast<FDataScene*>(m_currentScene.get());
		if (m_isQuitting || m_pendingScene || m_completedOwner || !nextScene || !candidate ||
			nextScene->m_progress != candidate.get() || nextScene->m_sceneId != candidate->GetSceneId() ||
			&nextScene->m_store != &m_store || nextScene->m_isRetired || nextScene->m_isDispatching ||
			nextScene->m_isHandlingFrame || nextScene->m_isCommitting || nextScene->m_isRendering ||
			nextScene->IsUiBusy() || nextScene->m_uiLayers.empty() || m_progress != active.get() ||
			(previous && &previous->m_uiManager != &nextScene->m_uiManager))
		{
			errorMessage = L"Progress request conflicts with a pending operation or has an invalid prepared candidate.";
			return false;
		}
		m_pendingProgress = std::move(candidate);
		m_pendingScene = std::move(nextScene);
		m_pendingOwner = &active;
		MarkTransitionRequested();
		errorMessage.clear();
		return true;
	}

	bool FSceneManager::TakeProgressResult(std::unique_ptr<InGameData>& active, bool& published, std::wstring& errorMessage)
	{
		if (m_isEndingFrame || m_completedOwner != &active) return false;
		published = m_progressPublished;
		errorMessage = std::move(m_publicationError);
		m_completedOwner = nullptr;
		return true;
	}

	bool FSceneManager::IsSafePointEligible() const noexcept
	{
		return FSceneHost::IsSafePointEligible(m_currentScene.get(), m_pendingScene.get());
	}

	void FSceneManager::CancelPendingTransitions() noexcept
	{
		m_isQuitting = true;
		MarkTransitionRequested();
		if (auto* current = dynamic_cast<FDataScene*>(m_currentScene.get())) current->InvalidateUiInputForShutdown();
		if (m_pendingScene) m_pendingScene->m_transitionRequested = true;
		if (auto* next = dynamic_cast<FDataScene*>(m_pendingScene.get())) next->InvalidateUiInputForShutdown();
		// 논리 취소는 즉시, 실행 중인 Scene/root와 결과의 물리 정리는 공통 안전점에서만 수행한다.
		if (!IsSafePointEligible())
		{
			m_isCancellationDeferred = true;
			return;
		}
		// 한 번 이월한 취소는 callback 반환 뒤 idle 재요청으로도 프레임 끝을 앞당길 수 없다.
		if (!m_isCancellationDeferred) DiscardPendingTransitions();
	}

	void FSceneManager::DiscardPendingTransitions() noexcept
	{
		SEndFrameScope cancel(*this);
		// 가상 destructor 전에 멤버 소유권을 모두 비운다. 재진입은 이 snapshot에 접근할 수 없다.
		auto progress = std::move(m_pendingProgress);
		auto scene = std::move(m_pendingScene);
		auto* owner = std::exchange(m_pendingOwner, nullptr);
		DiscardPreparedScene(scene);
		progress.reset(); // 후보 root는 모든 Scene/UI destructor 반환 뒤에만 해제한다.
		if (owner)
		{
			m_completedOwner = owner;
			m_progressPublished = false;
		}
		m_isCancellationDeferred = false;
	}

	bool FSceneManager::EndFrame(std::wstring& errorMessage)
	{
		auto* previous = dynamic_cast<FDataScene*>(m_currentScene.get());
		if (!IsSafePointEligible())
		{
			errorMessage = L"EndFrame requires all Scene and UI operations to have returned.";
			return false;
		}
		errorMessage.clear();
		if (m_isQuitting)
		{
			DiscardPendingTransitions();
			return true;
		}
		if (!m_pendingScene) return true;
		SEndFrameScope ending(*this);
		struct SPublicationScope final
		{
			bool& Publishing;
			~SPublicationScope() { Publishing = false; }
		} publication{m_isPublishingProgress};
		// 단 하나의 snapshot만 게시한다. OnExit/OnEnter/파괴 콜백의 새 요청은 다음 프레임 몫이다.
		auto progress = std::move(m_pendingProgress);
		auto scene = std::move(m_pendingScene);
		auto* owner = std::exchange(m_pendingOwner, nullptr);
		m_isPublishingProgress = owner != nullptr;
		if (owner)
		{
			m_completedOwner = owner;
			m_progressPublished = false;
			m_publicationError.clear();
		}
		auto* next = dynamic_cast<FDataScene*>(scene.get());
		bool prepared = !next || (!next->m_isRetired && !next->m_uiLayers.empty() && !next->IsUiBusy() &&
			!next->m_isDispatching && !next->m_isHandlingFrame && !next->m_isRendering && !next->m_isCommitting &&
			next->m_progress == (owner ? progress.get() : m_progress) &&
			(!previous || &previous->m_uiManager == &next->m_uiManager));
		if (owner && (!next || !progress || owner->get() != m_progress)) prepared = false;
		if (!prepared) errorMessage = L"Prepared Scene no longer matches the active progress or UI owner.";
		else
		{
			try
			{
				// 준비된 FDataScene은 Bind만 필요하다. generic 진입 실패도 옛 소유 핸들을 바꾸지 않는다.
				if (!next) scene->OnEnter();
				if (m_isQuitting) errorMessage = L"Scene publication was canceled by shutdown.";
				else if (m_currentScene) m_currentScene->OnExit();
				if (m_isQuitting) errorMessage = L"Scene publication was canceled by shutdown.";
			}
			catch (const std::exception& exception)
			{
				const std::string error = exception.what();
				errorMessage.assign(error.begin(), error.end());
			}
			catch (...) { errorMessage = L"Scene activation failed with an unknown exception."; }
		}
		if (!errorMessage.empty())
		{
			if (owner) m_publicationError = errorMessage;
			DiscardPreparedScene(scene);
			if (m_currentScene) m_currentScene->m_transitionRequested = static_cast<bool>(m_pendingScene);
			if (previous) previous->m_replacementRequested = static_cast<bool>(m_pendingScene);
			return false;
		}
		if (previous)
		{
			m_confirmHeld = previous->m_confirmHeld;
			m_primaryHeld = previous->m_primaryConfirmHeld;
			m_currentBatch = previous->m_currentBatch;
			// 모든 옛 UI 비소유 뷰는 옛 root가 유효한 동안 해제한다.
			previous->RetireForReplacement();
		}
		m_currentScene.reset();
		// old UI와 Scene의 가상 destructor도 종료 요청을 낼 수 있다. 이 이후만 commit point다.
		if (m_isQuitting)
		{
			errorMessage = L"Scene publication was canceled by shutdown.";
			if (owner) m_publicationError = errorMessage;
			DiscardPreparedScene(scene);
			// old Scene은 이미 해제됐다. 복원했다고 주장하지 않고 root/파일을 보존한 채 종료한다.
			return false;
		}
		// 여기부터 가상 콜백/할당 없이 진행과 Scene을 함께 게시한다.
		if (owner)
		{
			owner->swap(progress);
			m_progress = owner->get();
		}
		scene->m_owner = this;
		m_currentScene = std::move(scene);
		m_confirmGate = m_confirmHeld;
		m_primaryGate = m_primaryHeld;
		if (next)
		{
			next->m_confirmHeld = m_confirmHeld;
			next->m_primaryConfirmHeld = m_primaryHeld;
			next->m_dropBatch = m_currentBatch;
			next->m_uiManager.Bind(*next);
		}
		if (owner) m_progressPublished = true;
		m_messageKey.clear();
		// 새 요청은 준비한 당시 진행과 다음 EndFrame에서 다시 대조한다.
		if (m_pendingScene) MarkTransitionRequested();
		return true;
	}

	void FSceneManager::SetContinueAvailable(bool available) noexcept
	{
		m_canContinue = available;
	}

	void FSceneManager::ShowMessage(std::wstring textKey)
	{
		m_messageKey = std::move(textKey);
	}

	bool FSceneManager::SetScene(std::unique_ptr<IScene> nextScene)
	{
		if (!nextScene) return false;
		auto* next = dynamic_cast<FDataScene*>(nextScene.get());
		auto* previous = dynamic_cast<FDataScene*>(m_currentScene.get());
		if (m_isQuitting || m_pendingScene || (next && (m_isPublishingProgress || next->m_isRetired || next->m_uiLayers.empty() ||
			next->IsUiBusy() || next->m_progress != m_progress || &next->m_store != &m_store ||
			next->m_isDispatching || next->m_isHandlingFrame || next->m_isCommitting || next->m_isRendering ||
			(previous && &previous->m_uiManager != &next->m_uiManager))))
		{
			DiscardPreparedScene(nextScene);
			return false;
		}
		m_pendingScene = std::move(nextScene);
		MarkTransitionRequested();
		return true;
	}

	bool FSceneManager::ObserveInput(const SInputEvent& event) noexcept
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
			if (event.Type == EInputEventType::KeyUp && event.IsLogicalTransition)
				m_confirmHeld[key] = m_confirmGate[key] = false;
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

	SSceneAction FSceneManager::HandleInput(const SInputEvent& inputEvent)
	{
		if (inputEvent.Type == EInputEventType::KeyDown && inputEvent.Key == EInputKey::Escape)
		{
			CancelPendingTransitions();
			return { ESceneAction::Quit, {} };
		}
		SHostOperation operation(*this);
		if (m_isEndingFrame || m_isQuitting || !m_currentScene)
		{
			(void)ObserveInput(inputEvent);
			return {};
		}
		return m_currentScene->HandleInput(inputEvent);
	}

	void FSceneManager::Update(float deltaSeconds)
	{
		SHostOperation operation(*this);
		if (!m_isEndingFrame && !m_isQuitting && m_currentScene) m_currentScene->Update(deltaSeconds);
	}

	SSceneAction FSceneManager::HandleInputFrame(const std::vector<SInputEvent>& events)
	{
		SHostOperation operation(*this);
		if (!m_currentScene)
		{
			for (const auto& event : events)
			{
				(void)ObserveInput(event);
				if (event.Type == EInputEventType::KeyDown && event.Key == EInputKey::Escape)
				{
					CancelPendingTransitions();
					return {ESceneAction::Quit, {}};
				}
			}
			return {};
		}
		return m_currentScene->HandleInputFrame(events);
	}

	void FSceneManager::Render(FFrameBuffer& frameBuffer) const
	{
		SHostOperation operation(*this);
		if (!m_isEndingFrame && m_currentScene)
		{
			m_currentScene->Render(frameBuffer);
			if (!m_messageKey.empty() && frameBuffer.GetWidth() >= 132 && frameBuffer.GetHeight() >= 43)
				frameBuffer.DrawText(88, 40, GetLocalizedText(m_store, m_messageKey), 7, 42);
		}
	}

}
