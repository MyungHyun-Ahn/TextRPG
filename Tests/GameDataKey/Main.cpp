#include <Windows.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <array>
#include <cstdint>
#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include "GameData/FGameDataKey.h"
namespace fs = std::filesystem;
void Require(bool condition, const char* message)
{
	if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}
int wmain(int count, wchar_t* args[])
{
	Require(count == 2, "isolated root required");
	const fs::path base(args[1]);
	fs::create_directories(base);
	SetEnvironmentVariableW(L"LOCALAPPDATA", base.c_str());
	std::array<std::uint8_t, 32> expected{}, key{};
	Require(BCryptGenRandom(nullptr, expected.data(), static_cast<ULONG>(expected.size()),
		BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0, "random failed");
	std::wstring encoded;
	for (auto value : expected)
	{
		encoded += L"0123456789abcdef"[value >> 4];
		encoded += L"0123456789abcdef"[value & 15];
	}
	std::string error;
	SetEnvironmentVariableW(L"TEXT_RPG_GAMEDATA_KEY", encoded.c_str());
	Require(GameData::ReadGameDataKey(key, error) && key == expected, "valid environment rejected");
	for (auto& ch : encoded) if (ch >= L'a' && ch <= L'f') ch -= L'a' - L'A';
	SetEnvironmentVariableW(L"TEXT_RPG_GAMEDATA_KEY", encoded.c_str());
	Require(GameData::ReadGameDataKey(key, error) && key == expected, "uppercase rejected");
	const fs::path file = base / L"TextRPG/Development/GameData.key";
	Require(GameData::EnsureDevelopmentGameDataKey(error) && !fs::exists(file), "environment preparation created file");
	SetEnvironmentVariableW(L"TEXT_RPG_GAMEDATA_KEY", nullptr);
	Require(!GameData::ReadGameDataKey(key, error) && !fs::exists(file), "read created missing key");
	fs::create_directories(file.parent_path());
	DATA_BLOB plain{static_cast<DWORD>(expected.size()), expected.data()}, protectedData{};
	Require(CryptProtectData(&plain, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
		&protectedData) != FALSE, "fixture protection failed");
	{
		std::ofstream stream(file, std::ios::binary);
		stream.write(reinterpret_cast<const char*>(protectedData.pbData), protectedData.cbData);
	}
	LocalFree(protectedData.pbData);
	Require(GameData::ReadGameDataKey(key, error) && key == expected, "same-user DPAPI key rejected");
	for (const auto& bad : {std::wstring{}, std::wstring(63, L'a'), std::wstring(65, L'a'),
		std::wstring(64, L'g'), std::wstring(64, L' '), std::wstring(64, L'\uff21')})
	{
		SetEnvironmentVariableW(L"TEXT_RPG_GAMEDATA_KEY", bad.c_str());
		key.fill(1);
		Require(!GameData::EnsureDevelopmentGameDataKey(error), "invalid preparation override accepted");
		Require(!GameData::ReadGameDataKey(key, error) && !error.empty(), "invalid override used fallback");
		for (auto value : key) Require(value == 0, "failed read retained key");
	}
	SetEnvironmentVariableW(L"TEXT_RPG_GAMEDATA_KEY", nullptr);
	Require(GameData::EnsureDevelopmentGameDataKey(error), "existing protected key not reused");
	fs::remove(file);
	Require(GameData::EnsureDevelopmentGameDataKey(error), "provisioning failed");
	Require(GameData::ReadGameDataKey(key, error), "provisioned key unreadable");
	expected = key;
	Require(GameData::EnsureDevelopmentGameDataKey(error), "idempotent preparation failed");
	Require(GameData::ReadGameDataKey(key, error) && key == expected, "preparation replaced key");
	{
		std::ofstream stream(file, std::ios::binary | std::ios::trunc);
		stream << "malformed protected file";
	}
	Require(!GameData::EnsureDevelopmentGameDataKey(error), "malformed file replaced");
	{
		std::ifstream stream(file, std::ios::binary);
		const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		Require(bytes == "malformed protected file", "malformed file changed");
	}
	Require(!GameData::ReadGameDataKey(key, error), "malformed file accepted");
	for (DWORD size : {DWORD{0}, DWORD{31}, DWORD{33}})
	{
		std::array<BYTE, 33> other{};
		Require(BCryptGenRandom(nullptr, other.data(), static_cast<ULONG>(other.size()),
			BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0, "boundary random failed");
		DATA_BLOB invalidPlain{size, other.data()}, sealed{};
		Require(CryptProtectData(&invalidPlain, nullptr, nullptr, nullptr, nullptr,
			CRYPTPROTECT_UI_FORBIDDEN, &sealed) != FALSE, "boundary protection failed");
		{
			std::ofstream stream(file, std::ios::binary | std::ios::trunc);
			stream.write(reinterpret_cast<const char*>(sealed.pbData), sealed.cbData);
		}
		Require(!GameData::ReadGameDataKey(key, error), "wrong decoded size accepted");
		Require(!GameData::EnsureDevelopmentGameDataKey(error), "wrong decoded size replaced");
		{
			std::ifstream stream(file, std::ios::binary);
			const std::string bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
			Require(bytes.size() == sealed.cbData &&
				std::memcmp(bytes.data(), sealed.pbData, bytes.size()) == 0, "invalid protected file changed");
		}
		SecureZeroMemory(other.data(), other.size());
		LocalFree(sealed.pbData);
	}
	SecureZeroMemory(key.data(), key.size());
	SecureZeroMemory(expected.data(), expected.size());
	SecureZeroMemory(encoded.data(), encoded.size() * sizeof(wchar_t));
	std::cout << "PASS env + read-only DPAPI\n";
}
