#pragma once

namespace TextRPG
{
	enum class EInputReadResult
	{
		Event,
		Timeout,
		Closed,
		Error
	};
	class IInputSource
	{
	public:
		virtual ~IInputSource() = default;
		// 출력 인자는 Event일 때만 유효하다. Timeout은 정상 무입력이며 Closed/Error와 구분한다.
		[[nodiscard]] virtual EInputReadResult ReadEvent(SInputEvent& event) noexcept = 0;
	};
}
