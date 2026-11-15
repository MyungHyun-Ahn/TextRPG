#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	struct SCharacterStats final
	{
		std::uint32_t MaxHp{};
		std::uint32_t Attack{};
		std::uint32_t Defense{};
		bool operator==(const SCharacterStats&) const = default;
	};
	struct SCharacterProgress final
	{
		std::wstring CharacterId;
		std::uint32_t Level{};
		std::uint32_t Experience{};
		std::uint32_t CurrentHp{};
		SCharacterStats StatBonuses;
		bool operator==(const SCharacterProgress&) const = default;
	};
	struct SInventoryItemProgress final
	{
		std::uint64_t InstanceId{};
		std::wstring ItemId;
		std::uint32_t Count{};
		bool operator==(const SInventoryItemProgress&) const = default;
	};
	struct SAchievementProgress final
	{
		std::wstring AchievementId;
		std::uint32_t Progress{};
		bool IsCompleted{};
		bool operator==(const SAchievementProgress&) const = default;
	};

	// 진행 값만 소유한다. 정의/Scene/UI/저장 서비스의 수명이나 포인터를 보관하지 않는다.
	class InGameData final
	{
	public:
		InGameData(const InGameData&) = delete;
		InGameData& operator=(const InGameData&) = delete;
		[[nodiscard]] static bool CreateNew(const GameData::FGameDataStore& store, const std::wstring& sceneId,
			std::unique_ptr<InGameData>& output, std::string& error);
		[[nodiscard]] static bool Deserialize(const std::vector<std::uint8_t>& bytes, const GameData::FGameDataStore& store,
			std::unique_ptr<InGameData>& output, std::string& error);
		[[nodiscard]] bool Serialize(std::vector<std::uint8_t>& output, std::string& error) const;
		[[nodiscard]] const SCharacterProgress& GetCharacter() const noexcept;
		[[nodiscard]] std::uint32_t GetGold() const noexcept;
		[[nodiscard]] const std::wstring& GetSceneId() const noexcept;
		[[nodiscard]] const std::vector<SInventoryItemProgress>& GetInventory() const noexcept;
		[[nodiscard]] const std::vector<SAchievementProgress>& GetAchievements() const noexcept;
		[[nodiscard]] bool GetStats(const GameData::FGameDataStore& store, SCharacterStats& output, std::string& error) const;
		[[nodiscard]] bool SetCurrentHp(const GameData::FGameDataStore& store, std::uint32_t hp, std::string& error);
		[[nodiscard]] bool SetGold(std::uint32_t gold, std::string& error);
		[[nodiscard]] bool SetSceneId(const GameData::FGameDataStore& store, const std::wstring& sceneId, std::string& error);
		// 전체 후보 컬렉션을 검증한 뒤 교체한다. 실패하면 기존 값/참조가 유지된다.
		[[nodiscard]] bool SetInventory(const GameData::FGameDataStore& store,
			const std::vector<SInventoryItemProgress>& inventory, std::string& error);
		[[nodiscard]] bool SetAchievements(const GameData::FGameDataStore& store,
			const std::vector<SAchievementProgress>& achievements, std::string& error);
	private:
		InGameData() = default;
		[[nodiscard]] bool Validate(const GameData::FGameDataStore& store, std::string& error) const;
		std::uint32_t m_contentVersion{};
		std::wstring m_sceneId;
		SCharacterProgress m_character;
		std::uint32_t m_gold{};
		std::vector<SInventoryItemProgress> m_inventory;
		std::vector<SAchievementProgress> m_achievements;
	};
}
