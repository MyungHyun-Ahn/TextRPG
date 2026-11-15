#include "GameData/pch.h"

#include "InGameData/FSaveDataStore.h"

// 실제 원본/실제 암호화를 유지하고 Win32 실패 반환만 결정적으로 주입한다.
namespace SaveTest
{
	extern const std::vector<std::uint8_t> kOriginal;
	bool publishLateWinner = false;
	bool lateWinnerPublished = false;
	DWORD publicationFailure = ERROR_SUCCESS;
	bool transientPublication = false;
	unsigned transientHits = 0;

	BOOL WINAPI Move(LPCWSTR from, LPCWSTR to, DWORD flags)
	{
		if (transientPublication && (flags & MOVEFILE_REPLACE_EXISTING) != 0)
		{
			transientPublication = false;
			++transientHits;
			publicationFailure = ERROR_ACCESS_DENIED;
			SetLastError(ERROR_ACCESS_DENIED);
			return FALSE;
		}
		const BOOL moved = ::MoveFileExW(from, to, flags);
		if (!moved)
		{
			publicationFailure = GetLastError();
			SetLastError(publicationFailure);
		}
		return moved;
	}

	HANDLE WINAPI Create(LPCWSTR path, DWORD access, DWORD sharing, LPSECURITY_ATTRIBUTES attributes,
		DWORD disposition, DWORD flags, HANDLE templateFile)
	{
		HANDLE file = ::CreateFileW(path, access, sharing, attributes, disposition, flags, templateFile);
		const DWORD reason = GetLastError();
		if (publishLateWinner && file == INVALID_HANDLE_VALUE && disposition == OPEN_EXISTING
			&& reason == ERROR_FILE_NOT_FOUND && std::filesystem::path(path).filename() == L"SaveData.key")
		{
			publishLateWinner = false;
			std::string error;
			lateWinnerPublished = TextRPG::FSaveDataStore(std::filesystem::path(path).parent_path()).Save(kOriginal, error);
			SetLastError(reason);
		}
		return file;
	}

	enum class EFault { None, Write, Flush, Close, Read };
	EFault fault = EFault::None;
	struct SInjection final
	{
		unsigned hits = 0;
		HANDLE reachedHandle = INVALID_HANDLE_VALUE;
		EFault completedFault = EFault::None;
		DWORD windowsReason = ERROR_SUCCESS;
		bool partialSucceeded = false;
		DWORD writeCount = 0;
		DWORD partialWritten = 0;
	};
	SInjection injection;
	HANDLE writeHandle = INVALID_HANDLE_VALUE;
	std::vector<std::uint8_t> lastWrite;

	void CompleteInjection(EFault mode, HANDLE file, DWORD reason)
	{
		SetLastError(reason);
		injection.windowsReason = GetLastError();
		injection.completedFault = mode;
		injection.reachedHandle = file;
		++injection.hits;
		fault = EFault::None;
	}

	BOOL WINAPI Write(HANDLE file, LPCVOID bytes, DWORD count, LPDWORD written, LPOVERLAPPED overlapped)
	{
		const auto* begin = static_cast<const std::uint8_t*>(bytes);
		lastWrite.assign(begin, begin + count);
		writeHandle = file;
		if (fault == EFault::Write)
		{
			injection.writeCount = count;
			const BOOL partial = ::WriteFile(file, bytes, count / 2, written, overlapped);
			injection.partialSucceeded = partial != FALSE;
			injection.partialWritten = written != nullptr ? *written : 0;
			// 실제 양수 부분 쓰기가 끝난 뒤에만 의도한 실패의 완료를 기록한다.
			if (!partial || written == nullptr || count / 2 == 0 || *written != count / 2) return FALSE;
			CompleteInjection(EFault::Write, file, ERROR_DISK_FULL);
			return FALSE;
		}
		return ::WriteFile(file, bytes, count, written, overlapped);
	}

