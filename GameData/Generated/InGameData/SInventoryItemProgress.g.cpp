#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SInventoryItemProgress::SInventoryItemProgress() = default;
	SInventoryItemProgress::~SInventoryItemProgress() = default;

	SInventoryItemProgress::SInventoryItemProgress(SInventoryItemProgress&& _other) noexcept = default;
	SInventoryItemProgress& SInventoryItemProgress::operator=(SInventoryItemProgress&& _other) noexcept = default;

	SInventoryItemProgress::SInventoryItemProgress(
		::std::uint64_t _field0,
		::std::uint32_t _field1,
		::std::uint32_t _field2)
		: InstanceId(::std::move(_field0))
		, ItemId(::std::move(_field1))
		, Count(::std::move(_field2))
	{
	}

	bool SInventoryItemProgress::operator==(const SInventoryItemProgress& _other) const
	{
		return this->InstanceId == _other.InstanceId &&
			this->ItemId == _other.ItemId &&
			this->Count == _other.Count;
	}

	::std::string_view SInventoryItemProgress::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SInventoryItemProgress::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("InstanceId", this->InstanceId, _error)) return false;
		if (!_writer.Field("ItemId", this->ItemId, _error)) return false;
		if (!_writer.Field("Count", this->Count, _error)) return false;
		return true;
	}

	bool SInventoryItemProgress::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (!_reader.Field("InstanceId", this->InstanceId, _error, true)) return false;
		if (!_reader.Field("ItemId", this->ItemId, _error, true)) return false;
		if (!_reader.Field("Count", this->Count, _error, true)) return false;
		return true;
	}
}
