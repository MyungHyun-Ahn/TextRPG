#pragma once

namespace GameData
{
	class FGameDataStore;
	namespace CombatRule
	{
		struct SCombatRuleData;
	}
	namespace InGameData
	{
		struct SBasicStats;
		struct SBattleProgress;
	}
}

namespace TextRPG
{
	class InGameData;
	struct SCombatStats final
	{
		std::uint32_t m_attack = 0;
		std::uint32_t m_defense = 0;
		std::uint32_t m_speed = 0;
		std::uint32_t m_criticalChance = 0;
	};

	// 기본 공격은 양쪽에 같은 경로를 쓴다. 미래 행동의 비용/쿨타임/효과는 이 확정 경계 앞에서 검증한다.
	class FCombat final
	{
	public:
		[[nodiscard]] static bool GetPlayerStats(const GameData::FGameDataStore& store, const InGameData& progress,
			const GameData::CombatRule::SCombatRuleData& rule, SCombatStats& output, std::string& error);
		[[nodiscard]] static bool Validate(const GameData::FGameDataStore& store, const InGameData& progress, std::string& error);
		[[nodiscard]] static bool Travel(const GameData::FGameDataStore& store, InGameData& candidate,
			std::uint32_t targetSceneId, std::string& error);
		[[nodiscard]] static bool BasicAttack(const GameData::FGameDataStore& store, InGameData& progress, std::string& error);
	};
}
