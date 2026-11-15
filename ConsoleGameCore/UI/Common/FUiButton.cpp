#include "pch.h"
#include "FUiElement.h"
#include "FUiButton.h"

namespace TextRPG
{
	FUiButton::FUiButton(SCellRect rect, std::wstring text, std::function<void()> activate, int shortcut)
		: FUiElement(rect)
		, m_text(std::move(text))
		, m_activate(std::move(activate))
		, m_shortcut(shortcut)
	{
	}
	void FUiButton::Render(const SUiRenderContext& context) const
	{
		if (!m_isVisible)
		{
			return;
		}
		const auto rect = GetOuterRect(context);
		const auto visible = rect.Intersect(context.Clip);
		if (visible.Width <= 0 || visible.Height <= 0)
		{
			return;
		}
		const std::uint16_t attributes = !m_isEnabled ? 8 : (m_isFocused ? 0x1E : 7);
		context.DrawBorder(rect, {}, attributes);
		if (rect.Width > 2 && rect.Height > 2 && rect.X < (std::numeric_limits<int>::max)() &&
			rect.Y < (std::numeric_limits<int>::max)())
		{
			const SUiRenderContext clipped{context.Frame, context.OriginX, context.OriginY, visible};
			clipped.DrawText(rect.X + 1, rect.Y + 1, m_text, attributes, rect.Width - 2);
		}
	}
	bool FUiButton::IsFocusable() const noexcept
	{
		return true;
	}
	int FUiButton::GetShortcut() const noexcept
	{
		return m_shortcut;
	}
	void FUiButton::SetFocused(bool focused)
	{
		m_isFocused = focused;
	}
	EUiReply FUiButton::OnPointerDown(const SInputEvent&)
	{
		return EUiReply::Capture;
	}
	EUiReply FUiButton::OnClick(const SInputEvent&)
	{
		return Activate();
	}
	EUiReply FUiButton::Activate()
	{
		if (m_isEnabled && m_activate)
		{
			m_activate();
			return EUiReply::Handled;
		}
		return EUiReply::Unhandled;
	}
	EUiReply FUiButton::OnKeyDown(const SInputEvent& event)
	{
		if (event.Key == EInputKey::Enter ||
			(m_shortcut > 0 && static_cast<int>(event.Key) - static_cast<int>(EInputKey::Digit1) + 1 == m_shortcut))
		{
			return Activate();
		}
		return EUiReply::Unhandled;
	}
}
