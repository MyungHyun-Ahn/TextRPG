#pragma once

namespace TextRPG
{
	class IObject
	{
	public:
		virtual ~IObject() = default;
		virtual void Update(float deltaSeconds) = 0;
	};
}
