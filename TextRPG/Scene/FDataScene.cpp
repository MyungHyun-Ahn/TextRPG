#include "pch.h"
#include "Common/GameDataEnums.g.h"
#include "UI/FUIManager.h"
#include "UI/FGameUiBridge.h"
#include "UI/Common/FUiElement.h"
#include "UI/Common/FUiPanel.h"
#include "UI/Common/FUiButton.h"
#include "UI/Common/FSceneUiRoot.h"
#include "UI/MainMenu/FMainMenuUi.h"
#include "UI/InGameData/FInGameStatusUi.h"
#include "Localization/Localization.h"
#include "FDataScene.h"
#include "GameData/FGameDataStore.h"

#include "Rendering/FFrameBuffer.h"
#include "Localization/LocalizationData.g.h"
#include "Scene/SceneData.g.h"
#include "SceneOption/SceneOptionData.g.h"
#include "SceneTypeConfig/SceneTypeConfigData.g.h"

namespace TextRPG
{
	using ETextKey = GameData::Common::ETextKey;
	namespace
	{
		// 사용자 virtual callback이 예외로 빠져도 framework 작업 상태는 원래 값으로 돌아간다.
		class SUiFlagScope final
		{
		public:
			explicit SUiFlagScope(bool& flag, bool value = true) noexcept
				: Flag(&flag), Previous(std::exchange(flag, value)) {}
			~SUiFlagScope() { Restore(); }
			void Restore() noexcept
			{
				if (auto* flag = std::exchange(Flag, nullptr)) *flag = Previous;
			}
			SUiFlagScope(const SUiFlagScope&) = delete;
			SUiFlagScope& operator=(const SUiFlagScope&) = delete;
		private:
			bool* Flag;
			bool Previous;
		};
	}
	FDataScene::FDataScene(const GameData::FGameDataStore& store, FGameUiBridge& uiManager,
		const InGameData* progress, bool canContinue)
		: m_progress(progress)
		, m_canContinue(canContinue)
		, m_store(store)
		, m_uiManager(uiManager)
	{
	}

	FDataScene::~FDataScene()
	{
		const SOperation operation(*this);
		if (!m_isRetired)
		{
			try { OnExit(); }
			catch (const std::exception& exception)
			{
				std::cerr << "Scene cleanup failed: " << exception.what() << '\n';
			}
			catch (...)
			{
				std::cerr << "Scene cleanup failed with an unknown exception.\n";
			}
		}
		RetireForReplacement();
	}

	void FDataScene::RetireForReplacement() noexcept
	{
		const SOperation operation(*this);
		// 현재 작업과 OnExit가 반환한 뒤, 옛 root가 유효한 동안 UI를 정리한다.
		m_isRetired = true;
		m_uiManager.Detach(*this);
		{
			// 멤버를 먼저 비워 destructor의 조회/명령 재진입이 해제 중인 컨테이너를 보지 않게 한다.
			auto commands = std::move(m_uiLayerCommands);
			auto layers = std::move(m_uiLayers);
			m_uiLayerCommands.clear();
			m_uiLayers.clear();
			commands.clear();
			layers.clear();
		}
		m_uiManager.Detach(*this); // destructor 재진입이 자신을 다시 Bind했어도 비소유 연결을 남기지 않는다.
		m_progress = nullptr;
	}

