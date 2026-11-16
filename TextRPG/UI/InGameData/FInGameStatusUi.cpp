#include "pch.h"
#include "UI/Common/FUiElement.h"
#include "UI/Common/FUiTextArtView.h"
#include "UI/Common/FUiTextArt.h"
#include "FInGameStatusUi.h"
#include "InGameDataAll.h"
#include "InGameData/InGameData.h"
#include "CharacterDefinition/CharacterDefinitionData.g.h"
#include "GameData/FGameDataStore.h"
#include "Localization/Localization.h"

namespace TextRPG
{
	FInGameStatusUi::FInGameStatusUi(const GameData::FGameDataStore& store, const InGameData& progress)
		: FUiElement({0, 0, 86, 43})
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
		const auto number = [this](const wchar_t* key, std::uint64_t value)
		{
			return GetLocalizedText(m_store, key) + L": " + std::to_wstring(value);
		};
		clipped.DrawText(rect.X + 2, rect.Y + 1, GetLocalizedText(m_store, m_nameTextKey) + L"  " +
			number(L"InGameData.Ui.Level", m_progress.GetCharacter().Level), 7, 38);
		clipped.DrawText(rect.X + 2, rect.Y + 2, number(L"InGameData.Ui.Hp", m_progress.GetCharacter().CurrentHp) +
			L"/" + std::to_wstring(stats.MaxHp), 7, 38);
		clipped.DrawText(rect.X + 2, rect.Y + 3, number(L"InGameData.Ui.Attack", stats.Attack) + L"  " +
			number(L"InGameData.Ui.Defense", stats.Defense), 7, 38);
		clipped.DrawText(rect.X + 45, rect.Y + 1, number(L"InGameData.Ui.Gold", m_progress.GetGold()), 7, 38);
		clipped.DrawText(rect.X + 45, rect.Y + 2, number(L"InGameData.Ui.Inventory", m_progress.GetInventory().size()), 7, 38);
		clipped.DrawText(rect.X + 45, rect.Y + 3, number(L"InGameData.Ui.Achievements", m_progress.GetAchievements().size()), 7, 38);
		if (m_art) m_art->Render(clipped);
	}
}
