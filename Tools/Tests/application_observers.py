"""Fail-closed Windows directory activity observation for owned application fixtures.

This is a shared helper, not an executable runner or a process-wide sandbox.
The caller must fully reap its child before leaving the context. Kernel directory
notifications, not final inventories, cover each supplied root and its subtree.
"""
import ctypes as c
from ctypes import wintypes as w
import math
import os
from pathlib import Path
import struct
import time
import uuid


class _Offsets(c.Structure):
    _fields_ = [('Offset', w.DWORD), ('OffsetHigh', w.DWORD)]


class _Position(c.Union):
    _fields_ = [('offsets', _Offsets), ('Pointer', c.c_void_p)]


class _Overlapped(c.Structure):
    _fields_ = [('Internal', c.c_size_t), ('InternalHigh', c.c_size_t),
                ('position', _Position), ('hEvent', w.HANDLE)]


_ACTIONS = {1: 'ADDED', 2: 'REMOVED', 3: 'MODIFIED',
            4: 'RENAMED_OLD_NAME', 5: 'RENAMED_NEW_NAME'}
# 취소 완료가 확인되지 않으면 커널이 사용하는 buffer/OVERLAPPED를 해제하지 않는다.
# 이 예외적인 자원 보존은 정상 종료가 아닌 관찰 실패로 명시한다.
_UNDRAINED = []
_BUFFER_BYTES = 64 * 1024


class _ObserverFailure(ValueError):
    pass


class _ObserverTimeout(TimeoutError):
    pass


# 내부의 고정 reason만 공개하고 임의 예외의 message/chain은 기록하지 않는다.
_REASONS = frozenset((
    'zero-byte notification overflow/loss',
    'truncated FILE_NOTIFY_INFORMATION header',
    'invalid FILE_NOTIFY_INFORMATION action/name bounds',
    'invalid relative notification name',
    'unexpected notification trailing bytes',
    'invalid FILE_NOTIFY_INFORMATION next-entry bounds',
    'notification completion exceeds allocated buffer',
    'directory notification completion timeout',
    'post-child ADD+REMOVE health barrier timeout',
))


class _ObserverIOError(OSError):
    def __init__(self, stage, code, wait_result=None):
        native = c.WinError(code)
        super().__init__(native.errno, 'directory observation I/O failed')
        self.winerror = code
        self.stage = stage
        self.wait_result = wait_result


def _parse_notifications(data):
    """Validate every variable-length FILE_NOTIFY_INFORMATION before decoding."""
    if not data:
        raise _ObserverFailure('zero-byte notification overflow/loss')
    result = []
    offset = 0
    while True:
        if offset + 12 > len(data):
            raise _ObserverFailure('truncated FILE_NOTIFY_INFORMATION header')
        following, action, name_bytes = struct.unpack_from('<III', data, offset)
        end = offset + 12 + name_bytes
        if action not in _ACTIONS or not name_bytes or name_bytes % 2 or end > len(data):
            raise _ObserverFailure('invalid FILE_NOTIFY_INFORMATION action/name bounds')
        name = data[offset + 12:end].decode('utf-16-le', errors='strict')
        if '\x00' in name or name.startswith(('\\', '/')) or '..' in name.split('\\'):
            raise _ObserverFailure('invalid relative notification name')
        result.append((action, name))
        if not following:
            if len(data) - end > 3:
                raise _ObserverFailure('unexpected notification trailing bytes')
            return result
        if following % 4 or following < 12 + name_bytes or offset + following + 12 > len(data):
            raise _ObserverFailure('invalid FILE_NOTIFY_INFORMATION next-entry bounds')
        offset += following


def _kernel():
    k = c.WinDLL('kernel32', use_last_error=True)
    k.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p,
                             w.DWORD, w.DWORD, w.HANDLE]
    k.CreateFileW.restype = w.HANDLE
    k.CreateEventW.argtypes = [c.c_void_p, w.BOOL, w.BOOL, w.LPCWSTR]
    k.CreateEventW.restype = w.HANDLE
    k.CloseHandle.argtypes = [w.HANDLE]
    k.CloseHandle.restype = w.BOOL
    k.ResetEvent.argtypes = [w.HANDLE]
    k.ResetEvent.restype = w.BOOL
    k.ReadDirectoryChangesW.argtypes = [w.HANDLE, c.c_void_p, w.DWORD, w.BOOL,
                                      w.DWORD, c.POINTER(w.DWORD),
                                      c.POINTER(_Overlapped), c.c_void_p]
    k.ReadDirectoryChangesW.restype = w.BOOL
    k.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
    k.WaitForSingleObject.restype = w.DWORD
    k.GetOverlappedResult.argtypes = [w.HANDLE, c.POINTER(_Overlapped),
                                    c.POINTER(w.DWORD), w.BOOL]
    k.GetOverlappedResult.restype = w.BOOL
    k.CancelIoEx.argtypes = [w.HANDLE, c.POINTER(_Overlapped)]
    k.CancelIoEx.restype = w.BOOL
    return k


