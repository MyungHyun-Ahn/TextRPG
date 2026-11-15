"""Reject setup failures that previously masqueraded as intended cleanup faults."""
import argparse
from contextlib import ExitStack, nullcontext, redirect_stderr, redirect_stdout
import ast
import ctypes as c
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
from unittest.mock import patch

TARGET = Path(__file__).with_name('test_game_data_application_cleanup.py')
CONTROLS = [('attach-unavailable', 'first-handle'),
            ('attach-unavailable', 'second-handle'),
            ('first-open-unavailable', 'second-handle')]
ACCEPTANCE_CONTROLS = ('later-failure', 'missing-fixture', 'nonexistent-fixture', 'fixture-directory',
                       'missing-required-case', 'failed-required-case', 'all-required-success',
                       'newgame-only-success', 'initial-console-failure', 'partial-progress-failure',
                       'report-update-success')


class Function:
    def __init__(self, callback):
        self.callback = callback

    def __call__(self, *args):
        return self.callback(*args)


def controlled_worker(work, control, case):
    spec = importlib.util.spec_from_file_location('cleanup_under_test', TARGET)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    real_kernel = c.WinDLL('kernel32', use_last_error=True)
    real_kernel.CreateFileW.argtypes = [c.c_wchar_p, c.c_ulong, c.c_ulong, c.c_void_p,
                                      c.c_ulong, c.c_ulong, c.c_void_p]
    real_kernel.CreateFileW.restype = c.c_void_p
    trace = {'control': control, 'case': case, 'attachPids': [], 'attachSuccess': 0,
             'createNames': [], 'acquired': 0, 'boundaryFaultHit': 0}

    def attach(pid):
        trace['attachPids'].append(pid)
        if control == 'attach-unavailable':
            trace['boundaryFaultHit'] += 1
            c.set_last_error(123)
            return 0
        result = real_kernel.AttachConsole(pid)
        trace['attachSuccess'] += int(bool(result))
        return result

    def create(*args):
        trace['createNames'].append(args[0])
        if control == 'first-open-unavailable' and len(trace['createNames']) == 1:
            trace['boundaryFaultHit'] += 1
            c.set_last_error(123)
            return c.c_void_p(-1).value
        handle = real_kernel.CreateFileW(*args)
        trace['acquired'] += int(handle not in (None, 0, c.c_void_p(-1).value))
        return handle

    class Kernel:
        AttachConsole = Function(attach)
        CreateFileW = Function(create)

        def __getattr__(self, name):
            return getattr(real_kernel, name)

    try:
        # 실제 작업자를 별도 무콘솔 프로세스에서 실행하고 Win32 경계만 바꾼다.
        with patch.object(module.c, 'WinDLL', return_value=Kernel()):
            return module.worker(work, case)
    finally:
        (work / 'boundary.json').write_text(json.dumps(trace, indent=2), encoding='utf-8')


def verify_rejection(record, trace, exit_code, control):
    # nonzero만으로 통과시키지 않고 실제 설정 실패·소유권·회수를 먼저 확인한다.
    assert len(record['ownedPids']) == 1, 'control did not launch one owned child'
    assert trace['attachPids'] and set(trace['attachPids']) == set(record['ownedPids']), 'non-owned attach'
    assert record['reapedBySmoke'] and record['allReapedAfterExternalSafetyNet'], 'control child not reaped by smoke'
    assert not record['externalCleanupNeeded'], 'control needed external process cleanup'
    assert record['openedHandles'] == record['closedBySmoke'] == trace['acquired'] == 0
    assert record['calls']['key'] == 0 and record['calls']['free'] == 2
    if control == 'attach-unavailable':
        assert trace['boundaryFaultHit'] == len(trace['attachPids']) == record['calls']['attach'] == 60
        assert trace['attachSuccess'] == 0 and trace['createNames'] == []
        assert record['calls']['create'] == 0
        label = 'owned console attach'
    else:
        assert trace['boundaryFaultHit'] == trace['attachSuccess'] == 1
        assert trace['createNames'] == ['CONOUT$'] and record['calls']['create'] == 1
        label = 'owned CONOUT$'
    assert record['injectedError'] == f'OSError: {OSError(123, label)}', 'wrong controlled setup error'
    assert exit_code != 0 and record['passed'] is False, 'worker falsely accepted unrelated setup failure'
    assert record['failure'] == 'intended fault was not reached', 'unrelated worker failure is not proof of rejection'
    assert record['faultHit'] == 0, 'intended injection unexpectedly reached'
    assert record['errorIdentity'] == {'type': 'OSError', 'errno': 123, 'label': label}


def observer_activity_control(work, root_name, watch_factory=None):
    """Real child transient writes must fail even with identical final inventory."""
    if watch_factory is None:
        from application_observers import FileActivityWatch
        watch_factory = FileActivityWatch
    roots = [work / name for name in ('app', 'cwd', 'tmp')]
    for root in roots:
        root.mkdir(parents=True)
    target = roots[('app', 'cwd', 'tmp').index(root_name)] / 'nested/deeper/extracted.yaml'
    before = [sorted(str(p.relative_to(root)) for p in root.rglob('*')) for root in roots]
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([os.environ.get('SystemRoot', r'C:\Windows') + r'\System32',
                                  os.environ.get('SystemRoot', r'C:\Windows'), r'C:\Program Files\dotnet'])
    for key in ('TMP', 'TEMP', 'TMPDIR'):
        env[key] = str(roots[2])
    command = [sys.executable, '-I', '-B', '-c',
               'from pathlib import Path; import sys; p=Path(sys.argv[1]); '
               'p.parent.mkdir(parents=True); '
               'p.write_bytes(b"transient-owned-control"); '
               'assert p.read_bytes()==b"transient-owned-control"; p.unlink(); '
               'p.parent.rmdir(); p.parent.parent.rmdir()', str(target)]
    with watch_factory(roots) as watch:
        armed_before_spawn = watch.report()['ready']
        assert armed_before_spawn, 'observer was not armed before child spawn'
        child = subprocess.run(command, cwd=roots[1], env=env, capture_output=True, timeout=10,
                               creationflags=subprocess.CREATE_NO_WINDOW)
        assert child.returncode == 0, f'transient child failed: {child.stderr!r}'
    after = [sorted(str(p.relative_to(root)) for p in root.rglob('*')) for root in roots]
    assert before == after, 'transient control did not leave identical final inventory'
    assert all(not paths for paths in after), 'transient control final roots were not empty'
    rejected = None
    try:
        watch.assert_clean()
    except AssertionError as exception:
        rejected = {'code': 'OBSERVER_REJECTED', 'type': type(exception).__name__}
    assert rejected is not None, 'snapshot-only acceptance missed real child create-read-delete'
    report = watch.report()
    (work / 'observer.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    matching = [event for event in report['events']
                if event['root'] == str(target.parents[2])
                and event['relativePath'] == 'nested\\deeper\\extracted.yaml']
    assert {'ADDED', 'REMOVED'} <= {event['action'] for event in matching}, \
        'observer did not capture exact same-file ADD+REMOVE'
    assert not report['errors'], f'observer health failed instead of detecting activity: {report["errors"]}'
    assert all(root['barrierComplete'] for root in report['readiness']), 'missing positive drain boundary'
    return {'name': 'observer-transient-' + root_name, 'passed': True, 'command': command,
            'exitCode': child.returncode, 'reaped': True, 'armedBeforeSpawn': armed_before_spawn,
            'identicalFinalInventory': before == after, 'rejection': rejected,
            'initialInventory': before, 'finalInventory': after,
            'sameFileEvents': matching, 'observer': report}


