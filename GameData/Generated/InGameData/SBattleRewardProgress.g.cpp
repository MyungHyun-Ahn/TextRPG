#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SBattleRewardProgress::SBattleRewardProgress() = default;
	SBattleRewardProgress::~SBattleRewardProgress() = default;

	SBattleRewardProgress::SBattleRewardProgress(SBattleRewardProgress&& _other) noexcept = default;
	SBattleRewardProgress& SBattleRewardProgress::operator=(SBattleRewardProgress&& _other) noexcept = default;

	SBattleRewardProgress::SBattleRewardProgress(
		::std::uint32_t _field0,
		::std::uint32_t _field1,
		::std::uint32_t _field2,
		::std::uint32_t _field3,
		::std::uint32_t _field4,
		::std::uint32_t _field5,
		::std::uint32_t _field6,
		::std::uint32_t _field7,
		::std::uint64_t _field8,
		::std::uint64_t _field9,
		::std::vector<::GameData::InGameData::SCurrencyRewardProgress> _field10)
		: ExperienceBefore(::std::move(_field0))
		, ExperienceAfter(::std::move(_field1))
		, CurveMaxLevel(::std::move(_field2))
		, State(::std::move(_field3))
		, PlannedExperience(::std::move(_field4))
		, GrantedExperience(::std::move(_field5))
		, LevelBefore(::std::move(_field6))
		, LevelAfter(::std::move(_field7))
		, CurrencySeed(::std::move(_field8))
		, CurrencyRandCount(::std::move(_field9))
		, Currencies(::std::move(_field10))
	{
	}

	bool SBattleRewardProgress::operator==(const SBattleRewardProgress& _other) const
	{
		return this->ExperienceBefore == _other.ExperienceBefore &&
			this->ExperienceAfter == _other.ExperienceAfter &&
			this->CurveMaxLevel == _other.CurveMaxLevel &&
			this->State == _other.State &&
			this->PlannedExperience == _other.PlannedExperience &&
			this->GrantedExperience == _other.GrantedExperience &&
			this->LevelBefore == _other.LevelBefore &&
			this->LevelAfter == _other.LevelAfter &&
			this->CurrencySeed == _other.CurrencySeed &&
			this->CurrencyRandCount == _other.CurrencyRandCount &&
			this->Currencies == _other.Currencies;
	}

	::std::string_view SBattleRewardProgress::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SBattleRewardProgress::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("ExperienceBefore", this->ExperienceBefore, _error)) return false;
		if (!_writer.Field("ExperienceAfter", this->ExperienceAfter, _error)) return false;
		if (!_writer.Field("CurveMaxLevel", this->CurveMaxLevel, _error)) return false;
		if (!_writer.Field("State", this->State, _error)) return false;
		if (!_writer.Field("PlannedExperience", this->PlannedExperience, _error)) return false;
		if (!_writer.Field("GrantedExperience", this->GrantedExperience, _error)) return false;
		if (!_writer.Field("LevelBefore", this->LevelBefore, _error)) return false;
		if (!_writer.Field("LevelAfter", this->LevelAfter, _error)) return false;
		if (!_writer.Field("CurrencySeed", this->CurrencySeed, _error)) return false;
		if (!_writer.Field("CurrencyRandCount", this->CurrencyRandCount, _error)) return false;
		if (!_writer.Field("Currencies", this->Currencies, _error)) return false;
		return true;
	}

	bool SBattleRewardProgress::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (_reader.GetSchemaVersion() < 5u) this->ExperienceBefore = ::std::uint32_t{};
		if (!_reader.Field("ExperienceBefore", this->ExperienceBefore, _error, _reader.GetSchemaVersion() >= 5u)) return false;
		if (_reader.GetSchemaVersion() < 5u) this->ExperienceAfter = ::std::uint32_t{};
		if (!_reader.Field("ExperienceAfter", this->ExperienceAfter, _error, _reader.GetSchemaVersion() >= 5u)) return false;
		if (_reader.GetSchemaVersion() < 5u) this->CurveMaxLevel = ::std::uint32_t{};
		if (!_reader.Field("CurveMaxLevel", this->CurveMaxLevel, _error, _reader.GetSchemaVersion() >= 5u)) return false;
		if (!_reader.Field("State", this->State, _error, true)) return false;
		if (!_reader.Field("PlannedExperience", this->PlannedExperience, _error, true)) return false;
		if (!_reader.Field("GrantedExperience", this->GrantedExperience, _error, true)) return false;
		if (!_reader.Field("LevelBefore", this->LevelBefore, _error, true)) return false;
		if (!_reader.Field("LevelAfter", this->LevelAfter, _error, true)) return false;
		if (!_reader.Field("CurrencySeed", this->CurrencySeed, _error, true)) return false;
		if (!_reader.Field("CurrencyRandCount", this->CurrencyRandCount, _error, true)) return false;
		if (!_reader.Field("Currencies", this->Currencies, _error, true)) return false;
		return true;
	}
}
