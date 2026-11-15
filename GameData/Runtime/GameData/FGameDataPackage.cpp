#include "GameData/pch.h"

#include "FGameDataPackage.h"

namespace
{
	using TBytes = std::vector<std::uint8_t>;
	constexpr std::array<std::uint8_t, 4> kMagic = { 'T', 'G', 'D', 'P' };
	constexpr std::uint32_t kVersion = 2;
	constexpr std::size_t kNonceSize = 12;
	constexpr std::size_t kTagSize = 16;
	constexpr std::size_t kMaxEntryCount = 10000;
	constexpr std::uint64_t kMaxEntrySize = 64ULL * 1024ULL * 1024ULL;
	constexpr std::uint64_t kMaxPackageSize = 512ULL * 1024ULL * 1024ULL;

	void WipeEntries(std::vector<GameData::SPackageEntry>& entries) noexcept
	{
		for (GameData::SPackageEntry& entry : entries)
		{
			if (!entry.contents.empty())
			{
				SecureZeroMemory(entry.contents.data(), entry.contents.size());
			}
		}
	}

	void AppendU32(TBytes& bytes, std::uint32_t value)
	{
		for (unsigned shift = 0; shift < 32; shift += 8)
		{
			bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
		}
	}

	void AppendU64(TBytes& bytes, std::uint64_t value)
	{
		for (unsigned shift = 0; shift < 64; shift += 8)
		{
			bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFF));
		}
	}

	bool ReadU32(const TBytes& bytes, std::size_t& offset, std::uint32_t& value)
	{
		if (offset > bytes.size() || bytes.size() - offset < 4)
		{
			return false;
		}
		value = 0;
		for (unsigned index = 0; index < 4; ++index)
		{
			value |= static_cast<std::uint32_t>(bytes[offset++]) << (index * 8);
		}
		return true;
	}

	bool ReadU64(const TBytes& bytes, std::size_t& offset, std::uint64_t& value)
	{
		if (offset > bytes.size() || bytes.size() - offset < 8)
		{
			return false;
		}
		value = 0;
		for (unsigned index = 0; index < 8; ++index)
		{
			value |= static_cast<std::uint64_t>(bytes[offset++]) << (index * 8);
		}
		return true;
	}

	bool IsValidRelativeName(const std::string& name)
	{
		if (name.empty() || name.front() == '/' || name.find('\\') != std::string::npos
			|| name.find(':') != std::string::npos || name.find('\0') != std::string::npos)
		{
			return false;
		}

		int wideLength = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(),
			static_cast<int>(name.size()), nullptr, 0);
		if (wideLength <= 0)
		{
			return false;
		}

		std::size_t segmentStart = 0;
		while (segmentStart <= name.size())
		{
			const std::size_t separator = name.find('/', segmentStart);
			const std::string segment = name.substr(segmentStart,
				separator == std::string::npos ? std::string::npos : separator - segmentStart);
			if (segment.empty() || segment == "." || segment == "..")
			{
				return false;
			}
			if (separator == std::string::npos)
			{
				break;
			}
			segmentStart = separator + 1;
		}
		return true;
	}

	std::wstring WideName(const std::string& name)
	{
		const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), nullptr, 0);
		if (count <= 0) return {};
		std::wstring wide(static_cast<std::size_t>(count), L' ');
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), wide.data(), count) != count)
			return {};
		return wide;
	}

	struct SOrdinalLess
	{
		bool operator()(const std::wstring& left, const std::wstring& right) const noexcept
		{
			return CompareStringOrdinal(left.data(), static_cast<int>(left.size()), right.data(),
				static_cast<int>(right.size()), TRUE) == CSTR_LESS_THAN;
		}
	};

	bool CheckNoReparse(const std::filesystem::path& path, std::string& error)
	{
		// 명시 루트 자체뿐 아니라 모든 조상도 검사하여 junction을 통한 경계 이탈을 막는다.
		std::error_code code;
		const auto absolute = std::filesystem::absolute(path, code);
		if (code) { error = "Could not resolve input/output path."; return false; }
		auto current = absolute.root_path();
		for (const auto& part : absolute.relative_path())
		{
			current /= part;
			const DWORD attributes = GetFileAttributesW(current.c_str());
			if (attributes == INVALID_FILE_ATTRIBUTES)
			{
				const DWORD failure = GetLastError();
				if (failure == ERROR_FILE_NOT_FOUND || failure == ERROR_PATH_NOT_FOUND) continue;
				error = "Could not inspect input/output path.";
				return false;
			}
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			{
				error = "Input/output reparse points are not allowed.";
				return false;
			}
		}
		return true;
	}

	bool IsWithin(const std::filesystem::path& path, const std::filesystem::path& root)
	{
		const auto relative = path.lexically_relative(root);
		return !relative.empty() && *relative.begin() != L"..";
	}

	bool BCryptOk(NTSTATUS status)
	{
		return BCRYPT_SUCCESS(status);
	}

	bool Crypt(
		bool encrypt,
		const TBytes& input,
		const std::array<std::uint8_t, 32>& key,
		const std::array<std::uint8_t, kNonceSize>& nonce,
		const TBytes& authenticatedData,
		TBytes& output,
		std::array<std::uint8_t, kTagSize>& tag,
		std::string& error)
	{
		BCRYPT_ALG_HANDLE algorithm = nullptr;
		BCRYPT_KEY_HANDLE symmetricKey = nullptr;
		TBytes keyObject;
		NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0);
		if (!BCryptOk(status))
		{
			error = "Could not open the Windows AES provider.";
			return false;
		}

		wchar_t chainingMode[] = BCRYPT_CHAIN_MODE_GCM;
		status = BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
			reinterpret_cast<PUCHAR>(chainingMode), sizeof(chainingMode), 0);
		DWORD objectSize = 0;
		DWORD resultSize = 0;
		if (BCryptOk(status))
		{
			status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
				reinterpret_cast<PUCHAR>(&objectSize), sizeof(objectSize), &resultSize, 0);
		}
		if (BCryptOk(status))
		{
			keyObject.resize(objectSize);
			status = BCryptGenerateSymmetricKey(algorithm, &symmetricKey, keyObject.data(),
				objectSize, const_cast<PUCHAR>(key.data()), static_cast<ULONG>(key.size()), 0);
		}
		if (!BCryptOk(status) || symmetricKey == nullptr)
		{
			if (!keyObject.empty()) SecureZeroMemory(keyObject.data(), keyObject.size());
			if (algorithm != nullptr) BCryptCloseAlgorithmProvider(algorithm, 0);
			error = "Could not initialize AES-GCM.";
			return false;
		}

		output.resize(input.size());
		BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
		BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
		authInfo.pbNonce = const_cast<PUCHAR>(nonce.data());
		authInfo.cbNonce = static_cast<ULONG>(nonce.size());
		authInfo.pbAuthData = authenticatedData.empty()
			? nullptr
			: const_cast<PUCHAR>(authenticatedData.data());
		authInfo.cbAuthData = static_cast<ULONG>(authenticatedData.size());
		authInfo.pbTag = tag.data();
		authInfo.cbTag = static_cast<ULONG>(tag.size());
		ULONG outputSize = 0;
		status = encrypt
			? BCryptEncrypt(symmetricKey, const_cast<PUCHAR>(input.data()), static_cast<ULONG>(input.size()),
				&authInfo, nullptr, 0, output.data(), static_cast<ULONG>(output.size()), &outputSize, 0)
			: BCryptDecrypt(symmetricKey, const_cast<PUCHAR>(input.data()), static_cast<ULONG>(input.size()),
				&authInfo, nullptr, 0, output.data(), static_cast<ULONG>(output.size()), &outputSize, 0);
		BCryptDestroyKey(symmetricKey);
		BCryptCloseAlgorithmProvider(algorithm, 0);
		if (!keyObject.empty()) SecureZeroMemory(keyObject.data(), keyObject.size());
		if (!BCryptOk(status))
		{
			SecureZeroMemory(output.data(), output.size());
			output.clear();
			error = encrypt ? "AES-GCM encryption failed." : "Package authentication failed or the key is incorrect.";
			return false;
		}
		output.resize(outputSize);
		return true;
	}

	TBytes BuildAssociatedData(std::uint64_t cipherSize)
	{
		TBytes data(kMagic.begin(), kMagic.end());
		AppendU32(data, kVersion);
		AppendU64(data, cipherSize);
		return data;
	}

	bool ReadFile(const std::filesystem::path& path, TBytes& bytes, std::uint64_t maxSize, std::string& error)
	{
		std::error_code fileError;
		const std::uint64_t size = std::filesystem::file_size(path, fileError);
		if (fileError || size > maxSize || size > std::numeric_limits<std::size_t>::max())
		{
			error = "File is inaccessible or exceeds the allowed size.";
			return false;
		}
		std::ifstream stream(path, std::ios::binary);
		if (!stream)
		{
			error = "Could not open file: " + path.string();
			return false;
		}
		bytes.resize(static_cast<std::size_t>(size));
		if (!bytes.empty())
		{
			stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		}
		if (!stream || static_cast<std::size_t>(stream.gcount()) != bytes.size())
		{
			error = "Could not read complete file: " + path.string();
			return false;
		}
		return true;
	}

	struct STemporaryPackageFile final
	{
		std::filesystem::path path;
		HANDLE handle = INVALID_HANDLE_VALUE;
		bool ownsPath = false;

		STemporaryPackageFile() = default;
		STemporaryPackageFile(const STemporaryPackageFile&) = delete;
		STemporaryPackageFile& operator=(const STemporaryPackageFile&) = delete;

		~STemporaryPackageFile()
		{
			if (handle != INVALID_HANDLE_VALUE)
			{
				CloseHandle(handle);
			}
			if (ownsPath)
			{
				DeleteFileW(path.c_str());
			}
		}
	};

	bool WriteAtomic(const std::filesystem::path& path, const TBytes& bytes, std::string& error)
	{
		std::error_code filesystemError;
		if (!path.parent_path().empty())
		{
			std::filesystem::create_directories(path.parent_path(), filesystemError);
			if (filesystemError)
			{
				error = "Could not create output directory.";
				return false;
			}
		}
		STemporaryPackageFile temporary;
		constexpr wchar_t kHexDigits[] = L"0123456789abcdef";
		for (unsigned attempt = 0; attempt < 32; ++attempt)
		{
			std::array<std::uint8_t, 16> randomBytes{};
			if (!BCryptOk(BCryptGenRandom(nullptr, randomBytes.data(), static_cast<ULONG>(randomBytes.size()),
				BCRYPT_USE_SYSTEM_PREFERRED_RNG)))
			{
				error = "Could not generate a temporary package filename.";
				return false;
			}
			std::wstring suffix = L".";
			for (std::uint8_t value : randomBytes)
			{
				suffix += kHexDigits[value >> 4];
				suffix += kHexDigits[value & 0x0F];
			}
			suffix += L".tmp";
			temporary.path = path;
			temporary.path += suffix;
			temporary.handle = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr,
				CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (temporary.handle != INVALID_HANDLE_VALUE)
			{
				temporary.ownsPath = true;
				break;
			}
			const DWORD createError = GetLastError();
			if (createError != ERROR_FILE_EXISTS && createError != ERROR_ALREADY_EXISTS)
			{
				error = "Could not create temporary package file.";
				return false;
			}
		}
		if (!temporary.ownsPath)
		{
			error = "Could not reserve a unique temporary package file.";
			return false;
		}
		std::size_t offset = 0;
		while (offset < bytes.size())
		{
			const DWORD chunkSize = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset,
				std::numeric_limits<DWORD>::max()));
			DWORD written = 0;
			if (!WriteFile(temporary.handle, bytes.data() + offset, chunkSize, &written, nullptr) || written != chunkSize)
			{
				error = "Could not write complete package file.";
				return false;
			}
			offset += written;
		}
		if (!FlushFileBuffers(temporary.handle))
		{
			error = "Could not flush complete package file.";
			return false;
		}
		if (!CloseHandle(temporary.handle))
		{
			error = "Could not close complete package file.";
			return false;
		}
		temporary.handle = INVALID_HANDLE_VALUE;
		// 동시 실행은 각자 임시 파일을 소유하며 마지막으로 성공한 게시가 최종 결과가 된다.
		if (!MoveFileExW(temporary.path.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			error = "Could not atomically publish the package.";
			return false;
		}
		temporary.ownsPath = false;
		return true;
	}
}