	bool FDataScene::LoadFromGameData(std::uint32_t sceneId, std::wstring& errorMessage)
	{
		const bool wasOperating = IsOperating();
		const SOperation operation(*this);
		if (IsPublicationPrepared())
		{
			errorMessage = L"Accepted Scene preparation is fixed until publication or disposal.";
			return false;
		}
		if (wasOperating || m_isRetired || m_isDispatching || m_isCommitting || m_isRendering || IsUiBusy())
		{
			errorMessage = L"Scene preparation is not allowed during UI dispatch, commit or rendering.";
			return false;
		}
		SUiFlagScope committing(m_isCommitting);
		const GameData::Scene::SSceneData* scene = m_store.FindScene(sceneId);
		if (scene == nullptr)
		{
			errorMessage = L"Scene ID is not registered in GameData: ";
			errorMessage += std::to_wstring(sceneId);
			return false;
		}

		const GameData::SceneTypeConfig::SSceneTypeConfigData* config = m_store.FindSceneTypeConfig(scene->sceneType);
		if (config == nullptr)
		{
			errorMessage = L"SceneType configuration is missing for scene: ";
			errorMessage += std::to_wstring(sceneId);
			return false;
		}

		std::vector<SUIOption> options;
		for (const GameData::SceneOption::SSceneOptionData& dataOption : m_store.GetSceneOptions())
		{
			if (dataOption.sceneType != scene->sceneType)
			{
				continue;
			}

			const GameData::Localization::SLocalizationData* localization =
				m_store.FindLocalization(dataOption.optionTextKey);
			if (localization == nullptr)
			{
				errorMessage = L"Scene option localization key is not registered: ";
				errorMessage += std::to_wstring(dataOption.optionTextKey);
				return false;
			}

			options.push_back({
				.OptionId = dataOption.optionId,
				.OptionOrder = dataOption.optionOrder,
				.Text = localization->koKR,
				.ActionType = dataOption.actionType,
				.TargetSceneId = dataOption.targetSceneId,
				.IsEnabled = dataOption.isEnabled,
			});
		}
		std::sort(options.begin(), options.end(),
				  [](const SUIOption& left, const SUIOption& right) { return left.OptionOrder < right.OptionOrder; });

		if (options.size() != 4 || config->maxVisibleOptions != 4 ||
			config->optionOverflowPolicy != GameData::Common::EOptionOverflowPolicy::NoScroll)
		{
			errorMessage = L"Scene UI requires exactly four fixed options.";
			return false;
		}
		std::unique_ptr<FSceneUiRoot> uiRoot = std::make_unique<FSceneUiRoot>(SCellRect{0, 0, 132, 43}, &m_nextUiId);
		const bool isMainMenu = scene->sceneType == GameData::Common::ESceneType::MainMenu;
		if (isMainMenu)
		{
			auto logoPanel = std::make_unique<FMainMenuUi>();
			if (!logoPanel->Prepare(m_store, errorMessage))
			{
				return false;
			}
			uiRoot->AddBase(std::move(logoPanel));
		}
		else
		{
			for (int left : {0, 43})
			{
				uiRoot->AddBase(std::make_unique<FUiPanel>(SCellRect{left, 0, 42, 5}));
				uiRoot->AddBase(std::make_unique<FUiPanel>(SCellRect{left, 5, 42, 38}));
			}
			uiRoot->AddBase(std::make_unique<FUiPanel>(SCellRect{86, 0, 46, 11}));
			if (m_progress)
			{
				auto status = std::make_unique<FInGameStatusUi>(m_store, *m_progress);
				if (!status->Prepare(errorMessage)) return false;
				uiRoot->AddBase(std::move(status));
			}
		}
		auto optionPanel = std::make_unique<FUiPanel>(SCellRect{86, isMainMenu ? 0 : 11, 46, isMainMenu ? 43 : 32});
		if (isMainMenu)
		{
			optionPanel->SetTitle(GetLocalizedText(m_store, static_cast<std::uint32_t>(ETextKey::Scene_MainMenu_Title)));
		}
		for (std::size_t index = 0; index < options.size(); ++index)
		{
			SUIOption& option = options[index];
			if (option.OptionOrder != index + 1)
			{
				errorMessage = L"Scene option orders must be 1 through 4.";
				return false;
			}
			const bool isImplemented = option.ActionType == GameData::Common::ESceneActionType::StartGame ||
									   option.ActionType == GameData::Common::ESceneActionType::Quit ||
				(option.ActionType == GameData::Common::ESceneActionType::Continue && m_canContinue);
			option.IsEnabled = option.IsEnabled && isImplemented;
			const int top = isMainMenu ? 3 + static_cast<int>(index) * 9 : 2 + static_cast<int>(index) * 7;
			auto button = std::make_unique<FUiButton>(
				SCellRect{2, top, 38, 3}, std::to_wstring(option.OptionOrder) + L". " + option.Text,
				[this, option]()
				{
					if (option.ActionType == GameData::Common::ESceneActionType::StartGame)
					{
						m_pendingAction = {ESceneAction::StartGame, option.TargetSceneId};
						DiscardUiLayerCommands();
					}
					else if (option.ActionType == GameData::Common::ESceneActionType::Continue)
					{
						m_pendingAction = {ESceneAction::Continue, {}};
						DiscardUiLayerCommands();
					}
					else if (option.ActionType == GameData::Common::ESceneActionType::Quit)
					{
						m_pendingAction = {ESceneAction::Quit, {}};
						DiscardUiLayerCommands();
					}
				},
				static_cast<int>(option.OptionOrder));
			button->SetEnabled(option.IsEnabled);
			optionPanel->AddChild(std::move(button));
		}
		uiRoot->AddBase(std::move(optionPanel));
		// 준비된 UI와 숫자 식별자를 함께 게시한다.
		const auto preparedId = sceneId;
		const auto baseLayer = m_nextUiId++;
		std::vector<SUiLayer> preparedLayers;
		preparedLayers.push_back({baseLayer, 1, true, std::move(uiRoot)});
		const bool wasBound = m_uiManager.GetBoundScene() == this;
		CancelUiInput();
		{
			auto commands = std::move(m_uiLayerCommands);
			auto layers = std::move(m_uiLayers);
			m_uiLayerCommands.clear();
			m_uiLayers.clear();
			commands.clear();
			layers.clear();
		}
		m_uiTopLayer = 0;
		m_baseLayer = baseLayer;
		m_uiLayers.swap(preparedLayers);
		m_sceneId = preparedId;
		RefreshUiTopLayer();
		if (wasBound)
		{
			m_uiManager.Bind(*this);
		}
		errorMessage.clear();
		return true;
	}

