#include "pch.h"
#include <stdexcept>
#include "Rendering/FFrameBuffer.h"
#include "Rendering/FConsoleRenderer.h"

namespace
{
	void Require(bool condition, const char* message)
	{
		if (!condition)
		{
			throw std::runtime_error(message);
		}
	}

	class FOwnedOutput
	{
	public:
		FOwnedOutput()
			: m_original(GetStdHandle(STD_OUTPUT_HANDLE))
			, m_handle(CreateConsoleScreenBuffer(GENERIC_READ | GENERIC_WRITE, 0, nullptr, CONSOLE_TEXTMODE_BUFFER, nullptr))
		{
			Require(m_handle != INVALID_HANDLE_VALUE, "CreateConsoleScreenBuffer");
			Require(SetStdHandle(STD_OUTPUT_HANDLE, m_handle) != FALSE, "SetStdHandle");
		}

		~FOwnedOutput()
		{
			SetStdHandle(STD_OUTPUT_HANDLE, m_original);
			CloseHandle(m_handle);
		}

		FOwnedOutput(const FOwnedOutput&) = delete;
		FOwnedOutput& operator=(const FOwnedOutput&) = delete;

		std::vector<CHAR_INFO> Read(const TextRPG::FFrameBuffer& frame, COORD origin) const
		{
			const COORD size = { static_cast<SHORT>(frame.GetWidth()), static_cast<SHORT>(frame.GetHeight()) };
			std::vector<CHAR_INFO> cells(static_cast<std::size_t>(size.X) * static_cast<std::size_t>(size.Y));
			SMALL_RECT rectangle = { origin.X, origin.Y, static_cast<SHORT>(origin.X + size.X - 1),
				static_cast<SHORT>(origin.Y + size.Y - 1) };
			Require(ReadConsoleOutputW(m_handle, cells.data(), size, { 0, 0 }, &rectangle) != FALSE, "ReadConsoleOutputW");
			return cells;
		}

	private:
		HANDLE m_original;
		HANDLE m_handle;
	};

	void TestMixedClippingAndLifecycle(std::ofstream& report)
	{
		FOwnedOutput output;
		TextRPG::FConsoleRenderer renderer;
		Require(renderer.BeginFrame(), "mixed BeginFrame");
		auto& frame = renderer.GetBackBuffer();
		const int width = frame.GetWidth();
		const int height = frame.GetHeight();
		Require(width >= 24 && height >= 10, "owned console too small for fixture");
		frame.DrawText(0, 0, L"|1. 대화", 0x001E);
		frame.DrawText(0, 1, L"|2. 상점", 0x0008);
		frame.DrawText(0, 2, L"|3. 여관");
		frame.DrawText(0, 3, L"|4. 이동");
		frame.DrawText(0, 4, L"┌─Town 한中─┐", 0x002F);
		frame.DrawText(0, 5, L"|ASCII options|");
		for (int y = 0; y < 6; ++y)
		{
			frame.DrawText(20, y, L"|");
		}
		frame.DrawText(-1, 6, L"한A");
		Require(frame.GetCells()[static_cast<std::size_t>(6 * width)] == L' ', "left clipping drew half a glyph");
		Require(frame.GetCells()[static_cast<std::size_t>(6 * width + 1)] == L'A', "left clipping lost following ASCII");
		frame.DrawText(width - 1, 6, L"한");
		Require(frame.GetCells()[static_cast<std::size_t>(7 * width - 1)] == L' ', "right clipping drew half a glyph");
		frame.DrawText(0, 7, L"한A", 0x0007, 1);
		Require(frame.GetCells()[static_cast<std::size_t>(7 * width)] == L' ', "max-width clipping drew half a glyph");
		frame.DrawText(2, 7, L"한A", 0x0007, 2);
		Require(frame.GetCells()[static_cast<std::size_t>(7 * width + 4)] == L' ', "max-width clipping exceeded cell budget");
		frame.DrawText(4, 8, L"한중", 0x001E);
		frame.DrawText(5, 8, L"語", 0x002F);
		const std::size_t row = static_cast<std::size_t>(8 * width);
		Require(frame.GetCells()[row + 4] == L' ' && frame.GetAttributes()[row + 4] == 0x001E,
			"wide overwrite left orphan from first pair");
		Require(frame.GetCells()[row + 7] == L' ' && frame.GetAttributes()[row + 7] == 0x001E,
			"wide overwrite left orphan from second pair");
		Require(frame.GetCells()[row + 5] == L'語' && frame.GetCells()[row + 6] == L'語', "wide replacement lost glyph");
		const auto expectedCells = frame.GetCells();
		const auto expectedAttributes = frame.GetAttributes();
		const TextRPG::FFrameBuffer dimensions(width, height);
		const auto origin = renderer.GetViewportOrigin();
		Require(renderer.Present(), "mixed Present");
		const auto cells = output.Read(dimensions, origin);
		for (std::size_t index = 0; index < cells.size(); ++index)
		{
			Require(cells[index].Char.UnicodeChar == expectedCells[index], "native mixed/clip/overwrite glyph differs");
			Require(cells[index].Attributes == expectedAttributes[index], "native mixed/clip/overwrite attributes differ");
		}
		for (int y = 0; y < 6; ++y)
		{
			Require(cells[static_cast<std::size_t>(y * width + 20)].Char.UnicodeChar == L'|', "mixed row border column changed");
		}
		Require(renderer.BeginFrame(), "clear BeginFrame");
		Require(renderer.Present(), "clear Present");
		const auto cleared = output.Read(dimensions, origin);
		for (const auto& cell : cleared)
		{
			Require(cell.Char.UnicodeChar == L' ' && cell.Attributes == 0x0007, "native next-frame clear leaves wide glyph/flags");
		}
		TextRPG::FFrameBuffer resized(4, 2);
		resized.DrawText(0, 0, L"한");
		resized.Resize(1, 1);
		Require(resized.GetCells().size() == 1 && resized.GetCells()[0] == L' ' && resized.GetAttributes()[0] == 0x0007,
			"resize retains wide glyph/flags");
		resized.Resize(0, 0);
		resized.DrawText(0, 0, L"한");
		Require(resized.GetCells().empty() && resized.GetAttributes().empty(), "zero-size resize not empty");
		TextRPG::FFrameBuffer supplementary(4, 1);
		const std::wstring surrogatePair{ static_cast<wchar_t>(0xD83D), static_cast<wchar_t>(0xDE00) };
		supplementary.DrawText(0, 0, surrogatePair);
		Require(supplementary.GetCells()[0] == surrogatePair[0] && supplementary.GetCells()[1] == surrogatePair[1],
			"surrogate code units must not be duplicated as BMP glyphs");
		Require(supplementary.GetAttributes()[0] == 0x0007 && supplementary.GetAttributes()[1] == 0x0007,
			"surrogate pair incorrectly marked as BMP DBCS cells");
		report << "PASS native mixed Korean/CJK/title/ASCII fixed borders, clips, overlapping pairs, frame clear, resize\n";
		report << "PASS supplementary code units unchanged (arbitrary Unicode console rendering is outside this BMP fix)\n";
	}