	BOOL WINAPI Flush(HANDLE file)
	{
		if (fault == EFault::Flush)
		{
			CompleteInjection(EFault::Flush, file, ERROR_WRITE_FAULT);
			return FALSE;
		}
		return ::FlushFileBuffers(file);
	}

	BOOL WINAPI Close(HANDLE file)
	{
		if (fault == EFault::Close && file == writeHandle)
		{
			CompleteInjection(EFault::Close, file, ERROR_WRITE_FAULT);
			return FALSE;
		}
		return ::CloseHandle(file);
	}

	BOOL WINAPI Read(HANDLE file, LPVOID bytes, DWORD count, LPDWORD copied, LPOVERLAPPED overlapped)
	{
		if (fault == EFault::Read)
		{
			CompleteInjection(EFault::Read, file, ERROR_READ_FAULT);
			return FALSE;
		}
		return ::ReadFile(file, bytes, count, copied, overlapped);
	}
}
#define MoveFileExW SaveTest::Move
#define CreateFileW SaveTest::Create
#define WriteFile SaveTest::Write
#define FlushFileBuffers SaveTest::Flush
#define CloseHandle SaveTest::Close
#define ReadFile SaveTest::Read
#include "../../TextRPG/InGameData/FSaveDataStore.cpp"
#undef MoveFileExW
#undef CreateFileW
#undef WriteFile
#undef FlushFileBuffers
#undef CloseHandle
#undef ReadFile

namespace SaveTest
{
	using TData = std::vector<std::uint8_t>;
	const TData kOriginal{0, 1, 2, 3, 255, 67, 21};
	const TData kSentinel{91, 92, 93};

