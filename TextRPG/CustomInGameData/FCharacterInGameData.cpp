#include "pch.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"
#include "FCharacterInGameData.h"

namespace TextRPG
{
	bool FCharacterInGameData::OnAfterDeserialize(std::string& error)
	{
		// 정의 조회·전체 관계 검증은 루트가 수행한다. 여기서는 객체 자체의 필수 식별자만 검사한다.
		if (CharacterId == 0)
		{
			error = "CharacterId must be positive";
			return false;
		}
		error.clear();
		return true;
	}

	bool RegisterCharacterInGameData(FSerializableTypeRegistry& registry, std::string& error)
	{
		return registry.Replace<GameData::InGameData::SCharacterProgress>(
			GameData::InGameData::SCharacterProgress::kTypeName,
			[]() -> std::unique_ptr<GameData::InGameData::SCharacterProgress>
			{
				return std::make_unique<FCharacterInGameData>();
			}, error);
	}
}
