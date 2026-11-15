#pragma once
namespace TextRPG
{
	class FUIManager
	{
	public:
		void Bind(IUiInputTarget& target) noexcept;
		void Detach(const IUiInputTarget& target) noexcept;
		void Clear();
		void HandleInput(const SInputEvent& event);
		[[nodiscard]] FSceneUiRoot* GetBoundRoot() const noexcept;
		[[nodiscard]] IUiInputTarget* GetBoundTarget() const noexcept;
	private:
		IUiInputTarget* m_target = nullptr;
	};
}
