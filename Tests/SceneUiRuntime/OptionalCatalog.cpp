#include "pch.h"
#include "Common/GameDataEnums.g.h"
#include "UI/FUIManager.h"
#include "UI/FGameUiBridge.h"
#include "Scene/FDataScene.h"
#include "Rendering/FFrameBuffer.h"
#include <cassert>
#include "GameData/FGameDataPackage.h"
#include "GameData/FGameDataStore.h"
#include "RuntimeFixture.h"

int main(int argc, char** argv)
{
	assert(argc == 3 || argc == 4);
	GameData::FGameDataStore store;
	LoadRuntimeFixture(store, std::filesystem::path(argv[1]), std::filesystem::path(argv[2]), false, argc == 4 ? std::filesystem::path(argv[3]) : std::filesystem::path{});
	TextRPG::FGameUiBridge bridge;
	TextRPG::FDataScene scene(store, bridge);
	std::wstring error;
	assert(scene.LoadFromGameData(L"MainMenu", error));
	scene.OnEnter();
	TextRPG::FFrameBuffer frame(132, 43);
	scene.Render(frame);
	assert(frame.GetCells()[86] == L'┌');
	std::cout << "PASS optional TextArt catalog absent builds and loads real Scene\n";
}
