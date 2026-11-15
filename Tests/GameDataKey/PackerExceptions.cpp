#include "GameData/pch.h"

#include "GameData/FGameDataPackage.h"
namespace
{
	unsigned wipes = 0;
	void WipeTracked(void* data, std::size_t size)
	{
		if (size == 32) ++wipes;
		RtlSecureZeroMemory(data, size);
	}
}
namespace GameData
{
	bool ThrowPackage(const std::filesystem::path&, const std::filesystem::path&,
		const std::array<std::uint8_t, 32>&, std::string&, const std::filesystem::path&)
	{
		throw std::runtime_error("injected package exception");
	}
}
#undef SecureZeroMemory
#define SecureZeroMemory WipeTracked
#define BuildEncryptedPackage ThrowPackage
#define wmain PackerMain
#include "GameDataPacker.cpp"
#undef wmain
#undef BuildEncryptedPackage
#undef SecureZeroMemory
int main()
{
	std::array<BYTE, 32> key{};
	if (BCryptGenRandom(nullptr, key.data(), static_cast<ULONG>(key.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 1;
	std::wstring encoded;
	for (BYTE value : key)
	{
		encoded += L"0123456789abcdef"[value >> 4];
		encoded += L"0123456789abcdef"[value & 15];
	}
	SetEnvironmentVariableW(L"TEXT_RPG_GAMEDATA_KEY", encoded.c_str());
	RtlSecureZeroMemory(key.data(), key.size());
	RtlSecureZeroMemory(encoded.data(), encoded.size() * sizeof(wchar_t));
	wchar_t executable[] = L"Packer";
	wchar_t command[] = L"pack";
	wchar_t input[] = L"unused";
	wchar_t output[] = L"unused.dat";
	wchar_t* args[]{executable, command, input, output};
	try
	{
		const int code = PackerMain(4, args);
		SetEnvironmentVariableW(L"TEXT_RPG_GAMEDATA_KEY", nullptr);
		if (code != 1 || wipes == 0) return 1;
	}
	catch (const std::exception&)
	{
		SetEnvironmentVariableW(L"TEXT_RPG_GAMEDATA_KEY", nullptr);
		std::cerr << "exception escaped packer boundary\n";
		return 1;
	}
	std::cout << "PASS packer exception key wipe\n";
}
