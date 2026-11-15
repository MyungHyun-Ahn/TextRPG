#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	[[nodiscard]] const std::wstring& GetLocalizedText(const GameData::FGameDataStore& store, const std::wstring& textKey, bool useEnglish = false) noexcept;
}
