#include "pch.h"
#include "FInputManager.h"

namespace TextRPG
{
	FInputManager::FInputManager(IInputSource& source) noexcept
		: m_source(source)
	{
	}

	void FInputManager::Reset() noexcept
	{
		m_events.clear();
		m_keys = {};
		m_mouseButtons = {};
		m_physicalKeys = {};
		InvalidateMousePosition();
		m_mouseWheelDelta = 0;
	}

	void FInputManager::InvalidateMousePosition() noexcept
	{
		m_hasMousePosition = false;
		m_mouseX = -1;
		m_mouseY = -1;
		m_mouseDeltaX = 0;
		m_mouseDeltaY = 0;
	}

	void FInputManager::ReleaseAll() noexcept
	{
		for (auto& key : m_keys)
		{
			key.Released = key.Released || key.Down;
			key.Down = false;
		}
		for (auto& button : m_mouseButtons)
		{
			button.Released = button.Released || button.Down;
			button.Down = false;
		}
		m_physicalKeys = {};
		InvalidateMousePosition();
		m_mouseWheelDelta = 0;
	}

	EInputReadResult FInputManager::Update()
	{
		try
		{
			return CollectFrame();
		}
		catch (const std::exception&)
		{
			// 공급자가 이미 읽은 이벤트는 되돌릴 수 없으므로 부분 프레임을 폐기한다.
			Reset();
			return EInputReadResult::Error;
		}
	}

	EInputReadResult FInputManager::CollectFrame()
	{
		m_events.clear();
		m_mouseDeltaX = 0;
		m_mouseDeltaY = 0;
		m_mouseWheelDelta = 0;
		for (auto& button : m_mouseButtons)
		{
			button.Pressed = false;
			button.Released = false;
		}
		for (auto& key : m_keys)
		{
			key.Pressed = false;
			key.Released = false;
		}
		SInputEvent event;
		EInputReadResult result;
		while ((result = m_source.ReadEvent(event)) == EInputReadResult::Event)
		{
			const auto typeIndex = static_cast<std::size_t>(event.Type);
			const auto keyIndex = static_cast<std::size_t>(event.Key);
			const bool isPointerButton = event.Type == EInputEventType::PointerDown || event.Type == EInputEventType::PointerUp;
			if (typeIndex > static_cast<std::size_t>(EInputEventType::FocusLost) || keyIndex >= m_keys.size() ||
				event.PhysicalKey >= 512 || (isPointerButton && static_cast<std::size_t>(event.MouseButton) >= m_mouseButtons.size()))
			{
				Reset();
				return EInputReadResult::Error;
			}
			event.RepeatCount = std::max<std::uint16_t>(1, event.RepeatCount);
			event.IsLogicalTransition = false;
			event.IsRepeat = false;
			if (event.Type == EInputEventType::FocusLost)
			{
				ReleaseAll();
			}
			else if (event.Type == EInputEventType::Resize)
			{
				InvalidateMousePosition();
			}
			if (event.HasMousePosition)
			{
				if (m_hasMousePosition)
				{
					m_mouseDeltaX += static_cast<std::int64_t>(event.MouseX) - m_mouseX;
					m_mouseDeltaY += static_cast<std::int64_t>(event.MouseY) - m_mouseY;
				}
				m_hasMousePosition = true;
				m_mouseX = event.MouseX;
				m_mouseY = event.MouseY;
			}
			if (event.Type == EInputEventType::Wheel)
			{
				m_mouseWheelDelta += event.WheelDelta;
			}
			const auto buttonIndex = static_cast<std::size_t>(event.MouseButton);
			if (buttonIndex < m_mouseButtons.size() &&
				(event.Type == EInputEventType::PointerDown || event.Type == EInputEventType::PointerUp))
			{
				auto& button = m_mouseButtons[buttonIndex];
				const bool down = event.Type == EInputEventType::PointerDown;
				event.IsLogicalTransition = button.Down != down;
				event.IsRepeat = down && button.Down;
				button.Pressed = button.Pressed || (down && !button.Down);
				button.Released = button.Released || (!down && button.Down);
				button.Down = down;
			}
			const auto index = static_cast<std::size_t>(event.Key);
			if (index > 0 && index < m_keys.size() && event.PhysicalKey < m_physicalKeys[index].size())
			{
				auto& key = m_keys[index];
				auto& physical = m_physicalKeys[index][event.PhysicalKey];
				if (event.Type == EInputEventType::KeyDown)
				{
					event.IsRepeat = key.Down;
					event.IsLogicalTransition = !key.Down;
					key.Pressed = key.Pressed || !key.Down;
					physical = true;
					key.Down = true;
				}
				else if (event.Type == EInputEventType::KeyUp)
				{
					physical = false;
					const bool down = std::any_of(m_physicalKeys[index].begin(), m_physicalKeys[index].end(),
						[](bool held) { return held; });
					event.IsLogicalTransition = key.Down && !down;
					key.Released = key.Released || event.IsLogicalTransition;
					key.Down = down;
				}
			}
			m_events.push_back(event);
		}
		if (result == EInputReadResult::Error || result == EInputReadResult::Closed)
		{
			ReleaseAll();
		}
		return result == EInputReadResult::Timeout && !m_events.empty() ? EInputReadResult::Event : result;
	}

	const std::vector<SInputEvent>& FInputManager::GetEvents() const noexcept
	{
		return m_events;
	}

	EKeyStatus FInputManager::GetMouseButtonStatus(EMouseButton button) const noexcept
	{
		const auto& state = m_mouseButtons[static_cast<std::size_t>(button)];
		return state.Down ? (state.Pressed ? EKeyStatus::Pressed : EKeyStatus::Held) :
			(state.Released ? EKeyStatus::Released : EKeyStatus::None);
	}

	bool FInputManager::WasMouseButtonPressed(EMouseButton button) const noexcept
	{
		return m_mouseButtons[static_cast<std::size_t>(button)].Pressed;
	}

	bool FInputManager::WasMouseButtonReleased(EMouseButton button) const noexcept
	{
		return m_mouseButtons[static_cast<std::size_t>(button)].Released;
	}

	bool FInputManager::IsMouseButtonDown(EMouseButton button) const noexcept
	{
		return m_mouseButtons[static_cast<std::size_t>(button)].Down;
	}

	EKeyStatus FInputManager::GetKeyStatus(EInputKey key) const noexcept
	{
		const auto& state = m_keys[static_cast<std::size_t>(key)];
		return state.Down ? (state.Pressed ? EKeyStatus::Pressed : EKeyStatus::Held) :
			(state.Released ? EKeyStatus::Released : EKeyStatus::None);
	}

	bool FInputManager::WasKeyPressed(EInputKey key) const noexcept
	{
		return m_keys[static_cast<std::size_t>(key)].Pressed;
	}

	bool FInputManager::WasKeyReleased(EInputKey key) const noexcept
	{
		return m_keys[static_cast<std::size_t>(key)].Released;
	}

	bool FInputManager::IsKeyDown(EInputKey key) const noexcept
	{
		return m_keys[static_cast<std::size_t>(key)].Down;
	}
}
