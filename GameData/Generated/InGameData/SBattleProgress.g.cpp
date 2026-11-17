#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	SBattleProgress::SBattleProgress() = default;
	SBattleProgress::~SBattleProgress() = default;

	SBattleProgress::SBattleProgress(SBattleProgress&& _other) noexcept = default;
	SBattleProgress& SBattleProgress::operator=(SBattleProgress&& _other) noexcept = default;

	SBattleProgress::SBattleProgress(
		::std::uint32_t _field0,
		::std::uint32_t _field1,
		::std::uint32_t _field2,
		::std::uint32_t _field3,
		::std::uint32_t _field4,
		::std::uint64_t _field5,
		::std::uint64_t _field6,
		bool _field7,
		::std::uint32_t _field8,
		::std::uint32_t _field9,
		bool _field10,
		bool _field11,
		::std::uint32_t _field12)
		: RuleId(::std::move(_field0))
		, MonsterId(::std::move(_field1))
		, MonsterHp(::std::move(_field2))
		, Round(::std::move(_field3))
		, Outcome(::std::move(_field4))
		, Seed(::std::move(_field5))
		, RandCount(::std::move(_field6))
		, FirstIsPlayer(::std::move(_field7))
		, FirstDamage(::std::move(_field8))
		, SecondDamage(::std::move(_field9))
		, FirstCritical(::std::move(_field10))
		, SecondCritical(::std::move(_field11))
		, HitCount(::std::move(_field12))
	{
	}

	bool SBattleProgress::operator==(const SBattleProgress& _other) const
	{
		return this->RuleId == _other.RuleId &&
			this->MonsterId == _other.MonsterId &&
			this->MonsterHp == _other.MonsterHp &&
			this->Round == _other.Round &&
			this->Outcome == _other.Outcome &&
			this->Seed == _other.Seed &&
			this->RandCount == _other.RandCount &&
			this->FirstIsPlayer == _other.FirstIsPlayer &&
			this->FirstDamage == _other.FirstDamage &&
			this->SecondDamage == _other.SecondDamage &&
			this->FirstCritical == _other.FirstCritical &&
			this->SecondCritical == _other.SecondCritical &&
			this->HitCount == _other.HitCount;
	}

	::std::string_view SBattleProgress::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SBattleProgress::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const
	{
		if (!_writer.Field("RuleId", this->RuleId, _error)) return false;
		if (!_writer.Field("MonsterId", this->MonsterId, _error)) return false;
		if (!_writer.Field("MonsterHp", this->MonsterHp, _error)) return false;
		if (!_writer.Field("Round", this->Round, _error)) return false;
		if (!_writer.Field("Outcome", this->Outcome, _error)) return false;
		if (!_writer.Field("Seed", this->Seed, _error)) return false;
		if (!_writer.Field("RandCount", this->RandCount, _error)) return false;
		if (!_writer.Field("FirstIsPlayer", this->FirstIsPlayer, _error)) return false;
		if (!_writer.Field("FirstDamage", this->FirstDamage, _error)) return false;
		if (!_writer.Field("SecondDamage", this->SecondDamage, _error)) return false;
		if (!_writer.Field("FirstCritical", this->FirstCritical, _error)) return false;
		if (!_writer.Field("SecondCritical", this->SecondCritical, _error)) return false;
		if (!_writer.Field("HitCount", this->HitCount, _error)) return false;
		return true;
	}

	bool SBattleProgress::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)
	{
		if (!_reader.Field("RuleId", this->RuleId, _error, true)) return false;
		if (!_reader.Field("MonsterId", this->MonsterId, _error, true)) return false;
		if (!_reader.Field("MonsterHp", this->MonsterHp, _error, true)) return false;
		if (!_reader.Field("Round", this->Round, _error, true)) return false;
		if (!_reader.Field("Outcome", this->Outcome, _error, true)) return false;
		if (!_reader.Field("Seed", this->Seed, _error, true)) return false;
		if (!_reader.Field("RandCount", this->RandCount, _error, true)) return false;
		if (!_reader.Field("FirstIsPlayer", this->FirstIsPlayer, _error, true)) return false;
		if (!_reader.Field("FirstDamage", this->FirstDamage, _error, true)) return false;
		if (!_reader.Field("SecondDamage", this->SecondDamage, _error, true)) return false;
		if (!_reader.Field("FirstCritical", this->FirstCritical, _error, true)) return false;
		if (!_reader.Field("SecondCritical", this->SecondCritical, _error, true)) return false;
		if (!_reader.Field("HitCount", this->HitCount, _error, true)) return false;
		return true;
	}
}
