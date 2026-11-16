#include "pch.h"
#include "GameDataAll.h"
#include "Frame/FrameTypes.h"
#include "Frame/IFrameClock.h"
#include "Frame/FFrame.h"
#include "Frame/FTextRpgFrame.h"
#include "Rendering/FFrameBuffer.h"
#include "Rendering/FConsoleRenderer.h"
#include "Scene/FSceneManager.h"
#include "System/GameDataPaths.h"
#include "UI/FUIManager.h"
#include "UI/FGameUiBridge.h"
#include "Input/FConsoleInput.h"
#include "Input/FInputManager.h"
#include "GameData/FGameDataStore.h"
#include "GameData/FGameDataKey.h"
#include "InGameData/FGameSession.h"
#include "Localization/Localization.h"

namespace TextRPG
{
	namespace
	{
		struct SScopedGameDataKey final
		{
			std::array<std::uint8_t, 32> Bytes{};
			~SScopedGameDataKey() { SecureZeroMemory(Bytes.data(), Bytes.size()); }
		};
	}

	struct FTextRpgFrame::SState final
	{
		std::wstring ErrorMessage;
		std::filesystem::path PackagePath;
		std::string DataError;
		GameData::FGameDataStore Store;
		// 역순 파괴: Scene/UI 비소유 뷰가 먼저, 진행 루트와 Store는 마지막이다.
		FGameSession Session;
		FGameUiBridge UiManager;
		FSceneManager SceneManager;
		FConsoleInput ConsoleInput;
		FInputManager InputManager;
		FConsoleRenderer Renderer;
		bool StartingNewGame = false;

		SState()
			: Session(Store)
			, SceneManager(Store)
			, InputManager(ConsoleInput)
		{
		}
	};

	FTextRpgFrame::FTextRpgFrame(const SFrameOptions& options)
		: FFrame(options)
	{
	}

	FTextRpgFrame::~FTextRpgFrame() = default;

	bool FTextRpgFrame::ReadOptions(int argc, char* const argv[], SFrameOptions& options) noexcept
	{
		if (argc == 1 && argv != nullptr) return true;
		if (argc != 3 || argv == nullptr || argv[1] == nullptr || argv[2] == nullptr) return false;
		if (std::string_view(argv[1]) != "--frame-mode") return false;
		const std::string_view mode(argv[2]);
		if (mode == "basic") options.Mode = EFrameMode::Basic;
		else if (mode == "fixed-update") options.Mode = EFrameMode::FixedUpdate;
		else if (mode == "frame-skip") options.Mode = EFrameMode::FrameSkip;
		else return false;
		return true;
	}

	void FTextRpgFrame::OnStart()
	{
		m_state = std::make_unique<SState>();
		auto& errorMessage = m_state->ErrorMessage;
		auto& packagePath = m_state->PackagePath;
		auto& dataError = m_state->DataError;
		auto& store = m_state->Store;
		auto& session = m_state->Session;
		auto& uiManager = m_state->UiManager;
		auto& sceneManager = m_state->SceneManager;
		if (!ResolveGameDataPackagePath(packagePath, errorMessage))
		{
			std::wcerr << errorMessage << L'\n';
			RequestExit(1);
			return;
		}
		{
			SScopedGameDataKey key;
			if (!GameData::ReadGameDataKey(key.Bytes, dataError) || !store.Load(packagePath, key.Bytes, dataError))
			{
				std::wcerr << L"GameData load failed: " << packagePath.wstring() << L'\n'
					<< std::wstring(dataError.begin(), dataError.end()) << L'\n';
				RequestExit(1);
				return;
			}
		}
		const bool canContinue = session.CanContinue(sceneManager, uiManager, dataError);
		sceneManager.SetContinueAvailable(canContinue);
		if (!sceneManager.LoadStartupScene(uiManager, {}, errorMessage) || !sceneManager.EndFrame(errorMessage))
		{
			std::wcerr << errorMessage << L'\n';
			RequestExit(1);
			return;
		}
		if (!canContinue) sceneManager.ShowMessage(L"InGameData.Ui.LoadFailed");
		if (!m_state->ConsoleInput.Initialize(errorMessage))
		{
			std::wcerr << errorMessage << L'\n';
			RequestExit(1);
		}
	}

	void FTextRpgFrame::OnStartFrame()
	{
		if (!m_state->Renderer.BeginFrame())
		{
			std::wcerr << L"Unable to read the console viewport.\n";
			RequestExit(1);
		}
	}

