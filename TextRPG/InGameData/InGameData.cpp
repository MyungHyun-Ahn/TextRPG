#include "GameData/pch.h"
#include "CharacterDefinition/CharacterDefinitionData.g.h"
#include "LevelDefinition/LevelDefinitionData.g.h"
#include "NewGameProfile/NewGameProfileData.g.h"
#include "ItemDefinition/ItemDefinitionData.g.h"
#include "AchievementDefinition/AchievementDefinitionData.g.h"
#include "GameData/FGameDataStore.h"
#include "InGameData.h"


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
	void CheckBounds(const std::wstring& sceneId, const TextRPG::SCharacterProgress& character,
		const std::vector<TextRPG::SInventoryItemProgress>& inventory, const std::vector<TextRPG::SAchievementProgress>& achievements)
	{
		CheckId(sceneId);
		CheckId(character.CharacterId);
		if (inventory.size() > kMaxEntries || achievements.size() > kMaxEntries)
			throw std::runtime_error("Progress collection count exceeds limit");
		std::size_t bytes = 56 + 2 * (sceneId.size() + character.CharacterId.size());
		for (const auto& item : inventory)
		{
			CheckId(item.ItemId);
			bytes += 16 + 2 * item.ItemId.size();
		}
		for (const auto& achievement : achievements)
		{
			CheckId(achievement.AchievementId);
			bytes += 9 + 2 * achievement.AchievementId.size();
		}
		if (bytes > kMaxBytes) throw std::runtime_error("Total progress bytes exceed limit");
	}
	// 프로토콜 정수는 모두 little-endian이며 wchar_t/객체 메모리를 덤프하지 않는다.
	constexpr std::uint32_t kMagic = 0x31444749; // IGD1
	constexpr std::uint32_t kFormatVersion = 1;
	class FWriter final
	{
	public:
		void Number(std::uint64_t value, std::size_t width)
		{
			if (width > kMaxBytes - m_bytes.size()) throw std::runtime_error("Total progress bytes exceed limit");
			for (std::size_t i = 0; i < width; ++i) m_bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
		}
		void Id(const std::wstring& value)
		{
			CheckId(value);
			Number(value.size(), 4);
			for (const auto unit : value) Number(static_cast<std::uint16_t>(unit), 2);
		}
		std::vector<std::uint8_t> Take() { return std::move(m_bytes); }
	private:
		std::vector<std::uint8_t> m_bytes;
	};
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
	void WriteStats(FWriter& writer, const TextRPG::SCharacterStats& value)
	{
		writer.Number(value.MaxHp, 4);
		writer.Number(value.Attack, 4);
		writer.Number(value.Defense, 4);
	}
	TextRPG::SCharacterStats ReadStats(FReader& reader)
	{
		return {reader.UInt32(), reader.UInt32(), reader.UInt32()};
	}
	void WriteCharacter(FWriter& writer, const TextRPG::SCharacterProgress& value)
	{
		writer.Id(value.CharacterId);
		writer.Number(value.Level, 4);
		writer.Number(value.Experience, 4);
		writer.Number(value.CurrentHp, 4);
		WriteStats(writer, value.StatBonuses);
	}
	TextRPG::SCharacterProgress ReadCharacter(FReader& reader)
	{
		return {reader.Id(), reader.UInt32(), reader.UInt32(), reader.UInt32(), ReadStats(reader)};
	}
	void WriteItem(FWriter& writer, const TextRPG::SInventoryItemProgress& value)
	{
		writer.Number(value.InstanceId, 8);
		writer.Id(value.ItemId);
		writer.Number(value.Count, 4);
	}
	TextRPG::SInventoryItemProgress ReadItem(FReader& reader)
	{
		return {reader.Number(8), reader.Id(), reader.UInt32()};
	}
	void WriteAchievement(FWriter& writer, const TextRPG::SAchievementProgress& value)
	{
		writer.Id(value.AchievementId);
		writer.Number(value.Progress, 4);
		writer.Number(value.IsCompleted ? 1 : 0, 1);
	}
	TextRPG::SAchievementProgress ReadAchievement(FReader& reader)
	{
		return {reader.Id(), reader.UInt32(), reader.Boolean()};
	}
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
		std::set<std::wstring> ids;
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
	bool InGameData::CreateNew(const GameData::FGameDataStore& store, const std::wstring& sceneId,
		std::unique_ptr<InGameData>& output, std::string& error)
	{
		try
		{
			const auto* profile = store.GetDefaultNewGameProfile();
			if (!profile) throw std::runtime_error("New-game catalog is missing");
			auto candidate = std::unique_ptr<InGameData>(new InGameData());
			candidate->m_contentVersion = profile->contentVersion;
			candidate->m_sceneId = sceneId;
			candidate->m_character = {profile->characterId, profile->initialLevel, profile->initialExperience, 0,
				{profile->initialMaxHpBonus, profile->initialAttackBonus, profile->initialDefenseBonus}};
			candidate->m_gold = profile->initialGold;
			SCharacterStats stats;
			if (!candidate->GetStats(store, stats, error)) return false;
			candidate->m_character.CurrentHp = stats.MaxHp;
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
	const SCharacterProgress& InGameData::GetCharacter() const noexcept { return m_character; }
	std::uint32_t InGameData::GetGold() const noexcept { return m_gold; }
	const std::wstring& InGameData::GetSceneId() const noexcept { return m_sceneId; }
	const std::vector<SInventoryItemProgress>& InGameData::GetInventory() const noexcept { return m_inventory; }
	const std::vector<SAchievementProgress>& InGameData::GetAchievements() const noexcept { return m_achievements; }
	bool InGameData::GetStats(const GameData::FGameDataStore& store, SCharacterStats& output, std::string& error) const
	{
		const auto* level = store.FindLevelDefinition(m_character.Level);
		if (!level || !store.FindCharacterDefinition(m_character.CharacterId))
		{
			error = "CharacterId or Level definition is missing";
			return false;
		}
		constexpr auto maximum = (std::numeric_limits<std::uint32_t>::max)();
		const auto& bonus = m_character.StatBonuses;
		if (bonus.MaxHp > maximum - level->maxHp || bonus.Attack > maximum - level->attack || bonus.Defense > maximum - level->defense)
		{
			error = "Derived stats overflow";
			return false;
		}
		output = {level->maxHp + bonus.MaxHp, level->attack + bonus.Attack, level->defense + bonus.Defense};
		error.clear();
		return true;
	}
	bool InGameData::Serialize(std::vector<std::uint8_t>& output, std::string& error) const
	{
		try
		{
			CheckBounds(m_sceneId, m_character, m_inventory, m_achievements);
			FWriter writer;
			writer.Number(kMagic, 4);
			writer.Number(kFormatVersion, 4);
			writer.Number(m_contentVersion, 4);
			writer.Id(m_sceneId);
			WriteCharacter(writer, m_character);
			writer.Number(m_gold, 4);
			writer.Number(m_inventory.size(), 4);
			for (const auto& item : m_inventory) WriteItem(writer, item);
			writer.Number(m_achievements.size(), 4);
			for (const auto& achievement : m_achievements) WriteAchievement(writer, achievement);
			auto candidate = writer.Take();
			output.swap(candidate);
			error.clear();
			return true;
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
	}
	bool InGameData::Deserialize(const std::vector<std::uint8_t>& bytes, const GameData::FGameDataStore& store,
		std::unique_ptr<InGameData>& output, std::string& error)
	{
		try
		{
			if (bytes.size() > kMaxBytes) throw std::runtime_error("Total progress bytes exceed limit");
			FReader reader(bytes);
			if (reader.UInt32() != kMagic) throw std::runtime_error("Invalid progress magic");
			if (reader.UInt32() != kFormatVersion) throw std::runtime_error("Unsupported progress format version");
			auto candidate = std::unique_ptr<InGameData>(new InGameData());
			candidate->m_contentVersion = reader.UInt32();
			candidate->m_sceneId = reader.Id();
			candidate->m_character = ReadCharacter(reader);
			candidate->m_gold = reader.UInt32();
			const auto inventoryCount = reader.Count(18);
			candidate->m_inventory.reserve(inventoryCount);
			for (std::uint32_t i = 0; i < inventoryCount; ++i) candidate->m_inventory.push_back(ReadItem(reader));
			const auto achievementCount = reader.Count(11);
			candidate->m_achievements.reserve(achievementCount);
			for (std::uint32_t i = 0; i < achievementCount; ++i) candidate->m_achievements.push_back(ReadAchievement(reader));
			if (!reader.IsDone()) throw std::runtime_error("Trailing progress bytes");
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
		m_character.CurrentHp = hp;
		error.clear();
		return true;
	}
	bool InGameData::SetGold(std::uint32_t gold, std::string& error)
	{
		m_gold = gold;
		error.clear();
		return true;
	}
	bool InGameData::SetSceneId(const GameData::FGameDataStore& store, const std::wstring& sceneId, std::string& error)
	{
		try
		{
			if (!Validate(store, error)) return false;
			CheckBounds(sceneId, m_character, m_inventory, m_achievements);
			if (!store.FindScene(sceneId)) throw std::runtime_error("SceneId definition is missing");
			auto candidate = sceneId;
			m_sceneId.swap(candidate);
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
			CheckBounds(m_sceneId, m_character, inventory, m_achievements);
			CheckInventory(store, inventory);
			auto candidate = inventory;
			m_inventory.swap(candidate);
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
			CheckBounds(m_sceneId, m_character, m_inventory, achievements);
			CheckAchievements(store, achievements);
			auto candidate = achievements;
			m_achievements.swap(candidate);
			error.clear();
			return true;
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
	}
	bool InGameData::Validate(const GameData::FGameDataStore& store, std::string& error) const
	{
		try
		{
			CheckBounds(m_sceneId, m_character, m_inventory, m_achievements);
			CheckInventory(store, m_inventory);
			CheckAchievements(store, m_achievements);
		}
		catch (const std::exception& exception) { error = exception.what(); return false; }
		const auto* profile = store.GetDefaultNewGameProfile();
		if (!profile || m_contentVersion != profile->contentVersion)
		{
			error = "Unsupported content version or missing catalog";
			return false;
		}
		if (!store.FindScene(m_sceneId))
		{
			error = "SceneId definition is missing";
			return false;
		}
		SCharacterStats stats;
		if (!GetStats(store, stats, error)) return false;
		const auto* level = store.FindLevelDefinition(m_character.Level);
		if (!level || m_character.Experience < level->requiredExperience ||
			std::any_of(store.GetLevelDefinitions().begin(), store.GetLevelDefinitions().end(), [&](const auto& next)
			{ return next.level > m_character.Level && m_character.Experience >= next.requiredExperience; }))
		{
			error = "Character level/experience mismatch";
			return false;
		}
		if (m_character.CurrentHp > stats.MaxHp)
		{
			error = "CurrentHp exceeds derived MaxHp";
			return false;
		}
		error.clear();
		return true;
	}
}
