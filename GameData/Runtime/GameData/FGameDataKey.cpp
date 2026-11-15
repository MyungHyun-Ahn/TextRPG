#include "GameData/pch.h"
#include "FGameDataKey.h"

namespace
{
	struct FMemoryWiper final
	{
		void* data;
		std::size_t size;
		~FMemoryWiper() { SecureZeroMemory(data, size); }
	};
	struct FHandle final
	{
		HANDLE value = INVALID_HANDLE_VALUE;
		~FHandle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
	};
	struct FBlob final
	{
		DATA_BLOB value{};
		~FBlob()
		{
			if (value.pbData != nullptr)
			{
				SecureZeroMemory(value.pbData, value.cbData);
				LocalFree(value.pbData);
			}
		}
	};
	bool Fail(std::string& error, const char* message)
	{
		error = message;
		return false;
	}
	bool ReadEnvironment(std::array<std::uint8_t, 32>& key, bool& absent, std::string& error)
	{
		wchar_t encoded[65]{};
		FMemoryWiper wiper{encoded, sizeof(encoded)};
		SetLastError(ERROR_SUCCESS);
		const DWORD copied = GetEnvironmentVariableW(L"TEXT_RPG_GAMEDATA_KEY", encoded, 65);
		absent = copied == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND;
		if (absent) return true;
		if (copied != 64) return Fail(error, "A valid 32-byte hexadecimal key is required in TEXT_RPG_GAMEDATA_KEY.");
		for (std::size_t index = 0; index < key.size(); ++index)
		{
			const auto nibble = [](wchar_t ch) -> int
			{
				if (ch >= L'0' && ch <= L'9') return ch - L'0';
				if (ch >= L'a' && ch <= L'f') return ch - L'a' + 10;
				if (ch >= L'A' && ch <= L'F') return ch - L'A' + 10;
				return -1;
			};
			const int high = nibble(encoded[index * 2]);
			const int low = nibble(encoded[index * 2 + 1]);
			if (high < 0 || low < 0)
				return Fail(error, "A valid 32-byte hexadecimal key is required in TEXT_RPG_GAMEDATA_KEY.");
			key[index] = static_cast<std::uint8_t>((high << 4) | low);
		}
		return true;
	}
	bool DevelopmentPath(std::filesystem::path& path, std::string& error)
	{
		const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
		if (size <= 1) return Fail(error, "LOCALAPPDATA is required for the development game data key.");
		std::wstring base(size, L'\0');
		const DWORD copied = GetEnvironmentVariableW(L"LOCALAPPDATA", base.data(), size);
		if (copied == 0 || copied >= size) return Fail(error, "Could not read LOCALAPPDATA.");
		base.resize(copied);
		path = std::filesystem::path(base) / L"TextRPG/Development/GameData.key";
		if (!path.is_absolute()) return Fail(error, "LOCALAPPDATA must be absolute.");
		return true;
	}
	bool ReadProtected(const std::filesystem::path& path, std::array<std::uint8_t, 32>& key,
		bool& missing, std::string& error)
	{
		FHandle file;
		// rename 게시 중 Windows의 짧은 공유 잠금만 제한적으로 재시도한다.
		for (unsigned attempt = 0; attempt < 50; ++attempt)
		{
			file.value = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
				OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file.value != INVALID_HANDLE_VALUE) break;
			const DWORD reason = GetLastError();
			if ((reason != ERROR_SHARING_VIOLATION && reason != ERROR_LOCK_VIOLATION) || attempt == 49) break;
			Sleep(10);
		}
		missing = false;
		if (file.value == INVALID_HANDLE_VALUE)
		{
			const DWORD reason = GetLastError();
			missing = reason == ERROR_FILE_NOT_FOUND || reason == ERROR_PATH_NOT_FOUND;
			return Fail(error, missing ? "Development game data key is missing; run GameDataPacker prepare-development-key."
				: "Could not open the development game data key.");
		}
		LARGE_INTEGER size{};
		if (!GetFileSizeEx(file.value, &size) || size.QuadPart <= 0 || size.QuadPart > 65536)
			return Fail(error, "Invalid protected development game data key.");
		std::vector<BYTE> bytes(static_cast<std::size_t>(size.QuadPart));
		DWORD copied = 0;
		if (!ReadFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()), &copied, nullptr) || copied != bytes.size())
			return Fail(error, "Could not read the protected development game data key.");
		DATA_BLOB input{static_cast<DWORD>(bytes.size()), bytes.data()};
		FBlob output;
		if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output.value)
			|| output.value.pbData == nullptr || output.value.cbData != key.size())
			return Fail(error, "Invalid protected development game data key.");
		for (std::size_t index = 0; index < key.size(); ++index) key[index] = output.value.pbData[index];
		return true;
	}
}

