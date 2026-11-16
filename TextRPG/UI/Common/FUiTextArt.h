#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	class FUiTextArt final : public FUiTextArtView
	{
	public:
		explicit FUiTextArt(SCellRect rect);
		[[nodiscard]] bool LoadFromGameData(const GameData::FGameDataStore& store, std::uint32_t artId,
											std::wstring& errorMessage);
	};
}
