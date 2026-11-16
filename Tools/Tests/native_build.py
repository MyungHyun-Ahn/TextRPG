"""Shared MSBuild backend for the existing native-test entry points (not a runner)."""
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]


def generated_source_paths(suite, generated_root=None):
    roots = [generated_root.resolve() if generated_root is not None else ROOT / 'GameData/Generated/GameData',
             ROOT / 'GameData/Generated/InGameData']
    if suite == 'InGameData':
        roots += [ROOT / 'Out/Tests/InGameDataSchema' / name for name in ('SharedGraph', 'CompatibilityV1')]
    return sorted({path for root in roots for path in root.rglob('*') if path.suffix in ('.h', '.cpp')})


def native_contract_sources(suite, generated_root=None):
    paths = [ROOT / name for name in ('NativeTests.vcxproj', 'NativeTests.vcxproj.filters',
             'ConsoleGameCore/ConsoleGameCore.vcxproj', 'GameData/GameData.vcxproj', 'Tools/Tests/native_build.py')]
    paths += list((ROOT / 'ConsoleGameCore').rglob('*.h')) + list((ROOT / 'ConsoleGameCore').rglob('*.cpp'))
    paths += list((ROOT / 'GameData/Runtime').rglob('*.h')) + list((ROOT / 'GameData/Runtime').rglob('*.cpp'))
    paths += list((ROOT / 'GameData/InGameDataSchema').glob('*.yaml'))
    paths += list((ROOT / 'Tools/GameDataGenerator').glob('*.cs')) + list((ROOT / 'Tools/GameDataGenerator').glob('*.csproj'))
    if suite in ('SceneUiRuntime', 'InGameData', 'SaveData'):
        paths += [ROOT / 'TextRPG' / name for name in
                  ('pch.h', 'pch.cpp', 'Scene/IScene.h')]
    if suite == 'InGameData':
        paths += list((ROOT / 'Tests/InGameData/Schema').glob('*.yaml'))
        paths += list((ROOT / 'TextRPG/CustomInGameData').glob('*.cpp'))
        paths += list((ROOT / 'TextRPG/CustomInGameData').glob('*.h'))
    return paths + generated_source_paths(suite, generated_root)


def run_directory(scratch_root, label):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,79}", label) or label.endswith("."):
        raise ValueError("label must be a single safe directory name")
    directory = scratch_root.resolve() / label
    directory.mkdir(parents=True, exist_ok=False)
    return directory


def write_native_report(path, report):
    """배타적으로 만든 pending만 소유한다. write/close/replace 실패는 이전 바이트를 보존한다."""
    pending = path.with_name('.' + path.name + '.' + uuid.uuid4().hex + '.pending')
    owned = False
    try:
        stream = pending.open('x', encoding='utf-8', newline='\n')
        owned = True
        with stream:
            text = json.dumps(report, ensure_ascii=False, indent=2)
            if stream.write(text) != len(text):
                raise OSError('short report write')
            stream.flush()
        pending.replace(path)
    except BaseException:
        if owned:
            try:
                pending.unlink(missing_ok=True)
            except OSError:
                pass  # 원래 게시 실패를 정리 실패로 교체하지 않는다.
        raise


@contextmanager
def native_report(work, report, summary):
    """import main은 예외를 보존한다. 최종 출력/drain 뒤에만 전체 성공을 게시한다."""
    path = work / 'results.json'
    stage = 'initial-publication'
    try:
        write_native_report(path, dict(report, passed=False, status='incomplete'))
        stage = 'execution'
        yield report
        stage = 'final-output'
        print(json.dumps(summary(), ensure_ascii=False, indent=2))
        print(str(work))
        stage = 'final-flush'
        for stream in (sys.stdout, sys.stderr):
            if stream is None:
                raise OSError('required CLI stream unavailable')
            stream.flush()
        stage = 'final-publication'
        report['status'] = 'complete' if report.get('passed') else 'failed'
        write_native_report(path, report)
    except BaseException as error:
        report.update(passed=False, status='failed', error={'type': type(error).__name__, 'stage': stage})
        if 'records' in report:
            report['recordCount'] = len(report['records'])
        # 게시 자체의 실패에서는 재시도로 이전 canonical을 덮어쓰지 않는다.
        if stage not in ('initial-publication', 'final-publication'):
            try:
                write_native_report(path, report)
            except BaseException as publication_error:
                error.add_note('failure report publication: ' + type(publication_error).__name__)
        raise


