#pragma once

namespace TextRPG
{
	class FConsoleInput final : public IInputSource
	{
	public:
		FConsoleInput() = default;
		FConsoleInput(const FConsoleInput&) = delete;
		FConsoleInput& operator=(const FConsoleInput&) = delete;
		FConsoleInput(FConsoleInput&&) = delete;
		FConsoleInput& operator=(FConsoleInput&&) = delete;
		~FConsoleInput() override;
		[[nodiscard]] bool Initialize(std::wstring& errorMessage);
		void BeginBatch(COORD viewportOrigin, int screenWidth, int screenHeight) noexcept;
		EInputReadResult ReadEvent(SInputEvent& event) noexcept override;
		// TranslateRecord가 한 OS 레코드에서 만든 나머지 버튼 전이를 먼저 비운다.
		[[nodiscard]] bool TryReadPendingEvent(SInputEvent& event) noexcept;
		[[nodiscard]] bool TranslateRecord(const INPUT_RECORD& record, COORD viewportOrigin, int screenWidth, int screenHeight,
										   SInputEvent& event) noexcept;

	private:
		HANDLE m_inputHandle = INVALID_HANDLE_VALUE;
		DWORD m_originalMode = 0;
		bool m_hasOriginalMode = false;
		bool m_isInitialized = false;
		DWORD m_mouseButtonsDown = 0;
		std::array<SInputEvent, 6> m_pendingEvents{};
		std::size_t m_pendingNext = 0;
		std::size_t m_pendingCount = 0;
		bool m_hasViewportEvent = false;
		std::array<EInputKey, 512> m_keysDown{};
		std::array<std::uint16_t, 512> m_scanPhysicalKeys{};
		COORD m_viewportOrigin{};
		int m_screenWidth = 0;
		int m_screenHeight = 0;
		std::uint64_t m_batchId = 0;
	};
}
