#include "GameData/pch.h"
#include "ConsoleGameCorePch.h"
#include "GameDataAll.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"
#include "CustomInGameData/FCharacterInGameData.h"
#include "InGameData/InGameData.h"
#include "Combat/FCombat.h"
#include "Math/Random/FSelector.h"
#include "GameData/FGameDataStore.h"
#include "GameData/FGameDataPackage.h"

namespace TextRPGTest
{
	using namespace TextRPG;
	using SRoot = GameData::InGameData::SRootInGameData;

	// 전투 도입 전 숫자 스키마2의 독립 writer: 과거 필드 여섯 개만 기록한다.
	class FNumericV2Root final : public ISerializable
	{
	public:
		std::uint32_t m_contentVersion = 0;
		std::uint32_t m_sceneId = 0;
		std::shared_ptr<GameData::InGameData::SCharacterProgress> m_character = std::make_shared<FCharacterInGameData>();
		std::uint32_t m_gold = 54321;
		std::vector<GameData::InGameData::SInventoryItemProgress> m_inventory;
		std::vector<GameData::InGameData::SAchievementProgress> m_achievements;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return "SRootInGameData";
		}
	protected:
		bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
		{
			return writer.Field("m_contentVersion", m_contentVersion, error) &&
				writer.Field("m_sceneId", m_sceneId, error) && writer.Field("m_character", m_character, error) &&
				writer.Field("m_gold", m_gold, error) && writer.Field("m_inventory", m_inventory, error) &&
				writer.Field("m_achievements", m_achievements, error);
		}
		bool DeserializeFields(FBinaryReader&, std::string& error) override
		{
			error = "numeric schema2 fixture is write only";
			return false;
		}
	};

	static void TestNumericV2(const GameData::FGameDataStore& store, std::uint32_t townSceneId)
	{
		std::string error;
		std::unique_ptr<InGameData> original;
		assert(InGameData::CreateNew(store, townSceneId, original, error));
		FSerializableTypeRegistry registry;
		assert(GameData::InGameData::RegisterInGameDataTypes(registry, error));
		assert(RegisterCharacterInGameData(registry, error));
		FNumericV2Root old;
		std::vector<std::uint8_t> bytes;
		old.m_contentVersion = store.GetDefaultNewGameProfile()->contentVersion;
		old.m_sceneId = townSceneId;
		*old.m_character = original->GetCharacter();
		old.m_inventory.push_back({71, static_cast<std::uint32_t>(GameData::Common::EItemId::Token), 2});
		const auto& award = *store.FindAchievementDefinition(static_cast<std::uint32_t>(GameData::Common::EAchievementId::Journey));
		old.m_achievements.push_back({award.achievementId, award.targetCount, true});
		old.m_gold = 54321;
		old.m_character->CurrentHp = 7;
		old.m_character->StatBonuses = {3, 4, 5};
		const auto inventory = old.m_inventory;
		const auto achievements = old.m_achievements;
		assert(FSerializationArchive::Serialize(old, bytes, error, 2, &registry));
		std::unique_ptr<InGameData> restored;
		const bool accepted = InGameData::Deserialize(bytes, store, restored, error);
		std::cerr << "numeric-schema2 accepted=" << accepted << " error=" << error << '\n';
		assert(accepted);
		assert(restored->GetGold() == 54321 && restored->GetCharacter().CurrentHp == 7);
		assert(restored->GetCharacter().StatBonuses == old.m_character->StatBonuses);
		assert(restored->GetInventory() == inventory && restored->GetAchievements() == achievements);
		assert(restored->GetBattle() == GameData::InGameData::SBattleProgress{});
		assert(restored->Serialize(bytes, error));
		SRoot current;
		assert(FSerializationArchive::Deserialize(bytes, current, error, GameData::InGameData::kSchemaVersion, &registry));
		assert(current.m_basicBonuses == GameData::InGameData::SBasicStats{});
		SCombatStats stats;
		assert(FCombat::GetPlayerStats(store, *restored, store.GetCombatRules().front(), stats, error));
		std::cout << "PASS independent numeric schema2 restore preserves HP gold original bonuses inventory and defaults\n";
	}

	static std::unique_ptr<InGameData> Fixture(const GameData::FGameDataStore& store, const InGameData& original,
		const std::function<void(SRoot&)>& edit)
	{
		std::string error;
		std::vector<std::uint8_t> bytes;
		FSerializableTypeRegistry registry;
		assert(GameData::InGameData::RegisterInGameDataTypes(registry, error));
		assert(RegisterCharacterInGameData(registry, error));
		assert(original.Serialize(bytes, error));
		SRoot root;
		assert(FSerializationArchive::Deserialize(bytes, root, error, GameData::InGameData::kSchemaVersion, &registry));
		edit(root);
		assert(FSerializationArchive::Serialize(root, bytes, error, GameData::InGameData::kSchemaVersion, &registry));
		std::unique_ptr<InGameData> result;
		assert(InGameData::Deserialize(bytes, store, result, error));
		return result;
	}

	void TestCombat(const GameData::FGameDataStore& store, const std::filesystem::path& source, const std::filesystem::path& work)
	{
		std::string error;
		const auto& rule = store.GetCombatRules().front();
		TestNumericV2(store, rule.townSceneId);
		std::unique_ptr<InGameData> town;
		assert(InGameData::CreateNew(store, rule.townSceneId, town, error));
		std::vector<std::uint8_t> before, after;
		assert(town->Serialize(before, error));
		assert(!FCombat::BasicAttack(store, *town, error));
		assert(town->Serialize(after, error) && before == after);
		assert(FCombat::Travel(store, *town, rule.battleSceneId, error));
		assert(town->Serialize(before, error));
		assert(!town->SetCurrentHp(store, 0, error));
		assert(town->Serialize(after, error) && before == after);
		assert(!town->SetSceneId(store, rule.townSceneId, error));
		assert(town->Serialize(after, error) && before == after);
		SCombatStats stats;
		assert(FCombat::GetPlayerStats(store, *town, rule, stats, error));
		assert(stats.m_attack == 16 && stats.m_defense == 10 && stats.m_speed == 6 && stats.m_criticalChance == 500);
		auto intelligence = Fixture(store, *town, [](SRoot& root)
		{
			root.m_basicBonuses.Intelligence = 100;
		});
		SCombatStats unchanged;
		assert(FCombat::GetPlayerStats(store, *intelligence, rule, unchanged, error));
		assert(unchanged.m_attack == stats.m_attack && unchanged.m_defense == stats.m_defense && unchanged.m_speed == stats.m_speed);
		auto luck = Fixture(store, *town, [](SRoot& root)
		{
			root.m_basicBonuses.Luck = 100;
		});
		assert(FCombat::GetPlayerStats(store, *luck, rule, unchanged, error));
		assert(unchanged.m_attack == stats.m_attack && unchanged.m_criticalChance == rule.criticalCap);
		auto strength = Fixture(store, *town, [](SRoot& root)
		{
			root.m_basicBonuses.Strength = 2;
		});
		assert(FCombat::GetPlayerStats(store, *strength, rule, unchanged, error));
		assert(unchanged.m_defense == stats.m_defense + 2 && unchanged.m_attack == stats.m_attack + 2);
		const auto rejectState = [&](const std::function<void(SRoot&)>& edit, const std::string& expected)
		{
			FSerializableTypeRegistry registry;
			assert(GameData::InGameData::RegisterInGameDataTypes(registry, error));
			assert(RegisterCharacterInGameData(registry, error));
			assert(town->Serialize(before, error));
			SRoot root;
			assert(FSerializationArchive::Deserialize(before, root, error, GameData::InGameData::kSchemaVersion, &registry));
			edit(root);
			std::vector<std::uint8_t> invalid;
			assert(FSerializationArchive::Serialize(root, invalid, error, GameData::InGameData::kSchemaVersion, &registry));
			const auto* prior = town.get();
			const auto* character = &town->GetCharacter();
			assert(!InGameData::Deserialize(invalid, store, town, error) && error == expected);
			assert(town.get() == prior && &town->GetCharacter() == character);
			assert(town->Serialize(after, error) && after == before);
		};
		rejectState([](SRoot& root)
		{
			root.m_battle.Outcome = 3;
		}, "Combat state bounds or outcome are invalid");
		rejectState([](SRoot& root)
		{
			root.m_battle.FirstCritical = true;
		}, "Combat state bounds or outcome are invalid");
		rejectState([](SRoot& root)
		{
			root.m_battle.MonsterHp = UINT32_MAX;
		}, "Combat state bounds or outcome are invalid");
		rejectState([](SRoot& root)
		{
			root.m_basicBonuses.Strength = UINT32_MAX;
		}, "Combat arithmetic exceeds uint32");
		rejectState([&rule](SRoot& root)
		{
			root.m_battle.Round = rule.maxRounds - 1;
			root.m_battle.RandCount = rule.maxRounds;
			root.m_battle.HitCount = 2;
			root.m_battle.FirstDamage = root.m_battle.SecondDamage = 1;
		}, "Combat state bounds or outcome are invalid");
		rejectState([&rule](SRoot& root)
		{
			root.m_battle.Round = rule.maxRounds;
			root.m_battle.RandCount = rule.maxRounds;
			root.m_battle.HitCount = 2;
			root.m_battle.FirstDamage = root.m_battle.SecondDamage = 1;
		}, "Combat state bounds or outcome are invalid");
		rejectState([&rule](SRoot& root)
		{
			root.m_battle.Round = rule.maxRounds - 1;
			root.m_battle.RandCount = rule.maxRounds;
			root.m_battle.HitCount = 2;
			root.m_battle.FirstDamage = root.m_battle.SecondDamage = 1;
			root.m_battle.MonsterHp = rule.minimumDamage + 1;
		}, "Combat state bounds or outcome are invalid");
		auto lastRound = Fixture(store, *town, [&rule](SRoot& root)
		{
			root.m_battle.Round = rule.maxRounds - 1;
			root.m_battle.RandCount = rule.maxRounds;
			root.m_battle.HitCount = 2;
			root.m_battle.FirstDamage = root.m_battle.SecondDamage = 1;
			root.m_battle.MonsterHp = rule.minimumDamage;
		});
		assert(FCombat::BasicAttack(store, *lastRound, error));
		assert(lastRound->GetBattle().Round == rule.maxRounds && lastRound->GetBattle().Outcome != 0);
		assert(FCombat::Travel(store, *lastRound, rule.townSceneId, error));
		std::cout << "PASS remaining-round exact damage budget terminates and insufficient budget rolls back\n";
		std::cout << "PASS combat original bonuses derived roles unsupported states bounds and rollback\n";
		bool playerFirst = false, monsterFirst = false, critical = false, ordinary = false;
		for (std::uint64_t seed = 1; seed <= 128; ++seed)
		{
			auto battle = Fixture(store, *town, [seed](SRoot& root)
			{
				root.m_battle.Seed = seed;
			});
			assert(FCombat::BasicAttack(store, *battle, error));
			const auto state = battle->GetBattle();
			playerFirst |= state.FirstIsPlayer;
			monsterFirst |= !state.FirstIsPlayer;
			critical |= state.FirstCritical || state.SecondCritical;
			ordinary |= !state.FirstCritical || !state.SecondCritical;
			assert(state.HitCount == 2 && state.Round == 1);
			assert(battle->Serialize(before, error));
			std::unique_ptr<InGameData> resumed;
			assert(InGameData::Deserialize(before, store, resumed, error));
			assert(FCombat::BasicAttack(store, *battle, error) && FCombat::BasicAttack(store, *resumed, error));
			assert(battle->GetBattle() == resumed->GetBattle());
			assert(battle->GetCharacter().CurrentHp == resumed->GetCharacter().CurrentHp);
			assert(resumed->Serialize(after, error));
			assert(battle->Serialize(before, error) && before == after);
		}
		assert(playerFirst && monsterFirst && critical && ordinary);
		std::cout << "PASS combat seeded both initiative orders crit noncrit and exact round-boundary resume\n";

		bool won = false, lost = false;
		for (std::uint64_t seed = 1; seed <= 32; ++seed)
		{
			auto lethal = Fixture(store, *town, [seed](SRoot& root)
			{
				root.m_battle.Seed = seed;
				root.m_battle.MonsterHp = 1;
				root.m_battle.Round = 1;
				root.m_battle.RandCount = 3;
				root.m_battle.HitCount = 2;
				root.m_battle.FirstDamage = root.m_battle.SecondDamage = 1;
				root.m_character->CurrentHp = 1;
			});
			assert(FCombat::BasicAttack(store, *lethal, error));
			const auto state = lethal->GetBattle();
			assert(state.HitCount == 1 && state.SecondDamage == 0 && !state.SecondCritical && state.Outcome != 0);
			won |= state.Outcome == 1;
			lost |= state.Outcome == 2;
			assert(lethal->Serialize(before, error));
			assert(!FCombat::BasicAttack(store, *lethal, error));
			assert(lethal->Serialize(after, error) && after == before);
			assert(FCombat::Travel(store, *lethal, rule.townSceneId, error));
			assert(lethal->GetBattle().RuleId == 0 && lethal->GetCharacter().CurrentHp > 0);
		}
		assert(won && lost);
		std::cout << "PASS combat lethal stops retaliation victory defeat extra action preservation and healed return\n";

		// 현재 정의를 실제 암호화 패키지로 변형해 확률 양끝을 강제한다. 원본 생성물은 변경하지 않는다.
		const auto yaml = work / "combat-yaml";
		std::filesystem::copy(source / "Data/Client", yaml, std::filesystem::copy_options::recursive);
		std::ifstream input(yaml / "CombatRule.yaml", std::ios::binary);
		const std::string normal((std::istreambuf_iterator<char>(input)), {});
		input.close();
		std::array<std::uint8_t, 32> key{};
		for (const bool guaranteed : {false, true})
		{
			auto text = normal;
			const auto replace = [&text](const std::string& from, const std::string& to)
			{
				const auto at = text.find(from);
				assert(at != std::string::npos);
				text.replace(at, from.size(), to);
			};
			replace("\"LuckCritical\": 100", guaranteed ? "\"LuckCritical\": 10000" : "\"LuckCritical\": 0");
			replace("\"CriticalCap\": 5000", "\"CriticalCap\": 10000");
			std::ofstream(yaml / "CombatRule.yaml", std::ios::binary) << text;
			const auto package = work / "combat.dat";
			assert(GameData::BuildEncryptedPackage(yaml, package, key, error, source / "Asset"));
			GameData::FGameDataStore variant;
			assert(variant.Load(package, key, error));
			std::unique_ptr<InGameData> combat;
			assert(InGameData::CreateNew(variant, rule.townSceneId, combat, error));
			assert(FCombat::Travel(variant, *combat, rule.battleSceneId, error));
			assert(FCombat::BasicAttack(variant, *combat, error));
			assert(combat->GetBattle().FirstCritical == guaranteed && combat->GetBattle().SecondCritical == guaranteed);
		}
		std::cout << "PASS combat real Store data forced zero and full critical probability both sides same path\n";
		auto capped = normal;
		const auto capAt = capped.find("\"MaxRounds\": 1000000");
		assert(capAt != std::string::npos);
		capped.replace(capAt, std::string("\"MaxRounds\": 1000000").size(), "\"MaxRounds\": 1");
		std::ofstream(yaml / "CombatRule.yaml", std::ios::binary) << capped;
		const auto cappedPackage = work / "capped.dat";
		assert(GameData::BuildEncryptedPackage(yaml, cappedPackage, key, error, source / "Asset"));
		GameData::FGameDataStore cappedStore;
		const bool acceptedCap = cappedStore.Load(cappedPackage, key, error);
		if (acceptedCap)
		{
			std::unique_ptr<InGameData> combat;
			assert(InGameData::CreateNew(cappedStore, rule.townSceneId, combat, error));
			assert(FCombat::Travel(cappedStore, *combat, rule.battleSceneId, error));
			assert(FCombat::BasicAttack(cappedStore, *combat, error));
			assert(combat->GetBattle().Round == 1 && combat->GetBattle().Outcome == 0);
			assert(combat->GetCharacter().CurrentHp != 0 && combat->GetBattle().MonsterHp != 0);
			assert(!FCombat::BasicAttack(cappedStore, *combat, error));
			assert(!FCombat::Travel(cappedStore, *combat, rule.townSceneId, error));
			std::cerr << "cap=1 witness: both alive, round=1, outcome=0, attack and return rejected\n";
		}
		assert(!acceptedCap);
		assert(error == "Combat catalog cohort: CombatRule MaxRounds cannot guarantee termination");
		std::cout << "PASS too-short encounter cap rejects with exact termination diagnostic\n";
		// 파서만이 아닌 실제 인증 패키지로 상한 양성과 상한+1 음성을 검사한다.
		std::ofstream(yaml / "CombatRule.yaml", std::ios::binary) << normal;
		std::ifstream monsterInput(yaml / "MonsterDefinition.yaml", std::ios::binary);
		const std::string monsterNormal((std::istreambuf_iterator<char>(monsterInput)), {});
		monsterInput.close();
		for (const auto attack : {1000000U, 1000001U})
		{
			auto text = monsterNormal;
			const auto at = text.find("\"Attack\": 7\n");
			assert(at != std::string::npos);
			text.replace(at, std::string("\"Attack\": 7\n").size(), "\"Attack\": " + std::to_string(attack) + "\n");
			std::ofstream(yaml / "MonsterDefinition.yaml", std::ios::binary) << text;
			assert(GameData::BuildEncryptedPackage(yaml, cappedPackage, key, error, source / "Asset"));
			GameData::FGameDataStore variant;
			const bool accepted = variant.Load(cappedPackage, key, error);
			std::cerr << "MonsterDefinition.Attack=" << attack << " accepted=" << accepted << " error=" << error << '\n';
			assert(accepted == (attack == 1000000));
			if (!accepted)
				assert(error == "Data/Client/MonsterDefinition.yaml: row at line 2: Attack: combat workbook range exceeded");
		}
		std::cout << "PASS authenticated Store monster attack workbook boundary and boundary+1\n";
		std::map<std::string, std::string> originals;
		for (const auto* table : {"CharacterCombat", "MonsterDefinition", "CombatRule"})
		{
			std::ifstream file(source / "Data/Client" / (std::string(table) + ".yaml"), std::ios::binary);
			originals[table] = std::string((std::istreambuf_iterator<char>(file)), {});
		}
		const auto setNumber = [](std::string& text, const std::string& field, std::uint32_t value)
		{
			const auto prefix = "\"" + field + "\": ";
			const auto at = text.find(prefix);
			assert(at != std::string::npos);
			const auto end = text.find('\n', at);
			text.replace(at + prefix.size(), end - at - prefix.size(), std::to_string(value));
		};
		// 정상 Store가 수용한 큰 치명타는 대상 HP로 제한한 뒤에만 축소해야 한다.
		for (const bool outgoing : {false, true})
		{
			auto data = originals;
			for (const auto* field : {"StrengthDefense", "DexteritySpeed", "StrengthCoefficient",
				"DexterityCoefficient", "IntelligenceCoefficient", "LuckCoefficient"})
				setNumber(data["CombatRule"], field, 0);
			for (const auto* field : {"Scale", "BaseSpeed", "LuckCritical", "CriticalCap", "MinimumDamage"})
				setNumber(data["CombatRule"], field, 1);
			setNumber(data["CombatRule"], "CriticalMultiplier", 10000);
			setNumber(data["MonsterDefinition"], "MaxHp", 1000000);
			setNumber(data["MonsterDefinition"], "Attack", outgoing ? 1U : 1000000U);
			for (const auto& [name, text] : data)
				std::ofstream(yaml / (name + ".yaml"), std::ios::binary) << text;
			assert(GameData::BuildEncryptedPackage(yaml, cappedPackage, key, error, source / "Asset"));
			GameData::FGameDataStore variant;
			assert(variant.Load(cappedPackage, key, error));
			std::unique_ptr<InGameData> created;
			assert(InGameData::CreateNew(variant, rule.townSceneId, created, error));
			const auto& wideRule = variant.GetCombatRules().front();
			auto prepared = Fixture(variant, *created, [outgoing](SRoot& root)
			{
				if (outgoing)
				{
					root.m_character->StatBonuses.Attack = 1000000;
					root.m_character->StatBonuses.MaxHp = 10000;
					root.m_character->CurrentHp = 10100;
				}
			});
			SCombatStats derived;
			assert(FCombat::GetPlayerStats(variant, *prepared, wideRule, derived, error));
			assert(derived.m_attack == (outgoing ? 1000010U : 10U) && derived.m_defense == 5);
			assert(FCombat::Travel(variant, *prepared, rule.battleSceneId, error));
			std::cerr << "wide-critical " << (outgoing ? "player" : "monster")
				<< " Store/CreateNew/Travel positive attack=" << derived.m_attack << '\n';
			FProcTable initiative, guaranteed;
			assert(initiative.AddProc(1, 1, error) && initiative.AddProc(2, 1, error) && initiative.Prepare(error));
			assert(guaranteed.AddProc(1, 1, error) && guaranteed.AddProc(2, 0, error) && guaranteed.Prepare(error));
			for (const bool firstIsPlayer : {false, true})
			{
				std::uint64_t seed = 1;
				for (; seed <= 128; ++seed)
				{
					FSelector probe(seed);
					SSelectResult selected;
					assert(probe.SelectKey(initiative, selected, error));
					if ((selected.key == 1) == firstIsPlayer) break;
				}
				assert(seed <= 128);
				auto combat = Fixture(variant, *prepared, [seed](SRoot& root)
				{
					root.m_battle.Seed = seed;
				});
				assert(combat->Serialize(before, error));
				assert(!FCombat::Travel(variant, *combat, rule.townSceneId, error));
				assert(combat->Serialize(after, error) && before == after);
				const auto initialHp = combat->GetCharacter().CurrentHp;
				const auto hitCount = (outgoing == firstIsPlayer) ? 1U : 2U;
				FSelector expected(seed);
				SSelectResult selected;
				assert(expected.SelectKey(initiative, selected, error));
				assert((selected.key == 1) == firstIsPlayer);
				for (std::uint32_t hit = 0; hit < hitCount; ++hit)
					assert(expected.SelectKey(guaranteed, selected, error) && selected.key == 1);
				const auto rawDamage = outgoing ? static_cast<std::uint64_t>(derived.m_attack - 2) * 10000 : 9999950000ULL;
				assert(rawDamage > UINT32_MAX);
				const bool attacked = FCombat::BasicAttack(variant, *combat, error);
				std::cerr << "wide-critical " << (outgoing ? "player" : "monster") << " seed=" << seed
					<< " firstIsPlayer=" << firstIsPlayer << " raw=" << rawDamage << " accepted=" << attacked
					<< " error=" << error << '\n';
				if (!attacked)
				{
					assert(error == "Combat arithmetic exceeds uint32");
					assert(combat->Serialize(after, error) && before == after);
					std::cerr << "wide-critical rejected attack preserves serialized HP/round/RNG\n";
				}
				assert(attacked);
				const auto state = combat->GetBattle();
				assert(state.FirstIsPlayer == firstIsPlayer && state.FirstCritical && state.HitCount == hitCount);
				assert(state.Round == 1 && state.Seed == seed && state.RandCount == expected.GetState().randCount);
				const auto playerDamage = outgoing ? 1000000U : 80000U;
				const auto monsterDamage = outgoing ? 10000U : initialHp;
				assert(state.FirstDamage == (firstIsPlayer ? playerDamage : monsterDamage));
				assert(state.SecondDamage == (hitCount == 1 ? 0U : (firstIsPlayer ? monsterDamage : playerDamage)));
				assert(state.SecondCritical == (hitCount == 2));
				assert(state.Outcome == (outgoing ? 1U : 2U));
				assert(state.MonsterHp == (outgoing ? 0U : (firstIsPlayer ? 920000U : 1000000U)));
				assert(combat->GetCharacter().CurrentHp == (outgoing ? initialHp - (firstIsPlayer ? 0U : monsterDamage) : 0U));
				assert(combat->GetCharacter().StatBonuses == prepared->GetCharacter().StatBonuses);
				assert(combat->Serialize(before, error));
				std::unique_ptr<InGameData> resumed;
				assert(InGameData::Deserialize(before, variant, resumed, error));
				assert(resumed->GetBattle() == state && resumed->GetCharacter().CurrentHp == combat->GetCharacter().CurrentHp);
				assert(!FCombat::BasicAttack(variant, *combat, error));
				assert(combat->Serialize(after, error) && before == after);
				assert(FCombat::Travel(variant, *combat, rule.townSceneId, error));
				assert(FCombat::Travel(variant, *resumed, rule.townSceneId, error));
				assert(combat->GetSceneId() == rule.townSceneId && combat->GetBattle() == GameData::InGameData::SBattleProgress{});
				assert(combat->GetCharacter().CurrentHp == (outgoing ? 10100U : 100U));
				assert(combat->Serialize(before, error) && resumed->Serialize(after, error) && before == after);
			}
		}
		std::cout << "PASS wide critical both actors both orders HP cap no retaliation exact RNG resume and finished return\n";
		const std::tuple<const char*, const char*, std::uint32_t> limits[]{
			{"CharacterCombat", "Strength", 1000000U},
			{"CharacterCombat", "Dexterity", 1000000U},
			{"CharacterCombat", "Intelligence", 1000000U},
			{"CharacterCombat", "Luck", 1000000U},
			{"MonsterDefinition", "MonsterId", 1000000U},
			{"MonsterDefinition", "MaxHp", 1000000U},
			{"MonsterDefinition", "Attack", 1000000U},
			{"MonsterDefinition", "Defense", 1000000U},
			{"MonsterDefinition", "Strength", 1000000U},
			{"MonsterDefinition", "Dexterity", 1000000U},
			{"MonsterDefinition", "Intelligence", 1000000U},
			{"MonsterDefinition", "Luck", 1000000U},
			{"CombatRule", "RuleId", 1000000U},
			{"CombatRule", "MonsterId", 1000000U},
			{"CombatRule", "Scale", 1000000U},
			{"CombatRule", "StrengthDefense", 1000000U},
			{"CombatRule", "DexteritySpeed", 1000000U},
			{"CombatRule", "BaseSpeed", 1000000U},
			{"CombatRule", "LuckCritical", 1000000U},
			{"CombatRule", "CriticalCap", 1000000U},
			{"CombatRule", "CriticalMultiplier", 1000000U},
			{"CombatRule", "MinimumDamage", 1000000U},
			{"CombatRule", "StrengthCoefficient", 1000000U},
			{"CombatRule", "DexterityCoefficient", 1000000U},
			{"CombatRule", "IntelligenceCoefficient", 1000000U},
			{"CombatRule", "LuckCoefficient", 1000000U},
			{"CombatRule", "MaxStat", 1000000U},
			{"CombatRule", "MaxRounds", 1000000U},
			{"CombatRule", "HealOnReturn", 1U},
		};
		for (const auto& [table, field, maximum] : limits)
		{
			for (const auto value : {maximum, maximum + 1})
			{
				auto data = originals;
				// 참조·확률·종료성 양성 조건을 유지하도록 관련 값을 함께 조정한다.
				if (std::string_view(field) == "MonsterId")
				{
					setNumber(data["MonsterDefinition"], "MonsterId", value);
					setNumber(data["CombatRule"], "MonsterId", value);
				}
				if (std::string_view(field) == "Scale" || std::string_view(field) == "CriticalCap")
				{
					setNumber(data["CombatRule"], "Scale", maximum);
					setNumber(data["CombatRule"], "CriticalMultiplier", maximum);
				}
				setNumber(data[table], field, value);
				for (const auto& [name, text] : data)
					std::ofstream(yaml / (name + ".yaml"), std::ios::binary) << text;
				assert(GameData::BuildEncryptedPackage(yaml, cappedPackage, key, error, source / "Asset"));
				GameData::FGameDataStore variant;
				const bool accepted = variant.Load(cappedPackage, key, error);
				if (accepted != (value == maximum))
					std::cerr << "range " << table << '.' << field << '=' << value << " accepted=" << accepted << ' ' << error << '\n';
				assert(accepted == (value == maximum));
				if (!accepted && std::string_view(field) != "HealOnReturn")
				{
					const std::string rejectedTable = std::string_view(field) == "MonsterId" ? "CombatRule" : table;
					assert(error == "Data/Client/" + rejectedTable + ".yaml: row at line 2: " + field +
						": combat workbook range exceeded");
				}
				else if (!accepted)
					assert(error == "Combat catalog cohort: CombatRule bounds are invalid");
			}
		}
		std::cout << "PASS all finite Combat workbook upper boundaries and boundary+1 with complete positive controls\n";
		for (const auto rounds : {23U, 22U})
		{
			auto data = originals;
			setNumber(data["CombatRule"], "MinimumDamage", 2);
			setNumber(data["CombatRule"], "MaxRounds", rounds);
			for (const auto& [name, text] : data)
				std::ofstream(yaml / (name + ".yaml"), std::ios::binary) << text;
			assert(GameData::BuildEncryptedPackage(yaml, cappedPackage, key, error, source / "Asset"));
			GameData::FGameDataStore variant;
			const bool accepted = variant.Load(cappedPackage, key, error);
			assert(accepted == (rounds == 23));
			if (!accepted)
				assert(error == "Combat catalog cohort: CombatRule MaxRounds cannot guarantee termination");
			else
			{
				std::unique_ptr<InGameData> combat;
				assert(InGameData::CreateNew(variant, rule.townSceneId, combat, error));
				assert(FCombat::Travel(variant, *combat, rule.battleSceneId, error));
				while (combat->GetBattle().Outcome == 0)
					assert(FCombat::BasicAttack(variant, *combat, error));
				assert(combat->GetBattle().Round <= rounds);
				assert(FCombat::Travel(variant, *combat, rule.townSceneId, error));
			}
		}
		std::cout << "PASS ceil nondivisible termination bound exact positive and one-below negative\n";
	}
}
