#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	class FMainMenuUi final : public FUiPanel
	{
	public:
		FMainMenuUi();
		[[nodiscard]] bool Prepare(const GameData::FGameDataStore& store, std::wstring& errorMessage);

	private:
		std::wstring m_logoArtId = L"MainLogo";
	};
}
