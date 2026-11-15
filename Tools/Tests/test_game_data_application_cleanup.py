"""Exercise console_smoke cleanup with owned harmless Windows children and boundary faults."""
import argparse
import ctypes as c
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
TARGET = ROOT / 'Tools/Tests/test_game_data_application.py'
CASES = ['attach', 'first-handle', 'second-handle', 'cleanup-key', 'cleanup-wait']


class Function:
    def __init__(self, callback):
        self.callback = callback

    def __call__(self, *args):
        return self.callback(*args)


def worker(work, case):
    spec = importlib.util.spec_from_file_location('application_under_test', TARGET)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    real_kernel = c.WinDLL('kernel32', use_last_error=True)
    real_kernel.CreateFileW.argtypes = [c.c_wchar_p, c.c_ulong, c.c_ulong, c.c_void_p, c.c_ulong, c.c_ulong, c.c_void_p]
    real_kernel.CreateFileW.restype = c.c_void_p
    real_kernel.CloseHandle.argtypes = [c.c_void_p]
    real_kernel.WriteConsoleInputW.argtypes = [c.c_void_p, c.POINTER(module.Record), c.c_ulong, c.POINTER(c.c_ulong)]
    real_popen = subprocess.Popen
    child_script = work / 'harmless_child.py'
    child_script.write_text('import time\ntime.sleep(120)\n', encoding='utf-8')
    children = []
    opened = []
    closed = []
    calls = {'attach': 0, 'attachSuccess': 0, 'create': 0, 'key': 0, 'keySuccess': 0,
             'bufferFault': 0, 'waitFault': 0, 'free': 0}
    fault_hits = []
    create_names = []
    ownership_errors = []

    def require_owned(condition, message):
        if not condition:
            ownership_errors.append(message)
            raise AssertionError(message)

    class Process:
        def __init__(self, process):
            self.process = process
            self.pid = process.pid

        def poll(self):
            return self.process.poll()

        def terminate(self):
            return self.process.terminate()

        def wait(self, timeout):
            if case == 'cleanup-wait' and calls['waitFault'] == 0:
                calls['waitFault'] += 1
                fault_hits.append(case)
                raise OSError(6, 'injected owned process wait failure')
            return self.process.wait(timeout=timeout)

    def launch(command, **kwargs):
        require_owned(command == [str(sys.executable)], 'unexpected child command')
        require_owned(kwargs.get('creationflags') == subprocess.CREATE_NEW_CONSOLE,
                      'owned child requires a new console')
        process = real_popen([sys.executable, str(child_script)], **kwargs)
        children.append(process)
        return Process(process)

    def attach(pid):
        require_owned(len(children) == 1 and pid == children[0].pid, 'never attach a non-owned PID')
        calls['attach'] += 1
        if case == 'attach':
            fault_hits.append(case)
            c.set_last_error(5)
            return 0
        result = real_kernel.AttachConsole(pid)
        calls['attachSuccess'] += int(bool(result))
        return result

    def create(*args):
        calls['create'] += 1
        create_names.append(args[0])
        require_owned(create_names == ['CONOUT$', 'CONIN$'][:calls['create']]
                      and calls['create'] <= 2, 'unexpected console device acquisition')
        if (case == 'first-handle' and calls['create'] == 1) or (case == 'second-handle' and calls['create'] == 2):
            fault_hits.append(case)
            c.set_last_error(6)
            return c.c_void_p(-1).value
        handle = real_kernel.CreateFileW(*args)
        if handle not in (None, 0, c.c_void_p(-1).value):
            opened.append(handle)
        return handle

    def close(handle):
        require_owned(handle in opened and handle not in closed, 'closing non-owned or already closed handle')
        result = real_kernel.CloseHandle(handle)
        if result:
            closed.append(handle)
        return result

    def free():
        calls['free'] += 1
        return real_kernel.FreeConsole()

    def fail_buffer(*_):
        # 두 핸들 획득 뒤 본문 실패를 만들고 실제 finally를 실행한다.
        require_owned(len(opened) == 2 and _[0] == opened[0], 'buffer used without owned output handle')
        calls['bufferFault'] += 1
        c.set_last_error(87)
        return 0

    def key(*_):
        calls['key'] += 1
        require_owned(len(opened) == 2 and _[0] == opened[1], 'key used without valid owned input handle')
        require_owned(_[1][0].event.key.vk == _[1][1].event.key.vk == 0x1B,
                      'unexpected non-Escape cleanup input')
        if case == 'cleanup-key':
            fault_hits.append(case)
            c.set_last_error(6)
            return 0
        result = real_kernel.WriteConsoleInputW(*_)
        calls['keySuccess'] += int(bool(result))
        return result

    class Kernel:
        def __getattr__(self, name):
            replacements = {'AttachConsole': attach, 'CreateFileW': create, 'CloseHandle': close,
                            'FreeConsole': free, 'SetConsoleScreenBufferSize': fail_buffer,
                            'WriteConsoleInputW': key}
            value = Function(replacements[name]) if name in replacements else getattr(real_kernel, name)
            setattr(self, name, value)
            return value

    failure = None
    observed_reaped = False
    external_cleanup = False
    error = None
    error_identity = None
    try:
        with patch.object(module.c, 'WinDLL', return_value=Kernel()), patch.object(module.subprocess, 'Popen', side_effect=launch):
            try:
                module.console_smoke(sys.executable, work, os.environ.copy(), work)
            except OSError as exception:
                error = f'{type(exception).__name__}: {exception}'
                error_identity = {'type': type(exception).__name__, 'errno': exception.errno,
                                  'label': exception.strerror}
        assert len(children) == 1, 'expected exactly one owned child'
        observed_reaped = children[0].returncode is not None and children[0].poll() is not None
        assert observed_reaped, 'owned child leaked from console_smoke cleanup'
        assert not ownership_errors, f'ownership assertion failed: {ownership_errors}'
        assert sorted(opened) == sorted(closed), 'owned console handles leaked'
        assert calls['free'] == 2, 'expected initial and final console detach'
        assert fault_hits, 'intended fault was not reached'
        assert fault_hits == [case] * (60 if case == 'attach' else 1), 'unexpected intended fault count'
        expected_create, expected_acquired, expected_errno, expected_label = {
            'attach': (0, 0, 5, 'owned console attach'),
            'first-handle': (1, 0, 6, 'owned CONOUT$'),
            'second-handle': (2, 1, 6, 'owned CONIN$'),
            'cleanup-key': (2, 2, 6, 'owned console key'),
            'cleanup-wait': (2, 2, 6, 'injected owned process wait failure'),
        }[case]
        assert calls['attachSuccess'] == (0 if case == 'attach' else 1), 'required real attach did not succeed'
        assert 1 <= calls['attach'] <= 60 and (case != 'attach' or calls['attach'] == 60)
        assert calls['create'] == expected_create, 'unexpected CreateFileW call count'
        assert create_names == ['CONOUT$', 'CONIN$'][:expected_create], 'unexpected CreateFileW sequence'
        assert len(opened) == len(set(opened)) == len(closed) == expected_acquired, 'unexpected acquired/closed handle count'
        has_input = expected_acquired == 2
        assert calls['key'] == int(has_input), 'Escape count must match owned input availability'
        assert calls['keySuccess'] == int(case == 'cleanup-wait'), 'unexpected cleanup input result'
        assert calls['bufferFault'] == int(has_input), 'intended body failure was not reached'
        assert calls['waitFault'] == int(case == 'cleanup-wait'), 'unexpected cleanup wait fault count'
        assert error_identity == {'type': 'OSError', 'errno': expected_errno, 'label': expected_label}, \
            f'unexpected error identity: {error_identity}'
    except Exception as exception:
        failure = str(exception)
    finally:
        # RED 경로도 소유한 자식만 외부 안전망으로 반드시 회수한다.
        for child in children:
            try:
                if child.poll() is None:
                    external_cleanup = True
                    child.terminate()
            finally:
                child.wait(timeout=10)
        for handle in opened:
            if handle not in closed:
                real_kernel.CloseHandle(handle)
        real_kernel.FreeConsole()
    result = {'name': case, 'passed': failure is None, 'failure': failure, 'injectedError': error,
              'errorIdentity': error_identity, 'faultHit': len(fault_hits), 'createNames': create_names,
              'ownershipErrors': ownership_errors,
              'ownedPids': [child.pid for child in children], 'allReapedAfterExternalSafetyNet':
              all(child.returncode is not None for child in children), 'reapedBySmoke': observed_reaped,
              'externalCleanupNeeded': external_cleanup, 'openedHandles': len(opened), 'closedBySmoke': len(closed),
              'calls': calls, 'sourceSha256': hashlib.sha256(TARGET.read_bytes()).hexdigest()}
    (work / 'result.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(json.dumps(result))
    return int(failure is not None)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--label', required=True)
    parser.add_argument('--only', choices=CASES)
    parser.add_argument('--worker', choices=CASES, help=argparse.SUPPRESS)
    args = parser.parse_args()
    work = args.scratch_root.resolve() / args.label
    work.mkdir(parents=True, exist_ok=False)
    if args.worker:
        return worker(work, args.worker)
    records = []
    for case in ([args.only] if args.only else CASES):
        command = [sys.executable, str(Path(__file__).resolve()), '--scratch-root', str(work),
                   '--label', case, '--worker', case]
        env = os.environ.copy()
        for name in ('TMP', 'TEMP', 'TMPDIR'):
            env[name] = str(work)
        # 작업자는 사용자 콘솔을 상속하지 않으며 소유 자식 콘솔에만 붙는다.
        result = subprocess.run(command, env=env, capture_output=True, timeout=45, creationflags=subprocess.CREATE_NO_WINDOW)
        (work / (case + '.log')).write_bytes(result.stdout + result.stderr)
        result_path = work / case / 'result.json'
        if not result_path.is_file():
            raise RuntimeError(f'worker failed without result: {case}: {result.stdout!r} {result.stderr!r}')
        record = json.loads(result_path.read_text(encoding='utf-8'))
        record['command'] = command
        record['exitCode'] = result.returncode
        record['passed'] = record['passed'] and result.returncode == 0
        records.append(record)
        print(case, 'exit=', result.returncode, record['failure'])
    summary = {'passed': all(record['passed'] for record in records), 'recordCount': len(records), 'records': records}
    (work / 'results.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps({'directory': str(work), 'recordCount': len(records), 'passed': summary['passed']}))
    return int(not summary['passed'])


if __name__ == '__main__':
    raise SystemExit(main())
