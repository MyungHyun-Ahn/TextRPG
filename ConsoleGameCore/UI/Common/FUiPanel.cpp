#include "pch.h"
#include "Rendering/FFrameBuffer.h"
#include "FUiElement.h"
#include "FUiPanel.h"

namespace TextRPG
{
	FUiPanel::FUiPanel(SCellRect rect)
		: FUiElement(rect)
	{
	}
	void FUiPanel::SetTitle(std::wstring title)
	{
		m_title = std::move(title);
	}
	void FUiPanel::Render(const SUiRenderContext& context) const
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
		const std::wstring blank(static_cast<std::size_t>(visible.Width), L' ');
		for (int y = visible.Y; y < visible.Y + visible.Height; ++y)
		{
			context.DrawText(visible.X, y, blank);
		}
		context.DrawBorder(rect, m_title);
		const auto childContext = GetChildContext(context);
		for (const auto& child : m_children)
		{
			child->Render(childContext);
		}
	}
	SCellRect FUiPanel::GetContentRect(const SUiRenderContext& context) const noexcept
	{
		const SCellRect outer = GetOuterRect(context);
		if (outer.Width <= 2 || outer.Height <= 2 || outer.X == (std::numeric_limits<int>::max)() ||
			outer.Y == (std::numeric_limits<int>::max)())
		{
			return {};
		}
		return {outer.X + 1, outer.Y + 1, outer.Width - 2, outer.Height - 2};
	}
	bool FUiPanel::AddChild(std::unique_ptr<IUI> child)
	{
		if (!child || m_id != 0)
		{
			return false;
		}
		m_children.push_back(std::move(child));
		return true;
	}
	void FUiPanel::AssignIds(FUiId& nextId)
	{
		FUiElement::AssignIds(nextId);
		for (const auto& child : m_children)
		{
			child->AssignIds(nextId);
		}
	}
	IUI* FUiPanel::Find(FUiId id) noexcept
	{
		if (IUI* found = FUiElement::Find(id))
		{
			return found;
		}
		for (const auto& child : m_children)
		{
			if (IUI* found = child->Find(id))
			{
				return found;
			}
		}
		return nullptr;
	}
	IUI* FUiPanel::HitTest(const SUiRenderContext& context, int x, int y)
	{
		if (!m_isVisible)
		{
			return nullptr;
		}
		if (m_isEnabled)
		{
			const auto childContext = GetChildContext(context);
			for (auto child = m_children.rbegin(); child != m_children.rend(); ++child)
			{
				if (IUI* found = (*child)->HitTest(childContext, x, y))
				{
					return found;
				}
			}
		}
		return FUiElement::HitTest(context, x, y);
	}
	bool FUiPanel::ResolveGeometry(FUiId id, const SUiRenderContext& context, SUiGeometry& geometry) const
	{
		if (FUiElement::ResolveGeometry(id, context, geometry))
		{
			return true;
		}
		const auto childContext = GetChildContext(context);
		for (const auto& child : m_children)
		{
			if (child->ResolveGeometry(id, childContext, geometry))
			{
				return true;
			}
		}
		return false;
	}
	bool FUiPanel::IsAvailable(FUiId id) const noexcept
	{
		if (!m_isVisible || !m_isEnabled)
		{
			return false;
		}
		if (id == m_id)
		{
			return true;
		}
		for (const auto& child : m_children)
		{
			if (child->IsAvailable(id))
			{
				return true;
			}
		}
		return false;
	}
	void FUiPanel::CollectFocusable(std::vector<IUI*>& output, bool includeInactive)
	{
		if (!includeInactive && (!m_isVisible || !m_isEnabled))
		{
			return;
		}
		for (const auto& child : m_children)
		{
			child->CollectFocusable(output, includeInactive);
		}
	}
}
