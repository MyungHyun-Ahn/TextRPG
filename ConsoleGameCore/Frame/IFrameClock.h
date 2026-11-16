#pragma once
namespace TextRPG
{
	class IFrameClock
	{
	public:
		virtual ~IFrameClock() = default;
		virtual double NowSeconds() = 0;
		virtual void WaitUntil(double deadlineSeconds) = 0;
	};
}
