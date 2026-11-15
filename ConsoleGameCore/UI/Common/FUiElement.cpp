#include "pch.h"
#include "Rendering/FFrameBuffer.h"
#include "FUiElement.h"

namespace TextRPG
{
	bool SCellRect::Contains(int x, int y) const noexcept
	{
		return Width > 0 && Height > 0 && x >= X && y >= Y &&
			static_cast<std::int64_t>(x) < static_cast<std::int64_t>(X) + Width &&
			static_cast<std::int64_t>(y) < static_cast<std::int64_t>(Y) + Height;
	}
	SCellRect SCellRect::Intersect(SCellRect other) const noexcept
	{
		const int x = std::max(X, other.X);
		const int y = std::max(Y, other.Y);
		const auto right = std::min(static_cast<std::int64_t>(X) + Width, static_cast<std::int64_t>(other.X) + other.Width);
		const auto bottom = std::min(static_cast<std::int64_t>(Y) + Height, static_cast<std::int64_t>(other.Y) + other.Height);
		if (Width <= 0 || Height <= 0 || other.Width <= 0 || other.Height <= 0 || right <= x || bottom <= y)
		{
			return {};
		}
		return {x, y, static_cast<int>(right - x), static_cast<int>(bottom - y)};
	}
	void SUiRenderContext::DrawText(int x, int y, const std::wstring& text, std::uint16_t attributes, int width) const
	{
		const auto clip = Clip.Intersect({0, 0, Frame.GetWidth(), Frame.GetHeight()});
		if (clip.Width <= 0 || clip.Height <= 0 || y < clip.Y || static_cast<std::int64_t>(y) >=
			static_cast<std::int64_t>(clip.Y) + clip.Height)
		{
			return;
		}
		const auto right = static_cast<std::int64_t>(clip.X) + clip.Width;
		std::int64_t offset = 0;
		for (std::size_t index = 0; index < text.size(); ++index)
		{
			if (static_cast<std::int64_t>(x) + offset >= right)
			{
				break;
			}
			std::wstring glyph(1, text[index]);
			if (text[index] >= 0xD800 && text[index] <= 0xDBFF && index + 1 < text.size())
			{
				glyph += text[++index];
			}
			if (glyph == L"\n" || glyph == L"\r")
			{
				glyph = L" ";
			}
			const int cells = FFrameBuffer::GetDisplayCellWidth(glyph);
			if (width >= 0 && offset + cells > width)
			{
				break;
			}
			const auto targetX = static_cast<std::int64_t>(x) + offset;
			if (targetX >= clip.X && targetX + cells <= right)
			{
				Frame.DrawText(static_cast<int>(targetX), y, glyph, attributes, cells);
			}
			offset += cells;
		}
	}
	void SUiRenderContext::DrawBorder(SCellRect rect, const std::wstring& title, std::uint16_t attributes) const
	{
		const auto visible = rect.Intersect(Clip).Intersect({0, 0, Frame.GetWidth(), Frame.GetHeight()});
		if (rect.Width < 2 || rect.Height < 2 || visible.Width <= 0 || visible.Height <= 0)
		{
			return;
		}
		const auto right = static_cast<std::int64_t>(rect.X) + rect.Width - 1;
		const auto bottom = static_cast<std::int64_t>(rect.Y) + rect.Height - 1;
		for (int y = visible.Y; y < visible.Y + visible.Height; ++y)
		{
			for (int x = visible.X; x < visible.X + visible.Width; ++x)
			{
				wchar_t glyph = 0;
				if (y == rect.Y)
				{
					glyph = x == rect.X ? L'┌' : (x == right ? L'┐' : L'─');
				}
				else if (y == bottom)
				{
					glyph = x == rect.X ? L'└' : (x == right ? L'┘' : L'─');
				}
				else if (x == rect.X || x == right)
				{
					glyph = L'│';
				}
				if (glyph != 0)
				{
					Frame.DrawText(x, y, std::wstring(1, glyph), attributes, 1);
				}
			}
		}
		const auto titleX = static_cast<std::int64_t>(rect.X) + 2;
		if (rect.Width > 3 && titleX <= (std::numeric_limits<int>::max)())
		{
			DrawText(static_cast<int>(titleX), rect.Y, title, attributes, rect.Width - 3);
		}
	}
	FUiElement::FUiElement(SCellRect rect)
		: m_rect(rect)
	{
	}
	SCellRect FUiElement::GetOuterRect(const SUiRenderContext& context) const noexcept
	{
		const auto x = static_cast<std::int64_t>(context.OriginX) + m_rect.X;
		const auto y = static_cast<std::int64_t>(context.OriginY) + m_rect.Y;
		if (m_rect.Width <= 0 || m_rect.Height <= 0 ||
			x < (std::numeric_limits<int>::min)() || x > (std::numeric_limits<int>::max)() ||
			y < (std::numeric_limits<int>::min)() || y > (std::numeric_limits<int>::max)())
		{
			return {};
		}
		return {static_cast<int>(x), static_cast<int>(y), m_rect.Width, m_rect.Height};
	}
	SCellRect FUiElement::GetContentRect(const SUiRenderContext& context) const noexcept
	{
		return GetOuterRect(context);
	}
	SUiRenderContext FUiElement::GetChildContext(const SUiRenderContext& context) const noexcept
	{
		const SCellRect content = GetContentRect(context);
		return {context.Frame, content.X, content.Y, context.Clip.Intersect(content)};
	}
	FUiId FUiElement::GetId() const noexcept
	{
		return m_id;
	}
	void FUiElement::AssignIds(FUiId& nextId)
	{
		if (m_id == 0)
		{
			m_id = nextId++;
		}
	}
	IUI* FUiElement::Find(FUiId id) noexcept
	{
		return id == m_id ? this : nullptr;
	}
	IUI* FUiElement::HitTest(const SUiRenderContext& context, int x, int y)
	{
		return m_isVisible && !m_isPassThrough && GetOuterRect(context).Intersect(context.Clip).Contains(x, y) ? this : nullptr;
	}
	void FUiElement::CollectFocusable(std::vector<IUI*>& output, bool includeInactive)
	{
		if (((m_isVisible && m_isEnabled) || includeInactive) && IsFocusable())
		{
			output.push_back(this);
		}
	}
	bool FUiElement::IsAvailable(FUiId id) const noexcept
	{
		return id == m_id && m_isVisible && m_isEnabled;
	}
	bool FUiElement::IsVisible() const noexcept
	{
		return m_isVisible;
	}
	bool FUiElement::IsEnabled() const noexcept
	{
		return m_isEnabled;
	}
	bool FUiElement::ResolveGeometry(FUiId id, const SUiRenderContext& context, SUiGeometry& geometry) const
	{
		if (id != m_id)
		{
			return false;
		}
		geometry.Outer = GetOuterRect(context);
		geometry.Clip = geometry.Outer.Intersect(context.Clip);
		return true;
	}
	void FUiElement::SetRect(SCellRect rect) noexcept
	{
		m_rect = rect;
	}
	void FUiElement::SetVisible(bool visible) noexcept
	{
		m_isVisible = visible;
	}
	void FUiElement::SetEnabled(bool enabled) noexcept
	{
		m_isEnabled = enabled;
	}
	void FUiElement::SetPassThrough(bool passThrough) noexcept
	{
		m_isPassThrough = passThrough;
	}
}
