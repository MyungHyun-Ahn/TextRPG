#include "GameData/pch.h"

#include "FSaveDataStore.h"

namespace
{
	using TBytes = std::vector<std::uint8_t>;
	constexpr std::size_t kMaxPlainBytes = 16U * 1024U * 1024U;
	constexpr std::size_t kHeaderBytes = 28;
	constexpr std::size_t kTagBytes = 16;
	constexpr std::array<std::uint8_t, 4> kMagic{'T', 'R', 'S', 'V'};

	bool Fail(std::string& error, const char* message)
	{
		error = message;
		return false;
	}

	struct FBytesWiper final
	{
		TBytes& bytes;
		~FBytesWiper() { if (!bytes.empty()) SecureZeroMemory(bytes.data(), bytes.size()); }
	};

	struct FSecret final
	{
		std::array<std::uint8_t, 32> bytes{};
		~FSecret() { SecureZeroMemory(bytes.data(), bytes.size()); }
	};

	struct FCipher final
	{
		TBytes keyObject;
		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_KEY_HANDLE key = nullptr;
		~FCipher()
		{
			if (key != nullptr) BCryptDestroyKey(key);
			if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
			if (!keyObject.empty()) SecureZeroMemory(keyObject.data(), keyObject.size());
		}
	};

	void PutU32(TBytes& bytes, std::size_t offset, std::uint32_t value)
	{
		for (unsigned index = 0; index < 4; ++index)
		{
			bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8));
		}
	}

	std::uint32_t GetU32(const TBytes& bytes, std::size_t offset)
	{
		std::uint32_t value = 0;
		for (unsigned index = 0; index < 4; ++index)
		{
			value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8);
		}
		return value;
	}

	struct FHandle final
	{
		HANDLE value = INVALID_HANDLE_VALUE;
		~FHandle() { if (value != INVALID_HANDLE_VALUE) CloseHandle(value); }
		FHandle() = default;
		FHandle(const FHandle&) = delete;
		FHandle& operator=(const FHandle&) = delete;
		bool Close()
		{
			if (value == INVALID_HANDLE_VALUE) return true;
			if (!CloseHandle(value)) return false;
			value = INVALID_HANDLE_VALUE;
			return true;
		}
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

	struct FTemporary final
	{
		std::filesystem::path path;
		bool owned = false;
		~FTemporary() { if (owned) DeleteFileW(path.c_str()); }
	};

	bool IsReserved(const std::wstring& part)
	{
		const auto base = part.substr(0, part.find(L'.'));
		for (const wchar_t* reserved : {L"CON", L"PRN", L"AUX", L"NUL", L"CLOCK$", L"CONIN$", L"CONOUT$"})
		{
			if (CompareStringOrdinal(base.c_str(), -1, reserved, -1, TRUE) == CSTR_EQUAL) return true;
		}
		if (base.size() != 4) return false;
		const bool device = CompareStringOrdinal(base.data(), 3, L"COM", 3, TRUE) == CSTR_EQUAL
			|| CompareStringOrdinal(base.data(), 3, L"LPT", 3, TRUE) == CSTR_EQUAL;
		return device && ((base[3] >= L'1' && base[3] <= L'9') || base[3] == L'\u00b9'
			|| base[3] == L'\u00b2' || base[3] == L'\u00b3');
	}

	bool ValidateDirectory(const std::filesystem::path& path, std::string& error)
	{
		const auto root = path.root_name().wstring();
		if (!path.is_absolute() || root.size() != 2 || root[1] != L':'
			|| !((root[0] >= L'A' && root[0] <= L'Z') || (root[0] >= L'a' && root[0] <= L'z')))
			return Fail(error, "Save directory must be an ordinary absolute local path.");
		const UINT drive = GetDriveTypeW(path.root_path().c_str());
		if (drive != DRIVE_FIXED && drive != DRIVE_REMOVABLE && drive != DRIVE_RAMDISK)
			return Fail(error, "Save directory must use a local disk.");
		auto current = path.root_path();
		for (const auto& component : path.relative_path())
		{
			const auto part = component.wstring();
			if (part.empty()) continue;
			if (part == L"." || part == L".." || part.back() == L'.' || part.back() == L' ' || IsReserved(part))
				return Fail(error, "Invalid save directory component.");
			for (wchar_t character : part)
			{
				if (character < 32 || std::wstring_view(L"<>:\"|?*").find(character) != std::wstring_view::npos)
					return Fail(error, "Invalid character in save directory.");
			}
			current /= component;
			const DWORD attributes = GetFileAttributesW(current.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES)
			{
				const DWORD reason = GetLastError();
				if (reason == ERROR_FILE_NOT_FOUND || reason == ERROR_PATH_NOT_FOUND) continue;
				return Fail(error, "Could not inspect save directory.");
			}
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
				return Fail(error, "Save directory must not contain reparse points or files.");
		}
		return true;
	}

	bool ReadEnvironment(const wchar_t* name, std::wstring& value, bool& missing, std::string& error)
	{
		SetLastError(ERROR_SUCCESS);
		const DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
		missing = size == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND;
		if (missing) return true;
		if (size <= 1 || size > 32767) return Fail(error, "Save directory environment value is empty or invalid.");
		value.resize(size);
		const DWORD copied = GetEnvironmentVariableW(name, value.data(), size);
		if (copied == 0 || copied >= size) return Fail(error, "Could not read save directory environment value.");
		value.resize(copied);
		return true;
	}

	bool ReadBytes(const std::filesystem::path& path, std::size_t limit, TBytes& bytes, bool& missing, std::string& error)
	{
		FHandle file;
		// 완성된 키를 rename 게시하는 경쟁자의 짧은 공유 잠금만 재시도한다.
		for (unsigned attempt = 0; attempt < 50; ++attempt)
		{
			file.value = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
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
			return Fail(error, missing ? "Save file or save key is missing." : "Could not open save file or save key.");
		}
		BY_HANDLE_FILE_INFORMATION information{};
		LARGE_INTEGER size{};
		if (!GetFileInformationByHandle(file.value, &information) || GetFileType(file.value) != FILE_TYPE_DISK
			|| (information.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) != 0
			|| !GetFileSizeEx(file.value, &size) || size.QuadPart <= 0 || static_cast<ULONGLONG>(size.QuadPart) > limit)
			return Fail(error, "Invalid save file or save key size/type.");
		TBytes candidate(static_cast<std::size_t>(size.QuadPart));
		DWORD copied = 0;
		if (!ReadFile(file.value, candidate.data(), static_cast<DWORD>(candidate.size()), &copied, nullptr)
			|| copied != candidate.size() || !file.Close()) return Fail(error, "Could not read/close the complete save file or key.");
		bytes.swap(candidate);
		return true;
	}

	bool ReadKey(const std::filesystem::path& path, FSecret& key, bool& missing, std::string& error)
	{
		TBytes protectedBytes;
		if (!ReadBytes(path, 65536, protectedBytes, missing, error)) return false;
		DATA_BLOB input{static_cast<DWORD>(protectedBytes.size()), protectedBytes.data()};
		FBlob output;
		if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output.value)
			|| output.value.pbData == nullptr || output.value.cbData != key.bytes.size())
			return Fail(error, "Invalid DPAPI-protected save key.");
		std::copy_n(output.value.pbData, key.bytes.size(), key.bytes.begin());
		return true;
	}

	bool Publish(const std::filesystem::path& destination, const BYTE* bytes, DWORD count, bool replace, std::string& error)
	{
		FTemporary temporary;
		FHandle file;
		for (unsigned attempt = 0; attempt < 32; ++attempt)
		{
			std::array<BYTE, 16> random{};
			if (BCryptGenRandom(nullptr, random.data(), static_cast<ULONG>(random.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
				return Fail(error, "Could not generate a save temporary filename.");
			std::wstring suffix = L".";
			for (BYTE value : random)
			{
				suffix += L"0123456789abcdef"[value >> 4];
				suffix += L"0123456789abcdef"[value & 15];
			}
			temporary.path = destination.wstring() + suffix + L".tmp";
			file.value = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
				FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file.value != INVALID_HANDLE_VALUE)
			{
				temporary.owned = true;
				break;
			}
			const DWORD reason = GetLastError();
			if (reason != ERROR_FILE_EXISTS && reason != ERROR_ALREADY_EXISTS)
				return Fail(error, "Could not create save temporary file.");
		}
		if (!temporary.owned) return Fail(error, "Could not reserve save temporary file.");
		DWORD written = 0;
		if (!WriteFile(file.value, bytes, count, &written, nullptr) || written != count)
			return Fail(error, "Could not write complete save temporary file.");
		if (!FlushFileBuffers(file.value)) return Fail(error, "Could not flush save temporary file.");
		if (!file.Close()) return Fail(error, "Could not close save temporary file.");
		// 동일 디렉터리의 완성된 암호문/DPAPI blob만 게시한다. copy/delete fallback은 사용하지 않는다.
		for (unsigned attempt = 0; attempt < 50; ++attempt)
		{
			if (MoveFileExW(temporary.path.c_str(), destination.c_str(),
				MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0)))
			{
				temporary.owned = false;
				return true;
			}
			const DWORD reason = GetLastError();
			// 동시 rename의 Windows 공유/접근 잠금만 제한적으로 재시도한다. 영구 잠금도 결국 실패한다.
			if (!replace || (reason != ERROR_ACCESS_DENIED && reason != ERROR_SHARING_VIOLATION && reason != ERROR_LOCK_VIOLATION)
				|| attempt == 49) return Fail(error, "Could not publish save temporary file.");
			Sleep(10);
		}
		return Fail(error, "Could not publish save temporary file.");
	}

	bool EnsureKey(const std::filesystem::path& directory, FSecret& key, std::string& error)
	{
		const auto path = directory / L"SaveData.key";
		bool missing = false;
		if (ReadKey(path, key, missing, error)) return true;
		if (!missing) return false;
		const DWORD attributes = GetFileAttributesW((directory / L"InGameData.sav").c_str());
		if (attributes != INVALID_FILE_ATTRIBUTES)
		{
			// 최초 missing 관찰 뒤 다른 프로세스가 키와 세이브를 게시했을 수 있다.
			if (ReadKey(path, key, missing, error))
			{
				error.clear();
				return true;
			}
			if (!missing) return false;
			return Fail(error, "Existing save has no key; refusing to create a replacement key.");
		}
		const DWORD reason = GetLastError();
		if (reason != ERROR_FILE_NOT_FOUND && reason != ERROR_PATH_NOT_FOUND)
			return Fail(error, "Could not inspect existing save before key creation.");
		if (BCryptGenRandom(nullptr, key.bytes.data(), static_cast<ULONG>(key.bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
			return Fail(error, "Could not generate save key.");
		DATA_BLOB input{static_cast<DWORD>(key.bytes.size()), key.bytes.data()};
		FBlob protectedData;
		if (!CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &protectedData.value)
			|| protectedData.value.pbData == nullptr || protectedData.value.cbData == 0)
			return Fail(error, "Could not protect save key.");
		if (Publish(path, protectedData.value.pbData, protectedData.value.cbData, false, error))
		{
			error.clear();
			return true;
		}
		// 게시 경쟁에서 지면 새 키를 폐기하고 승자의 완성된 보호 키를 검증/사용한다.
		SecureZeroMemory(key.bytes.data(), key.bytes.size());
		if (!ReadKey(path, key, missing, error)) return false;
		error.clear();
		return true;
	}

	bool Crypt(bool encrypt, const TBytes& input, std::size_t offset, std::size_t size,
		const std::array<std::uint8_t, 32>& key, TBytes& framing, TBytes& output, std::string& error)
	{
		FCipher cipher;
		FSecret mutableKey;
		mutableKey.bytes = key;
		if (BCryptOpenAlgorithmProvider(&cipher.algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0 || cipher.algorithm == nullptr)
			return Fail(error, "Could not open the save AES provider.");
		wchar_t mode[] = BCRYPT_CHAIN_MODE_GCM;
		DWORD objectBytes = 0;
		DWORD copied = 0;
		if (BCryptSetProperty(cipher.algorithm, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(mode), sizeof(mode), 0) < 0
			|| BCryptGetProperty(cipher.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectBytes),
				sizeof(objectBytes), &copied, 0) < 0 || objectBytes == 0)
			return Fail(error, "Could not configure save AES-GCM.");
		cipher.keyObject.resize(objectBytes);
		if (BCryptGenerateSymmetricKey(cipher.algorithm, &cipher.key, cipher.keyObject.data(), objectBytes,
			mutableKey.bytes.data(), static_cast<ULONG>(mutableKey.bytes.size()), 0) < 0 || cipher.key == nullptr)
			return Fail(error, "Could not initialize the save AES key.");

		BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth;
		BCRYPT_INIT_AUTH_MODE_INFO(auth);
		auth.pbNonce = framing.data() + 16;
		auth.cbNonce = 12;
		// magic/version/length/reserved/nonce 모두 인증한다. 인증 전 진행 데이터는 게시하지 않는다.
		auth.pbAuthData = framing.data();
		auth.cbAuthData = static_cast<ULONG>(kHeaderBytes);
		auth.pbTag = framing.data() + kHeaderBytes + size;
		auth.cbTag = static_cast<ULONG>(kTagBytes);
		TBytes candidate(std::max<std::size_t>(size, 1));
		FBytesWiper wipe{candidate};
		ULONG produced = 0;
		PUCHAR source = size == 0 ? nullptr : const_cast<PUCHAR>(input.data() + offset);
		const NTSTATUS status = encrypt
			? BCryptEncrypt(cipher.key, source, static_cast<ULONG>(size), &auth, nullptr, 0,
				candidate.data(), static_cast<ULONG>(candidate.size()), &produced, 0)
			: BCryptDecrypt(cipher.key, source, static_cast<ULONG>(size), &auth, nullptr, 0,
				candidate.data(), static_cast<ULONG>(candidate.size()), &produced, 0);
		if (status < 0 || produced != size)
			return Fail(error, encrypt ? "Save encryption failed." : "Save authentication failed.");
		candidate.resize(size);
		output.swap(candidate);
		return true;
	}
}

namespace TextRPG
{
	FSaveDataStore::FSaveDataStore(std::filesystem::path directory)
		: m_directory(std::move(directory))
	{
	}

	bool FSaveDataStore::Save(const std::vector<std::uint8_t>& serialized, std::string& error) const
	{
		error.clear();
		try
		{
			if (serialized.size() > kMaxPlainBytes) return Fail(error, "Save plaintext exceeds the size limit.");
			if (!ValidateDirectory(m_directory, error)) return false;
			std::error_code reason;
			std::filesystem::create_directories(m_directory, reason);
			if (reason) return Fail(error, "Could not create save directory.");
			if (!ValidateDirectory(m_directory, error)) return false;
			FSecret key;
			TBytes encrypted;
			if (!EnsureKey(m_directory, key, error) || !EncryptSaveData(serialized, key.bytes, encrypted, error)) return false;
			return Publish(m_directory / L"InGameData.sav", encrypted.data(), static_cast<DWORD>(encrypted.size()), true, error);
		}
		catch (const std::exception&)
		{
			return Fail(error, "Could not save game data.");
		}
	}

	bool FSaveDataStore::Load(std::vector<std::uint8_t>& serialized, std::string& error) const
	{
		error.clear();
		try
		{
			if (!ValidateDirectory(m_directory, error)) return false;
			FSecret key;
			bool missing = false;
			TBytes encrypted;
			return ReadKey(m_directory / L"SaveData.key", key, missing, error)
				&& ReadBytes(m_directory / L"InGameData.sav", kMaxPlainBytes + kHeaderBytes + kTagBytes, encrypted, missing, error)
				&& DecryptSaveData(encrypted, key.bytes, serialized, error);
		}
		catch (const std::exception&)
		{
			return Fail(error, "Could not load game data.");
		}
	}

	bool ResolveSaveDataDirectory(std::filesystem::path& directory, std::string& error)
	{
		error.clear();
		try
		{
			std::wstring value;
			bool missing = false;
			if (!ReadEnvironment(L"TEXT_RPG_SAVE_ROOT", value, missing, error)) return false;
			std::filesystem::path candidate;
			if (!missing) candidate = value;
			else
			{
				if (!ReadEnvironment(L"LOCALAPPDATA", value, missing, error)) return false;
				if (missing) return Fail(error, "LOCALAPPDATA is missing.");
				candidate = std::filesystem::path(value) / L"TextRPG/Saves/Default";
			}
			if (!ValidateDirectory(candidate, error)) return false;
			directory.swap(candidate);
			return true;
		}
		catch (const std::exception&)
		{
			return Fail(error, "Could not resolve save directory.");
		}
	}

	bool EncryptSaveData(const std::vector<std::uint8_t>& plain, const std::array<std::uint8_t, 32>& key,
		std::vector<std::uint8_t>& encrypted, std::string& error)
	{
		error.clear();
		try
		{
			if (plain.size() > kMaxPlainBytes) return Fail(error, "Save plaintext exceeds the size limit.");
			TBytes framing(kHeaderBytes + plain.size() + kTagBytes);
			FBytesWiper framingWiper{framing};
			std::copy(kMagic.begin(), kMagic.end(), framing.begin());
			PutU32(framing, 4, 1);
			PutU32(framing, 8, static_cast<std::uint32_t>(plain.size()));
			if (BCryptGenRandom(nullptr, framing.data() + 16, 12, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
				return Fail(error, "Could not generate a save nonce.");
			TBytes cipher;
			if (!Crypt(true, plain, 0, plain.size(), key, framing, cipher, error)) return false;
			std::copy(cipher.begin(), cipher.end(), framing.begin() + kHeaderBytes);
			encrypted.swap(framing);
			return true;
		}
		catch (const std::exception&)
		{
			return Fail(error, "Could not encrypt save data.");
		}
	}

	bool DecryptSaveData(const std::vector<std::uint8_t>& encrypted, const std::array<std::uint8_t, 32>& key,
		std::vector<std::uint8_t>& plain, std::string& error)
	{
		error.clear();
		try
		{
			if (encrypted.size() < kHeaderBytes + kTagBytes || encrypted.size() > kMaxPlainBytes + kHeaderBytes + kTagBytes
				|| !std::equal(kMagic.begin(), kMagic.end(), encrypted.begin()) || GetU32(encrypted, 4) != 1
				|| GetU32(encrypted, 12) != 0 || GetU32(encrypted, 8) != encrypted.size() - kHeaderBytes - kTagBytes)
				return Fail(error, "Invalid save framing, version or length.");
			TBytes framing(encrypted);
			return Crypt(false, encrypted, kHeaderBytes, encrypted.size() - kHeaderBytes - kTagBytes, key, framing, plain, error);
		}
		catch (const std::exception&)
		{
			return Fail(error, "Could not decrypt save data.");
		}
	}
}
