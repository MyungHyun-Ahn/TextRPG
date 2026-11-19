#include "pch.h"
#include "Common/GameDataEnums.g.h"
#include "CharacterDefinition/CharacterDefinitionData.g.h"
#include "LevelDefinition/LevelDefinitionData.g.h"
#include "NewGameProfile/NewGameProfileData.g.h"
#include "ItemDefinition/ItemDefinitionData.g.h"
#include "AchievementDefinition/AchievementDefinitionData.g.h"
#include "GameData/FGameDataStore.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"
#include "CustomInGameData/FCharacterInGameData.h"
#include "InGameData.h"
#include "Combat/FCombat.h"


namespace
{
	constexpr std::size_t kMaxBytes = 1024 * 1024;
	constexpr std::size_t kMaxIdUnits = 256;
	constexpr std::size_t kMaxEntries = 4096;

	void CheckId(const std::wstring& value)
	{
		if (value.empty() || value.size() > kMaxIdUnits) throw std::runtime_error("ID length is invalid");
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			const auto unit = static_cast<std::uint16_t>(value[i]);
			if (unit == 0) throw std::runtime_error("ID contains NUL");
			if (unit >= 0xD800 && unit <= 0xDBFF)
			{
				if (++i == value.size() || value[i] < 0xDC00 || value[i] > 0xDFFF)
					throw std::runtime_error("ID contains invalid UTF-16");
			}
			else if (unit >= 0xDC00 && unit <= 0xDFFF) throw std::runtime_error("ID contains invalid UTF-16");
		}
	}
	void CheckBounds(std::uint32_t, const TextRPG::SCharacterProgress&,
		const std::vector<TextRPG::SInventoryItemProgress>& inventory, const std::vector<TextRPG::SAchievementProgress>& achievements)
	{
		if (inventory.size() > kMaxEntries || achievements.size() > kMaxEntries)
			throw std::runtime_error("Progress collection count exceeds limit");
	}
	// 과거 콘텐츠 이름은 이 읽기 전용 경계에서만 해석한다. 새 저장에는 숫자만 쓴다.
	std::uint32_t LegacySceneId(const std::wstring& value)
	{
		CheckId(value);
		if (value == L"MainMenu") return static_cast<std::uint32_t>(GameData::Common::ESceneId::MainMenu);
		if (value == L"Town") return static_cast<std::uint32_t>(GameData::Common::ESceneId::Town);
		if (value == L"BattleField") return static_cast<std::uint32_t>(GameData::Common::ESceneId::BattleField);
		throw std::runtime_error("SceneId definition is missing");
	}
	std::uint32_t LegacyCharacterId(const std::wstring& value)
	{
		CheckId(value);
		if (value == L"Swordswoman") return static_cast<std::uint32_t>(GameData::Common::ECharacterId::Swordswoman);
		throw std::runtime_error("CharacterId or Level definition is missing");
	}
	std::uint32_t LegacyItemId(const std::wstring& value)
	{
		CheckId(value);
		if (value == L"Token") return static_cast<std::uint32_t>(GameData::Common::EItemId::Token);
		throw std::runtime_error("ItemId definition is missing");
	}
	std::uint32_t LegacyAchievementId(const std::wstring& value)
	{
		CheckId(value);
		if (value == L"Journey") return static_cast<std::uint32_t>(GameData::Common::EAchievementId::Journey);
		throw std::runtime_error("AchievementId definition is missing");
	}
	// 프로토콜 정수는 모두 little-endian이며 wchar_t/객체 메모리를 덤프하지 않는다.
	constexpr std::uint32_t kMagic = 0x31444749; // IGD1
	constexpr std::uint32_t kFormatVersion = 1;
	class FReader final
	{
	public:
		explicit FReader(const std::vector<std::uint8_t>& bytes) : m_bytes(bytes) {}
		std::uint64_t Number(std::size_t width)
		{
			if (width > m_bytes.size() - m_position) throw std::runtime_error("Truncated progress payload");
			std::uint64_t value = 0;
			for (std::size_t i = 0; i < width; ++i) value |= static_cast<std::uint64_t>(m_bytes[m_position++]) << (i * 8);
			return value;
		}
		std::uint32_t UInt32() { return static_cast<std::uint32_t>(Number(4)); }
		std::wstring Id()
		{
			const auto count = UInt32();
			if (count == 0 || count > kMaxIdUnits || count > (m_bytes.size() - m_position) / 2)
				throw std::runtime_error("ID length is invalid or truncated");
			std::wstring value;
			value.reserve(count);
			for (std::uint32_t i = 0; i < count; ++i) value += static_cast<wchar_t>(Number(2));
			CheckId(value);
			return value;
		}
		std::uint32_t Count(std::size_t minimumBytes)
		{
			const auto count = UInt32();
			if (count > kMaxEntries || count > (m_bytes.size() - m_position) / minimumBytes)
				throw std::runtime_error("Progress collection count is invalid or truncated");
			return count;
		}
		bool Boolean()
		{
			const auto value = Number(1);
			if (value > 1) throw std::runtime_error("Progress boolean is not canonical");
			return value == 1;
		}
		bool IsDone() const noexcept { return m_position == m_bytes.size(); }
	private:
		const std::vector<std::uint8_t>& m_bytes;
		std::size_t m_position{};
	};
	TextRPG::SCharacterStats ReadStats(FReader& reader)
	{
		TextRPG::SCharacterStats result;
		result.MaxHp = reader.UInt32();
		result.Attack = reader.UInt32();
		result.Defense = reader.UInt32();
		return result;
	}
	TextRPG::SCharacterProgress ReadCharacter(FReader& reader)
	{
		// 순차 wire 읽기를 생성 타입의 생성자 인수 평가 순서에 맡기지 않는다.
		TextRPG::SCharacterProgress result;
		result.CharacterId = LegacyCharacterId(reader.Id());
		result.Level = reader.UInt32();
		result.Experience = reader.UInt32();
		result.CurrentHp = reader.UInt32();
		result.StatBonuses = ReadStats(reader);
		return result;
	}
	TextRPG::SInventoryItemProgress ReadItem(FReader& reader)
	{
		TextRPG::SInventoryItemProgress result;
		result.InstanceId = reader.Number(8);
		result.ItemId = LegacyItemId(reader.Id());
		result.Count = reader.UInt32();
		return result;
	}
	TextRPG::SAchievementProgress ReadAchievement(FReader& reader)
	{
		TextRPG::SAchievementProgress result;
		result.AchievementId = LegacyAchievementId(reader.Id());
		result.Progress = reader.UInt32();
		result.IsCompleted = reader.Boolean();
		return result;
	}
	// schema1 문자열 signature는 콘텐츠 전용 reader로 읽고 Core의 타입 계약은 그대로 둔다.
	class FLegacyReadOnly : public TextRPG::ISerializable
	{
	protected:
		bool SerializeFields(TextRPG::FBinaryWriter&, std::string& error) const override
		{
			error = "Legacy progress is read only";
			return false;
		}
	};
	struct SLegacyCharacter final : public FLegacyReadOnly
	{
		static constexpr std::string_view kTypeName = "SCharacterProgress";
		std::wstring CharacterId;
		std::uint32_t Level{}, Experience{}, CurrentHp{};
		TextRPG::SCharacterStats StatBonuses;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool DeserializeFields(TextRPG::FBinaryReader& reader, std::string& error) override
		{
			return reader.Field("CharacterId", CharacterId, error) && reader.Field("Level", Level, error) &&
				reader.Field("Experience", Experience, error) && reader.Field("CurrentHp", CurrentHp, error) &&
				reader.Field("StatBonuses", StatBonuses, error);
		}
	};
	struct SLegacyItem final : public FLegacyReadOnly
	{
		static constexpr std::string_view kTypeName = "SInventoryItemProgress";
		std::uint64_t InstanceId{};
		std::wstring ItemId;
		std::uint32_t Count{};
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool DeserializeFields(TextRPG::FBinaryReader& reader, std::string& error) override
		{
			return reader.Field("InstanceId", InstanceId, error) && reader.Field("ItemId", ItemId, error) &&
				reader.Field("Count", Count, error);
		}
	};
	struct SLegacyAchievement final : public FLegacyReadOnly
	{
		static constexpr std::string_view kTypeName = "SAchievementProgress";
		std::wstring AchievementId;
		std::uint32_t Progress{};
		bool IsCompleted{};
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool DeserializeFields(TextRPG::FBinaryReader& reader, std::string& error) override
		{
			return reader.Field("AchievementId", AchievementId, error) && reader.Field("Progress", Progress, error) &&
				reader.Field("IsCompleted", IsCompleted, error);
		}
	};
	struct SLegacyRoot final : public FLegacyReadOnly
	{
		static constexpr std::string_view kTypeName = "SRootInGameData";
		std::uint32_t ContentVersion{}, Gold{};
		std::wstring SceneId;
		std::shared_ptr<SLegacyCharacter> Character;
		std::vector<SLegacyItem> Inventory;
		std::vector<SLegacyAchievement> Achievements;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool DeserializeFields(TextRPG::FBinaryReader& reader, std::string& error) override
		{
			return reader.Field("m_contentVersion", ContentVersion, error) && reader.Field("m_sceneId", SceneId, error) &&
				reader.Field("m_character", Character, error) && reader.Field("m_gold", Gold, error) &&
				reader.Field("m_inventory", Inventory, error) && reader.Field("m_achievements", Achievements, error);
		}
	};
	void CheckInventory(const GameData::FGameDataStore& store, const std::vector<TextRPG::SInventoryItemProgress>& inventory)
	{
		std::set<std::uint64_t> instances;
		for (const auto& item : inventory)
		{
			const auto* definition = store.FindItemDefinition(item.ItemId);
			if (!definition) throw std::runtime_error("ItemId definition is missing");
			if (item.InstanceId == 0 || !instances.insert(item.InstanceId).second)
				throw std::runtime_error("Inventory has zero or duplicate InstanceId");
			if (item.Count == 0 || item.Count > definition->maxStack) throw std::runtime_error("Inventory Count is invalid");
		}
	}
	void CheckAchievements(const GameData::FGameDataStore& store, const std::vector<TextRPG::SAchievementProgress>& achievements)
	{
		std::set<std::uint32_t> ids;
		for (const auto& achievement : achievements)
		{
			const auto* definition = store.FindAchievementDefinition(achievement.AchievementId);
			if (!definition) throw std::runtime_error("AchievementId definition is missing");
			if (!ids.insert(achievement.AchievementId).second) throw std::runtime_error("Duplicate AchievementId");
			if (achievement.Progress > definition->targetCount ||
				achievement.IsCompleted != (achievement.Progress == definition->targetCount))
				throw std::runtime_error("Achievement completion/progress is inconsistent");
		}
	}
}

