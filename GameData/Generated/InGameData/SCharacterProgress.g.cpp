#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SCharacterProgress::SCharacterProgress() = default;
	SCharacterProgress::~SCharacterProgress() = default;

	SCharacterProgress::SCharacterProgress(SCharacterProgress&& _other) noexcept = default;
	SCharacterProgress& SCharacterProgress::operator=(SCharacterProgress&& _other) noexcept = default;

	SCharacterProgress::SCharacterProgress(
		::std::uint32_t _field0,
		::std::uint32_t _field1,
		::std::uint32_t _field2,
		::std::uint32_t _field3,
		::GameData::InGameData::SCharacterStats _field4)
		: CharacterId(::std::move(_field0))
		, Level(::std::move(_field1))
		, Experience(::std::move(_field2))
		, CurrentHp(::std::move(_field3))
		, StatBonuses(::std::move(_field4))
	{
	}

	bool SCharacterProgress::operator==(const SCharacterProgress& _other) const
	{
		return this->CharacterId == _other.CharacterId &&
			this->Level == _other.Level &&
			this->Experience == _other.Experience &&
			this->CurrentHp == _other.CurrentHp &&
			this->StatBonuses == _other.StatBonuses;
	}

	::std::string_view SCharacterProgress::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SCharacterProgress::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("CharacterId", this->CharacterId, _error)) return false;
		if (!_writer.Field("Level", this->Level, _error)) return false;
		if (!_writer.Field("Experience", this->Experience, _error)) return false;
		if (!_writer.Field("CurrentHp", this->CurrentHp, _error)) return false;
		if (!_writer.Field("StatBonuses", this->StatBonuses, _error)) return false;
		return true;
	}

	bool SCharacterProgress::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (!_reader.Field("CharacterId", this->CharacterId, _error, true)) return false;
		if (!_reader.Field("Level", this->Level, _error, true)) return false;
		if (!_reader.Field("Experience", this->Experience, _error, true)) return false;
		if (!_reader.Field("CurrentHp", this->CurrentHp, _error, true)) return false;
		if (!_reader.Field("StatBonuses", this->StatBonuses, _error, true)) return false;
		return true;
	}
}