def inject_observer_faults(stack, watch, mode, hits):
    """실제 관찰기의 startup/child 회수 후 경계에서만 민감 예외를 주입한다."""
    original_arm, original_complete = watch._arm, watch._complete

    def startup(state):
        if len(watch._states) == 2 and not hits:
            hits.append('sensitive-startup')
            sensitive_failure(OSError)
        return original_arm(state)

    def marker(state):
        if not hits:
            hits.append('sensitive-health')
            sensitive_failure(OSError)
        return original_marker(state)

    def drain(state, seconds, cancelling=False):
        result = original_complete(state, seconds, cancelling=cancelling)
        if not cancelling and not hits:
            hits.append('sensitive-drain')
            sensitive_failure(OSError)
        return result

    original_marker = watch._make_marker
    target, callback = {'sensitive-startup': ('_arm', startup),
                        'sensitive-health': ('_make_marker', marker),
                        'sensitive-drain': ('_complete', drain)}[mode]
    stack.enter_context(patch.object(watch, target, side_effect=callback))


def observer_health_control(work, mode):
    """Exercise real watches, including loss/error faults at completed Win32 I/O."""
    import application_observers as observer
    roots = [work / name for name in ('app', 'cwd', 'tmp')]
    for root in roots:
        root.mkdir(parents=True)
    if mode == 'partial-startup':
        roots[1] = work / 'nonexistent-root'
    watch = observer.FileActivityWatch(roots, timeout_seconds=0.1 if mode == 'health-timeout' else 5)
    command = [sys.executable, '-I', '-B', '-c', 'pass']
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([os.environ.get('SystemRoot', r'C:\Windows') + r'\System32',
                                  os.environ.get('SystemRoot', r'C:\Windows'), r'C:\Program Files\dotnet'])
    for key in ('TMP', 'TEMP', 'TMPDIR'):
        env[key] = str(roots[2])
    hits = []
    child = None
    rejection = None
    armed_before_spawn = False
    original_complete = watch._k.GetOverlappedResult
    original_parse = observer._parse_notifications

    def zero_byte_completion(*args):
        result = original_complete(*args)
        if result and not hits:
            hits.append('completed-I/O-zero-byte-overflow')
            c.cast(args[2], c.POINTER(c.c_ulong))[0] = 0
        return result

    def truncated_completed_record(data):
        if not hits:
            hits.append('completed-I/O-truncated-record')
            return original_parse(data[:7])
        return original_parse(data)

    def lost_health_marker(state):
        hits.append('missing-post-child-health-notification')
        state['marker'] = '.observer-health-intentionally-not-created'

    from contextlib import ExitStack
    try:
        with ExitStack() as patches:
            if mode.startswith('sensitive-'):
                inject_observer_faults(patches, watch, mode, hits)
            if mode == 'overflow':
                patches.enter_context(patch.object(watch._k, 'GetOverlappedResult',
                                                    side_effect=zero_byte_completion))
            if mode == 'malformed':
                patches.enter_context(patch.object(observer, '_parse_notifications',
                                                    side_effect=truncated_completed_record))
            if mode == 'health-timeout':
                patches.enter_context(patch.object(watch, '_make_marker', side_effect=lost_health_marker))
            with watch:
                armed_before_spawn = watch.report()['ready']
                assert armed_before_spawn, 'clean child started before watch readiness'
                child = subprocess.run(command, cwd=roots[1], env=env, capture_output=True,
                                       timeout=10, creationflags=subprocess.CREATE_NO_WINDOW)
                assert child.returncode == 0, f'clean child failed: {child.stderr!r}'
        watch.assert_clean()
    except AssertionError as exception:
        rejection = {'code': 'OBSERVER_REJECTED', 'type': type(exception).__name__}
    report = watch.report()
    (work / 'observer.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    assert all(root['closed'] and not root['resourcesRetained'] for root in report['readiness']), \
        'observer failed partial/normal resource cleanup'
    assert not report['events'], 'health-only fixture unexpectedly generated non-health activity'
    if mode == 'clean':
        assert rejection is None and not report['errors'], 'clean watch falsely rejected'
        assert child is not None and child.returncode == 0 and armed_before_spawn
        assert all(root['barrierComplete'] and root['cancellationCompleted'] for root in report['readiness'])
        for root in report['roots']:
            assert {'ADDED', 'REMOVED'} <= {event['action'] for event in report['healthEvents']
                                           if event['root'] == root}, 'health marker ADD+REMOVE absent'
    else:
        assert rejection is not None and report['errors'], 'health/loss control falsely accepted'
        if mode in ('partial-startup', 'sensitive-startup'):
            assert child is None and not armed_before_spawn and not report['ready']
            assert len(report['readiness']) == 2 and report['errors'][0]['operation'] == 'startup'
            assert report['readiness'][0]['armed'] and report['readiness'][0]['cancellationCompleted']
        else:
            assert child is not None and child.returncode == 0 and armed_before_spawn
            assert len(hits) == (3 if mode == 'health-timeout' else 1), 'intended health fault not reached'
        if mode.startswith('sensitive-'):
            if len(hits) != 1 or SECURITY_SENTINEL in json.dumps(report) or SECURITY_SENTINEL in json.dumps(rejection):
                raise RuntimeError('observer public artifact leaked sensitive exception or missed injection')
        elif mode != 'partial-startup':
            expected = {'overflow': 'zero-byte notification overflow/loss',
                        'malformed': 'truncated FILE_NOTIFY_INFORMATION header',
                        'health-timeout': 'directory notification completion timeout'}[mode]
            assert any(error['operation'] == 'post-child drain' and expected in error['error']
                       for error in report['errors']), 'wrong health failure identity'
    return {'name': 'observer-' + mode, 'passed': True, 'faultHits': hits,
            'command': command if child else None, 'exitCode': child.returncode if child else None,
            'reaped': child is not None and child.returncode is not None,
            'armedBeforeSpawn': armed_before_spawn, 'rejection': rejection, 'observer': report}


def observer_exact_token_control(work):
    from application_observers import FileActivityWatch
    roots = [work / name for name in ('app', 'cwd', 'tmp')]
    for root in roots:
        root.mkdir(parents=True)
    command = [sys.executable, '-I', '-B', '-c',
               'from pathlib import Path; import sys; p=Path(sys.argv[1]); '
               'p.write_bytes(b"owned"); q=p.with_name(p.name+"-renamed"); p.rename(q); q.unlink()',
               str(roots[0] / '.observer-health-not-an-allocated-token')]
    env = os.environ.copy()
    env['PATH'] = os.pathsep.join([os.environ.get('SystemRoot', r'C:\Windows') + r'\System32',
                                  os.environ.get('SystemRoot', r'C:\Windows'), r'C:\Program Files\dotnet'])
    for key in ('TMP', 'TEMP', 'TMPDIR'):
        env[key] = str(roots[2])
    with FileActivityWatch(roots) as watch:
        assert watch.report()['ready']
        child = subprocess.run(command, cwd=roots[1], env=env, capture_output=True,
                               timeout=10, creationflags=subprocess.CREATE_NO_WINDOW)
        assert child.returncode == 0, f'prefix/rename child failed: {child.stderr!r}'
    report = watch.report()
    (work / 'observer.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    rejection = None
    try:
        watch.assert_clean()
    except AssertionError as exception:
        rejection = {'code': 'OBSERVER_REJECTED', 'type': type(exception).__name__}
    assert rejection and not report['errors'], 'prefix/rename failed for wrong reason'
    assert {'ADDED', 'MODIFIED', 'RENAMED_OLD_NAME', 'RENAMED_NEW_NAME', 'REMOVED'} <= \
        {event['action'] for event in report['events']}, 'write/rename or arbitrary-prefix event was ignored'
    assert all(not list(root.iterdir()) for root in roots), 'prefix control final roots were not empty'
    return {'name': 'observer-exact-health-token', 'passed': True, 'command': command,
            'exitCode': child.returncode, 'reaped': True, 'rejection': rejection, 'observer': report}


# 아래 값은 실제 키가 아닌 누출 탐지 전용 가짜 민감값이다.
SECURITY_SENTINEL = 'CONTROL_ONLY_SECRET_MUST_NOT_APPEAR_IN_FAILURE_ARTIFACT'
STREAM_CONTROLS = ('stdout-second-flush-failure', 'application-success-artifact-failure',
                   'application-compound-output-artifact-failure',
                   'meta-success-stdout-write-failure', 'meta-success-stdout-flush-failure',
                   'meta-success-artifact-failure', 'meta-compound-output-artifact-failure',
                   'cli-all-required-success', 'cli-newgame-only-success', 'meta-all-required-success')
REPORT_CONTROLS = ('report-update-truncate-failure', 'report-update-partial-failure',
                   'report-update-close-failure', 'report-update-replace-failure',
                   'report-update-success', 'report-initial-truncate-failure')
CLI_CONTROLS = ('initial-console-failure', 'partial-progress-failure', 'error-artifact-failure',
                'stderr-sink-failure', 'stdout-sink-failure', 'stdout-flush-failure',
                'meta-failure', 'meta-error-artifact-failure', 'meta-stderr-sink-failure',
                'observer-sensitive-startup', 'observer-sensitive-health', 'observer-sensitive-drain') + STREAM_CONTROLS + REPORT_CONTROLS


def execute_entrypoint(module, path):
    # 파일의 실제 __main__ guard를 실행한다. 별도 복제 runner/경계 구현은 없다.
    tree = ast.parse(path.read_text(encoding='utf-8'), filename=str(path))
    guard = tree.body[-1]
    if not isinstance(guard, ast.If) or ast.unparse(guard.test) not in (
            "__name__ == '__main__'", "'__main__' == __name__"):
        raise RuntimeError('expected final script entrypoint guard')
    namespace = module.__dict__
    previous = namespace['__name__']
    try:
        namespace['__name__'] = '__main__'
        exec(compile(ast.Module(body=[guard], type_ignores=[]), str(path), 'exec'), namespace)
    finally:
        namespace['__name__'] = previous


def sensitive_failure(exception_type=AssertionError):
    try:
        raise ValueError('CONTROL_ONLY_CAUSE_TEXT ' + SECURITY_SENTINEL)
    except ValueError as cause:
        if exception_type is OSError:
            error = OSError(123, 'CONTROL_ONLY_EXCEPTION_TEXT ' + SECURITY_SENTINEL)
            error.winerror = 123
            raise error from cause
        raise exception_type('CONTROL_ONLY_EXCEPTION_TEXT ' + SECURITY_SENTINEL) from cause


def install_security_faults(stack, trace, mode, artifact):
    original_write = Path.write_text
    original_stdout = sys.stdout

    def write(path, text, *args, **kwargs):
        same_target = path == artifact or path == artifact.with_name('.' + artifact.name + '.pending')
        if mode in STREAM_CONTROLS and same_target:
            trace.setdefault('publicationAttempts', []).append(json.loads(text))
            if 'artifact-failure' in mode and json.loads(text).get('status') != 'incomplete':
                trace['artifactSinkHits'] = trace.get('artifactSinkHits', 0) + 1
                sensitive_failure(OSError)
        if same_target and (mode in ('meta-error-artifact-failure', 'meta-stderr-sink-failure') or (mode == 'error-artifact-failure' and '"status": "failed"' in text)):
            trace['artifactSinkHits'] = trace.get('artifactSinkHits', 0) + 1
            sensitive_failure(OSError)
        return original_write(path, text, *args, **kwargs)

    class BrokenStream:
        def write(self, text):
            if mode in STREAM_CONTROLS:
                if ('"directory"' in text or '"exeSha256"' in text) and '"passed"' in text:
                    trace['summaryOutputReached'] = True
                    if 'meta-' in mode:
                        rows = [json.loads(path.read_text(encoding='utf-8')) for path in
                                (artifact.parent).glob('application-*/result.json')]
                        trace['allPassBeforeOutput'] = len(rows) == len(ACCEPTANCE_CONTROLS) and all(
                            row.get('passed') is True for row in rows)
                if trace.get('summaryOutputReached') and ('write-failure' in mode or 'compound-output' in mode):
                    trace['logSinkHits'] = trace.get('logSinkHits', 0) + 1
                    sensitive_failure(OSError)
                return original_stdout.write(text)
            if mode == 'stdout-flush-failure':
                return 0
            trace['logSinkHits'] = trace.get('logSinkHits', 0) + 1
            sensitive_failure(OSError)

        def flush(self):
            if mode in STREAM_CONTROLS:
                trace['flushCalls'] = trace.get('flushCalls', 0) + 1
                if (mode == 'stdout-second-flush-failure' and trace['flushCalls'] == 2
                        or mode == 'meta-success-stdout-flush-failure' and trace.get('summaryOutputReached')):
                    trace['flushSinkHits'] = trace.get('flushSinkHits', 0) + 1
                    sensitive_failure(OSError)
                return original_stdout.flush()
            # stderr write 실패를 flush 실패가 먼저 가리지 않도록 별도 단계로 만든다.
            if mode == 'stdout-flush-failure' or trace.get('logSinkHits'):
                trace['flushSinkHits'] = trace.get('flushSinkHits', 0) + 1
                sensitive_failure(OSError)

    if mode in STREAM_CONTROLS or mode in ('error-artifact-failure', 'meta-error-artifact-failure', 'meta-stderr-sink-failure'):
        stack.enter_context(patch.object(Path, 'write_text', write))
    if mode in ('stderr-sink-failure', 'meta-stderr-sink-failure'):
        stack.enter_context(patch.object(sys, 'stderr', BrokenStream()))
    if mode in STREAM_CONTROLS or mode in ('stdout-sink-failure', 'stdout-flush-failure'):
        stack.enter_context(patch.object(sys, 'stdout', BrokenStream()))


def install_report_faults(stack, trace, mode, artifact):
    """원본 write_text/open으로 실제 truncate·쓰기·close를 끝낸 경계만 주입한다."""
    original_open, original_replace = Path.open, Path.replace
    pending = artifact.with_name('.' + artifact.name + '.pending')
    stage = mode.removeprefix('report-update-').removesuffix('-failure')
    initial = mode == 'report-initial-truncate-failure'
    if initial:
        stage = 'truncate'

    def fail(path):
        # fault-hit은 실제 I/O 관측 완료 뒤에만 올린다. 오류 문자열/cause는 공개하지 않는다.
        reason = 13 if stage == 'replace' else 28
        error = OSError(reason, 'CONTROL_ONLY_REPORT_IO_EXCEPTION ' + SECURITY_SENTINEL)
        trace['reportIO'].update(stage=stage, faultHits=1, completed=True,
                                 target='pending' if path == pending else 'canonical',
                                 error={'type': type(error).__name__, 'errno': error.errno})
        try:
            raise ValueError('CONTROL_ONLY_REPORT_IO_CAUSE ' + SECURITY_SENTINEL)
        except ValueError as cause:
            raise error from cause

    class Writer:
        def __init__(self, path, stream):
            self.path, self.stream = path, stream

        def __enter__(self):
            self.stream.__enter__()
            return self

        def write(self, text):
            expected = text.encode('utf-8').replace(b'\n', os.linesep.encode())
            trace['reportIO']['requestedBytes'] = len(expected)
            if stage == 'truncate':
                if self.path.read_bytes() != b'':
                    raise RuntimeError('report truncate precondition not completed')
                trace['reportIO'].update(truncated=True, writtenBytes=0)
                fail(self.path)
            if stage == 'partial':
                prefix = text[:len(text) // 2]
                count = self.stream.write(prefix)
                self.stream.flush()
                actual = self.path.read_bytes()
                expected_prefix = prefix.encode('utf-8').replace(b'\n', os.linesep.encode())
                if count != len(prefix) or not (0 < len(actual) < len(expected)) or actual != expected_prefix:
                    raise RuntimeError('report positive prefix write precondition not completed')
                trace['reportIO'].update(writtenBytes=len(actual), positivePrefixVerified=True)
                fail(self.path)
            count = self.stream.write(text)
            trace['reportIO']['writeReturnedCharacters'] = count
            return count

        def __exit__(self, *args):
            result = self.stream.__exit__(*args)
            trace['reportIO']['realCloseCompleted'] = self.stream.closed
            if stage in ('close', 'replace') and args[0] is None:
                actual = self.path.read_bytes()
                trace['reportIO']['writtenBytes'] = len(actual)
                trace['reportIO']['fullWriteVerified'] = (
                    len(actual) == trace['reportIO']['requestedBytes']
                    and json.loads(actual)['status'] == 'failed')
                if not self.stream.closed or not trace['reportIO']['fullWriteVerified']:
                    raise RuntimeError('report full write/real close precondition not completed')
                if stage == 'close':
                    fail(self.path)
            return result

    def open_file(path, *args, **kwargs):
        stream = original_open(path, *args, **kwargs)
        writing = (args[0] if args else kwargs.get('mode', 'r')) == 'w'
        active = initial or trace.get('reportRunFailureReached') is True
        if writing and path in (artifact, pending) and active and mode != 'report-update-success':
            return Writer(path, stream)
        return stream

    def replace(path, target):
        if (stage == 'replace' and path == pending and Path(target) == artifact
                and trace.get('reportRunFailureReached') is True):
            fail(path)
        return original_replace(path, target)

    trace['reportIO'] = {'faultHits': 0, 'completed': False, 'writtenBytes': 0}
    stack.enter_context(patch.object(Path, 'open', open_file))
    stack.enter_context(patch.object(Path, 'replace', replace))


def report_update_failures(record, mode):
    """실제 관측과 변형 음성을 같은 -O-safe 판정으로 검사한다."""
    trace, final = record['trace'], record['finalArtifact']
    io_record = trace.get('reportIO', {})
    failures = []
    initial = mode == 'report-initial-truncate-failure'
    positive = mode == 'report-update-success'
    if trace.get('pendingRemaining'):
        failures.append('owned pending remains')
    if initial:
        if (trace['subprocessCalls'] or trace['consoleCalls'] or trace['progressCalls']
                or trace.get('reportRunFailureReached') or final is not None):
            failures.append('initial write failure falsely reached work or published success')
    else:
        required_initial = ['missing-package', 'ingamedata-newgame-autosave']
        incomplete = {'passed': False, 'status': 'incomplete', 'cases': 0, 'results': []}
        if (trace.get('reportBefore') != incomplete or not trace.get('reportBeforeSha256')
                or trace.get('reportInitialPositiveNames') != required_initial
                or trace.get('reportRunFailureReached') is not True
                or trace['faultHits'] != 1 or trace['progressCalls'] != 1
                or trace['consoleCalls'] != 1 or len(trace['subprocessCalls']) != 2
                or trace.get('watchExited') != 2):
            failures.append('valid incomplete and actual run_application positives/failure not reached')
        if positive:
            if (final is None or final.get('passed') is not False or final.get('status') != 'failed'
                    or final.get('error') != {'type': 'RuntimeError'} or final.get('cases') != 2
                    or [row.get('name') for row in final.get('results', [])] != required_initial
                    or any(row.get('passed') is not True for row in final.get('results', []))):
                failures.append('normal failed-report publication lost original RuntimeError/results')
        elif (final != incomplete or trace.get('canonicalBytesUnchanged') is not True
                or trace.get('reportBeforeSha256') != trace.get('reportAfterSha256')):
            failures.append('canonical incomplete bytes corrupted')
    if positive:
        if io_record.get('faultHits') != 0:
            failures.append('positive publication unexpectedly injected I/O fault')
        return failures
    stage = 'truncate' if initial else mode.removeprefix('report-update-').removesuffix('-failure')
    reason = 13 if stage == 'replace' else 28
    expected_error = {'type': 'PermissionError' if reason == 13 else 'OSError', 'errno': reason}
    if (io_record.get('stage') != stage or io_record.get('error') != expected_error
            or io_record.get('faultHits') != 1 or io_record.get('completed') is not True
            or io_record.get('realCloseCompleted') is not True
            or io_record.get('target') != ('canonical' if initial else 'pending')):
        failures.append('exact report I/O stage/reason/hit/completion/ownership mismatch')
    written, requested = io_record.get('writtenBytes', -1), io_record.get('requestedBytes', 0)
    if stage == 'truncate' and (written != 0 or io_record.get('truncated') is not True or requested <= 0):
        failures.append('real truncation not completed')
    if stage == 'partial' and (not 0 < written < requested or io_record.get('positivePrefixVerified') is not True):
        failures.append('positive partial write not completed')
    if stage in ('close', 'replace') and (written != requested or written <= 0 or io_record.get('fullWriteVerified') is not True):
        failures.append('full write before close/replace not completed')
    return failures


def application_acceptance_control(work, mode, entrypoint=False):
    """실제 main의 게시 순서 검사다. 제품 런타임 성공 증거로 사용하지 않는다."""
    application = TARGET.with_name('test_game_data_application.py')
    spec = importlib.util.spec_from_file_location('acceptance_under_test', application)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    executable = work / 'control-only-never-executed.exe'
    executable.write_bytes(b'runner control-flow fixture, not a product executable')
    output = work / 'application'
    artifact = output / 'results.json'
    trace = {'subprocessCalls': [], 'consoleCalls': 0, 'progressCalls': 0,
             'observedArtifacts': [], 'faultHits': 0}
    run_exceptions = []
    sensitive_value = SECURITY_SENTINEL
    trigger_mode = ('newgame-only-success' if mode == 'cli-newgame-only-success'
                    else 'all-required-success' if mode in STREAM_CONTROLS or mode in ('stdout-sink-failure', 'stdout-flush-failure')
                    else 'initial-console-failure' if mode in ('error-artifact-failure', 'stderr-sink-failure')
                    else mode)
    required_progress = ['restart-continue-restores-changed-progress-and-quit-roundtrip',
                         'corrupt-save-keeps-exact-menu-localized-error-and-old-bytes',
                         'quit-save-failure-localized-nonzero-preserves-ciphertext']

    def observe(stage):
        value = json.loads(artifact.read_text(encoding='utf-8')) if artifact.is_file() else None
        trace['observedArtifacts'].append({'stage': stage, 'artifact': value})

    class Watch:
        def __init__(self, roots):
            assert [root.name for root in roots] == ['app', 'unrelated-cwd', 'runtime-temp']
            assert output / 'save-root' not in roots, 'save root crossed extraction watch boundary'

        def __enter__(self):
            trace['watchEntered'] = trace.get('watchEntered', 0) + 1
            return self

        def __exit__(self, *_):
            trace['watchExited'] = trace.get('watchExited', 0) + 1
            return False

        def assert_clean(self):
            pass

        def report(self):
            return {'scope': 'runner control-flow only; no real directory observer'}

    def child(command, **kwargs):
        trace['subprocessCalls'].append(command)
        assert kwargs['env']['TEXT_RPG_SAVE_ROOT'] == str(output / 'save-root')
        assert all(kwargs['env'][key] == str(output / 'runtime-temp')
                   for key in ('TMP', 'TEMP', 'TMPDIR'))
        if len(trace['subprocessCalls']) == 1:
            assert command == [str(output / 'app/TextRPG.exe')]
            assert kwargs['cwd'] == output / 'unrelated-cwd'
            return subprocess.CompletedProcess(command, 1, b'GameData.dat package missing')
        assert len(trace['subprocessCalls']) == 2 and command[1] == 'pack'
        Path(command[3]).write_bytes(b'runner pack control, not an encrypted product package')
        return subprocess.CompletedProcess(command, 0, b'runner pack control')

    def console(*args, **kwargs):
        trace['consoleCalls'] += 1
        assert args[7] is True
        observe('newgame-before-return')
        if trigger_mode == 'initial-console-failure':
            trace['faultHits'] += 1
            sensitive_failure()

    def progress(*args):
        trace['progressCalls'] += 1
        assert args[0] == executable
        results = args[10]
        assert [row['name'] for row in results] == ['missing-package', 'ingamedata-newgame-autosave']
        assert all(row['passed'] for row in results), 'required initial controls did not succeed'
        observe('progress-after-initial-positive')
        if mode in REPORT_CONTROLS:
            before = artifact.read_bytes()
            trace['reportBefore'] = json.loads(before)
            trace['reportBeforeSha256'] = hashlib.sha256(before).hexdigest()
            (work / 'incomplete-before.json').write_bytes(before)
            trace['reportInitialPositiveNames'] = [row['name'] for row in results if row.get('passed') is True]
            trace['reportRunFailureReached'] = True
            trace['faultHits'] += 1
            try:
                raise ValueError('CONTROL_ONLY_REPORT_RUN_CAUSE ' + SECURITY_SENTINEL)
            except ValueError as cause:
                error = RuntimeError('CONTROL_ONLY_REPORT_RUN_EXCEPTION ' + SECURITY_SENTINEL)
                run_exceptions.append(error)
                raise error from cause
        if mode == 'later-failure':
            trace['faultHits'] += 1
            raise AssertionError('intended subsequent progress acceptance failure')
        selected = required_progress[:-1] if mode == 'missing-required-case' else required_progress
        for name in selected:
            results.append({'name': name, 'passed': not (mode == 'failed-required-case' and name == selected[-1])})
            observe('progress-completed-' + name)
            if trigger_mode == 'partial-progress-failure':
                trace['faultHits'] += 1
                sensitive_failure()

    invalid_fixture = mode in ('missing-fixture', 'nonexistent-fixture', 'fixture-directory')
    success = trigger_mode in ('all-required-success', 'newgame-only-success')
    command = ['test_game_data_application.py', '--game', str(executable), '--packer', str(executable),
               '--scratch-root', str(output), '--only',
               'ingamedata-newgame' if trigger_mode == 'newgame-only-success' else 'ingamedata']
    if trigger_mode not in ('missing-fixture', 'newgame-only-success'):
        fixture = work / 'does-not-exist.exe' if mode == 'nonexistent-fixture' else (
            work if mode == 'fixture-directory' else executable)
        command += ['--progress-fixture', str(fixture)]
    error_type = None
    exit_code = None
    stderr, stdout = io.StringIO(), io.StringIO()
    try:
        with ExitStack() as stack:
            if not entrypoint:
                stack.enter_context(redirect_stderr(stderr))
                stack.enter_context(redirect_stdout(stdout))
            install_security_faults(stack, trace, mode, artifact)
            if mode in REPORT_CONTROLS:
                install_report_faults(stack, trace, mode, artifact)
            stack.enter_context(patch.object(sys, 'argv', command))
            stack.enter_context(patch.dict(module.os.environ, {'TEXT_RPG_GAMEDATA_KEY': sensitive_value}))
            stack.enter_context(patch.object(module, 'FileActivityWatch', Watch))
            if mode.startswith('observer-sensitive-'):
                from application_observers import FileActivityWatch

                def real_watch(roots):
                    watch = FileActivityWatch(roots)
                    inject_observer_faults(stack, watch, mode.removeprefix('observer-'),
                                           trace.setdefault('observerFaultHits', []))
                    trace['realObserverCount'] = trace.get('realObserverCount', 0) + 1
                    return watch

                stack.enter_context(patch.object(module, 'FileActivityWatch', side_effect=real_watch))
            stack.enter_context(patch.object(module.subprocess, 'run', side_effect=child))
            stack.enter_context(patch.object(module, 'console_smoke', side_effect=console))
            stack.enter_context(patch.object(module, 'progress_acceptance', side_effect=progress))
            try:
                if entrypoint:
                    execute_entrypoint(module, application)
                else:
                    module.main()
            finally:
                trace['disabledStreams'] = [name for name in ('stdout', 'stderr') if getattr(sys, name) is None]
    except (AssertionError, RuntimeError) as exception:
        if entrypoint:
            raise
        error_type = type(exception).__name__
        if mode == 'report-update-success':
            trace['originalRunExceptionReraised'] = (
                len(run_exceptions) == 1 and exception is run_exceptions[0]
                and type(exception) is RuntimeError
                and str(exception) == 'CONTROL_ONLY_REPORT_RUN_EXCEPTION ' + sensitive_value
                and type(exception.__cause__) is ValueError
                and str(exception.__cause__) == 'CONTROL_ONLY_REPORT_RUN_CAUSE ' + sensitive_value)
        if mode == 'later-failure':
            assert str(exception) == 'intended subsequent progress acceptance failure'
        if mode in ('initial-console-failure', 'partial-progress-failure'):
            assert str(exception) == 'CONTROL_ONLY_EXCEPTION_TEXT ' + sensitive_value
    except SystemExit as exception:
        error_type = type(exception).__name__
        exit_code = exception.code
    finally:
        raw = artifact.read_bytes() if artifact.is_file() else None
        try:
            final = json.loads(raw) if raw is not None else None
        except ValueError:
            if mode not in REPORT_CONTROLS:
                raise
            final = None
        if mode in REPORT_CONTROLS:
            trace['reportAfterSha256'] = hashlib.sha256(raw).hexdigest() if raw is not None else None
            before_path = work / 'incomplete-before.json'
            trace['canonicalBytesUnchanged'] = before_path.is_file() and raw == before_path.read_bytes()
            trace['pendingRemaining'] = artifact.with_name('.' + artifact.name + '.pending').exists()
        record = {'name': 'application-' + mode,
                  'scope': ('actual script guard in separate process' if entrypoint else 'direct imported main')
                           + ' with controlled child boundaries; NOT product runtime acceptance',
                  'errorType': error_type, 'exitCode': exit_code, 'diagnostic': stderr.getvalue(),
                  'stdout': stdout.getvalue(), 'trace': trace, 'finalArtifact': final,
                  'sourceSha256': hashlib.sha256(application.read_bytes()).hexdigest()}
        (work / 'control-trace.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
    if entrypoint:
        # 외부 부모가 실제 process exit/stdout/stderr와 모든 산출물을 검증한다.
        return exit_code
    if mode in REPORT_CONTROLS:
        if report_update_failures(record, mode) or trace.get('originalRunExceptionReraised') is not True:
            raise RuntimeError('direct main failed report/original exception contract')
        return dict(record, passed=True)
    if invalid_fixture:
        assert trace['subprocessCalls'] == [] and trace['consoleCalls'] == trace['progressCalls'] == 0, \
            'missing/invalid required fixture reached children or packing before CLI rejection'
        assert error_type == 'SystemExit' and exit_code == 2
        assert '--progress-fixture native executable required' in stderr.getvalue()
        assert not output.exists(), 'invalid CLI created application work before preflight rejection'
    else:
        assert trace['consoleCalls'] == 1 and len(trace['subprocessCalls']) == 2
        assert trace['progressCalls'] == int(mode not in ('newgame-only-success', 'initial-console-failure'))
        if success:
            assert error_type is None and final['passed'] is True and final['status'] == 'complete'
            names = ['missing-package', 'ingamedata-newgame-autosave']
            if mode == 'all-required-success':
                names += required_progress
            assert [row['name'] for row in final['results']] == names
            assert final['cases'] == len(names) and all(row['passed'] is True for row in final['results'])
        else:
            assert error_type is not None, 'incomplete/failed required cases were accepted as success'
            assert trace['faultHits'] == int(mode in ('later-failure', 'initial-console-failure', 'partial-progress-failure'))
            assert final is not None and final['passed'] is False and final['status'] == 'failed'
            assert final['error'] == {'type': error_type}
            assert final['cases'] == len(final['results'])
    if not success:
        assert final is None or final.get('passed') is False, \
            'later required failure left passed:true results.json after successful initial cases'
    assert all(item['artifact'] is None or item['artifact'].get('passed') is False
               for item in trace['observedArtifacts']), 'success was published before all required cases completed'
    assert sensitive_value not in artifact.read_text(encoding='utf-8') if artifact.is_file() else True
    return dict(record, passed=True)


def real_application_preflight(work, mode, game, packer):
    """동결된 실제 경로와 감사 훅으로 자식 생성 전 CLI 거부를 확인한다(-O 포함)."""
    application = TARGET.with_name('test_game_data_application.py')
    spec = importlib.util.spec_from_file_location('real_preflight_under_test', application)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if not game.is_file() or not packer.is_file():
        raise RuntimeError('real preflight executable prerequisites missing')
    output = work / 'application'
    command = [str(application), '--game', str(game), '--packer', str(packer),
               '--scratch-root', str(output), '--only', 'ingamedata']
    if mode == 'nonexistent-fixture':
        command += ['--progress-fixture', str(work / 'does-not-exist.exe')]
    children = []
    auditing = [True]

    def audit(event, args):
        if auditing[0] and event == 'subprocess.Popen':
            children.append(str(args[0]))
            # 잘못된 runner도 실제 제품 자식을 실행하지 않도록 감사 경계에서 거부한다.
            raise RuntimeError('required fixture validation reached child creation')

    sys.addaudithook(audit)
    stderr = io.StringIO()
    exit_code = None
    try:
        with patch.object(sys, 'argv', command), redirect_stderr(stderr):
            module.main()
    except SystemExit as exception:
        exit_code = exception.code
    finally:
        auditing[0] = False
    diagnostic = stderr.getvalue()
    if (exit_code != 2 or children or output.exists()
            or '--progress-fixture native executable required' not in diagnostic):
        raise RuntimeError('real preflight did not fail closed before children/work creation')
    return {'name': 'real-preflight-' + mode, 'passed': True,
            'scope': 'actual main with frozen real executable paths; preflight only, no product child execution',
            'optimized': not __debug__, 'applicationExitCode': exit_code, 'applicationArgv': command,
            'childrenAttempted': children, 'childCount': len(children), 'workCreated': output.exists(),
            'successArtifactPresent': (output / 'results.json').exists(), 'diagnostic': diagnostic,
            'sourceSha256': hashlib.sha256(application.read_bytes()).hexdigest(),
            'gameSha256': hashlib.sha256(game.read_bytes()).hexdigest(),
            'packerSha256': hashlib.sha256(packer.read_bytes()).hexdigest()}


def meta_security_worker(work, mode):
    path = Path(__file__).resolve()
    spec = importlib.util.spec_from_file_location('meta_security_under_test', path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    trace = {'faultHits': 0}
    command = [str(path), '--scratch-root', str(work), '--label', 'meta', '--only', 'acceptance']
    artifact = work / ('meta/results.json' if mode in STREAM_CONTROLS else 'meta/application-later-failure/result.json')

    def fail(*_):
        trace['faultHits'] += 1
        sensitive_failure(RuntimeError)

    try:
        with ExitStack() as stack:
            install_security_faults(stack, trace, mode, artifact)
            stack.enter_context(patch.object(sys, 'argv', command))
            if mode not in STREAM_CONTROLS:
                stack.enter_context(patch.object(module, 'application_acceptance_control', side_effect=fail))
            # 내부 meta가 security process를 다시 생성하지 않는다.
            stack.enter_context(patch.object(module, 'CLI_CONTROLS', ()))
            try:
                execute_entrypoint(module, path)
            finally:
                trace['disabledStreams'] = [name for name in ('stdout', 'stderr') if getattr(sys, name) is None]
    finally:
        (work / 'control-trace.json').write_text(json.dumps({'trace': trace}, indent=2), encoding='utf-8')


def cli_security_control(work, mode, optimized):
    name = mode + ('-optimized' if optimized else '-normal')
    # 중첩 meta의 실제 자산 fixture도 Win32 MAX_PATH 안에 둔다.
    label = 'c' + str(CLI_CONTROLS.index(mode)) + ('o' if optimized else 'n')
    command = [sys.executable, '-B', *(['-O'] if optimized else []), str(Path(__file__).resolve()),
               '--scratch-root', str(work), '--label', label, '--worker-security', mode]
    env = os.environ.copy()
    for key in ('TMP', 'TEMP', 'TMPDIR'):
        env[key] = str(work)
    child = subprocess.run(command, env=env, capture_output=True, timeout=30,
                           creationflags=subprocess.CREATE_NO_WINDOW)
    directory = work / label
    stdout, stderr = child.stdout.decode('utf-8', errors='replace'), child.stderr.decode('utf-8', errors='replace')
    (work / (name + '.stdout.log')).write_bytes(child.stdout)
    (work / (name + '.stderr.log')).write_bytes(child.stderr)
    record = json.loads((directory / 'control-trace.json').read_text(encoding='utf-8'))
    trace = record['trace']
    paths = sorted(path for path in directory.rglob('*') if path.is_file())
    forbidden = (SECURITY_SENTINEL, 'CONTROL_ONLY_EXCEPTION_TEXT', 'CONTROL_ONLY_CAUSE_TEXT',
                 'Traceback (most recent call last)', 'The above exception was the direct cause',
                 'During handling of the above exception')
    failures = []
    success_control = mode in ('cli-all-required-success', 'cli-newgame-only-success', 'meta-all-required-success')
    if child.returncode != (0 if success_control else 1):
        failures.append('wrong explicit exit status')
    if any(token in stdout or token in stderr or any(token.encode() in path.read_bytes() for path in paths)
           for token in forbidden):
        failures.append('exception-detail-or-chain-leak')
    is_meta = mode.startswith('meta-')
    artifact = directory / ('meta/results.json' if is_meta else 'application/results.json')
    try:
        final = json.loads(artifact.read_text(encoding='utf-8')) if artifact.is_file() else None
    except ValueError:
        if mode not in REPORT_CONTROLS:
            raise
        final = None
    if mode in REPORT_CONTROLS:
        # worker가 쓴 관측값만 신뢰하지 않고 외부에서 canonical 바이트를 다시 대조한다.
        before = directory / 'incomplete-before.json'
        raw = artifact.read_bytes() if artifact.is_file() else None
        if before.is_file():
            trace['reportBeforeSha256'] = hashlib.sha256(before.read_bytes()).hexdigest()
            trace['reportBefore'] = json.loads(before.read_bytes())
            trace['canonicalBytesUnchanged'] = raw == before.read_bytes()
        trace['reportAfterSha256'] = hashlib.sha256(raw).hexdigest() if raw is not None else None
        trace['pendingRemaining'] = artifact.with_name('.' + artifact.name + '.pending').exists()
        record['finalArtifact'] = final
        failures += report_update_failures(record, mode)
        positive = mode == 'report-update-success'
        expected_type = 'RuntimeError' if positive else ('PermissionError' if 'replace' in mode else 'OSError')
        if stdout or stderr.splitlines() != ['APPLICATION_FAILED type=' + expected_type]:
            failures.append('report failure CLI diagnostic not fixed/type-only')
        controls = []
        for name in ('wrong-reason', 'unhit', 'uncompleted'):
            modified = json.loads(json.dumps(record))
            if name == 'wrong-reason':
                if positive:
                    modified['finalArtifact']['error'] = {'type': 'OSError'}
                else:
                    modified['trace']['reportIO']['error'] = {'type': expected_type, 'errno': 999}
            elif name == 'unhit':
                modified['trace']['faultHits' if positive else 'reportIO'] = (0 if positive else
                    dict(modified['trace']['reportIO'], faultHits=0))
            else:
                if positive:
                    modified['trace']['reportRunFailureReached'] = False
                else:
                    modified['trace']['reportIO']['completed'] = False
            rejected = report_update_failures(modified, mode)
            controls.append({'name': name, 'rejected': bool(rejected), 'failures': rejected})
        if not all(control['rejected'] for control in controls):
            failures.append('report oracle accepted wrong-reason/unhit/uncompleted control')
        return {'name': 'cli-security-' + mode + ('-optimized' if optimized else '-normal'),
                'passed': not failures, 'failures': failures,
                'scope': 'real Python report I/O/actual main/guard/CLI; controlled application children, NOT native SAVE/product acceptance',
                'optimized': optimized, 'command': command, 'exitCode': child.returncode,
                'stdoutSentinelAbsent': not any(token in stdout for token in forbidden),
                'stderrSentinelAbsent': not any(token in stderr for token in forbidden),
                'artifactSentinelAbsent': not any(token.encode() in path.read_bytes() for token in forbidden for path in paths),
                'filesScanned': [str(path.relative_to(directory)) for path in paths],
                'trace': trace, 'oracleControls': controls, 'finalArtifact': final,
                'sourceSha256': record['sourceSha256']}
    if mode in STREAM_CONTROLS:
        if success_control:
            if final is None or final.get('passed') is not True:
                failures.append('normal positive control did not publish success')
        elif final is not None and final.get('passed') is not False:
            failures.append('late output/publication failure left success artifact')
        if mode == 'stdout-second-flush-failure' and (trace.get('flushSinkHits') != 1 or trace.get('flushCalls', 0) < 2):
            failures.append('exact second flush fault not reached')
        if 'meta-' in mode and ('stdout' in mode or 'compound' in mode) and not trace.get('allPassBeforeOutput'):
            failures.append('meta was not all-pass before final output failure')
        if 'artifact-failure' in mode and not trace.get('artifactSinkHits'):
            failures.append('publication sink fault not reached')
        if ('write-failure' in mode or 'compound-output' in mode) and not trace.get('logSinkHits'):
            failures.append('final stdout write fault not reached')
        if mode == 'meta-success-stdout-flush-failure' and not trace.get('flushSinkHits'):
            failures.append('final meta flush fault not reached')
        if not is_meta and (trace['consoleCalls'] != 1 or len(trace['subprocessCalls']) != 2
                or trace['progressCalls'] != int(mode != 'cli-newgame-only-success') or trace.get('watchExited') != 2):
            failures.append('application positive prerequisites or watch lifetime not reached')
        return {'name': 'cli-security-' + name, 'passed': not failures, 'failures': failures,
                'scope': 'actual __main__/shared CLI and controlled flow; NOT product runtime acceptance',
                'optimized': optimized, 'command': command, 'exitCode': child.returncode,
                'stdoutSentinelAbsent': not any(token in stdout for token in forbidden),
                'stderrSentinelAbsent': not any(token in stderr for token in forbidden),
                'artifactSentinelAbsent': not any(token.encode() in path.read_bytes() for token in forbidden for path in paths),
                'filesScanned': [str(path.relative_to(directory)) for path in paths], 'trace': trace,
                'finalArtifact': final}
    if is_meta:
        if trace['faultHits'] != (len(ACCEPTANCE_CONTROLS) if mode == 'meta-failure' else 1):
            failures.append('meta fault not reached exactly')
        if mode == 'meta-failure' and (final is None or final['passed'] is not False
                or len(final['records']) != len(ACCEPTANCE_CONTROLS)
                or any(row.get('passed') is not False for row in final['records'])):
            failures.append('meta false success or missing failed records')
        if mode != 'meta-failure' and final is not None:
            failures.append('meta artifact write failure published summary')
    else:
        if final is None or final['passed'] is not False:
            failures.append('application failure left success artifact')
        expected_status = 'incomplete' if mode == 'error-artifact-failure' else 'failed'
        if final and final.get('status') != expected_status:
            failures.append('wrong application failure artifact state')
        if mode.startswith('observer-sensitive-'):
            expected_children = int(mode != 'observer-sensitive-startup')
            if (trace['consoleCalls'] or trace['progressCalls'] or len(trace['subprocessCalls']) != expected_children
                    or trace.get('realObserverCount') != 1 or len(trace.get('observerFaultHits', [])) != 1):
                failures.append('real observer fault route or child lifetime incorrect')
            reports = list((directory / 'application').glob('*-file-activity.json'))
            report = json.loads(reports[0].read_text(encoding='utf-8')) if len(reports) == 1 else {}
            errors = report.get('errors', [])
            operation = {'observer-sensitive-startup': 'startup',
                         'observer-sensitive-health': 'post-child health marker',
                         'observer-sensitive-drain': 'post-child drain'}[mode]
            if not any(row.get('operation') == operation and row.get('errorType') == 'OSError'
                       and row.get('errno') == row.get('winerror') == 123 for row in errors):
                failures.append('observer failure classification or native reason lost')
            if (not report.get('finished') or not errors or not report.get('readiness')
                    or any(not row['closed'] or row['resourcesRetained'] for row in report['readiness'])):
                failures.append('observer cleanup/readiness lost')
        elif trace['consoleCalls'] != 1 or len(trace['subprocessCalls']) != 2:
            failures.append('application prerequisite controls not reached')
        expected_progress = int(mode in ('partial-progress-failure', 'stdout-sink-failure', 'stdout-flush-failure'))
        if not mode.startswith('observer-') and (trace['progressCalls'] != expected_progress or trace.get('watchExited') != 2):
            failures.append('wrong progress route or watch cleanup lifetime')
        if not mode.startswith('observer-') and trace['faultHits'] != int(mode not in ('stdout-sink-failure', 'stdout-flush-failure')):
            failures.append('application fault not reached exactly')
        if final and mode != 'error-artifact-failure':
            expected_type = 'OSError' if mode.startswith('stdout-') else 'AssertionError'
            if final.get('error') != {'type': expected_type}:
                failures.append('wrong application exception type')
    if 'artifact-failure' in mode or mode == 'meta-stderr-sink-failure':
        if trace.get('artifactSinkHits') != 1:
            failures.append('artifact sink fault not reached exactly')
    if 'stderr-sink' in mode or mode == 'stdout-sink-failure':
        stream = 'stderr' if 'stderr' in mode else 'stdout'
        if not trace.get('logSinkHits') or stream not in trace.get('disabledStreams', []):
            failures.append('failed log sink was not retired')
    if mode == 'stdout-flush-failure':
        if not trace.get('flushSinkHits') or 'stdout' not in trace.get('disabledStreams', []):
            failures.append('failed flush sink was not retired')
    if not ('stderr-sink' in mode or mode == 'meta-failure'):
        code = 'META_FAILED' if is_meta else 'APPLICATION_FAILED'
        if code not in stderr or 'type=' not in stderr:
            failures.append('fixed failure diagnostic absent or wrong boundary')
    return {'name': 'cli-security-' + name, 'passed': not failures, 'failures': failures,
            'scope': 'real process and actual __main__ guard/shared CLI; controlled flow, NOT product runtime acceptance',
            'optimized': optimized, 'command': command, 'exitCode': child.returncode,
            'stdoutSentinelAbsent': not any(token in stdout for token in forbidden),
            'stderrSentinelAbsent': not any(token in stderr for token in forbidden),
            'artifactSentinelAbsent': not any(token.encode() in path.read_bytes() for token in forbidden for path in paths),
            'filesScanned': [str(path.relative_to(directory)) for path in paths], 'trace': trace,
            'finalArtifact': final}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--label', required=True)
    parser.add_argument('--only', choices=['cleanup', 'observer', 'acceptance'], help='run one scoped meta/control group')
    parser.add_argument('--report-only', action='store_true', help='scope acceptance to report publication controls')
    parser.add_argument('--game', type=Path, help='optional real EXE for audited preflight controls; never launched')
    parser.add_argument('--packer', type=Path, help='optional real packer for audited preflight controls; never launched')
    parser.add_argument('--worker-security', choices=CLI_CONTROLS, help=argparse.SUPPRESS)
    parser.add_argument('--worker-preflight', choices=['missing-fixture', 'nonexistent-fixture'], help=argparse.SUPPRESS)
    parser.add_argument('--worker-control', choices=sorted({x[0] for x in CONTROLS}), help=argparse.SUPPRESS)
    parser.add_argument('--case', choices=['first-handle', 'second-handle'], help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.report_only and args.only != 'acceptance':
        parser.error('--report-only requires --only acceptance')
    if bool(args.game) != bool(args.packer) or (args.worker_preflight and not args.game):
        parser.error('--game and --packer must be supplied together')
    work = args.scratch_root.resolve() / args.label
    work.mkdir(parents=True, exist_ok=False)
    if args.worker_security:
        if args.worker_security.startswith('meta-'):
            return meta_security_worker(work, args.worker_security)
        return application_acceptance_control(work, args.worker_security, entrypoint=True)
    if args.worker_preflight:
        record = real_application_preflight(work, args.worker_preflight, args.game, args.packer)
        (work / 'result.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
        return 0
    if args.worker_control:
        if (args.worker_control, args.case) not in CONTROLS:
            parser.error('unsupported internal control/case pair')
        return controlled_worker(work, args.worker_control, args.case)
    records = []
    for control, case in (CONTROLS if args.only in (None, 'cleanup') else []):
        name = control + '-' + case
        command = [sys.executable, str(Path(__file__).resolve()), '--scratch-root', str(work),
                   '--label', name, '--worker-control', control, '--case', case]
        env = os.environ.copy()
        env['PYTHONDONTWRITEBYTECODE'] = '1'
        for key in ('TMP', 'TEMP', 'TMPDIR'):
            env[key] = str(work)
        env['PATH'] = os.pathsep.join([os.environ.get('SystemRoot', r'C:\Windows') + r'\System32',
                                      os.environ.get('SystemRoot', r'C:\Windows'), r'C:\Program Files\dotnet'])
        result = subprocess.run(command, env=env, capture_output=True, timeout=45,
                                creationflags=subprocess.CREATE_NO_WINDOW)
        (work / (name + '.log')).write_bytes(result.stdout + result.stderr)
        failure = None
        record = trace = None
        try:
            record = json.loads((work / name / 'result.json').read_text(encoding='utf-8'))
            trace = json.loads((work / name / 'boundary.json').read_text(encoding='utf-8'))
            verify_rejection(record, trace, result.returncode, control)
        except (AssertionError, OSError, ValueError, KeyError) as exception:
            failure = {'code': 'META_CONTROL_FAILED', 'type': type(exception).__name__}
        records.append({'name': name, 'passed': failure is None, 'failure': failure,
                        'expectedNegativeWorkerExit': True, 'exitCode': result.returncode,
                        'workerRecord': record, 'boundary': trace, 'command': command})
        print(name, 'exit=', result.returncode, 'metaFailure=', failure)
    if args.only in (None, 'observer'):
        observer_cases = [('transient-' + root, observer_activity_control, root)
                          for root in ('app', 'cwd', 'tmp')]
        observer_cases += [(mode, observer_health_control, mode) for mode in
                           ('clean', 'partial-startup', 'overflow', 'malformed', 'health-timeout',
                            'sensitive-startup', 'sensitive-health', 'sensitive-drain')]
        observer_cases += [('exact-health-token', observer_exact_token_control, None)]
        for name, control, parameter in observer_cases:
            directory = work / ('observer-' + name)
            directory.mkdir()
            try:
                record = control(directory, parameter) if parameter is not None else control(directory)
            except Exception as exception:
                record = {'name': 'observer-' + name, 'passed': False,
                          'failure': {'code': 'META_CONTROL_FAILED', 'type': type(exception).__name__}}
            (directory / 'result.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
            records.append(record)
            print(record['name'], 'passed=', record['passed'], record.get('failure'))
    if args.only in (None, 'acceptance'):
        for mode in (('report-update-success',) if args.report_only else ACCEPTANCE_CONTROLS):
            directory = work / ('application-' + mode)
            directory.mkdir()
            try:
                record = application_acceptance_control(directory, mode)
            except Exception as exception:
                record = {'name': 'application-' + mode, 'passed': False,
                          'failure': {'code': 'META_CONTROL_FAILED', 'type': type(exception).__name__}}
            (directory / 'result.json').write_text(json.dumps(record, indent=2), encoding='utf-8')
            records.append(record)
            print(record['name'], 'passed=', record['passed'], record.get('failure'))
        for mode in (REPORT_CONTROLS if args.report_only else CLI_CONTROLS):
            for optimized in (False, True):
                record = cli_security_control(work, mode, optimized)
                records.append(record)
                print(record['name'], 'passed=', record['passed'], record['failures'])
        if args.game:
            for mode in ('missing-fixture', 'nonexistent-fixture'):
                for optimized in (False, True):
                    name = 'real-preflight-' + mode + ('-optimized' if optimized else '-normal')
                    command = [sys.executable, '-B', *(['-O'] if optimized else []),
                               str(Path(__file__).resolve()), '--scratch-root', str(work), '--label', name,
                               '--worker-preflight', mode, '--game', str(args.game), '--packer', str(args.packer)]
                    env = os.environ.copy()
                    for key in ('TMP', 'TEMP', 'TMPDIR'):
                        env[key] = str(work)
                    child = subprocess.run(command, env=env, capture_output=True, timeout=20,
                                           creationflags=subprocess.CREATE_NO_WINDOW)
                    (work / (name + '.log')).write_bytes(child.stdout + child.stderr)
                    path = work / name / 'result.json'
                    record = json.loads(path.read_text(encoding='utf-8')) if path.is_file() else {
                        'name': name, 'passed': False, 'failure': 'preflight worker did not produce a result'}
                    record.update(command=command, exitCode=child.returncode,
                                  passed=record['passed'] and child.returncode == 0)
                    records.append(record)
                    print(name, 'passed=', record['passed'])
    summary = {'passed': all(r['passed'] for r in records), 'recordCount': len(records),
               'records': records, 'workerSha256': hashlib.sha256(TARGET.read_bytes()).hexdigest(),
               'metaSha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    observer_path = Path(__file__).with_name('application_observers.py')
    summary['observerSha256'] = hashlib.sha256(observer_path.read_bytes()).hexdigest()
    from test_game_data_application import publish_report
    publish_report(work / 'results.json', summary,
                   {'directory': str(work), 'recordCount': len(records), 'passed': summary['passed']})
    return int(not summary['passed'])


def cli():
    from test_game_data_application import cli as application_cli
    return application_cli(main_function=main, failure_code='META_FAILED')


if __name__ == '__main__':
    raise SystemExit(cli())