	void FDataScene::OnEnterImpl()
	{
		m_uiManager.Bind(*this);
		if (auto* top = GetUiLayer(m_uiTopLayer))
		{
			top->ResumeInput();
		}
	}
	void FDataScene::OnExitImpl()
	{
		const bool wasBound = m_uiManager.GetBoundTarget() == this;
		try
		{
			if (wasBound)
			{
				m_uiManager.Clear();
			}
			CancelUiInput();
			auto commands = std::move(m_uiLayerCommands);
			m_uiLayerCommands.clear();
		}
		catch (...)
		{
			// Clear 이후 재캡처된 root의 두 번째 취소도 같은 퇴장 복구 경계다.
			// callback이 게시한 새 연결은 보존하고, 최종 파괴에서는 Retire가 반드시 Detach한다.
			if (wasBound && !m_uiManager.GetBoundTarget()) m_uiManager.Bind(*this);
			throw;
		}
	}

	void FDataScene::UpdateImpl(float deltaSeconds)
	{
		(void)deltaSeconds;
	}

	SSceneAction FDataScene::HandleInputImpl(const SInputEvent& inputEvent)
	{
		if (m_isDispatching || m_isCommitting || m_isRendering || IsUiBusy())
		{
			return {};
		}
		FlushUiLayerCommands();
		m_pendingAction = {};
		m_currentBatch = inputEvent.BatchId;
		const bool confirm = inputEvent.Key == EInputKey::Enter ||
			(inputEvent.Key >= EInputKey::Digit1 && inputEvent.Key <= EInputKey::Digit4);
		const auto key = static_cast<std::size_t>(inputEvent.Key);
		const bool wasDown = key < m_confirmHeld.size() && m_confirmHeld[key];
		if (confirm && key < m_confirmHeld.size())
		{
			if (inputEvent.Type == EInputEventType::KeyDown)
			{
				m_confirmHeld[key] = true;
			}
			if (inputEvent.Type == EInputEventType::KeyUp && inputEvent.IsLogicalTransition)
			{
				m_confirmHeld[key] = false;
			}
		}
		const bool wasPointerDown = m_primaryConfirmHeld;
		const bool pointerButton = inputEvent.Type == EInputEventType::PointerDown || inputEvent.Type == EInputEventType::PointerUp;
		const bool primaryButton = inputEvent.MouseButton == EMouseButton::Left;
		if (primaryButton && inputEvent.Type == EInputEventType::PointerDown)
		{
			m_primaryConfirmHeld = true;
		}
		if (primaryButton && inputEvent.Type == EInputEventType::PointerUp && inputEvent.IsLogicalTransition)
		{
			m_primaryConfirmHeld = false;
		}
		const bool system = inputEvent.Type == EInputEventType::FocusLost || inputEvent.Type == EInputEventType::Resize;
		if (inputEvent.Type == EInputEventType::FocusLost)
		{
			m_confirmHeld.fill(false);
			m_primaryConfirmHeld = false;
		}
		const bool nonTransitionRelease = !inputEvent.IsLogicalTransition &&
			(inputEvent.Type == EInputEventType::KeyUp || inputEvent.Type == EInputEventType::PointerUp);
		const bool blocked = m_replacementRequested || nonTransitionRelease || (pointerButton && !primaryButton) || m_dropFrame ||
			(inputEvent.BatchId != 0 && inputEvent.BatchId == m_dropBatch) ||
			(inputEvent.Type == EInputEventType::KeyDown && confirm && (wasDown || inputEvent.IsRepeat)) ||
			(inputEvent.Type == EInputEventType::PointerDown && wasPointerDown);
		SUiFlagScope dispatching(m_isDispatching);
		if (system)
		{
			for (auto& layer : m_uiLayers)
			{
				layer.Root->Dispatch(inputEvent, false);
			}
		}
		else if (!blocked)
		{
			if (auto* top = GetUiLayer(m_uiTopLayer))
				top->Dispatch(inputEvent, inputEvent.ScreenWidth >= 132 && inputEvent.ScreenHeight >= 43);
		}
		dispatching.Restore();
		FlushUiLayerCommands();
		return std::exchange(m_pendingAction, {});
	}

