#pragma once

namespace TextRPG
{
	using FUiId = std::uint64_t;
	class FFrameBuffer;
	struct SCellRect final
	{
		int X = 0;
		int Y = 0;
		int Width = 0;
		int Height = 0;
		bool operator==(const SCellRect&) const = default;
		[[nodiscard]] bool Contains(int x, int y) const noexcept;
		[[nodiscard]] SCellRect Intersect(SCellRect other) const noexcept;
	};
	struct SUiGeometry final
	{
		SCellRect Outer;
		SCellRect Clip;
		bool operator==(const SUiGeometry&) const = default;
	};
	struct SUiRenderContext final
	{
		FFrameBuffer& Frame;
		int OriginX;
		int OriginY;
		SCellRect Clip;
		void DrawText(int x, int y, const std::wstring& text, std::uint16_t attributes = 7, int width = -1) const;
		void DrawBorder(SCellRect rect, const std::wstring& title = {}, std::uint16_t attributes = 7) const;
	};
	enum class EUiReply
	{
		Unhandled,
		Handled,
		Capture
	};
	enum class EUiDragPhase
	{
		Begin,
		Move,
		End,
		Cancel
	};
	class IUI
	{
	public:
		virtual ~IUI() = default;
		virtual void Render(const SUiRenderContext& context) const = 0;
		[[nodiscard]] virtual FUiId GetId() const noexcept = 0;
		virtual void AssignIds(FUiId& nextId) = 0;
		[[nodiscard]] virtual IUI* Find(FUiId id) noexcept = 0;
		[[nodiscard]] virtual IUI* HitTest(const SUiRenderContext& context, int x, int y) = 0;
		[[nodiscard]] virtual bool ResolveGeometry(FUiId id, const SUiRenderContext& context, SUiGeometry& geometry) const = 0;
		virtual void CollectFocusable(std::vector<IUI*>& output, bool includeInactive = false) = 0;
		[[nodiscard]] virtual bool IsVisible() const noexcept = 0;
		[[nodiscard]] virtual bool IsEnabled() const noexcept = 0;
		[[nodiscard]] virtual bool IsAvailable(FUiId id) const noexcept = 0;
		[[nodiscard]] virtual bool IsFocusable() const noexcept
		{
			return false;
		}
		[[nodiscard]] virtual bool SupportsDrag() const noexcept
		{
			return false;
		}
		[[nodiscard]] virtual int GetShortcut() const noexcept
		{
			return 0;
		}
		virtual void SetFocused(bool focused)
		{
			(void)focused;
		}
		virtual EUiReply OnPointerDown(const SInputEvent&)
		{
			return EUiReply::Unhandled;
		}
		virtual EUiReply OnPointerMove(const SInputEvent&)
		{
			return EUiReply::Unhandled;
		}
		virtual EUiReply OnPointerUp(const SInputEvent&)
		{
			return EUiReply::Unhandled;
		}
		virtual void OnPointerCancel()
		{
		}
		virtual EUiReply OnClick(const SInputEvent&)
		{
			return EUiReply::Unhandled;
		}
		virtual EUiReply OnDrag(EUiDragPhase, const SInputEvent&)
		{
			return EUiReply::Unhandled;
		}
		virtual EUiReply OnKeyDown(const SInputEvent&)
		{
			return EUiReply::Unhandled;
		}
		virtual EUiReply OnKeyUp(const SInputEvent&)
		{
			return EUiReply::Unhandled;
		}
		virtual EUiReply OnWheel(const SInputEvent&)
		{
			return EUiReply::Unhandled;
		}
	};
}
