"""실제 MSVC 분석과 AES-GCM 빈 입력/인증 실패/키 초기화 실패 회귀를 격리 실행한다."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile


HARNESS = r'''#include "GameData/pch.h"

namespace
{
	int keyMode = 0;
	NTSTATUS WINAPI GenerateKey(BCRYPT_ALG_HANDLE algorithm, BCRYPT_KEY_HANDLE* key,
		PUCHAR object, ULONG objectSize, PUCHAR secret, ULONG secretSize, ULONG flags)
	{
		if (keyMode != 0)
		{
			*key = nullptr;
			return keyMode == 1 ? static_cast<NTSTATUS>(0xC0000001L) : 0;
		}
		return BCryptGenerateSymmetricKey(algorithm, key, object, objectSize, secret, secretSize, flags);
	}
}
// 실제 구현을 사용하되 재현하기 어려운 공급자 초기화 실패만 주입한다.
#define BCryptGenerateSymmetricKey GenerateKey
#include "GameData/FGameDataPackage.cpp"
#undef BCryptGenerateSymmetricKey

void Require(bool condition, const char* message)
{
	if (!condition)
	{
		std::cerr << message << '\n';
		std::exit(1);
	}
}

int main()
{
	std::array<std::uint8_t, 32> key{};
	Require(BCryptOk(BCryptGenRandom(nullptr, key.data(), static_cast<ULONG>(key.size()),
		BCRYPT_USE_SYSTEM_PREFERRED_RNG)), "test key generation failed");
	std::array<std::uint8_t, kNonceSize> nonce{};
	std::string error;
	unsigned cases = 0;
	for (std::size_t size : { std::size_t{0}, std::size_t{1}, std::size_t{32} })
	{
		for (bool retained : { false, true })
		{
			TBytes plain(size, 0x42);
			TBytes cipher;
			TBytes decoded;
			if (retained)
			{
				plain.reserve(64);
				cipher.reserve(64);
				decoded.reserve(64);
			}
			Require(BCryptOk(BCryptGenRandom(nullptr, nonce.data(), static_cast<ULONG>(nonce.size()),
				BCRYPT_USE_SYSTEM_PREFERRED_RNG)), "test nonce generation failed");
			TBytes aad = BuildAssociatedData(size);
			std::array<std::uint8_t, kTagSize> tag{};
			Require(Crypt(true, plain, key, nonce, aad, cipher, tag, error), "encryption failed");
			Require(cipher.size() == plain.size(), "cipher length changed");
			Require(Crypt(false, cipher, key, nonce, aad, decoded, tag, error), "decryption failed");
			Require(decoded == plain, "plaintext mismatch");
			tag[0] ^= 1;
			Require(!Crypt(false, cipher, key, nonce, aad, decoded, tag, error), "invalid tag accepted");
			Require(decoded.empty(), "failed authentication retained plaintext");
			Require(error == "Package authentication failed or the key is incorrect.", "wrong authentication diagnostic");
			++cases;
		}
	}
	for (int mode : { 1, 2 })
	{
		keyMode = mode;
		TBytes plain{ 1, 2, 3 };
		TBytes cipher;
		TBytes aad = BuildAssociatedData(plain.size());
		std::array<std::uint8_t, kTagSize> tag{};
		Require(!Crypt(true, plain, key, nonce, aad, cipher, tag, error), "invalid key initialization accepted");
		Require(cipher.empty(), "initialization failure published output");
		Require(error == "Could not initialize AES-GCM.", "null/failed key escaped initialization boundary");
		++cases;
	}
	keyMode = 0;
	// 같은 인증 경로로 경로와 원시 바이트가 함께 보호되는지 확인한다.
	const auto root = std::filesystem::current_path() / "raw-package";
	std::filesystem::create_directories(root / "yaml");
	std::filesystem::create_directories(root / "assets/Nested");
	const std::string yaml = "answer: 42\n";
	std::ofstream(root / "yaml/one.yaml", std::ios::binary) << yaml;
	const TBytes raw{0xEF, 0xBB, 0xBF, ' ', ' ', 'x', ' ', '\r', '\n', 0, 0xFF};
	std::ofstream rawFile(root / "assets/Nested/MixedCase.bin", std::ios::binary);
	rawFile.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
	rawFile.close();
	std::ofstream(root / "assets/empty.bin", std::ios::binary).close();
	const auto package = root / "GameData.dat";
	Require(GameData::BuildEncryptedPackage(root / "yaml", package, key, error, root / "assets"), "raw package build failed");
	GameData::FGameDataPackageReader reader;
	Require(reader.Load(package, key, error), "raw package load failed");
	Require(reader.GetEntries().size() == 3, "unexpected explicit-root entry count");
	Require(reader.FindEntry("Asset/Nested/MixedCase.bin") && reader.FindEntry("Asset/Nested/MixedCase.bin")->contents == raw,
		"raw bytes, whitespace, BOM, CRLF or binary data changed");
	Require(reader.FindEntry("Asset/empty.bin") && reader.FindEntry("Asset/empty.bin")->contents.empty(), "empty asset changed");
	Require(reader.FindEntry("Data/Client/one.yaml") && !reader.FindEntry("one.yaml"), "YAML namespace missing");
	++cases;
	const auto publish = [&](const std::vector<std::string>& names)
	{
		TBytes plain;
		AppendU32(plain, static_cast<std::uint32_t>(names.size()));
		for (const auto& name : names)
		{
			AppendU32(plain, static_cast<std::uint32_t>(name.size()));
			plain.insert(plain.end(), name.begin(), name.end());
			AppendU64(plain, raw.size());
			plain.insert(plain.end(), raw.begin(), raw.end());
		}
		TBytes cipher;
		std::array<std::uint8_t, kTagSize> tag{};
		const TBytes aad = BuildAssociatedData(plain.size());
		Require(Crypt(true, plain, key, nonce, aad, cipher, tag, error), "metadata fixture encryption failed");
		TBytes bytes(kMagic.begin(), kMagic.end());
		AppendU32(bytes, kVersion);
		bytes.insert(bytes.end(), nonce.begin(), nonce.end());
		AppendU64(bytes, cipher.size());
		bytes.insert(bytes.end(), cipher.begin(), cipher.end());
		bytes.insert(bytes.end(), tag.begin(), tag.end());
		Require(WriteAtomic(package, bytes, error), "metadata fixture publish failed");
	};
	for (const std::vector<std::string>& names : {
		std::vector<std::string>{"Asset/a", "Asset/a"}, {"Asset/A", "Asset/a"}, {"Asset/../escape"},
		{"/Asset/absolute"}, {"Asset//empty"}, {"Asset/dir\\file"}, {"Asset/file:stream"}})
	{
		publish(names);
		Require(!reader.Load(package, key, error), "unsafe authenticated metadata accepted");
		Require(error == "Package entry metadata is invalid or duplicated.", "wrong metadata rejection");
		Require(reader.GetEntries().empty(), "failed reader published entries");
		++cases;
	}
	publish({"Asset/renamed.bin"});
	Require(reader.Load(package, key, error) && reader.FindEntry("Asset/renamed.bin")->contents == raw,
		"authenticated path-and-byte control failed");
	std::ifstream packageInput(package, std::ios::binary);
	TBytes bytes((std::istreambuf_iterator<char>(packageInput)), {});
	packageInput.close();
	bytes[28] ^= 1;
	Require(WriteAtomic(package, bytes, error), "tamper fixture write failed");
	Require(!reader.Load(package, key, error) && error.find("authentication") != std::string::npos,
		"path/payload ciphertext tampering bypassed authentication");
	++cases;
	SecureZeroMemory(key.data(), key.size());
	std::cout << "PASS " << cases << " crypto cases\n";
	return 0;
}
'''


def run(command, directory, label):
    result = subprocess.run(command, cwd=directory, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, timeout=180)
    (directory / (label + ".log")).write_bytes(result.stdout)
    return {"name": label, "exitCode": result.returncode, "command": command}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scratch-root", required=True, type=Path)
    parser.add_argument("--vcvarsall", type=Path)
    args = parser.parse_args()
    if not args.scratch_root.is_absolute():
        parser.error("--scratch-root must be absolute")
    root = Path(__file__).resolve().parents[2]
    args.scratch_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="package-crypto-", dir=args.scratch_root))
    print(f"logDirectory: {work}", flush=True)
    vcvars = args.vcvarsall
    if vcvars is None:
        vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        installation = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"]).decode("utf-8-sig").strip()
        if not installation:
            raise RuntimeError("MSVC installation not found")
        vcvars = Path(installation) / "VC/Auxiliary/Build/vcvarsall.bat"
    vcvars = vcvars.resolve(strict=True)
    results = []
    for arch in ("x64", "x86"):
        directory = work / arch
        directory.mkdir()
        (directory / "Crypto.cpp").write_text(HARNESS, encoding="utf-8")
        flags = '/nologo /std:c++20 /permissive- /utf-8 /EHsc /W4 /WX /sdl /Od /MDd /D_DEBUG /D_CONSOLE /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN'
        commands = {
            "analyze": f'cl {flags} /I"{root / "GameData/Runtime"}" /analyze /c "{root / "GameData/Runtime/GameData/FGameDataPackage.cpp"}" /Fo:Package.obj /Fd:Package.pdb',
            "compile-crypto": f'cl {flags} /I"{root / "GameData/Runtime"}" Crypto.cpp /Fe:Crypto.exe',
            "compile-packer": f'cl {flags} /I"{root / "GameData/Runtime"}" "{root / "Tools/GameDataPacker/GameDataPacker.cpp"}" "{root / "GameData/Runtime/GameData/FGameDataPackage.cpp"}" "{root / "GameData/Runtime/GameData/FGameDataKey.cpp"}" /Fe:GameDataPacker.exe /link crypt32.lib bcrypt.lib',
        }
        for label, command in commands.items():
            batch = directory / (label + ".cmd")
            batch.write_text(f'@echo off\ncall "{vcvars}" {arch}\nif errorlevel 1 exit /b %errorlevel%\n{command}\nexit /b %errorlevel%\n', encoding="utf-8")
            record = run([os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", str(batch)], directory, label)
            record.update(arch=arch, compilerCommand=command)
            results.append(record)
        if (directory / "Crypto.exe").exists():
            record = run([str(directory / "Crypto.exe")], directory, "crypto")
            record.update(arch=arch)
            results.append(record)
        if (directory / "GameDataPacker.exe").exists():
            import sys
            record = run([sys.executable, str(root / "Tools/Tests/test_package_safety.py"),
                          "--packer", str(directory / "GameDataPacker.exe"), "--scratch", str(directory)], directory, "package-safety")
            record.update(arch=arch)
            results.append(record)
    summary = {"passed": all(r["exitCode"] == 0 for r in results) and len(results) == 10,
               "results": results, "logDirectory": str(work)}
    (work / "results.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary))
    raise SystemExit(0 if summary["passed"] else 1)


if __name__ == "__main__":
    main()
