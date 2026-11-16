#include "pch.h"
#include "Rendering/FFrameBuffer.h"
#include "UI/Common/FUiElement.h"
#include "UI/Common/FUiTextArtView.h"
#include "FUiTextArt.h"
#include "GameData/FGameDataStore.h"
#if __has_include("TextArt/TextArtData.g.h")
#include "TextArt/TextArtData.g.h"
#endif

namespace TextRPG
{
	FUiTextArt::FUiTextArt(SCellRect rect)
		: FUiTextArtView(rect)
	{
		SetPassThrough(true);
	}
	bool FUiTextArt::LoadFromGameData(const GameData::FGameDataStore& store, const std::wstring& artId, std::wstring& errorMessage)
	{
		const auto* row = store.FindTextArt(artId);
		if (!row || row->width != static_cast<std::uint32_t>(m_rect.Width) || row->height != static_cast<std::uint32_t>(m_rect.Height))
		{
			errorMessage = L"TextArt is missing or does not fit the UI slot.";
			return false;
		}
		const auto* bytes = store.FindAsset(row->assetPath);
		if (!bytes)
		{
			errorMessage = L"TextArt AssetPath is missing from the package.";
			return false;
		}
		std::vector<std::wstring> lines;
		std::string error;
		if (!GameData::DecodeTextArt(*bytes, row->width, row->height, lines, error))
		{
			errorMessage.assign(error.begin(), error.end());
			return false;
		}
		SetLines(std::move(lines));
		errorMessage.clear();
		return true;
	}
}