namespace GameData
{
	bool BuildEncryptedPackage(
		const std::filesystem::path& yamlRoot,
		const std::filesystem::path& outputPath,
		const std::array<std::uint8_t, 32>& key,
		std::string& error,
		const std::filesystem::path& assetRoot)
	{
		std::error_code filesystemError;
		if (!CheckNoReparse(yamlRoot, error) || !CheckNoReparse(outputPath, error) ||
			(!assetRoot.empty() && !CheckNoReparse(assetRoot, error))) return false;
		const auto output = std::filesystem::weakly_canonical(outputPath, filesystemError);
		if (filesystemError) { error = "Could not resolve package output path."; return false; }
		std::vector<std::pair<std::string, std::filesystem::path>> files;
		std::set<std::wstring, SOrdinalLess> names;
		const auto collect = [&](const std::filesystem::path& root, const char* prefix, bool yamlOnly)
		{
			if (!std::filesystem::is_directory(root, filesystemError) || filesystemError)
			{
				error = "Input directory does not exist.";
				return false;
			}
			const auto resolvedRoot = std::filesystem::canonical(root, filesystemError);
			if (filesystemError) { error = "Could not resolve input directory."; return false; }
			if (IsWithin(output, resolvedRoot))
			{
				error = "Package output must not be within an input directory.";
				return false;
			}
			for (std::filesystem::recursive_directory_iterator iterator(resolvedRoot), end; iterator != end; ++iterator)
			{
				const auto status = iterator->symlink_status(filesystemError);
				if (filesystemError || !CheckNoReparse(iterator->path(), error))
				{
					if (filesystemError) error = "Could not inspect input directory.";
					return false;
				}
				if (!std::filesystem::is_regular_file(status) || (yamlOnly && iterator->path().extension() != L".yaml")) continue;
				const auto relative = iterator->path().lexically_relative(resolvedRoot).generic_u8string();
				std::string name = prefix;
				name.append(reinterpret_cast<const char*>(relative.data()), relative.size());
				if (name.size() > 4096 || !IsValidRelativeName(name) || !names.insert(WideName(name)).second)
				{
					error = "Input has an invalid or case-ambiguous relative name.";
					return false;
				}
				files.emplace_back(std::move(name), iterator->path());
				if (files.size() > kMaxEntryCount) { error = "Too many package files."; return false; }
			}
			return true;
		};
		if (!collect(yamlRoot, "Data/Client/", true)) return false;
		if (files.empty()) { error = "No .yaml files were found."; return false; }
		if (!assetRoot.empty() && !collect(assetRoot, "Asset/", false)) return false;
		std::sort(files.begin(), files.end(), [](const auto& left, const auto& right) { return left.first < right.first; });
		const bool outputExists = std::filesystem::exists(outputPath, filesystemError);
		if (filesystemError) { error = "Could not inspect package output path."; return false; }
		if (outputExists)
		{
			for (const auto& file : files)
			{
				const bool isInput = std::filesystem::equivalent(file.second, outputPath, filesystemError);
				if (filesystemError || isInput)
				{
					error = "Package output must not alias an input file; input identity could not be excluded.";
					return false;
				}
			}
		}

		TBytes plain;
		AppendU32(plain, static_cast<std::uint32_t>(files.size()));
		for (const auto& [name, path] : files)
		{
			if (name.size() > std::numeric_limits<std::uint32_t>::max())
			{
				error = "YAML filename is too long.";
				return false;
			}
			TBytes contents;
			if (!ReadFile(path, contents, kMaxEntrySize, error))
			{
				return false;
			}
			if (plain.size() > kMaxPackageSize - 12 - name.size() ||
				contents.size() > kMaxPackageSize - plain.size() - 12 - name.size())
			{
				error = "Combined data exceeds the package size limit.";
				return false;
			}
			AppendU32(plain, static_cast<std::uint32_t>(name.size()));
			plain.insert(plain.end(), name.begin(), name.end());
			AppendU64(plain, contents.size());
			plain.insert(plain.end(), contents.begin(), contents.end());
			if (plain.size() > kMaxPackageSize)
			{
				error = "Combined YAML data exceeds the package size limit.";
				return false;
			}
		}

		std::array<std::uint8_t, kNonceSize> nonce{};
		NTSTATUS status = BCryptGenRandom(nullptr, nonce.data(), static_cast<ULONG>(nonce.size()),
			BCRYPT_USE_SYSTEM_PREFERRED_RNG);
		if (!BCryptOk(status))
		{
			error = "Could not generate a package nonce.";
			return false;
		}
		TBytes authenticatedData = BuildAssociatedData(plain.size());
		TBytes cipher;
		std::array<std::uint8_t, kTagSize> tag{};
		if (!Crypt(true, plain, key, nonce, authenticatedData, cipher, tag, error))
		{
			SecureZeroMemory(plain.data(), plain.size());
			return false;
		}
		SecureZeroMemory(plain.data(), plain.size());

		TBytes package(kMagic.begin(), kMagic.end());
		AppendU32(package, kVersion);
		package.insert(package.end(), nonce.begin(), nonce.end());
		AppendU64(package, cipher.size());
		package.insert(package.end(), cipher.begin(), cipher.end());
		package.insert(package.end(), tag.begin(), tag.end());
		if (!WriteAtomic(outputPath, package, error))
		{
			SecureZeroMemory(cipher.data(), cipher.size());
			return false;
		}
		SecureZeroMemory(cipher.data(), cipher.size());
		return true;
	}

