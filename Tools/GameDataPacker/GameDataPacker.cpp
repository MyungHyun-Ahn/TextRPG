#include "GameData/pch.h"

#include "GameData/FGameDataPackage.h"
#include "GameData/FGameDataKey.h"

namespace
{
	struct FKeyWiper final
	{
		std::array<std::uint8_t, 32>& key;
		~FKeyWiper() { SecureZeroMemory(key.data(), key.size()); }
	};

	void PrintUsage()
	{
		std::wcerr << L"Usage:\n"
			<< L"  GameDataPacker pack <yaml-directory> <output-package> [--asset-root <asset-directory>]\n"
			<< L"  GameDataPacker verify <package>\n"
			<< L"  GameDataPacker prepare-development-key\n"
			<< L"Use TEXT_RPG_GAMEDATA_KEY or the current-user DPAPI development key.\n";
	}
}

int wmain(int argumentCount, wchar_t* arguments[])
try
{
	const std::wstring command = argumentCount > 1 ? arguments[1] : L"";
	if ((command == L"pack" && argumentCount != 4 &&
			(argumentCount != 6 || std::wstring(arguments[4]) != L"--asset-root" || std::wstring(arguments[5]).empty()))
		|| (command == L"verify" && argumentCount != 3)
		|| (command == L"prepare-development-key" && argumentCount != 2)
		|| (command != L"pack" && command != L"verify" && command != L"prepare-development-key"))
	{
		PrintUsage();
		return 2;
	}

	std::string error;
	if (command == L"prepare-development-key")
	{
		if (!GameData::EnsureDevelopmentGameDataKey(error))
		{
			std::cerr << "GameDataPacker failed: " << error << '\n';
			return 2;
		}
		std::wcout << L"Game data key is ready (environment or current-user development key).\n";
		return 0;
	}

	std::array<std::uint8_t, 32> key{};
	FKeyWiper keyWiper{ key };
	if (!GameData::ReadGameDataKey(key, error))
	{
		std::cerr << error << '\n';
		return 2;
	}

	bool succeeded = false;
	if (command == L"pack")
	{
		succeeded = GameData::BuildEncryptedPackage(arguments[2], arguments[3], key, error,
			argumentCount == 6 ? std::filesystem::path(arguments[5]) : std::filesystem::path{});
		if (succeeded)
		{
			std::wcout << L"Encrypted YAML package written.\n";
		}
	}
	else
	{
		GameData::FGameDataPackageReader reader;
		succeeded = reader.Load(arguments[2], key, error);
		if (succeeded)
		{
			std::wcout << L"Package authenticated and loaded: " << reader.GetEntries().size() << L" file(s).\n";
			for (const GameData::SPackageEntry& entry : reader.GetEntries())
			{
				std::cout << "  " << entry.name << " (" << entry.contents.size() << " bytes)\n";
			}
		}
	}
	SecureZeroMemory(key.data(), key.size());
	if (!succeeded)
	{
		std::cerr << "GameDataPacker failed: " << error << '\n';
		return 1;
	}
	return 0;
}
catch (const std::exception&)
{
	std::cerr << "GameDataPacker failed: Unexpected failure.\n";
	return 1;
}
