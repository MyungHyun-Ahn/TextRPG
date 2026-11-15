#pragma once

namespace TextRPG
{
	class FUiElement : public IUI
	{
	public:
		explicit FUiElement(SCellRect rect);
		[[nodiscard]] SCellRect GetOuterRect(const SUiRenderContext& context) const noexcept;
		[[nodiscard]] virtual SCellRect GetContentRect(const SUiRenderContext& context) const noexcept;
		[[nodiscard]] SUiRenderContext GetChildContext(const SUiRenderContext& context) const noexcept;
		[[nodiscard]] FUiId GetId() const noexcept override;
		void AssignIds(FUiId& nextId) override;
		[[nodiscard]] IUI* Find(FUiId id) noexcept override;
		[[nodiscard]] IUI* HitTest(const SUiRenderContext& context, int x, int y) override;
		void CollectFocusable(std::vector<IUI*>& output, bool includeInactive = false) override;
		[[nodiscard]] bool IsVisible() const noexcept override;
		[[nodiscard]] bool IsEnabled() const noexcept override;
		[[nodiscard]] bool IsAvailable(FUiId id) const noexcept override;
		[[nodiscard]] bool ResolveGeometry(FUiId id, const SUiRenderContext& context, SUiGeometry& geometry) const override;
		void SetRect(SCellRect rect) noexcept;
		void SetVisible(bool visible) noexcept;
		void SetEnabled(bool enabled) noexcept;
		void SetPassThrough(bool passThrough) noexcept;

	protected:
		FUiId m_id = 0;
		bool m_isVisible = true;
		bool m_isEnabled = true;
		bool m_isPassThrough = false;
		SCellRect m_rect;
	};
}
