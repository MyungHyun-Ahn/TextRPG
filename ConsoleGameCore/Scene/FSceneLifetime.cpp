#include "pch.h"
namespace TextRPG
{
	bool FSceneLifetime::IsBusy() const noexcept
	{
		if (IsOperating()) return true;
		// 최초 사용자 virtual 조회도 재진입할 수 있다. 조회가 반환하기 전에는 이 Scene을 해제하지 못한다.
		SOperation operation(*this);
		return IsOwnedUiBusy();
	}
	FSceneLifetime::SOperation::SOperation(const FSceneLifetime& scene) noexcept : Scene(scene) { ++Scene.m_operationDepth; }
	FSceneLifetime::SOperation::~SOperation() { --Scene.m_operationDepth; }
}
