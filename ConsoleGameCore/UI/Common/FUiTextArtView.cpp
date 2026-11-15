#include "pch.h"
#include "Rendering/FFrameBuffer.h"
#include "FUiElement.h"
#include "FUiTextArtView.h"
namespace TextRPG
{
	FUiTextArtView::FUiTextArtView(SCellRect rect) : FUiElement(rect) { SetPassThrough(true); }
	void FUiTextArtView::SetLines(std::vector<std::wstring> lines) { m_lines = std::move(lines); }
	void FUiTextArtView::Render(const SUiRenderContext& context) const
	{
		if (!m_isVisible)
		{
			return;
		}
		const auto rect = GetOuterRect(context);
		const auto visible = rect.Intersect(context.Clip).Intersect({0, 0, context.Frame.GetWidth(), context.Frame.GetHeight()});
		if (visible.Width <= 0 || visible.Height <= 0)
		{
			return;
		}
		const SUiRenderContext clipped{context.Frame, context.OriginX, context.OriginY, visible};
		for (int y = visible.Y; y < visible.Y + visible.Height; ++y)
		{
			const auto index = static_cast<std::size_t>(static_cast<std::int64_t>(y) - rect.Y);
			if (index < m_lines.size())
			{
				clipped.DrawText(rect.X, y, m_lines[index], 7, rect.Width);
			}
		}
	}
}