def native_cli(main_function):
    """실행형 CLI만 예외를 exit 1로 변환한다. import main은 호출자에게 예외를 전달한다."""
    try:
        return main_function()
    except SystemExit:
        raise  # argparse help/usage는 기존 종료 코드를 유지한다.
    except BaseException as error:
        for name in ('stdout', 'stderr'):
            stream = getattr(sys, name)
            if stream is not None:
                try:
                    stream.flush()
                except BaseException:
                    setattr(sys, name, None)
        if sys.stderr is not None:
            try:
                sys.stderr.write('NATIVE_RUNNER_FAILED type=' + type(error).__name__ + '\n')
                sys.stderr.flush()
            except BaseException:
                sys.stderr = None
        return 1


def core_dependency_verdict(build, output, positive_control):
    """정확한 consumer include 실패만 통과시킨다. 로그 대조도 같은 판정을 쓴다."""
    control_passed = (positive_control.get('compileExit') == 0 and positive_control.get('warnings') == 0
                      and positive_control.get('runExit') == 0 and positive_control.get('passCheckpointGroups', 0) > 0)
    text = output.decode('utf-8', errors='replace') if isinstance(output, bytes) else output
    source = str(ROOT / 'Tests/ConsoleGameCore/Main.cpp').replace('\\', '/')
    project = str(ROOT / 'NativeTests.vcxproj').replace('\\', '/')
    expected = re.compile(r'\s*' + re.escape(source) +
                          r'\(3,10\): (?:fatal )?error C1083: [^\r\n]*[\'\"]Common/GameDataEnums\.g\.h[\'\"][^\r\n]* \[' +
                          re.escape(project) + r'\]\s*', re.IGNORECASE)
    diagnostics = []
    diagnostic_stages = []
    first_stage = None
    stage = None
    injection_observed = False
    for line in text.splitlines():
        normalized = line.replace('\\', '/')
        if re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*:', line.strip()):
            stage = line.strip()[:-1]
        if (stage == 'ClCompile' and re.search(r'\bCL\.exe\b', line, re.IGNORECASE)
                and re.search(r'/D\s+CORE_DEPENDENCY_NEGATIVE\b', line)
                and 'Tests/ConsoleGameCore/Main.cpp' in normalized):
            injection_observed = True
        if re.search(r'\b(?:fatal\s+)?error(?:\s+[A-Z]+[0-9]+)?\s*:', line, re.IGNORECASE):
            if not diagnostics:
                first_stage = stage
            diagnostics.append(normalized)
            diagnostic_stages.append(stage)
    reasons = []
    if not control_passed:
        reasons.append('standalone-success-control-failed')
    if build.get('compileExit') != 1:
        reasons.append('not-completed-expected-build-failure')
    if build.get('warnings') != 0 or re.search(r'\bwarning(?:\s+[A-Z]+[0-9]+)?\s*:', text, re.IGNORECASE):
        reasons.append('warnings')
    if build.get('suite') != 'ConsoleGameCore' or build.get('coreDependencyNegative') is not True:
        reasons.append('wrong-build-variant')
    if not injection_observed or not diagnostic_stages or any(value != 'ClCompile' for value in diagnostic_stages):
        reasons.append('wrong-stage-or-unhit-injection')
    if not diagnostics or not all(expected.fullmatch(line) for line in diagnostics):
        reasons.append('unexpected-source-header-or-additional-diagnostic')
    return {'passed': not reasons, 'reasons': reasons, 'diagnosticOccurrences': len(diagnostics),
            'firstDiagnosticStage': first_stage, 'injectionObserved': injection_observed,
            'positiveControlPassed': control_passed}