namespace TextRPG
{
	InGameData::InGameData()
	{
		m_data.m_character = std::make_shared<FCharacterInGameData>();
	}

	std::uint32_t InGameData::ExperienceGrant(std::uint32_t before, std::uint32_t planned, std::uint32_t cap) noexcept
	{
		return (std::min)(planned, before >= cap ? 0U : cap - before);
	}

	std::uint32_t InGameData::ExperienceLevel(const GameData::FGameDataStore& store,
		std::uint32_t experience, std::uint32_t maximum) noexcept
	{
		std::uint32_t result = 0;
		for (const auto& level : store.GetLevelDefinitions())
			if (level.level <= maximum && experience >= level.requiredExperience) result = level.level;
		return result;
	}

	void InGameData::RestoreCurve(const GameData::FGameDataStore& store, bool legacy)
	{
		const auto& levels = store.GetLevelDefinitions();
		// 레거시 카탈로그 부재의 오류 우선순위는 기존 전체 후보 검증에 맡긴다.
		if (levels.empty()) return;
		auto& character = *m_data.m_character;
		if (legacy)
		{
			// 구형은 저장 곡선이 없다. 현재 구간 또는 저장 레벨에서 끝나는 유효 prefix만 추론한다.
			const auto* level = store.FindLevelDefinition(character.Level);
			if (!level) throw std::runtime_error("CharacterId or Level definition is missing");
			if (character.Experience < level->requiredExperience ||
				(character.Level == 1 && ExperienceLevel(store, character.Experience, levels.back().level) != 1))
				throw std::runtime_error("Character level/experience mismatch");
			auto& receipt = m_data.m_battle.Reward;
			if (receipt.State == 2)
			{
				if (receipt.GrantedExperience > character.Experience)
					throw std::runtime_error("Reward experience receipt is invalid");
				receipt.ExperienceBefore = character.Experience - receipt.GrantedExperience;
				receipt.ExperienceAfter = character.Experience;
				receipt.CurveMaxLevel = 0;
				for (const auto& cap : levels)
				{
					const auto grant = m_data.m_battle.Outcome == 1 ?
						ExperienceGrant(receipt.ExperienceBefore, receipt.PlannedExperience, cap.requiredExperience) : 0U;
					if (grant == receipt.GrantedExperience &&
						ExperienceLevel(store, receipt.ExperienceBefore, cap.level) == receipt.LevelBefore &&
						ExperienceLevel(store, receipt.ExperienceAfter, cap.level) == receipt.LevelAfter &&
						receipt.LevelAfter == character.Level)
					{
						receipt.CurveMaxLevel = cap.level;
						break;
					}
				}
				if (receipt.CurveMaxLevel == 0) throw std::runtime_error("Reward experience receipt is invalid");
			}
		}
		else
		{
			// 이 벡터는 호환 증거일 뿐이다. 게임 수치·상한은 항상 Store에서만 조회한다.
			if (m_data.m_curveThresholds.empty() || m_data.m_curveThresholds.size() > levels.size())
				throw std::runtime_error("Character curve provenance is invalid");
			for (std::size_t i = 0; i < m_data.m_curveThresholds.size(); ++i)
				if (m_data.m_curveThresholds[i] != levels[i].requiredExperience)
					throw std::runtime_error("Character curve provenance is invalid");
			const auto savedMaximum = static_cast<std::uint32_t>(m_data.m_curveThresholds.size());
			if (ExperienceLevel(store, character.Experience, savedMaximum) != character.Level)
				throw std::runtime_error("Character level/experience mismatch");
			if (m_data.m_battle.Reward.CurveMaxLevel > m_data.m_curveThresholds.size())
				throw std::runtime_error("Character curve provenance is invalid");
		}
		character.Level = ExperienceLevel(store, character.Experience, levels.back().level);
		m_data.m_curveThresholds.clear();
		for (const auto& level : levels) m_data.m_curveThresholds.push_back(level.requiredExperience);
	}

