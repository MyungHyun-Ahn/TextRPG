#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SBasicStats::SBasicStats() = default;
	SBasicStats::~SBasicStats() = default;

	SBasicStats::SBasicStats(SBasicStats&& _other) noexcept = default;
	SBasicStats& SBasicStats::operator=(SBasicStats&& _other) noexcept = default;

	SBasicStats::SBasicStats(
		::std::uint32_t _field0,
		::std::uint32_t _field1,
		::std::uint32_t _field2,
		::std::uint32_t _field3)
		: Strength(::std::move(_field0))
		, Dexterity(::std::move(_field1))
		, Intelligence(::std::move(_field2))
		, Luck(::std::move(_field3))
	{
	}

	bool SBasicStats::operator==(const SBasicStats& _other) const
	{
		return this->Strength == _other.Strength &&
			this->Dexterity == _other.Dexterity &&
			this->Intelligence == _other.Intelligence &&
			this->Luck == _other.Luck;
	}

	::std::string_view SBasicStats::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SBasicStats::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("Strength", this->Strength, _error)) return false;
		if (!_writer.Field("Dexterity", this->Dexterity, _error)) return false;
		if (!_writer.Field("Intelligence", this->Intelligence, _error)) return false;
		if (!_writer.Field("Luck", this->Luck, _error)) return false;
		return true;
	}

	bool SBasicStats::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (_reader.GetSchemaVersion() < 3u) this->Strength = ::std::uint32_t{};
		if (!_reader.Field("Strength", this->Strength, _error, _reader.GetSchemaVersion() >= 3u)) return false;
		if (_reader.GetSchemaVersion() < 3u) this->Dexterity = ::std::uint32_t{};
		if (!_reader.Field("Dexterity", this->Dexterity, _error, _reader.GetSchemaVersion() >= 3u)) return false;
		if (_reader.GetSchemaVersion() < 3u) this->Intelligence = ::std::uint32_t{};
		if (!_reader.Field("Intelligence", this->Intelligence, _error, _reader.GetSchemaVersion() >= 3u)) return false;
		if (_reader.GetSchemaVersion() < 3u) this->Luck = ::std::uint32_t{};
		if (!_reader.Field("Luck", this->Luck, _error, _reader.GetSchemaVersion() >= 3u)) return false;
		return true;
	}
}
