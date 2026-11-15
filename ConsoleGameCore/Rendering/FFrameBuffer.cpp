#include "pch.h"
#include "FFrameBuffer.h"

namespace TextRPG
{
	FFrameBuffer::FFrameBuffer(int width, int height)
	{
		Resize(width, height);
	}

	void FFrameBuffer::Resize(int width, int height)
	{
		const int preparedWidth = std::max(width, 0);
		const int preparedHeight = std::max(height, 0);
		const auto columns = static_cast<std::size_t>(preparedWidth);
		const auto rows = static_cast<std::size_t>(preparedHeight);
		const auto maxCells = std::min(m_cells.max_size(), m_attributes.max_size());
		// 곱셈 전에 두 vector의 원소 한계를 검사하므로 Win32 size_t wrap도 허용하지 않는다.
		if (rows != 0 && columns > maxCells / rows)
		{
			throw std::length_error("Frame dimensions exceed storage limits.");
		}
		const auto cellCount = columns * rows;
		std::vector<wchar_t> cells(cellCount, L' ');
		std::vector<std::uint16_t> attributes(cellCount, 0x0007);
		// 두 할당이 모두 성공한 뒤에만 저장소와 크기를 예외 없이 함께 게시한다.
		m_cells.swap(cells);
		m_attributes.swap(attributes);
		m_width = preparedWidth;
		m_height = preparedHeight;
	}

	void FFrameBuffer::Clear() noexcept
	{
		std::fill(m_cells.begin(), m_cells.end(), L' ');
		std::fill(m_attributes.begin(), m_attributes.end(), std::uint16_t{0x0007});
	}

	void FFrameBuffer::ClearWideCell(std::size_t cellIndex) noexcept
	{
		const std::size_t column = cellIndex % static_cast<std::size_t>(m_width);
		std::size_t first = cellIndex;
		std::size_t last = cellIndex;
		if ((m_attributes[cellIndex] & COMMON_LVB_LEADING_BYTE) != 0 && column + 1 < static_cast<std::size_t>(m_width))
		{
			last = cellIndex + 1;
		}
		else if ((m_attributes[cellIndex] & COMMON_LVB_TRAILING_BYTE) != 0 && column > 0)
		{
			first = cellIndex - 1;
		}
		else
		{
			return;
		}
		for (std::size_t index = first; index <= last; ++index)
		{
			m_cells[index] = L' ';
			m_attributes[index] &= static_cast<std::uint16_t>(~(COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE));
		}
	}

	void FFrameBuffer::DrawText(int x, int y, const std::wstring& text, std::uint16_t attributes, int maxCellWidth) noexcept
	{
		if (y < 0 || y >= m_height)
		{
			return;
		}

		attributes &= static_cast<std::uint16_t>(~(COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE));
		int displayOffset = 0;
		const int clipWidth = maxCellWidth < 0 ? m_width : std::max(maxCellWidth, 0);
		for (std::size_t index = 0; index < text.size();)
		{
			std::uint32_t codePoint = static_cast<std::uint16_t>(text[index]);
			std::size_t codeUnitCount = 1;
			if (codePoint >= 0xD800 && codePoint <= 0xDBFF && index + 1 < text.size())
			{
				const std::uint32_t low = static_cast<std::uint16_t>(text[index + 1]);
				if (low >= 0xDC00 && low <= 0xDFFF)
				{
					codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
					codeUnitCount = 2;
				}
			}

			const int cellWidth = GetCodePointCellWidth(codePoint);
			if (cellWidth == 0)
			{
				index += codeUnitCount;
				continue;
			}
			if (displayOffset + cellWidth > clipWidth)
			{
				break;
			}

			const int targetX = x + displayOffset;
			if (targetX >= 0 && targetX + cellWidth <= m_width)
			{
				const std::size_t cellIndex = static_cast<std::size_t>(y) * static_cast<std::size_t>(m_width) + static_cast<std::size_t>(targetX);
				ClearWideCell(cellIndex);
				if (cellWidth == 2)
				{
					ClearWideCell(cellIndex + 1);
				}
				m_cells[cellIndex] = text[index];
				m_attributes[cellIndex] = attributes;
				if (cellWidth == 2)
				{
					if (codeUnitCount == 1)
					{
						// BMP 전각 문자는 두 CHAR_INFO 셀에 같은 문자와 선행/후행 속성을 기록한다.
						m_cells[cellIndex + 1] = text[index];
						m_attributes[cellIndex] = attributes | COMMON_LVB_LEADING_BYTE;
						m_attributes[cellIndex + 1] = attributes | COMMON_LVB_TRAILING_BYTE;
					}
					else
					{
						m_cells[cellIndex + 1] = text[index + 1];
						m_attributes[cellIndex + 1] = attributes;
					}
				}
			}
			displayOffset += cellWidth;
			index += codeUnitCount;
		}
	}

	int FFrameBuffer::GetCodePointCellWidth(std::uint32_t codePoint) noexcept
	{
		if ((codePoint >= 0x0300 && codePoint <= 0x036F) ||
			(codePoint >= 0x1AB0 && codePoint <= 0x1AFF) ||
			(codePoint >= 0x1DC0 && codePoint <= 0x1DFF) ||
			(codePoint >= 0xFE00 && codePoint <= 0xFE0F) ||
			(codePoint >= 0xFE20 && codePoint <= 0xFE2F) ||
			(codePoint >= 0xE0100 && codePoint <= 0xE01EF) ||
			codePoint == 0x200D)
		{
			return 0;
		}

		if ((codePoint >= 0x1100 && codePoint <= 0x11FF) ||
			(codePoint >= 0x2E80 && codePoint <= 0xA4CF) ||
			(codePoint >= 0xAC00 && codePoint <= 0xD7A3) ||
			(codePoint >= 0xF900 && codePoint <= 0xFAFF) ||
			(codePoint >= 0xFE10 && codePoint <= 0xFE6F) ||
			(codePoint >= 0xFF01 && codePoint <= 0xFF60) ||
			(codePoint >= 0xFFE0 && codePoint <= 0xFFE6) ||
			(codePoint >= 0x1F300 && codePoint <= 0x1FAFF))
		{
			return 2;
		}
		return 1;
	}

	int FFrameBuffer::GetDisplayCellWidth(const std::wstring& text) noexcept
	{
		int width = 0;
		for (std::size_t index = 0; index < text.size();)
		{
			std::uint32_t codePoint = static_cast<std::uint16_t>(text[index]);
			std::size_t codeUnitCount = 1;
			if (codePoint >= 0xD800 && codePoint <= 0xDBFF && index + 1 < text.size())
			{
				const std::uint32_t low = static_cast<std::uint16_t>(text[index + 1]);
				if (low >= 0xDC00 && low <= 0xDFFF)
				{
					codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
					codeUnitCount = 2;
				}
			}
			width += GetCodePointCellWidth(codePoint);
			index += codeUnitCount;
		}
		return width;
	}

	int FFrameBuffer::GetWidth() const noexcept
	{
		return m_width;
	}

	int FFrameBuffer::GetHeight() const noexcept
	{
		return m_height;
	}

	const std::vector<wchar_t>& FFrameBuffer::GetCells() const noexcept
	{
		return m_cells;
	}

	const std::vector<std::uint16_t>& FFrameBuffer::GetAttributes() const noexcept
	{
		return m_attributes;
	}
}
