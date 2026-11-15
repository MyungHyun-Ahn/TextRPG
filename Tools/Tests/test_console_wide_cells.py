"""Verify production wide-cell rendering in an owned native Windows console."""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--label', default='run')
    args = parser.parse_args()
    work = args.scratch_root.resolve() / args.label
    work.mkdir(parents=True, exist_ok=True)
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    installation = Path(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], encoding='utf-8-sig').strip())
    sources = [ROOT / p for p in ['Tests/ConsoleWideCells/Main.cpp',
        'ConsoleGameCore/Rendering/FFrameBuffer.cpp', 'ConsoleGameCore/Rendering/FConsoleRenderer.cpp']]
    flags = ['/nologo', '/std:c++20', '/utf-8', '/EHsc', '/W3', '/WX', '/Y-', '/analyze',
        f'/I"{ROOT / "ConsoleGameCore"}"']
    (work / 'compile.rsp').write_text('\n'.join(flags + [f'"{p}"' for p in sources] +
        [f'/Fe"{work / "probe.exe"}"']), encoding='utf-8')
    (work / 'compile.cmd').write_text('\n'.join(['@echo off',
        f'call "{installation / "VC/Auxiliary/Build/vcvarsall.bat"}" x64',
        'if errorlevel 1 exit /b %errorlevel%', f'cl @"{work / "compile.rsp"}"']) + '\n', encoding='utf-8')
    result = subprocess.run([os.environ.get('COMSPEC', 'cmd.exe'), '/d', '/c', str(work / 'compile.cmd')],
        cwd=work, capture_output=True, timeout=180)
    (work / 'compile.log').write_bytes(result.stdout + result.stderr)
    if result.returncode:
        print((result.stdout + result.stderr).decode('utf-8', errors='replace'))
        raise SystemExit(result.returncode)
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = subprocess.SW_HIDE
    result = subprocess.run([str(work / 'probe.exe'), str(work / 'run.log')], cwd=work,
        creationflags=subprocess.CREATE_NEW_CONSOLE, startupinfo=startup, timeout=30)
    print(str(work), 'exit=', result.returncode)
    if (work / 'run.log').exists():
        print((work / 'run.log').read_text(encoding='utf-8'))
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
