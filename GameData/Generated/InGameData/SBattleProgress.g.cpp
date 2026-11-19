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
		::GameData::InGameData::SBattleRewardProgress _field0,
		::std::uint32_t _field1,
		::std::uint32_t _field2,
		::std::uint32_t _field3,
		::std::uint32_t _field4,
		::std::uint32_t _field5,
		::std::uint64_t _field6,
		::std::uint64_t _field7,
		bool _field8,
		::std::uint32_t _field9,
		::std::uint32_t _field10,
		bool _field11,
		bool _field12,
		::std::uint32_t _field13)
		: Reward(::std::move(_field0))
		, RuleId(::std::move(_field1))
		, MonsterId(::std::move(_field2))
		, MonsterHp(::std::move(_field3))
		, Round(::std::move(_field4))
		, Outcome(::std::move(_field5))
		, Seed(::std::move(_field6))
		, RandCount(::std::move(_field7))
		, FirstIsPlayer(::std::move(_field8))
		, FirstDamage(::std::move(_field9))
		, SecondDamage(::std::move(_field10))
		, FirstCritical(::std::move(_field11))
		, SecondCritical(::std::move(_field12))
		, HitCount(::std::move(_field13))
	{
	}

	bool SBattleProgress::operator==(const SBattleProgress& _other) const
	{
		return this->Reward == _other.Reward &&
			this->RuleId == _other.RuleId &&
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
		if (!_writer.Field("Reward", this->Reward, _error)) return false;
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
		if (_reader.GetSchemaVersion() < 4u) this->Reward = ::GameData::InGameData::SBattleRewardProgress{};
		if (!_reader.Field("Reward", this->Reward, _error, _reader.GetSchemaVersion() >= 4u)) return false;
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