def _visual_studio():
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    installations = json.loads(subprocess.check_output(
        [str(vswhere), "-all", "-products", "*", "-format", "json", "-utf8"]).decode("utf-8-sig"))
    for item in installations:
        root = Path(item["installationPath"])
        msbuild = root / "MSBuild/Current/Bin/MSBuild.exe"
        if item["installationVersion"].startswith("18.") and msbuild.is_file() and (root / "VC/Tools/MSVC").is_dir():
            return msbuild
    raise RuntimeError("VS18 MSBuild and C++ toolchain not found")


def _property(value):
    # MSBuild property-list metacharacters must remain literal path characters.
    value = str(value)
    for source, target in [("%", "%25"), (";", "%3B"), (",", "%2C"), ("$", "%24"),
                           ("@", "%40"), ("'", "%27"), ("(", "%28"), (")", "%29")]:
        value = value.replace(source, target)
    if any(character in value for character in "\r\n"):
        raise ValueError("newline in MSBuild property")
    return value


def _without_textart(generated, cache):
    # Per-cache fixture: its lock also covers header publication and compiler reads.
    headers = sorted(p for p in generated.rglob("*.h") if "TextArt" not in p.relative_to(generated).parts)
    fixture = cache / "headers-without-textart"
    expected = {source.relative_to(generated) for source in headers}
    for retired in fixture.rglob('*.h'):
        if retired.relative_to(fixture) not in expected:
            retired.unlink()
    for source in headers:
        target = fixture / source.relative_to(generated)
        data = source.read_bytes()
        if not target.exists() or target.read_bytes() != data:
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
    return fixture


def _state(directory, suffix):
    return {p.name: (p.stat().st_mtime_ns, p.stat().st_size) for p in directory.glob("*" + suffix)}


