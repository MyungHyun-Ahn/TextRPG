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
// 기존 모델 타깃에서 원본 저장 서비스를 함께 검사하며 별도 runner나 소스 사본은 만들지 않는다.
#include "InGameData/FSaveDataStore.cpp"

namespace TextRPGTest
{
	using namespace TextRPG;
	using SRoot = GameData::InGameData::SRootInGameData;

#ifdef _DEBUG
	struct SRewardAllocationFault
	{
		int m_remaining = 0;
		bool m_hit = false;
		bool m_selector = false;
	};
	static SRewardAllocationFault g_rewardFault;
	static int __cdecl RewardAllocationHook(int type, void*, std::size_t size, int block, long, const unsigned char*, int)
	{
		// seed_seq 첫 push_back의 4바이트 payload는 Win32의 8바이트 noexcept proxy와도 구분된다.
		const bool payload = size == (g_rewardFault.m_selector ? sizeof(std::uint32_t) :
			sizeof(GameData::InGameData::SCurrencyRewardProgress)) && size != sizeof(std::_Container_proxy);
		if (type == _HOOK_ALLOC && block == _NORMAL_BLOCK && payload && !g_rewardFault.m_hit && --g_rewardFault.m_remaining == 0)
		{
			g_rewardFault.m_hit = true;
			return 0;
		}
		return 1;
	}
#endif

	// 전투 도입 전 숫자 스키마2의 독립 writer: 과거 필드 여섯 개만 기록한다.
	class FNumericV2Root : public ISerializable
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

	// schema3 전투 writer는 새 영수증 필드를 전혀 알지 못한다.
	class FLegacyBattle final : public ISerializable
	{
	public:
		static constexpr std::string_view kTypeName = "SBattleProgress";
		std::uint32_t m_ruleId{};
		std::uint32_t m_monsterId{};
		std::uint32_t m_monsterHp{};
		std::uint32_t m_round{};
		std::uint32_t m_outcome{};
		std::uint64_t m_seed{};
		std::uint64_t m_randCount{};
		bool m_firstIsPlayer{};
		std::uint32_t m_firstDamage{};
		std::uint32_t m_secondDamage{};
		bool m_firstCritical{};
		bool m_secondCritical{};
		std::uint32_t m_hitCount{};
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
		{
			return writer.Field("RuleId", m_ruleId, error) &&
				writer.Field("MonsterId", m_monsterId, error) &&
				writer.Field("MonsterHp", m_monsterHp, error) &&
				writer.Field("Round", m_round, error) &&
				writer.Field("Outcome", m_outcome, error) &&
				writer.Field("Seed", m_seed, error) &&
				writer.Field("RandCount", m_randCount, error) &&
				writer.Field("FirstIsPlayer", m_firstIsPlayer, error) &&
				writer.Field("FirstDamage", m_firstDamage, error) &&
				writer.Field("SecondDamage", m_secondDamage, error) &&
				writer.Field("FirstCritical", m_firstCritical, error) &&
				writer.Field("SecondCritical", m_secondCritical, error) &&
				writer.Field("HitCount", m_hitCount, error);
		}
		bool DeserializeFields(FBinaryReader&, std::string&) override
		{
			return false;
		}
	};
	class FNumericV3Root final : public FNumericV2Root
	{
	public:
		GameData::InGameData::SBasicStats m_basicBonuses;
		FLegacyBattle m_battle;
	protected:
		bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
		{
			return FNumericV2Root::SerializeFields(writer, error) && writer.Field("m_basicBonuses", m_basicBonuses, error) &&
				writer.Field("m_battle", m_battle, error);
		}
	};