	SSceneAction FDataScene::HandleInputFrameImpl(const std::vector<SInputEvent>& events)
	{
		if (m_isHandlingFrame || m_isDispatching || m_isCommitting || m_isRendering || IsUiBusy())
		{
			return {};
		}
		FlushUiLayerCommands();
		SUiFlagScope handlingFrame(m_isHandlingFrame);
		SUiFlagScope dropFrame(m_dropFrame, false);
		SSceneAction result;
		for (const auto& event : events)
		{
			const auto generation = m_layerGeneration;
			auto action = HandleInput(event);
			if (action.Type != ESceneAction::None)
			{
				result = std::move(action);
			}
			if (generation != m_layerGeneration || result.Type != ESceneAction::None)
			{
				m_dropFrame = true;
			}
		}
		dropFrame.Restore();
		handlingFrame.Restore();
		return result;
	}

	void FDataScene::RenderImpl(FFrameBuffer& frameBuffer) const
	{
		if (frameBuffer.GetWidth() < 132 || frameBuffer.GetHeight() < 43)
		{
			frameBuffer.Clear();
			frameBuffer.DrawText(0, frameBuffer.GetHeight() / 2,
				GetLocalizedText(m_store, static_cast<std::uint32_t>(ETextKey::System_ScreenTooSmall)));
			return;
		}
		if (m_isRendering)
		{
			return;
		}
		SUiFlagScope rendering(m_isRendering);
		frameBuffer.Clear();
		for (const auto& layer : m_uiLayers)
		{
			if (layer.Visible)
			{
				layer.Root->Render(frameBuffer, false);
			}
		}
		rendering.Restore();
	}

	FSceneUiRoot& FDataScene::GetUiRoot() noexcept
	{
		const SOperation operation(*this);
		return *GetUiBaseRoot();
	}

	FSceneUiRoot* FDataScene::GetUiBaseRoot() const noexcept
	{
		const SOperation operation(*this);
		return GetUiLayer(m_baseLayer);
	}

