#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SCharacterStats::SCharacterStats() = default;
	SCharacterStats::~SCharacterStats() = default;

	SCharacterStats::SCharacterStats(SCharacterStats&& _other) noexcept = default;
	SCharacterStats& SCharacterStats::operator=(SCharacterStats&& _other) noexcept = default;

	SCharacterStats::SCharacterStats(
		::std::uint32_t _field0,
		::std::uint32_t _field1,
		::std::uint32_t _field2)
		: MaxHp(::std::move(_field0))
		, Attack(::std::move(_field1))
		, Defense(::std::move(_field2))
	{
	}

	bool SCharacterStats::operator==(const SCharacterStats& _other) const
	{
		return this->MaxHp == _other.MaxHp &&
			this->Attack == _other.Attack &&
			this->Defense == _other.Defense;
	}

	::std::string_view SCharacterStats::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SCharacterStats::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("MaxHp", this->MaxHp, _error)) return false;
		if (!_writer.Field("Attack", this->Attack, _error)) return false;
		if (!_writer.Field("Defense", this->Defense, _error)) return false;
		return true;
	}

	bool SCharacterStats::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (!_reader.Field("MaxHp", this->MaxHp, _error, true)) return false;
		if (!_reader.Field("Attack", this->Attack, _error, true)) return false;
		if (!_reader.Field("Defense", this->Defense, _error, true)) return false;
		return true;
	}
}
