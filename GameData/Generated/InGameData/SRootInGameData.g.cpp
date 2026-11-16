#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SRootInGameData::SRootInGameData() = default;
	SRootInGameData::~SRootInGameData() = default;

	SRootInGameData::SRootInGameData(SRootInGameData&& _other) noexcept = default;
	SRootInGameData& SRootInGameData::operator=(SRootInGameData&& _other) noexcept = default;

	SRootInGameData::SRootInGameData(
		::std::uint32_t _field0,
		::std::wstring _field1,
		::std::shared_ptr<::GameData::InGameData::SCharacterProgress> _field2,
		::std::uint32_t _field3,
		::std::vector<::GameData::InGameData::SInventoryItemProgress> _field4,
		::std::vector<::GameData::InGameData::SAchievementProgress> _field5)
		: m_contentVersion(::std::move(_field0))
		, m_sceneId(::std::move(_field1))
		, m_character(::std::move(_field2))
		, m_gold(::std::move(_field3))
		, m_inventory(::std::move(_field4))
		, m_achievements(::std::move(_field5))
	{
	}

	bool SRootInGameData::operator==(const SRootInGameData& _other) const
	{
		return this->m_contentVersion == _other.m_contentVersion &&
			this->m_sceneId == _other.m_sceneId &&
			this->m_character == _other.m_character &&
			this->m_gold == _other.m_gold &&
			this->m_inventory == _other.m_inventory &&
			this->m_achievements == _other.m_achievements;
	}

	::std::string_view SRootInGameData::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SRootInGameData::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("m_contentVersion", this->m_contentVersion, _error)) return false;
		if (!_writer.Field("m_sceneId", this->m_sceneId, _error)) return false;
		if (!_writer.Field("m_character", this->m_character, _error)) return false;
		if (!_writer.Field("m_gold", this->m_gold, _error)) return false;
		if (!_writer.Field("m_inventory", this->m_inventory, _error)) return false;
		if (!_writer.Field("m_achievements", this->m_achievements, _error)) return false;
		return true;
	}

	bool SRootInGameData::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (!_reader.Field("m_contentVersion", this->m_contentVersion, _error, true)) return false;
		if (!_reader.Field("m_sceneId", this->m_sceneId, _error, true)) return false;
		if (!_reader.Field("m_character", this->m_character, _error, true)) return false;
		if (!_reader.Field("m_gold", this->m_gold, _error, true)) return false;
		if (!_reader.Field("m_inventory", this->m_inventory, _error, true)) return false;
		if (!_reader.Field("m_achievements", this->m_achievements, _error, true)) return false;
		return true;
	}
}
