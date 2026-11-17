#include "pch.h"
#include "CharacterCombat/CharacterCombatData.g.h"
#include "MonsterDefinition/MonsterDefinitionData.g.h"
#include "CombatRule/CombatRuleData.g.h"
#include "InGameDataAll.h"
#include "InGameData/InGameData.h"
#include "GameData/FGameDataStore.h"
#include "Math/Random/FSelector.h"
#include "Combat/FCombat.h"

namespace TextRPG
{
	namespace
	{
		using SBasicStats = GameData::InGameData::SBasicStats;
		using SBattleProgress = GameData::InGameData::SBattleProgress;
		using SRule = GameData::CombatRule::SCombatRuleData;

		std::uint32_t Narrow(std::uint64_t value)
		{
			if (value > UINT32_MAX) throw std::runtime_error("Combat arithmetic exceeds uint32");
			return static_cast<std::uint32_t>(value);
		}
		std::uint64_t Add(std::uint64_t left, std::uint64_t right)
		{
			if (right > UINT64_MAX - left) throw std::runtime_error("Combat arithmetic exceeds uint64");
			return left + right;
		}
		SCombatStats Derive(const SBasicStats& basic, std::uint32_t attack, std::uint32_t defense, const SRule& rule)
		{
			if (rule.scale == 0 || basic.Strength > rule.maxStat || basic.Dexterity > rule.maxStat ||
				basic.Intelligence > rule.maxStat || basic.Luck > rule.maxStat)
				throw std::runtime_error("Combat original stats exceed rule limit");
			std::uint64_t contribution = static_cast<std::uint64_t>(basic.Strength) * rule.strengthCoefficient;
			contribution = Add(contribution, static_cast<std::uint64_t>(basic.Dexterity) * rule.dexterityCoefficient);
			contribution = Add(contribution, static_cast<std::uint64_t>(basic.Intelligence) * rule.intelligenceCoefficient);
			contribution = Add(contribution, static_cast<std::uint64_t>(basic.Luck) * rule.luckCoefficient);
			return {Narrow(Add(attack, contribution / rule.scale)),
				Narrow(Add(defense, static_cast<std::uint64_t>(basic.Strength) * rule.strengthDefense / rule.scale)),
				Narrow(Add(rule.baseSpeed, static_cast<std::uint64_t>(basic.Dexterity) * rule.dexteritySpeed / rule.scale)),
				static_cast<std::uint32_t>((std::min)(static_cast<std::uint64_t>(rule.criticalCap),
					static_cast<std::uint64_t>(basic.Luck) * rule.luckCritical))};
		}
		SCombatStats MonsterStats(const GameData::MonsterDefinition::SMonsterDefinitionData& monster, const SRule& rule)
		{
			return Derive({monster.strength, monster.dexterity, monster.intelligence, monster.luck}, monster.attack, monster.defense, rule);
		}
		bool Roll(FSelector& selector, std::uint32_t yes, std::uint32_t no, bool& result, std::string& error)
		{
			FProcTable table;
			SSelectResult selected;
			if (!table.AddProc(1, yes, error) || !table.AddProc(2, no, error) || !table.Prepare(error) ||
				!selector.SelectKey(table, selected, error)) return false;
			result = selected.key == 1;
			return true;
		}
		bool Hit(const SCombatStats& attacker, const SCombatStats& defender, const SRule& rule, FSelector& selector,
			std::uint32_t& hp, std::uint32_t& damage, bool& critical, std::string& error)
		{
			if (!Roll(selector, attacker.m_criticalChance, rule.scale - attacker.m_criticalChance, critical, error)) return false;
			const auto normal = (std::max)(rule.minimumDamage,
				attacker.m_attack > defender.m_defense ? attacker.m_attack - defender.m_defense : 0U);
			const std::uint64_t adjusted = critical ? static_cast<std::uint64_t>(normal) * rule.criticalMultiplier / rule.scale : normal;
			damage = static_cast<std::uint32_t>((std::min)(static_cast<std::uint64_t>(hp), adjusted));
			hp -= damage;
			return true;
		}
	}