	void Require(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	void Pass(const char* name) { std::cout << "PASS " << name << '\n'; }

	const char* ExpectedDiagnostic(EFault mode)
	{
		switch (mode)
		{
		case EFault::Write: return "Could not write complete save temporary file.";
		case EFault::Flush: return "Could not flush save temporary file.";
		case EFault::Close: return "Could not close save temporary file.";
		case EFault::Read: return "Could not read/close the complete save file or key.";
		default: return "";
		}
	}

	DWORD ExpectedWindowsReason(EFault mode)
	{
		switch (mode)
		{
		case EFault::Write: return ERROR_DISK_FULL;
		case EFault::Flush:
		case EFault::Close: return ERROR_WRITE_FAULT;
		case EFault::Read: return ERROR_READ_FAULT;
		default: return ERROR_SUCCESS;
		}
	}

	bool FaultAccepted(EFault expected, bool succeeded, const std::string& error, const SInjection& observed, EFault pending)
	{
		return expected != EFault::None && !succeeded && error == ExpectedDiagnostic(expected)
			&& observed.hits == 1 && observed.completedFault == expected && pending == EFault::None
			&& observed.reachedHandle != INVALID_HANDLE_VALUE && observed.reachedHandle != nullptr
			&& observed.windowsReason == ExpectedWindowsReason(expected)
			&& (expected != EFault::Write || (observed.partialSucceeded && observed.writeCount / 2 > 0
				&& observed.partialWritten == observed.writeCount / 2));
	}

	void FaultOracleControls(EFault mode, const std::string& error)
	{
		Require(FaultAccepted(mode, false, error, injection, fault), "fault oracle positive control rejected real injection");
		Require(!FaultAccepted(mode, false, "Could not save game data.", injection, fault),
			"fault oracle accepted wrong nonempty diagnostic");
		Require(!FaultAccepted(mode, false, "", injection, fault), "fault oracle accepted missing diagnostic");
		Require(!FaultAccepted(mode, true, error, injection, fault), "fault oracle accepted successful operation");
		auto bad = injection;
		bad.hits = 0;
		Require(!FaultAccepted(mode, false, error, bad, fault), "fault oracle accepted unreached injection");
		bad = injection;
		bad.completedFault = EFault::None;
		Require(!FaultAccepted(mode, false, error, bad, fault), "fault oracle accepted incomplete injection");
		bad = injection;
		bad.windowsReason = ERROR_ACCESS_DENIED;
		Require(!FaultAccepted(mode, false, error, bad, fault), "fault oracle accepted wrong Windows reason");
		Require(!FaultAccepted(mode, false, error, injection, mode), "fault oracle accepted still-armed injection");
		bad = injection;
		bad.reachedHandle = INVALID_HANDLE_VALUE;
		Require(!FaultAccepted(mode, false, error, bad, fault), "fault oracle accepted missing real handle");
		if (mode == EFault::Write)
		{
			bad = injection;
			bad.partialSucceeded = false;
			Require(!FaultAccepted(mode, false, error, bad, fault), "fault oracle accepted failed partial prerequisite");
			bad = injection;
			bad.partialWritten = 0;
			Require(!FaultAccepted(mode, false, error, bad, fault), "fault oracle accepted incomplete partial prerequisite");
			bad = injection;
			--bad.partialWritten;
			Require(!FaultAccepted(mode, false, error, bad, fault), "fault oracle accepted wrong positive partial count");
		}
	}

	void FailedPartialPrerequisite(const std::filesystem::path& save)
	{
		// 실제 암호문과 읽기 전용 핸들로 부분 쓰기 자체의 실패를 재현한다.
		const auto ciphertext = lastWrite;
		HANDLE readOnly = ::CreateFileW(save.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		Require(readOnly != INVALID_HANDLE_VALUE, "partial prerequisite read-only fixture failed");
		fault = EFault::Write;
		injection = {};
		DWORD written = 0;
		const BOOL result = Write(readOnly, ciphertext.data(), static_cast<DWORD>(ciphertext.size()), &written, nullptr);
		const DWORD reason = GetLastError();
		const auto observed = injection;
		const EFault pending = fault;
		fault = EFault::None;
		const bool closed = ::CloseHandle(readOnly) != FALSE;
		Require(!result && reason == ERROR_ACCESS_DENIED && closed && !observed.partialSucceeded
			&& observed.writeCount == ciphertext.size() && observed.partialWritten == 0 && observed.hits == 0
			&& observed.completedFault == EFault::None && observed.windowsReason == ERROR_SUCCESS
			&& observed.reachedHandle == INVALID_HANDLE_VALUE && pending == EFault::Write,
			"failed real partial prerequisite counted as completed injection");
		Require(!FaultAccepted(EFault::Write, false, ExpectedDiagnostic(EFault::Write), observed, pending),
			"fault oracle accepted failed real partial prerequisite");
	}

	TData ReadDisk(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary);
		Require(file.good(), "read fixture file failed");
		return TData(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	void WriteDisk(const std::filesystem::path& path, const TData& data)
	{
		std::filesystem::create_directories(path.parent_path());
		std::ofstream file(path, std::ios::binary | std::ios::trunc);
		file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
		file.close();
		Require(!file.fail(), "write fixture file failed");
	}

	struct FTestBlob final
	{
		DATA_BLOB value{};
		~FTestBlob()
		{
			if (value.pbData != nullptr)
			{
				SecureZeroMemory(value.pbData, value.cbData);
				LocalFree(value.pbData);
			}
		}
	};

	struct FTestSecret final
	{
		std::array<std::uint8_t, 32> bytes{};
		~FTestSecret() { SecureZeroMemory(bytes.data(), bytes.size()); }
	};

	void ReadSaveSecret(const std::filesystem::path& directory, FTestSecret& secret)
	{
		auto blob = ReadDisk(directory / L"SaveData.key");
		DATA_BLOB input{static_cast<DWORD>(blob.size()), blob.data()};
		FTestBlob output;
		Require(CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output.value)
			&& output.value.pbData != nullptr && output.value.cbData == secret.bytes.size(), "real DPAPI unprotect failed");
		std::copy_n(output.value.pbData, secret.bytes.size(), secret.bytes.begin());
	}

	void Crypto()
	{
		FTestSecret key;
		key.bytes.fill(72);
		TData cipher;
		TData restored;
		std::string error;
		Require(TextRPG::EncryptSaveData(kOriginal, key.bytes, cipher, error), "encrypt roundtrip failed");
		Require(TextRPG::DecryptSaveData(cipher, key.bytes, restored, error) && restored == kOriginal, "decrypt roundtrip failed");
		Pass("save-crypto-roundtrip");
		TData second;
		Require(TextRPG::EncryptSaveData(kOriginal, key.bytes, second, error) && second != cipher, "fresh nonce missing");
		Pass("save-fresh-nonce");
		FTestSecret wrong;
		wrong.bytes.fill(17);
		restored = kSentinel;
		Require(!TextRPG::DecryptSaveData(cipher, wrong.bytes, restored, error) && restored == kSentinel && !error.empty(),
			"wrong key accepted or output changed");
		Pass("save-wrong-key-output-preserved");
		for (std::size_t index = 0; index < cipher.size(); ++index)
		{
			TData bad(cipher);
			bad[index] ^= 1;
			restored = kSentinel;
			Require(!TextRPG::DecryptSaveData(bad, key.bytes, restored, error) && restored == kSentinel && !error.empty(),
				"altered metadata/nonce/cipher/tag accepted or output changed");
		}
		Pass("save-every-byte-tamper-rejected");
		for (std::size_t size = 0; size < cipher.size(); ++size)
		{
			TData shortData(cipher.begin(), cipher.begin() + size);
			Require(!TextRPG::DecryptSaveData(shortData, key.bytes, restored, error) && restored == kSentinel,
				"truncated save accepted or output changed");
		}
		TData trailing(cipher);
		trailing.push_back(0);
		Require(!TextRPG::DecryptSaveData(trailing, key.bytes, restored, error) && restored == kSentinel, "trailing byte accepted");
		Pass("save-all-truncations-and-trailing-rejected");
		TData oversized(16U * 1024U * 1024U + 1, 7);
		TData preserved = cipher;
		Require(!TextRPG::EncryptSaveData(oversized, key.bytes, preserved, error) && preserved == cipher, "oversize encrypt accepted");
		oversized.resize(16U * 1024U * 1024U + 45);
		Require(!TextRPG::DecryptSaveData(oversized, key.bytes, restored, error) && restored == kSentinel, "oversize decrypt accepted");
		Pass("save-oversize-output-preserved");
		oversized.resize(16U * 1024U * 1024U);
		Require(TextRPG::EncryptSaveData(oversized, key.bytes, cipher, error)
			&& TextRPG::DecryptSaveData(cipher, key.bytes, restored, error) && restored == oversized, "maximum boundary failed");
		Require(TextRPG::EncryptSaveData({}, key.bytes, cipher, error)
			&& TextRPG::DecryptSaveData(cipher, key.bytes, restored, error) && restored.empty(), "empty roundtrip failed");
		TData aliased = kOriginal;
		Require(TextRPG::EncryptSaveData(aliased, key.bytes, aliased, error)
			&& TextRPG::DecryptSaveData(aliased, key.bytes, aliased, error) && aliased == kOriginal, "in-place alias failed");
		Pass("save-empty-and-maximum-boundaries");
	}

	void NoTemporary(const std::filesystem::path& directory)
	{
		Require(ReadDisk(directory / L"unowned.tmp") == kSentinel, "unowned temporary file changed");
		for (const auto& entry : std::filesystem::directory_iterator(directory))
		{
			Require(entry.path().extension() != L".tmp" || entry.path().filename() == L"unowned.tmp", "owned temporary file leaked");
		}
	}

	void Service(const std::filesystem::path& directory)
	{
		std::string error;
		FTestSecret contentSecret;
		contentSecret.bytes.fill(165);
		DATA_BLOB contentInput{static_cast<DWORD>(contentSecret.bytes.size()), contentSecret.bytes.data()};
		FTestBlob protectedContent;
		Require(CryptProtectData(&contentInput, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
			&protectedContent.value) && protectedContent.value.pbData != nullptr, "content key fixture DPAPI protection failed");
		const auto contentPath = directory.parent_path() / L"local-app-data/TextRPG/Development/GameData.key";
		const TData contentBlob(protectedContent.value.pbData, protectedContent.value.pbData + protectedContent.value.cbData);
		WriteDisk(contentPath, contentBlob);
		TextRPG::FSaveDataStore store(directory);
		TData restored = kSentinel;
		Require(!store.Load(restored, error) && restored == kSentinel && !std::filesystem::exists(directory),
			"missing load created state or changed output");
		Pass("save-load-missing-does-not-create");
		Require(store.Save(kOriginal, error) && store.Load(restored, error) && restored == kOriginal, "service roundtrip failed");
		Pass("save-service-roundtrip");
		WriteDisk(directory / L"unowned.tmp", kSentinel);
		const auto save = directory / L"InGameData.sav";
		const auto key = directory / L"SaveData.key";
		const auto keyBytes = ReadDisk(key);
		const auto first = ReadDisk(save);
		Require(store.Save(kOriginal, error) && ReadDisk(key) == keyBytes && ReadDisk(save) != first, "key changed or nonce reused");
		const auto baseline = ReadDisk(save);
		FTestSecret secret;
		ReadSaveSecret(directory, secret);
		Require(secret.bytes != contentSecret.bytes && ReadDisk(contentPath) == contentBlob, "content key reused or changed");
		Require(TextRPG::DecryptSaveData(baseline, secret.bytes, restored, error) && restored == kOriginal, "DPAPI save key unusable");
		Require(lastWrite == baseline && lastWrite != kOriginal, "disk writer did not receive ciphertext");
		const auto other = directory.parent_path() / L"independent-key";
		Require(TextRPG::FSaveDataStore(other).Save(kOriginal, error), "independent key save failed");
		FTestSecret otherSecret;
		ReadSaveSecret(other, otherSecret);
		Require(secret.bytes != otherSecret.bytes, "independent save directories reused key");
		restored = kSentinel;
		Require(!TextRPG::DecryptSaveData(baseline, otherSecret.bytes, restored, error) && restored == kSentinel,
			"separate key decrypted save");
		Pass("save-real-dpapi-dedicated-stable-independent-key");
		NoTemporary(directory);
		Pass("save-ciphertext-only-publication");

		for (const EFault mode : {EFault::Write, EFault::Flush, EFault::Close})
		{
			fault = mode;
			injection = {};
			writeHandle = INVALID_HANDLE_VALUE;
			const bool saved = store.Save(kSentinel, error);
			Require(FaultAccepted(mode, saved, error, injection, fault),
				"save fault did not reach intended real handle or was accepted");
			Require(ReadDisk(save) == baseline && ReadDisk(key) == keyBytes, "failed save changed prior bytes");
			NoTemporary(directory);
			DWORD flags = 0;
			Require(!GetHandleInformation(injection.reachedHandle, &flags), "failed temporary handle leaked");
			if (mode == EFault::Write) Pass("save-partial-write-failure-hit1-old-bytes-preserved");
			else if (mode == EFault::Flush) Pass("save-flush-failure-hit1-old-bytes-preserved");
			else Pass("save-close-failure-hit1-old-bytes-preserved");
			FaultOracleControls(mode, error);
		}
		Pass("save-write-flush-close-oracle-negative-controls");
		FailedPartialPrerequisite(save);
		Require(ReadDisk(save) == baseline && ReadDisk(key) == keyBytes, "partial prerequisite control changed prior bytes");
		NoTemporary(directory);
		Pass("save-real-failed-partial-prerequisite-not-completed");
		HANDLE locked = CreateFileW(save.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		Require(locked != INVALID_HANDLE_VALUE, "destination lock fixture failed");
		const bool saved = store.Save(kSentinel, error);
		const bool closed = CloseHandle(locked) != FALSE;
		Require(!saved && closed && error == "Could not publish save temporary file." && ReadDisk(save) == baseline,
			"real locked destination did not preserve old bytes");
		NoTemporary(directory);
		Pass("save-real-locked-destination-publication-preserved");
		fault = EFault::Read;
		injection = {};
		restored = kSentinel;
		const bool loaded = store.Load(restored, error);
		Require(FaultAccepted(EFault::Read, loaded, error, injection, fault) && restored == kSentinel && ReadDisk(save) == baseline,
			"read failure changed output/file or fault not reached");
		Pass("save-read-failure-hit1-output-preserved");
		FaultOracleControls(EFault::Read, error);
		Require(ReadDisk(key) == keyBytes, "read fault changed prior key bytes");
		NoTemporary(directory);
		DWORD readFlags = 0;
		Require(!GetHandleInformation(injection.reachedHandle, &readFlags), "failed read handle leaked");
		Pass("save-read-oracle-negative-controls");
		TData corrupt = baseline;
		corrupt.back() ^= 64;
		WriteDisk(save, corrupt);
		Require(!store.Load(restored, error) && restored == kSentinel && ReadDisk(save) == corrupt, "corrupt load changed state");
		WriteDisk(save, {});
		Require(!store.Load(restored, error) && restored == kSentinel && ReadDisk(save).empty(), "empty save load accepted");
		WriteDisk(save, TData(16U * 1024U * 1024U + 45, 0));
		Require(!store.Load(restored, error) && restored == kSentinel, "oversized disk save accepted");
		WriteDisk(save, baseline);
		Require(!store.Save(TData(16U * 1024U * 1024U + 1, 0), error) && ReadDisk(save) == baseline,
			"oversized service save changed prior bytes");
		Pass("save-corrupt-and-empty-load-preserved");
		std::filesystem::remove(key);
		Require(!store.Save(kSentinel, error) && !std::filesystem::exists(key) && ReadDisk(save) == baseline,
			"orphan save generated replacement key");
		Require(!store.Load(restored, error) && restored == kSentinel && !std::filesystem::exists(key), "load generated missing key");
		Pass("save-existing-save-missing-key-no-rekey");
		WriteDisk(key, kSentinel);
		Require(!store.Save(kOriginal, error) && ReadDisk(key) == kSentinel && ReadDisk(save) == baseline,
			"corrupt save key overwritten");
		Require(!store.Load(restored, error) && restored == kSentinel, "corrupt key load accepted");
		WriteDisk(key, {});
		Require(!store.Save(kOriginal, error) && ReadDisk(key).empty() && ReadDisk(save) == baseline, "empty key overwritten");
		WriteDisk(key, TData(65537, 0));
		Require(!store.Load(restored, error) && restored == kSentinel && ReadDisk(key).size() == 65537, "oversized key accepted");
		WriteDisk(key, keyBytes);
		Pass("save-corrupt-key-never-overwritten");
		Require(store.Load(restored, error) && restored == kOriginal && store.Save(kOriginal, error), "post-fault positive control failed");
		Require(ReadDisk(contentPath) == contentBlob, "save service modified development content key");
		Pass("save-post-fault-positive-control");
	}

	void Paths(const std::filesystem::path& directory)
	{
		std::string error;
		std::filesystem::path result;
		Require(TextRPG::ResolveSaveDataDirectory(result, error) && result == directory, "override resolution failed");
		Pass("save-explicit-isolated-root");
		for (const wchar_t* bad : {L"", L"relative", L"C:relative", L"\\\\server\\share", L"\\\\?\\C:\\save", L"C:/a/../b",
			L"C:/a/NUL.dat", L"C:/a/CON", L"C:/a:stream", L"C:/a/trailing.", L"C:/a/bad?"})
		{
			Require(SetEnvironmentVariableW(L"TEXT_RPG_SAVE_ROOT", bad) != FALSE, "set invalid environment failed");
			result = L"sentinel";
			Require(!TextRPG::ResolveSaveDataDirectory(result, error) && result == L"sentinel" && !error.empty(),
				"bad override fell back or changed output");
			TData output = kSentinel;
			TextRPG::FSaveDataStore invalid{std::filesystem::path(bad)};
			Require(!invalid.Save(kOriginal, error) && !invalid.Load(output, error) && output == kSentinel,
				"explicit invalid service directory accepted");
		}
		Require(SetEnvironmentVariableW(L"TEXT_RPG_SAVE_ROOT", nullptr) != FALSE, "remove override failed");
		const DWORD size = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
		std::wstring local(size, L'\0');
		Require(size > 1 && GetEnvironmentVariableW(L"LOCALAPPDATA", local.data(), size) == size - 1, "localappdata fixture missing");
		local.resize(size - 1);
		Require(TextRPG::ResolveSaveDataDirectory(result, error)
			&& result == std::filesystem::path(local) / L"TextRPG/Saves/Default", "default directory mismatch");
		Require(!std::filesystem::exists(result), "path resolver created default save files");
		Require(SetEnvironmentVariableW(L"LOCALAPPDATA", L"relative") != FALSE, "set invalid localappdata failed");
		result = L"sentinel";
		Require(!TextRPG::ResolveSaveDataDirectory(result, error) && result == L"sentinel", "invalid localappdata accepted");
		Require(SetEnvironmentVariableW(L"LOCALAPPDATA", nullptr) != FALSE, "remove localappdata failed");
		Require(!TextRPG::ResolveSaveDataDirectory(result, error) && result == L"sentinel", "missing localappdata accepted");
		Require(SetEnvironmentVariableW(L"LOCALAPPDATA", local.c_str()) != FALSE
			&& SetEnvironmentVariableW(L"TEXT_RPG_SAVE_ROOT", directory.c_str()) != FALSE, "restore test child environment failed");
		Pass("save-invalid-empty-override-no-fallback-and-default-root");
	}

	void TransientPublish(const std::filesystem::path& directory)
	{
		const auto root = directory.parent_path() / L"transient-publication";
		TextRPG::FSaveDataStore store(root);
		std::string error;
		Require(store.Save(kOriginal, error), "transient fixture positive control failed");
		transientPublication = true;
		transientHits = 0;
		const bool saved = store.Save(kSentinel, error);
		Require(transientHits == 1 && !transientPublication, "transient rename fault not reached");
		Require(saved, "transient access-denied publication was not retried");
		TData output;
		Require(store.Load(output, error) && output == kSentinel, "retried publication is not authenticated");
		Pass("save-transient-publication-hit1-retried-and-authenticated");
	}

	void LateWinner(const std::filesystem::path& directory)
	{
		const auto root = directory.parent_path() / L"late-key-winner";
		std::string error;
		publishLateWinner = true;
		lateWinnerPublished = false;
		const bool saved = TextRPG::FSaveDataStore(root).Save(kSentinel, error);
		Require(lateWinnerPublished && !publishLateWinner, "late winner boundary was not reached");
		if (!saved) std::cerr << "FAIL late-key-winner: " << error << '\n';
		Require(saved, "existing valid winner key was not re-read after initial missing result");
		TData output;
		Require(TextRPG::FSaveDataStore(root).Load(output, error) && output == kSentinel, "late winner key/save mismatch");
		Pass("save-late-key-winner-revalidated-before-orphan-rejection");
	}

	struct FEvent final
	{
		HANDLE value = nullptr;
		~FEvent() { if (value != nullptr) CloseHandle(value); }
	};

	struct FChildren final
	{
		std::array<PROCESS_INFORMATION, 6> values{};
		~FChildren()
		{
			for (auto& child : values)
			{
				if (child.hThread != nullptr) CloseHandle(child.hThread);
				if (child.hProcess != nullptr)
				{
					if (WaitForSingleObject(child.hProcess, 1000) == WAIT_TIMEOUT)
					{
						TerminateProcess(child.hProcess, 99);
						WaitForSingleObject(child.hProcess, 1000);
					}
					CloseHandle(child.hProcess);
				}
			}
		}
	};

	void Race(const wchar_t* executable, const std::filesystem::path& directory)
	{
		const auto root = directory.parent_path() / L"key-race";
		const auto name = L"Local\\TextRPG.SaveTest." + std::to_wstring(GetCurrentProcessId());
		FEvent gate;
		gate.value = CreateEventW(nullptr, TRUE, FALSE, name.c_str());
		if (gate.value == nullptr) throw std::runtime_error("race gate creation failed");
		FChildren children;
		for (auto& child : children.values)
		{
			std::wstring command = L"\"" + std::wstring(executable) + L"\" --child-save \"" + root.wstring() + L"\" \"" + name + L"\"";
			STARTUPINFOW startup{};
			startup.cb = sizeof(startup);
			if (!CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child))
			{
				throw std::runtime_error("race child creation failed");
			}
		}
		Require(SetEvent(gate.value) != FALSE, "race start failed");
		for (const auto& child : children.values)
		{
			DWORD exitCode = 99;
			Require(WaitForSingleObject(child.hProcess, 20000) == WAIT_OBJECT_0
				&& GetExitCodeProcess(child.hProcess, &exitCode), "concurrent child wait failed");
			if (exitCode != 0)
			{
				std::cerr << "FAIL concurrent child " << child.dwProcessId << " exit=" << exitCode << '\n';
				throw std::runtime_error("concurrent key/save child failed; see owned race-child diagnostic");
			}
		}
		TData output;
		std::string error;
		Require(TextRPG::FSaveDataStore(root).Load(output, error) && output == kOriginal, "race winning key cannot decrypt save");
		WriteDisk(root / L"unowned.tmp", kSentinel);
		NoTemporary(root);
		Pass("save-six-process-key-publication-race-real-dpapi");
	}
}

int wmain(int argc, wchar_t* argv[])
{
	try
	{
		if (argc == 4 && std::wstring_view(argv[1]) == L"--child-save")
		{
			HANDLE gate = OpenEventW(SYNCHRONIZE, FALSE, argv[3]);
			if (gate == nullptr) return 10;
			const DWORD waited = WaitForSingleObject(gate, 20000);
			const bool closed = CloseHandle(gate) != FALSE;
			if (waited != WAIT_OBJECT_0 || !closed) return 11;
			std::string error;
			if (TextRPG::FSaveDataStore(std::filesystem::path(argv[2])).Save(SaveTest::kOriginal, error)) return 0;
			const auto diagnostic = std::filesystem::path(argv[2]).parent_path()
				/ (L"race-child-" + std::to_wstring(GetCurrentProcessId()) + L".error.txt");
			std::ofstream log(diagnostic);
			log << error << " publicationWin32=" << SaveTest::publicationFailure;
			return 12;
		}
		SaveTest::Require(argc == 2, "isolated service root argument required");
		const std::filesystem::path directory(argv[1]);
		SaveTest::Crypto();
		SaveTest::Paths(directory);
		SaveTest::Service(directory);
		SaveTest::TransientPublish(directory);
		SaveTest::LateWinner(directory);
		SaveTest::Race(argv[0], directory);
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "FAIL " << error.what() << '\n';
		return 1;
	}
}
