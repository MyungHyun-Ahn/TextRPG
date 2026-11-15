#include "GameData/pch.h"
#include "Common/GameDataEnums.g.h"
#include "Scene/SceneData.g.h"
#include "SceneOption/SceneOptionData.g.h"
#include "SceneTypeConfig/SceneTypeConfigData.g.h"
#include "Localization/LocalizationData.g.h"
#if __has_include("TextArt/TextArtData.g.h")
#include "TextArt/TextArtData.g.h"
#endif
#include "CharacterDefinition/CharacterDefinitionData.g.h"
#include "LevelDefinition/LevelDefinitionData.g.h"
#include "NewGameProfile/NewGameProfileData.g.h"
#include "ItemDefinition/ItemDefinitionData.g.h"
#include "AchievementDefinition/AchievementDefinitionData.g.h"
#include "FGameDataPackage.h"
#include "FGeneratedYaml.h"
#include "FGameDataStore.h"

namespace
{
	using namespace GameData;
	std::string FieldName(const wchar_t* name)
	{
		std::string label;
		for (; *name != 0; ++name) label += static_cast<char>(*name);
		return label;
	}
	const SYamlScalar& Field(const SYamlRow& row, const wchar_t* name, EYamlScalar kind)
	{
		const auto found = row.fields.find(name);
		if (found == row.fields.end()) throw std::runtime_error(FieldName(name) + ": required field is missing");
		if (found->second.kind != kind) throw std::runtime_error(FieldName(name) + ": wrong scalar type or required null");
		return found->second;
	}
	bool IsWhitespace(wchar_t c)
	{
		// Generator의 Unicode 공백 필수값 판정과 같은 문자 집합이며 현재 locale에 의존하지 않는다.
		return (c >= 0x09 && c <= 0x0D) || c == 0x20 || c == 0x85 || c == 0xA0 || c == 0x1680 ||
			(c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
	}
	std::wstring String(const SYamlRow& row, const wchar_t* name, bool nullable = false)
	{
		const auto found = row.fields.find(name);
		if (nullable && found != row.fields.end() && found->second.kind == EYamlScalar::Null) return {};
		const auto& value = Field(row, name, EYamlScalar::String).text;
		if (!nullable && std::all_of(value.begin(), value.end(), IsWhitespace))
			throw std::runtime_error(FieldName(name) + ": required string is empty");
		return value;
	}
	std::uint32_t Number(const SYamlRow& row, const wchar_t* name)
	{
		return Field(row, name, EYamlScalar::UInt32).number;
	}
	bool Boolean(const SYamlRow& row, const wchar_t* name)
	{
		return Field(row, name, EYamlScalar::Bool).boolean;
	}
	template<class Rows, class Key>
	void Unique(const Rows& rows, Key key, const char* field)
	{
		std::set<decltype(key(rows.front()))> keys;
		for (const auto& row : rows)
			if (!keys.insert(key(row)).second) throw std::runtime_error(std::string(field) + ": duplicate primary key");
	}
	void AssetPath(const std::wstring& value)
	{
		if (value.empty() || value.front() == L'/' || value.front() == L'\\' ||
			value.find_first_of(L":*?\"<>|	\r\n") != std::wstring::npos)
			throw std::runtime_error("AssetPath must be a safe relative path");
		std::wstring normalized = value;
		std::replace(normalized.begin(), normalized.end(), L'\\', L'/');
		if (!normalized.starts_with(L"Asset/")) throw std::runtime_error("AssetPath must use the Asset namespace");
		std::size_t start = 0;
		while (start <= normalized.size())
		{
			const auto end = normalized.find(L'/', start);
			const auto part = normalized.substr(start, end == std::wstring::npos ? end : end - start);
			if (part.empty() || part == L"." || part == L".." || part.back() == L'.' || part.back() == L' ' || part.front() == L' ')
				throw std::runtime_error("AssetPath has an unsafe component");
			for (const wchar_t c : part) if (c < 0x20) throw std::runtime_error("AssetPath contains a control character");
			std::wstring stem = part.substr(0, part.find(L'.'));
			// 예약명 판정용 임시 stem만 끝 공백을 제거하고 원래 AssetPath는 보존한다.
			while (!stem.empty() && IsWhitespace(stem.back())) stem.pop_back();
			for (auto& c : stem) if (c >= L'a' && c <= L'z') c = static_cast<wchar_t>(c - L'a' + L'A');
			if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" || stem == L"CONIN$" || stem == L"CONOUT$" ||
				(stem.size() == 4 && (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) &&
					((stem[3] >= L'1' && stem[3] <= L'9') || stem[3] == L'\u00B9' || stem[3] == L'\u00B2' || stem[3] == L'\u00B3')))
				throw std::runtime_error("AssetPath contains a reserved device name");
			if (end == std::wstring::npos) break;
			start = end + 1;
		}
	}
	void Count(const SYamlRow& row, std::size_t fields)
	{
		if (row.fields.size() != fields) throw std::runtime_error("unknown or missing field");
	}
	Common::ESceneType SceneType(const SYamlRow& row)
	{
		const auto value = String(row, L"SceneType");
		if (value == L"MainMenu") return Common::ESceneType::MainMenu;
		if (value == L"Town") return Common::ESceneType::Town;
		if (value == L"BattleField") return Common::ESceneType::BattleField;
		throw std::runtime_error("invalid SceneType enum");
	}
	Common::ESceneActionType ActionType(const SYamlRow& row)
	{
		const auto value = String(row, L"ActionType");
#define ACTION(Name) if (value == L## #Name) return Common::ESceneActionType::Name
		ACTION(StartGame); ACTION(Continue); ACTION(Settings); ACTION(Credits); ACTION(Quit); ACTION(Move); ACTION(Inventory);
		ACTION(Shop); ACTION(Rest); ACTION(Talk); ACTION(Attack); ACTION(Skill); ACTION(Item); ACTION(Defend);
#undef ACTION
		throw std::runtime_error("invalid ActionType enum");
	}
}