	bool FCombat::GetPlayerStats(const GameData::FGameDataStore& store, const InGameData& progress,
		const GameData::CombatRule::SCombatRuleData& rule, SCombatStats& output, std::string& error)
	{
		try
		{
			const auto* definition = store.FindCharacterCombat(progress.GetCharacter().CharacterId);
			SCharacterStats legacy;
			if (!definition || !progress.GetStats(store, legacy, error))
			{
				if (!definition) error = "CharacterCombat definition is missing";
				return false;
			}
			const auto& bonus = progress.m_data.m_basicBonuses;
			const SBasicStats basic{Narrow(Add(definition->strength, bonus.Strength)),
				Narrow(Add(definition->dexterity, bonus.Dexterity)), Narrow(Add(definition->intelligence, bonus.Intelligence)),
				Narrow(Add(definition->luck, bonus.Luck))};
			output = Derive(basic, legacy.Attack, legacy.Defense, rule);
			error.clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}

	bool FCombat::Validate(const GameData::FGameDataStore& store, const InGameData& progress, std::string& error)
	{
		try
		{
			const auto& battle = progress.GetBattle();
			if (battle.RuleId == 0)
			{
				if (!(battle == SBattleProgress{})) throw std::runtime_error("Inactive combat state is not empty");
				if (!store.GetCombatRules().empty())
				{
					for (const auto& rule : store.GetCombatRules())
					{
						SCombatStats stats;
						if (!GetPlayerStats(store, progress, rule, stats, error)) return false;
					}
				}
				error.clear();
				return true;
			}
			const auto* rule = store.FindCombatRule(battle.RuleId);
			const auto* monster = store.FindMonsterDefinition(battle.MonsterId);
			if (!rule || !monster || rule->monsterId != battle.MonsterId || progress.GetSceneId() != rule->battleSceneId)
				throw std::runtime_error("Combat state definition or scene mismatch");
			SCombatStats stats;
			if (!GetPlayerStats(store, progress, *rule, stats, error)) return false;
			(void)MonsterStats(*monster, *rule);
			const auto hp = progress.GetCharacter().CurrentHp;
			if (battle.Outcome > 2 || battle.MonsterHp > monster->maxHp || battle.Round > rule->maxRounds || battle.HitCount > 2 ||
				(battle.Outcome == 0 && (hp == 0 || battle.MonsterHp == 0 || battle.Round == rule->maxRounds ||
					static_cast<std::uint64_t>(battle.MonsterHp) >
						static_cast<std::uint64_t>(rule->minimumDamage) * (rule->maxRounds - battle.Round))) ||
				(battle.Outcome == 1 && (hp == 0 || battle.MonsterHp != 0)) ||
				(battle.Outcome == 2 && (hp != 0 || battle.MonsterHp == 0)) ||
				(battle.Round == 0 && (battle.HitCount != 0 || battle.RandCount != 0 || battle.MonsterHp != monster->maxHp ||
					battle.FirstIsPlayer || battle.FirstCritical || battle.SecondCritical ||
					battle.FirstDamage != 0 || battle.SecondDamage != 0)) ||
				(battle.Round != 0 && (battle.HitCount == 0 || battle.RandCount < battle.Round || battle.FirstDamage == 0)) ||
				(battle.HitCount == 1 && (battle.Outcome == 0 || battle.SecondDamage != 0 || battle.SecondCritical)) ||
				(battle.HitCount == 2 && battle.SecondDamage == 0))
				throw std::runtime_error("Combat state bounds or outcome are invalid");
			error.clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}

	bool FCombat::Travel(const GameData::FGameDataStore& store, InGameData& candidate,
		std::uint32_t targetSceneId, std::string& error)
	{
		try
		{
			if (!candidate.Validate(store, error)) return false;
			auto& battle = candidate.m_data.m_battle;
			if (battle.RuleId != 0)
			{
				const auto* rule = store.FindCombatRule(battle.RuleId);
				if (!rule || battle.Outcome == 0 || targetSceneId != rule->townSceneId)
					throw std::runtime_error("Return requires a finished battle and its town");
				SCharacterStats stats;
				if (!candidate.GetStats(store, stats, error)) return false;
				if (rule->healOnReturn != 0) candidate.m_data.m_character->CurrentHp = stats.MaxHp;
				battle = {};
				candidate.m_data.m_sceneId = targetSceneId;
			}
			else
			{
				const auto& rules = store.GetCombatRules();
				const auto found = std::find_if(rules.begin(), rules.end(), [&](const auto& rule)
				{
					return rule.townSceneId == candidate.GetSceneId() && rule.battleSceneId == targetSceneId;
				});
				if (found == rules.end() || candidate.GetCharacter().CurrentHp == 0)
					throw std::runtime_error("Travel does not identify a live encounter");
				const auto* monster = store.FindMonsterDefinition(found->monsterId);
				if (!monster) throw std::runtime_error("Monster definition is missing");
				SCombatStats stats;
				if (!GetPlayerStats(store, candidate, *found, stats, error)) return false;
				(void)MonsterStats(*monster, *found);
				battle.RuleId = found->ruleId;
				battle.MonsterId = monster->monsterId;
				battle.MonsterHp = monster->maxHp;
				battle.Seed = GenerateSeed();
				candidate.m_data.m_sceneId = targetSceneId;
			}
			return candidate.Validate(store, error);
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}

	bool FCombat::BasicAttack(const GameData::FGameDataStore& store, InGameData& progress, std::string& error)
	{
		try
		{
			if (!progress.Validate(store, error)) return false;
			auto battle = progress.GetBattle();
			const auto* rule = store.FindCombatRule(battle.RuleId);
			const auto* monster = store.FindMonsterDefinition(battle.MonsterId);
			if (!rule || !monster || battle.Outcome != 0 || battle.Round >= rule->maxRounds)
				throw std::runtime_error("Basic attack requires an unfinished battle within its round limit");
			SCombatStats player;
			if (!GetPlayerStats(store, progress, *rule, player, error)) return false;
			const auto enemy = MonsterStats(*monster, *rule);
			FSelector selector(battle.Seed, battle.RandCount);
			if (!Roll(selector, player.m_speed, enemy.m_speed, battle.FirstIsPlayer, error)) return false;
			std::uint32_t playerHp = progress.GetCharacter().CurrentHp;
			battle.FirstDamage = battle.SecondDamage = 0;
			battle.FirstCritical = battle.SecondCritical = false;
			const auto hit = [&](bool isPlayer, std::uint32_t& damage, bool& critical)
			{
				return Hit(isPlayer ? player : enemy, isPlayer ? enemy : player, *rule, selector,
					isPlayer ? battle.MonsterHp : playerHp, damage, critical, error);
			};
			if (!hit(battle.FirstIsPlayer, battle.FirstDamage, battle.FirstCritical)) return false;
			battle.HitCount = 1;
			if (playerHp != 0 && battle.MonsterHp != 0)
			{
				if (!hit(!battle.FirstIsPlayer, battle.SecondDamage, battle.SecondCritical)) return false;
				battle.HitCount = 2;
			}
			++battle.Round;
			battle.Outcome = battle.MonsterHp == 0 ? 1U : (playerHp == 0 ? 2U : 0U);
			battle.RandCount = selector.GetState().randCount;
			// 여기까지 값 후보만 변경한다. 공유 캐릭터의 HP는 모든 실패 가능 처리가 끝난 뒤 게시한다.
			progress.m_data.m_battle = battle;
			progress.m_data.m_character->CurrentHp = playerHp;
			error.clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			error = exception.what();
			return false;
		}
	}
}