	bool FGameDataPackageReader::Load(
		const std::filesystem::path& packagePath,
		const std::array<std::uint8_t, 32>& key,
		std::string& error)
	{
		Unload();
		TBytes package;
		if (!ReadFile(packagePath, package, kMaxPackageSize + 64, error))
		{
			return false;
		}
		constexpr std::size_t headerSize = 4 + 4 + kNonceSize + 8;
		if (package.size() < headerSize + kTagSize
			|| !std::equal(kMagic.begin(), kMagic.end(), package.begin()))
		{
			error = "Package header is invalid.";
			return false;
		}
		std::size_t offset = 4;
		std::uint32_t version = 0;
		if (!ReadU32(package, offset, version) || version != kVersion)
		{
			error = "Package version is not supported.";
			return false;
		}
		std::array<std::uint8_t, kNonceSize> nonce{};
		std::copy_n(package.begin() + offset, nonce.size(), nonce.begin());
		offset += nonce.size();
		std::uint64_t cipherSize = 0;
		if (!ReadU64(package, offset, cipherSize)
			|| cipherSize > kMaxPackageSize
			|| cipherSize != package.size() - offset - kTagSize)
		{
			error = "Package length is invalid.";
			return false;
		}
		TBytes cipher(package.begin() + offset, package.end() - kTagSize);
		std::array<std::uint8_t, kTagSize> tag{};
		std::copy_n(package.end() - kTagSize, tag.size(), tag.begin());
		TBytes plain;
		TBytes authenticatedData = BuildAssociatedData(cipherSize);
		if (!Crypt(false, cipher, key, nonce, authenticatedData, plain, tag, error))
		{
			return false;
		}
		SecureZeroMemory(cipher.data(), cipher.size());

		std::vector<SPackageEntry> entries;
		std::set<std::wstring, SOrdinalLess> names;
		std::size_t plainOffset = 0;
		std::uint32_t entryCount = 0;
		if (!ReadU32(plain, plainOffset, entryCount) || entryCount == 0 || entryCount > kMaxEntryCount)
		{
			SecureZeroMemory(plain.data(), plain.size());
			error = "Package entry count is invalid.";
			return false;
		}
		entries.reserve(entryCount);
		for (std::uint32_t index = 0; index < entryCount; ++index)
		{
			std::uint32_t nameSize = 0;
			if (!ReadU32(plain, plainOffset, nameSize) || nameSize == 0 || nameSize > 4096
				|| plainOffset > plain.size() || plain.size() - plainOffset < nameSize)
			{
				WipeEntries(entries);
				SecureZeroMemory(plain.data(), plain.size());
				error = "Package filename length is invalid.";
				return false;
			}
			std::string name(reinterpret_cast<const char*>(plain.data() + plainOffset), nameSize);
			plainOffset += nameSize;
			std::uint64_t contentSize = 0;
			if (!IsValidRelativeName(name) || !names.insert(WideName(name)).second
				|| !ReadU64(plain, plainOffset, contentSize) || contentSize > kMaxEntrySize
				|| plainOffset > plain.size() || contentSize > plain.size() - plainOffset)
			{
				WipeEntries(entries);
				SecureZeroMemory(plain.data(), plain.size());
				error = "Package entry metadata is invalid or duplicated.";
				return false;
			}
			SPackageEntry entry;
			entry.name = std::move(name);
			const std::size_t localContentSize = static_cast<std::size_t>(contentSize);
			entry.contents.assign(plain.begin() + plainOffset, plain.begin() + plainOffset + localContentSize);
			plainOffset += localContentSize;
			entries.push_back(std::move(entry));
		}
		SecureZeroMemory(plain.data(), plain.size());
		if (plainOffset != cipherSize)
		{
			WipeEntries(entries);
			error = "Package contains trailing or unparsed data.";
			return false;
		}
		m_entries = std::move(entries);
		return true;
	}

	const SPackageEntry* FGameDataPackageReader::FindEntry(const std::string& relativeName) const noexcept
	{
		auto iterator = std::find_if(m_entries.begin(), m_entries.end(), [&relativeName](const SPackageEntry& entry)
		{
			return entry.name == relativeName;
		});
		return iterator == m_entries.end() ? nullptr : &*iterator;
	}

	const std::vector<SPackageEntry>& FGameDataPackageReader::GetEntries() const noexcept
	{
		return m_entries;
	}

	void FGameDataPackageReader::Unload() noexcept
	{
		for (SPackageEntry& entry : m_entries)
		{
			if (!entry.contents.empty())
			{
				SecureZeroMemory(entry.contents.data(), entry.contents.size());
			}
		}
		m_entries.clear();
	}
}
