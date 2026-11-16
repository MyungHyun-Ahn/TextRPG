#include "GameData/pch.h"
#include "Common/GameDataEnums.g.h"
#include "Scene/SceneData.g.h"
#include "SceneOption/SceneOptionData.g.h"
#include "SceneTypeConfig/SceneTypeConfigData.g.h"
#include "Localization/LocalizationData.g.h"
#if __has_include("TextArt/TextArtData.g.h")
#include "TextArt/TextArtData.g.h"
#endif
#include "CharacterDefinition/CharacterDefinitionData.g.h"
#include "LevelDefinition/LevelDefinitionData.g.h"
#include "NewGameProfile/NewGameProfileData.g.h"
#include "ItemDefinition/ItemDefinitionData.g.h"
#include "AchievementDefinition/AchievementDefinitionData.g.h"
#include "GameData/FGameDataPackage.h"
#include "GameData/FGeneratedYaml.h"
#include "GameData/FGameDataStore.h"

namespace
{
	using namespace GameData;
	constexpr auto kMainMenuSceneId = static_cast<std::uint32_t>(Common::ESceneId::MainMenu);
	constexpr auto kNewGameTextKey = static_cast<std::uint32_t>(Common::ETextKey::MainMenu_NewGame);
	constexpr auto kMainLogoArtId = static_cast<std::uint32_t>(Common::EArtId::MainLogo);
	static_assert(std::is_same_v<decltype(Scene::SSceneData::sceneId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(SceneOption::SSceneOptionData::optionId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(SceneOption::SSceneOptionData::optionTextKey), std::uint32_t>);
	static_assert(std::is_same_v<decltype(SceneOption::SSceneOptionData::targetSceneId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(SceneTypeConfig::SSceneTypeConfigData::configId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(Localization::SLocalizationData::textKey), std::uint32_t>);
	static_assert(std::is_same_v<decltype(TextArt::STextArtData::artId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(CharacterDefinition::SCharacterDefinitionData::characterId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(CharacterDefinition::SCharacterDefinitionData::nameTextKey), std::uint32_t>);
	static_assert(std::is_same_v<decltype(CharacterDefinition::SCharacterDefinitionData::artId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(NewGameProfile::SNewGameProfileData::profileId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(NewGameProfile::SNewGameProfileData::characterId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(ItemDefinition::SItemDefinitionData::itemId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(ItemDefinition::SItemDefinitionData::nameTextKey), std::uint32_t>);
	static_assert(std::is_same_v<decltype(AchievementDefinition::SAchievementDefinitionData::achievementId), std::uint32_t>);
	static_assert(std::is_same_v<decltype(AchievementDefinition::SAchievementDefinitionData::nameTextKey), std::uint32_t>);
	auto Values(const Scene::SSceneData& row)
	{
		return std::make_tuple(row.sceneId, row.sceneType, row.isStartupScene);
	}
	auto Values(const SceneOption::SSceneOptionData& row)
	{
		return std::make_tuple(row.optionId, row.sceneType, row.optionOrder, row.optionTextKey,
			row.actionType, row.targetSceneId, row.isEnabled);
	}
	auto Values(const SceneTypeConfig::SSceneTypeConfigData& row)
	{
		return std::make_tuple(row.configId, row.sceneType, row.maxVisibleOptions, row.optionOverflowPolicy);
	}
	auto Values(const Localization::SLocalizationData& row)
	{
		return std::make_tuple(row.textKey, row.koKR, row.enUS);
	}
	auto Values(const TextArt::STextArtData& row)
	{
		return std::make_tuple(row.artId, row.assetPath, row.width, row.height);
	}
	auto Values(const CharacterDefinition::SCharacterDefinitionData& row)
	{
		return std::make_tuple(row.characterId, row.nameTextKey, row.artId);
	}
	auto Values(const LevelDefinition::SLevelDefinitionData& row)
	{
		return std::make_tuple(row.level, row.requiredExperience, row.maxHp, row.attack, row.defense);
	}
	auto Values(const NewGameProfile::SNewGameProfileData& row)
	{
		return std::make_tuple(row.profileId, row.characterId, row.initialLevel, row.initialExperience, row.initialGold,
			row.initialMaxHpBonus, row.initialAttackBonus, row.initialDefenseBonus, row.contentVersion, row.isDefault);
	}
	auto Values(const ItemDefinition::SItemDefinitionData& row)
	{
		return std::make_tuple(row.itemId, row.nameTextKey, row.maxStack);
	}
	auto Values(const AchievementDefinition::SAchievementDefinitionData& row)
	{
		return std::make_tuple(row.achievementId, row.nameTextKey, row.targetCount);
	}
	struct SFixtureTable final
	{
		const char* name;
		std::size_t entries{};
		std::size_t rows{};
		std::size_t checks{};
	};
	bool HasCompleteSnapshot(const std::array<SFixtureTable, 10>& tables)
	{
		return std::all_of(tables.begin(), tables.end(), [](const auto& table)
		{
			return table.entries == 1 && table.rows > 0 && table.checks == table.rows;
		});
	}
	template<class Lookup>
	void Remember(std::vector<std::function<void()>>& checks, Lookup lookup)
	{
		const auto* pointer = lookup();
		assert(pointer);
		const auto values = Values(*pointer);
		checks.emplace_back([lookup, pointer, values]()
		{
			assert(lookup() == pointer);
			assert(Values(*pointer) == values);
		});
	}
}

int main(int argc, char** argv)
{
	assert(argc == 3);
	const std::filesystem::path source(argv[1]);
	const std::filesystem::path work(argv[2]);
	const auto assets = work / "Asset";
	std::filesystem::copy(source / "Asset", assets, std::filesystem::copy_options::recursive |
		std::filesystem::copy_options::overwrite_existing);
	const auto yaml = work / "yaml";
	std::filesystem::create_directories(yaml);
	for (const auto& entry : std::filesystem::directory_iterator(source / "Data/Client"))
	{
		if (entry.path().extension() != ".yaml") continue;
		std::ifstream input(entry.path(), std::ios::binary);
		std::string text((std::istreambuf_iterator<char>(input)), {});
		if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3);
		if (!text.starts_with("# TextRPG.GameData ")) text.insert(0, "# TextRPG.GameData 1\n");
		std::ofstream(yaml / entry.path().filename(), std::ios::binary) << text;
	}
	std::array<std::uint8_t, 32> key{}; // 테스트 전용 메모리 키. 운영 키 공급 경로와 무관하다.
	std::string error;
	const auto package = work / "GameData.dat";
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, assets));
	GameData::FGameDataStore store;
	assert(store.Load(package, key, error));
	assert(error.empty());
	assert(store.FindScene(kMainMenuSceneId) && store.FindScene(kMainMenuSceneId)->isStartupScene);
	assert(store.FindLocalization(kNewGameTextKey)->koKR == L"새 게임");
	std::cout << "PASS authenticated package to typed runtime rows\n";
	const auto* prior = store.FindScene(kMainMenuSceneId);
	std::ifstream sceneInput(yaml / "Scene.yaml", std::ios::binary);
	std::string scenes((std::istreambuf_iterator<char>(sceneInput)), {});
	sceneInput.close();
	const auto startup = scenes.find("\"IsStartupScene\": true");
	assert(startup != std::string::npos);
	scenes.replace(startup, 22, "\"IsStartupScene\": false");
	std::ofstream(yaml / "Scene.yaml", std::ios::binary) << scenes;
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, assets));
	assert(!store.Load(package, key, error));
	assert(error.find("startup") != std::string::npos);
	assert(store.FindScene(kMainMenuSceneId) == prior && prior->isStartupScene);
	std::cout << "PASS startup semantic failure preserves published store\n";
	// 메모리 후보의 PK 유일성은 YAML mapping 이름의 유일성과 별도 계약이다.
	std::ifstream validSceneInput(source / "Data/Client/Scene.yaml", std::ios::binary);
	std::string validScenes((std::istreambuf_iterator<char>(validSceneInput)), {});
	if (validScenes.starts_with("\xEF\xBB\xBF")) validScenes.erase(0, 3);
	if (!validScenes.starts_with("# TextRPG.GameData ")) validScenes.insert(0, "# TextRPG.GameData 1\n");
	const auto townKey = validScenes.find("\"SceneId\": 2\n");
	assert(townKey != std::string::npos);
	validScenes.replace(townKey, std::string("\"SceneId\": 2\n").size(), "\"SceneId\": 1\n");
	std::ofstream(yaml / "Scene.yaml", std::ios::binary) << validScenes;
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, assets));
	assert(!store.Load(package, key, error));
	assert(error.find("duplicate") != std::string::npos);
	assert(store.FindScene(kMainMenuSceneId) == prior);
	std::cout << "PASS duplicate primary key preserves published store\n";
	// 모든 변형은 실제 인증 패키지를 다시 생성한다. 의미 오류와 GCM 실패를 혼동하지 않는다.
	std::map<std::string, std::string> baseline;
	for (const auto& entry : std::filesystem::directory_iterator(source / "Data/Client"))
	{
		if (entry.path().extension() != ".yaml") continue;
		std::ifstream input(entry.path(), std::ios::binary);
		std::string text((std::istreambuf_iterator<char>(input)), {});
		if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3);
		if (!text.starts_with("# TextRPG.GameData ")) text.insert(0, "# TextRPG.GameData 1\n");
		baseline.emplace(entry.path().filename().string(), text);
	}
	const auto replace = [](std::string& text, const std::string& from, const std::string& to)
	{
		const auto at = text.find(from);
		assert(at != std::string::npos);
		text.replace(at, from.size(), to);
	};
	// 게시된 모든 행의 포인터와 전체 필드를 보존하는지 모든 음성 변형 뒤 확인한다.
	std::vector<std::function<void()>> publishedChecks;
	std::size_t publishedRowCount = 0;
	std::array<SFixtureTable, 10> tables{{
		{"Scene.yaml"}, {"SceneOption.yaml"}, {"SceneTypeConfig.yaml"}, {"Localization.yaml"}, {"TextArt.yaml"},
		{"CharacterDefinition.yaml"}, {"LevelDefinition.yaml"}, {"NewGameProfile.yaml"},
		{"ItemDefinition.yaml"}, {"AchievementDefinition.yaml"}
	}};
	for (const auto& [file, bytes] : baseline)
	{
		const auto table = std::find_if(tables.begin(), tables.end(), [&file](const auto& candidate)
		{
			return file == candidate.name;
		});
		assert(table != tables.end());
		++table->entries;
		std::vector<GameData::SYamlRow> rows;
		const std::vector<std::uint8_t> contents(bytes.begin(), bytes.end());
		assert(GameData::ParseGeneratedYaml(contents, rows, error));
		table->rows += rows.size();
		for (const auto& row : rows)
		{
			const auto checksBeforeRow = publishedChecks.size();
			++publishedRowCount;
			if (file == "Scene.yaml")
			{
				const auto id = row.fields.at(L"SceneId").number;
				Remember(publishedChecks, [&store, id]() { return store.FindScene(id); });
			}
			else if (file == "SceneOption.yaml")
			{
				const auto id = row.fields.at(L"OptionId").number;
				Remember(publishedChecks, [&store, id]() { return store.FindSceneOption(id); });
			}
			else if (file == "SceneTypeConfig.yaml")
			{
				const auto type = row.fields.at(L"SceneType").text;
				assert(type == L"MainMenu" || type == L"Town" || type == L"BattleField");
				const auto id = type == L"MainMenu" ? GameData::Common::ESceneType::MainMenu :
					(type == L"Town" ? GameData::Common::ESceneType::Town : GameData::Common::ESceneType::BattleField);
				Remember(publishedChecks, [&store, id]() { return store.FindSceneTypeConfig(id); });
			}
			else if (file == "Localization.yaml")
			{
				const auto id = row.fields.at(L"TextKey").number;
				Remember(publishedChecks, [&store, id]() { return store.FindLocalization(id); });
			}
			else if (file == "TextArt.yaml")
			{
				const auto id = row.fields.at(L"ArtId").number;
				Remember(publishedChecks, [&store, id]() { return store.FindTextArt(id); });
			}
			else if (file == "CharacterDefinition.yaml")
			{
				const auto id = row.fields.at(L"CharacterId").number;
				Remember(publishedChecks, [&store, id]() { return store.FindCharacterDefinition(id); });
			}
			else if (file == "LevelDefinition.yaml")
			{
				const auto id = row.fields.at(L"Level").number;
				Remember(publishedChecks, [&store, id]() { return store.FindLevelDefinition(id); });
			}
			else if (file == "NewGameProfile.yaml")
			{
				const auto id = row.fields.at(L"ProfileId").number;
				Remember(publishedChecks, [&store, id]() { return store.FindNewGameProfile(id); });
			}
			else if (file == "ItemDefinition.yaml")
			{
				const auto id = row.fields.at(L"ItemId").number;
				Remember(publishedChecks, [&store, id]() { return store.FindItemDefinition(id); });
			}
			else if (file == "AchievementDefinition.yaml")
			{
				const auto id = row.fields.at(L"AchievementId").number;
				Remember(publishedChecks, [&store, id]() { return store.FindAchievementDefinition(id); });
			}
			else assert(false);
			assert(publishedChecks.size() == checksBeforeRow + 1);
			table->checks += publishedChecks.size() - checksBeforeRow;
		}
	}
	assert(publishedChecks.size() == publishedRowCount && publishedRowCount > 0);
	assert(HasCompleteSnapshot(tables));
	for (std::size_t index = 0; index < tables.size(); ++index)
	{
		auto missingTable = tables;
		missingTable[index].entries = 0;
		missingTable[index].rows = 0;
		missingTable[index].checks = 0;
		assert(!HasCompleteSnapshot(missingTable));
	}
	for (const auto& table : tables)
		std::cout << "COVERAGE " << table.name << " entries=" << table.entries << " rows=" << table.rows
			<< " checks=" << table.checks << '\n';
	std::size_t publishedAssetCount = 0;
	for (const auto& file : std::filesystem::recursive_directory_iterator(assets))
	{
		if (!file.is_regular_file()) continue;
		++publishedAssetCount;
		const auto name = (std::filesystem::path(L"Asset") / file.path().lexically_relative(assets)).generic_wstring();
		std::ifstream input(file.path(), std::ios::binary);
		const std::vector<std::uint8_t> expected((std::istreambuf_iterator<char>(input)), {});
		const auto* bytes = store.FindAsset(name);
		assert(bytes && *bytes == expected);
		const auto* buffer = bytes->data();
		publishedChecks.emplace_back([&store, name, bytes, buffer, expected]()
		{
			assert(store.FindAsset(name) == bytes && bytes->data() == buffer && *bytes == expected);
		});
	}
	assert(publishedAssetCount == 28);
	std::cout << "PASS all 28 referenced/unreferenced assets preserve exact raw bytes after temporary reader destruction\n";
	const auto* publishedScenes = store.GetScenes().data();
	const auto* publishedOptions = store.GetSceneOptions().data();
	const auto publishedSceneCount = store.GetScenes().size();
	const auto publishedOptionCount = store.GetSceneOptions().size();
	const auto assertPublished = [&]()
	{
		assert(store.GetScenes().data() == publishedScenes && store.GetSceneOptions().data() == publishedOptions);
		assert(store.GetScenes().size() == publishedSceneCount && store.GetSceneOptions().size() == publishedOptionCount);
		for (const auto& check : publishedChecks) check();
	};
	const auto reject = [&](const char* name, const char* table, const std::string& text, const char* diagnostic, bool omit = false)
	{
		for (const auto& entry : std::filesystem::directory_iterator(yaml)) std::filesystem::remove(entry.path());
		for (const auto& [file, bytes] : baseline) std::ofstream(yaml / file, std::ios::binary) << bytes;
		if (omit) std::filesystem::remove(yaml / table);
		else std::ofstream(yaml / table, std::ios::binary) << text;
		assert(GameData::BuildEncryptedPackage(yaml, package, key, error, assets));
		const bool loaded = store.Load(package, key, error);
		if (loaded || error.find(diagnostic) == std::string::npos)
		{
			std::cerr << name << ": loaded=" << loaded << " diagnostic=" << error << '\n';
			std::abort();
		}
		assert(store.FindScene(kMainMenuSceneId) == prior && prior->isStartupScene);
		assertPublished();
		std::cout << "PASS " << name << " rejects and rolls back\n";
	};
	const auto logoFile = assets / "Logo/Logo_Text_Rpg.txt";
	std::ifstream logoInput(logoFile, std::ios::binary);
	const std::string logoBytes((std::istreambuf_iterator<char>(logoInput)), {});
	logoInput.close();
	const auto rejectAsset = [&](const char* name, const std::string& bytes, const char* diagnostic, bool missing = false)
	{
		for (const auto& [file, text] : baseline) std::ofstream(yaml / file, std::ios::binary) << text;
		if (missing) std::filesystem::remove(logoFile);
		else std::ofstream(logoFile, std::ios::binary) << bytes;
		assert(GameData::BuildEncryptedPackage(yaml, package, key, error, assets));
		assert(!store.Load(package, key, error));
		assert(error.find(diagnostic) != std::string::npos);
		assertPublished();
		std::ofstream(logoFile, std::ios::binary) << logoBytes;
		std::cout << "PASS " << name << " preserves published tables and exact asset bytes\n";
	};
	rejectAsset("missing-referenced-asset", "", "AssetPath", true);
	rejectAsset("invalid-asset-utf8", std::string("\xff"), "UTF-8");
	rejectAsset("invalid-asset-glyph", std::string("	") + logoBytes.substr(1), "glyph");
	rejectAsset("invalid-asset-dimensions", logoBytes + "\n", "dimensions");
	rejectAsset("empty-referenced-asset", "", "size");
	std::string changed = baseline.at("TextArt.yaml");
	replace(changed, "Asset/Logo/Logo_Text_Rpg.txt", "Asset/Logo/logo_Text_Rpg.txt");
	reject("asset-key-case-exact", "TextArt.yaml", changed, "AssetPath");
	changed = baseline.at("TextArt.yaml");
	replace(changed, "\"Width\": 83", "\"Width\": 36");
	replace(changed, "\"Height\": 41", "\"Height\": 36");
	reject("main-logo-slot", "TextArt.yaml", changed, "slot");
	changed = baseline.at("SceneOption.yaml");
	replace(changed, "\"TargetSceneId\": 2\n", "\"TargetSceneId\": 999\n");
	reject("target-reference", "SceneOption.yaml", changed, "TargetSceneId");
	changed = baseline.at("SceneOption.yaml");
	replace(changed, "\"OptionOrder\": 4", "\"OptionOrder\": 3");
	reject("four-options-distinct-orders", "SceneOption.yaml", changed, "OptionOrder");
	changed = baseline.at("SceneOption.yaml");
	replace(changed, "\"OptionId\": 1\n", "\"OptionId\": 0\n");
	reject("metadata-OptionId-min", "SceneOption.yaml", changed,
		"SceneOption.yaml/optionId: primary key must be positive");
	changed = baseline.at("TextArt.yaml");
	replace(changed, "\"Width\": 83", "\"Width\": 84");
	reject("TextArt-dimension-pair", "TextArt.yaml", changed, "Width/Height");
	changed = baseline.at("Scene.yaml");
	replace(changed, "  \"IsStartupScene\": true", "  \"AssetPath\": \"Asset/retired.txt\"\n  \"IsStartupScene\": true");
	reject("legacy-Scene-AssetPath", "Scene.yaml", changed, "unknown or missing field");
	changed = baseline.at("TextArt.yaml");
	replace(changed, "Asset/Logo/Logo_Text_Rpg.txt", "../outside.txt");
	reject("AssetPath-traversal", "TextArt.yaml", changed, "AssetPath");
	changed = baseline.at("Localization.yaml");
	replace(changed, "\"TextKey\": 10\n", "\"TextKey\": 999\n");
	reject("mandatory-UI-localization", "Localization.yaml", changed, "Localization.yaml: required TextKey 10 is missing");
	struct SMutation
	{
		const char* name;
		const char* table;
		const char* from;
		const char* to;
		const char* diagnostic;
	};
	const SMutation mutations[] = {
		{"version-unsupported", "Scene.yaml", "# TextRPG.GameData 1", "# TextRPG.GameData 2", "version"},
		{"version-missing", "Scene.yaml", "# TextRPG.GameData 1\n", "", "version"},
		{"unknown-field", "Scene.yaml", "  \"SceneId\":", "  \"OtherId\":", "SceneId"},
		{"unknown-table", "Other.yaml", "", "", "unknown table"},
		{"enum-scene", "Scene.yaml", "\"SceneType\": \"MainMenu\"", "\"SceneType\": \"Invalid\"", "SceneType"},
		{"enum-action", "SceneOption.yaml", "\"ActionType\": \"StartGame\"", "\"ActionType\": \"Invalid\"", "ActionType"},
		{"enum-policy", "SceneTypeConfig.yaml", "\"OptionOverflowPolicy\": \"NoScroll\"", "\"OptionOverflowPolicy\": \"Invalid\"", "enum"},
		{"forbid-scroll", "SceneTypeConfig.yaml", "\"NoScroll\"", "\"Scroll\"", "OptionOverflowPolicy"},
		{"config-four", "SceneTypeConfig.yaml", "\"MaxVisibleOptions\": 4", "\"MaxVisibleOptions\": 5", "MaxVisibleOptions"},
		{"order-range", "SceneOption.yaml", "\"OptionOrder\": 1", "\"OptionOrder\": 5", "OptionOrder"},
		{"uint32-overflow", "SceneOption.yaml", "\"OptionId\": 1\n", "\"OptionId\": 4294967296\n", "range"},
		{"uint32-negative", "SceneOption.yaml", "\"OptionId\": 1\n", "\"OptionId\": -1\n", "scalar"},
		{"uint32-quoted", "SceneOption.yaml", "\"OptionId\": 1\n", "\"OptionId\": \"1\"\n", "OptionId"},
		{"numeric-key-quoted-Scene-SceneId", "Scene.yaml",
			"\"SceneId\": 1\n", "\"SceneId\": \"1\"\n", "SceneId"},
		{"numeric-key-quoted-SceneTypeConfig-ConfigId", "SceneTypeConfig.yaml",
			"\"ConfigId\": 1\n", "\"ConfigId\": \"1\"\n", "ConfigId"},
		{"numeric-key-quoted-Localization-TextKey", "Localization.yaml",
			"\"TextKey\": 1\n", "\"TextKey\": \"1\"\n", "TextKey"},
		{"numeric-key-quoted-TextArt-ArtId", "TextArt.yaml",
			"\"ArtId\": 1\n", "\"ArtId\": \"1\"\n", "ArtId"},
		{"numeric-key-quoted-CharacterDefinition-CharacterId", "CharacterDefinition.yaml",
			"\"CharacterId\": 1\n", "\"CharacterId\": \"1\"\n", "CharacterId"},
		{"numeric-key-quoted-LevelDefinition-Level", "LevelDefinition.yaml",
			"\"Level\": 1\n", "\"Level\": \"1\"\n", "Level"},
		{"numeric-key-quoted-NewGameProfile-ProfileId", "NewGameProfile.yaml",
			"\"ProfileId\": 1\n", "\"ProfileId\": \"1\"\n", "ProfileId"},
		{"numeric-key-quoted-ItemDefinition-ItemId", "ItemDefinition.yaml",
			"\"ItemId\": 1\n", "\"ItemId\": \"1\"\n", "ItemId"},
		{"numeric-key-quoted-AchievementDefinition-AchievementId", "AchievementDefinition.yaml",
			"\"AchievementId\": 1\n", "\"AchievementId\": \"1\"\n", "AchievementId"},
		{"numeric-key-quoted-SceneOption-OptionTextKey", "SceneOption.yaml",
			"\"OptionTextKey\": 1\n", "\"OptionTextKey\": \"1\"\n", "OptionTextKey"},
		{"numeric-key-quoted-SceneOption-TargetSceneId", "SceneOption.yaml",
			"\"TargetSceneId\": 2\n", "\"TargetSceneId\": \"2\"\n", "TargetSceneId"},
		{"numeric-key-quoted-CharacterDefinition-NameTextKey", "CharacterDefinition.yaml",
			"\"NameTextKey\": 31\n", "\"NameTextKey\": \"31\"\n", "NameTextKey"},
		{"numeric-key-quoted-CharacterDefinition-ArtId", "CharacterDefinition.yaml",
			"\"ArtId\": 2\n", "\"ArtId\": \"2\"\n", "ArtId"},
		{"numeric-key-quoted-NewGameProfile-CharacterId", "NewGameProfile.yaml",
			"\"CharacterId\": 1\n", "\"CharacterId\": \"1\"\n", "CharacterId"},
		{"numeric-key-quoted-ItemDefinition-NameTextKey", "ItemDefinition.yaml",
			"\"NameTextKey\": 32\n", "\"NameTextKey\": \"32\"\n", "NameTextKey"},
		{"numeric-key-quoted-AchievementDefinition-NameTextKey", "AchievementDefinition.yaml",
			"\"NameTextKey\": 33\n", "\"NameTextKey\": \"33\"\n", "NameTextKey"},
		{"bool-quoted", "Scene.yaml", "\"IsStartupScene\": true", "\"IsStartupScene\": \"true\"", "IsStartupScene"},
		{"invalid-utf8", "Localization.yaml", "New Game", "\xff", "UTF-8"},
		{"invalid-escape", "Localization.yaml", "New Game", "bad\\q", "escape"},
		{"unpaired-surrogate", "Localization.yaml", "New Game", "\\uD800", "escape"},
		{"raw-nul", "Localization.yaml", "New Game", "\\u0000", "escape"},
		{"yaml-sequence", "Scene.yaml", "\"Scene1\":", "- \"Scene1\":", "mapping"},
		{"yaml-anchor", "Scene.yaml", "\"IsStartupScene\": true", "\"IsStartupScene\": &x true", "scalar"},
		{"yaml-flow", "Scene.yaml", "\"Scene1\":", "\"Scene1\": {}", "row key"},
		{"yaml-comment", "Scene.yaml", "\"IsStartupScene\": true", "\"IsStartupScene\": true # comment", "scalar"},
		{"duplicate-field", "Scene.yaml", "  \"SceneId\": 1\n", "  \"SceneId\": 1\n  \"SceneId\": 999\n", "duplicate field"},
		{"duplicate-mapping", "Scene.yaml", "\"Scene2\":", "\"Scene1\":", "duplicate"},
		{"duplicate-option-PK", "SceneOption.yaml", "\"OptionId\": 2\n", "\"OptionId\": 1\n", "duplicate"},
		{"duplicate-config-PK", "SceneTypeConfig.yaml", "\"ConfigId\": 2\n", "\"ConfigId\": 1\n", "duplicate"},
		{"duplicate-config-type", "SceneTypeConfig.yaml", "\"SceneType\": \"Town\"",
			"\"SceneType\": \"MainMenu\"", "SceneType"},
		{"duplicate-loc-PK", "Localization.yaml", "\"TextKey\": 2\n", "\"TextKey\": 1\n", "duplicate"},
		{"duplicate-art-PK", "TextArt.yaml", "\"ArtId\": 3\n", "\"ArtId\": 1\n", "duplicate"},
		{"empty-required", "Localization.yaml", "\"EnUS\": \"New Game\"", "\"EnUS\": \"\"", "EnUS"},
		{"blank-required", "Localization.yaml", "\"EnUS\": \"New Game\"", "\"EnUS\": \"   \"", "EnUS"},
		{"blank-unicode-required", "Localization.yaml", "\"EnUS\": \"New Game\"", "\"EnUS\": \"　\"", "EnUS"},
		{"option-loc-reference", "SceneOption.yaml", "\"OptionTextKey\": 1\n", "\"OptionTextKey\": 999\n", "OptionTextKey"},
		{"disabled-target-reference", "SceneOption.yaml",
			"\"ActionType\": \"Settings\"\n  \"TargetSceneId\": 0\n  \"IsEnabled\": false",
			"\"ActionType\": \"Settings\"\n  \"TargetSceneId\": 999\n  \"IsEnabled\": false", "TargetSceneId"},
		{"enabled-target-zero", "SceneOption.yaml", "\"TargetSceneId\": 2\n", "\"TargetSceneId\": 0\n", "TargetSceneId"},
		{"enabled-target-null", "SceneOption.yaml", "\"TargetSceneId\": 2\n", "\"TargetSceneId\": null\n", "TargetSceneId"},
		{"startup-multiple", "Scene.yaml", "\"IsStartupScene\": false", "\"IsStartupScene\": true", "startup"},
		{"title-required", "Localization.yaml", "\"TextKey\": 17\n", "\"TextKey\": 999\n",
			"Localization.yaml: required TextKey 17 is missing"},
		{"path-absolute", "TextArt.yaml", "Asset/Logo/Logo_Text_Rpg.txt", "C:/outside.txt", "AssetPath"},
		{"path-ADS", "TextArt.yaml", "Asset/Logo/Logo_Text_Rpg.txt", "Asset/file.txt:stream", "AssetPath"},
		{"path-device", "TextArt.yaml", "Asset/Logo/Logo_Text_Rpg.txt", "Asset/CON.txt", "AssetPath"},
		{"path-console-device", "TextArt.yaml", "Asset/Logo/Logo_Text_Rpg.txt", "Asset/CONIN$.txt", "AssetPath"},
		{"path-superscript-device", "TextArt.yaml", "Asset/Logo/Logo_Text_Rpg.txt", "Asset/COM¹.txt", "AssetPath"},
		{"path-device-preextension-space", "TextArt.yaml", "Asset/Logo/Logo_Text_Rpg.txt", "Asset/CON .txt", "reserved device"},
		{"path-empty-component", "TextArt.yaml", "Asset/Logo/Logo_Text_Rpg.txt", "Asset//file.txt", "AssetPath"},
		{"path-trailing-space", "TextArt.yaml", "Asset/Logo/Logo_Text_Rpg.txt", "Asset/file.txt ", "AssetPath"},
		{"art-height-range", "TextArt.yaml", "\"Height\": 41", "\"Height\": 44", "Width/Height"}
	};
	for (const auto& mutation : mutations)
	{
		changed = baseline.contains(mutation.table) ? baseline.at(mutation.table) : std::string{};
		if (*mutation.from) replace(changed, mutation.from, mutation.to);
		reject(mutation.name, mutation.table, changed, mutation.diagnostic);
	}
	// 예약 장치 이름은 YAML metadata로만 사용하며 해당 이름의 파일은 생성/열기하지 않는다.
	const auto yamlQuoted = [](const std::string& path)
	{
		std::string quoted = "\"";
		for (const char c : path)
		{
			if (c == '\\' || c == '"') quoted += '\\';
			quoted += c;
		}
		return quoted + '"';
	};
	std::vector<std::string> deviceNames{"con", "PrN", "aUx", "NuL", "conIN$", "ConOut$"};
	for (const auto* prefix : {"cOm", "LpT"})
	{
		for (char digit = '1'; digit <= '9'; ++digit) deviceNames.push_back(std::string(prefix) + digit);
		for (const auto* digit : {"¹", "²", "³"}) deviceNames.push_back(std::string(prefix) + digit);
	}
	std::vector<std::string> reservedPaths;
	for (const auto& device : deviceNames)
	{
		reservedPaths.push_back("Asset/" + device);
		reservedPaths.push_back("Asset/Nested/" + device + ".txt");
		reservedPaths.push_back("Asset/" + device + ".txt/leaf.txt");
		reservedPaths.push_back("Asset/" + device + " .txt");
	}
	for (const auto* whitespace : {"\u0085", "\u00A0", "\u1680", "\u2000", "\u2001", "\u2002", "\u2003", "\u2004", "\u2005", "\u2006",
		"\u2007", "\u2008", "\u2009", "\u200A", "\u2028", "\u2029", "\u202F", "\u205F", "\u3000"})
	{
		reservedPaths.push_back(std::string("Asset/Nested/cOnOuT$") + whitespace + ".txt");
		reservedPaths.push_back(std::string("Asset/lPt² ") + whitespace + " .txt");
	}
	std::size_t pathRejectCount = 0;
	std::size_t pathAcceptCount = 0;
	for (const auto* table : {"TextArt.yaml"})
	{
		const std::string original = yamlQuoted("Asset/Logo/Logo_Text_Rpg.txt");
		for (const auto& path : reservedPaths)
		{
			changed = baseline.at(table);
			replace(changed, original, yamlQuoted(path));
			const auto name = std::string("reserved-matrix-") + table + '-' + std::to_string(++pathRejectCount);
			reject(name.c_str(), table, changed, "AssetPath contains a reserved device name");
			assert(error.starts_with(std::string(table) + ':'));
		}
		std::vector<std::string> acceptedPaths;
		for (const auto* name : {"COM0", "COM10", "COM1x", "LPT0", "LPT10", "LPT1x", "CONIN", "CONOUT", "CONSOLE",
			"CONIN$x", "CONOUT$x", "COM¹x", "LPT²x", "COM⁴", "LPT⁴", "xCON", "auxiliary"})
		{
			acceptedPaths.push_back(std::string("Asset/") + name + ".txt");
			acceptedPaths.push_back(std::string("Asset/") + name + "/leaf.txt");
		}
		acceptedPaths.push_back("Asset/CoM10 .txt");
		acceptedPaths.push_back("Asset/Nested/file\u00A0.txt");
		acceptedPaths.push_back("Asset/Nested/file\u3000 .txt");
		acceptedPaths.push_back("Asset/Nested/Name with spaces.TXT");
		acceptedPaths.push_back("Asset\\MixedCase\\cOm0.txt");
		for (const auto& path : acceptedPaths)
		{
			for (const auto& [file, bytes] : baseline) std::ofstream(yaml / file, std::ios::binary) << bytes;
			changed = baseline.at(table);
			replace(changed, original, yamlQuoted(path));
			std::ofstream(yaml / table, std::ios::binary) << changed;
			const auto relative = std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size())).lexically_relative(L"Asset");
			std::filesystem::create_directories((assets / relative).parent_path());
			std::filesystem::copy_file(source / "Asset/Logo/Logo_Text_Rpg.txt", assets / relative,
				std::filesystem::copy_options::overwrite_existing);
			assert(GameData::BuildEncryptedPackage(yaml, package, key, error, assets));
			GameData::FGameDataStore accepted;
			assert(accepted.Load(package, key, error) && error.empty());
			const auto length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), nullptr, 0);
			assert(length > 0);
			std::wstring expected(static_cast<std::size_t>(length), L'\0');
			assert(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()),
				expected.data(), length) == length);
			const auto& actual = accepted.FindTextArt(kMainLogoArtId)->assetPath;
			assert(actual == expected);
			assertPublished();
			std::cout << "PASS accepted-path-" << table << '-' << ++pathAcceptCount << " preserves raw AssetPath\n";
		}
	}
	std::cout << "PATH-MATRIX rejected=" << pathRejectCount << " accepted=" << pathAcceptCount
		<< " preservedRows=" << publishedRowCount << '\n';
	const std::map<std::string, std::vector<std::string>> requiredFields{
		{"Scene.yaml", {"SceneId", "SceneType", "IsStartupScene"}},
		{"SceneOption.yaml", {"OptionId", "SceneType", "OptionOrder", "OptionTextKey", "ActionType", "IsEnabled"}},
		{"SceneTypeConfig.yaml", {"ConfigId", "SceneType", "MaxVisibleOptions", "OptionOverflowPolicy"}},
		{"Localization.yaml", {"TextKey", "KoKR", "EnUS"}},
		{"TextArt.yaml", {"ArtId", "AssetPath", "Width", "Height"}},
		{"CharacterDefinition.yaml", {"CharacterId", "NameTextKey", "ArtId"}},
		{"LevelDefinition.yaml", {"Level", "RequiredExperience", "MaxHp", "Attack", "Defense"}},
		{"NewGameProfile.yaml", {"ProfileId", "CharacterId", "InitialLevel", "InitialExperience", "InitialGold",
			"InitialMaxHpBonus", "InitialAttackBonus", "InitialDefenseBonus", "ContentVersion", "IsDefault"}},
		{"ItemDefinition.yaml", {"ItemId", "NameTextKey", "MaxStack"}},
		{"AchievementDefinition.yaml", {"AchievementId", "NameTextKey", "TargetCount"}}
	};
	for (const auto& [table, fields] : requiredFields)
	{
		for (const auto& field : fields)
		{
			changed = baseline.at(table);
			const std::string prefix = "  \"" + field + "\": ";
			const auto at = changed.find(prefix);
			assert(at != std::string::npos);
			const auto end = changed.find('\n', at);
			changed.replace(at + prefix.size(), end - at - prefix.size(), "null");
			const std::string name = "required-null-" + table + "-" + field;
			reject(name.c_str(), table.c_str(), changed, field.c_str());
		}
	}
	for (const auto* table : {"Scene.yaml", "SceneOption.yaml", "SceneTypeConfig.yaml", "Localization.yaml"})
		reject((std::string("missing-table-") + table).c_str(), table, "", "required table", true);
	for (const auto* table : {"CharacterDefinition.yaml", "LevelDefinition.yaml", "NewGameProfile.yaml",
		"ItemDefinition.yaml", "AchievementDefinition.yaml"})
		reject((std::string("missing-progress-table-") + table).c_str(), table, "", "cohort", true);
	// 진행 정의가 남은 채 TextArt만 빠지면 필수 ArtId 참조 오류다.
	reject("required-character-art-catalog", "TextArt.yaml", "", "ArtId reference is missing", true);
	// 필수 열 삭제 및 scalar 종류 오류도 각 테이블의 모든 필수 열에 대해 검사한다.
	for (const auto& [table, fields] : requiredFields)
	{
		for (const auto& field : fields)
		{
			const std::string prefix = "  \"" + field + "\": ";
			changed = baseline.at(table);
			const auto at = changed.find(prefix);
			const auto end = changed.find('\n', at);
			const bool isString = changed[at + prefix.size()] == '"';
			changed.replace(at + prefix.size(), end - at - prefix.size(), isString ? "false" : "\"wrong\"");
			reject(("wrong-kind-" + table + "-" + field).c_str(), table.c_str(), changed, field.c_str());
			changed = baseline.at(table);
			changed.erase(at, end + 1 - at);
			reject(("missing-field-" + table + "-" + field).c_str(), table.c_str(), changed, "missing field");
		}
	}
	// MainMenu의 비활성 행도 총 슬롯 수에 포함한다.
	const auto& allOptions = baseline.at("SceneOption.yaml");
	const auto bodyStart = allOptions.find('\n') + 1;
	std::vector<std::string> menuRows;
	std::string otherRows;
	for (std::size_t at = bodyStart; at < allOptions.size();)
	{
		const auto next = allOptions.find("\n\"", at);
		const auto stop = next == std::string::npos ? allOptions.size() : next + 1;
		const auto row = allOptions.substr(at, stop - at);
		if (row.find("\"SceneType\": \"MainMenu\"") != std::string::npos) menuRows.push_back(row);
		else otherRows += row;
		at = stop;
	}
	assert(menuRows.size() == 4);
	for (const std::size_t count : {0u, 1u, 3u, 5u, 8u})
	{
		changed = allOptions.substr(0, bodyStart) + otherRows;
		for (std::size_t i = 0; i < count; ++i)
		{
			std::string row = menuRows[i % 4];
			if (i >= 4)
			{
				const auto nameEnd = row.find('\n');
				row.replace(0, nameEnd, "\"SceneOption" + std::to_string(9000 + i) + "\":");
				const auto id = row.find("  \"OptionId\": ");
				const auto end = row.find('\n', id);
				row.replace(id, end - id, "  \"OptionId\": " + std::to_string(9000 + i));
			}
			changed += row;
		}
		reject(("four-option-count-" + std::to_string(count)).c_str(), "SceneOption.yaml", changed,
			count < 4 ? "four options" : "OptionOrder");
	}
	// 인증 실패가 YAML 해석보다 먼저이며, 어떤 실패도 게시된 행을 비우지 않는다.
	const auto assertPackageFailure = [&](const std::filesystem::path& pathToLoad, const auto& loadKey, const char* diagnostic)
	{
		assert(!store.Load(pathToLoad, loadKey, error));
		assert(error.find(diagnostic) != std::string::npos);
		assert(error.find("YAML line") == std::string::npos);
		assert(store.FindScene(kMainMenuSceneId) == prior);
		assertPublished();
	};
	changed = baseline.at("Scene.yaml");
	replace(changed, "\"IsStartupScene\": true", "\"IsStartupScene\": malformed");
	for (const auto& [file, bytes] : baseline) std::ofstream(yaml / file, std::ios::binary) << bytes;
	std::ofstream(yaml / "Scene.yaml", std::ios::binary) << changed;
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, assets));
	std::ifstream encryptedInput(package, std::ios::binary);
	std::string encrypted((std::istreambuf_iterator<char>(encryptedInput)), {});
	encryptedInput.close();
	assert(encrypted.size() > 64);
	encrypted.back() ^= 1;
	std::ofstream(package, std::ios::binary) << encrypted;
	assertPackageFailure(package, key, "authentication");
	std::cout << "PASS GCM authentication occurs before malformed YAML parsing and preserves rows\n";
	encrypted.back() ^= 1;
	std::ofstream(package, std::ios::binary) << encrypted;
	auto wrongKey = key;
	wrongKey[0] ^= 1;
	assertPackageFailure(package, wrongKey, "authentication");
	assertPackageFailure(work / "missing.dat", key, "GameData.dat");
	std::ofstream(package, std::ios::binary) << encrypted.substr(0, 10);
	assertPackageFailure(package, key, "GameData.dat");
	encrypted[4] = 1;
	std::ofstream(package, std::ios::binary) << encrypted;
	assertPackageFailure(package, key, "version");
	std::cout << "PASS wrong key missing truncated unsupported encrypted format preserve rows\n";
	// 데이터만 변경한 인증 패키지를 같은 저장소에 재로드하고 UTF-16 코드 단위를 검증한다.
	for (const auto& [file, bytes] : baseline) std::ofstream(yaml / file, std::ios::binary) << bytes;
	changed = baseline.at("Localization.yaml");
	replace(changed, "\"KoKR\": \"새 게임\"", R"fixture("KoKR": "\uC0C8 게임 \"quote\" \\ \n \u0009 \uD83D\uDE00")fixture");
	std::ofstream(yaml / "Localization.yaml", std::ios::binary) << changed;
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, assets));
	assert(store.Load(package, key, error));
	assert(store.FindLocalization(kNewGameTextKey)->koKR == L"새 게임 \"quote\" \\ \n 	 \U0001F600");
	assert(store.FindScene(kMainMenuSceneId));
	std::cout << "PASS successful data-only reload preserves escaped Korean supplementary UTF-16 text\n";
	// TextArt 헤더 유무와 관계없이 선택 카탈로그가 없는 패키지도 정상이다.
	for (const auto* table : {"TextArt.yaml", "CharacterDefinition.yaml", "LevelDefinition.yaml", "NewGameProfile.yaml",
		"ItemDefinition.yaml", "AchievementDefinition.yaml"}) std::filesystem::remove(yaml / table);
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, assets));
	assert(store.Load(package, key, error));
	assert(store.FindTextArt(kMainLogoArtId) == nullptr && store.FindScene(kMainMenuSceneId));
	assert(store.GetDefaultNewGameProfile() == nullptr);
	std::cout << "PASS optional TextArt absence publishes a complete store\n";

}
