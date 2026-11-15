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
#include "InGameData/InGameData.h"
#include "GameData/FGameDataPackage.h"
#include "GameData/FGameDataStore.h"

namespace
{
	bool IsExpectedRejection(bool succeeded, const std::string& actual, const std::string& expected)
	{
		return !succeeded && !expected.empty() && actual == expected;
	}

	void RequireRejection(bool succeeded, const std::string& actual, const std::string& expected,
		const std::string& context)
	{
		if (!IsExpectedRejection(succeeded, actual, expected))
		{
			std::cerr << "FAIL rejection " << context << " expected [" << expected << "] actual [" << actual
				<< "] succeeded=" << succeeded << '\n';
			std::abort();
		}
		// 실제 관찰을 변형한 오탐 반례도 실제 거부와 동일한 판정 경로를 사용한다.
		assert(!IsExpectedRejection(false, "Unrelated failure", expected));
		assert(!IsExpectedRejection(false, "", expected));
		assert(!IsExpectedRejection(true, actual, expected));
		assert(!IsExpectedRejection(false, actual, ""));
		assert(!IsExpectedRejection(false, actual + " at another boundary", expected));
	}
}

int main(int argc, char** argv)
{
	assert(argc == 3);
	// 실제 음성 판정과 같은 함수에 오탐 반례를 연결한다.
	const std::string framingError = "Truncated progress payload";
	bool oracleControlsPassed = true;
	const auto oracleControl = [&](const char* name, bool succeeded, const std::string& actual,
		const std::string& expected, bool shouldMatch)
	{
		if (IsExpectedRejection(succeeded, actual, expected) != shouldMatch)
		{
			std::cerr << "FAIL rejection oracle control: " << name << '\n';
			oracleControlsPassed = false;
		}
	};
	oracleControl("exact rejection", false, framingError, framingError, true);
	oracleControl("unrelated diagnostic", false, "Unrelated failure", framingError, false);
	oracleControl("empty actual diagnostic", false, "", framingError, false);
	oracleControl("operation did not reject", true, framingError, framingError, false);
	oracleControl("empty expected reason is not a wildcard", false, "Unrelated failure", "", false);
	oracleControl("diagnostic substring is not identity", false, framingError + " at another boundary", framingError, false);
	if (!oracleControlsPassed) return 1;
	std::cout << "PASS rejection oracle requires exact nonempty reason and rejects false-positive controls\n";
	const std::filesystem::path source(argv[1]);
	const std::filesystem::path work(argv[2]);
	std::array<std::uint8_t, 32> key{};
	std::string error;
	const auto package = work / "GameData.dat";
	assert(GameData::BuildEncryptedPackage(source / "Data/Client", package, key, error, source / "Asset"));
	GameData::FGameDataStore store;
	if (!store.Load(package, key, error))
	{
		std::cerr << "FAIL complete Excel-defined progress catalog must load: " << error << '\n';
		return 1;
	}
	std::cout << "PASS complete Excel-defined progress catalog loads transactionally\n";

	const auto yaml = work / "yaml";
	std::filesystem::copy(source / "Data/Client", yaml, std::filesystem::copy_options::recursive);
	std::map<std::string, std::string> baseline;
	for (const auto& file : std::filesystem::directory_iterator(yaml))
	{
		std::ifstream input(file.path(), std::ios::binary);
		baseline[file.path().filename().string()] = std::string((std::istreambuf_iterator<char>(input)), {});
	}
	const std::array<const char*, 5> cohort{"CharacterDefinition.yaml", "LevelDefinition.yaml", "NewGameProfile.yaml",
		"ItemDefinition.yaml", "AchievementDefinition.yaml"};
	const auto* prior = store.FindScene(L"MainMenu");

	std::vector<std::function<void()>> catalogChecks;
	const auto remember = [&](auto lookup, auto values)
	{
		const auto* row = lookup();
		assert(row);
		const auto snapshot = values(*row);
		catalogChecks.emplace_back([lookup, values, row, snapshot]()
		{
			assert(lookup() == row && values(*row) == snapshot);
		});
	};
	remember([&]() { return store.FindCharacterDefinition(L"Swordswoman"); }, [](const auto& row)
		{ return std::make_tuple(row.characterId, row.nameTextKey, row.artId); });
	remember([&]() { return store.FindLevelDefinition(1); }, [](const auto& row)
		{ return std::make_tuple(row.level, row.requiredExperience, row.maxHp, row.attack, row.defense); });
	remember([&]() { return store.FindLevelDefinition(2); }, [](const auto& row)
		{ return std::make_tuple(row.level, row.requiredExperience, row.maxHp, row.attack, row.defense); });
	remember([&]() { return store.GetDefaultNewGameProfile(); }, [](const auto& row)
		{ return std::make_tuple(row.profileId, row.characterId, row.initialLevel, row.initialExperience, row.initialGold,
			row.initialMaxHpBonus, row.initialAttackBonus, row.initialDefenseBonus, row.contentVersion, row.isDefault); });
	remember([&]() { return store.FindItemDefinition(L"Token"); }, [](const auto& row)
		{ return std::make_tuple(row.itemId, row.nameTextKey, row.maxStack); });
	remember([&]() { return store.FindAchievementDefinition(L"Journey"); }, [](const auto& row)
		{ return std::make_tuple(row.achievementId, row.nameTextKey, row.targetCount); });

	const auto restore = [&]()
	{
		for (const auto& [name, text] : baseline) std::ofstream(yaml / name, std::ios::binary) << text;
	};
	const auto reject = [&](const char* table, const char* from, const char* to, const char* diagnostic,
		const std::string& expected)
	{
		restore();
		if (!from) std::filesystem::remove(yaml / table);
		else
		{
			auto text = baseline.at(table);
			const auto at = text.find(from);
			assert(at != std::string::npos);
			text.replace(at, std::string(from).size(), to);
			std::ofstream(yaml / table, std::ios::binary) << text;
		}
		assert(GameData::BuildEncryptedPackage(yaml, package, key, error, source / "Asset"));
		error = "stale catalog error must be replaced";
		RequireRejection(store.Load(package, key, error), error, expected, table);
		assert(store.FindScene(L"MainMenu") == prior);
		for (const auto& check : catalogChecks) check();
		std::cout << "PASS invalid catalog rejects with rollback: " << table << ' ' << diagnostic << '\n';
	};
	const std::string catalogLocation = "InGameData catalog cohort: ";
	for (const auto* table : cohort) reject(table, nullptr, "", "cohort", catalogLocation + "partial catalog cohort");
	reject("NewGameProfile.yaml", "\"IsDefault\": true", "\"IsDefault\": false", "default",
		catalogLocation + "exactly one default profile is required");
	reject("NewGameProfile.yaml", "\"CharacterId\": \"Swordswoman\"", "\"CharacterId\": \"Missing\"", "CharacterId",
		catalogLocation + "CharacterId reference is missing");
	reject("NewGameProfile.yaml", "\"InitialLevel\": 1", "\"InitialLevel\": 7", "InitialLevel",
		catalogLocation + "InitialLevel reference is missing");
	reject("NewGameProfile.yaml", "\"InitialExperience\": 0", "\"InitialExperience\": 100", "experience",
		catalogLocation + "initial level/experience mismatch");
	reject("NewGameProfile.yaml", "\"ContentVersion\": 1", "\"ContentVersion\": 0", "ContentVersion",
		catalogLocation + "ContentVersion must be positive and identical across profiles");
	reject("NewGameProfile.yaml", "\"InitialMaxHpBonus\": 0", "\"InitialMaxHpBonus\": 4294967295", "overflow",
		catalogLocation + "initial derived stats overflow");
	reject("NewGameProfile.yaml", "\"InitialAttackBonus\": 0", "\"InitialAttackBonus\": 4294967295", "overflow",
		catalogLocation + "initial derived stats overflow");
	reject("NewGameProfile.yaml", "\"InitialDefenseBonus\": 0", "\"InitialDefenseBonus\": 4294967295", "overflow",
		catalogLocation + "initial derived stats overflow");
	reject("CharacterDefinition.yaml", "\"ArtId\": \"PlayerSwordswoman\"", "\"ArtId\": \"Missing\"", "ArtId",
		catalogLocation + "ArtId reference is missing");
	reject("CharacterDefinition.yaml", "\"NameTextKey\": \"InGameData.Character.Swordswoman.Name\"",
		"\"NameTextKey\": \"Missing\"", "NameTextKey", catalogLocation + "NameTextKey reference is missing");
	reject("LevelDefinition.yaml", "\"Level\": 2", "\"Level\": 1", "duplicate", catalogLocation + "Level: duplicate primary key");
	reject("LevelDefinition.yaml", "\"Level\": 1", "\"Level\": 0", "Level", catalogLocation + "Level must be positive");
	reject("LevelDefinition.yaml", "\"MaxHp\": 100", "\"MaxHp\": 0", "MaxHp", catalogLocation + "MaxHp must be positive");
	reject("LevelDefinition.yaml", "\"RequiredExperience\": 100", "\"RequiredExperience\": 0", "experience",
		catalogLocation + "level experience thresholds must increase");
	reject("ItemDefinition.yaml", "\"MaxStack\": 99", "\"MaxStack\": 0", "MaxStack", catalogLocation + "MaxStack must be positive");
	reject("ItemDefinition.yaml", "\"NameTextKey\": \"InGameData.Item.Token.Name\"", "\"NameTextKey\": \"Missing\"", "NameTextKey",
		catalogLocation + "NameTextKey reference is missing");
	reject("AchievementDefinition.yaml", "\"TargetCount\": 1", "\"TargetCount\": 0", "TargetCount",
		catalogLocation + "TargetCount must be positive");
	reject("AchievementDefinition.yaml", "\"NameTextKey\": \"InGameData.Achievement.Journey.Name\"",
		"\"NameTextKey\": \"Missing\"", "NameTextKey", catalogLocation + "NameTextKey reference is missing");

	const auto replace = [](std::string& text, const std::string& from, const std::string& to)
	{
		const auto at = text.find(from);
		assert(at != std::string::npos);
		text.replace(at, from.size(), to);
	};
	// 생성된 실제 각 필드의 누락/타입/null 오류는 기존 Store의 strict decoder를 통과할 수 없다.
	for (const auto* table : cohort)
	{
		const auto& text = baseline.at(table);
		for (std::size_t at = text.find("  \""); at != std::string::npos;)
		{
			const auto end = text.find('\n', at);
			const auto colon = text.find(": ", at);
			const auto fieldEnd = text.find('"', at + 3);
			const auto field = text.substr(at + 3, fieldEnd - at - 3);
			const auto original = text.substr(at, end - at);
			const auto prefix = text.substr(at, colon + 2 - at);
			const auto rowStart = text.rfind("\n\"", at) + 1;
			const auto rowLine = 1 + std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(rowStart), '\n');
			const auto location = std::string("Data/Client/") + table + ": row at line " + std::to_string(rowLine) + ": ";
			reject(table, (original + "\n").c_str(), "", "missing field", location + "unknown or missing field");
			reject(table, original.c_str(), (prefix + "null").c_str(), field.c_str(),
				location + field + ": wrong scalar type or required null");
			reject(table, original.c_str(), (prefix + (text[colon + 2] == '"' ? "false" : "\"wrong\"")).c_str(), field.c_str(),
				location + field + ": wrong scalar type or required null");
			at = text.find("  \"", end);
		}
		const auto rowAt = text.find('\n') + 1;
		const auto nextRow = text.find("\n\"", rowAt);
		auto row = text.substr(rowAt, nextRow == std::string::npos ? std::string::npos : nextRow + 1 - rowAt);
		const auto rowEnd = row.find('\n');
		row.replace(0, rowEnd, "\"Extra\":");
		const auto keyStart = text.find("  \"") + 3;
		const auto primaryKey = text.substr(keyStart, text.find('"', keyStart) - keyStart);
		reject(table, "# TextRPG.GameData 1\n", ("# TextRPG.GameData 1\n" + row).c_str(), "duplicate",
			catalogLocation + primaryKey + ": duplicate primary key");
	}
	auto extraProfile = baseline.at("NewGameProfile.yaml");
	replace(extraProfile, "\"ProfileId\": \"Default\"", "\"ProfileId\": \"Extra\"");
	extraProfile.erase(0, extraProfile.find('\n') + 1);
	extraProfile.replace(0, extraProfile.find('\n'), "\"ExtraDefault\":");
	reject("NewGameProfile.yaml", "# TextRPG.GameData 1\n", ("# TextRPG.GameData 1\n" + extraProfile).c_str(), "default",
		catalogLocation + "exactly one default profile is required");
	restore();
	for (const auto* table : cohort) std::filesystem::remove(yaml / table);
	std::filesystem::remove(yaml / "TextArt.yaml");
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, source / "Asset"));
	GameData::FGameDataStore legacy;
	assert(legacy.Load(package, key, error));
	std::cout << "PASS absence of entire progress catalog preserves legacy fixtures\n";

	const auto* profile = store.GetDefaultNewGameProfile();
	assert(profile && store.FindNewGameProfile(profile->profileId) == profile);
	const auto* level = store.FindLevelDefinition(profile->initialLevel);
	assert(level && store.GetLevelDefinitions().size() >= 2);
	std::wstring initialScene;
	for (const auto& option : store.GetSceneOptions())
		if (option.isEnabled && option.actionType == GameData::Common::ESceneActionType::StartGame) initialScene = option.targetSceneId;
	assert(!initialScene.empty());
	std::unique_ptr<TextRPG::InGameData> progress;
	if (!TextRPG::InGameData::CreateNew(store, initialScene, progress, error))
	{
		std::cerr << "FAIL Excel profile creates progress: " << error << '\n';
		return 1;
	}
	assert(progress->GetCharacter().CharacterId == profile->characterId);
	assert(progress->GetCharacter().Level == profile->initialLevel);
	assert(progress->GetCharacter().Experience == profile->initialExperience);
	const TextRPG::SCharacterStats bonuses{profile->initialMaxHpBonus, profile->initialAttackBonus, profile->initialDefenseBonus};
	assert(progress->GetCharacter().StatBonuses == bonuses);
	TextRPG::SCharacterStats stats;
	assert(progress->GetStats(store, stats, error));
	assert(stats.MaxHp == level->maxHp + bonuses.MaxHp && stats.Attack == level->attack + bonuses.Attack &&
		stats.Defense == level->defense + bonuses.Defense);
	assert(progress->GetCharacter().CurrentHp == stats.MaxHp && progress->GetGold() == profile->initialGold);
	assert(progress->GetSceneId() == initialScene && progress->GetInventory().empty() && progress->GetAchievements().empty());
	const auto* active = progress.get();
	const auto expectUnchanged = [&](const std::unique_ptr<TextRPG::InGameData>& output, auto operation,
		const std::string& expected, const std::string& context)
	{
		const auto* root = output.get();
		assert(root);
		const auto* character = &root->GetCharacter();
		const auto* characterId = character->CharacterId.data();
		const auto* scene = root->GetSceneId().data();
		const auto* items = &root->GetInventory();
		const auto* itemBuffer = items->data();
		const auto* awards = &root->GetAchievements();
		const auto* awardBuffer = awards->data();
		std::vector<const wchar_t*> itemIds, awardIds;
		for (const auto& item : *items) itemIds.push_back(item.ItemId.data());
		for (const auto& award : *awards) awardIds.push_back(award.AchievementId.data());
		std::vector<std::uint8_t> before, after;
		std::string preservationError;
		assert(root->Serialize(before, preservationError));
		error = "stale progress error must be replaced";
		const bool succeeded = operation();
		RequireRejection(succeeded, error, expected, context);
		assert(output.get() == root && &root->GetCharacter() == character && character->CharacterId.data() == characterId);
		assert(root->GetSceneId().data() == scene);
		assert(&root->GetInventory() == items && items->data() == itemBuffer && items->size() == itemIds.size());
		assert(&root->GetAchievements() == awards && awards->data() == awardBuffer && awards->size() == awardIds.size());
		for (std::size_t i = 0; i < itemIds.size(); ++i) assert((*items)[i].ItemId.data() == itemIds[i]);
		for (std::size_t i = 0; i < awardIds.size(); ++i) assert((*awards)[i].AchievementId.data() == awardIds[i]);
		assert(root->Serialize(after, preservationError) && after == before);
	};
	expectUnchanged(progress, [&]() { return TextRPG::InGameData::CreateNew(legacy, initialScene, progress, error); },
		"New-game catalog is missing", "CreateNew legacy catalog");
	expectUnchanged(progress, [&]() { return TextRPG::InGameData::CreateNew(store, L"Missing", progress, error); },
		"SceneId definition is missing", "CreateNew SceneId");
	assert(progress.get() == active);
	std::cout << "PASS Excel profile and derived stats create one progress root without fallback\n";

	if (!progress->SetCurrentHp(store, 7, error))
	{
		std::cerr << "FAIL bounded progress mutation: " << error << '\n';
		return 1;
	}
	assert(progress->GetCharacter().CurrentHp == 7);
	expectUnchanged(progress, [&]() { return progress->SetCurrentHp(store, stats.MaxHp + 1, error); },
		"CurrentHp exceeds derived MaxHp", "SetCurrentHp");
	assert(progress->GetCharacter().CurrentHp == 7);
	assert(progress->SetCurrentHp(store, 0, error) && progress->GetCharacter().CurrentHp == 0);
	assert(progress->SetCurrentHp(store, 7, error));
	assert(progress->SetGold(54321, error) && progress->GetGold() == 54321);
	assert(progress->SetSceneId(store, initialScene, error));
	expectUnchanged(progress, [&]() { return progress->SetSceneId(store, L"Missing", error); },
		"SceneId definition is missing", "SetSceneId reference");
	assert(progress->GetSceneId() == initialScene);
	expectUnchanged(progress, [&]() { return progress->SetSceneId(store, std::wstring(257, L'x'), error); },
		"ID length is invalid", "SetSceneId length");
	expectUnchanged(progress, [&]() { return progress->SetSceneId(store, std::wstring(1, static_cast<wchar_t>(0xD800)), error); },
		"ID contains invalid UTF-16", "SetSceneId UTF-16");
	const std::vector<TextRPG::SInventoryItemProgress> inventory{{0x123456789ABCDEF0ULL, L"Token", 2}, {2, L"Token", 1}};
	const std::vector<TextRPG::SAchievementProgress> achievements{{L"Journey", 1, true}};
	assert(progress->SetInventory(store, inventory, error) && progress->GetInventory() == inventory);
	assert(progress->SetAchievements(store, achievements, error) && progress->GetAchievements() == achievements);
	const auto* inventoryBuffer = progress->GetInventory().data();
	const std::vector<std::pair<std::vector<TextRPG::SInventoryItemProgress>, const char*>> invalidInventories{
		{{{0, L"Token", 1}}, "Inventory has zero or duplicate InstanceId"},
		{{{1, L"Missing", 1}}, "ItemId definition is missing"},
		{{{1, L"Token", 0}}, "Inventory Count is invalid"},
		{{{1, L"Token", 100}}, "Inventory Count is invalid"},
		{{{1, L"Token", 1}, {1, L"Token", 1}}, "Inventory has zero or duplicate InstanceId"},
		{std::vector<TextRPG::SInventoryItemProgress>(4097), "Progress collection count exceeds limit"}};
	for (const auto& [invalid, diagnostic] : invalidInventories)
	{
		expectUnchanged(progress, [&]() { return progress->SetInventory(store, invalid, error); }, diagnostic, "SetInventory");
		assert(progress->GetInventory() == inventory && progress->GetInventory().data() == inventoryBuffer);
	}
	const std::vector<std::pair<std::vector<TextRPG::SAchievementProgress>, const char*>> invalidAchievements{
		{{{L"Missing", 0, false}}, "AchievementId definition is missing"},
		{{{L"Journey", 0, true}}, "Achievement completion/progress is inconsistent"},
		{{{L"Journey", 1, false}}, "Achievement completion/progress is inconsistent"},
		{{{L"Journey", 2, true}}, "Achievement completion/progress is inconsistent"},
		{{{L"Journey", 0, false}, {L"Journey", 1, true}}, "Duplicate AchievementId"},
		{std::vector<TextRPG::SAchievementProgress>(4097), "Progress collection count exceeds limit"}};
	for (const auto& [invalid, diagnostic] : invalidAchievements)
	{
		expectUnchanged(progress, [&]() { return progress->SetAchievements(store, invalid, error); }, diagnostic, "SetAchievements");
		assert(progress->GetAchievements() == achievements);
	}
	std::cout << "PASS scalar and nonempty nested progress mutations are bounded and transactional\n";

	std::vector<std::uint8_t> bytes{0xAA, 0xBB};
	if (!progress->Serialize(bytes, error))
	{
		std::cerr << "FAIL whole progress roundtrip: " << error << '\n';
		return 1;
	}
	std::unique_ptr<TextRPG::InGameData> restored;
	assert(TextRPG::InGameData::Deserialize(bytes, store, restored, error));
	assert(restored && restored.get() != progress.get());
	assert(restored->GetCharacter() == progress->GetCharacter());
	assert(restored->GetInventory() == inventory && restored->GetAchievements() == achievements);
	assert(restored->GetGold() == progress->GetGold() && restored->GetSceneId() == progress->GetSceneId());
	TextRPG::SCharacterStats restoredStats;
	assert(restored->GetStats(store, restoredStats, error) && restoredStats == stats);
	std::vector<std::uint8_t> encoded;
	assert(restored->Serialize(encoded, error) && encoded == bytes);
	std::unique_ptr<TextRPG::InGameData> empty;
	assert(TextRPG::InGameData::CreateNew(store, initialScene, empty, error));
	assert(empty->Serialize(encoded, error));
	assert(TextRPG::InGameData::Deserialize(encoded, store, restored, error));
	assert(restored->GetInventory().empty() && restored->GetAchievements().empty());
	std::cout << "PASS root character stats inventory and achievements roundtrip all actual owned values\n";

	// 독립 wire 오라클로 endian/필드 순서/하위 객체 전체를 고정한다.
	std::vector<std::uint8_t> oracle;
	struct SWireBoundary
	{
		std::size_t Begin, End, MinimumEnd;
		std::string Field, Diagnostic;
	};
	std::vector<SWireBoundary> wireBoundaries;
	const auto appendNumber = [&](std::uint64_t value, std::size_t width, const std::string& field)
	{
		const auto begin = oracle.size();
		for (std::size_t i = 0; i < width; ++i) oracle.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
		wireBoundaries.push_back({begin, oracle.size(), oracle.size(), field, "Truncated progress payload"});
	};
	const auto appendId = [&](const std::wstring& value, const std::string& field)
	{
		appendNumber(value.size(), 4, field + ".length");
		const auto begin = oracle.size();
		for (const auto unit : value)
		{
			oracle.push_back(static_cast<std::uint8_t>(unit));
			oracle.push_back(static_cast<std::uint8_t>(static_cast<std::uint16_t>(unit) >> 8));
		}
		wireBoundaries.push_back({begin, oracle.size(), oracle.size(), field + ".UTF16Units", "ID length is invalid or truncated"});
	};
	const auto appendCount = [&](std::size_t count, std::size_t minimumRecordBytes, const std::string& field)
	{
		appendNumber(count, 4, field);
		wireBoundaries.back().MinimumEnd = oracle.size() + count * minimumRecordBytes;
	};
	appendNumber(0x31444749, 4, "Magic"); appendNumber(1, 4, "FormatVersion");
	appendNumber(profile->contentVersion, 4, "ContentVersion");
	appendId(progress->GetSceneId(), "SceneId"); appendId(progress->GetCharacter().CharacterId, "CharacterId");
	appendNumber(progress->GetCharacter().Level, 4, "Level");
	appendNumber(progress->GetCharacter().Experience, 4, "Experience");
	appendNumber(7, 4, "CurrentHp"); appendNumber(bonuses.MaxHp, 4, "Bonuses.MaxHp");
	appendNumber(bonuses.Attack, 4, "Bonuses.Attack"); appendNumber(bonuses.Defense, 4, "Bonuses.Defense");
	appendNumber(54321, 4, "Gold");
	// 최소 레코드는 InstanceId(8)+ID 길이(4)+한 UTF-16 단위(2)+Count(4), 업적은 4+2+4+1이다.
	// 개수 framing은 요소를 읽기 전에 전체 잔여 길이를 먼저 검사한다.
	appendCount(inventory.size(), 8 + 4 + 2 + 4, "Inventory.count");
	for (std::size_t i = 0; i < inventory.size(); ++i)
	{
		const auto field = "Inventory[" + std::to_string(i) + "]";
		appendNumber(inventory[i].InstanceId, 8, field + ".InstanceId");
		appendId(inventory[i].ItemId, field + ".ItemId"); appendNumber(inventory[i].Count, 4, field + ".Count");
	}
	appendCount(achievements.size(), 4 + 2 + 4 + 1, "Achievements.count");
	appendId(achievements[0].AchievementId, "Achievements[0].AchievementId");
	appendNumber(1, 4, "Achievements[0].Progress"); appendNumber(1, 1, "Achievements[0].IsCompleted");
	assert(oracle == bytes);
	assert(bytes.size() <= 1024 * 1024 && wireBoundaries.front().Begin == 0 && wireBoundaries.back().End == bytes.size());
	for (std::size_t i = 1; i < wireBoundaries.size(); ++i) assert(wireBoundaries[i - 1].End == wireBoundaries[i].Begin);
	assert(TextRPG::InGameData::Deserialize(bytes, store, restored, error));
	const auto* restoredBefore = restored.get();
	const auto rejectBytes = [&](const std::vector<std::uint8_t>& invalid, const std::string& diagnostic,
		const std::string& context = "wire mutation")
	{
		expectUnchanged(restored, [&]() { return TextRPG::InGameData::Deserialize(invalid, store, restored, error); },
			diagnostic, context);
		assert(restored.get() == restoredBefore);
		std::vector<std::uint8_t> preserved;
		std::string preservationError;
		assert(restored->Serialize(preserved, preservationError) && preserved == bytes);
	};
	const auto put = [](std::vector<std::uint8_t>& target, std::size_t at, std::uint64_t value, std::size_t width = 4)
	{
		assert(at + width <= target.size());
		for (std::size_t i = 0; i < width; ++i) target[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
	};
	const auto mutate = [&](std::size_t at, std::uint64_t value, const char* diagnostic, std::size_t width = 4)
	{
		auto invalid = bytes; put(invalid, at, value, width); rejectBytes(invalid, diagnostic);
	};
	const std::size_t sceneUnits = 16;
	const auto characterLength = sceneUnits + 2 * progress->GetSceneId().size();
	const auto levelAt = characterLength + 4 + 2 * progress->GetCharacter().CharacterId.size();
	const auto inventoryCountAt = levelAt + 28;
	const auto firstItemAt = inventoryCountAt + 4;
	const auto firstItemCountAt = firstItemAt + 12 + 2 * inventory[0].ItemId.size();
	const auto secondItemAt = firstItemCountAt + 4;
	const auto achievementCountAt = secondItemAt + 16 + 2 * inventory[1].ItemId.size();
	const auto achievementAt = achievementCountAt + 4;
	const auto achievementProgressAt = achievementAt + 4 + 2 * achievements[0].AchievementId.size();
	mutate(0, 0, "Invalid progress magic"); mutate(4, 2, "Unsupported progress format version");
	mutate(8, 2, "Unsupported content version or missing catalog");
	for (const auto length : {0U, 257U, 0xFFFFFFFFU}) mutate(12, length, "ID length is invalid or truncated");
	mutate(sceneUnits, 0xD800, "ID contains invalid UTF-16", 2);
	mutate(sceneUnits, 0xDC00, "ID contains invalid UTF-16", 2); mutate(sceneUnits, 0, "ID contains NUL", 2);
	mutate(sceneUnits, L'X', "SceneId definition is missing", 2);
	mutate(characterLength + 4, L'X', "CharacterId or Level definition is missing", 2);
	mutate(levelAt, 0, "CharacterId or Level definition is missing");
	mutate(levelAt, 7, "CharacterId or Level definition is missing");
	mutate(levelAt + 4, 100, "Character level/experience mismatch");
	mutate(levelAt + 8, 0xFFFFFFFF, "CurrentHp exceeds derived MaxHp");
	for (std::size_t bonusAt = levelAt + 12; bonusAt < levelAt + 24; bonusAt += 4)
		mutate(bonusAt, 0xFFFFFFFF, "Derived stats overflow");
	mutate(inventoryCountAt, 4097, "Progress collection count is invalid or truncated");
	mutate(inventoryCountAt, 0xFFFFFFFF, "Progress collection count is invalid or truncated");
	mutate(firstItemAt, 0, "Inventory has zero or duplicate InstanceId", 8);
	mutate(secondItemAt, inventory[0].InstanceId, "Inventory has zero or duplicate InstanceId", 8);
	mutate(firstItemAt + 8, 257, "ID length is invalid or truncated");
	mutate(firstItemAt + 12, L'X', "ItemId definition is missing", 2);
	mutate(firstItemCountAt, 0, "Inventory Count is invalid"); mutate(firstItemCountAt, 100, "Inventory Count is invalid");
	mutate(achievementCountAt, 4097, "Progress collection count is invalid or truncated");
	mutate(achievementAt + 4, L'X', "AchievementId definition is missing", 2);
	mutate(achievementProgressAt, 2, "Achievement completion/progress is inconsistent");
	mutate(achievementProgressAt, 0, "Achievement completion/progress is inconsistent");
	mutate(achievementProgressAt + 4, 0, "Achievement completion/progress is inconsistent", 1);
	mutate(achievementProgressAt + 4, 2, "Progress boolean is not canonical", 1);
	auto invalid = bytes;
	invalid.insert(invalid.end(), bytes.begin() + static_cast<std::ptrdiff_t>(achievementAt), bytes.end());
	put(invalid, achievementCountAt, 2); rejectBytes(invalid, "Duplicate AchievementId");
	invalid = bytes; invalid.push_back(0); rejectBytes(invalid, "Trailing progress bytes");
	rejectBytes(std::vector<std::uint8_t>(1024 * 1024 + 1), "Total progress bytes exceed limit");
	std::ofstream prefixProof(work / "prefix-oracle.tsv");
	assert(prefixProof);
	prefixProof << "cut	field	begin	end	minimumEnd	expected\n";
	std::set<std::string> prefixReasons;
	for (std::size_t cut = 0; cut < bytes.size(); ++cut)
	{
		// 기대값은 정상 fixture의 독립 필드 경계와 잔여 바이트 요구로만 도출한다. 제품 오류를 읽어 정하지 않는다.
		const SWireBoundary* boundary = nullptr;
		std::string expected;
		for (const auto& field : wireBoundaries)
		{
			if (cut < field.End) { boundary = &field; expected = field.Diagnostic; break; }
			if (cut < field.MinimumEnd)
			{
				boundary = &field; expected = "Progress collection count is invalid or truncated"; break;
			}
		}
		assert(boundary && !expected.empty());
		prefixReasons.insert(expected);
		prefixProof << cut << '	' << boundary->Field << '	' << boundary->Begin << '	' << boundary->End
			<< '	' << boundary->MinimumEnd << '	' << expected << '\n';
		rejectBytes(std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(cut)), expected,
			"prefix cut=" + std::to_string(cut) + " field=" + boundary->Field + " end=" + std::to_string(boundary->End));
	}
	assert(prefixReasons.size() == 3);
	prefixProof.close();
	assert(prefixProof);
	expectUnchanged(restored, [&]() { return TextRPG::InGameData::Deserialize(bytes, legacy, restored, error); },
		"ItemId definition is missing", "Deserialize legacy catalog (inventory definition precedes content validation)");
	assert(restored.get() == restoredBefore);
	std::cout << "PASS exact wire oracle and malformed versions strings counts references stats booleans truncation rollback\n";
	// 보정과 경계값은 정의와 독립적으로 보존되며 파생 스탯은 다시 계산한다.
	auto boundary = bytes;
	put(boundary, levelAt + 4, 99);
	put(boundary, levelAt + 8, 0);
	put(boundary, levelAt + 12, 3); put(boundary, levelAt + 16, 4); put(boundary, levelAt + 20, 5);
	put(boundary, levelAt + 24, 0xFFFFFFFF);
	put(boundary, firstItemAt, 0xFFFFFFFFFFFFFFFFULL, 8); put(boundary, firstItemCountAt, 99);
	std::unique_ptr<TextRPG::InGameData> edge;
	assert(TextRPG::InGameData::Deserialize(boundary, store, edge, error));
	assert(edge->GetCharacter().StatBonuses == (TextRPG::SCharacterStats{3, 4, 5}));
	assert(edge->GetStats(store, restoredStats, error) && restoredStats == (TextRPG::SCharacterStats{103, 14, 10}));
	assert(edge->GetCharacter().Experience == 99 && edge->GetCharacter().CurrentHp == 0 && edge->GetGold() == 0xFFFFFFFF);
	assert(edge->GetInventory()[0].InstanceId == 0xFFFFFFFFFFFFFFFFULL && edge->GetInventory()[0].Count == 99);
	assert(edge->Serialize(encoded, error) && encoded == boundary);
	put(boundary, levelAt, 2); put(boundary, levelAt + 4, 100);
	assert(TextRPG::InGameData::Deserialize(boundary, store, edge, error));
	assert(edge->GetStats(store, restoredStats, error) && restoredStats == (TextRPG::SCharacterStats{123, 16, 11}));
	const auto* edgeBefore = edge.get();
	std::vector<std::uint8_t> edgeBytes;
	assert(edge->Serialize(edgeBytes, error) && edgeBytes == boundary);
	put(boundary, levelAt + 4, 99);
	expectUnchanged(edge, [&]() { return TextRPG::InGameData::Deserialize(boundary, store, edge, error); },
		"Character level/experience mismatch", "Deserialize level 2 below required experience");
	assert(edge.get() == edgeBefore && edge->Serialize(encoded, error) && encoded == edgeBytes);
	std::cout << "PASS nonzero bonuses level experience thresholds dead HP and fixed-width integer boundaries roundtrip\n";

	// 유효한 데이터 전용 변경은 새 게임 수치를 바꾸지만 저장된 진행은 기본값으로 덮지 않는다.
	restore();
	auto alternateProfile = baseline.at("NewGameProfile.yaml");
	replace(alternateProfile, "\"InitialGold\": 0", "\"InitialGold\": 2468");
	replace(alternateProfile, "\"InitialLevel\": 1", "\"InitialLevel\": 2");
	replace(alternateProfile, "\"InitialExperience\": 0", "\"InitialExperience\": 100");
	replace(alternateProfile, "\"InitialMaxHpBonus\": 0", "\"InitialMaxHpBonus\": 9");
	replace(alternateProfile, "\"InitialAttackBonus\": 0", "\"InitialAttackBonus\": 8");
	replace(alternateProfile, "\"InitialDefenseBonus\": 0", "\"InitialDefenseBonus\": 7");
	std::ofstream(yaml / "NewGameProfile.yaml", std::ios::binary) << alternateProfile;
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, source / "Asset"));
	GameData::FGameDataStore alternate;
	assert(alternate.Load(package, key, error));
	std::unique_ptr<TextRPG::InGameData> fromAlternate;
	assert(TextRPG::InGameData::CreateNew(alternate, initialScene, fromAlternate, error));
	assert(fromAlternate->GetCharacter().Level == 2 && fromAlternate->GetCharacter().Experience == 100);
	assert(fromAlternate->GetCharacter().StatBonuses == (TextRPG::SCharacterStats{9, 8, 7}));
	assert(fromAlternate->GetCharacter().CurrentHp == 129 && fromAlternate->GetGold() == 2468);
	assert(fromAlternate->Serialize(encoded, error));
	assert(TextRPG::InGameData::Deserialize(encoded, alternate, edge, error));
	assert(edge->GetCharacter() == fromAlternate->GetCharacter());
	assert(TextRPG::InGameData::Deserialize(bytes, alternate, edge, error));
	assert(edge->GetCharacter() == progress->GetCharacter() && edge->GetGold() == 54321);
	std::cout << "PASS data-only profile changes supply initial level experience gold and all stat bonuses\n";
	// 다수 객체와 정확한 컬렉션 상한의 실제 진행을 왕복한다.
	std::vector<TextRPG::SInventoryItemProgress> many;
	for (std::uint64_t i = 1; i <= 4096; ++i) many.push_back({i, L"Token", 1});
	assert(edge->SetInventory(store, many, error));
	assert(edge->Serialize(encoded, error));
	assert(TextRPG::InGameData::Deserialize(encoded, store, fromAlternate, error));
	assert(fromAlternate->GetInventory() == many);
	std::cout << "PASS 4096 distinct inventory instances roundtrip without pointers or partial publication\n";
	// 기술적 문자열/총량 경계는 실제 참조가 있는 별도 카탈로그로 확인한다.
	restore();
	const std::string longItem(256, 'a');
	auto longDefinition = baseline.at("ItemDefinition.yaml");
	replace(longDefinition, "\"ItemId\": \"Token\"", "\"ItemId\": \"" + longItem + "\"");
	std::ofstream(yaml / "ItemDefinition.yaml", std::ios::binary) << longDefinition;
	auto unicodeCharacter = baseline.at("CharacterDefinition.yaml");
	replace(unicodeCharacter, "\"CharacterId\": \"Swordswoman\"", "\"CharacterId\": \"\\uAC80\\uC0AC\\uD83D\\uDE00\"");
	std::ofstream(yaml / "CharacterDefinition.yaml", std::ios::binary) << unicodeCharacter;
	auto unicodeProfile = baseline.at("NewGameProfile.yaml");
	replace(unicodeProfile, "\"CharacterId\": \"Swordswoman\"", "\"CharacterId\": \"\\uAC80\\uC0AC\\uD83D\\uDE00\"");
	std::ofstream(yaml / "NewGameProfile.yaml", std::ios::binary) << unicodeProfile;
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, source / "Asset"));
	GameData::FGameDataStore limits;
	assert(limits.Load(package, key, error));
	assert(TextRPG::InGameData::CreateNew(limits, initialScene, edge, error));
	assert(edge->GetCharacter().CharacterId == L"검사\U0001F600");
	const std::vector<TextRPG::SInventoryItemProgress> longInventory{{1, std::wstring(256, L'a'), 1}};
	assert(edge->SetInventory(limits, longInventory, error));
	assert(edge->Serialize(encoded, error));
	assert(TextRPG::InGameData::Deserialize(encoded, limits, fromAlternate, error));
	assert(fromAlternate->GetCharacter() == edge->GetCharacter() && fromAlternate->GetInventory() == longInventory);
	many.clear();
	for (std::uint64_t i = 1; i <= 4096; ++i) many.push_back({i, std::wstring(256, L'a'), 1});
	expectUnchanged(edge, [&]() { return edge->SetInventory(limits, many, error); },
		"Total progress bytes exceed limit", "SetInventory aggregate byte limit");
	assert(edge->GetInventory() == longInventory);
	std::cout << "PASS valid supplementary UTF-16 and 256-unit IDs roundtrip while aggregate byte limit rejects\n";






}
