#include "GameData/pch.h"
#include "Common/GameDataEnums.g.h"
#include "Scene/SceneData.g.h"
#include "SceneOption/SceneOptionData.g.h"
#include "SceneTypeConfig/SceneTypeConfigData.g.h"
#include "Localization/LocalizationData.g.h"
#include "TextArt/TextArtData.g.h"
#include "GameData/FGameDataPackage.h"
#include "GameData/FGeneratedYaml.h"
#include "GameData/FGameDataStore.h"

namespace
{
	int failure = 0;
	std::size_t delegatedLoads = 0;
	std::size_t injectedFailures = 0;
	void Require(bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}
}

namespace GameData
{
	// 실제 reader를 유지하고 Load 예외 경계만 결정적으로 주입한다. 실제 OOM은 유발하지 않는다.
	class FThrowingPackageReader final
	{
	public:
		bool Load(const std::filesystem::path& path, const std::array<std::uint8_t, 32>& key, std::string& error)
		{
			if (failure == 1) { ++injectedFailures; throw std::bad_alloc(); }
			if (failure == 2) { ++injectedFailures; throw std::runtime_error("injected package reader failure"); }
			++delegatedLoads;
			return m_reader.Load(path, key, error);
		}
		const std::vector<SPackageEntry>& GetEntries() const noexcept { return m_reader.GetEntries(); }
	private:
		FGameDataPackageReader m_reader;
	};
}

#define FGameDataPackageReader FThrowingPackageReader
#include "GameData/FGameDataStore.cpp"
#undef FGameDataPackageReader

namespace
{
	using namespace GameData;
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
	auto Values(const CharacterCombat::SCharacterCombatData& row)
	{
		return std::make_tuple(row.characterId, row.strength, row.dexterity, row.intelligence, row.luck);
	}
	auto Values(const MonsterDefinition::SMonsterDefinitionData& row)
	{
		return std::make_tuple(row.monsterId, row.nameTextKey, row.artId, row.maxHp, row.attack,
			row.defense, row.strength, row.dexterity, row.intelligence, row.luck,
			row.rewardExperience, row.rewardCurrencyType, row.rewardAmountMin, row.rewardAmountMax);
	}
	auto Values(const CombatRule::SCombatRuleData& row)
	{
		return std::make_tuple(row.ruleId, row.monsterId, row.battleSceneId, row.townSceneId, row.scale,
			row.strengthDefense, row.dexteritySpeed, row.baseSpeed, row.luckCritical, row.criticalCap,
			row.criticalMultiplier, row.minimumDamage, row.strengthCoefficient, row.dexterityCoefficient, row.intelligenceCoefficient,
			row.luckCoefficient, row.maxStat, row.maxRounds, row.healOnReturn, row.hitTextKey,
			row.criticalTextKey, row.victoryTextKey, row.defeatTextKey, row.readyTextKey, row.roundTextKey);
	}
	struct SFixtureTable final
	{
		const char* name;
		std::size_t entries{};
		std::size_t rows{};
		std::size_t checks{};
	};
	bool HasCompleteSnapshot(const std::array<SFixtureTable, 13>& tables)
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
		if (pointer == nullptr) throw std::runtime_error("fixture lookup missing");
		const auto values = Values(*pointer);
		checks.emplace_back([lookup, pointer, values]()
		{
			Require(lookup() == pointer, "published row pointer changed on failure");
			Require(Values(*pointer) == values, "published row values changed on failure");
		});
	}
	template<class Lookup, class ControlLookup>
	void RememberRecovery(std::vector<std::function<void()>>& checks, Lookup lookup, ControlLookup controlLookup)
	{
		const auto* control = controlLookup();
		if (control == nullptr) throw std::runtime_error("recovery control lookup missing");
		const auto values = Values(*control);
		checks.emplace_back([lookup, values]()
		{
			const auto* recovered = lookup();
			if (recovered == nullptr) throw std::runtime_error("recovery definition lookup missing");
			Require(Values(*recovered) == values, "recovery definition values differ from control");
		});
	}
}

