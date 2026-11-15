#include "pch.h"
#include "FUIManager.h"
namespace TextRPG
{
	void FUIManager::Bind(IUiInputTarget& target) noexcept { m_target = &target; }
	void FUIManager::Detach(const IUiInputTarget& target) noexcept { if (m_target == &target) m_target = nullptr; }
	void FUIManager::Clear()
	{
		if (auto* target = std::exchange(m_target, nullptr))
		{
			try { target->CancelUiInput(); }
			catch (...)
			{
				// 사용자 callback이 게시한 새 연결은 이전 연결로 덮어쓰지 않는다.
				if (!m_target) m_target = target;
				throw;
			}
		}
	}
	void FUIManager::HandleInput(const SInputEvent& event) { if (m_target) m_target->DispatchUiInput(event); }
	FSceneUiRoot* FUIManager::GetBoundRoot() const noexcept { return m_target ? m_target->GetUiBaseRoot() : nullptr; }
	IUiInputTarget* FUIManager::GetBoundTarget() const noexcept { return m_target; }
}
