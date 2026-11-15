"""입력 서비스의 실제 프로젝트 등록 계약. native 동작 검증과 별도다."""
from contextlib import contextmanager
from pathlib import Path
import json
import io
import re
import sys
import runpy
import subprocess
import tempfile
import unittest
from unittest import mock
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


class _ReportFaultWriter:
    """실제 owned pending I/O 뒤에만 실패를 주입하는 공통 unit control."""
    def __init__(self, stream, pending, fault, trace):
        self.stream, self.pending, self.fault, self.trace = stream, pending, fault, trace

    def __enter__(self):
        return self

    def fail(self):
        self.trace.update(faultHits=self.trace['faultHits'] + 1, pendingRawHex=self.pending.read_bytes().hex())
        raise OSError('synthetic pending ' + self.fault)

    def write(self, text):
        if self.fault in ('truncate', 'partial-write', 'short-write'):
            prefix = '' if self.fault == 'truncate' else text[:17]
            self.stream.write(prefix)
            self.stream.flush()
            if self.fault == 'short-write':
                self.trace.update(faultHits=self.trace['faultHits'] + 1, pendingRawHex=self.pending.read_bytes().hex())
                return len(prefix)
            self.fail()
        return self.stream.write(text)

    def flush(self):
        self.stream.flush()
        if self.fault == 'pending-flush':
            self.fail()

    def __exit__(self, kind, value, traceback):
        self.stream.close()
        self.trace['closeCompleted'] = True
        if self.fault == 'close-after-completed-close' and kind is None:
            self.fail()


def _scene_finalization_fault_matches(trace, result, fault):
    """실제 실패와 wrong-reason/unhit control에 같은 판정을 적용한다."""
    stage = {'final-write': 'final-output', 'final-flush': 'final-flush',
             'stderr-flush': 'final-flush', 'unlock': 'execution'}[fault]
    return (trace.get('faultHits') == 1 and trace.get('faultReason') == fault
            and trace.get('syntheticNativeCalls') == 1 and trace.get('buildContextExits', 0) > 0
            and trace.get('exitCode') == 1 and trace.get('successWritesAfterFault') == 0
            and result.get('passed') is False and result.get('status') == 'failed'
            and result.get('error') == {'type': 'OSError', 'stage': stage})


def _native_publication_worker(runner, fault, directory, imported=False, mode='default'):
    """실제 Python entry 경계만 검사한다. native 빌드/실행은 명시적인 format control이다."""
    import native_build
    root = Path(directory)
    trace = {'evidenceKind': 'synthetic-build-native-format-control', 'fault': fault,
             'faultHits': 0, 'successfulStreamWrites': 0, 'syntheticNativeCalls': 0,
             'successWritesAfterFault': 0, 'events': [], 'rootPublications': []}
    failure = OSError(5, 'synthetic invocation ' + fault)

    def fail(reason):
        if trace['faultHits'] == 0:
            trace.update(faultHits=1, faultReason=reason)
        else:
            trace['repeatFaults'] = trace.get('repeatFaults', 0) + 1
        raise failure

    class Sink(io.StringIO):
        def write(self, text):
            if trace['faultHits'] and '"passed": true' in text:
                trace['successWritesAfterFault'] += 1
            if fault == 'body-write' and 'PASS synthetic publication control' in text:
                super().write(text[:5])
                trace['successfulStreamWrites'] += 1
                fail('body-write')
            if '"passed": true' in text or '"sourceDrift":' in text:
                self.final_output = True
            if fault == 'final-write' and getattr(self, 'final_output', False) and text == '\n':
                fail('final-write')
            trace['events'].append('stdout-write')
            trace['successfulStreamWrites'] += 1
            return super().write(text)

        def flush(self):
            trace['events'].append('stdout-flush')
            if fault == 'final-flush' and getattr(self, 'final_output', False):
                fail('final-flush')
            return super().flush()

    class ErrorSink(io.StringIO):
        def flush(self):
            trace['events'].append('stderr-flush')
            if fault == 'stderr-flush' and getattr(sink, 'final_output', False):
                fail('stderr-flush')
            return super().flush()

    @contextmanager
    def build(suite, architecture, configuration, work, **kwargs):
        if fault == 'build-entry':
            fail('build-entry')
        negative = kwargs.get('core_dependency_negative', False)
        diagnostic = ('ClCompile:\n  CL.exe /c /D CORE_DEPENDENCY_NEGATIVE Tests\\ConsoleGameCore\\Main.cpp\n'
                      f"{ROOT / 'Tests/ConsoleGameCore/Main.cpp'}(3,10): error C1083: Cannot open include file: "
                      f"'Common/GameDataEnums.g.h': missing [{ROOT / 'NativeTests.vcxproj'}]\n")
        (work / 'compile.log').write_text(diagnostic if negative else '', encoding='utf-8')
        try:
            yield {'compileExit': 1 if negative else 0, 'warnings': 0, 'buildSeconds': 0.0,
                   'suite': suite, 'coreDependencyNegative': negative, 'compiledObjects': [], 'pchRebuilt': False,
                   'analysisObserved': False, 'executable': 'format-control-no-native.exe', 'buildCommand': ['format-control']}, {}
        finally:
            trace['buildContextExits'] = trace.get('buildContextExits', 0) + 1
            trace['events'].append('unlock-' + work.name)
            if (fault == 'unlock' and work.name == 'run') or (fault == 'positive-unlock' and work.name == 'standalone-control'):
                fail(fault)

    def native_control(*args, **kwargs):
        trace['syntheticNativeCalls'] += 1
        return subprocess.CompletedProcess([], 0, b'PASS synthetic publication control\n', b'')

    sink = Sink()
    code = None
    original_open, original_replace = Path.open, Path.replace
    original_read, original_write = Path.read_bytes, Path.write_bytes
    fingerprint_reads = 0
    root_opens = 0
    artifact_phase, separator, artifact_fault = fault.partition(':')

    def read_control(target):
        nonlocal fingerprint_reads
        if target == ROOT / 'NativeTests.vcxproj':
            fingerprint_reads += 1
            if (fault == 'fingerprint' and fingerprint_reads == 1) or (fault == 'drift' and fingerprint_reads == 2):
                fail(fault)
        if fault == 'compile-log' and target == root / 'run/compile.log':
            fail(fault)
        return original_read(target)

    def write_control(target, data):
        if fault == 'runtime-log' and target == root / 'run/run.log':
            fail(fault)
        return original_write(target, data)

    def open_control(target, mode='r', *args, **kwargs):
        nonlocal root_opens
        stream = original_open(target, mode, *args, **kwargs)
        if mode == 'x' and target.parent == root / 'run' and target.name.startswith('.results.json.'):
            root_opens += 1
            if root_opens in (1, 2):
                canonical = root / 'run/results.json'
                trace['priorRawHex'] = canonical.read_bytes().hex() if canonical.exists() else None
            if separator and root_opens == (1 if artifact_phase == 'initial' else 2):
                return _ReportFaultWriter(stream, target, artifact_fault, trace)
        return stream

    def replace_control(target, destination):
        if destination == root / 'run/results.json':
            candidate = json.loads(target.read_text(encoding='utf-8'))
            trace['rootPublications'].append({'passed': candidate['passed'], 'status': candidate['status']})
            if separator and artifact_fault == 'replace' and root_opens == (1 if artifact_phase == 'initial' else 2):
                trace.update(pendingRawHex=target.read_bytes().hex(), closeCompleted=True)
                fail('replace')
            trace['events'].append('replace-' + candidate['status'])
        return original_replace(target, destination)

    options = {'core': ['--core-only'], 'negative': ['--core-dependency-negative'],
               'optional': ['--without-textart'], 'progress': ['--progress-tests'],
               'boundary': ['--boundary-tests', '02'], 'root-entry': ['--root-entry-tests', 'getid'],
               'legacy': ['--yaml-root', str(ROOT / 'GameData/Data/Client')],
               'generated': ['--generated-root', str(ROOT / 'GameData/Class')], 'analysis': ['--analyze']}
    with mock.patch.object(native_build, 'build_native_test', build), \
         mock.patch('subprocess.run', side_effect=native_control), \
         mock.patch.object(Path, 'open', open_control), mock.patch.object(Path, 'replace', replace_control), \
         mock.patch.object(Path, 'read_bytes', read_control), mock.patch.object(Path, 'write_bytes', write_control), \
         mock.patch('sys.argv', [runner, '--scratch-root', str(root), '--architecture', 'x64', '--label', 'run',
                                *options.get(mode, [])]), \
         mock.patch('sys.stdout', sink), mock.patch('sys.stderr', ErrorSink()):
        try:
            if imported:
                namespace = runpy.run_path(str(ROOT / 'Tools/Tests' / runner), run_name='import_control')
                code = namespace['main']()
            else:
                runpy.run_path(str(ROOT / 'Tools/Tests' / runner), run_name='__main__')
        except SystemExit as error:
            code = error.code
            trace['exceptionType'] = 'SystemExit'
        except OSError as error:
            trace['exceptionType'] = 'OSError'
            trace['originalExceptionPreserved'] = error is failure
            code = 1
    trace['exitCode'] = code
    trace['pendingRemaining'] = [p.name for p in (root / 'run').glob('.*.pending')]
    canonical = root / 'run/results.json'
    trace['canonicalRawHex'] = canonical.read_bytes().hex() if canonical.exists() else None
    (root / 'trace.json').write_text(json.dumps(trace, indent=2), encoding='utf-8')
    return code


