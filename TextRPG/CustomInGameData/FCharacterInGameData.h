#pragma once

namespace TextRPG
{
	// 영속 필드는 생성 타입만 소유한다. 게임 측 특수 검증은 훅으로 확장한다.
	class FCharacterInGameData final : public GameData::InGameData::SCharacterProgress
	{
	public:
		FCharacterInGameData() = default;
		using GameData::InGameData::SCharacterProgress::SCharacterProgress;

	protected:
		[[nodiscard]] bool OnAfterDeserialize(std::string& error) override;
	};

	[[nodiscard]] bool RegisterCharacterInGameData(FSerializableTypeRegistry& registry, std::string& error);
}
