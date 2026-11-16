#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	using SCharacterStats = GameData::InGameData::SCharacterStats;
	using SCharacterProgress = GameData::InGameData::SCharacterProgress;
	using SInventoryItemProgress = GameData::InGameData::SInventoryItemProgress;
	using SAchievementProgress = GameData::InGameData::SAchievementProgress;

	// 진행 값만 소유한다. 정의/Scene/UI/저장 서비스의 수명이나 포인터를 보관하지 않는다.
	class InGameData final : public ISerializable
	{
	public:
		InGameData(const InGameData&) = delete;
		InGameData& operator=(const InGameData&) = delete;
		[[nodiscard]] static bool CreateNew(const GameData::FGameDataStore& store, const std::wstring& sceneId,
			std::unique_ptr<InGameData>& output, std::string& error);
		[[nodiscard]] static bool Deserialize(const std::vector<std::uint8_t>& bytes, const GameData::FGameDataStore& store,
			std::unique_ptr<InGameData>& output, std::string& error);
		[[nodiscard]] bool Serialize(std::vector<std::uint8_t>& output, std::string& error) const;
		[[nodiscard]] std::string_view GetSerializableTypeName() const noexcept override;
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
	protected:
		[[nodiscard]] bool SerializeFields(FBinaryWriter& writer, std::string& error) const override;
		[[nodiscard]] bool DeserializeFields(FBinaryReader& reader, std::string& error) override;
	private:
		InGameData();
		[[nodiscard]] bool Validate(const GameData::FGameDataStore& store, std::string& error) const;
		// 유일한 변경 가능 원본이다. UI에는 내부 shared_ptr나 변경 가능한 루트를 노출하지 않는다.
		GameData::InGameData::SRootInGameData m_data;
	};
}
