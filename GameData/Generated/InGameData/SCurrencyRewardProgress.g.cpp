#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SCurrencyRewardProgress::SCurrencyRewardProgress() = default;
	SCurrencyRewardProgress::~SCurrencyRewardProgress() = default;

	SCurrencyRewardProgress::SCurrencyRewardProgress(SCurrencyRewardProgress&& _other) noexcept = default;
	SCurrencyRewardProgress& SCurrencyRewardProgress::operator=(SCurrencyRewardProgress&& _other) noexcept = default;

	SCurrencyRewardProgress::SCurrencyRewardProgress(
		::std::uint32_t _field0,
		::std::uint32_t _field1,
		::std::uint32_t _field2,
		::std::uint32_t _field3,
		::std::uint32_t _field4)
		: CurrencyType(::std::move(_field0))
		, AmountMin(::std::move(_field1))
		, AmountMax(::std::move(_field2))
		, RolledAmount(::std::move(_field3))
		, GrantedAmount(::std::move(_field4))
	{
	}

	bool SCurrencyRewardProgress::operator==(const SCurrencyRewardProgress& _other) const
	{
		return this->CurrencyType == _other.CurrencyType &&
			this->AmountMin == _other.AmountMin &&
			this->AmountMax == _other.AmountMax &&
			this->RolledAmount == _other.RolledAmount &&
			this->GrantedAmount == _other.GrantedAmount;
	}

	::std::string_view SCurrencyRewardProgress::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SCurrencyRewardProgress::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("CurrencyType", this->CurrencyType, _error)) return false;
		if (!_writer.Field("AmountMin", this->AmountMin, _error)) return false;
		if (!_writer.Field("AmountMax", this->AmountMax, _error)) return false;
		if (!_writer.Field("RolledAmount", this->RolledAmount, _error)) return false;
		if (!_writer.Field("GrantedAmount", this->GrantedAmount, _error)) return false;
		return true;
	}

	bool SCurrencyRewardProgress::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (!_reader.Field("CurrencyType", this->CurrencyType, _error, true)) return false;
		if (!_reader.Field("AmountMin", this->AmountMin, _error, true)) return false;
		if (!_reader.Field("AmountMax", this->AmountMax, _error, true)) return false;
		if (!_reader.Field("RolledAmount", this->RolledAmount, _error, true)) return false;
		if (!_reader.Field("GrantedAmount", this->GrantedAmount, _error, true)) return false;
		return true;
	}
}