class InputBuildContract(unittest.TestCase):
    def test_native_runners_use_incremental_pch_msbuild(self):
        for filename in ("test_input_manager.py", "test_scene_ui_runtime.py",
                         "test_gamedata_store_runtime.py", "test_gamedata_store_exceptions.py"):
            with self.subTest(runner=filename):
                text = (ROOT / "Tools/Tests" / filename).read_text(encoding="utf-8-sig")
                self.assertIn("build_native_test(", text,
                              "existing runners must reuse the shared incremental MSBuild backend")
                self.assertNotIn("/Y-", text, "normal runtime tests must preserve PCH")
                self.assertNotIn("compile.rsp", text, "do not compile the entire source list per run")
                self.assertNotIn("text.replace(", text, "do not rewrite original source includes")


    def test_native_project_uses_original_paths_and_isolated_pch(self):
        project = ET.parse(ROOT / "NativeTests.vcxproj").getroot()
        filters = ET.parse(ROOT / "NativeTests.vcxproj.filters").getroot()
        configurations = {item.get("Include") for item in project.findall(".//m:ProjectConfiguration", NS)}
        self.assertEqual(configurations, {"Debug|Win32", "Debug|x64", "Release|Win32", "Release|x64"})
        defaults = project.find("m:ItemDefinitionGroup/m:ClCompile", NS)
        self.assertEqual(defaults.findtext("m:PrecompiledHeader", namespaces=NS), "Use")
        self.assertIn("$(IntDir)", defaults.findtext("m:PrecompiledHeaderOutputFile", namespaces=NS))
        for kind in ("ClCompile", "ClInclude"):
            items = [x for x in project.findall(f".//m:{kind}", NS) if x.get("Include")]
            paths = [x.get("Include") for x in items]
            self.assertEqual(len(paths), len(set(paths)))
            self.assertTrue(all((ROOT / name.replace("\\", "/")).is_file() for name in paths))
            linked = [x.get("Include") for x in filters.findall(f".//m:{kind}", NS)]
            self.assertEqual(set(paths), set(linked))
        self.assertEqual(project.findtext("m:PropertyGroup/m:RunCodeAnalysis", namespaces=NS), "$(NativeAnalyze)")


    def test_store_targets_compile_only_their_original_standalone_sources(self):
        project = ET.parse(ROOT / "NativeTests.vcxproj").getroot()
        shared = {"GameData\\Runtime\\GameData\\pch.cpp", "GameData\\Runtime\\GameData\\FGameDataPackage.cpp", "GameData\\Runtime\\GameData\\FGeneratedYaml.cpp"}
        expected = {
            "GameDataStore": shared | {"Tests\\SceneUiRuntime\\GameDataStore.cpp", "GameData\\Runtime\\GameData\\FGameDataStore.cpp"},
            "GameDataStoreExceptions": shared | {"Tests\\SceneUiRuntime\\GameDataStoreExceptions.cpp"},
        }
        for suite, paths in expected.items():
            with self.subTest(suite=suite):
                active = {}
                for group in project.findall("m:ItemGroup", NS):
                    for item in group.findall("m:ClCompile", NS):
                        conditions = (group.get("Condition", ""), item.get("Condition", ""))
                        if all(not condition or suite in re.findall(r"'\$\(NativeTestSuite\)'=='([^']+)'", condition)
                               for condition in conditions):
                            active[item.get("Include")] = item
                self.assertEqual(set(active), paths, "store targets must not compile Input/Scene sources or double-compile the macro-included store")
                for name, item in active.items():
                    self.assertEqual(item.findtext("m:PrecompiledHeader", namespaces=NS),
                                     "Create" if name.endswith("\\pch.cpp") else "Use")
                    self.assertEqual(item.findtext("m:PrecompiledHeaderFile", namespaces=NS), "GameData/pch.h")
                    self.assertEqual(item.findtext("m:PrecompiledHeaderOutputFile", namespaces=NS), "$(IntDir)GameData.pch")
                self.assertEqual(active["GameData\\Runtime\\GameData\\pch.cpp"].findtext("m:ObjectFileName", namespaces=NS),
                                 "$(IntDir)GameDataPch.obj")
        self.assertTrue(any('NativeAnalyze' in e.get('Condition', '') and 'Analysis' in (e.text or '')
                            for e in project.findall('.//m:IntDir', NS)))
        self.assertFalse(project.findall('.//m:ForcedIncludeFiles', NS))
        compiler = project.find("m:ItemDefinitionGroup/m:ClCompile", NS)
        self.assertEqual(compiler.findtext("m:PrecompiledHeaderFile", namespaces=NS), "pch.h")
        self.assertEqual(compiler.findtext("m:PrecompiledHeaderOutputFile", namespaces=NS), "$(IntDir)NativeTests.pch")
        for item in project.findall('.//m:ClCompile', NS):
            name = item.get('Include', '')
            if name.startswith('GameData\\Runtime\\GameData\\'):
                self.assertEqual(item.findtext('m:PrecompiledHeaderFile', namespaces=NS), 'GameData/pch.h')
                self.assertEqual(item.findtext('m:PrecompiledHeaderOutputFile', namespaces=NS), '$(IntDir)GameData.pch')
            elif name == 'TextRPG\\pch.cpp':
                self.assertEqual(item.findtext('m:PrecompiledHeader', namespaces=NS), 'Create')
            elif name in ('Tests\\InputManager\\Main.cpp', 'Tests\\SceneUiRuntime\\Main.cpp',
                          'Tests\\SceneUiRuntime\\OptionalCatalog.cpp', 'TextRPG\\Input\\FInputManager.cpp'):
                self.assertIsNone(item.find('m:PrecompiledHeaderFile', NS))
                self.assertIsNone(item.find('m:PrecompiledHeader', NS))
        levels = compiler.findall("m:WarningLevel", NS)
        for suite in expected:
            self.assertTrue(any(level.text == "Level4" and suite in level.get("Condition", "") for level in levels))
        self.assertIn("NOMINMAX", compiler.findtext("m:PreprocessorDefinitions", namespaces=NS))
        self.assertNotIn("NDEBUG", ET.tostring(compiler, encoding="unicode"))
        self.assertEqual(compiler.findtext("m:TreatWarningAsError", namespaces=NS), "true")


    def _store_runner(self, filename, options=(), *, compile_exit=0, warnings=0, timeout=False, final_print_failure=False):
        import native_build
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            builds = []

            @contextmanager
            def build(suite, architecture, configuration, work, **kwargs):
                builds.append((suite, architecture, configuration, kwargs))
                (work / "compile.log").write_text("fixture compiler failure" if compile_exit else "", encoding="utf-8")
                yield {"compileExit": compile_exit, "warnings": warnings, "buildSeconds": 0.5,
                       "compiledObjects": ["fixture.obj"], "pchRebuilt": False,
                       "analysisEnabled": kwargs.get("analyze", False), "analysisObserved": kwargs.get("analyze", False),
                       "executable": str(root / "not-a-real-executable.exe"), "buildCommand": ["MSBuild.exe"]}, {}

            def print_control(*args, **kwargs):
                if final_print_failure and args and isinstance(args[0], str) and re.search(r'"passed":\s*true', args[0]):
                    raise OSError('synthetic final stream failure after native exit zero')

            executed = subprocess.CompletedProcess([], 0, b"PASS fixture\n", b"")
            with mock.patch.object(native_build, "build_native_test", build), \
                 mock.patch("subprocess.run", side_effect=subprocess.TimeoutExpired("fixture", 60) if timeout else None,
                            return_value=executed) as run, \
                 mock.patch("sys.argv", [filename, "--scratch-root", str(root), "--architecture", "x64", "--label", "run", *options]), \
                 mock.patch("builtins.print", side_effect=print_control), self.assertRaises((SystemExit, OSError)) as raised:
                runpy.run_path(str(ROOT / "Tools/Tests" / filename), run_name="__main__")
            result = json.loads((root / "run/results.json").read_text(encoding="utf-8"))
            code = raised.exception.code if isinstance(raised.exception, SystemExit) else 1
            return code, result, builds, run.call_args_list

    def test_native_exit_zero_then_final_stream_failure_never_publishes_success(self):
        for runner in ('test_gamedata_store_runtime.py', 'test_gamedata_store_exceptions.py'):
            with self.subTest(runner=runner):
                code, result, _, calls = self._store_runner(runner, final_print_failure=True)
                self.assertNotEqual(code, 0)
                self.assertTrue(calls, 'synthetic native exit-zero control must have run')
                self.assertFalse(result['passed'], 'final reporting is inside the invocation success boundary')
                if 'records' in result:
                    self.assertTrue(all(record['passed'] for record in result['records']))
                else:
                    self.assertEqual(result['runExit'], 0)
                    self.assertEqual(result['passCheckpointGroups'], 1)


    def test_store_default_analysis_and_legacy_roots_are_preserved(self):
        code, result, builds, calls = self._store_runner("test_gamedata_store_runtime.py",
            ("--generated-root", str(ROOT / "GameData/Class"), "--data-root", str(ROOT / "GameData")))
        self.assertEqual(code, 0)
        self.assertEqual(builds[0][:3], ("GameDataStore", "x64", "Debug"))
        self.assertTrue(builds[0][3]["analyze"])
        self.assertEqual(builds[0][3]["generated_root"], ROOT / "GameData/Class")
        self.assertEqual(calls[0].args[0][1], str(ROOT / "GameData"))
        self.assertEqual(result["passCheckpointGroups"], 1)
        code, _, builds, _ = self._store_runner("test_gamedata_store_runtime.py", ("--no-analyze", "--configuration", "Release"))
        self.assertEqual(code, 0)
        self.assertFalse(builds[0][3]["analyze"])
        self.assertEqual(builds[0][2], "Release")


    def test_exception_default_runs_both_modes_and_all_fault_states(self):
        code, result, builds, calls = self._store_runner("test_gamedata_store_exceptions.py")
        self.assertEqual(code, 0)
        self.assertEqual([entry[0] for entry in builds], ["GameDataStoreExceptions"] * 2)
        self.assertEqual([entry[3]["analyze"] for entry in builds], [False, True])
        self.assertEqual(result["recordCount"], 10)
        self.assertEqual(len(calls), 8)
        self.assertEqual([call.args[0][-2:] for call in calls],
                         [[str(fault), state] for _ in range(2) for fault in (1, 2) for state in ("empty", "populated")])
        for record in result["records"]:
            if record["name"].endswith("-compile"):
                self.assertIn("compiledObjects", record)
                self.assertIn("analysisObserved", record)
        code, result, builds, calls = self._store_runner("test_gamedata_store_exceptions.py",
            ("--only", "bad-alloc-empty", "--mode", "normal", "--configuration", "Release"))
        self.assertEqual(code, 0)
        self.assertEqual(result["recordCount"], 2)
        self.assertEqual(builds[0][2], "Release")
        self.assertEqual(calls[0].args[0][-2:], ["1", "empty"])


    def test_store_build_failure_or_warning_never_runs_a_stale_executable(self):
        for runner in ("test_gamedata_store_runtime.py", "test_gamedata_store_exceptions.py"):
            for compile_exit, warnings in ((1, 0), (124, 0), (0, 1)):
                with self.subTest(runner=runner, compileExit=compile_exit, warnings=warnings):
                    code, result, _, calls = self._store_runner(runner, compile_exit=compile_exit, warnings=warnings)
                    self.assertNotEqual(code, 0)
                    self.assertFalse(calls)
                    self.assertFalse(result["passed"])


    def test_store_run_timeouts_preserve_failed_result_evidence(self):
        for runner in ("test_gamedata_store_runtime.py", "test_gamedata_store_exceptions.py"):
            with self.subTest(runner=runner):
                code, result, _, _ = self._store_runner(runner, timeout=True)
                self.assertNotEqual(code, 0)
                self.assertFalse(result["passed"])
                if "records" in result:
                    self.assertTrue(all(record["exitCode"] == 124 for record in result["records"]
                                        if not record["name"].endswith("-compile")))
                else:
                    self.assertEqual(result["runExit"], 124)


    def test_native_report_artifact_faults_preserve_prior_raw_bytes(self):
        from native_build import native_report, write_native_report
        for fault in ('truncate', 'partial-write', 'short-write', 'pending-flush', 'close-after-completed-close', 'replace'):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as directory:
                work = Path(directory)
                path = work / 'results.json'
                unowned = work / '.results.json.unowned.pending'
                unowned.write_bytes(b'unowned pending must survive')
                report = {'passed': False, 'evidenceKind': 'synthetic-publication-control'}
                trace = {'fault': fault, 'faultHits': 0, 'pendingRawHex': None, 'closeCompleted': False}
                original_open, original_replace = Path.open, Path.replace
                opens = 0

                def open_control(target, mode='r', *args, **kwargs):
                    nonlocal opens
                    stream = original_open(target, mode, *args, **kwargs)
                    if mode == 'x' and target.name.startswith('.results.json.'):
                        opens += 1
                        if opens == 2:
                            return _ReportFaultWriter(stream, target, fault, trace)
                    return stream

                def replace_control(target, destination):
                    if fault == 'replace' and opens == 2 and destination == path:
                        trace.update(faultHits=trace['faultHits'] + 1, pendingRawHex=target.read_bytes().hex())
                        raise OSError('actual replace boundary fault')
                    return original_replace(target, destination)

                with mock.patch.object(Path, 'open', open_control), mock.patch.object(Path, 'replace', replace_control), \
                     mock.patch('sys.stdout', io.StringIO()), mock.patch('sys.stderr', io.StringIO()), self.assertRaises(OSError):
                    with native_report(work, report, lambda: {'passed': report['passed']}):
                        prior = path.read_bytes()
                        report.update(passed=True, records=[{'name': 'synthetic-observation', 'exitCode': 0,
                                                           'passCheckpointGroups': 3, 'passed': True}], recordCount=1)
                self.assertEqual(trace['faultHits'], 1)
                self.assertEqual(report['error']['stage'], 'final-publication')
                self.assertFalse(report['passed'])
                self.assertEqual(path.read_bytes(), prior)
                self.assertEqual(json.loads(prior)['status'], 'incomplete')
                self.assertNotIn('recordCount', json.loads(prior), 'initial publication must not invent future counts')
                self.assertEqual(list(work.glob('.*.pending')), [unowned])
                if fault in ('partial-write', 'short-write'):
                    self.assertEqual(len(bytes.fromhex(trace['pendingRawHex'])), 17)
                if fault == 'truncate':
                    self.assertEqual(trace['pendingRawHex'], '')
                if fault in ('replace', 'close-after-completed-close'):
                    self.assertTrue(trace['closeCompleted'])
                    self.assertTrue(json.loads(bytes.fromhex(trace['pendingRawHex']))['passed'])
                trace.update(stage=report['error']['stage'], priorRawHex=prior.hex(), canonicalRawHex=path.read_bytes().hex(),
                             ownedPendingRemaining=False, unownedPendingPreserved=True)
                print('CONTROL artifact ' + json.dumps(trace))
                # 기존 canonical이 임의 바이트여도 실패한 게시가 이를 잘라내지 않는다.
                raw = b'\xffprior canonical bytes\r\n'
                path.write_bytes(raw)
                with mock.patch.object(Path, 'replace', side_effect=OSError('replace refused')), self.assertRaises(OSError):
                    write_native_report(path, {'passed': True})
                self.assertEqual(path.read_bytes(), raw)
                self.assertEqual(list(work.glob('.*.pending')), [unowned])
                # 실행 실패 보고서도 같은 pending 경계를 쓰고, 게시 실패가 원래 예외를 바꾸지 않는다.
                opens = 0
                trace.update(faultHits=0, pendingRawHex=None, closeCompleted=False)
                failure = ValueError('synthetic execution failure before failure publication')
                with mock.patch.object(Path, 'open', open_control), mock.patch.object(Path, 'replace', replace_control), \
                     mock.patch('sys.stdout', io.StringIO()), mock.patch('sys.stderr', io.StringIO()), self.assertRaises(ValueError) as raised:
                    with native_report(work, report, lambda: report):
                        prior = path.read_bytes()
                        raise failure
                self.assertIs(raised.exception, failure)
                self.assertEqual(trace['faultHits'], 1)
                self.assertEqual(report['error']['stage'], 'execution')
                self.assertEqual(path.read_bytes(), prior)
                self.assertFalse(json.loads(prior)['passed'])
                self.assertEqual(json.loads(prior)['status'], 'incomplete')
                self.assertEqual(list(work.glob('.*.pending')), [unowned])
                self.assertIn('failure report publication: OSError', failure.__notes__)
                print('CONTROL failure-artifact ' + json.dumps({'fault': fault, 'faultHits': trace['faultHits'],
                    'evidenceKind': 'synthetic-publication-control-real-temp-io', 'stage': report['error']['stage'],
                    'previousBytesPreserved': True, 'originalExceptionPreserved': True}))

    def test_native_publication_cli_and_import_controls_normal_and_optimized(self):
        for optimized in (False, True):
            for runner in ('test_gamedata_store_runtime.py', 'test_gamedata_store_exceptions.py'):
                for fault in ('success', 'body-write', 'final-write', 'final-flush', 'stderr-flush', 'build-entry'):
                    for imported in (False, True):
                        with self.subTest(optimized=optimized, runner=runner, fault=fault, imported=imported), \
                             tempfile.TemporaryDirectory() as directory:
                            command = [sys.executable, *(['-O'] if optimized else []), str(Path(__file__).resolve()),
                                       '--native-publication-worker', runner, fault, directory, str(int(imported))]
                            executed = subprocess.run(command, capture_output=True, timeout=30)
                            trace = json.loads((Path(directory) / 'trace.json').read_text(encoding='utf-8'))
                            result = json.loads((Path(directory) / 'run/results.json').read_text(encoding='utf-8'))
                            self.assertEqual(executed.returncode, 0 if fault == 'success' else 1, executed.stderr)
                            self.assertEqual(result['passed'], fault == 'success')
                            self.assertFalse(trace['pendingRemaining'])
                            if fault == 'success':
                                self.assertEqual(result['status'], 'complete')
                                self.assertEqual(trace['faultHits'], 0)
                            else:
                                self.assertGreater(trace['faultHits'], 0)
                                self.assertEqual(trace['exceptionType'], 'OSError' if imported else 'SystemExit')
                                stage = {'body-write': 'execution', 'final-write': 'final-output', 'final-flush': 'final-flush',
                                         'stderr-flush': 'final-flush', 'build-entry': 'execution'}[fault]
                                self.assertEqual(result['error']['stage'], stage)
                                if fault == 'build-entry':
                                    self.assertEqual(trace['syntheticNativeCalls'], 0)
                                    self.assertNotIn('passCheckpointGroups', result)
                                    self.assertNotIn('runExit', result)
                                else:
                                    self.assertGreater(trace['successfulStreamWrites'], 0)
                                    self.assertGreater(trace['syntheticNativeCalls'], 0)
                                    if 'records' in result:
                                        self.assertEqual(result['recordCount'], len(result['records']))
                                        self.assertTrue(all(record['passed'] for record in result['records']))
                                    else:
                                        self.assertEqual(result['runExit'], 0)
                                        self.assertEqual(result['passCheckpointGroups'], 1)
                            trace.update(optimized=optimized, runner=runner, imported=imported,
                                         stage=result.get('error', {}).get('stage', 'complete'))
                            print('CONTROL entry ' + json.dumps(trace))

    def test_scene_invocation_finalization_controls_normal_and_optimized(self):
        for optimized in (False, True):
            for mode in ('core', 'negative'):
                for fault in ('success', 'final-write', 'final-flush', 'stderr-flush', 'unlock'):
                    for imported in (False, True):
                        with self.subTest(optimized=optimized, mode=mode, fault=fault, imported=imported), \
                             tempfile.TemporaryDirectory() as directory:
                            command = [sys.executable, *(['-O'] if optimized else []), str(Path(__file__).resolve()),
                                       '--native-publication-worker', 'test_scene_ui_runtime.py', fault, directory,
                                       str(int(imported)), mode]
                            executed = subprocess.run(command, capture_output=True, timeout=30)
                            trace = json.loads((Path(directory) / 'trace.json').read_text(encoding='utf-8'))
                            result = json.loads((Path(directory) / 'run/results.json').read_text(encoding='utf-8'))
                            print('CONTROL scene ' + json.dumps(dict(
                                {k: v for k, v in trace.items() if k not in ('canonicalRawHex', 'priorRawHex')},
                                mode=mode, optimized=optimized, imported=imported,
                                canonical={k: v for k, v in result.items() if k != 'sourceHashes'})))
                            self.assertEqual(executed.returncode, 0 if fault == 'success' else 1, executed.stderr)
                            self.assertEqual(result.get('passed'), fault == 'success')
                            self.assertEqual(result['status'], 'complete' if fault == 'success' else 'failed')
                            self.assertFalse(trace['pendingRemaining'])
                            self.assertEqual(trace['syntheticNativeCalls'], 1)
                            self.assertGreater(trace.get('buildContextExits', 0), 0)
                            if mode == 'negative':
                                self.assertTrue(result['dependencyVerdict']['passed'])
                                self.assertEqual(result['dependencyVerdict']['scope'], 'stage-observation')
                                self.assertEqual(result['expectedDependencyRejectionScope'], 'stage-observation')
                                self.assertEqual(result['positiveControl']['scope'], 'stage-observation')
                            else:
                                self.assertEqual(result['runExit'], 0)
                                self.assertEqual(result['passCheckpointGroups'], 1)
                            if fault != 'success':
                                self.assertTrue(_scene_finalization_fault_matches(trace, result, fault))
                                self.assertFalse(_scene_finalization_fault_matches(dict(trace, faultHits=0), result, fault))
                                self.assertFalse(_scene_finalization_fault_matches(dict(trace, faultReason='unrelated'), result, fault))
                                self.assertFalse(_scene_finalization_fault_matches(trace, dict(result, error={'type': 'OSError', 'stage': 'unrelated'}), fault))
                                self.assertEqual(trace['exceptionType'], 'OSError' if imported else 'SystemExit')
                                if imported:
                                    self.assertTrue(trace['originalExceptionPreserved'])
                                self.assertEqual(result['error']['stage'], {'final-write': 'final-output',
                                    'final-flush': 'final-flush', 'stderr-flush': 'final-flush', 'unlock': 'execution'}[fault])
                                self.assertNotIn({'passed': True, 'status': 'complete'}, trace['rootPublications'])
                            else:
                                events = trace['events']
                                self.assertEqual(events[-1], 'replace-complete', 'no stream operation after success commit')
                                self.assertLess(events.index('unlock-run'), events.index('stdout-flush'))
                                self.assertLess(events.index('stdout-flush'), events.index('stderr-flush'))
                                self.assertLess(events.index('stderr-flush'), events.index('replace-complete'))

    def test_scene_all_mode_families_share_invocation_boundary(self):
        for optimized in (False, True):
            for mode in ('default', 'optional', 'progress', 'boundary', 'root-entry', 'legacy', 'generated', 'analysis'):
                for fault in ('success', 'final-write'):
                    with self.subTest(optimized=optimized, mode=mode, fault=fault), tempfile.TemporaryDirectory() as directory:
                        command = [sys.executable, *(['-O'] if optimized else []), str(Path(__file__).resolve()),
                                   '--native-publication-worker', 'test_scene_ui_runtime.py', fault, directory, '0', mode]
                        executed = subprocess.run(command, capture_output=True, timeout=30)
                        trace = json.loads((Path(directory) / 'trace.json').read_text(encoding='utf-8'))
                        result = json.loads((Path(directory) / 'run/results.json').read_text(encoding='utf-8'))
                        self.assertEqual(executed.returncode, 0 if fault == 'success' else 1, executed.stderr)
                        self.assertEqual(result['passed'], fault == 'success')
                        self.assertEqual(result['runExit'], 0)
                        self.assertEqual(result['passCheckpointGroups'], 1)
                        self.assertEqual(result['suite'], 'SceneUiRuntime')
                        self.assertFalse(trace['pendingRemaining'])
                        if fault == 'final-write':
                            self.assertTrue(_scene_finalization_fault_matches(trace, result, fault))
                        else:
                            self.assertEqual(trace['events'][-1], 'replace-complete')
                        print('CONTROL mode ' + json.dumps({'mode': mode, 'optimized': optimized,
                            'evidenceKind': trace['evidenceKind'], 'fault': fault, 'exitCode': executed.returncode,
                            'passed': result['passed'], 'status': result['status']}))

    def test_scene_artifact_faults_use_real_owned_pending_io(self):
        for optimized in (False, True):
            for mode in ('core', 'negative'):
                for phase in ('initial', 'final'):
                    for fault in ('truncate', 'partial-write', 'short-write', 'pending-flush', 'close-after-completed-close', 'replace'):
                        with self.subTest(optimized=optimized, mode=mode, phase=phase, fault=fault), \
                             tempfile.TemporaryDirectory() as directory:
                            command = [sys.executable, *(['-O'] if optimized else []), str(Path(__file__).resolve()),
                                       '--native-publication-worker', 'test_scene_ui_runtime.py', phase + ':' + fault,
                                       directory, '0', mode]
                            executed = subprocess.run(command, capture_output=True, timeout=30)
                            trace = json.loads((Path(directory) / 'trace.json').read_text(encoding='utf-8'))
                            self.assertEqual(executed.returncode, 1, executed.stderr)
                            self.assertEqual(trace['exceptionType'], 'SystemExit')
                            self.assertEqual(trace['faultHits'], 1)
                            self.assertTrue(trace['closeCompleted'])
                            self.assertFalse(trace['pendingRemaining'])
                            self.assertEqual(trace['canonicalRawHex'], trace['priorRawHex'])
                            self.assertEqual(trace['syntheticNativeCalls'], 0 if phase == 'initial' else 1)
                            if phase == 'initial':
                                self.assertIsNone(trace['canonicalRawHex'])
                            else:
                                self.assertEqual(json.loads(bytes.fromhex(trace['canonicalRawHex']))['status'], 'incomplete')
                                self.assertFalse(json.loads(bytes.fromhex(trace['canonicalRawHex']))['passed'])
                            pending = bytes.fromhex(trace['pendingRawHex'])
                            if fault in ('partial-write', 'short-write'):
                                self.assertEqual(len(pending), 17, 'positive real prefix write, not pre-write failure')
                            elif fault == 'truncate':
                                self.assertEqual(pending, b'')
                            else:
                                self.assertEqual(json.loads(pending)['passed'], phase == 'final')
                            print('CONTROL scene-artifact ' + json.dumps({'evidenceKind': trace['evidenceKind'],
                                'mode': mode, 'optimized': optimized, 'phase': phase, 'fault': fault,
                                'faultHits': trace['faultHits'], 'pendingBytes': len(pending),
                                'previousBytesPreserved': True, 'exitCode': executed.returncode}))

    def test_scene_execution_failures_keep_failed_canonical(self):
        for optimized in (False, True):
            for fault, mode, calls in (('fingerprint', 'core', 0), ('build-entry', 'core', 0),
                    ('compile-log', 'core', 0), ('runtime-log', 'core', 1), ('drift', 'core', 1),
                    ('positive-unlock', 'negative', 1)):
                for imported in (False, True):
                    with self.subTest(optimized=optimized, fault=fault, imported=imported), tempfile.TemporaryDirectory() as directory:
                        command = [sys.executable, *(['-O'] if optimized else []), str(Path(__file__).resolve()),
                                   '--native-publication-worker', 'test_scene_ui_runtime.py', fault, directory, str(int(imported)), mode]
                        executed = subprocess.run(command, capture_output=True, timeout=30)
                        trace = json.loads((Path(directory) / 'trace.json').read_text(encoding='utf-8'))
                        result = json.loads((Path(directory) / 'run/results.json').read_text(encoding='utf-8'))
                        self.assertEqual(executed.returncode, 1, executed.stderr)
                        self.assertEqual(trace['syntheticNativeCalls'], calls)
                        self.assertEqual(trace['faultHits'], 1)
                        self.assertEqual(trace['faultReason'], fault)
                        self.assertFalse(result['passed'])
                        self.assertEqual(result['status'], 'failed')
                        self.assertEqual(result['error'], {'type': 'OSError', 'stage': 'execution'})
                        self.assertFalse(trace['pendingRemaining'])
                        if imported:
                            self.assertTrue(trace['originalExceptionPreserved'])
                        print('CONTROL scene-execution ' + json.dumps({'evidenceKind': trace['evidenceKind'],
                            'fault': fault, 'optimized': optimized, 'imported': imported, 'exitCode': executed.returncode,
                            'faultHits': trace['faultHits'], 'syntheticNativeCalls': calls, 'status': result['status']}))

    def test_log_labels_cannot_escape_the_requested_directory(self):
        from native_build import run_directory
        for label in ("../escape", "nested/path", "nested\\path", "..", "trailing."):
            with self.subTest(label=label), self.assertRaises(ValueError):
                run_directory(ROOT, label)


    def _core_negative_runner(self, *, compile_exit=1, warnings=0, log=None, positive_exit=0):
        import native_build
        source = ROOT / 'Tests/ConsoleGameCore/Main.cpp'
        diagnostic = (f"{source}(3,10): error C1083: Cannot open include file: "
                      f"'Common/GameDataEnums.g.h': No such file or directory [{ROOT / 'NativeTests.vcxproj'}]\n")
        if log is None:
            log = 'ClCompile:\n  CL.exe /c /D CORE_DEPENDENCY_NEGATIVE Tests\\ConsoleGameCore\\Main.cpp\n' + diagnostic
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            @contextmanager
            def build(suite, architecture, configuration, work, **kwargs):
                negative = kwargs.get('core_dependency_negative', False)
                (work / 'compile.log').write_text(log if negative else '', encoding='utf-8')
                yield {'compileExit': compile_exit if negative else positive_exit, 'warnings': warnings if negative else 0,
                       'suite': suite, 'coreDependencyNegative': negative, 'executable': 'format-control-not-native.exe'}, {}

            with mock.patch.object(native_build, 'build_native_test', build), \
                 mock.patch('subprocess.run', return_value=subprocess.CompletedProcess([], 0, b'PASS synthetic standalone control\n', b'')) as run, \
                 mock.patch('sys.argv', ['test_scene_ui_runtime.py', '--scratch-root', str(root), '--label', 'run', '--core-dependency-negative']), \
                 mock.patch('builtins.print'), self.assertRaises(SystemExit) as raised:
                runpy.run_path(str(ROOT / 'Tools/Tests/test_scene_ui_runtime.py'), run_name='__main__')
            result = json.loads((root / 'run/results.json').read_text(encoding='utf-8'))
            return raised.exception.code, result, run.call_count

    def test_dependency_negative_rejects_timeout_after_expected_diagnostic(self):
        code, result, _ = self._core_negative_runner(compile_exit=124)
        self.assertNotEqual(code, 0, 'timeout is incomplete even after the expected C1083')
        self.assertFalse(result['expectedDependencyRejection'])

    def test_dependency_negative_positive_and_false_positive_controls(self):
        source = str(ROOT / 'Tests/ConsoleGameCore/Main.cpp')
        log = ('ClCompile:\n  CL.exe /c /D CORE_DEPENDENCY_NEGATIVE Tests\\ConsoleGameCore\\Main.cpp\n'
               f"{source}(3,10): fatal error C1083: Cannot open include file: 'Common/GameDataEnums.g.h': missing [{ROOT / 'NativeTests.vcxproj'}]\n")
        code, result, calls = self._core_negative_runner(log=log)
        self.assertEqual(code, 0)
        self.assertTrue(result['expectedDependencyRejection'])
        self.assertTrue(result['positiveControl']['passCheckpointGroups'])
        self.assertEqual(calls, 1, 'only the normal standalone control may execute; no negative stale EXE')
        print('CONTROL dependency ' + json.dumps({'name': 'expected-rejection', 'evidenceKind': 'synthetic-diagnostic-format-control',
                                                'exitCode': code, 'verdict': result['dependencyVerdict']}))
        controls = {
            'timeout-after-diagnostic': {'compile_exit': 124},
            'unexpected-exit': {'compile_exit': 2},
            'unhit-include': {'compile_exit': 0, 'log': ''},
            'warning-counter': {'warnings': 1},
            'warning-log': {'log': log + 'warning C4100: unused parameter\n'},
            'additional-error': {'log': log + 'other.cpp(1): error C2000: unrelated failure\n'},
            'additional-uncoded-error': {'log': log + 'MSBUILD : error : unrelated failure\n'},
            'late-wrong-stage': {'log': log + 'Link:\n' + log.split('\n')[-2] + '\n'},
            'wrong-source': {'log': log.replace(source, str(ROOT / 'ConsoleGameCore/pch.cpp'))},
            'wrong-line': {'log': log.replace('(3,10)', '(1,10)')},
            'wrong-header': {'log': log.replace('Common/GameDataEnums.g.h', 'pch.h')},
            'wrong-project': {'log': log.replace(str(ROOT / 'NativeTests.vcxproj'), str(ROOT / 'ConsoleGameCore/ConsoleGameCore.vcxproj'))},
            'wrong-stage': {'log': log.replace('ClCompile:', 'Link:')},
            'unhit-injection': {'log': log.replace('/D CORE_DEPENDENCY_NEGATIVE', '')},
            'missing-diagnostic': {'log': log.split(source)[0]},
            'failed-positive-control': {'positive_exit': 1},
        }
        for name, options in controls.items():
            with self.subTest(control=name):
                code, result, calls = self._core_negative_runner(**dict({'log': log}, **options))
                self.assertNotEqual(code, 0)
                self.assertFalse(result['expectedDependencyRejection'])
                self.assertTrue(result['dependencyVerdict']['reasons'])
                self.assertEqual(calls, 0 if options.get('positive_exit') else 1)
                print('CONTROL dependency ' + json.dumps({'name': name, 'evidenceKind': 'synthetic-diagnostic-format-control',
                                                        'exitCode': code, 'verdict': result['dependencyVerdict']}))

    def test_core_consumer_has_closed_include_and_link_inputs(self):
        project = ET.parse(ROOT / "NativeTests.vcxproj").getroot()
        core_groups = [g for g in project.findall("m:ItemDefinitionGroup", NS)
                       if g.get("Condition") == "'$(NativeTestSuite)'=='ConsoleGameCore'"]
        self.assertEqual(len(core_groups), 1)
        self.assertEqual(core_groups[0].findtext("m:ClCompile/m:AdditionalIncludeDirectories", namespaces=NS),
                         "$(MSBuildThisFileDirectory)ConsoleGameCore")
        references = [i for i in project.findall(".//m:ProjectReference", NS)]
        for item in references:
            removed = set(item.findtext("m:GlobalPropertiesToRemove", namespaces=NS).split(';'))
            self.assertTrue({'OutDir', 'IntDir', 'TargetName', 'NativeGeneratedRoot', 'NativeTestSuite'} <= removed)
        data_group = next(g for g in project.findall("m:ItemGroup", NS)
                          if any(i.get('Include') == 'GameData\\GameData.vcxproj' for i in g))
        self.assertEqual(data_group.get('Condition'), "'$(NativeUseGameDataLibrary)'=='true'")
        backend = (ROOT / 'Tools/Tests/native_build.py').read_text(encoding='utf-8-sig')
        self.assertIn('not without_textart and generated == (ROOT / "GameData/Class").resolve()', backend)
        for item in project.findall('.//m:ClCompile', NS):
            name = item.get('Include', '')
            if name.startswith('GameData\\Runtime\\GameData\\') and not name.endswith('pch.cpp'):
                exclusion = item.find("m:ExcludedFromBuild", NS)
                self.assertIsNotNone(exclusion)
                self.assertEqual(exclusion.get('Condition'), "'$(NativeUseGameDataLibrary)'=='true'")
                self.assertEqual(exclusion.text, 'true')

    def test_input_manager_is_compiled_once_by_linked_core(self):
        project = ET.parse(ROOT / "ConsoleGameCore/ConsoleGameCore.vcxproj").getroot()
        paths = [item.get("Include") for item in project.findall(".//m:ClCompile", NS)]
        self.assertEqual(paths.count("Input\\FInputManager.cpp"), 1,
                         "the linked core must compile the real input state manager exactly once")


        game = ET.parse(ROOT / "TextRPG/TextRPG.vcxproj").getroot()
        self.assertFalse(any(item.get("Include") == "Input\\FInputManager.cpp" for item in game.findall(".//m:ClCompile", NS)))
        reference = next(item for item in game.findall(".//m:ProjectReference", NS)
                         if item.get("Include", "").endswith("ConsoleGameCore.vcxproj"))
        self.assertEqual(reference.findtext("m:LinkLibraryDependencies", namespaces=NS), "true")

    def test_input_items_have_matching_physical_input_filters(self):
        project = ET.parse(ROOT / "ConsoleGameCore/ConsoleGameCore.vcxproj").getroot()
        filters = ET.parse(ROOT / "ConsoleGameCore/ConsoleGameCore.vcxproj.filters").getroot()
        for kind, relative in (("ClCompile", "Input\\FInputManager.cpp"),
                               ("ClInclude", "Input\\FInputManager.h"),
                               ("ClInclude", "Input\\InputTypes.h")):
            with self.subTest(path=relative):
                entries = [item for item in project.findall(f".//m:{kind}", NS)
                           if item.get("Include") == relative]
                self.assertEqual(len(entries), 1, "project registration must be unique")
                entries = [item for item in filters.findall(f".//m:{kind}", NS)
                           if item.get("Include") == relative]
                self.assertEqual(len(entries), 1, "input files must appear in the physical Input filter")
                self.assertEqual(entries[0].findtext("m:Filter", namespaces=NS), "Input")


    def test_main_collects_one_input_frame_before_scene_dispatch(self):
        text = (ROOT / "TextRPG/Main.cpp").read_text(encoding="utf-8-sig")
        self.assertIn('FInputManager inputManager(consoleInput)', text)
        self.assertNotIn('consoleInput.ReadEvent(', text)
        self.assertEqual(text.count('inputManager.Update()'), 1)
        dispatch = text.index('sceneManager.HandleInputFrame(inputManager.GetEvents())')
        terminal = text.index('EInputReadResult::Error', text.index('inputManager.Update()'))
        self.assertLess(terminal, dispatch, "failed partial frames must not reach UI")
        self.assertIn('inputResult == TextRPG::EInputReadResult::Event', text[terminal:dispatch])
        self.assertNotIn('inputManager.Reset()', text)
        update = text.index('sceneManager.Update(deltaSeconds)', dispatch)
        render = text.index('sceneManager.Render(renderer.GetBackBuffer())', update)
        present = text.index('renderer.Present()', render)
        end_frame = text.index('sceneManager.EndFrame(errorMessage)', present)
        result = text.index('session.TakeStartResult(sceneManager, dataError)', end_frame)
        self.assertLess(dispatch, update)
        self.assertLess(update, render)
        self.assertLess(render, present)
        self.assertLess(present, end_frame, "Scene ownership changes only after Render/Present")
        self.assertLess(end_frame, result, "queued progress is not reported Started or saved before publication")
        self.assertIn('!sceneManager.LoadStartupScene(uiManager, {}, errorMessage) || !sceneManager.EndFrame(errorMessage)', text)
        quit_branch = text[text.index('if (!isRunning)'):update]
        self.assertLess(quit_branch.index('CancelPendingTransitions()'), quit_branch.index('session.Save(dataError)'))


    def test_main_honors_callback_shutdown_after_endframe_without_autosave(self):
        text = (ROOT / "TextRPG/Main.cpp").read_text(encoding="utf-8-sig")
        end_frame = text.index('const bool published = sceneManager.EndFrame(errorMessage)')
        result = text.index('session.TakeStartResult(sceneManager, dataError)', end_frame)
        shutdown = text.find('if (sceneManager.IsQuitting())', result)
        self.assertGreater(shutdown, result, 'QUIT-01 Main must observe destructor shutdown after the publication result')
        branch_end = text.index('}', shutdown)
        branch = text[shutdown:branch_end]
        self.assertIn('isRunning = false;', branch, 'late destructor Quit stops the outer app loop')
        self.assertIn('return;', branch, 'late Quit returns from the EndFrame hook before further policy')
        self.assertIn('if (!frameCompleted || !isRunning) break;', text[branch_end:], 'hook shutdown must leave the real loop')
        self.assertNotIn('session.Save(', branch, 'a canceled publication must not cause another save write')
        self.assertLess(branch_end, text.index('std::this_thread::sleep_for', end_frame))


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == '--native-publication-worker':
        raise SystemExit(_native_publication_worker(*sys.argv[2:5], imported=sys.argv[5] == '1',
                                                   mode=sys.argv[6] if len(sys.argv) > 6 else 'default'))
    unittest.main(verbosity=2)
