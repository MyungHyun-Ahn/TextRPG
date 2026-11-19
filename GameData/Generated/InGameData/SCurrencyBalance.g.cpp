#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SCurrencyBalance::SCurrencyBalance() = default;
	SCurrencyBalance::~SCurrencyBalance() = default;

	SCurrencyBalance::SCurrencyBalance(SCurrencyBalance&& _other) noexcept = default;
	SCurrencyBalance& SCurrencyBalance::operator=(SCurrencyBalance&& _other) noexcept = default;

	SCurrencyBalance::SCurrencyBalance(
		::std::uint32_t _field0,
		::std::uint32_t _field1)
		: CurrencyType(::std::move(_field0))
		, Amount(::std::move(_field1))
	{
	}

	bool SCurrencyBalance::operator==(const SCurrencyBalance& _other) const
	{
		return this->CurrencyType == _other.CurrencyType &&
			this->Amount == _other.Amount;
	}

	::std::string_view SCurrencyBalance::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SCurrencyBalance::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("CurrencyType", this->CurrencyType, _error)) return false;
		if (!_writer.Field("Amount", this->Amount, _error)) return false;
		return true;
	}

	bool SCurrencyBalance::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (!_reader.Field("CurrencyType", this->CurrencyType, _error, true)) return false;
		if (!_reader.Field("Amount", this->Amount, _error, true)) return false;
		return true;
	}
}