int main(int argc, char** argv)
{
	try
	{
		Require(argc == 5, "expected data-root work fault populated");
		const std::filesystem::path data(argv[1]);
		const std::filesystem::path work(argv[2]);
		const int fault = std::stoi(argv[3]);
		const bool populated = std::string(argv[4]) == "populated";
		Require(fault == 1 || fault == 2, "invalid fault");
		std::filesystem::create_directories(work);
		const auto package = work / "GameData.dat";
		std::array<std::uint8_t, 32> key{}; // 테스트 전용 키이며 실제 키 공급자는 변경하지 않는다.
		std::string error;
		Require(GameData::BuildEncryptedPackage(data / "Data/Client", package, key, error, data / "Asset"), "real package build failed");
		GameData::FGameDataStore control;
		Require(control.Load(package, key, error), "valid control failed");
		GameData::FGameDataStore store;
		if (populated) Require(store.Load(package, key, error), "initial valid load failed");
		const auto* scenesData = store.GetScenes().data();
		const auto* optionsData = store.GetSceneOptions().data();
		const auto sceneCount = store.GetScenes().size();
		const auto optionCount = store.GetSceneOptions().size();
		std::vector<std::function<void()>> checks;
		std::vector<std::function<void()>> recoveryChecks;
		std::array<SFixtureTable, 13> tables{{
			{"Data/Client/Scene.yaml"}, {"Data/Client/SceneOption.yaml"}, {"Data/Client/SceneTypeConfig.yaml"},
			{"Data/Client/Localization.yaml"}, {"Data/Client/TextArt.yaml"}, {"Data/Client/CharacterDefinition.yaml"},
			{"Data/Client/LevelDefinition.yaml"}, {"Data/Client/NewGameProfile.yaml"}, {"Data/Client/ItemDefinition.yaml"},
			{"Data/Client/AchievementDefinition.yaml"}, {"Data/Client/CharacterCombat.yaml"},
			{"Data/Client/MonsterDefinition.yaml"}, {"Data/Client/CombatRule.yaml"}
		}};
		std::size_t rowCount = 0;
		std::size_t assetCount = 0;
		GameData::FGameDataPackageReader reader;
		Require(reader.Load(package, key, error), "real reader control failed");
		for (const auto& entry : reader.GetEntries())
		{
			if (entry.name.starts_with("Asset/"))
			{
				++assetCount;
				const auto name = std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(entry.name.data()), entry.name.size())).wstring();
				const auto* bytes = store.FindAsset(name);
				if (populated) Require(bytes && *bytes == entry.contents, "asset snapshot differs");
				else Require(bytes == nullptr, "empty asset published");
				const auto contents = entry.contents;
				checks.emplace_back([&store, name, bytes, contents]()
				{
					Require(store.FindAsset(name) == bytes, "asset address changed on failure");
					if (bytes) Require(*bytes == contents, "asset bytes changed on failure");
				});
				continue;
			}
			const auto table = std::find_if(tables.begin(), tables.end(), [&entry](const auto& candidate)
			{
				return entry.name == candidate.name;
			});
			Require(table != tables.end(), "unexpected fixture table");
			++table->entries;
			std::vector<GameData::SYamlRow> rows;
			Require(GameData::ParseGeneratedYaml(entry.contents, rows, error), "fixture YAML failed");
			table->rows += rows.size();
			for (const auto& row : rows)
			{
				const auto checksBeforeRow = checks.size();
				++rowCount;
				if (entry.name == "Data/Client/Scene.yaml")
				{
					const auto id = row.fields.at(L"SceneId").number;
					if (populated) Remember(checks, [&store, id]() { return store.FindScene(id); });
					else checks.emplace_back([&store, id]() { Require(!store.FindScene(id), "empty Scene published"); });
				}
				else if (entry.name == "Data/Client/SceneOption.yaml")
				{
					const auto id = row.fields.at(L"OptionId").number;
					if (populated) Remember(checks, [&store, id]() { return store.FindSceneOption(id); });
					else checks.emplace_back([&store, id]() { Require(!store.FindSceneOption(id), "empty option published"); });
				}
				else if (entry.name == "Data/Client/SceneTypeConfig.yaml")
				{
					const auto type = row.fields.at(L"SceneType").text;
					Require(type == L"MainMenu" || type == L"Town" || type == L"BattleField", "invalid fixture SceneType");
					const auto id = type == L"MainMenu" ? GameData::Common::ESceneType::MainMenu :
						(type == L"Town" ? GameData::Common::ESceneType::Town : GameData::Common::ESceneType::BattleField);
					if (populated) Remember(checks, [&store, id]() { return store.FindSceneTypeConfig(id); });
					else checks.emplace_back([&store, id]() { Require(!store.FindSceneTypeConfig(id), "empty config published"); });
				}
				else if (entry.name == "Data/Client/Localization.yaml")
				{
					const auto id = row.fields.at(L"TextKey").number;
					if (populated) Remember(checks, [&store, id]() { return store.FindLocalization(id); });
					else checks.emplace_back([&store, id]() { Require(!store.FindLocalization(id), "empty localization published"); });
				}
				else if (entry.name == "Data/Client/TextArt.yaml")
				{
					const auto id = row.fields.at(L"ArtId").number;
					if (populated) Remember(checks, [&store, id]() { return store.FindTextArt(id); });
					else checks.emplace_back([&store, id]() { Require(!store.FindTextArt(id), "empty art published"); });
				}
				else if (entry.name == "Data/Client/CharacterDefinition.yaml")
				{
					const auto id = row.fields.at(L"CharacterId").number;
					const auto lookup = [&store, id]() { return store.FindCharacterDefinition(id); };
					if (populated) Remember(checks, lookup);
					else checks.emplace_back([lookup]() { Require(!lookup(), "empty character definition published"); });
					RememberRecovery(recoveryChecks, lookup, [&control, id]() { return control.FindCharacterDefinition(id); });
				}
				else if (entry.name == "Data/Client/LevelDefinition.yaml")
				{
					const auto id = row.fields.at(L"Level").number;
					const auto lookup = [&store, id]() { return store.FindLevelDefinition(id); };
					if (populated) Remember(checks, lookup);
					else checks.emplace_back([lookup]() { Require(!lookup(), "empty level definition published"); });
					RememberRecovery(recoveryChecks, lookup, [&control, id]() { return control.FindLevelDefinition(id); });
				}
				else if (entry.name == "Data/Client/NewGameProfile.yaml")
				{
					const auto id = row.fields.at(L"ProfileId").number;
					const auto lookup = [&store, id]() { return store.FindNewGameProfile(id); };
					if (populated) Remember(checks, lookup);
					else checks.emplace_back([lookup]() { Require(!lookup(), "empty new game profile published"); });
					RememberRecovery(recoveryChecks, lookup, [&control, id]() { return control.FindNewGameProfile(id); });
				}
				else if (entry.name == "Data/Client/ItemDefinition.yaml")
				{
					const auto id = row.fields.at(L"ItemId").number;
					const auto lookup = [&store, id]() { return store.FindItemDefinition(id); };
					if (populated) Remember(checks, lookup);
					else checks.emplace_back([lookup]() { Require(!lookup(), "empty item definition published"); });
					RememberRecovery(recoveryChecks, lookup, [&control, id]() { return control.FindItemDefinition(id); });
				}
				else if (entry.name == "Data/Client/AchievementDefinition.yaml")
				{
					const auto id = row.fields.at(L"AchievementId").number;
					const auto lookup = [&store, id]() { return store.FindAchievementDefinition(id); };
					if (populated) Remember(checks, lookup);
					else checks.emplace_back([lookup]() { Require(!lookup(), "empty achievement definition published"); });
					RememberRecovery(recoveryChecks, lookup, [&control, id]() { return control.FindAchievementDefinition(id); });
				}
				else if (entry.name == "Data/Client/CharacterCombat.yaml")
				{
					const auto id = row.fields.at(L"CharacterId").number;
					const auto lookup = [&store, id]()
					{
						return store.FindCharacterCombat(id);
					};
					if (populated) Remember(checks, lookup);
					else checks.emplace_back([lookup]()
					{
						Require(!lookup(), "empty combat definition published");
					});
					RememberRecovery(recoveryChecks, lookup, [&control, id]()
					{
						return control.FindCharacterCombat(id);
					});
				}
				else if (entry.name == "Data/Client/MonsterDefinition.yaml")
				{
					const auto id = row.fields.at(L"MonsterId").number;
					const auto lookup = [&store, id]()
					{
						return store.FindMonsterDefinition(id);
					};
					if (populated) Remember(checks, lookup);
					else checks.emplace_back([lookup]()
					{
						Require(!lookup(), "empty combat definition published");
					});
					RememberRecovery(recoveryChecks, lookup, [&control, id]()
					{
						return control.FindMonsterDefinition(id);
					});
				}
				else if (entry.name == "Data/Client/CombatRule.yaml")
				{
					const auto id = row.fields.at(L"RuleId").number;
					const auto lookup = [&store, id]()
					{
						return store.FindCombatRule(id);
					};
					if (populated) Remember(checks, lookup);
					else checks.emplace_back([lookup]()
					{
						Require(!lookup(), "empty combat definition published");
					});
					RememberRecovery(recoveryChecks, lookup, [&control, id]()
					{
						return control.FindCombatRule(id);
					});
				}
				else Require(false, "unexpected fixture table");
				Require(checks.size() == checksBeforeRow + 1, "fixture row snapshot missing");
				table->checks += checks.size() - checksBeforeRow;
			}
		}
		Require(checks.size() == rowCount + assetCount && rowCount > 0 && assetCount > 0, "incomplete snapshot");
		Require(HasCompleteSnapshot(tables), "incomplete fixture table coverage");
		// 실제 판정 함수를 공유하여 신규 cohort 한 테이블의 조용한 누락을 거부한다.
		for (std::size_t index = 0; index < tables.size(); ++index)
		{
			auto missingTable = tables;
			missingTable[index].entries = 0;
			missingTable[index].rows = 0;
			missingTable[index].checks = 0;
			Require(!HasCompleteSnapshot(missingTable), "missing fixture table control accepted");
		}
		std::size_t definitionRows = 0;
		for (std::size_t index = 5; index < tables.size(); ++index) definitionRows += tables[index].rows;
		Require(recoveryChecks.size() == definitionRows, "incomplete definition recovery snapshot");
		const auto delegatedBeforeFault = delegatedLoads;
		failure = fault;
		bool loaded = true;
		try { loaded = store.Load(package, key, error); }
		catch (const std::exception& exception)
		{
			std::cerr << "FAIL reader exception escaped store bool/error contract: " << exception.what() << '\n';
			return 1;
		}
		failure = 0;
		Require(!loaded, "exception load returned true");
		Require(injectedFailures == 1 && delegatedLoads == delegatedBeforeFault, "intended reader fault not hit exactly once");
		Require(error.starts_with("GameData.dat: "), "missing GameData.dat exception context");
		Require(error.find(fault == 1 ? "bad" : "injected package reader failure") != std::string::npos,
			"missing exception diagnostic");
		Require(store.GetScenes().data() == scenesData && store.GetScenes().size() == sceneCount, "Scene vector changed");
		Require(store.GetSceneOptions().data() == optionsData && store.GetSceneOptions().size() == optionCount, "option vector changed");
		for (const auto& check : checks) check();
		const auto before = delegatedLoads;
		Require(store.Load(package, key, error) && error.empty(), "later valid load failed");
		Require(delegatedLoads == before + 1 && !store.GetScenes().empty(), "recovery did not use real reader");
		for (const auto& check : recoveryChecks) check();
		for (const auto& table : tables)
			std::cout << "COVERAGE " << table.name << " entries=" << table.entries << " rows=" << table.rows
				<< " checks=" << table.checks << '\n';
		std::cout << "PASS fault=" << fault << " state=" << argv[4] << " rows=" << rowCount
			<< " tables=" << tables.size() << " definition-rows=" << definitionRows << " assets=" << assetCount
			<< " recovery-checks=" << recoveryChecks.size() << " missing-table-control=rejected"
			<< " fault-hit=" << injectedFailures << " pointer/value/asset rollback and real-reader recovery\n";
		return 0;
	}
	catch (const std::exception& exception)
	{
		std::cerr << "FAIL " << exception.what() << '\n';
		return 1;
	}
}