	std::string_view InGameData::GetSerializableTypeName() const noexcept
	{
		return GameData::InGameData::SRootInGameData::kTypeName;
	}

	bool InGameData::SerializeFields(FBinaryWriter& writer, std::string& error) const
	{
		return ForwardSerializeFields(m_data, writer, error);
	}

	bool InGameData::DeserializeFields(FBinaryReader& reader, std::string& error)
	{
		if (reader.GetSchemaVersion() >= 4) return ForwardDeserializeFields(m_data, reader, error);
		// 숫자 schema2/3의 당시 필드를 명시적으로 읽는다. 새 writer에는 구 Gold 원본이 없다.
		std::uint32_t gold = 0;
		if (!reader.Field("m_contentVersion", m_data.m_contentVersion, error) ||
			!reader.Field("m_sceneId", m_data.m_sceneId, error) || !reader.Field("m_character", m_data.m_character, error) ||
			!reader.Field("m_gold", gold, error) || !reader.Field("m_inventory", m_data.m_inventory, error) ||
			!reader.Field("m_achievements", m_data.m_achievements, error)) return false;
		if (reader.GetSchemaVersion() >= 3 &&
			(!reader.Field("m_basicBonuses", m_data.m_basicBonuses, error) ||
			 !reader.Field("m_battle", m_data.m_battle, error))) return false;
		m_data.m_currencies = {{static_cast<std::uint32_t>(GameData::Common::ECurrencyType::Gold), gold}};
		if (m_data.m_battle.RuleId != 0) m_data.m_battle.Reward.State = 3;
		return true;
	}
	bool InGameData::CreateNew(const GameData::FGameDataStore& store, std::uint32_t sceneId,
		std::unique_ptr<InGameData>& output, std::string& error)
	{
		try
		{
			const auto* profile = store.GetDefaultNewGameProfile();
			if (!profile) throw std::runtime_error("New-game catalog is missing");
			auto candidate = std::unique_ptr<InGameData>(new InGameData());
			candidate->m_data.m_contentVersion = profile->contentVersion;
			candidate->m_data.m_sceneId = sceneId;
			*candidate->m_data.m_character = {profile->characterId, profile->initialLevel, profile->initialExperience, 0,
				{profile->initialMaxHpBonus, profile->initialAttackBonus, profile->initialDefenseBonus}};
			candidate->m_data.m_currencies = {{static_cast<std::uint32_t>(GameData::Common::ECurrencyType::Gold), profile->initialGold}};
			SCharacterStats stats;
			if (!candidate->GetStats(store, stats, error)) return false;
			candidate->m_data.m_character->CurrentHp = stats.MaxHp;
			for (const auto& level : store.GetLevelDefinitions())
				candidate->m_data.m_curveThresholds.push_back(level.requiredExperience);
			if (!candidate->Validate(store, error)) return false;
			output = std::move(candidate);
			error.clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}
	const SCharacterProgress& InGameData::GetCharacter() const noexcept
	{
		// 게시된 루트에는 필수 캐릭터가 있다. 불변식 위반을 대체 데이터로 숨기지 않는다.
		if (!m_data.m_character) std::terminate();
		return *m_data.m_character;
	}
	const GameData::InGameData::SBattleProgress& InGameData::GetBattle() const noexcept
	{
		return m_data.m_battle;
	}
	std::uint32_t InGameData::GetGold() const noexcept { return GetCurrency(GameData::Common::ECurrencyType::Gold); }
	std::uint32_t InGameData::GetCurrency(GameData::Common::ECurrencyType type) const noexcept
	{
		for (const auto& balance : m_data.m_currencies) if (balance.CurrencyType == static_cast<std::uint32_t>(type)) return balance.Amount;
		return 0;
	}
	std::uint32_t InGameData::GetSceneId() const noexcept { return m_data.m_sceneId; }
	const std::vector<SInventoryItemProgress>& InGameData::GetInventory() const noexcept { return m_data.m_inventory; }
	const std::vector<SAchievementProgress>& InGameData::GetAchievements() const noexcept { return m_data.m_achievements; }
	bool InGameData::GetStats(const GameData::FGameDataStore& store, SCharacterStats& output, std::string& error) const
	{
		if (!m_data.m_character) { error = "Character progress is missing"; return false; }
		const auto* level = store.FindLevelDefinition(m_data.m_character->Level);
		if (!level || !store.FindCharacterDefinition(m_data.m_character->CharacterId))
		{
			error = "CharacterId or Level definition is missing";
			return false;
		}
		constexpr auto maximum = (std::numeric_limits<std::uint32_t>::max)();
		const auto& bonus = m_data.m_character->StatBonuses;
		const auto project = [maximum](std::uint32_t base, std::uint32_t increase)
		{
			return static_cast<std::uint32_t>((std::min)(std::uint64_t{maximum}, static_cast<std::uint64_t>(base) + increase));
		};
		output = {project(level->maxHp, bonus.MaxHp), project(level->attack, bonus.Attack), project(level->defense, bonus.Defense)};
		error.clear();
		return true;
	}
	bool InGameData::Serialize(std::vector<std::uint8_t>& output, std::string& error) const
	{
		try
		{
			if (!m_data.m_character) throw std::runtime_error("Character progress is missing");
			CheckBounds(m_data.m_sceneId, *m_data.m_character, m_data.m_inventory, m_data.m_achievements);
			FSerializableTypeRegistry registry;
			if (!GameData::InGameData::RegisterInGameDataTypes(registry, error) ||
				!RegisterCharacterInGameData(registry, error)) return false;
			return FSerializationArchive::Serialize(*this, output, error, GameData::InGameData::kSchemaVersion, &registry);
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
	}
	bool InGameData::Deserialize(const std::vector<std::uint8_t>& bytes, const GameData::FGameDataStore& store,
		std::unique_ptr<InGameData>& output, std::string& error)
	{
		try
		{
			if (bytes.size() > kMaxBytes) throw std::runtime_error("Total progress bytes exceed limit");
			const bool isFramed = bytes.size() >= 4 && bytes[0] == 'C' && bytes[1] == 'G' && bytes[2] == 'S' && bytes[3] == 'A';
			std::unique_ptr<InGameData> candidate;
			const bool isLegacyFramed = isFramed && bytes.size() >= 12 &&
				bytes[8] == 1 && bytes[9] == 0 && bytes[10] == 0 && bytes[11] == 0;
			if (isLegacyFramed)
			{
				SLegacyRoot legacyRoot;
				if (!FSerializationArchive::Deserialize(bytes, legacyRoot, error, 1)) return false;
				if (!legacyRoot.Character) throw std::runtime_error("Character progress is missing");
				candidate.reset(new InGameData());
				candidate->m_data.m_contentVersion = legacyRoot.ContentVersion;
				candidate->m_data.m_sceneId = LegacySceneId(legacyRoot.SceneId);
				const auto& character = *legacyRoot.Character;
				*candidate->m_data.m_character = {LegacyCharacterId(character.CharacterId), character.Level,
					character.Experience, character.CurrentHp, character.StatBonuses};
				candidate->m_data.m_currencies = {{static_cast<std::uint32_t>(GameData::Common::ECurrencyType::Gold), legacyRoot.Gold}};
				candidate->m_data.m_inventory.reserve(legacyRoot.Inventory.size());
				for (const auto& item : legacyRoot.Inventory)
					candidate->m_data.m_inventory.push_back({item.InstanceId, LegacyItemId(item.ItemId), item.Count});
				candidate->m_data.m_achievements.reserve(legacyRoot.Achievements.size());
				for (const auto& award : legacyRoot.Achievements)
					candidate->m_data.m_achievements.push_back({LegacyAchievementId(award.AchievementId),
						award.Progress, award.IsCompleted});
			}
			else if (isFramed)
			{
				candidate.reset(new InGameData());
				FSerializableTypeRegistry registry;
				if (!GameData::InGameData::RegisterInGameDataTypes(registry, error) ||
					!RegisterCharacterInGameData(registry, error)) return false;
				if (!FSerializationArchive::Deserialize(bytes, *candidate, error,
					GameData::InGameData::kSchemaVersion, &registry)) return false;
			}
			else
			{
				// IGD1은 새 후보로만 이관한다. Continue는 슬롯을 다시 쓰지 않는다.
				FReader reader(bytes);
				if (reader.UInt32() != kMagic) throw std::runtime_error("Invalid progress magic");
				if (reader.UInt32() != kFormatVersion) throw std::runtime_error("Unsupported progress format version");
				candidate.reset(new InGameData());
				candidate->m_data.m_contentVersion = reader.UInt32();
				candidate->m_data.m_sceneId = LegacySceneId(reader.Id());
				*candidate->m_data.m_character = ReadCharacter(reader);
				candidate->m_data.m_currencies = {{static_cast<std::uint32_t>(GameData::Common::ECurrencyType::Gold), reader.UInt32()}};
				const auto inventoryCount = reader.Count(18);
				candidate->m_data.m_inventory.reserve(inventoryCount);
				for (std::uint32_t i = 0; i < inventoryCount; ++i) candidate->m_data.m_inventory.push_back(ReadItem(reader));
				const auto achievementCount = reader.Count(11);
				candidate->m_data.m_achievements.reserve(achievementCount);
				for (std::uint32_t i = 0; i < achievementCount; ++i)
					candidate->m_data.m_achievements.push_back(ReadAchievement(reader));
				if (!reader.IsDone()) throw std::runtime_error("Trailing progress bytes");
			}
			// 구형 nullable pointer도 후보 검증 오류다. 마이그레이션 조회가 종료 경계를 우회하지 않는다.
			if (!candidate->m_data.m_character) throw std::runtime_error("Character progress is missing");
			candidate->RestoreCurve(store, !isFramed || (bytes.size() >= 12 && bytes[8] < 5));
			std::sort(candidate->m_data.m_currencies.begin(), candidate->m_data.m_currencies.end(), [](const auto& left, const auto& right)
			{
				return left.CurrencyType < right.CurrencyType;
			});
			if (!candidate->Validate(store, error)) return false;
			output = std::move(candidate);
			error.clear();
			return true;
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
	}
	bool InGameData::SetCurrentHp(const GameData::FGameDataStore& store, std::uint32_t hp, std::string& error)
	{
		if (!Validate(store, error)) return false;
		SCharacterStats stats;
		if (!GetStats(store, stats, error)) return false;
		if (hp > stats.MaxHp) { error = "CurrentHp exceeds derived MaxHp"; return false; }
		if (m_data.m_battle.RuleId != 0 && hp != m_data.m_character->CurrentHp)
		{
			error = "Active combat HP changes require a combat action";
			return false;
		}
		m_data.m_character->CurrentHp = hp;
		error.clear();
		return true;
	}
	bool InGameData::SetGold(std::uint32_t gold, std::string& error)
	{
		return SetCurrency(GameData::Common::ECurrencyType::Gold, gold, error);
	}
	bool InGameData::SetCurrency(GameData::Common::ECurrencyType type, std::uint32_t amount, std::string& error)
	{
		try
		{
			if (!GameData::Common::IsValidCurrencyType(type))
				throw std::runtime_error("Unknown currency type");
			auto candidate = m_data.m_currencies;
			const auto found = std::find_if(candidate.begin(), candidate.end(), [type](const auto& row)
			{
				return row.CurrencyType == static_cast<std::uint32_t>(type);
			});
			if (found == candidate.end()) candidate.push_back({static_cast<std::uint32_t>(type), amount});
			else found->Amount = amount;
			std::sort(candidate.begin(), candidate.end(), [](const auto& left, const auto& right)
			{
				return left.CurrencyType < right.CurrencyType;
			});
			m_data.m_currencies.swap(candidate);
			error.clear();
			return true;
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
	}
	bool InGameData::SetSceneId(const GameData::FGameDataStore& store, std::uint32_t sceneId, std::string& error)
	{
		try
		{
			if (!Validate(store, error)) return false;
			CheckBounds(sceneId, *m_data.m_character, m_data.m_inventory, m_data.m_achievements);
			if (!store.FindScene(sceneId)) throw std::runtime_error("SceneId definition is missing");
			if (m_data.m_battle.RuleId != 0 && sceneId != m_data.m_sceneId)
				throw std::runtime_error("Active combat scene changes require travel");
			m_data.m_sceneId = sceneId;
			error.clear();
			return true;
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
	}
	bool InGameData::SetInventory(const GameData::FGameDataStore& store,
		const std::vector<SInventoryItemProgress>& inventory, std::string& error)
	{
		try
		{
			if (!Validate(store, error)) return false;
			CheckBounds(m_data.m_sceneId, *m_data.m_character, inventory, m_data.m_achievements);
			CheckInventory(store, inventory);
			auto candidate = inventory;
			m_data.m_inventory.swap(candidate);
			error.clear();
			return true;
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
	}
	bool InGameData::SetAchievements(const GameData::FGameDataStore& store,
		const std::vector<SAchievementProgress>& achievements, std::string& error)
	{
		try
		{
			if (!Validate(store, error)) return false;
			CheckBounds(m_data.m_sceneId, *m_data.m_character, m_data.m_inventory, achievements);
			CheckAchievements(store, achievements);
			auto candidate = achievements;
			m_data.m_achievements.swap(candidate);
			error.clear();
			return true;
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
	}
	bool InGameData::Validate(const GameData::FGameDataStore& store, std::string& error) const
	{
		try
		{
			if (!m_data.m_character) throw std::runtime_error("Character progress is missing");
			CheckBounds(m_data.m_sceneId, *m_data.m_character, m_data.m_inventory, m_data.m_achievements);
			// 콘텐츠 상한이 아니라 공통 vector codec의 저장 가능 용량을 게시 전에 확인한다.
			if (m_data.m_curveThresholds.size() > SerializationDetail::kMaxCount)
				throw std::runtime_error("Character curve provenance exceeds serialization capacity");
			std::set<std::uint32_t> currencies;
			for (const auto& balance : m_data.m_currencies)
			{
				if (!GameData::Common::IsValidCurrencyType(static_cast<GameData::Common::ECurrencyType>(balance.CurrencyType)) ||
					!currencies.insert(balance.CurrencyType).second) throw std::runtime_error("Unknown or duplicate currency type");
			}
			CheckInventory(store, m_data.m_inventory);
			CheckAchievements(store, m_data.m_achievements);
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
		const auto* profile = store.GetDefaultNewGameProfile();
		if (!profile || m_data.m_contentVersion != profile->contentVersion)
		{
			error = "Unsupported content version or missing catalog";
			return false;
		}
		if (!store.FindScene(m_data.m_sceneId))
		{
			error = "SceneId definition is missing";
			return false;
		}
		SCharacterStats stats;
		if (!GetStats(store, stats, error)) return false;
		const auto* level = store.FindLevelDefinition(m_data.m_character->Level);
		if (!level || m_data.m_character->Experience < level->requiredExperience ||
			std::any_of(store.GetLevelDefinitions().begin(), store.GetLevelDefinitions().end(), [&](const auto& next)
			{ return next.level > m_data.m_character->Level && m_data.m_character->Experience >= next.requiredExperience; }))
		{
			error = "Character level/experience mismatch";
			return false;
		}
		if (!FCombat::Validate(store, *this, error)) return false;
		if (m_data.m_character->CurrentHp > stats.MaxHp)
		{
			error = "CurrentHp exceeds derived MaxHp";
			return false;
		}
		error.clear();
		return true;
	}
}