	void TestAttributeOwnership(std::ofstream& report)
	{
		TextRPG::FFrameBuffer frame(12, 1);
		frame.DrawText(0, 0, L"A한", 0x003E | COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);
		Require(frame.GetAttributes()[0] == 0x003E, "caller-supplied wide flags created an orphan ASCII cell");
		Require(frame.GetAttributes()[1] == (0x003E | COMMON_LVB_LEADING_BYTE), "leading cell has conflicting flags");
		Require(frame.GetAttributes()[2] == (0x003E | COMMON_LVB_TRAILING_BYTE), "trailing cell has conflicting flags");
		report << "PASS framebuffer owns wide flags independently of supplied colors\n";
	}

	void TestOverwrite(std::ofstream& report)
	{
		FOwnedOutput output;
		TextRPG::FConsoleRenderer renderer;
		for (const int target : { 4, 5 })
		{
			Require(renderer.BeginFrame(), "overwrite BeginFrame");
			auto& frame = renderer.GetBackBuffer();
			frame.DrawText(4, 0, L"한", 0x001E);
			frame.DrawText(target, 0, L"X", 0x002F);
			const int other = target == 4 ? 5 : 4;
			Require(frame.GetCells()[other] == L' ', "overwriting one half leaves an orphan wide glyph");
			Require(frame.GetAttributes()[other] == 0x001E, "orphan cleanup must preserve color but remove pair flags");
			Require(frame.GetCells()[target] == L'X' && frame.GetAttributes()[target] == 0x002F, "ASCII replacement changed");
			const TextRPG::FFrameBuffer dimensions(frame.GetWidth(), frame.GetHeight());
			const auto origin = renderer.GetViewportOrigin();
			Require(renderer.Present(), "overwrite Present");
			const auto cells = output.Read(dimensions, origin);
			Require(cells[other].Char.UnicodeChar == L' ' && cells[other].Attributes == 0x001E, "native orphan cell remains");
			Require(cells[target].Char.UnicodeChar == L'X' && cells[target].Attributes == 0x002F, "native ASCII replacement changed");
		}
		report << "PASS leading/trailing overwrite cleanup and highlight colors\n";
	}

	void TestKoreanOutput(std::ofstream& report)
	{
		FOwnedOutput output;
		TextRPG::FConsoleRenderer renderer;
		Require(renderer.BeginFrame(), "BeginFrame");
		auto& frame = renderer.GetBackBuffer();
		frame.DrawText(0, 0, L"|1. 대화");
		frame.DrawText(12, 0, L"|");
		const int width = frame.GetWidth();
		const int height = frame.GetHeight();
		const COORD origin = renderer.GetViewportOrigin();
		Require(renderer.Present(), "Present");
		const TextRPG::FFrameBuffer dimensions(width, height);
		const auto cells = output.Read(dimensions, origin);
		for (std::size_t x = 0; x < 14; ++x)
		{
			report << x << ": U+" << std::hex << static_cast<unsigned int>(cells[x].Char.UnicodeChar)
				<< " attr=" << cells[x].Attributes << std::dec << '\n';
		}
		Require(cells[12].Char.UnicodeChar == L'|', "Korean row border moved from column 12");
		Require((cells[4].Attributes & COMMON_LVB_LEADING_BYTE) != 0, "Korean glyph missing native leading-cell attribute");
		Require((cells[5].Attributes & COMMON_LVB_TRAILING_BYTE) != 0, "Korean glyph missing native trailing-cell attribute");
		Require(cells[4].Char.UnicodeChar == L'대' && cells[5].Char.UnicodeChar == L'대', "Korean pair is not one repeated wide glyph");
		report << "PASS native Korean output pair and fixed border\n";
	}
}

int wmain(int argc, wchar_t* argv[])
{
	if (argc != 2)
	{
		return 2;
	}
	std::ofstream report{ std::filesystem::path(argv[1]) };
	try
	{
		TestKoreanOutput(report);
		TestOverwrite(report);
		TestAttributeOwnership(report);
		TestMixedClippingAndLifecycle(report);
		return 0;
	}
	catch (const std::exception& error)
	{
		report << "FAIL " << error.what() << '\n';
		return 1;
	}
}
