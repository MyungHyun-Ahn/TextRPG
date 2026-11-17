#include "GameData/pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"

namespace GameData::InGameData
{
	bool RegisterInGameDataTypes(::TextRPG::FSerializableTypeRegistry& _registry, ::std::string& _error)
	{
		if (!_registry.Register<SAchievementProgress>(SAchievementProgress::kTypeName,
			[]()
			{
				return ::std::make_unique<SAchievementProgress>();
			}, _error)) return false;
		if (!_registry.Register<SBasicStats>(SBasicStats::kTypeName,
			[]()
			{
				return ::std::make_unique<SBasicStats>();
			}, _error)) return false;
		if (!_registry.Register<SBattleProgress>(SBattleProgress::kTypeName,
			[]()
			{
				return ::std::make_unique<SBattleProgress>();
			}, _error)) return false;
		if (!_registry.Register<SCharacterStats>(SCharacterStats::kTypeName,
			[]()
			{
				return ::std::make_unique<SCharacterStats>();
			}, _error)) return false;
		if (!_registry.Register<SCharacterProgress>(SCharacterProgress::kTypeName,
			[]()
			{
				return ::std::make_unique<SCharacterProgress>();
			}, _error)) return false;
		if (!_registry.Register<SInventoryItemProgress>(SInventoryItemProgress::kTypeName,
			[]()
			{
				return ::std::make_unique<SInventoryItemProgress>();
			}, _error)) return false;
		if (!_registry.Register<SRootInGameData>(SRootInGameData::kTypeName,
			[]()
			{
				return ::std::make_unique<SRootInGameData>();
			}, _error)) return false;
		return true;
	}
}
