#include "pch.h"
#include "Common/GameDataEnums.g.h"
#include "UI/Common/FUiElement.h"
#include "UI/Common/FUiTextArtView.h"
#include "UI/Common/FUiTextArt.h"
#include "FInGameStatusUi.h"
#include "InGameDataAll.h"
#include "InGameData/InGameData.h"
#include "CharacterDefinition/CharacterDefinitionData.g.h"
#include "GameData/FGameDataStore.h"
#include "Localization/Localization.h"
#include "MonsterDefinition/MonsterDefinitionData.g.h"
#include "CombatRule/CombatRuleData.g.h"
#include "Combat/FCombat.h"

namespace TextRPG
{
	using ETextKey = GameData::Common::ETextKey;
	FInGameStatusUi::FInGameStatusUi(const GameData::FGameDataStore& store, const InGameData& progress)
		: FUiElement({0, 0, 132, 43})
		, m_store(store)
		, m_progress(progress)
	{
		SetPassThrough(true);
	}

	FInGameStatusUi::~FInGameStatusUi() = default;

	bool FInGameStatusUi::Prepare(std::wstring& errorMessage)
	{
		const auto* definition = m_store.FindCharacterDefinition(m_progress.GetCharacter().CharacterId);
		SCharacterStats stats;
		std::string error;
		if (!definition || !m_store.FindLocalization(definition->nameTextKey) || !m_progress.GetStats(m_store, stats, error))
		{
			errorMessage = L"Character display definition is unavailable.";
			return false;
		}
		std::unique_ptr<FUiTextArt> art;
		// TextArt 카탈로그 자체가 없는 기존 배포/기하 fixture는 그대로 허용한다.
		if (m_store.FindTextArt(definition->artId))
		{
			art = std::make_unique<FUiTextArt>(SCellRect{3, 6, 36, 36});
			if (!art->LoadFromGameData(m_store, definition->artId, errorMessage))
			{
				return false;
			}
		}
		std::unique_ptr<FUiTextArt> monsterArt;
		if (m_progress.GetBattle().RuleId != 0)
		{
			const auto* monster = m_store.FindMonsterDefinition(m_progress.GetBattle().MonsterId);
			if (!monster)
			{
				errorMessage = L"Monster display definition is unavailable.";
				return false;
			}
			monsterArt = std::make_unique<FUiTextArt>(SCellRect{46, 6, 36, 36});
			if (!monsterArt->LoadFromGameData(m_store, monster->artId, errorMessage)) return false;
		}
		m_monsterArt = std::move(monsterArt);
		m_nameTextKey = definition->nameTextKey;
		m_art = std::move(art);
		errorMessage.clear();
		return true;
	}

	void FInGameStatusUi::Render(const SUiRenderContext& context) const
	{
		if (!m_isVisible) return;
		const auto rect = GetOuterRect(context);
		const SUiRenderContext clipped{context.Frame, context.OriginX, context.OriginY, rect.Intersect(context.Clip)};
		SCharacterStats stats;
		std::string error;
		if (!m_progress.GetStats(m_store, stats, error)) return;
		const auto number = [this](ETextKey key, std::uint64_t value)
		{
			return GetLocalizedText(m_store, static_cast<std::uint32_t>(key)) + L": " + std::to_wstring(value);
		};
		clipped.DrawText(rect.X + 2, rect.Y + 1, GetLocalizedText(m_store, m_nameTextKey) + L"  " +
			number(ETextKey::InGameData_Ui_Level, m_progress.GetCharacter().Level), 7, 38);
		clipped.DrawText(rect.X + 2, rect.Y + 2, number(ETextKey::InGameData_Ui_Hp, m_progress.GetCharacter().CurrentHp) +
			L"/" + std::to_wstring(stats.MaxHp), 7, 38);
		clipped.DrawText(rect.X + 2, rect.Y + 3, number(ETextKey::InGameData_Ui_Attack, stats.Attack) + L"  " +
			number(ETextKey::InGameData_Ui_Defense, stats.Defense), 7, 38);
		const auto& battle = m_progress.GetBattle();
		if (battle.RuleId != 0)
		{
			const auto* monster = m_store.FindMonsterDefinition(battle.MonsterId);
			const auto* rule = m_store.FindCombatRule(battle.RuleId);
			if (!monster || !rule) return;
			const auto text = [this](std::uint32_t key)
			{
				return GetLocalizedText(m_store, key);
			};
			clipped.DrawText(45, 1, text(monster->nameTextKey), 7, 38);
			clipped.DrawText(45, 2, number(ETextKey::InGameData_Ui_Hp, battle.MonsterHp) +
				L"/" + std::to_wstring(monster->maxHp), 7, 38);
			clipped.DrawText(45, 3, text(rule->roundTextKey) + L": " + std::to_wstring(battle.Round), 7, 38);
			SCombatStats derived;
			if (FCombat::GetPlayerStats(m_store, m_progress, *rule, derived, error))
				clipped.DrawText(2, 3, number(ETextKey::InGameData_Ui_Attack, derived.m_attack) + L"  " +
					number(ETextKey::InGameData_Ui_Defense, derived.m_defense), 7, 38);
			if (battle.HitCount == 0) clipped.DrawText(88, 1, text(rule->readyTextKey), 7, 42);
			for (std::uint32_t hit = 0; hit < battle.HitCount; ++hit)
			{
				const bool isPlayer = hit == 0 ? battle.FirstIsPlayer : !battle.FirstIsPlayer;
				const bool critical = hit == 0 ? battle.FirstCritical : battle.SecondCritical;
				const auto damage = hit == 0 ? battle.FirstDamage : battle.SecondDamage;
				clipped.DrawText(88, 1 + static_cast<int>(hit) * 3, text(isPlayer ? m_nameTextKey : monster->nameTextKey), 7, 42);
				clipped.DrawText(88, 2 + static_cast<int>(hit) * 3, text(critical ? rule->criticalTextKey : rule->hitTextKey) +
					L": " + std::to_wstring(damage), 7, 42);
			}
			if (battle.Outcome != 0) clipped.DrawText(88, 8, text(battle.Outcome == 1 ? rule->victoryTextKey : rule->defeatTextKey), 7, 42);
			if (m_monsterArt) m_monsterArt->Render(clipped);
		}
		else
		{
			clipped.DrawText(rect.X + 45, rect.Y + 1, number(ETextKey::InGameData_Ui_Gold, m_progress.GetGold()), 7, 38);
			clipped.DrawText(rect.X + 45, rect.Y + 2, number(ETextKey::InGameData_Ui_Inventory, m_progress.GetInventory().size()), 7, 38);
			clipped.DrawText(rect.X + 45, rect.Y + 3,
				number(ETextKey::InGameData_Ui_Achievements, m_progress.GetAchievements().size()), 7, 38);
		}
		if (m_art) m_art->Render(clipped);
	}
}