	void FTextRpgFrame::OnInput()
	{
		auto& renderer = m_state->Renderer;
		auto& consoleInput = m_state->ConsoleInput;
		auto& inputManager = m_state->InputManager;
		auto& sceneManager = m_state->SceneManager;
		auto& session = m_state->Session;
		auto& store = m_state->Store;
		auto& dataError = m_state->DataError;
		const int screenWidth = renderer.GetBackBuffer().GetWidth();
		const int screenHeight = renderer.GetBackBuffer().GetHeight();
		consoleInput.BeginBatch(renderer.GetViewportOrigin(), screenWidth, screenHeight);
		const EInputReadResult inputResult = inputManager.Update();
		if (inputResult == EInputReadResult::Error || inputResult == EInputReadResult::Closed)
		{
			// 부분 입력은 진단용이며 UI 행동에 전달하지 않는다.
			std::wcerr << L"Console input was closed or failed.\n";
			RequestExit(1);
			return;
		}
		if (inputResult == EInputReadResult::Event)
		{
			const SSceneAction action = sceneManager.HandleInputFrame(inputManager.GetEvents());
			if (action.Type == ESceneAction::Quit)
			{
				// 명시적 정상 Quit만 저장한다. OnStop/늦은 destructor Quit는 저장하지 않는다.
				sceneManager.CancelPendingTransitions();
				if (!session.Save(dataError))
				{
					sceneManager.ShowMessage(L"InGameData.Ui.SaveFailed");
					sceneManager.Render(renderer.GetBackBuffer());
					const bool presented = renderer.Present();
					std::wcerr << GetLocalizedText(store, L"InGameData.Ui.SaveFailed") << L'\n';
					std::cerr << "Save on exit failed: " << dataError << '\n';
					if (!presented) std::wcerr << L"Unable to present the save failure.\n";
					RequestExit(1);
					return;
				}
				RequestExit();
				return;
			}
			if (action.Type == ESceneAction::StartGame && !action.TargetSceneId.empty())
			{
				const auto result = session.NewGame(action.TargetSceneId, sceneManager, m_state->UiManager, dataError);
				m_state->StartingNewGame = result == EGameStartResult::Queued;
				if (result == EGameStartResult::Failed)
				{
					sceneManager.ShowMessage(L"InGameData.Ui.NewGameFailed");
					std::cerr << "New-game operation: " << dataError << '\n';
				}
			}
			else if (action.Type == ESceneAction::Continue)
			{
				m_state->StartingNewGame = false;
				if (!session.Continue(sceneManager, m_state->UiManager, dataError))
				{
					sceneManager.ShowMessage(L"InGameData.Ui.LoadFailed");
					std::cerr << "Continue operation: " << dataError << '\n';
				}
			}
		}
		if (sceneManager.HasPendingTransition() || sceneManager.IsQuitting()) SkipRemainingUpdates();
	}

	void FTextRpgFrame::OnUpdate(float deltaSeconds)
	{
		auto& sceneManager = m_state->SceneManager;
		if (sceneManager.HasPendingTransition() || sceneManager.IsQuitting())
		{
			SkipRemainingUpdates();
			return;
		}
		sceneManager.Update(deltaSeconds);
		if (sceneManager.HasPendingTransition() || sceneManager.IsQuitting()) SkipRemainingUpdates();
	}

	void FTextRpgFrame::OnRender()
	{
		auto& renderer = m_state->Renderer;
		m_state->SceneManager.Render(renderer.GetBackBuffer());
		if (!renderer.Present())
		{
			std::wcerr << L"Unable to present the console frame.\n";
			RequestExit(1);
		}
	}

	void FTextRpgFrame::OnEndFrame()
	{
		auto& sceneManager = m_state->SceneManager;
		auto& session = m_state->Session;
		auto& dataError = m_state->DataError;
		auto& errorMessage = m_state->ErrorMessage;
		// 의도적 Render 생략도 정상 안전점이다. 실제 입력/출력 실패에서는 여기에 진입하지 않는다.
		const bool published = sceneManager.EndFrame(errorMessage);
		const auto startResult = session.TakeStartResult(sceneManager, dataError);
		if (sceneManager.IsQuitting())
		{
			// 취소된 후보를 추가 게시/저장하거나 이미 해제된 Scene을 다시 그리지 않는다.
			RequestExit();
			return;
		}
		if (startResult == EGameStartResult::Failed || startResult == EGameStartResult::StartedSaveFailed)
		{
			sceneManager.ShowMessage(startResult == EGameStartResult::StartedSaveFailed ?
				L"InGameData.Ui.NewGameSaveFailed" : (m_state->StartingNewGame ? L"InGameData.Ui.NewGameFailed" : L"InGameData.Ui.LoadFailed"));
			std::cerr << "Progress publication: " << dataError << '\n';
		}
		if (!published) std::wcerr << L"Scene publication: " << errorMessage << L'\n';
	}

	void FTextRpgFrame::OnStop()
	{
		// 게임 저장 정책이 아니라 자원 정리다. 초기화 실패의 부분 상태도 같은 역순으로 해제한다.
		m_state.reset();
	}
}
