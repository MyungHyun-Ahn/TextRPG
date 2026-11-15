"""격리 LOCALAPPDATA에서 실제 공유 키 공급자/packer를 MSVC로 검증한다."""
import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import secrets
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scratch-root", type=Path, required=True)
    args = parser.parse_args()
    if not args.scratch_root.is_absolute():
        parser.error("--scratch-root must be absolute")
    args.scratch_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="key-provider-", dir=args.scratch_root))
    print(f"logDirectory: {work}", flush=True)
    root = Path(__file__).resolve().parents[2]
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    installation = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"]).decode("utf-8-sig").strip()
    vcvars = Path(installation) / "VC/Auxiliary/Build/vcvarsall.bat"
    records = []
    env = os.environ.copy()
    env.pop("TEXT_RPG_GAMEDATA_KEY", None)

    def execute(command, directory, label, environment=env, expected=0):
        result = subprocess.run(command, cwd=directory, env=environment, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, timeout=180)
        # 환경 키를 실수로 출력하는 회귀는 저장 전에 차단한다.
        encoded = environment.get("TEXT_RPG_GAMEDATA_KEY", "")
        if encoded and encoded.encode() in result.stdout:
            raise AssertionError("secret appeared in tool output")
        (directory / (label + ".log")).write_bytes(result.stdout)
        records.append({"name": label, "directory": str(directory), "exitCode": result.returncode,
                        "expected": expected, "passed": result.returncode == expected})
        if result.returncode != expected:
            raise AssertionError(f"{label}: exit {result.returncode}, expected {expected}")
        return result.stdout

    for arch in ("x64", "x86"):
        directory = work / arch
        directory.mkdir()
        local = directory / "isolated-local"
        isolated = dict(env, LOCALAPPDATA=str(local))
        flags = "/nologo /std:c++20 /permissive- /utf-8 /EHsc /W4 /WX /Y- /sdl /DNOMINMAX /DWIN32_LEAN_AND_MEAN"
        key = root / "GameData/Runtime/GameData/FGameDataKey.cpp"
        for analyze in (False, True):
            for packer in (False, True):
                label = ("packer" if packer else "provider") + ("-analysis" if analyze else "-normal")
                sources = [root / ("Tools/GameDataPacker/GameDataPacker.cpp" if packer else "Tests/GameDataKey/Main.cpp"), key]
                if packer:
                    sources.append(root / "GameData/Runtime/GameData/FGameDataPackage.cpp")
                name = "Packer.exe" if packer else "KeyTest.exe"
                compiler = f'cl {flags} ' + ("/analyze " if analyze else "") + f'/I"{root / "GameData/Runtime"}" '
                compiler += " ".join(f'"{s}"' for s in sources) + f" /Fe:{name} /link crypt32.lib bcrypt.lib"
                batch = directory / (label + ".cmd")
                batch.write_text(f'@echo off\ncall "{vcvars}" {arch}\nif errorlevel 1 exit /b %errorlevel%\n{compiler}\nexit /b %errorlevel%\n')
                output = execute([os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", str(batch)], directory, label)
                if b"warning C" in output or b"warning LNK" in output:
                    raise AssertionError("compiler warning")
        for analyze in (False, True):
            label = "exception-analysis" if analyze else "exception-normal"
            compiler = f'cl {flags} ' + ("/analyze " if analyze else "")
            compiler += f'/I"{root / "GameData/Runtime"}" /I"{root / "Tools/GameDataPacker"}" '
            compiler += f'"{root / "Tests/GameDataKey/PackerExceptions.cpp"}" "{key}" '
            compiler += f'"{root / "GameData/Runtime/GameData/FGameDataPackage.cpp"}" /Fe:ExceptionTest.exe /link crypt32.lib bcrypt.lib'
            batch = directory / (label + ".cmd")
            batch.write_text(f'@echo off\ncall "{vcvars}" {arch}\nif errorlevel 1 exit /b %errorlevel%\n{compiler}\nexit /b %errorlevel%\n')
            execute([os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", str(batch)], directory, label)
        execute([str(directory / "ExceptionTest.exe")], directory, "exception-run", isolated)
        execute([str(directory / "KeyTest.exe"), str(directory / "harness-local")], directory, "provider-run", isolated)
        packer = str(directory / "Packer.exe")
        file = local / "TextRPG/Development/GameData.key"
        execute([packer, "verify", "missing.dat"], directory, "read-does-not-provision", isolated, 2)
        assert not file.exists()
        # 여러 프로세스가 동시에 새 키를 준비해도 완성된 하나만 게시한다.
        def prepare(index):
            execute([packer, "prepare-development-key"], directory, f"concurrent-{index}", isolated)
        with concurrent.futures.ThreadPoolExecutor(max_workers=16) as pool:
            list(pool.map(prepare, range(32)))
        protected = file.read_bytes()
        execute([packer, "prepare-development-key"], directory, "idempotent", isolated)
        assert file.read_bytes() == protected
        assert sorted(p.name for p in file.parent.iterdir()) == ["GameData.key"]
        yaml = directory / "yaml"
        yaml.mkdir()
        (yaml / "Fixture.yaml").write_text("Fixture:\n  - Id: fixture\n", encoding="utf-8")
        package = directory / "GameData.dat"
        execute([packer, "pack", str(yaml), str(package)], directory, "dev-pack", isolated)
        execute([packer, "verify", str(package)], directory, "dev-verify", isolated)
        for index, bad in enumerate(("", "g" * 64, "a" * 63, "a" * 65, " " * 64)):
            invalid = dict(isolated, TEXT_RPG_GAMEDATA_KEY=bad)
            execute([packer, "prepare-development-key"], directory, f"invalid-prepare-{index}", invalid, 2)
            execute([packer, "verify", str(package)], directory, f"invalid-read-{index}", invalid, 2)
            assert file.read_bytes() == protected
        external = dict(isolated, TEXT_RPG_GAMEDATA_KEY=secrets.token_hex(32))
        file.write_bytes(b"malformed DPAPI file")
        for command in (["prepare-development-key"], ["verify", str(package)]):
            execute([packer, *command], directory, "malformed-" + command[0], isolated, 2)
            assert file.read_bytes() == b"malformed DPAPI file"
        execute([packer, "prepare-development-key"], directory, "environment-precedence", external)
        execute([packer, "pack", str(yaml), str(package)], directory, "environment-pack", external)
        execute([packer, "verify", str(package)], directory, "environment-verify", external)
        assert file.read_bytes() == b"malformed DPAPI file"
        external["TEXT_RPG_GAMEDATA_KEY"] = secrets.token_hex(32)
        execute([packer, "verify", str(package)], directory, "wrong-key", external, 1)
        execute([packer, "prepare-development-key", "extra"], directory, "prepare-usage", isolated, 2)
        execute([packer, "unknown"], directory, "unknown-usage", isolated, 2)
        # 기존 패키지 안전성 회귀도 변경한 실제 packer로 실행한다.
        import sys
        execute([sys.executable, str(root / "Tools/Tests/test_package_safety.py"), "--packer", packer,
                 "--scratch", str(directory)], directory, "package-safety", isolated)
    result = {"passed": all(r["passed"] for r in records), "records": records, "count": len(records)}
    (work / "results.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({"passed": result["passed"], "count": len(records), "logDirectory": str(work)}))


if __name__ == "__main__":
    main()
