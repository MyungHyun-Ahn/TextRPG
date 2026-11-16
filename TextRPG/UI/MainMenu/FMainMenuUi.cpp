#include "pch.h"
#include "Common/GameDataEnums.g.h"
#include "UI/Common/FUiElement.h"
#include "UI/Common/FUiPanel.h"
#include "UI/Common/FUiTextArtView.h"
#include "UI/Common/FUiTextArt.h"
#include "FMainMenuUi.h"
#include "GameData/FGameDataStore.h"
#if __has_include("TextArt/TextArtData.g.h")
#include "TextArt/TextArtData.g.h"
#endif

namespace TextRPG
{
	FMainMenuUi::FMainMenuUi()
		: FUiPanel({0, 0, 85, 43})
		, m_logoArtId(static_cast<std::uint32_t>(GameData::Common::EArtId::MainLogo))
	{
	}
	bool FMainMenuUi::Prepare(const GameData::FGameDataStore& store, std::wstring& errorMessage)
	{
		// 선택적인 카탈로그가 없으면 Scene 자체를 실패시키지 않는다.
		if (!store.FindTextArt(m_logoArtId))
		{
			return true;
		}
		auto logo = std::make_unique<FUiTextArt>(SCellRect{0, 0, 83, 41});
		if (!logo->LoadFromGameData(store, m_logoArtId, errorMessage))
		{
			return false;
		}
		return AddChild(std::move(logo));
	}
}