@contextmanager
def build_native_test(suite, architecture, configuration, work, *, generated_root=None,
                      without_textart=False, analyze=False, core_dependency_negative=False):
    if suite not in ("SceneUiRuntime", "InputManager", "GameDataStore", "GameDataStoreExceptions", "SaveData", "InGameData", "ConsoleGameCore") or architecture not in ("x64", "x86"):
        raise ValueError("unsupported native test target")
    if configuration not in ("Debug", "Release"):
        raise ValueError("unsupported configuration")
    generated = (generated_root or ROOT / "GameData/Generated/GameData").resolve()
    uses_generated = suite not in ("InputManager", "SaveData", "ConsoleGameCore")
    if uses_generated and not generated.is_dir() and generated != (ROOT / "GameData/Generated/GameData").resolve():
        raise ValueError("generated headers directory does not exist")
    # 기본 출력의 최초 생성/타입 추가는 같은 obj/PCH 캐시에서 MSBuild 의존 추적이 처리한다.
    source_set = (sorted(p.relative_to(generated).as_posix() for p in generated.rglob("*.h"))
                  if uses_generated and generated != (ROOT / "GameData/Generated/GameData").resolve() else [])
    signature = json.dumps([str(generated).lower() if uses_generated else None,
                            source_set, without_textart, analyze])
    if core_dependency_negative:
        if suite != "ConsoleGameCore":
            raise ValueError("core dependency negative requires ConsoleGameCore")
        signature += "|core-dependency-negative"
    variant = hashlib.sha256(signature.encode()).hexdigest()[:16]
    platform = "Win32" if architecture == "x86" else "x64"
    cache = ROOT / "Out/Tests/NativeRuntime" / suite / variant / configuration / platform
    cache.mkdir(parents=True, exist_ok=True)
    intermediate = cache / "obj"
    intermediate.mkdir(exist_ok=True)
    # Serialize a shared cache through execution too: another caller must not relink a running EXE.
    import msvcrt
    with (cache / "build.lock").open("a+b") as lock:
        lock.seek(0, 2)
        if not lock.tell():
            lock.write(b"0")
            lock.flush()
        lock.seek(0)
        deadline = time.monotonic() + 180
        while True:
            try:
                msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
                break
            except OSError:
                if time.monotonic() >= deadline:
                    raise TimeoutError("native build cache is busy")
                time.sleep(0.1)
        try:
            optional_source = generated
            if without_textart:
                # 같은 MSBuild invocation의 생성 target 뒤에서 소유 fixture를 갱신한다.
                generated = cache / "headers-without-textart"
            environment = os.environ.copy()
            environment.pop("LIB", None)
            # MSBuild Exec uses cmd.exe, whose inherited PATH can exceed its limit.
            # Keep this child-only tool path short; never modify the user's environment.
            import shutil
            dotnet = shutil.which("dotnet")
            windows = Path(os.environ.get("SystemRoot", "C:/Windows"))
            for name in list(environment):
                if name.lower() == "path":
                    del environment[name]
            environment["PATH"] = os.pathsep.join(([str(Path(dotnet).parent)] if dotnet else []) +
                                                  [str(windows / "System32"), str(windows)])
            environment.update({name: str(work) for name in ("TMP", "TEMP", "TMPDIR")})
            command = [str(_visual_studio()), str(ROOT / "NativeTests.vcxproj"), "-nologo", "-v:normal",
                       "-t:Build", "-m:1", "-warnaserror", f"-p:Configuration={configuration}",
                       f"-p:Platform={platform}", f"-p:NativeTestSuite={suite}",
                       "-p:NativeGeneratedRoot=" + _property(generated),
                       "-p:NativeWithoutTextArt=" + str(without_textart).lower(),
                       "-p:NativeAnalyze=" + str(analyze).lower(),
                       "-p:NativeCoreDependencyNegative=" + str(core_dependency_negative).lower(),
                       "-p:NativeUseGameDataLibrary=" + str(suite in ("SceneUiRuntime", "InGameData") and not without_textart and generated == (ROOT / "GameData/Generated/GameData").resolve()).lower(),
                       "-p:OutDir=" + _property(str(cache) + "/"),
                       "-p:IntDir=" + _property(str(intermediate) + "/")]
            if without_textart:
                command += ["-p:NativeGeneratedSourceRoot=" + _property(optional_source),
                            "-p:NativePythonExecutable=" + _property(sys.executable)]
            (work / "build-command.json").write_text(json.dumps(command, indent=2), encoding="utf-8")
            before = _state(intermediate, ".obj")
            pch_before = _state(intermediate, ".pch")
            started = time.perf_counter()
            process = subprocess.Popen(command, cwd=ROOT, env=environment, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NEW_PROCESS_GROUP)
            try:
                output, _ = process.communicate(timeout=180)
                exit_code = process.returncode
            except subprocess.TimeoutExpired:
                subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True, check=False)
                output, _ = process.communicate()
                exit_code = 124
            elapsed = time.perf_counter() - started
            (work / "compile.log").write_bytes(output)
            after = _state(intermediate, ".obj")
            pch_after = _state(intermediate, ".pch")
            result = {"compileExit": exit_code, "warnings": len(re.findall(rb"warning [A-Z]+[0-9]+", output)),
                      "suite": suite, "coreDependencyNegative": core_dependency_negative,
                      "buildSeconds": elapsed, "compiledObjects": [n for n in after if before.get(n) != after[n]],
                      "pchRebuilt": pch_before != pch_after, "cacheDirectory": str(cache),
                      "executable": str(cache / "NativeTests.exe"), "analysisEnabled": analyze,
                      "analysisObserved": b"/analyze" in output, "buildCommand": command}
            yield result, environment
        finally:
            lock.seek(0)
            msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
