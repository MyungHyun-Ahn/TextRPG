#include "pch.h"
#include "Localization.h"

#include "Localization/LocalizationData.g.h"
#include "GameData/FGameDataStore.h"

namespace TextRPG
{
	const std::wstring& GetLocalizedText(const GameData::FGameDataStore& store, std::uint32_t textKey, bool useEnglish) noexcept
	{
		if (const GameData::Localization::SLocalizationData* row = store.FindLocalization(textKey))
		{
			return useEnglish ? row->enUS : row->koKR;
		}

		static const std::wstring kMissingText;
		return kMissingText;
	}
}
