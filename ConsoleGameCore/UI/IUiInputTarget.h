#pragma once
namespace TextRPG
{
	class FSceneUiRoot;
	class IUiInputTarget
	{
	public:
		virtual ~IUiInputTarget() = default;
		virtual void DispatchUiInput(const SInputEvent& event) = 0;
		virtual void CancelUiInput() = 0;
		[[nodiscard]] virtual FSceneUiRoot* GetUiBaseRoot() const noexcept = 0;
	};
}
