#pragma once

namespace TextRPG
{
	class FFrameBuffer
	{
	public:
		FFrameBuffer() = default;
		FFrameBuffer(int width, int height);

		// 실패하면 크기와 두 저장소/기존 내용이 모두 유지된다. 음수 크기는 0으로 정규화한다.
		void Resize(int width, int height);
		void Clear() noexcept;
		void DrawText(
			int x,
			int y,
			const std::wstring& text,
			std::uint16_t attributes = 0x0007,
			int maxCellWidth = -1) noexcept;

		[[nodiscard]] int GetWidth() const noexcept;
		[[nodiscard]] int GetHeight() const noexcept;
		[[nodiscard]] const std::vector<wchar_t>& GetCells() const noexcept;
		[[nodiscard]] const std::vector<std::uint16_t>& GetAttributes() const noexcept;
		[[nodiscard]] static int GetDisplayCellWidth(const std::wstring& text) noexcept;

	private:
		int m_width = 0;
		int m_height = 0;
		std::vector<wchar_t> m_cells;
		std::vector<std::uint16_t> m_attributes;

		void ClearWideCell(std::size_t cellIndex) noexcept;
		[[nodiscard]] static int GetCodePointCellWidth(std::uint32_t codePoint) noexcept;
	};
}
