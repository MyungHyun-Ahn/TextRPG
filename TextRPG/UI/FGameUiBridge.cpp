#include "pch.h"
#include "Common/GameDataEnums.g.h"
#include "UI/FUIManager.h"
#include "FGameUiBridge.h"
#include "Scene/FDataScene.h"
namespace TextRPG
{
	void FGameUiBridge::Bind(FDataScene& scene) noexcept { FUIManager::Bind(scene); }
	FDataScene* FGameUiBridge::GetBoundScene() const noexcept { return dynamic_cast<FDataScene*>(GetBoundTarget()); }
}
