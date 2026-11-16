#pragma once

namespace TextRPG
{
	enum class EInputKey
	{
		None,
		Up,
		Down,
		Enter,
		Escape,
		Digit1,
		Digit2,
		Digit3,
		Digit4,
	};

	enum class EKeyStatus
	{
		None,
		Pressed,
		Held,
		Released,
	};

	enum class EMouseButton
	{
		Left,
		Right,
		Middle,
		X1,
		X2,
		Count,
	};

	enum class EInputEventType
	{
		KeyDown,
		KeyUp,
		PointerDown,
		PointerUp,
		PointerMove,
		Wheel,
		Resize,
		FocusLost,
	};

	struct SInputEvent final
	{
		EInputKey Key = EInputKey::None;
		EInputEventType Type = EInputEventType::KeyDown;
		std::uint16_t RepeatCount = 1;
		bool IsRepeat = false;
		int WheelDelta = 0;
		std::uint64_t BatchId = 0;
		bool HasMousePosition = false;
		int MouseX = -1;
		int MouseY = -1;
		int LocalX = -1;
		int LocalY = -1;
		int ScreenWidth = 0;
		int ScreenHeight = 0;
		// 0은 물리 ID를 제공하지 않는 기존 공급자, 1~511은 공급자가 정한 기기 키다.
		std::uint16_t PhysicalKey = 0;
		// Manager가 논리 눌림이 실제 바뀌었는지 표시한다. 기존 수동 이벤트는 전이로 취급한다.
		bool IsLogicalTransition = true;
		// 포인터 전이의 버튼이다. 기존 수동 포인터 이벤트는 왼쪽 버튼으로 유지한다.
		EMouseButton MouseButton = EMouseButton::Left;
	};
}
