#include "pch.h"
#include "Common/GameDataEnums.g.h"
#include "UI/FUIManager.h"
#include "UI/FGameUiBridge.h"
#include "Scene/FDataScene.h"
#include "Scene/TSceneManager.h"
#include "Scene/FTextRPGSceneManager.h"
#include "Rendering/FFrameBuffer.h"
#include <cassert>
#include "GameData/FGameDataPackage.h"
#include "GameData/FGameDataStore.h"
#include "RuntimeFixture.h"

int main(int argc, char** argv)
{
	assert(argc == 3 || argc == 4);
	static_assert(std::is_same_v<GameData::TextArt::STextArtData::KeyType, std::uint32_t>);
	static_assert(std::is_same_v<decltype(GameData::TextArt::STextArtData::artId), std::uint32_t>);
	GameData::FGameDataStore store;
	LoadRuntimeFixture(store, std::filesystem::path(argv[1]), std::filesystem::path(argv[2]), false, argc == 4 ? std::filesystem::path(argv[3]) : std::filesystem::path{});
	assert(store.FindTextArt(static_cast<std::uint32_t>(GameData::Common::EArtId::MainLogo)) == nullptr);
	assert(store.GetDefaultNewGameProfile() == nullptr);
	TextRPG::FGameUiBridge bridge;
	TextRPG::FDataScene scene(store, bridge);
	std::wstring error;
	assert(scene.LoadFromGameData(static_cast<std::uint32_t>(GameData::Common::ESceneId::MainMenu), error));
	scene.OnEnter();
	TextRPG::FFrameBuffer frame(132, 43);
	scene.Render(frame);
	assert(frame.GetCells()[86] == L'┌');
	TextRPG::FGameUiBridge managedBridge;
	TextRPG::FTextRPGSceneManager manager(store);
	assert(manager.LoadStartupScene(managedBridge, error));
	assert(!managedBridge.GetBoundScene() && manager.HasPendingTransition());
	assert(manager.EndFrame(error) && managedBridge.GetBoundScene());
	manager.Render(frame);
	assert(frame.GetCells()[86] == L'┌');
	std::cout << "PASS optional TextArt catalog absent builds and loads real Scene\n";
}
