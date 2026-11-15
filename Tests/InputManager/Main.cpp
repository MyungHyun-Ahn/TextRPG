#include "pch.h"
#include "Input/FInputManager.h"
#include "Input/FConsoleInput.h"
#include <type_traits>

using namespace TextRPG;

void ArmInputAllocationFailure() noexcept;
unsigned GetInputAllocationFaultHits() noexcept;

namespace
{
	int failures = 0;
	int checks = 0;
	void Check(bool condition, const char* label)
	{
		++checks;
		if (!condition)
		{
			std::cout << "FAIL: " << label << '\n';
			++failures;
		}
	}

	class FQueuedInput final : public IInputSource
	{
	public:
		std::vector<SInputEvent> Events;
		std::size_t Next = 0;
		EInputReadResult End = EInputReadResult::Timeout;
		EInputReadResult ReadEvent(SInputEvent& event) noexcept override
		{
			if (Next == Events.size())
			{
				return End;
			}
			event = Events[Next++];
			return EInputReadResult::Event;
		}
		void Queue(SInputEvent event)
		{
			Events.push_back(event);
		}
	};

	SInputEvent Key(EInputKey key, EInputEventType type)
	{
		SInputEvent event;
		event.Key = key;
		event.Type = type;
		return event;
	}

	void KeyMatrix()
	{
		Check((std::is_same_v<decltype(std::declval<const FInputManager&>().GetEvents()), const std::vector<SInputEvent>&>),
			"const event vector reference type");
		for (int value = static_cast<int>(EInputKey::Up); value <= static_cast<int>(EInputKey::Digit4); ++value)
		{
			FQueuedInput source;
			FInputManager manager(source);
			const auto key = static_cast<EInputKey>(value);
			source.Queue(Key(key, EInputEventType::KeyDown));
			Check(manager.Update() == EInputReadResult::Event && manager.GetKeyStatus(key) == EKeyStatus::Pressed,
				"all logical keys Pressed");
			Check(manager.Update() == EInputReadResult::Timeout && manager.GetKeyStatus(key) == EKeyStatus::Held,
				"all logical keys Held");
			source.Queue(Key(key, EInputEventType::KeyUp));
			Check(manager.Update() == EInputReadResult::Event && manager.GetKeyStatus(key) == EKeyStatus::Released,
				"all logical keys Released");
			Check(manager.Update() == EInputReadResult::Timeout && manager.GetKeyStatus(key) == EKeyStatus::None,
				"all logical keys None");
		}
	}

	void AliasMatrix()
	{
		for (WORD digit = 1; digit <= 4; ++digit)
		{
			FConsoleInput adapter;
			FQueuedInput source;
			FInputManager manager(source);
			INPUT_RECORD record{};
			record.EventType = KEY_EVENT;
			record.Event.KeyEvent.bKeyDown = TRUE;
			record.Event.KeyEvent.wVirtualKeyCode = static_cast<WORD>('0' + digit);
			SInputEvent top;
			Check(adapter.TranslateRecord(record, {}, 132, 43, top), "all top digits translated");
			record.Event.KeyEvent.wVirtualKeyCode = static_cast<WORD>(VK_NUMPAD0 + digit);
			SInputEvent pad;
			Check(adapter.TranslateRecord(record, {}, 132, 43, pad) && top.PhysicalKey != pad.PhysicalKey,
				"all numpad aliases distinct");
			source.Queue(top);
			source.Queue(pad);
			Check(manager.Update() == EInputReadResult::Event, "alias matrix down");
			pad.Type = EInputEventType::KeyUp;
			source.Queue(pad);
			Check(manager.Update() == EInputReadResult::Event && manager.IsKeyDown(top.Key) && !manager.WasKeyReleased(top.Key),
				"reverse alias release retains top digit");
			top.Type = EInputEventType::KeyUp;
			source.Queue(top);
			Check(manager.Update() == EInputReadResult::Event && manager.WasKeyReleased(top.Key), "reverse last alias released");
		}
	}