class FileActivityWatch:
    """Arm owned roots before spawn; positively drain them after child reap.

    Each root has a continuously allocated kernel change buffer from its first
    ReadDirectoryChangesW. A full buffer fails closed rather than claiming clean.
    Shutdown observes exact unique post-child marker ADD+REMOVE events before
    canceling/draining any outstanding read and releasing its backing memory.
    """
    def __init__(self, roots, timeout_seconds=5):
        if os.name != 'nt':
            raise OSError('FileActivityWatch requires Windows directory notifications')
        if not math.isfinite(timeout_seconds) or timeout_seconds <= 0:
            raise ValueError('timeout_seconds must be finite and positive')
        self.timeout_seconds = timeout_seconds
        self.roots = [Path(root).resolve() for root in roots]
        if not self.roots or any(a == b or a in b.parents or b in a.parents
                                 for index, a in enumerate(self.roots)
                                 for b in self.roots[index + 1:]):
            raise ValueError('watch roots must be nonempty, distinct and non-overlapping')
        self._k = _kernel()
        self._states = []
        self._events = []
        self._health = []
        self._errors = []
        self._entered = False
        self._finished = False
        self._ready = False

    def _error(self, state, operation, error):
        reason = 'observer operation failed'
        kind = type(error).__name__
        if type(error) in (_ObserverFailure, _ObserverTimeout):
            reason = error.args[0] if error.args and error.args[0] in _REASONS else reason
            kind = 'ValueError' if type(error) is _ObserverFailure else 'TimeoutError'
        record = {'root': str(state['root']) if state else None,
                  'operation': operation, 'error': reason, 'errorType': kind}
        if isinstance(error, OSError):
            for key in ('errno', 'winerror'):
                value = getattr(error, key, None)
                if type(value) is int:
                    record[key] = value
        if type(error) is _ObserverIOError:
            record.update(errorType='OSError', stage=error.stage)
            if error.wait_result is not None:
                record['waitResult'] = error.wait_result
        self._errors.append(record)

    def _arm(self, state):
        if not self._k.ResetEvent(state['event']):
            raise _ObserverIOError('ResetEvent', c.get_last_error())
        state['overlapped'] = _Overlapped()
        state['overlapped'].hEvent = state['event']
        # 비동기 호출에서는 lpBytesReturned가 완료 바이트 수가 아니므로 NULL을 전달한다.
        if not self._k.ReadDirectoryChangesW(state['handle'], state['buffer'], _BUFFER_BYTES,
                                            True, 0x15F, None,
                                            c.byref(state['overlapped']), None):
            raise _ObserverIOError('ReadDirectoryChangesW', c.get_last_error())
        state['pending'] = True
        state['armed'] = True
        state['readRequests'] += 1

    def __enter__(self):
        if self._entered:
            raise RuntimeError('FileActivityWatch cannot be reused')
        self._entered = True
        try:
            for root in self.roots:
                state = {'root': root, 'handle': None, 'event': None, 'pending': False,
                         'armed': False, 'closed': False, 'barrierComplete': False,
                         'marker': None, 'markerActions': set(), 'readRequests': 0,
                         'completions': 0, 'cancellationRequested': False,
                         'cancellationCompleted': False, 'resourcesRetained': False,
                         'buffer': (w.DWORD * (_BUFFER_BYTES // c.sizeof(w.DWORD)))()}
                self._states.append(state)
                state['handle'] = self._k.CreateFileW(str(root), 0x1, 0x7, None, 3,
                                                      0x42000000, None)
                if state['handle'] in (None, 0, c.c_void_p(-1).value):
                    state['handle'] = None
                    raise _ObserverIOError('CreateFileW', c.get_last_error())
                state['event'] = self._k.CreateEventW(None, True, False, None)
                if not state['event']:
                    raise _ObserverIOError('CreateEventW', c.get_last_error())
                self._arm(state)
            self._ready = True
            return self
        except Exception as exception:
            self._error(self._states[-1] if self._states else None, 'startup', exception)
            self._shutdown()
            self._finished = True
            raise AssertionError('file activity observer startup failed') from None

    def _consume(self, state, byte_count):
        if byte_count > _BUFFER_BYTES:
            raise _ObserverFailure('notification completion exceeds allocated buffer')
        data = c.string_at(c.addressof(state['buffer']), byte_count)
        for action, name in _parse_notifications(data):
            event = {'root': str(state['root']), 'relativePath': name,
                     'action': _ACTIONS[action], 'actionCode': action}
            # 직접 할당한 루트별 이름만 제외하며 같은 접두사의 임의 파일은 제외하지 않는다.
            if name == state['marker']:
                self._health.append(event)
                state['markerActions'].add(action)
                state['barrierComplete'] = {1, 2} <= state['markerActions']
            else:
                self._events.append(event)

    def _complete(self, state, seconds, cancelling=False):
        wait = self._k.WaitForSingleObject(state['event'], max(1, math.ceil(seconds * 1000)))
        if wait == 258:
            raise _ObserverTimeout('directory notification completion timeout')
        if wait != 0:
            raise _ObserverIOError('WaitForSingleObject', c.get_last_error(), wait)
        count = w.DWORD()
        success = self._k.GetOverlappedResult(state['handle'], c.byref(state['overlapped']),
                                             c.byref(count), False)
        error = 0 if success else c.get_last_error()
        if not success and error == 996:  # IO_INCOMPLETE이면 메모리 수명을 유지한다.
            raise _ObserverIOError('GetOverlappedResult incomplete', error)
        state['pending'] = False
        state['completions'] += 1
        if not success:
            if cancelling and error == 995:
                state['cancellationCompleted'] = True
                return
            raise _ObserverIOError('GetOverlappedResult', error)
        if cancelling:
            state['cancellationCompleted'] = True
        self._consume(state, count.value)

    def _make_marker(self, state):
        name = '.observer-health-' + uuid.uuid4().hex
        marker = state['root'] / name
        # 배타적 생성에 성공한 이름만 제외한다. 알림 소비는 같은 스레드에서 수행하므로
        # 이름 할당과 health 이벤트 분류 사이에 경쟁이 없다.
        with marker.open('xb'):
            state['marker'] = name
        try:
            marker.unlink()
        except OSError:
            # 정리 실패 시에도 정확한 marker 식별자를 보고서에 남긴다.
            raise

    def _shutdown(self):
        for state in self._states:
            if state['pending']:
                if not self._k.CancelIoEx(state['handle'], c.byref(state['overlapped'])):
                    error = c.get_last_error()
                    if error != 1168:  # 취소 전에 완료된 I/O는 ERROR_NOT_FOUND일 수 있다.
                        self._error(state, 'CancelIoEx', _ObserverIOError('CancelIoEx', error))
                else:
                    state['cancellationRequested'] = True
                try:
                    self._complete(state, self.timeout_seconds, cancelling=True)
                except Exception as exception:
                    self._error(state, 'cancellation completion', exception)
            if state['pending']:
                state['resourcesRetained'] = True
                _UNDRAINED.append((self._k, state))
                self._error(state, 'resource retention', RuntimeError(
                    'uncompleted I/O retained; OVERLAPPED/buffer/handles cannot safely be freed'))
                continue
            for key in ('event', 'handle'):
                if state[key]:
                    if not self._k.CloseHandle(state[key]):
                        self._error(state, 'CloseHandle ' + key, _ObserverIOError('CloseHandle', c.get_last_error()))
                    else:
                        state[key] = None
            state['closed'] = not state['handle'] and not state['event']

    def __exit__(self, exception_type, exception, traceback):
        try:
            # 호출자가 자식을 회수한 뒤에만 health 확인용 파일을 생성한다.
            for state in self._states:
                try:
                    self._make_marker(state)
                except Exception as marker_error:
                    self._error(state, 'post-child health marker', marker_error)
            for state in self._states:
                if not state['marker']:
                    continue
                deadline = time.monotonic() + self.timeout_seconds
                try:
                    while not state['barrierComplete']:
                        remaining = deadline - time.monotonic()
                        if remaining <= 0:
                            raise _ObserverTimeout('post-child ADD+REMOVE health barrier timeout')
                        self._complete(state, remaining)
                        self._arm(state)
                except Exception as drain_error:
                    self._error(state, 'post-child drain', drain_error)
        finally:
            self._shutdown()
            self._finished = True
        return False

    def assert_clean(self):
        report = self.report()
        if not (self._ready and self._finished):
            raise AssertionError('file activity observer was not armed and fully stopped')
        if report['errors']:
            raise AssertionError('file activity observer health failure')
        if not all(root['barrierComplete'] and root['closed'] for root in report['readiness']):
            raise AssertionError('file activity observer missing positive shutdown boundary or resource cleanup')
        if report['events']:
            raise AssertionError('unexpected file activity in owned roots')

    def report(self):
        return {'scope': 'supplied owned directory roots and subtrees; no process attribution',
                'roots': [str(root) for root in self.roots], 'ready': self._ready,
                'finished': self._finished,
                'readiness': [{key: (str(state[key]) if key == 'root' else state[key])
                               for key in ('root', 'armed', 'closed', 'barrierComplete',
                                           'marker', 'readRequests', 'completions',
                                           'cancellationRequested', 'cancellationCompleted',
                                           'resourcesRetained')} for state in self._states],
                'events': [dict(event) for event in self._events],
                'healthEvents': [dict(event) for event in self._health],
                'errors': [dict(error) for error in self._errors]}
