#include "pch.h"
#include "GameDataAll.h"
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
#include "Frame/FFramePipeline.h"

int main()
{
	std::wstring errorMessage;
	std::filesystem::path packagePath;
	if (!TextRPG::ResolveGameDataPackagePath(packagePath, errorMessage))
	{
		std::wcerr << errorMessage << L'\n';
		return 1;
	}
	std::array<std::uint8_t, 32> key{};
	std::string dataError;
	GameData::FGameDataStore store;
	if (!GameData::ReadGameDataKey(key, dataError) || !store.Load(packagePath, key, dataError))
	{
		SecureZeroMemory(key.data(), key.size());
		std::wcerr << L"GameData load failed: " << packagePath.wstring() << L'\n'
			<< std::wstring(dataError.begin(), dataError.end()) << L'\n';
		return 1;
	}
	SecureZeroMemory(key.data(), key.size());

	// 역순 파괴: Scene/UI 비소유 뷰가 먼저, 앱 진행 루트가 마지막이다.
	TextRPG::FGameSession session(store);
	TextRPG::FGameUiBridge uiManager;
	TextRPG::FSceneManager sceneManager(store);
	const bool canContinue = session.CanContinue(sceneManager, uiManager, dataError);
	sceneManager.SetContinueAvailable(canContinue);
	if (!sceneManager.LoadStartupScene(uiManager, {}, errorMessage) || !sceneManager.EndFrame(errorMessage))
	{
		std::wcerr << errorMessage << L'\n';
		return 1;
	}

	if (!canContinue) sceneManager.ShowMessage(L"InGameData.Ui.LoadFailed");

	TextRPG::FConsoleInput consoleInput;
	if (!consoleInput.Initialize(errorMessage))
	{
		std::wcerr << errorMessage << L'\n';
		return 1;
	}

	TextRPG::FInputManager inputManager(consoleInput);
	TextRPG::FConsoleRenderer renderer;
	TextRPG::FFramePipeline framePipeline;
	bool isRunning = true;
	bool startingNewGame = false;
	auto previousFrameTime = std::chrono::steady_clock::now();
	while (isRunning)
	{
		const auto frameStartTime = std::chrono::steady_clock::now();
		const float deltaSeconds = std::chrono::duration<float>(frameStartTime - previousFrameTime).count();
		previousFrameTime = frameStartTime;

		if (!renderer.BeginFrame())
			{
			std::wcerr << L"Unable to read the console viewport.\n";
			return 1;
		}

		int frameExitCode = 0;
		const bool frameCompleted = framePipeline.Run([&]()
		{
			const int screenWidth = renderer.GetBackBuffer().GetWidth();
			const int screenHeight = renderer.GetBackBuffer().GetHeight();
			consoleInput.BeginBatch(renderer.GetViewportOrigin(), screenWidth, screenHeight);
			const TextRPG::EInputReadResult inputResult = inputManager.Update();
			if (inputResult == TextRPG::EInputReadResult::Error || inputResult == TextRPG::EInputReadResult::Closed)
			{
				// 부분 프레임은 진단용일 뿐이며 UI 행동에 사용하지 않는다.
				std::wcerr << L"Console input was closed or failed.\n";
				frameExitCode = 1;
				return false;
			}
			if (inputResult == TextRPG::EInputReadResult::Event)
			{
				const TextRPG::SSceneAction action = sceneManager.HandleInputFrame(inputManager.GetEvents());
				if (action.Type == TextRPG::ESceneAction::Quit)
				{
					isRunning = false;
				}
				else if (action.Type == TextRPG::ESceneAction::StartGame && !action.TargetSceneId.empty())
				{
					const auto result = session.NewGame(action.TargetSceneId, sceneManager, uiManager, dataError);
					startingNewGame = result == TextRPG::EGameStartResult::Queued;
					if (result == TextRPG::EGameStartResult::Failed)
					{
						sceneManager.ShowMessage(L"InGameData.Ui.NewGameFailed");
						std::cerr << "New-game operation: " << dataError << '\n';
					}
				}
				else if (action.Type == TextRPG::ESceneAction::Continue)
				{
					startingNewGame = false;
					if (!session.Continue(sceneManager, uiManager, dataError))
					{
						sceneManager.ShowMessage(L"InGameData.Ui.LoadFailed");
						std::cerr << "Continue operation: " << dataError << '\n';
					}
				}
			}

			if (!isRunning)
			{
				// Escape/Quit는 즉시 종료한다. 미게시 전환을 실행하지 않고 현재 진행만 저장한다.
				sceneManager.CancelPendingTransitions();
				if (!session.Save(dataError))
				{
					sceneManager.ShowMessage(L"InGameData.Ui.SaveFailed");
					sceneManager.Render(renderer.GetBackBuffer());
					const bool presented = renderer.Present();
					std::wcerr << TextRPG::GetLocalizedText(store, L"InGameData.Ui.SaveFailed") << L'\n';
					std::cerr << "Save on exit failed: " << dataError << '\n';
					if (!presented) std::wcerr << L"Unable to present the save failure.\n";
					frameExitCode = 1;
					return false;
				}
				return false;
			}

			return true;
		}, [&]()
		{
			sceneManager.Update(deltaSeconds);
		}, [&]()
		{
			sceneManager.Render(renderer.GetBackBuffer());
			if (!renderer.Present())
			{
				std::wcerr << L"Unable to present the console frame.\n";
				frameExitCode = 1;
				return false;
			}

			return true;
		}, [&]()
		{
			// Input -> Update -> Render/Present -> EndFrame. 이 지점만 활성 소유권을 바꾼다.
			const bool published = sceneManager.EndFrame(errorMessage);
			const auto startResult = session.TakeStartResult(sceneManager, dataError);
			if (sceneManager.IsQuitting())
			{
				// 파괴 콜백의 종료는 취소된 후보를 게시/저장하지 않는다. 이미 해제된 Scene도 다시 그리지 않는다.
				isRunning = false;
				return;
			}
			if (startResult == TextRPG::EGameStartResult::Failed || startResult == TextRPG::EGameStartResult::StartedSaveFailed)
			{
				sceneManager.ShowMessage(startResult == TextRPG::EGameStartResult::StartedSaveFailed ?
					L"InGameData.Ui.NewGameSaveFailed" : (startingNewGame ? L"InGameData.Ui.NewGameFailed" : L"InGameData.Ui.LoadFailed"));
				std::cerr << "Progress publication: " << dataError << '\n';
			}
			if (!published) std::wcerr << L"Scene publication: " << errorMessage << L'\n';
		});
		if (frameExitCode != 0) return frameExitCode;
		if (!frameCompleted || !isRunning) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	return 0;
}
