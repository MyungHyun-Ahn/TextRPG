#pragma once

// 구형 schema 1 writer를 독립적으로 고정한다. 제품 codec/숫자 모델을 복사해 기대값을 만들지 않는다.
namespace LegacyProgressFixture
{
	class FWriteOnly : public TextRPG::ISerializable
	{
	protected:
		bool DeserializeFields(TextRPG::FBinaryReader&, std::string& error) override
		{
			error = "legacy fixture is write only";
			return false;
		}
	};

	struct SCharacterProgress final : public FWriteOnly
	{
		static constexpr std::string_view kTypeName = "SCharacterProgress";
		std::wstring CharacterId = L"Swordswoman";
		std::uint32_t Level = 1, Experience = 0, CurrentHp = 7;
		GameData::InGameData::SCharacterStats StatBonuses;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool SerializeFields(TextRPG::FBinaryWriter& writer, std::string& error) const override
		{
			return writer.Field("CharacterId", CharacterId, error) && writer.Field("Level", Level, error) &&
				writer.Field("Experience", Experience, error) && writer.Field("CurrentHp", CurrentHp, error) &&
				writer.Field("StatBonuses", StatBonuses, error);
		}
	};

	struct SInventoryItemProgress final : public FWriteOnly
	{
		static constexpr std::string_view kTypeName = "SInventoryItemProgress";
		std::uint64_t InstanceId = 1;
		std::wstring ItemId = L"Token";
		std::uint32_t Count = 1;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool SerializeFields(TextRPG::FBinaryWriter& writer, std::string& error) const override
		{
			return writer.Field("InstanceId", InstanceId, error) && writer.Field("ItemId", ItemId, error) &&
				writer.Field("Count", Count, error);
		}
	};

	struct SAchievementProgress final : public FWriteOnly
	{
		static constexpr std::string_view kTypeName = "SAchievementProgress";
		std::wstring AchievementId = L"Journey";
		std::uint32_t Progress = 1;
		bool IsCompleted = true;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool SerializeFields(TextRPG::FBinaryWriter& writer, std::string& error) const override
		{
			return writer.Field("AchievementId", AchievementId, error) && writer.Field("Progress", Progress, error) &&
				writer.Field("IsCompleted", IsCompleted, error);
		}
	};

	struct SRootInGameData final : public FWriteOnly
	{
		static constexpr std::string_view kTypeName = "SRootInGameData";
		std::uint32_t ContentVersion = 1;
		std::wstring SceneId = L"Town";
		std::shared_ptr<SCharacterProgress> Character = std::make_shared<SCharacterProgress>();
		std::uint32_t Gold = 54321;
		std::vector<SInventoryItemProgress> Inventory;
		std::vector<SAchievementProgress> Achievements;
		std::string_view GetSerializableTypeName() const noexcept override
		{
			return kTypeName;
		}
	protected:
		bool SerializeFields(TextRPG::FBinaryWriter& writer, std::string& error) const override
		{
			return writer.Field("m_contentVersion", ContentVersion, error) && writer.Field("m_sceneId", SceneId, error) &&
				writer.Field("m_character", Character, error) && writer.Field("m_gold", Gold, error) &&
				writer.Field("m_inventory", Inventory, error) && writer.Field("m_achievements", Achievements, error);
		}
	};
}
