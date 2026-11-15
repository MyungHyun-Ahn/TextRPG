#pragma once


namespace TextRPG
{
	class FUiPanel : public FUiElement
	{
	public:
		explicit FUiPanel(SCellRect rect);
		void SetTitle(std::wstring title);
		void Render(const SUiRenderContext& context) const override;
		[[nodiscard]] SCellRect GetContentRect(const SUiRenderContext& context) const noexcept override;
		// 등록 전 조립 전용이다. 등록 후 트리 변경은 루트 명령으로 제한한다.
		bool AddChild(std::unique_ptr<IUI> child);
		[[nodiscard]] bool ResolveGeometry(FUiId id, const SUiRenderContext& context, SUiGeometry& geometry) const override;
		void AssignIds(FUiId& nextId) override;
		[[nodiscard]] IUI* Find(FUiId id) noexcept override;
		[[nodiscard]] IUI* HitTest(const SUiRenderContext& context, int x, int y) override;
		void CollectFocusable(std::vector<IUI*>& output, bool includeInactive = false) override;
		[[nodiscard]] bool IsAvailable(FUiId id) const noexcept override;

	private:
		std::vector<std::unique_ptr<IUI>> m_children;
		std::wstring m_title;
	};
}
