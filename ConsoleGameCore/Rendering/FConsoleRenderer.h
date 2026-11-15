#pragma once

namespace TextRPG
{
	class FConsoleRenderer
	{
	public:
		FConsoleRenderer() noexcept;

		[[nodiscard]] bool BeginFrame() noexcept;
		[[nodiscard]] FFrameBuffer& GetBackBuffer() noexcept;
		[[nodiscard]] bool Present() noexcept;
		[[nodiscard]] COORD GetViewportOrigin() const noexcept;

	private:
		HANDLE m_outputHandle = INVALID_HANDLE_VALUE;
		SMALL_RECT m_viewport = {};
		FFrameBuffer m_frontBuffer;
		FFrameBuffer m_backBuffer;
	};
}
