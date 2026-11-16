#pragma once

namespace GameData
{
	namespace Common { enum class ESceneType : std::uint8_t; }
	namespace CharacterDefinition { struct SCharacterDefinitionData; }
	namespace LevelDefinition { struct SLevelDefinitionData; }
	namespace NewGameProfile { struct SNewGameProfileData; }
	namespace ItemDefinition { struct SItemDefinitionData; }
	namespace AchievementDefinition { struct SAchievementDefinitionData; }
	namespace Scene { struct SSceneData; }
	namespace SceneOption { struct SSceneOptionData; }
	namespace SceneTypeConfig { struct SSceneTypeConfigData; }
	namespace Localization { struct SLocalizationData; }
#if __has_include("TextArt/TextArtData.g.h")
	namespace TextArt { struct STextArtData; }
#else
	// 선택 카탈로그 타입 헤더가 없는 빌드에서도 빈 조회 계약은 유지한다.
	namespace TextArt
	{
		struct STextArtData final
		{
			using KeyType = std::uint32_t;
			std::uint32_t artId{};
			std::wstring assetPath;
			std::uint32_t width{};
			std::uint32_t height{};
		};
	}
#endif
	// 원문 바이트는 보존하며 성공한 디코드 결과만 lines에 게시한다.
	[[nodiscard]] bool DecodeTextArt(const std::vector<std::uint8_t>& bytes, std::uint32_t width, std::uint32_t height,
		std::vector<std::wstring>& lines, std::string& error);

	class FGameDataStore final
	{
	public:
		FGameDataStore();
		~FGameDataStore();
		FGameDataStore(const FGameDataStore&) = delete;
		FGameDataStore& operator=(const FGameDataStore&) = delete;
		// 성공한 재로드만 기존 행을 교체한다. 행 참조는 다음 성공한 Load까지 유효하다.
		[[nodiscard]] bool Load(const std::filesystem::path& path, const std::array<std::uint8_t, 32>& key, std::string& error);
		[[nodiscard]] const Scene::SSceneData* FindScene(std::uint32_t key) const noexcept;
		[[nodiscard]] const SceneOption::SSceneOptionData* FindSceneOption(std::uint32_t key) const noexcept;
		// SceneType은 분류 조회다. 실제 PK는 ConfigId다.
		[[nodiscard]] const SceneTypeConfig::SSceneTypeConfigData* FindSceneTypeConfigById(std::uint32_t key) const noexcept;
		[[nodiscard]] const SceneTypeConfig::SSceneTypeConfigData* FindSceneTypeConfig(Common::ESceneType key) const noexcept;
		[[nodiscard]] const Localization::SLocalizationData* FindLocalization(std::uint32_t key) const noexcept;
		[[nodiscard]] const TextArt::STextArtData* FindTextArt(std::uint32_t key) const noexcept;
		// 정확한 대소문자의 GameData 기준 Asset/... 키. 다음 성공한 Load 또는 소멸까지 유효하다.
		// 실패한 Load는 바이트 객체/버퍼 주소와 기존 행을 모두 보존한다. reader의 임시 메모리를 참조하지 않는다.
		[[nodiscard]] const std::vector<std::uint8_t>* FindAsset(const std::wstring& assetPath) const noexcept;
		[[nodiscard]] const std::vector<Scene::SSceneData>& GetScenes() const noexcept;
		[[nodiscard]] const std::vector<SceneOption::SSceneOptionData>& GetSceneOptions() const noexcept;
		[[nodiscard]] const CharacterDefinition::SCharacterDefinitionData* FindCharacterDefinition(std::uint32_t key) const noexcept;
		[[nodiscard]] const LevelDefinition::SLevelDefinitionData* FindLevelDefinition(std::uint32_t key) const noexcept;
		[[nodiscard]] const NewGameProfile::SNewGameProfileData* FindNewGameProfile(std::uint32_t key) const noexcept;
		[[nodiscard]] const ItemDefinition::SItemDefinitionData* FindItemDefinition(std::uint32_t key) const noexcept;
		[[nodiscard]] const AchievementDefinition::SAchievementDefinitionData* FindAchievementDefinition(
			std::uint32_t key) const noexcept;
		[[nodiscard]] const NewGameProfile::SNewGameProfileData* GetDefaultNewGameProfile() const noexcept;
		[[nodiscard]] const std::vector<LevelDefinition::SLevelDefinitionData>& GetLevelDefinitions() const noexcept;
	private:
		std::vector<CharacterDefinition::SCharacterDefinitionData> m_characterDefinition;
		std::vector<LevelDefinition::SLevelDefinitionData> m_levelDefinition;
		std::vector<NewGameProfile::SNewGameProfileData> m_newGameProfile;
		std::vector<ItemDefinition::SItemDefinitionData> m_itemDefinition;
		std::vector<AchievementDefinition::SAchievementDefinitionData> m_achievementDefinition;
		std::vector<Scene::SSceneData> m_scenes;
		std::vector<SceneOption::SSceneOptionData> m_options;
		std::vector<SceneTypeConfig::SSceneTypeConfigData> m_configs;
		std::vector<Localization::SLocalizationData> m_localization;
		std::vector<TextArt::STextArtData> m_textArt;
		std::map<std::wstring, std::vector<std::uint8_t>> m_assets;
	};
}
