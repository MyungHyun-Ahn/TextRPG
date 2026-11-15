#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	class InGameData;
	class FUiTextArt;

	// 진행 원본은 앱 수명 소유자에 있다. 이 뷰는 Scene보다 오래 사는 원본만 조회한다.
	class FInGameStatusUi final : public FUiElement
	{
	public:
		FInGameStatusUi(const GameData::FGameDataStore& store, const InGameData& progress);
		~FInGameStatusUi() override;
		[[nodiscard]] bool Prepare(std::wstring& errorMessage);
		void Render(const SUiRenderContext& context) const override;

	private:
		const GameData::FGameDataStore& m_store;
		const InGameData& m_progress;
		std::wstring m_nameTextKey;
		std::unique_ptr<FUiTextArt> m_art;
	};
}