	// 당시 선언된 필드만 쓰는 독립 schema4 writer. 최신 writer의 버전 숫자만 바꾸지 않는다.
	class FNumericV4Reward final : public ISerializable
	{
	public:
		static constexpr std::string_view kTypeName = "SBattleRewardProgress";
		GameData::InGameData::SBattleRewardProgress m_value;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
		{
			return writer.Field("State", m_value.State, error) &&
				writer.Field("PlannedExperience", m_value.PlannedExperience, error) &&
				writer.Field("GrantedExperience", m_value.GrantedExperience, error) &&
				writer.Field("LevelBefore", m_value.LevelBefore, error) && writer.Field("LevelAfter", m_value.LevelAfter, error) &&
				writer.Field("CurrencySeed", m_value.CurrencySeed, error) &&
				writer.Field("CurrencyRandCount", m_value.CurrencyRandCount, error) &&
				writer.Field("Currencies", m_value.Currencies, error);
		}
		bool DeserializeFields(FBinaryReader&, std::string&) override
		{
			return false;
		}
	};
	class FNumericV4Battle final : public ISerializable
	{
	public:
		static constexpr std::string_view kTypeName = "SBattleProgress";
		GameData::InGameData::SBattleProgress m_value;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
		{
			FNumericV4Reward reward;
			reward.m_value = m_value.Reward;
			return writer.Field("Reward", reward, error) && writer.Field("RuleId", m_value.RuleId, error) &&
				writer.Field("MonsterId", m_value.MonsterId, error) && writer.Field("MonsterHp", m_value.MonsterHp, error) &&
				writer.Field("Round", m_value.Round, error) && writer.Field("Outcome", m_value.Outcome, error) &&
				writer.Field("Seed", m_value.Seed, error) && writer.Field("RandCount", m_value.RandCount, error) &&
				writer.Field("FirstIsPlayer", m_value.FirstIsPlayer, error) && writer.Field("FirstDamage", m_value.FirstDamage, error) &&
				writer.Field("SecondDamage", m_value.SecondDamage, error) && writer.Field("FirstCritical", m_value.FirstCritical, error) &&
				writer.Field("SecondCritical", m_value.SecondCritical, error) && writer.Field("HitCount", m_value.HitCount, error);
		}
		bool DeserializeFields(FBinaryReader&, std::string&) override
		{
			return false;
		}
	};
	class FNumericV4Root final : public ISerializable
	{
	public:
		static constexpr std::string_view kTypeName = "SRootInGameData";
		SRoot m_value;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
		{
			FNumericV4Battle battle;
			battle.m_value = m_value.m_battle;
			return writer.Field("m_contentVersion", m_value.m_contentVersion, error) &&
				writer.Field("m_sceneId", m_value.m_sceneId, error) && writer.Field("m_character", m_value.m_character, error) &&
				writer.Field("m_currencies", m_value.m_currencies, error) && writer.Field("m_inventory", m_value.m_inventory, error) &&
				writer.Field("m_basicBonuses", m_value.m_basicBonuses, error) && writer.Field("m_battle", battle, error) &&
				writer.Field("m_achievements", m_value.m_achievements, error);
		}
		bool DeserializeFields(FBinaryReader&, std::string&) override
		{
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
		old.m_character->Level = 2;
		old.m_character->Experience = 380;
		assert(FSerializationArchive::Serialize(old, bytes, error, 2, &registry));
		assert(InGameData::Deserialize(bytes, store, restored, error));
		assert(restored->GetCharacter().Level == 3 && restored->GetCharacter().Experience == 380);
		assert(restored->GetCharacter().CurrentHp == 7 && restored->GetGold() == 54321);
		assert(restored->Serialize(bytes, error) && InGameData::Deserialize(bytes, store, restored, error));
		assert(restored->GetGold() == 54321 && restored->GetCharacter().CurrentHp == 7);
		// schema4에서 구 Gold만 기록한 writer는 필수 wallet 누락으로 거부한다.
		assert(FSerializationArchive::Serialize(old, bytes, error, 4, &registry));
		const auto* retained = restored.get();
		assert(!InGameData::Deserialize(bytes, store, restored, error) && restored.get() == retained);
		assert(error == "missing required field");
		std::cout << "PASS legacy max XP curve extension preserves HP currency and schema4 missing wallet rejects\n";
		old.m_character.reset();
		assert(FSerializationArchive::Serialize(old, bytes, error, 2, &registry));
		assert(!InGameData::Deserialize(bytes, store, restored, error));
		assert(error == "Character progress is missing" && restored.get() == retained);
		std::cout << "PASS numeric legacy null character rejects before curve migration and preserves active root\n";
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

	static void TestCurrencyReceiptDeterminism(const GameData::FGameDataStore& store, const InGameData& active)
	{
		std::string error;
		FSerializableTypeRegistry registry;
		assert(GameData::InGameData::RegisterInGameDataTypes(registry, error));
		assert(RegisterCharacterInGameData(registry, error));
		bool allRejected = true;
		for (const bool saturated : {false, true})
		{
			auto live = Fixture(store, active, [saturated](SRoot& root)
			{
				root.m_character->StatBonuses.Attack = 1000;
				root.m_currencies.front().Amount = saturated ? UINT32_MAX : 10;
				root.m_battle.Reward.CurrencySeed = 71;
				auto& entry = root.m_battle.Reward.Currencies.front();
				entry.AmountMin = 10;
				entry.AmountMax = 20;
				if (GameData::Common::IsValidCurrencyType(static_cast<GameData::Common::ECurrencyType>(2)))
					root.m_battle.Reward.Currencies.push_back({2, 3, 9, 0, 0});
			});
			assert(live->SetInventory(store, {{1, 1, 1}}, error));
			assert(live->SetAchievements(store, {{1, 0, false}}, error));
			assert(FCombat::BasicAttack(store, *live, error) && live->GetBattle().Outcome == 1);
			assert(live->GetBattle().Reward.CurrencyRandCount > 0);
			assert(!saturated || live->GetBattle().Reward.Currencies.front().GrantedAmount == 0);
			std::vector<std::uint8_t> valid, encoded, preserved;
			assert(live->Serialize(valid, error));
			const auto* rootAddress = live.get();
			const auto* character = &live->GetCharacter();
			const auto* items = live->GetInventory().data();
			const auto* awards = live->GetAchievements().data();
			const auto* currencies = live->GetBattle().Reward.Currencies.data();
			for (const bool old4 : {false, true})
			{
				for (std::uint32_t mutation = 0; mutation < (saturated ? 1U : 2U); ++mutation)
				{
					SRoot root;
					assert(FSerializationArchive::Deserialize(valid, root, error, GameData::InGameData::kSchemaVersion, &registry));
					const auto encode = [&]()
					{
						if (!old4) return FSerializationArchive::Serialize(root, encoded, error,
							GameData::InGameData::kSchemaVersion, &registry);
						FNumericV4Root writer;
						writer.m_value = root;
						return FSerializationArchive::Serialize(writer, encoded, error, 4, &registry);
					};
					assert(encode());
					std::unique_ptr<InGameData> control;
					assert(InGameData::Deserialize(encoded, store, control, error));
					assert(control->Serialize(preserved, error) && preserved == valid);
					if (saturated)
					{
						auto& roll = root.m_battle.Reward.Currencies.front().RolledAmount;
						roll = roll == 20 ? 10 : roll + 1;
					}
					else root.m_battle.Reward.CurrencyRandCount = mutation == 0 ? 0 : root.m_battle.Reward.CurrencyRandCount + 1;
					assert(encode());
					const bool accepted = InGameData::Deserialize(encoded, store, live, error);
					const std::string expected = saturated ? "Reward currency roll does not match snapshot RNG" :
						"Reward currency count does not match snapshot RNG";
					std::cerr << "RUNTIME-01 schema=" << (old4 ? 4 : 5) << " saturated=" << saturated
						<< " accepted=" << accepted << " error=" << error << '\n';
					allRejected &= !accepted && error == expected;
					if (!accepted)
					{
						assert(live.get() == rootAddress && &live->GetCharacter() == character);
						assert(live->GetInventory().data() == items && live->GetAchievements().data() == awards);
						assert(live->GetBattle().Reward.Currencies.data() == currencies);
						assert(live->Serialize(preserved, error) && preserved == valid);
					}
				}
			}
#ifdef _DEBUG
			// MSVC v145 seed_seq 첫 payload의 단일 할당을 양성 보정한 뒤 복구 가능한 실패만 주입한다.
			if (allRejected)
			{
				g_rewardFault = {2, false, true};
				const auto previousHook = _CrtSetAllocHook(RewardAllocationHook);
				const bool calibrated = FCombat::Validate(store, *live, error);
				(void)_CrtSetAllocHook(previousHook);
				assert(calibrated && error.empty() && !g_rewardFault.m_hit && g_rewardFault.m_remaining == 1);
				g_rewardFault = {1, false, true};
				(void)_CrtSetAllocHook(RewardAllocationHook);
				const bool validated = FCombat::Validate(store, *live, error);
				(void)_CrtSetAllocHook(previousHook);
				std::cerr << "RUNTIME-01 selector proxy=" << sizeof(std::_Container_proxy)
					<< " payload=" << sizeof(std::uint32_t) << " calibrated=" << calibrated
					<< " hit=" << g_rewardFault.m_hit << " validated=" << validated << " error=" << error << '\n';
				assert(g_rewardFault.m_hit && !validated && error == "Random: engine initialization failed");
				assert(live.get() == rootAddress && &live->GetCharacter() == character);
				assert(live->GetInventory().data() == items && live->GetAchievements().data() == awards);
				assert(live->GetBattle().Reward.Currencies.data() == currencies);
				assert(live->Serialize(preserved, error) && preserved == valid);
				assert(FCombat::Validate(store, *live, error));
			}
#endif
			// 정산 뒤 허용된 잔액 변경은 역사적 추첨 검증과 독립이다.
			if (allRejected)
			{
				const auto receipt = live->GetBattle().Reward;
				assert(live->SetGold(1, error) && live->Serialize(preserved, error));
				std::unique_ptr<InGameData> changedWallet;
				assert(InGameData::Deserialize(preserved, store, changedWallet, error));
				assert(changedWallet->GetGold() == 1 && changedWallet->GetBattle().Reward == receipt);
			}
		}
		assert(allRejected);
		std::cout << "PASS deterministic currency count and saturated roll schema4/5 reject with refs bytes preserved\n";
	}

	static void TestCurveCodecCapacity(const GameData::FGameDataStore& store,
		const std::filesystem::path& source, const std::filesystem::path& work)
	{
		std::string error;
		std::unique_ptr<InGameData> original;
		const auto scene = store.GetCombatRules().front().townSceneId;
		assert(InGameData::CreateNew(store, scene, original, error));
		assert(original->SetInventory(store, {{1, 1, 1}}, error));
		assert(original->SetAchievements(store, {{1, 0, false}}, error));
		assert(original->SetCurrentHp(store, 7, error) && original->SetGold(54321, error));
		std::vector<std::uint8_t> prefix;
		assert(original->Serialize(prefix, error));
		const auto yaml = work / "curve-codec-yaml";
		std::filesystem::copy(source / "Data/Client", yaml, std::filesystem::copy_options::recursive);
		const auto readBytes = [](const std::filesystem::path& file)
		{
			std::ifstream input(file, std::ios::binary);
			assert(input);
			return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)), {});
		};
		const auto saveDirectory = work / "curve-codec-save";
		FSaveDataStore files(saveDirectory);
		assert(files.Save(prefix, error));
		const auto slot = saveDirectory / "InGameData.sav";
		const auto keyFile = saveDirectory / "SaveData.key";
		const auto slotBefore = readBytes(slot);
		const auto keyBefore = readBytes(keyFile);
		std::ifstream input(yaml / "LevelDefinition.yaml", std::ios::binary);
		std::string levels((std::istreambuf_iterator<char>(input)), {});
		input.close();
		std::array<std::uint8_t, 32> key{};
		const auto package = work / "curve-codec.dat";
		bool allRejected = true;
		for (std::uint32_t count = 4096; count <= 4097; ++count)
		{
			const std::uint32_t first = count == 4096 ? 5 : count;
			for (std::uint32_t level = first; level <= count; ++level)
			{
				levels += "\"LevelDefinition" + std::to_string(level) + "\":\n  \"Level\": " + std::to_string(level) +
					"\n  \"RequiredExperience\": " + std::to_string(450 + (level - 4) * 200) +
					"\n  \"MaxHp\": 160\n  \"Attack\": 16\n  \"Defense\": 8\n";
			}
			std::ofstream(yaml / "LevelDefinition.yaml", std::ios::binary) << levels;
			assert(GameData::BuildEncryptedPackage(yaml, package, key, error, source / "Asset"));
			GameData::FGameDataStore largeStore;
			const bool loaded = largeStore.Load(package, key, error);
			std::cerr << "DATA-01 cohort=" << count << " Store=" << loaded << " error=" << error << '\n';
			assert(loaded && largeStore.GetLevelDefinitions().size() == count);
			if (count == 4096)
			{
				std::unique_ptr<InGameData> boundary, resumed;
				assert(InGameData::CreateNew(largeStore, scene, boundary, error));
				std::vector<std::uint8_t> bytes, repeated;
				assert(boundary->Serialize(bytes, error));
				FSaveDataStore boundaryFile(work / "curve-codec-4096");
				assert(boundaryFile.Save(bytes, error));
				std::vector<std::uint8_t> loadedBytes;
				assert(boundaryFile.Load(loadedBytes, error) && loadedBytes == bytes);
				assert(InGameData::Deserialize(loadedBytes, largeStore, resumed, error));
				assert(resumed->Serialize(repeated, error) && bytes == repeated);
				assert(InGameData::Deserialize(prefix, largeStore, resumed, error));
				assert(resumed->GetCharacter() == original->GetCharacter() && resumed->GetGold() == original->GetGold());
				assert(resumed->Serialize(bytes, error) && InGameData::Deserialize(bytes, largeStore, resumed, error));
				std::cout << "PASS complete Store 4096 curve Create authenticated save load and old prefix Continue\n";
				continue;
			}
			for (const bool continuing : {false, true})
			{
				std::unique_ptr<InGameData> live;
				assert(InGameData::Deserialize(prefix, store, live, error));
				const auto* rootAddress = live.get();
				const auto* character = &live->GetCharacter();
				const auto* items = live->GetInventory().data();
				const auto* awards = live->GetAchievements().data();
				std::vector<std::uint8_t> loadedPrefix;
				assert(files.Load(loadedPrefix, error) && loadedPrefix == prefix);
				const bool accepted = continuing ? InGameData::Deserialize(loadedPrefix, largeStore, live, error) :
					InGameData::CreateNew(largeStore, scene, live, error);
				std::cerr << "DATA-01 cohort=4097 Continue=" << continuing << " accepted=" << accepted
					<< " error=" << error << '\n';
				allRejected &= !accepted && error == "Character curve provenance exceeds serialization capacity";
				if (!accepted)
				{
					assert(live.get() == rootAddress && &live->GetCharacter() == character);
					assert(live->GetInventory().data() == items && live->GetAchievements().data() == awards);
					std::vector<std::uint8_t> after;
					assert(live->Serialize(after, error) && after == prefix);
				}
				assert(readBytes(slot) == slotBefore && readBytes(keyFile) == keyBefore);
			}
		}
		assert(allRejected);
		std::cout << "PASS complete Store 4097 rejects NewGame Continue before output with refs bytes slot key preserved\n";
	}

	void TestCombat(const GameData::FGameDataStore& store, const std::filesystem::path& source, const std::filesystem::path& work)
	{
		std::string error;
		const auto& rule = store.GetCombatRules().front();
		TestNumericV2(store, rule.townSceneId);
		TestCurveCodecCapacity(store, source, work);
		std::unique_ptr<InGameData> town;
		assert(InGameData::CreateNew(store, rule.townSceneId, town, error));
		std::vector<std::uint8_t> before, after;
		assert(town->Serialize(before, error));
		assert(!FCombat::BasicAttack(store, *town, error));
		assert(town->Serialize(after, error) && before == after);
		assert(FCombat::Travel(store, *town, rule.battleSceneId, error));
		TestCurrencyReceiptDeterminism(store, *town);
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
		rejectState([](SRoot& root)
		{
			root.m_curveThresholds.clear();
		}, "Character curve provenance is invalid");
		rejectState([](SRoot& root)
		{
			root.m_curveThresholds.push_back(999);
		}, "Character curve provenance is invalid");
		rejectState([](SRoot& root)
		{
			root.m_curveThresholds[1] += 1;
		}, "Character curve provenance is invalid");
		rejectState([](SRoot& root)
		{
			root.m_character->Experience = 500;
			root.m_character->Level = 3;
		}, "Character level/experience mismatch");
		rejectState([](SRoot& root)
		{
			root.m_battle.Reward.CurveMaxLevel = 5;
		}, "Character curve provenance is invalid");
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

		// 승리 한 번의 성장·잔액을 실제 codec 재개까지 관찰한다.
		auto rewarded = Fixture(store, *town, [](SRoot& root)
		{
			root.m_character->StatBonuses.Attack = 1000;
		});
		const auto previousExperience = rewarded->GetCharacter().Experience;
		const auto previousCurrency = rewarded->GetGold();
		while (rewarded->GetBattle().Outcome == 0) assert(FCombat::BasicAttack(store, *rewarded, error));
		assert(rewarded->GetBattle().Outcome == 1);
		assert(rewarded->GetCharacter().Experience > previousExperience);
		assert(rewarded->GetGold() > previousCurrency);
		assert(rewarded->Serialize(before, error));
		std::unique_ptr<InGameData> restarted;
		assert(InGameData::Deserialize(before, store, restarted, error));
		assert(restarted->Serialize(after, error) && after == before);
		assert(!FCombat::BasicAttack(store, *restarted, error));
		assert(restarted->Serialize(after, error) && after == before);
		std::cout << "PASS reward victory growth wallet and exact codec restart without replay\n";
		// 유효한 레벨 ID라도 적용된 원본과 모순되는 영수증은 복원하지 않는다.
		FSerializableTypeRegistry rewardRegistry;
		assert(GameData::InGameData::RegisterInGameDataTypes(rewardRegistry, error));
		SRoot inconsistent;
		assert(FSerializationArchive::Deserialize(before, inconsistent, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		inconsistent.m_battle.Reward.LevelAfter = 1;
		assert(FSerializationArchive::Serialize(inconsistent, after, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		const auto* retained = restarted.get();
		assert(!InGameData::Deserialize(after, store, restarted, error));
		assert(restarted.get() == retained && restarted->Serialize(after, error) && after == before);

		// 누적 문턱·다중 승급·원본 보너스·부족 HP를 같은 마지막 타격에서 검사한다.
		auto growth = Fixture(store, *town, [](SRoot& root)
		{
			root.m_character->Experience = 80;
			root.m_character->CurrentHp = 70;
			root.m_character->StatBonuses.Attack = 1000;
			root.m_battle.Reward.CurrencySeed = 71;
		});
		const auto* stableCharacter = &growth->GetCharacter();
		assert(FCombat::BasicAttack(store, *growth, error));
		assert(growth->GetBattle().Outcome == 1 && &growth->GetCharacter() == stableCharacter);
		assert(growth->GetCharacter().Experience == 380 && growth->GetCharacter().Level == 3);
		const auto& growthRound = growth->GetBattle();
		const auto incomingDamage = growthRound.FirstIsPlayer ? growthRound.SecondDamage : growthRound.FirstDamage;
		assert(growth->GetCharacter().CurrentHp == 110 - incomingDamage);
		assert(growth->GetCharacter().StatBonuses.Attack == 1000);
		assert(growthRound.Reward.LevelBefore == 1 && growthRound.Reward.LevelAfter == 3);
		const auto& growthCurrency = growthRound.Reward.Currencies.front();
		assert(growthCurrency.RolledAmount >= 10 && growthCurrency.RolledAmount <= 20);
		assert(growthCurrency.GrantedAmount == growthCurrency.RolledAmount && growthRound.Reward.CurrencyRandCount > 0);
		// 실제 다중 승급 영수증의 지급 XP만 변조해도 기존 루트·참조·바이트를 보존하며 거부한다.
		assert(growth->Serialize(before, error));
		// 두 공통 경계 반례를 각각 관측한 뒤 함께 판정한다.
		assert(FSerializationArchive::Deserialize(before, inconsistent, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		inconsistent.m_battle.Reward.GrantedExperience = 299;
		assert(FSerializationArchive::Serialize(inconsistent, after, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		std::unique_ptr<InGameData> shortReceipt;
		const bool shortAccepted = InGameData::Deserialize(after, store, shortReceipt, error);
		std::cerr << "REPAIR2 short receipt XP380 planned300 granted299 accepted=" << shortAccepted << " error=" << error << '\n';
		for (const auto granted : {0U, 381U, 299U})
		{
			assert(FSerializationArchive::Deserialize(before, inconsistent, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
			if (granted != 299) inconsistent.m_battle.Reward.PlannedExperience = 1000;
			inconsistent.m_battle.Reward.GrantedExperience = granted;
			assert(FSerializationArchive::Serialize(inconsistent, after, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
			const auto* prior = growth.get();
			const auto* character = &growth->GetCharacter();
			const bool accepted = InGameData::Deserialize(after, store, growth, error);
			std::cerr << "growth receipt granted=" << granted << " accepted=" << accepted << " error=" << error << '\n';
			assert(!accepted && error == "Reward experience receipt is invalid");
			assert(growth.get() == prior && &growth->GetCharacter() == character);
			assert(growth->Serialize(after, error) && after == before);
		}
		FNumericV4Root old4Growth;
		assert(FSerializationArchive::Deserialize(before, old4Growth.m_value, error,
			GameData::InGameData::kSchemaVersion, &rewardRegistry));
		assert(FSerializationArchive::Serialize(old4Growth, after, error, 4, &rewardRegistry));
		assert(InGameData::Deserialize(after, store, restarted, error));
		assert(restarted->GetBattle() == growth->GetBattle() && restarted->GetCharacter() == growth->GetCharacter());
		assert(restarted->Serialize(after, error) && after == before);
		old4Growth.m_value.m_battle.Reward.GrantedExperience = 299;
		assert(FSerializationArchive::Serialize(old4Growth, after, error, 4, &rewardRegistry));
		const auto* preservedOld4Root = restarted.get();
		const auto* preservedOld4Character = &restarted->GetCharacter();
		assert(!InGameData::Deserialize(after, store, restarted, error));
		assert(error == "Reward experience receipt is invalid");
		assert(restarted.get() == preservedOld4Root && &restarted->GetCharacter() == preservedOld4Character);
		assert(restarted->Serialize(after, error) && after == before);
		// 같은 구 writer에 schema5를 주장하면 새 필수 필드가 없어 반드시 거부한다.
		assert(FSerializationArchive::Serialize(old4Growth, after, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		assert(!InGameData::Deserialize(after, store, restarted, error) && error == "missing required field");
		assert(restarted.get() == preservedOld4Root && restarted->Serialize(after, error) && after == before);
		std::cout << "PASS independent old4 full receipt migrates; in-interval short grant and missing since5 reject preserving root\n";
		std::cout << "PASS settled growth receipt reconstructs prior XP and rejects zero grant level jump and underflow preserving root\n";
		for (const bool excess : {false, true})
		{
			auto cappedReward = Fixture(store, *town, [excess](SRoot& root)
			{
				root.m_character->Level = excess ? 4 : 3;
				root.m_character->Experience = excess ? UINT32_MAX : 430;
				root.m_character->StatBonuses.Attack = 1000;
				root.m_currencies.front().Amount = UINT32_MAX - 3;
				auto& entry = root.m_battle.Reward.Currencies.front();
				entry.AmountMin = entry.AmountMax = 5;
			});
			assert(FCombat::BasicAttack(store, *cappedReward, error));
			const auto& reward = cappedReward->GetBattle().Reward;
			assert(cappedReward->GetCharacter().Experience == (excess ? UINT32_MAX : 450));
			assert(cappedReward->GetCharacter().Level == 4 && reward.GrantedExperience == (excess ? 0U : 20U));
			assert(cappedReward->GetGold() == UINT32_MAX && reward.CurrencyRandCount == 0);
			assert(reward.Currencies.front().RolledAmount == 5 && reward.Currencies.front().GrantedAmount == 3);
			assert(cappedReward->Serialize(before, error) && InGameData::Deserialize(before, store, restarted, error));
			assert(restarted->Serialize(after, error) && before == after);
		}
		auto zero = Fixture(store, *town, [](SRoot& root)
		{
			root.m_character->StatBonuses.Attack = UINT32_MAX;
			root.m_battle.Reward.PlannedExperience = 0;
			auto& entry = root.m_battle.Reward.Currencies.front();
			entry.AmountMin = entry.AmountMax = 0;
		});
		assert(FCombat::BasicAttack(store, *zero, error));
		assert(zero->GetBattle().Reward.CurrencyRandCount == 0 && zero->GetCharacter().Experience == 0);
		assert(zero->GetGold() == previousCurrency);
		assert(zero->Serialize(before, error));
		assert(!zero->SetCurrency(static_cast<GameData::Common::ECurrencyType>(0), 17, error));
		assert(zero->Serialize(after, error) && before == after);
		assert(FSerializationArchive::Deserialize(before, inconsistent, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		inconsistent.m_currencies.push_back(inconsistent.m_currencies.front());
		assert(FSerializationArchive::Serialize(inconsistent, after, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		assert(!InGameData::Deserialize(after, store, restarted, error) && error == "Unknown or duplicate currency type");
		assert(FSerializationArchive::Deserialize(before, inconsistent, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		inconsistent.m_currencies.front().CurrencyType = UINT32_MAX;
		assert(FSerializationArchive::Serialize(inconsistent, after, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		assert(!InGameData::Deserialize(after, store, restarted, error) && error == "Unknown or duplicate currency type");
		std::cout << "PASS reward cumulative multi-upgrade HP deficit original bonuses "
			<< "fixed zero max excess wallet cap invalid preservation\n";

#ifdef _DEBUG
		// 마지막 타격의 복구 가능한 후보 payload 할당 실패는 HP/성장/두 RNG/영수증/참조를 모두 보존한다.
		int allocationFailures = 0;
		for (int allocation = 1; allocation <= 128; ++allocation)
		{
			auto failed = Fixture(store, *town, [](SRoot& root)
			{
				root.m_character->StatBonuses.Attack = 1000;
				root.m_battle.Reward.CurrencySeed = 71;
			});
			assert(failed->Serialize(before, error));
			const auto* characterAddress = &failed->GetCharacter();
			const auto* inventoryAddress = failed->GetInventory().data();
			g_rewardFault = {allocation, false};
			const auto previousHook = _CrtSetAllocHook(RewardAllocationHook);
			const bool applied = FCombat::BasicAttack(store, *failed, error);
			(void)_CrtSetAllocHook(previousHook);
			if (!g_rewardFault.m_hit)
			{
				assert(applied && failed->GetBattle().Outcome == 1);
				break;
			}
			assert(!applied && &failed->GetCharacter() == characterAddress && failed->GetInventory().data() == inventoryAddress);
			assert(failed->Serialize(after, error) && before == after);
			++allocationFailures;
			assert(FCombat::BasicAttack(store, *failed, error) && failed->GetBattle().Outcome == 1);
		}
		assert(allocationFailures > 0 && allocationFailures < 128);
		std::cout << "PASS reward final-hit allocation fault stages=" << allocationFailures
			<< " full candidate RNG bytes pointers preservation and retry\n";
#endif
		auto cappedStream = Fixture(store, *town, [](SRoot& root)
		{
			root.m_character->StatBonuses.Attack = 1000;
			root.m_character->Experience = 80;
			root.m_character->CurrentHp = 70;
			root.m_currencies.front().Amount = UINT32_MAX;
			root.m_battle.Reward.CurrencySeed = 71;
		});
		assert(FCombat::BasicAttack(store, *cappedStream, error));
		assert(cappedStream->GetBattle().RandCount == growth->GetBattle().RandCount);
		assert(cappedStream->GetBattle().Reward.CurrencyRandCount == growth->GetBattle().Reward.CurrencyRandCount);
		assert(cappedStream->GetBattle().Reward.Currencies.front().RolledAmount == growthCurrency.RolledAmount);
		assert(cappedStream->GetBattle().Reward.Currencies.front().GrantedAmount == 0);
		std::cout << "PASS currency cap consumes original distribution and independent combat stream unchanged\n";

		// 별도 생성 카탈로그에서만 두 번째 고정 ID를 켜서 같은 제품 지급 경계를 검증한다.
		const auto extensionType = static_cast<GameData::Common::ECurrencyType>(2);
		if (GameData::Common::IsValidCurrencyType(extensionType))
		{
			auto extension = Fixture(store, *town, [](SRoot& root)
			{
				root.m_character->StatBonuses.Attack = 1000;
				auto& entry = root.m_battle.Reward.Currencies.front();
				entry.CurrencyType = 2;
				entry.AmountMin = entry.AmountMax = 5;
			});
			assert(extension->SetCurrency(extensionType, 777, error));
			const auto originalGold = extension->GetGold();
			assert(FCombat::BasicAttack(store, *extension, error));
			assert(extension->GetCurrency(extensionType) == 782 && extension->GetGold() == originalGold);
			assert(extension->Serialize(before, error) && InGameData::Deserialize(before, store, restarted, error));
			assert(restarted->GetCurrency(extensionType) == 782 && restarted->GetGold() == originalGold);
			std::cout << "PASS generated second currency same payout wallet independent balances numeric save restart\n";
		}
		else
		{
			assert(!zero->SetCurrency(extensionType, 777, error));
			std::cout << "PASS production currency catalog activates Gold only and rejects future IDs\n";
		}

		FNumericV3Root oldBattle;
		oldBattle.m_contentVersion = store.GetDefaultNewGameProfile()->contentVersion;
		oldBattle.m_sceneId = town->GetSceneId();
		*oldBattle.m_character = town->GetCharacter();
		oldBattle.m_basicBonuses.Luck = 2;
		oldBattle.m_battle.m_ruleId = town->GetBattle().RuleId;
		oldBattle.m_battle.m_monsterId = town->GetBattle().MonsterId;
		oldBattle.m_battle.m_monsterHp = town->GetBattle().MonsterHp;
		oldBattle.m_battle.m_round = town->GetBattle().Round;
		oldBattle.m_battle.m_outcome = town->GetBattle().Outcome;
		oldBattle.m_battle.m_seed = town->GetBattle().Seed;
		oldBattle.m_battle.m_randCount = town->GetBattle().RandCount;
		oldBattle.m_battle.m_firstIsPlayer = town->GetBattle().FirstIsPlayer;
		oldBattle.m_battle.m_firstDamage = town->GetBattle().FirstDamage;
		oldBattle.m_battle.m_secondDamage = town->GetBattle().SecondDamage;
		oldBattle.m_battle.m_firstCritical = town->GetBattle().FirstCritical;
		oldBattle.m_battle.m_secondCritical = town->GetBattle().SecondCritical;
		oldBattle.m_battle.m_hitCount = town->GetBattle().HitCount;
		oldBattle.m_character->CurrentHp = 87;
		oldBattle.m_character->StatBonuses = {3, 4, 5};
		oldBattle.m_inventory.push_back({91, static_cast<std::uint32_t>(GameData::Common::EItemId::Token), 2});
		oldBattle.m_battle.m_seed = 1234567;
		oldBattle.m_battle.m_round = 1;
		oldBattle.m_battle.m_randCount = 3;
		oldBattle.m_battle.m_hitCount = 2;
		oldBattle.m_battle.m_monsterHp = 30;
		oldBattle.m_battle.m_firstDamage = 15;
		oldBattle.m_battle.m_secondDamage = 1;
		assert(FSerializationArchive::Serialize(oldBattle, before, error, 3, &rewardRegistry));
		assert(InGameData::Deserialize(before, store, restarted, error));
		assert(restarted->GetBattle().Reward.State == 3 && restarted->GetGold() == oldBattle.m_gold);
		assert(restarted->GetCharacter() == *oldBattle.m_character && restarted->GetInventory() == oldBattle.m_inventory);
		assert(restarted->GetBattle().Seed == 1234567 && restarted->GetBattle().RandCount == 3);
		assert(restarted->Serialize(after, error));
		SRoot migratedBattle;
		assert(FSerializationArchive::Deserialize(after, migratedBattle, error, GameData::InGameData::kSchemaVersion, &rewardRegistry));
		assert(migratedBattle.m_basicBonuses == oldBattle.m_basicBonuses);
		while (restarted->GetBattle().Outcome == 0) assert(FCombat::BasicAttack(store, *restarted, error));
		assert(restarted->GetGold() == oldBattle.m_gold && restarted->GetCharacter().Experience == oldBattle.m_character->Experience);
		assert(restarted->Serialize(before, error) && InGameData::Deserialize(before, store, restarted, error));
		assert(restarted->GetBattle().Reward.State == 3 && restarted->GetGold() == oldBattle.m_gold);
		assert(FCombat::Travel(store, *restarted, rule.townSceneId, error));
		assert(FCombat::Travel(store, *restarted, rule.battleSceneId, error) && restarted->GetBattle().Reward.State == 1);
		oldBattle.m_battle.m_outcome = 1;
		oldBattle.m_battle.m_monsterHp = 0;
		oldBattle.m_battle.m_hitCount = 1;
		oldBattle.m_battle.m_secondDamage = 0;
		assert(FSerializationArchive::Serialize(oldBattle, before, error, 3, &rewardRegistry));
		assert(InGameData::Deserialize(before, store, restarted, error));
		assert(restarted->GetBattle().Outcome == 1 && restarted->GetBattle().Reward.State == 3);
		assert(restarted->GetGold() == oldBattle.m_gold && restarted->GetCharacter().Experience == 0);
		std::cout << "PASS independent schema3 numeric battle writer preserves RNG excludes old rewards and new encounter prepares\n";

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
		// 과거 만렙에서 실제 정산한 영수증은 다음 레벨 추가 뒤에도 다시 지급하거나 재해석하지 않는다.
		std::ifstream levelInput(yaml / "LevelDefinition.yaml", std::ios::binary);
		const std::string currentLevels((std::istreambuf_iterator<char>(levelInput)), {});
		levelInput.close();
		const auto fourthLevel = currentLevels.find("\"LevelDefinition4\":\n");
		assert(fourthLevel != std::string::npos);
		std::ofstream(yaml / "LevelDefinition.yaml", std::ios::binary) << currentLevels.substr(0, fourthLevel);
		const auto historicalPackage = work / "historical-reward.dat";
		assert(GameData::BuildEncryptedPackage(yaml, historicalPackage, key, error, source / "Asset"));
		GameData::FGameDataStore historicalStore;
		assert(historicalStore.Load(historicalPackage, key, error));
		std::unique_ptr<InGameData> historicalTown;
		assert(InGameData::CreateNew(historicalStore, rule.townSceneId, historicalTown, error));
		assert(FCombat::Travel(historicalStore, *historicalTown, rule.battleSceneId, error));
		auto historicalReward = Fixture(historicalStore, *historicalTown, [](SRoot& root)
		{
			root.m_character->Level = 2;
			root.m_character->Experience = 230;
			root.m_character->CurrentHp = 7;
			root.m_character->StatBonuses.Attack = 1000;
		});
		assert(FCombat::BasicAttack(historicalStore, *historicalReward, error));
		assert(historicalReward->GetBattle().Outcome == 1);
		assert(historicalReward->GetCharacter().Level == 3 && historicalReward->GetCharacter().Experience == 250);
		assert(historicalReward->GetBattle().Reward.GrantedExperience == 20);
		const auto historicalHp = historicalReward->GetCharacter().CurrentHp;
		const auto historicalGold = historicalReward->GetGold();
		assert(historicalReward->Serialize(before, error));
		FNumericV4Root old4Partial;
		assert(FSerializationArchive::Deserialize(before, old4Partial.m_value, error,
			GameData::InGameData::kSchemaVersion, &rewardRegistry));
		std::vector<std::uint8_t> old4PartialBytes;
		assert(FSerializationArchive::Serialize(old4Partial, old4PartialBytes, error, 4, &rewardRegistry));
		assert(InGameData::Deserialize(old4PartialBytes, store, restarted, error));
		assert(restarted->GetBattle() == historicalReward->GetBattle());
		assert(restarted->GetCharacter() == historicalReward->GetCharacter() && restarted->GetGold() == historicalGold);
		std::ofstream(yaml / "LevelDefinition.yaml", std::ios::binary) << currentLevels;
		assert(InGameData::Deserialize(before, store, restarted, error));
		assert(restarted->GetCharacter().Level == 3 && restarted->GetCharacter().Experience == 250);
		assert(restarted->GetCharacter().CurrentHp == historicalHp && restarted->GetGold() == historicalGold);
		assert(restarted->GetBattle().Reward.GrantedExperience == 20);
		assert(restarted->Serialize(after, error));
		assert(InGameData::Deserialize(after, store, restarted, error));
		std::vector<std::uint8_t> stableHistorical;
		assert(restarted->Serialize(stableHistorical, error) && stableHistorical == after);
		auto historicalExcess = Fixture(historicalStore, *historicalTown, [](SRoot& root)
		{
			root.m_character->Level = 3;
			root.m_character->Experience = 500;
			root.m_character->CurrentHp = 70;
			root.m_character->StatBonuses.Attack = 1000;
		});
		assert(FCombat::BasicAttack(historicalStore, *historicalExcess, error));
		assert(historicalExcess->GetCharacter().Experience == 500 && historicalExcess->GetBattle().Reward.GrantedExperience == 0);
		assert(historicalExcess->Serialize(before, error));
		const bool excessAccepted = InGameData::Deserialize(before, store, restarted, error);
		std::cerr << "REPAIR2 historical XP500 cap250 -> cap450 accepted=" << excessAccepted << " error=" << error << '\n';
		assert(excessAccepted);
		assert(!shortAccepted);
		assert(restarted->GetCharacter().Level == 4 && restarted->GetCharacter().Experience == 500);
		for (const bool old4 : {false, true})
		{
			for (const auto state : {0U, 1U, 2U})
			{
				auto excess = Fixture(historicalStore, *historicalTown, [](SRoot& root)
				{
					root.m_character->Level = 3;
					root.m_character->Experience = 500;
					root.m_character->CurrentHp = 70;
					root.m_character->StatBonuses = {3, 1000, 5};
					root.m_basicBonuses.Luck = 2;
					root.m_currencies.front().Amount = 54321;
					root.m_inventory.push_back({91, static_cast<std::uint32_t>(GameData::Common::EItemId::Token), 2});
					root.m_achievements.push_back({static_cast<std::uint32_t>(GameData::Common::EAchievementId::Journey), 0, false});
				});
				if (state != 0) assert(FCombat::BasicAttack(historicalStore, *excess, error));
				if (state == 2)
				{
					assert(FCombat::Travel(historicalStore, *excess, rule.townSceneId, error));
					assert(excess->SetCurrentHp(historicalStore, 7, error));
				}
				const auto expectedHp = excess->GetCharacter().CurrentHp;
				const auto expectedGold = excess->GetGold();
				auto expectedBattle = excess->GetBattle();
				const auto expectedInventory = excess->GetInventory();
				assert(excess->Serialize(before, error));
				if (old4)
				{
					FNumericV4Root writer;
					assert(FSerializationArchive::Deserialize(before, writer.m_value, error,
						GameData::InGameData::kSchemaVersion, &rewardRegistry));
					assert(FSerializationArchive::Serialize(writer, before, error, 4, &rewardRegistry));
				}
				assert(InGameData::Deserialize(before, store, restarted, error));
				assert(restarted->GetCharacter().Level == 4 && restarted->GetCharacter().Experience == 500);
				assert(restarted->GetCharacter().CurrentHp == expectedHp && restarted->GetGold() == expectedGold);
				assert(restarted->GetInventory() == expectedInventory);
				assert(restarted->GetAchievements() == excess->GetAchievements());
				assert(restarted->GetCharacter().StatBonuses == excess->GetCharacter().StatBonuses);
				if (old4 && state == 1)
				{
					// 독립 구 writer에는 없던 최소 이력만 sound prefix 추론으로 추가한다.
					expectedBattle.Reward.ExperienceBefore = expectedBattle.Reward.ExperienceAfter = 500;
					expectedBattle.Reward.CurveMaxLevel = 3;
				}
				assert(restarted->GetBattle() == expectedBattle);
				for (int repeat = 0; repeat < 2; ++repeat)
				{
					assert(restarted->Serialize(before, error));
					assert(InGameData::Deserialize(before, store, restarted, error));
					assert(restarted->Serialize(after, error) && before == after);
					SRoot checked;
					assert(FSerializationArchive::Deserialize(after, checked, error,
						GameData::InGameData::kSchemaVersion, &rewardRegistry));
					assert(checked.m_basicBonuses.Luck == 2 && checked.m_achievements == excess->GetAchievements());
					assert(restarted->GetCharacter().CurrentHp == expectedHp && restarted->GetGold() == expectedGold);
					assert(restarted->GetBattle() == expectedBattle);
				}
				if (state == 0)
				{
					assert(FCombat::BasicAttack(store, *restarted, error));
					assert(restarted->GetBattle().Reward.GrantedExperience == 0 && restarted->GetCharacter().Experience == 500);
				}
				std::cout << "PASS curve crossing450 schema=" << (old4 ? 4 : 5) << " state=" << state
					<< " XP500 level4 HP wallet inventory receipt both RNG repeated load preserved\n";
			}
		}
		std::cout << "PASS historical capped receipt survives expanded level curve without replay or healing\n";
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
				auto expectedOwner = std::make_unique<FSelector>(seed);
				auto& expected = *expectedOwner;
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
				SCharacterStats afterStats;
				assert(combat->GetStats(variant, afterStats, error));
				const auto growthHp = outgoing ? afterStats.MaxHp - (100U + prepared->GetCharacter().StatBonuses.MaxHp) : 0;
				assert(combat->GetCharacter().CurrentHp ==
					(outgoing ? initialHp - (firstIsPlayer ? 0U : monsterDamage) + growthHp : 0U));
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
				assert(combat->GetCharacter().CurrentHp == afterStats.MaxHp);
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
			{"MonsterDefinition", "RewardExperience", 1000000U},
			{"MonsterDefinition", "RewardAmountMin", 1000000U},
			{"MonsterDefinition", "RewardAmountMax", 1000000U},
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
				if (std::string_view(field) == "RewardAmountMin")
					setNumber(data["MonsterDefinition"], "RewardAmountMax", maximum);
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
		for (const auto& [name, text] : originals)
			std::ofstream(yaml / (name + ".yaml"), std::ios::binary) << text;
		assert(GameData::BuildEncryptedPackage(yaml, cappedPackage, key, error, source / "Asset"));
		GameData::FGameDataStore rewardStore;
		assert(rewardStore.Load(cappedPackage, key, error));
		const auto* previousMonster = rewardStore.FindMonsterDefinition(rule.monsterId);
		auto invertedReward = originals.at("MonsterDefinition");
		setNumber(invertedReward, "RewardAmountMin", 21);
		std::ofstream(yaml / "MonsterDefinition.yaml", std::ios::binary) << invertedReward;
		assert(GameData::BuildEncryptedPackage(yaml, cappedPackage, key, error, source / "Asset"));
		assert(!rewardStore.Load(cappedPackage, key, error));
		assert(error == "Combat catalog cohort: RewardAmountMin must not exceed RewardAmountMax");
		assert(rewardStore.FindMonsterDefinition(rule.monsterId) == previousMonster && previousMonster->rewardAmountMin == 10);
		std::cout << "PASS reward inverted range authenticated Store exact diagnostic and original pointer preservation\n";
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