namespace GameData
{
	bool DecodeTextArt(const std::vector<std::uint8_t>& bytes, std::uint32_t width, std::uint32_t height,
		std::vector<std::wstring>& lines, std::string& error)
	{
		if (bytes.empty() || bytes.size() > 1024 * 1024)
		{
			error = "TextArt size is invalid.";
			return false;
		}
		const std::string encoded(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, encoded.data(), static_cast<int>(encoded.size()), nullptr, 0);
		if (count == 0)
		{
			error = "TextArt is not strict UTF-8.";
			return false;
		}
		std::wstring decoded(static_cast<std::size_t>(count), L' ');
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, encoded.data(), static_cast<int>(encoded.size()), decoded.data(), count) !=
			count)
		{
			error = "Cannot decode TextArt.";
			return false;
		}
		if (!decoded.empty() && decoded.front() == 0xFEFF)
		{
			decoded.erase(decoded.begin());
		}
		std::vector<std::wstring> candidate;
		std::wstring line;
		for (std::size_t index = 0; index < decoded.size(); ++index)
		{
			const wchar_t glyph = decoded[index];
			if (glyph == L'\r' && index + 1 < decoded.size() && decoded[index + 1] == L'\n')
			{
				continue;
			}
			if (glyph == L'\n')
			{
				candidate.push_back(std::move(line));
				line.clear();
				continue;
			}
			const bool allowed = (glyph >= 0x20 && glyph <= 0x7E) || (glyph >= 0x2500 && glyph <= 0x259F) ||
								 (glyph >= 0x2190 && glyph <= 0x2193) || glyph == 0x25B2 || glyph == 0x25B6 || glyph == 0x25BC ||
								 glyph == 0x25C0 || glyph == 0x25CB;
			if (!allowed)
			{
				error = "TextArt contains a non-single-cell glyph.";
				return false;
			}
			line += glyph;
		}
		if (!decoded.empty() && decoded.back() != L'\n')
		{
			candidate.push_back(std::move(line));
		}
		if (candidate.size() != height ||
			std::any_of(candidate.begin(), candidate.end(), [width](const auto& value) { return value.size() != width; }))
		{
			error = "TextArt dimensions are invalid.";
			return false;
		}
		lines = std::move(candidate);
		error.clear();
		return true;
	}

	FGameDataStore::FGameDataStore() = default;
	FGameDataStore::~FGameDataStore() = default;

	bool FGameDataStore::Load(const std::filesystem::path& path, const std::array<std::uint8_t, 32>& key, std::string& error)
	{
		std::string location = "GameData.dat";
		try
		{
			FGameDataPackageReader reader;
			if (!reader.Load(path, key, error))
			{
				error = "GameData.dat: " + error;
				return false;
			}
			FGameDataStore candidate;
			std::set<std::string> tables;
			for (const auto& entry : reader.GetEntries())
			{
				location = entry.name;
				if (entry.name.starts_with("Asset/"))
				{
					const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, entry.name.data(),
						static_cast<int>(entry.name.size()), nullptr, 0);
					if (size <= 0) throw std::runtime_error("Asset path is not strict UTF-8");
					std::wstring name(static_cast<std::size_t>(size), L' ');
					if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, entry.name.data(),
						static_cast<int>(entry.name.size()), name.data(), size) != size)
						throw std::runtime_error("Asset path decode failed");
					AssetPath(name);
					if (!candidate.m_assets.emplace(std::move(name), entry.contents).second)
						throw std::runtime_error("duplicate asset");
					continue;
				}
				if (!entry.name.starts_with("Data/Client/")) throw std::runtime_error("unknown table namespace");
				const std::string tableName = entry.name.substr(12);
				if (tableName != "Scene.yaml" && tableName != "SceneOption.yaml" && tableName != "SceneTypeConfig.yaml" &&
					tableName != "Localization.yaml" && tableName != "TextArt.yaml" &&
					tableName != "CharacterDefinition.yaml" &&
					tableName != "LevelDefinition.yaml" &&
					tableName != "NewGameProfile.yaml" &&
					tableName != "ItemDefinition.yaml" &&
					tableName != "AchievementDefinition.yaml") throw std::runtime_error("unknown table");
				if (!tables.insert(tableName).second) throw std::runtime_error("duplicate table");
				std::vector<SYamlRow> rows;
				if (!ParseGeneratedYaml(entry.contents, rows, error)) { error = location + ": " + error; return false; }
				for (const auto& row : rows)
				{
					location = entry.name + ": row at line " + std::to_string(row.line);
					if (tableName == "Scene.yaml")
					{
						Count(row, 3);
						candidate.m_scenes.push_back({String(row, L"SceneId"), SceneType(row), Boolean(row, L"IsStartupScene")});
					}
					else if (tableName == "SceneOption.yaml")
					{
						Count(row, 7);
						candidate.m_options.push_back({Number(row, L"OptionId"), SceneType(row), Number(row, L"OptionOrder"),
							String(row, L"OptionTextKey"), ActionType(row), String(row, L"TargetSceneId", true), Boolean(row, L"IsEnabled")});
					}
					else if (tableName == "SceneTypeConfig.yaml")
					{
						Count(row, 3);
						const auto policy = String(row, L"OptionOverflowPolicy");
						if (policy != L"NoScroll" && policy != L"Scroll") throw std::runtime_error("invalid overflow enum");
						candidate.m_configs.push_back({SceneType(row), Number(row, L"MaxVisibleOptions"), policy == L"NoScroll" ?
							Common::EOptionOverflowPolicy::NoScroll : Common::EOptionOverflowPolicy::Scroll});
					}
					else if (tableName == "Localization.yaml")
					{
						Count(row, 3);
						candidate.m_localization.push_back({String(row, L"TextKey"), String(row, L"KoKR"), String(row, L"EnUS")});
					}
					else if (tableName == "CharacterDefinition.yaml")
					{
						Count(row, 3);
						candidate.m_characterDefinition.push_back({String(row, L"CharacterId"),
							String(row, L"NameTextKey"), String(row, L"ArtId")});
					}
					else if (tableName == "LevelDefinition.yaml")
					{
						Count(row, 5);
						candidate.m_levelDefinition.push_back({Number(row, L"Level"), Number(row, L"RequiredExperience"),
							Number(row, L"MaxHp"), Number(row, L"Attack"), Number(row, L"Defense")});
					}
					else if (tableName == "NewGameProfile.yaml")
					{
						Count(row, 10);
						candidate.m_newGameProfile.push_back({String(row, L"ProfileId"), String(row, L"CharacterId"),
							Number(row, L"InitialLevel"), Number(row, L"InitialExperience"), Number(row, L"InitialGold"),
							Number(row, L"InitialMaxHpBonus"), Number(row, L"InitialAttackBonus"), Number(row, L"InitialDefenseBonus"),
							Number(row, L"ContentVersion"), Boolean(row, L"IsDefault")});
					}
					else if (tableName == "ItemDefinition.yaml")
					{
						Count(row, 3);
						candidate.m_itemDefinition.push_back({String(row, L"ItemId"),
							String(row, L"NameTextKey"), Number(row, L"MaxStack")});
					}
					else if (tableName == "AchievementDefinition.yaml")
					{
						Count(row, 3);
						candidate.m_achievementDefinition.push_back({String(row, L"AchievementId"),
							String(row, L"NameTextKey"), Number(row, L"TargetCount")});
					}
					else
					{
						Count(row, 4);
						candidate.m_textArt.push_back({String(row, L"ArtId"), String(row, L"AssetPath"), Number(row, L"Width"), Number(row, L"Height")});
					}
				}
			}
			for (const auto* required : {"Scene.yaml", "SceneOption.yaml", "SceneTypeConfig.yaml", "Localization.yaml"})
				if (!tables.contains(required)) throw std::runtime_error("required table is missing");
			location = "primary keys";
			Unique(candidate.m_scenes, [](const auto& row) { return row.sceneId; }, "Scene.yaml/sceneId");
			Unique(candidate.m_options, [](const auto& row) { return row.optionId; }, "SceneOption.yaml/optionId");
			Unique(candidate.m_configs, [](const auto& row) { return row.sceneType; }, "SceneTypeConfig.yaml/sceneType");
			Unique(candidate.m_localization, [](const auto& row) { return row.textKey; }, "Localization.yaml/textKey");
			Unique(candidate.m_textArt, [](const auto& row) { return row.artId; }, "TextArt.yaml/artId");
			location = "Scene.yaml";
			if (std::count_if(candidate.m_scenes.begin(), candidate.m_scenes.end(), [](const auto& row) { return row.isStartupScene; }) != 1)
				throw std::runtime_error("exactly one startup Scene is required");
			location = "SceneOption.yaml";
			for (const auto& row : candidate.m_options)
			{
				if (row.optionId == 0) throw std::runtime_error("OptionId must be at least 1");
				if (!candidate.FindLocalization(row.optionTextKey)) throw std::runtime_error("OptionTextKey reference is missing");
				if ((!row.targetSceneId.empty() && !candidate.FindScene(row.targetSceneId)) ||
					(row.isEnabled && row.actionType == Common::ESceneActionType::StartGame && row.targetSceneId.empty()))
					throw std::runtime_error("TargetSceneId reference is missing");
			}
			location = "SceneTypeConfig.yaml/SceneOption.yaml";
			for (const auto type : {Common::ESceneType::MainMenu, Common::ESceneType::Town, Common::ESceneType::BattleField})
			{
				const auto* config = candidate.FindSceneTypeConfig(type);
				if (!config) throw std::runtime_error("SceneType configuration is missing");
				if (config->maxVisibleOptions != 4) throw std::runtime_error("MaxVisibleOptions must be 4");
				if (config->optionOverflowPolicy != Common::EOptionOverflowPolicy::NoScroll)
					throw std::runtime_error("OptionOverflowPolicy must be NoScroll");
				std::set<std::uint32_t> orders;
				std::size_t count = 0;
				for (const auto& row : candidate.m_options)
				{
					if (row.sceneType != type) continue;
					++count;
					if (row.optionOrder < 1 || row.optionOrder > 4 || !orders.insert(row.optionOrder).second)
						throw std::runtime_error("OptionOrder must be distinct 1 through 4");
				}
				if (count != 4) throw std::runtime_error("SceneType requires exactly four options including disabled rows");
			}
			location = "TextArt.yaml";
			for (const auto& row : candidate.m_textArt)
			{
				AssetPath(row.assetPath);
				if (!((row.width == 36 && row.height == 36) || (row.width == 83 && row.height == 41)))
					throw std::runtime_error("Width/Height must be 36x36 or 83x41");
				if (row.artId == L"MainLogo" && (row.width != 83 || row.height != 41))
					throw std::runtime_error("TextArt MainLogo does not fit the 83x41 UI slot");
				const auto* bytes = candidate.FindAsset(row.assetPath);
				if (!bytes) throw std::runtime_error("TextArt AssetPath is missing from the package");
				std::vector<std::wstring> lines;
				if (!DecodeTextArt(*bytes, row.width, row.height, lines, error)) throw std::runtime_error(error);
			}
			location = "Localization.yaml";
			if (!candidate.FindLocalization(L"System.ScreenTooSmall"))
				throw std::runtime_error("required TextKey System.ScreenTooSmall is missing");
			if (!candidate.FindLocalization(L"Scene.MainMenu.Title"))
				throw std::runtime_error("required TextKey Scene.MainMenu.Title is missing");
			location = "InGameData catalog cohort";
			std::size_t catalogTables = 0;
			for (const auto* name : {"CharacterDefinition.yaml", "LevelDefinition.yaml", "NewGameProfile.yaml",
				"ItemDefinition.yaml", "AchievementDefinition.yaml"})
				if (tables.contains(name)) ++catalogTables;
			if (catalogTables != 0 && catalogTables != 5) throw std::runtime_error("partial catalog cohort");
			if (catalogTables != 0)
			{
				Unique(candidate.m_characterDefinition, [](const auto& row) { return row.characterId; }, "CharacterId");
				Unique(candidate.m_levelDefinition, [](const auto& row) { return row.level; }, "Level");
				Unique(candidate.m_newGameProfile, [](const auto& row) { return row.profileId; }, "ProfileId");
				Unique(candidate.m_itemDefinition, [](const auto& row) { return row.itemId; }, "ItemId");
				Unique(candidate.m_achievementDefinition, [](const auto& row) { return row.achievementId; }, "AchievementId");
				if (candidate.m_characterDefinition.empty() || candidate.m_levelDefinition.empty() || candidate.m_newGameProfile.empty())
					throw std::runtime_error("catalog cohort requires character, level and profile rows");
				for (const auto& row : candidate.m_characterDefinition)
				{
					if (!candidate.FindLocalization(row.nameTextKey)) throw std::runtime_error("NameTextKey reference is missing");
					if (!candidate.FindTextArt(row.artId)) throw std::runtime_error("ArtId reference is missing");
				}
				std::sort(candidate.m_levelDefinition.begin(), candidate.m_levelDefinition.end(),
					[](const auto& left, const auto& right) { return left.level < right.level; });
				for (std::size_t i = 0; i < candidate.m_levelDefinition.size(); ++i)
				{
					const auto& row = candidate.m_levelDefinition[i];
					if (row.level == 0) throw std::runtime_error("Level must be positive");
					if (row.maxHp == 0) throw std::runtime_error("MaxHp must be positive");
					if (i != 0 && row.requiredExperience <= candidate.m_levelDefinition[i - 1].requiredExperience)
						throw std::runtime_error("level experience thresholds must increase");
				}
				if (std::count_if(candidate.m_newGameProfile.begin(), candidate.m_newGameProfile.end(),
					[](const auto& row) { return row.isDefault; }) != 1)
					throw std::runtime_error("exactly one default profile is required");
				const auto version = candidate.GetDefaultNewGameProfile()->contentVersion;
				for (const auto& row : candidate.m_newGameProfile)
				{
					if (!candidate.FindCharacterDefinition(row.characterId)) throw std::runtime_error("CharacterId reference is missing");
					const auto* level = candidate.FindLevelDefinition(row.initialLevel);
					if (!level) throw std::runtime_error("InitialLevel reference is missing");
					if (row.contentVersion == 0 || row.contentVersion != version)
						throw std::runtime_error("ContentVersion must be positive and identical across profiles");
					if (row.initialExperience < level->requiredExperience ||
						std::any_of(candidate.m_levelDefinition.begin(), candidate.m_levelDefinition.end(), [&](const auto& next)
						{ return next.level > row.initialLevel && row.initialExperience >= next.requiredExperience; }))
						throw std::runtime_error("initial level/experience mismatch");
					constexpr auto maximum = (std::numeric_limits<std::uint32_t>::max)();
					if (row.initialMaxHpBonus > maximum - level->maxHp || row.initialAttackBonus > maximum - level->attack ||
						row.initialDefenseBonus > maximum - level->defense)
						throw std::runtime_error("initial derived stats overflow");
				}
				for (const auto& row : candidate.m_itemDefinition)
				{
					if (!candidate.FindLocalization(row.nameTextKey)) throw std::runtime_error("NameTextKey reference is missing");
					if (row.maxStack == 0) throw std::runtime_error("MaxStack must be positive");
				}
				for (const auto& row : candidate.m_achievementDefinition)
				{
					if (!candidate.FindLocalization(row.nameTextKey)) throw std::runtime_error("NameTextKey reference is missing");
					if (row.targetCount == 0) throw std::runtime_error("TargetCount must be positive");
				}
			}
			m_characterDefinition.swap(candidate.m_characterDefinition);
			m_levelDefinition.swap(candidate.m_levelDefinition);
			m_newGameProfile.swap(candidate.m_newGameProfile);
			m_itemDefinition.swap(candidate.m_itemDefinition);
			m_achievementDefinition.swap(candidate.m_achievementDefinition);
			m_scenes.swap(candidate.m_scenes);
			m_options.swap(candidate.m_options);
			m_configs.swap(candidate.m_configs);
			m_localization.swap(candidate.m_localization);
			m_textArt.swap(candidate.m_textArt);
			m_assets.swap(candidate.m_assets);
			error.clear();
			return true;
		}
		catch (const std::exception& exception)
		{
			error = location + ": " + exception.what();
			return false;
		}
	}

	const CharacterDefinition::SCharacterDefinitionData* FGameDataStore::FindCharacterDefinition(const std::wstring& key) const noexcept
	{
		for (const auto& row : m_characterDefinition) if (row.characterId == key) return &row;
		return nullptr;
	}
	const LevelDefinition::SLevelDefinitionData* FGameDataStore::FindLevelDefinition(std::uint32_t key) const noexcept
	{
		for (const auto& row : m_levelDefinition) if (row.level == key) return &row;
		return nullptr;
	}
	const NewGameProfile::SNewGameProfileData* FGameDataStore::FindNewGameProfile(const std::wstring& key) const noexcept
	{
		for (const auto& row : m_newGameProfile) if (row.profileId == key) return &row;
		return nullptr;
	}
	const ItemDefinition::SItemDefinitionData* FGameDataStore::FindItemDefinition(const std::wstring& key) const noexcept
	{
		for (const auto& row : m_itemDefinition) if (row.itemId == key) return &row;
		return nullptr;
	}
	const AchievementDefinition::SAchievementDefinitionData* FGameDataStore::FindAchievementDefinition(
		const std::wstring& key) const noexcept
	{
		for (const auto& row : m_achievementDefinition) if (row.achievementId == key) return &row;
		return nullptr;
	}
	const NewGameProfile::SNewGameProfileData* FGameDataStore::GetDefaultNewGameProfile() const noexcept
	{
		for (const auto& row : m_newGameProfile) if (row.isDefault) return &row;
		return nullptr;
	}
	const std::vector<LevelDefinition::SLevelDefinitionData>& FGameDataStore::GetLevelDefinitions() const noexcept
	{
		return m_levelDefinition;
	}

	const Scene::SSceneData* FGameDataStore::FindScene(const std::wstring& key) const noexcept
	{
		for (const auto& row : m_scenes) if (row.sceneId == key) return &row;
		return nullptr;
	}
	const SceneOption::SSceneOptionData* FGameDataStore::FindSceneOption(std::uint32_t key) const noexcept
	{
		for (const auto& row : m_options) if (row.optionId == key) return &row;
		return nullptr;
	}
	const SceneTypeConfig::SSceneTypeConfigData* FGameDataStore::FindSceneTypeConfig(Common::ESceneType key) const noexcept
	{
		for (const auto& row : m_configs) if (row.sceneType == key) return &row;
		return nullptr;
	}
	const Localization::SLocalizationData* FGameDataStore::FindLocalization(const std::wstring& key) const noexcept
	{
		for (const auto& row : m_localization) if (row.textKey == key) return &row;
		return nullptr;
	}
	const TextArt::STextArtData* FGameDataStore::FindTextArt(const std::wstring& key) const noexcept
	{
		for (const auto& row : m_textArt) if (row.artId == key) return &row;
		return nullptr;
	}
	const std::vector<std::uint8_t>* FGameDataStore::FindAsset(const std::wstring& assetPath) const noexcept
	{
		const auto found = m_assets.find(assetPath);
		if (found != m_assets.end()) return &found->second;
		// Windows 상대경로의 구분자 별칭만 해석하고 원래 metadata/키의 대소문자는 변경하지 않는다.
		for (const auto& [name, bytes] : m_assets)
		{
			if (name.size() == assetPath.size() && std::equal(name.begin(), name.end(), assetPath.begin(),
				[](wchar_t left, wchar_t right) { return left == (right == L'\\' ? L'/' : right); })) return &bytes;
		}
		return nullptr;
	}

	const std::vector<Scene::SSceneData>& FGameDataStore::GetScenes() const noexcept { return m_scenes; }
	const std::vector<SceneOption::SSceneOptionData>& FGameDataStore::GetSceneOptions() const noexcept { return m_options; }
}
