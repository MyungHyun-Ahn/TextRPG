#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SAchievementProgress::SAchievementProgress() = default;
	SAchievementProgress::~SAchievementProgress() = default;

	SAchievementProgress::SAchievementProgress(SAchievementProgress&& _other) noexcept = default;
	SAchievementProgress& SAchievementProgress::operator=(SAchievementProgress&& _other) noexcept = default;

	SAchievementProgress::SAchievementProgress(
		::std::uint32_t _field0,
		::std::uint32_t _field1,
		bool _field2)
		: AchievementId(::std::move(_field0))
		, Progress(::std::move(_field1))
		, IsCompleted(::std::move(_field2))
	{
	}

	bool SAchievementProgress::operator==(const SAchievementProgress& _other) const
	{
		return this->AchievementId == _other.AchievementId &&
			this->Progress == _other.Progress &&
			this->IsCompleted == _other.IsCompleted;
	}

	::std::string_view SAchievementProgress::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SAchievementProgress::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("AchievementId", this->AchievementId, _error)) return false;
		if (!_writer.Field("Progress", this->Progress, _error)) return false;
		if (!_writer.Field("IsCompleted", this->IsCompleted, _error)) return false;
		return true;
	}

	bool SAchievementProgress::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (!_reader.Field("AchievementId", this->AchievementId, _error, true)) return false;
		if (!_reader.Field("Progress", this->Progress, _error, true)) return false;
		if (!_reader.Field("IsCompleted", this->IsCompleted, _error, true)) return false;
		return true;
	}
}
