#include "pch.h"
#include "FFrameBuffer.h"
#include "FConsoleRenderer.h"

namespace TextRPG
{
	FConsoleRenderer::FConsoleRenderer() noexcept
		: m_outputHandle(GetStdHandle(STD_OUTPUT_HANDLE))
	{
	}

	bool FConsoleRenderer::BeginFrame() noexcept
	{
		CONSOLE_SCREEN_BUFFER_INFO screenInfo = {};
		if (m_outputHandle == INVALID_HANDLE_VALUE || !GetConsoleScreenBufferInfo(m_outputHandle, &screenInfo))
		{
			return false;
		}

		m_viewport = screenInfo.srWindow;
		const int width = static_cast<int>(m_viewport.Right - m_viewport.Left + 1);
		const int height = static_cast<int>(m_viewport.Bottom - m_viewport.Top + 1);
		if (m_backBuffer.GetWidth() != width || m_backBuffer.GetHeight() != height)
		{
			m_backBuffer.Resize(width, height);
			m_frontBuffer.Resize(width, height);
		}
		m_backBuffer.Clear();
		return width > 0 && height > 0;
	}

	FFrameBuffer& FConsoleRenderer::GetBackBuffer() noexcept
	{
		return m_backBuffer;
	}

	bool FConsoleRenderer::Present() noexcept
	{
		if (m_outputHandle == INVALID_HANDLE_VALUE || m_backBuffer.GetWidth() <= 0 || m_backBuffer.GetHeight() <= 0)
		{
			return false;
		}

		const int width = m_backBuffer.GetWidth();
		const int height = m_backBuffer.GetHeight();
		const std::vector<wchar_t>& cells = m_backBuffer.GetCells();
		const std::vector<std::uint16_t>& attributes = m_backBuffer.GetAttributes();
		std::vector<CHAR_INFO> output(cells.size());
		for (std::size_t index = 0; index < cells.size(); ++index)
		{
			output[index].Char.UnicodeChar = cells[index];
			output[index].Attributes = attributes[index];
		}

		const COORD bufferSize = { static_cast<SHORT>(width), static_cast<SHORT>(height) };
		const COORD bufferOrigin = { 0, 0 };
		SMALL_RECT destination = m_viewport;
		if (!WriteConsoleOutputW(m_outputHandle, output.data(), bufferSize, bufferOrigin, &destination))
		{
			return false;
		}

		std::swap(m_frontBuffer, m_backBuffer);
		return true;
	}

	COORD FConsoleRenderer::GetViewportOrigin() const noexcept
	{
		return { m_viewport.Left, m_viewport.Top };
	}
}
