#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	[[nodiscard]] const std::wstring& GetLocalizedText(const GameData::FGameDataStore& store, std::uint32_t textKey,
		bool useEnglish = false) noexcept;
}