namespace GameData
{
	bool ReadGameDataKey(std::array<std::uint8_t, 32>& key, std::string& error)
	{
		SecureZeroMemory(key.data(), key.size());
		error.clear();
		try
		{
			bool absent = false;
			bool ok = ReadEnvironment(key, absent, error);
			if (ok && absent)
			{
				std::filesystem::path path;
				bool missing = false;
				ok = DevelopmentPath(path, error) && ReadProtected(path, key, missing, error);
			}
			if (!ok) SecureZeroMemory(key.data(), key.size());
			return ok;
		}
		catch (const std::exception&)
		{
			SecureZeroMemory(key.data(), key.size());
			return Fail(error, "Could not read the game data key.");
		}
	}

	bool EnsureDevelopmentGameDataKey(std::string& error)
	{
		error.clear();
		std::array<std::uint8_t, 32> key{};
		FMemoryWiper keyWiper{key.data(), key.size()};
		try
		{
			bool absent = false;
			if (!ReadEnvironment(key, absent, error)) return false;
			if (!absent) return true;
			std::filesystem::path path;
			if (!DevelopmentPath(path, error)) return false;
			bool missing = false;
			if (ReadProtected(path, key, missing, error)) return true;
			if (!missing) return false;
			error.clear();
			std::error_code reason;
			std::filesystem::create_directories(path.parent_path(), reason);
			if (reason) return Fail(error, "Could not create the development key directory.");
			if (BCryptGenRandom(nullptr, key.data(), static_cast<ULONG>(key.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
				return Fail(error, "Could not generate the development game data key.");
			DATA_BLOB input{static_cast<DWORD>(key.size()), key.data()};
			FBlob protectedData;
			if (!CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &protectedData.value)
				|| protectedData.value.pbData == nullptr || protectedData.value.cbData == 0)
				return Fail(error, "Could not protect the development game data key.");
			// 완성된 보호 blob만 rename으로 게시한다. 경쟁자는 기존 파일을 교체하지 않는다.
			std::array<BYTE, 16> randomName{};
			if (BCryptGenRandom(nullptr, randomName.data(), static_cast<ULONG>(randomName.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
				return Fail(error, "Could not create a development key temporary name.");
			std::wstring suffix = L".";
			for (BYTE value : randomName)
			{
				suffix += L"0123456789abcdef"[value >> 4];
				suffix += L"0123456789abcdef"[value & 15];
			}
			const std::filesystem::path temporary(path.wstring() + suffix + L".tmp");
			struct FTemporaryFile final
			{
				const std::filesystem::path& path;
				bool owned = false;
				~FTemporaryFile() { if (owned) DeleteFileW(path.c_str()); }
			} cleanup{temporary};
			{
				FHandle file{CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
				if (file.value == INVALID_HANDLE_VALUE) return Fail(error, "Could not create the protected key temporary file.");
				cleanup.owned = true;
				DWORD written = 0;
				if (!WriteFile(file.value, protectedData.value.pbData, protectedData.value.cbData, &written, nullptr)
					|| written != protectedData.value.cbData || !FlushFileBuffers(file.value))
					return Fail(error, "Could not write the protected development game data key.");
			}
			if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_WRITE_THROUGH))
			{
				cleanup.owned = false;
				return true;
			}
			// 게시 경쟁에서 졌더라도 승자의 보호 키를 검증해야 성공이다.
			if (ReadProtected(path, key, missing, error)) return true;
			return false;
		}
		catch (const std::exception&)
		{
			return Fail(error, "Could not prepare the development game data key.");
		}
	}
}
