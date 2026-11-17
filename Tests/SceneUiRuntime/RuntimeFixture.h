#pragma once

// 이 fixture는 실제 Client YAML 복사본을 메모리 테스트 키로 인증 패키지화한 뒤 production Load를 사용한다.
inline void LoadRuntimeFixture(GameData::FGameDataStore& store, const std::filesystem::path& dataRoot,
	const std::filesystem::path& work, bool withTextArt = true, const std::filesystem::path& yamlRoot = {})
{
	const auto yaml = work / "runtime-yaml";
	std::filesystem::create_directories(yaml);
	const auto requiresArtCatalog = [](const std::filesystem::path& path)
	{
		const auto name = path.filename().string();
		return name == "TextArt.yaml" || name == "CharacterDefinition.yaml" || name == "LevelDefinition.yaml" ||
			name == "NewGameProfile.yaml" || name == "ItemDefinition.yaml" || name == "AchievementDefinition.yaml" ||
			name == "CharacterCombat.yaml" ||
			name == "MonsterDefinition.yaml" || name == "CombatRule.yaml";
	};
	for (const auto& entry : std::filesystem::directory_iterator(yamlRoot.empty() ? dataRoot / "Data/Client" : yamlRoot))
	{
		if (entry.path().extension() != ".yaml") continue;
		// 기존 선택 카탈로그 부재 검사는 진행 정의도 없는 Scene/UI fixture다.
		// CharacterDefinition의 필수 ArtId 참조만 끊긴 잘못된 전체 콘텐츠와 구분한다.
		if (!withTextArt && requiresArtCatalog(entry.path())) continue;
		std::ifstream input(entry.path(), std::ios::binary);
		std::string text((std::istreambuf_iterator<char>(input)), {});
		if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3);
		if (!text.starts_with("# TextRPG.GameData ")) text.insert(0, "# TextRPG.GameData 1\n");
		std::ofstream(yaml / entry.path().filename(), std::ios::binary) << text;
	}
	if (!withTextArt)
	{
		for (const auto& entry : std::filesystem::directory_iterator(yaml))
		{
			if (requiresArtCatalog(entry.path())) std::filesystem::remove(entry.path());
		}
	}
	const std::array<std::uint8_t, 32> key{};
	std::string error;
	const auto package = work / "runtime-fixture.dat";
	assert(GameData::BuildEncryptedPackage(yaml, package, key, error, dataRoot / "Asset"));
	assert(store.Load(package, key, error));
}