	std::uint64_t FDataScene::OpenUiLayer(std::unique_ptr<IUI> incomingUi, std::uint32_t level)
	{
		const SOperation operation(*this);
		auto ui = std::move(incomingUi); // 인수 거부/예외의 destructor도 Scene 작업보다 먼저 반환한다.
		if (m_isRetired || m_isRendering)
		{
			return 0;
		}
		if (level == 0)
		{
			for (const auto& layer : m_uiLayers)
			{
				level = std::max(level, layer.Level);
			}
			for (const auto& command : m_uiLayerCommands)
			{
				level = std::max(level, command.Layer.Level);
			}
			if (level == (std::numeric_limits<std::uint32_t>::max)())
			{
				return 0;
			}
			++level;
		}
		const auto id = m_nextUiId++;
		auto layerRoot = std::make_unique<FSceneUiRoot>(SCellRect{0, 0, 132, 43}, &m_nextUiId);
		if (ui && layerRoot->AddBase(std::move(ui)) == 0)
		{
			return 0;
		}
		m_uiLayerCommands.push_back({{id, level, true, std::move(layerRoot)}, 0, false, true});
		if (m_isDispatching && level >= GetUiTopLevel())
		{
			if (auto* top = GetUiLayer(m_uiTopLayer))
			{
				top->CancelInput();
			}
		}
		return id;
	}
	FUiId FDataScene::AddUiToLayer(std::uint64_t layerId, std::unique_ptr<IUI> incomingUi)
	{
		const SOperation operation(*this);
		auto ui = std::move(incomingUi);
		if (m_isRetired || m_isDispatching || m_isCommitting || m_isRendering || IsUiBusy())
		{
			return 0;
		}
		if (auto* layer = GetUiLayer(layerId))
		{
			return layer->AddBase(std::move(ui));
		}
		return 0;
	}
	void FDataScene::CloseUiLayer(std::uint64_t layerId)
	{
		const SOperation operation(*this);
		if (layerId && !IsPublicationPrepared() && !m_isRetired && !m_isRendering)
		{
			m_uiLayerCommands.push_back({{}, layerId, false, true});
			if (m_isDispatching && layerId == m_uiTopLayer)
			{
				if (auto* top = GetUiLayer(layerId))
				{
					top->CancelInput();
				}
			}
		}
	}
	void FDataScene::SetUiLayerVisible(std::uint64_t layerId, bool visible)
	{
		const SOperation operation(*this);
		if (layerId && !m_isRetired && !m_isRendering)
		{
			m_uiLayerCommands.push_back({{}, layerId, true, visible});
			const auto found = std::find_if(m_uiLayers.begin(), m_uiLayers.end(), [layerId](const SUiLayer& layer)
			{ return layer.Id == layerId; });
			const bool revealsTop = visible && found != m_uiLayers.end() && !found->Visible && found->Level >= GetUiTopLevel();
			if (m_isDispatching && ((!visible && layerId == m_uiTopLayer) || revealsTop))
			{
				if (auto* top = GetUiLayer(m_uiTopLayer))
				{
					top->CancelInput();
				}
			}
		}
	}
	void FDataScene::DiscardUiLayerCommands()
	{
		const SOperation operation(*this);
		if (m_isRetired || m_isRendering)
		{
			return;
		}
		m_discardLayerCommands = true;
		if (!m_isDispatching)
		{
			FlushUiLayerCommands();
		}
	}
	void FDataScene::InvalidateUiInputForShutdown() noexcept
	{
		const SOperation operation(*this);
		m_replacementRequested = true;
		if (m_isRetired) return; // Retire의 가상 destructor 재진입 중에는 이미 해제 중인 Layer를 순회하지 않는다.
		// 사용자 취소 콜백/캡처의 물리 정리는 안전점에 남기고 현재 root 제스처만 즉시 무효화한다.
		for (auto& layer : m_uiLayers) layer.Root->InvalidateInput();
	}
	void FDataScene::CancelUiInput()
	{
		const SOperation operation(*this);
		if (m_isRetired) return;
		SUiFlagScope dispatching(m_isDispatching);
		for (auto& layer : m_uiLayers)
		{
			layer.Root->CancelInput();
		}
		dispatching.Restore();
	}
	FSceneUiRoot* FDataScene::GetUiLayer(std::uint64_t layerId) const noexcept
	{
		const SOperation operation(*this);
		for (const auto& layer : m_uiLayers)
		{
			if (layer.Id == layerId)
			{
				return layer.Root.get();
			}
		}
		return nullptr;
	}
	IUI* FDataScene::FindUi(FUiId id) const noexcept
	{
		const SOperation operation(*this);
		for (const auto& layer : m_uiLayers)
		{
			if (auto* ui = layer.Root->Find(id))
			{
				return ui;
			}
		}
		return nullptr;
	}
	std::uint64_t FDataScene::GetUiTopLayer() const noexcept
	{
		const SOperation operation(*this);
		return m_uiTopLayer;
	}
	std::uint32_t FDataScene::GetUiTopLevel() const noexcept
	{
		const SOperation operation(*this);
		for (const auto& layer : m_uiLayers)
		{
			if (layer.Id == m_uiTopLayer)
			{
				return layer.Level;
			}
		}
		return 0;
	}
	std::size_t FDataScene::GetUiLayerCount() const noexcept
	{
		const SOperation operation(*this);
		return m_uiLayers.size();
	}
	void FDataScene::RefreshUiTopLayer()
	{
		std::uint64_t top = 0;
		std::uint32_t level = 0;
		for (const auto& layer : m_uiLayers)
		{
			if (layer.Visible && layer.Level >= level)
			{
				top = layer.Id;
				level = layer.Level;
			}
		}
		if (top == m_uiTopLayer)
		{
			return;
		}
		if (auto* previous = GetUiLayer(m_uiTopLayer))
		{
			previous->SuspendInput();
		}
		m_uiTopLayer = top;
		++m_layerGeneration;
		if (auto* current = GetUiLayer(top))
		{
			current->ResumeInput();
		}
		m_dropBatch = m_currentBatch;
	}
	void FDataScene::FlushUiLayerCommands()
	{
		const SOperation operation(*this);
		if (IsPublicationPrepared() || m_isRetired || m_isDispatching || m_isCommitting || m_isRendering || IsUiBusy())
		{
			return;
		}
		SUiFlagScope committing(m_isCommitting);
		if (m_discardLayerCommands)
		{
			auto discarded = std::move(m_uiLayerCommands);
			m_uiLayerCommands.clear();
			CancelUiInput();
			auto canceled = std::move(m_uiLayerCommands);
			m_uiLayerCommands.clear();
			m_discardLayerCommands = false;
			m_dropBatch = m_currentBatch;
			return;
		}
		auto commands = std::move(m_uiLayerCommands);
		m_uiLayerCommands.clear();
		for (auto& command : commands)
		{
			std::unique_ptr<FSceneUiRoot> closed;
			if (command.Layer.Root)
			{
				command.Layer.Root->SuspendInput();
				m_uiLayers.push_back(std::move(command.Layer));
				std::stable_sort(m_uiLayers.begin(), m_uiLayers.end(), [](const SUiLayer& left, const SUiLayer& right)
				{ return left.Level < right.Level; });
			}
			else
			{
				const auto found = std::find_if(m_uiLayers.begin(), m_uiLayers.end(), [&command](const SUiLayer& layer)
				{ return layer.Id == command.Target; });
				if (found == m_uiLayers.end())
				{
					continue;
				}
				if (command.IsVisibility)
				{
					found->Visible = command.Visible;
				}
				else
				{
					found->Root->SuspendInput();
					// erase 도중 사용자 destructor가 moved-from Layer를 조회하지 않게 먼저 소유권을 떼어 둔다.
					closed = std::move(found->Root);
					m_uiLayers.erase(found);
				}
			}
			RefreshUiTopLayer();
		}
	}
	bool FDataScene::IsOwnedUiBusy() const noexcept
	{
		return m_isDispatching || m_isHandlingFrame || m_isCommitting || m_isRendering || IsUiBusy();
	}
	bool FDataScene::IsUiBusy() const noexcept
	{
		return std::any_of(m_uiLayers.begin(), m_uiLayers.end(), [](const SUiLayer& layer)
		{
			return layer.Root->IsBusy();
		});
	}
}
