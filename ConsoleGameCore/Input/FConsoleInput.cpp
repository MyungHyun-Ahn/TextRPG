#include "pch.h"
#include "FConsoleInput.h"

namespace TextRPG
{
	namespace
	{
		EInputKey TranslateKey(WORD key) noexcept
		{
			switch (key)
			{
			case VK_UP: return EInputKey::Up;
			case VK_DOWN: return EInputKey::Down;
			case VK_RETURN: return EInputKey::Enter;
			case VK_ESCAPE: return EInputKey::Escape;
			case '1':
			case VK_NUMPAD1: return EInputKey::Digit1;
			case '2':
			case VK_NUMPAD2: return EInputKey::Digit2;
			case '3':
			case VK_NUMPAD3: return EInputKey::Digit3;
			case '4':
			case VK_NUMPAD4: return EInputKey::Digit4;
			default: return EInputKey::None;
			}
		}
	}

	FConsoleInput::~FConsoleInput()
	{
		if (m_hasOriginalMode)
		{
			SetConsoleMode(m_inputHandle, m_originalMode);
		}
	}

	bool FConsoleInput::Initialize(std::wstring& errorMessage)
	{
		if (m_isInitialized)
		{
			errorMessage.clear();
			return true;
		}
		m_inputHandle = GetStdHandle(STD_INPUT_HANDLE);
		if (m_inputHandle == INVALID_HANDLE_VALUE || m_inputHandle == nullptr || !GetConsoleMode(m_inputHandle, &m_originalMode))
		{
			errorMessage = L"Unable to read console input mode.";
			return false;
		}
		m_hasOriginalMode = true;

		DWORD inputMode = m_originalMode;
		inputMode |= ENABLE_EXTENDED_FLAGS | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT;
		inputMode &= ~(ENABLE_QUICK_EDIT_MODE | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
		if (!SetConsoleMode(m_inputHandle, inputMode))
		{
			errorMessage = L"Unable to enable console mouse input.";
			return false;
		}
		m_isInitialized = true;
		errorMessage.clear();
		return true;
	}

	void FConsoleInput::BeginBatch(COORD viewportOrigin, int screenWidth, int screenHeight) noexcept
	{
		m_hasViewportEvent = m_hasViewportEvent || m_viewportOrigin.X != viewportOrigin.X || m_viewportOrigin.Y != viewportOrigin.Y ||
							 m_screenWidth != screenWidth || m_screenHeight != screenHeight;
		m_viewportOrigin = viewportOrigin;
		m_screenWidth = screenWidth;
		m_screenHeight = screenHeight;
		++m_batchId;
	}
	bool FConsoleInput::TryReadPendingEvent(SInputEvent& event) noexcept
	{
		if (m_pendingNext == m_pendingCount)
		{
			return false;
		}
		event = m_pendingEvents[m_pendingNext++];
		return true;
	}

	EInputReadResult FConsoleInput::ReadEvent(SInputEvent& event) noexcept
	{
		if (TryReadPendingEvent(event))
		{
			return EInputReadResult::Event;
		}
		if (m_hasViewportEvent)
		{
			m_hasViewportEvent = false;
			event = {};
			event.Type = EInputEventType::Resize;
			event.ScreenWidth = m_screenWidth;
			event.ScreenHeight = m_screenHeight;
			event.BatchId = m_batchId;
			return EInputReadResult::Event;
		}
		if (m_inputHandle == nullptr || m_inputHandle == INVALID_HANDLE_VALUE)
		{
			return EInputReadResult::Closed;
		}
		for (;;)
		{
			DWORD queued = 0;
			if (!GetNumberOfConsoleInputEvents(m_inputHandle, &queued))
			{
				return EInputReadResult::Error;
			}
			if (queued == 0)
			{
				return EInputReadResult::Timeout;
			}
			INPUT_RECORD record{};
			DWORD read = 0;
			if (!ReadConsoleInputW(m_inputHandle, &record, 1, &read))
			{
				return EInputReadResult::Error;
			}
			if (read == 0)
			{
				return EInputReadResult::Closed;
			}
			if (TranslateRecord(record, m_viewportOrigin, m_screenWidth, m_screenHeight, event))
			{
				return EInputReadResult::Event;
			}
		}
	}
	bool FConsoleInput::TranslateRecord(const INPUT_RECORD& record, COORD viewportOrigin, int screenWidth, int screenHeight,
										SInputEvent& event) noexcept
	{
		event = {};
		event.ScreenWidth = screenWidth;
		event.ScreenHeight = screenHeight;
		event.BatchId = m_batchId;
		if (record.EventType == KEY_EVENT)
		{
			const auto& key = record.Event.KeyEvent;
			const WORD enhanced = (key.dwControlKeyState & ENHANCED_KEY) != 0 ? 0x100 : 0;
			const std::size_t scan = key.wVirtualScanCode > 0 && key.wVirtualScanCode < 256 ? key.wVirtualScanCode | enhanced : 0;
			event.PhysicalKey = static_cast<std::uint16_t>(key.wVirtualKeyCode | enhanced);
			event.Key = TranslateKey(key.wVirtualKeyCode);
			// NumLock/Shift가 바뀐 해제도 누를 때의 기기/논리 키로 돌려준다. 새 단축키는 추가하지 않는다.
			if (scan != 0 && m_scanPhysicalKeys[scan] != 0)
			{
				event.PhysicalKey = m_scanPhysicalKeys[scan];
				event.Key = m_keysDown[event.PhysicalKey];
			}
			if (event.Key == EInputKey::None || event.PhysicalKey >= m_keysDown.size())
			{
				return false;
			}
			const auto index = static_cast<std::size_t>(event.PhysicalKey);
			event.IsRepeat = key.bKeyDown && m_keysDown[index] != EInputKey::None;
			m_keysDown[index] = key.bKeyDown ? event.Key : EInputKey::None;
			if (scan != 0)
			{
				m_scanPhysicalKeys[scan] = key.bKeyDown ? event.PhysicalKey : 0;
			}
			event.Type = key.bKeyDown ? EInputEventType::KeyDown : EInputEventType::KeyUp;
			event.RepeatCount = std::max<WORD>(1, key.wRepeatCount);
			return true;
		}
		if (record.EventType == MOUSE_EVENT)
		{
			const auto& mouse = record.Event.MouseEvent;
			event.HasMousePosition = true;
			event.MouseX = static_cast<int>(mouse.dwMousePosition.X) - viewportOrigin.X;
			event.MouseY = static_cast<int>(mouse.dwMousePosition.Y) - viewportOrigin.Y;
			event.MouseLeftPressed = (mouse.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0;
			constexpr std::array<DWORD, 5> kButtonMasks = {FROM_LEFT_1ST_BUTTON_PRESSED, RIGHTMOST_BUTTON_PRESSED,
				FROM_LEFT_2ND_BUTTON_PRESSED, FROM_LEFT_3RD_BUTTON_PRESSED, FROM_LEFT_4TH_BUTTON_PRESSED};
			m_pendingNext = 0;
			m_pendingCount = 0;
			for (std::size_t i = 0; i < kButtonMasks.size(); ++i)
			{
				if (((mouse.dwButtonState ^ m_mouseButtonsDown) & kButtonMasks[i]) != 0)
				{
					auto transition = event;
					transition.Type = (mouse.dwButtonState & kButtonMasks[i]) != 0 ?
						EInputEventType::PointerDown : EInputEventType::PointerUp;
					transition.MouseButton = static_cast<EMouseButton>(i);
					if (m_pendingCount < m_pendingEvents.size())
					{
						m_pendingEvents[m_pendingCount++] = transition;
					}
				}
			}
			m_mouseButtonsDown = LOWORD(mouse.dwButtonState);
			if ((mouse.dwEventFlags & MOUSE_WHEELED) != 0 && m_pendingCount < m_pendingEvents.size())
			{
				event.Type = EInputEventType::Wheel;
				event.WheelDelta = static_cast<SHORT>(HIWORD(mouse.dwButtonState));
				m_pendingEvents[m_pendingCount++] = event;
			}
			else if ((m_pendingCount == 0 || (mouse.dwEventFlags & MOUSE_MOVED) != 0) && m_pendingCount < m_pendingEvents.size())
			{
				event.Type = EInputEventType::PointerMove;
				m_pendingEvents[m_pendingCount++] = event;
			}
			return TryReadPendingEvent(event);
		}
		if (record.EventType == WINDOW_BUFFER_SIZE_EVENT)
		{
			event.Type = EInputEventType::Resize;
			return true;
		}
		if (record.EventType == FOCUS_EVENT && !record.Event.FocusEvent.bSetFocus)
		{
			m_keysDown = {};
			m_scanPhysicalKeys = {};
			m_mouseButtonsDown = 0;
			m_pendingNext = 0;
			m_pendingCount = 0;
			event.Type = EInputEventType::FocusLost;
			return true;
		}
		return false;
	}
}
