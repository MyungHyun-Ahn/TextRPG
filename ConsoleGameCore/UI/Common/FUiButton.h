#pragma once

namespace TextRPG
{
	class FUiButton : public FUiElement
	{
	public:
		FUiButton(SCellRect rect, std::wstring text, std::function<void()> activate, int shortcut = 0);
		void Render(const SUiRenderContext& context) const override;
		[[nodiscard]] bool IsFocusable() const noexcept override;
		[[nodiscard]] int GetShortcut() const noexcept override;
		void SetFocused(bool focused) override;
		EUiReply OnPointerDown(const SInputEvent& event) override;
		EUiReply OnClick(const SInputEvent& event) override;
		EUiReply OnKeyDown(const SInputEvent& event) override;

	private:
		EUiReply Activate();
		std::wstring m_text;
		std::function<void()> m_activate;
		int m_shortcut = 0;
		bool m_isFocused = false;
	};
}
