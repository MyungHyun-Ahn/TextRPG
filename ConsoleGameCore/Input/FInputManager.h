#pragma once

namespace TextRPG
{
	class FInputManager final
	{
	public:
		// 공급자는 이 객체보다 오래 살아야 한다. Scene/UI 정책 없이 기기 프레임만 소유한다.
		explicit FInputManager(IInputSource& source) noexcept;
		// Timeout까지 비운다. 오류/닫힘은 부분 이벤트보다 우선하며 해당 프레임을 행동에 사용하면 안 된다.
		// vector 할당 등 std 예외는 부분 프레임과 모든 상태를 비운 뒤 Error로 변환한다.
		[[nodiscard]] EInputReadResult Update();
		// 명시적인 기기 재설정이다. Scene/Layer 변경의 입력 차단 수단이 아니다.
		void Reset() noexcept;
		// 다음 Update/Reset 전까지 유효한 비소비 뷰다. 상태 조회에는 유효한 enum 값을 전달한다.
		[[nodiscard]] const std::vector<SInputEvent>& GetEvents() const noexcept;
		[[nodiscard]] EKeyStatus GetKeyStatus(EInputKey key) const noexcept;
		[[nodiscard]] bool WasKeyPressed(EInputKey key) const noexcept;
		[[nodiscard]] bool WasKeyReleased(EInputKey key) const noexcept;
		[[nodiscard]] bool IsKeyDown(EInputKey key) const noexcept;
		[[nodiscard]] EKeyStatus GetMouseButtonStatus(EMouseButton button) const noexcept;
		[[nodiscard]] bool WasMouseButtonPressed(EMouseButton button) const noexcept;
		[[nodiscard]] bool WasMouseButtonReleased(EMouseButton button) const noexcept;
		[[nodiscard]] bool IsMouseButtonDown(EMouseButton button) const noexcept;
		[[nodiscard]] bool HasMousePosition() const noexcept { return m_hasMousePosition; }
		[[nodiscard]] int GetMouseX() const noexcept { return m_mouseX; }
		[[nodiscard]] int GetMouseY() const noexcept { return m_mouseY; }
		[[nodiscard]] std::int64_t GetMouseDeltaX() const noexcept { return m_mouseDeltaX; }
		[[nodiscard]] std::int64_t GetMouseDeltaY() const noexcept { return m_mouseDeltaY; }
		// Win32 휠 원시 단위(WHEEL_DELTA == 120), 문자 셀 이동과 구분한다.
		[[nodiscard]] std::int64_t GetMouseWheelDelta() const noexcept { return m_mouseWheelDelta; }

	private:
		[[nodiscard]] EInputReadResult CollectFrame();
		void ReleaseAll() noexcept;
		void InvalidateMousePosition() noexcept;
		struct SButtonState final
		{
			bool Down = false;
			bool Pressed = false;
			bool Released = false;
		};
		IInputSource& m_source;
		std::vector<SInputEvent> m_events;
		std::array<SButtonState, 9> m_keys{};
		std::array<std::array<bool, 512>, 9> m_physicalKeys{};
		std::array<SButtonState, static_cast<std::size_t>(EMouseButton::Count)> m_mouseButtons{};
		bool m_hasMousePosition = false;
		int m_mouseX = -1;
		int m_mouseY = -1;
		std::int64_t m_mouseDeltaX = 0;
		std::int64_t m_mouseDeltaY = 0;
		std::int64_t m_mouseWheelDelta = 0;
	};
}
