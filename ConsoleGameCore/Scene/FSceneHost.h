#pragma once
namespace TextRPG
{
	// Scene owner가 재사용하는 기반이다. 모든 Scene/root 작업의 공통 안전점을 판정한다.
	class FSceneHost
	{
	public:
		[[nodiscard]] bool IsSafePointEligible(const FSceneLifetime* current, const FSceneLifetime* pending) const noexcept;
		template<class TCommit>
		bool TryEndFrame(const FSceneLifetime* current, const FSceneLifetime* pending, TCommit&& commit)
		{
			if (!IsSafePointEligible(current, pending)) return false;
			SEndFrameScope ending(*this);
			commit();
			return true;
		}
	protected:
		struct SHostOperation final
		{
			explicit SHostOperation(const FSceneHost& host) noexcept;
			~SHostOperation();
			SHostOperation(const SHostOperation&) = delete;
			SHostOperation& operator=(const SHostOperation&) = delete;
			const FSceneHost& Host;
		};
		struct SEndFrameScope final
		{
			explicit SEndFrameScope(FSceneHost& host) noexcept;
			~SEndFrameScope();
			SEndFrameScope(const SEndFrameScope&) = delete;
			SEndFrameScope& operator=(const SEndFrameScope&) = delete;
			FSceneHost& Host;
			bool Previous;
		};
		bool m_isEndingFrame = false;
		mutable unsigned m_operationDepth = 0;
	};
}
