#include "pch.h"
#include "FSceneHost.h"
namespace TextRPG
{
	FSceneHost::SHostOperation::SHostOperation(const FSceneHost& host) noexcept : Host(host) { ++Host.m_operationDepth; }
	FSceneHost::SHostOperation::~SHostOperation() { --Host.m_operationDepth; }
	FSceneHost::SEndFrameScope::SEndFrameScope(FSceneHost& host) noexcept
		: Host(host), Previous(std::exchange(host.m_isEndingFrame, true)) {}
	FSceneHost::SEndFrameScope::~SEndFrameScope() { Host.m_isEndingFrame = Previous; }
	bool FSceneHost::IsSafePointEligible(const FSceneLifetime* current, const FSceneLifetime* pending) const noexcept
	{
		return !m_isEndingFrame && m_operationDepth == 0 && (!current || !current->IsBusy()) && (!pending || !pending->IsBusy());
	}
}