	void MovementBoundary()
	{
		FQueuedInput source;
		FInputManager manager(source);
		SInputEvent move;
		move.Type = EInputEventType::PointerMove;
		move.HasMousePosition = true;
		move.MouseX = std::numeric_limits<int>::min();
		move.MouseY = std::numeric_limits<int>::max();
		source.Queue(move);
		Check(manager.Update() == EInputReadResult::Event, "extreme initial position");
		std::swap(move.MouseX, move.MouseY);
		source.Queue(move);
		Check(manager.Update() == EInputReadResult::Event && manager.GetMouseDeltaX() == 4294967295LL &&
			manager.GetMouseDeltaY() == -4294967295LL, "movement widens before subtraction");
	}

	void NativeConsole()
	{
		Check(!std::is_copy_constructible_v<FConsoleInput> && !std::is_copy_assignable_v<FConsoleInput>, "console owner not copyable");
		HANDLE input = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
			nullptr, OPEN_EXISTING, 0, nullptr);
		Check(input != INVALID_HANDLE_VALUE, "owned console input opened");
		if (input == INVALID_HANDLE_VALUE) return;
		const HANDLE previous = GetStdHandle(STD_INPUT_HANDLE);
		DWORD originalMode = 0;
		Check(GetConsoleMode(input, &originalMode) != FALSE, "native original mode read");
		Check(SetStdHandle(STD_INPUT_HANDLE, input) != FALSE && FlushConsoleInputBuffer(input) != FALSE,
			"owned input selected and cleared");
		{
			FConsoleInput adapter;
			FInputManager manager(adapter);
			std::wstring error;
			Check(adapter.Initialize(error) && adapter.Initialize(error), "initialize idempotent");
			adapter.BeginBatch({10, 20}, 132, 43);
			Check(manager.Update() == EInputReadResult::Event && manager.GetEvents().size() == 1 &&
				manager.GetEvents()[0].Type == EInputEventType::Resize, "native viewport notification");
			Check(manager.Update() == EInputReadResult::Timeout, "native normal Timeout");
			std::array<INPUT_RECORD, 3> records{};
			records[0].EventType = KEY_EVENT;
			records[0].Event.KeyEvent.bKeyDown = TRUE;
			records[0].Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
			records[0].Event.KeyEvent.wRepeatCount = 2;
			records[1] = records[0];
			records[1].Event.KeyEvent.bKeyDown = FALSE;
			records[2].EventType = MOUSE_EVENT;
			records[2].Event.MouseEvent.dwMousePosition = {13, 24};
			records[2].Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED;
			DWORD written = 0;
			Check(WriteConsoleInputW(input, records.data(), static_cast<DWORD>(records.size()), &written) != FALSE && written == 3,
				"real Win32 records written");
			Check(manager.Update() == EInputReadResult::Event && manager.GetEvents().size() == 4, "native drain includes pending buttons");
			Check(manager.WasKeyPressed(EInputKey::Enter) && manager.WasKeyReleased(EInputKey::Enter) &&
				manager.GetEvents()[0].RepeatCount == 2 && manager.GetMouseX() == 3 && manager.GetMouseY() == 4,
				"native ordered tap repeat viewport");
			Check(manager.IsMouseButtonDown(EMouseButton::Left) && manager.IsMouseButtonDown(EMouseButton::Right),
				"native multi button tracking");
		}
		DWORD restoredMode = 0;
		Check(GetConsoleMode(input, &restoredMode) != FALSE && restoredMode == originalMode,
			"native original mode restored after double init");
		Check(SetStdHandle(STD_INPUT_HANDLE, previous) != FALSE, "standard input restored");
		const BOOL didClose = CloseHandle(input);
		input = INVALID_HANDLE_VALUE;
		Check(didClose != FALSE, "owned console input closed");
		FConsoleInput closed;
		SInputEvent event;
		Check(closed.ReadEvent(event) == EInputReadResult::Closed, "uninitialized provider Closed");
		HANDLE nonConsole = CreateEventW(nullptr, TRUE, FALSE, nullptr);
		Check(nonConsole != nullptr, "owned non-console handle");
		if (nonConsole != nullptr)
		{
			Check(SetStdHandle(STD_INPUT_HANDLE, nonConsole) != FALSE, "failed initialization input selected");
			{
				FConsoleInput invalid;
				std::wstring error;
				Check(!invalid.Initialize(error) && !error.empty(), "native initialization failure diagnostic");
				Check(invalid.ReadEvent(event) == EInputReadResult::Error, "native read failure Error not Timeout");
			}
			Check(SetStdHandle(STD_INPUT_HANDLE, previous) != FALSE, "failed initialization input restored");
			const BOOL didCloseEvent = CloseHandle(nonConsole);
			nonConsole = nullptr;
			Check(didCloseEvent != FALSE, "owned event closed");
		}
	}

	void IgnoredKeys()
	{
		FQueuedInput source;
		FInputManager manager(source);
		source.Queue(SInputEvent{});
		Check(manager.Update() == EInputReadResult::Event && !manager.GetEvents()[0].IsLogicalTransition &&
			!manager.IsKeyDown(EInputKey::None), "default event cannot claim a logical transition");
		source.Queue(Key(EInputKey::Up, EInputEventType::KeyUp));
		Check(manager.Update() == EInputReadResult::Event && !manager.WasKeyReleased(EInputKey::Up) &&
			!manager.GetEvents()[0].IsLogicalTransition, "orphan up is not a logical release");
	}

	void InvalidSourceEvents()
	{
		for (int invalidKind = 0; invalidKind < 4; ++invalidKind)
		{
			FQueuedInput source;
			FInputManager manager(source);
			auto event = Key(EInputKey::Enter, EInputEventType::KeyDown);
			if (invalidKind == 0) event.Key = static_cast<EInputKey>(999);
			if (invalidKind == 1) event.PhysicalKey = 512;
			if (invalidKind == 2) event.Type = static_cast<EInputEventType>(999);
			if (invalidKind == 3)
			{
				event.Type = EInputEventType::PointerDown;
				event.MouseButton = EMouseButton::Count;
			}
			source.Queue(event);
			Check(manager.Update() == EInputReadResult::Error && manager.GetEvents().empty(), "invalid source event fail closed");
		}
		FQueuedInput source;
		FInputManager manager(source);
		auto event = Key(EInputKey::Up, EInputEventType::KeyDown);
		event.RepeatCount = 0;
		source.Queue(event);
		Check(manager.Update() == EInputReadResult::Event && manager.GetEvents()[0].RepeatCount == 1, "zero repeat normalized safely");
	}

	void AllocationFailure()
	{
		FQueuedInput source;
		FInputManager manager(source);
		source.Queue(Key(EInputKey::Enter, EInputEventType::KeyDown));
		const unsigned before = GetInputAllocationFaultHits();
		ArmInputAllocationFailure();
		bool threw = false;
		EInputReadResult result = EInputReadResult::Timeout;
		try
		{
			result = manager.Update();
		}
		catch (const std::bad_alloc&)
		{
			threw = true;
		}
		Check(GetInputAllocationFaultHits() == before + 1, "allocation fault reached exactly once");
		Check(!threw && result == EInputReadResult::Error, "allocation failure returned Error");
		Check(manager.GetEvents().empty() && !manager.IsKeyDown(EInputKey::Enter), "allocation failure fail-closed reset");
		source.Queue(Key(EInputKey::Enter, EInputEventType::KeyDown));
		Check(manager.Update() == EInputReadResult::Event && manager.WasKeyPressed(EInputKey::Enter), "allocation failure recovery");
	}

	void ExplicitReset()
	{
		FQueuedInput source;
		FInputManager manager(source);
		source.Queue(Key(EInputKey::Up, EInputEventType::KeyDown));
		Check(manager.Update() == EInputReadResult::Event, "reset setup");
		manager.Reset();
		Check(manager.GetEvents().empty() && manager.GetKeyStatus(EInputKey::Up) == EKeyStatus::None &&
			!manager.WasKeyReleased(EInputKey::Up), "explicit reset neutral no fabricated release");
		source.Queue(Key(EInputKey::Up, EInputEventType::KeyDown));
		Check(manager.Update() == EInputReadResult::Event && !manager.GetEvents()[0].IsRepeat, "reset clears physical identities");
	}

	void Lifecycle()
	{
		FQueuedInput source;
		FInputManager manager(source);
		source.Queue(Key(EInputKey::Enter, EInputEventType::KeyDown));
		SInputEvent pointer;
		pointer.Type = EInputEventType::PointerDown;
		pointer.HasMousePosition = true;
		pointer.MouseX = 8;
		pointer.MouseY = 9;
		source.Queue(pointer);
		Check(manager.Update() == EInputReadResult::Event, "lifecycle setup");
		SInputEvent lifecycle;
		lifecycle.Type = EInputEventType::Resize;
		source.Queue(lifecycle);
		Check(manager.Update() == EInputReadResult::Event && manager.IsKeyDown(EInputKey::Enter) &&
			manager.IsMouseButtonDown(EMouseButton::Left), "resize preserves device held");
		Check(!manager.HasMousePosition(), "resize invalidates coordinate basis");
		lifecycle.Type = EInputEventType::FocusLost;
		source.Queue(lifecycle);
		Check(manager.Update() == EInputReadResult::Event, "focus lost remains lifecycle event");
		Check(!manager.IsKeyDown(EInputKey::Enter) && manager.WasKeyReleased(EInputKey::Enter) &&
			!manager.IsMouseButtonDown(EMouseButton::Left) && manager.WasMouseButtonReleased(EMouseButton::Left),
			"focus lost releases devices");
		source.Queue(Key(EInputKey::Enter, EInputEventType::KeyDown));
		Check(manager.Update() == EInputReadResult::Event && !manager.GetEvents()[0].IsRepeat, "focus recovery fresh press");
		for (const auto terminal : {EInputReadResult::Error, EInputReadResult::Closed})
		{
			source.Queue(Key(EInputKey::Enter, EInputEventType::KeyDown));
			source.End = terminal;
			Check(manager.Update() == terminal, "terminal not hidden by collected events");
			Check(!manager.IsKeyDown(EInputKey::Enter) && manager.WasKeyReleased(EInputKey::Enter), "terminal releases held");
			Check(manager.GetEvents().size() == 1, "terminal prefix preserved for diagnostics only");
		}
		FConsoleInput adapter;
		INPUT_RECORD record{};
		record.EventType = KEY_EVENT;
		record.Event.KeyEvent.bKeyDown = TRUE;
		record.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
		SInputEvent event;
		Check(adapter.TranslateRecord(record, {}, 132, 43, event), "provider focus setup");
		record.EventType = FOCUS_EVENT;
		record.Event.FocusEvent.bSetFocus = FALSE;
		Check(adapter.TranslateRecord(record, {}, 132, 43, event), "provider focus loss");
		record.EventType = KEY_EVENT;
		record.Event.KeyEvent.bKeyDown = TRUE;
		Check(adapter.TranslateRecord(record, {}, 132, 43, event) && !event.IsRepeat, "provider repeat state reset on focus loss");
	}

	void ProviderButtons()
	{
		FConsoleInput adapter;
		FQueuedInput source;
		FInputManager manager(source);
		INPUT_RECORD record{};
		record.EventType = MOUSE_EVENT;
		record.Event.MouseEvent.dwMousePosition = {14, 26};
		record.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED |
			FROM_LEFT_2ND_BUTTON_PRESSED | FROM_LEFT_3RD_BUTTON_PRESSED | FROM_LEFT_4TH_BUTTON_PRESSED;
		SInputEvent event;
		Check(adapter.TranslateRecord(record, {10, 20}, 132, 43, event), "multi button translated");
		Check(event.MouseX == 4 && event.MouseY == 6 && event.MouseButton == EMouseButton::Left, "provider viewport cells");
		source.Queue(event);
		while (adapter.TryReadPendingEvent(event))
		{
			source.Queue(event);
		}
		Check(manager.Update() == EInputReadResult::Event && manager.GetEvents().size() == 5, "all five button transitions emitted");
		for (std::size_t i = 0; i < static_cast<std::size_t>(EMouseButton::Count); ++i)
		{
			Check(manager.WasMouseButtonPressed(static_cast<EMouseButton>(i)), "all buttons pressed");
		}
		record.Event.MouseEvent.dwButtonState = static_cast<DWORD>(static_cast<WORD>(-WHEEL_DELTA)) << 16;
		record.Event.MouseEvent.dwEventFlags = MOUSE_WHEELED;
		Check(adapter.TranslateRecord(record, {10, 20}, 132, 43, event), "button releases plus wheel");
		source.Queue(event);
		while (adapter.TryReadPendingEvent(event))
		{
			source.Queue(event);
		}
		Check(manager.Update() == EInputReadResult::Event && manager.GetEvents().size() == 6 &&
			manager.GetEvents().back().Type == EInputEventType::Wheel, "multi record normalized order");
		Check(manager.GetMouseWheelDelta() == -WHEEL_DELTA, "signed wheel preserved with simultaneous releases");
		for (std::size_t i = 0; i < static_cast<std::size_t>(EMouseButton::Count); ++i)
		{
			Check(manager.WasMouseButtonReleased(static_cast<EMouseButton>(i)), "all buttons released");
		}
	}

	void MouseFrames()
	{
		FQueuedInput source;
		FInputManager manager(source);
		const FInputManager& view = manager;
		Check(!view.HasMousePosition() && view.GetMouseButtonStatus(EMouseButton::Left) == EKeyStatus::None, "initial mouse");
		SInputEvent event;
		event.Type = EInputEventType::PointerDown;
		event.MouseButton = EMouseButton::Right;
		event.HasMousePosition = true;
		event.MouseX = 10;
		event.MouseY = 20;
		source.Queue(event);
		Check(manager.Update() == EInputReadResult::Event, "mouse down event");
		Check(view.GetMouseButtonStatus(EMouseButton::Right) == EKeyStatus::Pressed && view.WasMouseButtonPressed(EMouseButton::Right),
			"right pressed");
		Check(view.IsMouseButtonDown(EMouseButton::Right) && !view.IsMouseButtonDown(EMouseButton::Left), "buttons independent");
		Check(view.HasMousePosition() && view.GetMouseX() == 10 && view.GetMouseY() == 20 && view.GetMouseDeltaX() == 0,
			"first sample no movement");
		Check(manager.Update() == EInputReadResult::Timeout && view.GetMouseButtonStatus(EMouseButton::Right) == EKeyStatus::Held,
			"mouse no-event Held");
		event.Type = EInputEventType::PointerMove;
		event.MouseX = 7;
		event.MouseY = 24;
		source.Queue(event);
		event.Type = EInputEventType::Wheel;
		event.WheelDelta = WHEEL_DELTA;
		source.Queue(event);
		event.WheelDelta = -2 * WHEEL_DELTA;
		source.Queue(event);
		Check(manager.Update() == EInputReadResult::Event, "move/wheel frame");
		Check(view.GetMouseDeltaX() == -3 && view.GetMouseDeltaY() == 4 && view.GetMouseWheelDelta() == -WHEEL_DELTA,
			"movement and raw wheel unit accumulation");
		Check(view.GetMouseWheelDelta() == -WHEEL_DELTA && view.GetEvents().size() == 3, "mouse getters nonconsuming");
		event.Type = EInputEventType::PointerUp;
		source.Queue(event);
		Check(manager.Update() == EInputReadResult::Event && view.WasMouseButtonReleased(EMouseButton::Right) &&
			view.GetMouseButtonStatus(EMouseButton::Right) == EKeyStatus::Released, "mouse Released");
		Check(view.GetMouseDeltaX() == 0 && view.GetMouseWheelDelta() == 0, "transients reset per frame");
		Check(manager.Update() == EInputReadResult::Timeout && view.GetMouseButtonStatus(EMouseButton::Right) == EKeyStatus::None,
			"mouse Released to None");
		event.Type = EInputEventType::PointerDown;
		event.MouseButton = EMouseButton::Left;
		source.Queue(event);
		event.Type = EInputEventType::PointerUp;
		source.Queue(event);
		Check(manager.Update() == EInputReadResult::Event && view.WasMouseButtonPressed(EMouseButton::Left) &&
			view.WasMouseButtonReleased(EMouseButton::Left) && !view.IsMouseButtonDown(EMouseButton::Left), "short mouse tap edges");
	}

	void NumpadReleaseIdentity()
	{
		FConsoleInput adapter;
		FQueuedInput source;
		FInputManager manager(source);
		INPUT_RECORD record{};
		record.EventType = KEY_EVENT;
		record.Event.KeyEvent.bKeyDown = TRUE;
		record.Event.KeyEvent.wVirtualKeyCode = VK_NUMPAD1;
		record.Event.KeyEvent.wVirtualScanCode = 0x4F;
		SInputEvent event;
		Check(adapter.TranslateRecord(record, {}, 132, 43, event), "scan numpad down");
		const auto physical = event.PhysicalKey;
		source.Queue(event);
		Check(manager.Update() == EInputReadResult::Event, "scan initial frame");
		record.Event.KeyEvent.bKeyDown = FALSE;
		record.Event.KeyEvent.wVirtualKeyCode = VK_END;
		const bool translated = adapter.TranslateRecord(record, {}, 132, 43, event);
		Check(translated && event.Key == EInputKey::Digit1 && event.PhysicalKey == physical,
			"numlock change release retains press identity");
		if (translated) source.Queue(event);
		Check(manager.Update() == EInputReadResult::Event && !manager.IsKeyDown(EInputKey::Digit1), "changed VK still releases numpad");
		record.Event.KeyEvent.bKeyDown = TRUE;
		Check(!adapter.TranslateRecord(record, {}, 132, 43, event), "unmapped End does not add shortcut");
	}

	void PhysicalAliases()
	{
		FConsoleInput adapter;
		FQueuedInput source;
		FInputManager manager(source);
		INPUT_RECORD record{};
		record.EventType = KEY_EVENT;
		record.Event.KeyEvent.bKeyDown = TRUE;
		record.Event.KeyEvent.wRepeatCount = 1;
		record.Event.KeyEvent.wVirtualKeyCode = '1';
		SInputEvent top;
		Check(adapter.TranslateRecord(record, {}, 132, 43, top), "top digit translated");
		record.Event.KeyEvent.wVirtualKeyCode = VK_NUMPAD1;
		SInputEvent pad;
		Check(adapter.TranslateRecord(record, {}, 132, 43, pad), "numpad translated");
		Check(top.PhysicalKey != 0 && top.PhysicalKey != pad.PhysicalKey, "physical aliases distinct");
		source.Queue(top);
		source.Queue(pad);
		Check(manager.Update() == EInputReadResult::Event && manager.WasKeyPressed(EInputKey::Digit1), "alias initial press");
		Check(!manager.GetEvents()[0].IsRepeat && manager.GetEvents()[1].IsRepeat, "alias second down not new confirmation");
		top.Type = EInputEventType::KeyUp;
		source.Queue(top);
		Check(manager.Update() == EInputReadResult::Event, "partial alias up");
		Check(manager.IsKeyDown(EInputKey::Digit1) && !manager.WasKeyReleased(EInputKey::Digit1), "partial alias does not release");
		Check(!manager.GetEvents()[0].IsLogicalTransition, "partial up tagged for UI gate");
		pad.Type = EInputEventType::KeyUp;
		source.Queue(pad);
		Check(manager.Update() == EInputReadResult::Event && manager.WasKeyReleased(EInputKey::Digit1), "last alias releases");
		Check(manager.GetEvents()[0].IsLogicalTransition, "last alias release marked");
	}

	void OrderedTransitions()
	{
		FQueuedInput source;
		FInputManager manager(source);
		auto down = Key(EInputKey::Digit1, EInputEventType::KeyDown);
		down.RepeatCount = 3;
		source.Queue(down);
		source.Queue(down);
		source.Queue(Key(EInputKey::Digit1, EInputEventType::KeyUp));
		Check(manager.Update() == EInputReadResult::Event, "tap collected");
		const auto& events = manager.GetEvents();
		Check(events.size() == 3 && events[0].Type == EInputEventType::KeyDown && events[2].Type == EInputEventType::KeyUp,
			"tap source order retained");
		Check(!events[0].IsRepeat && events[1].IsRepeat && events[1].RepeatCount == 3, "repeat normalized and count retained");
		Check(manager.WasKeyPressed(EInputKey::Digit1) && manager.WasKeyReleased(EInputKey::Digit1) &&
			manager.GetKeyStatus(EInputKey::Digit1) == EKeyStatus::Released, "short tap both edges final Released");
		source.Queue(down);
		Check(manager.Update() == EInputReadResult::Event, "next down");
		source.Queue(Key(EInputKey::Digit1, EInputEventType::KeyUp));
		source.Queue(down);
		Check(manager.Update() == EInputReadResult::Event, "up then down");
		Check(manager.WasKeyPressed(EInputKey::Digit1) && manager.WasKeyReleased(EInputKey::Digit1) &&
			manager.GetKeyStatus(EInputKey::Digit1) == EKeyStatus::Pressed, "up-down both edges final Pressed");
	}

	void KeyFrames()
	{
		FQueuedInput source;
		FInputManager manager(source);
		const FInputManager& view = manager;
		Check(view.GetKeyStatus(EInputKey::Enter) == EKeyStatus::None, "initial None");
		source.Queue(Key(EInputKey::Enter, EInputEventType::KeyDown));
		Check(manager.Update() == EInputReadResult::Event, "normal collected Event");
		Check(view.GetKeyStatus(EInputKey::Enter) == EKeyStatus::Pressed, "down becomes Pressed");
		Check(view.WasKeyPressed(EInputKey::Enter) && view.IsKeyDown(EInputKey::Enter), "const pressed/down");
		Check(view.GetKeyStatus(EInputKey::Enter) == EKeyStatus::Pressed && view.GetEvents().size() == 1, "nonconsuming view");
		Check(manager.Update() == EInputReadResult::Timeout, "empty Timeout");
		Check(view.GetKeyStatus(EInputKey::Enter) == EKeyStatus::Held && !view.WasKeyPressed(EInputKey::Enter), "empty frame Held");
		source.Queue(Key(EInputKey::Enter, EInputEventType::KeyUp));
		Check(manager.Update() == EInputReadResult::Event, "up Event");
		Check(view.GetKeyStatus(EInputKey::Enter) == EKeyStatus::Released && view.WasKeyReleased(EInputKey::Enter), "Released");
		Check(!view.IsKeyDown(EInputKey::Enter), "released not down");
		Check(manager.Update() == EInputReadResult::Timeout, "next empty Timeout");
		Check(view.GetKeyStatus(EInputKey::Enter) == EKeyStatus::None && !view.WasKeyReleased(EInputKey::Enter), "Released to None");
	}
}

int main()
{
	KeyFrames();
	OrderedTransitions();
	PhysicalAliases();
	NumpadReleaseIdentity();
	MouseFrames();
	ProviderButtons();
	Lifecycle();
	ExplicitReset();
	AllocationFailure();
	InvalidSourceEvents();
	IgnoredKeys();
	NativeConsole();
	KeyMatrix();
	AliasMatrix();
	MovementBoundary();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
