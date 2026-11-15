#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	class FUiTextArt final : public FUiTextArtView
	{
	public:
		explicit FUiTextArt(SCellRect rect);
		[[nodiscard]] bool LoadFromGameData(const GameData::FGameDataStore& store, const std::wstring& artId,
											std::wstring& errorMessage);

	private:
		std::wstring m_artId;
	};
}
