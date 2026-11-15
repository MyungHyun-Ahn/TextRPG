"""경로 해석기의 스택 사용량 분석 및 실제 환경 변수/부모 탐색 회귀."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

PROBE = r'''#include "pch.h"
#include "System/GameDataPaths.h"
int wmain(int argc, wchar_t** argv)
{
    if (argc != 3) return 10;
    std::filesystem::path result = L"unchanged";
    std::wstring error;
    const bool success = TextRPG::ResolveGameDataRoot(result, error);
    if (std::wstring(argv[1]) == L"fail")
        return !success && !error.empty() && result == L"unchanged" ? 0 : 1;
    return success && error.empty() && result == std::filesystem::weakly_canonical(argv[2]) ? 0 : 2;
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--source-root', type=Path, default=Path(__file__).resolve().parents[2] / 'TextRPG')
    args = parser.parse_args()
    args.scratch_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='path-analysis-', dir=args.scratch_root.resolve()))
    source = args.source_root.resolve(strict=True)
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    installation = Path(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], encoding='utf-8-sig').strip())
    for folder in ['GameData/Asset', 'Override/Asset', '한글경로/Asset', 'cwd']:
        (work / folder).mkdir(parents=True, exist_ok=True)
    cases = [
        ('parent-search', None, True, work / 'GameData'),
        ('absolute-override', str(work / 'Override'), True, work / 'Override'),
        ('relative-to-executable', '../Override', True, work / 'Override'),
        ('missing-asset-rejected', str(work / 'missing'), False, work / 'missing'),
        ('empty-override-parent-search', '', True, work / 'GameData'),
        ('unicode-override', str(work / '한글경로'), True, work / '한글경로'),
    ]
    results = []
    for arch in ['x64', 'x86']:
        directory = work / arch
        directory.mkdir()
        (directory / 'Probe.cpp').write_text(PROBE, encoding='utf-8')
        batch = directory / 'compile.cmd'
        batch.write_text(f'@echo off\ncall "{installation / "VC/Auxiliary/Build/vcvarsall.bat"}" {arch}\n'
            'if errorlevel 1 exit /b %errorlevel%\n'
            f'cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /analyze /sdl /Y- /DUNICODE /D_UNICODE /I"{source}" /I"{source.parent / "ConsoleGameCore"}" /I"{source.parent / "GameData/Runtime"}" '
            f'Probe.cpp "{source / "System/GameDataPaths.cpp"}" /Fe:Probe.exe\nexit /b %errorlevel%\n', encoding='utf-8')
        result = subprocess.run([os.environ.get('COMSPEC', 'cmd.exe'), '/d', '/c', str(batch)],
                                cwd=directory, capture_output=True, timeout=120)
        (directory / 'compile.log').write_bytes(result.stdout + result.stderr)
        results.append({'arch': arch, 'case': 'analyze-compile', 'exit': result.returncode})
        if result.returncode:
            continue
        for name, override, success, expected in cases:
            env = os.environ.copy()
            env.pop('TEXTRPG_GAMEDATA_ROOT', None)
            if override is not None:
                env['TEXTRPG_GAMEDATA_ROOT'] = override
            result = subprocess.run([str(directory / 'Probe.exe'), 'success' if success else 'fail', str(expected)],
                                    cwd=work / 'cwd', env=env, capture_output=True, timeout=15)
            results.append({'arch': arch, 'case': name, 'exit': result.returncode})
    passed = len(results) == 14 and all(item['exit'] == 0 for item in results)
    summary = {'passed': passed, 'results': results, 'logDirectory': str(work)}
    (work / 'results.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))
    raise SystemExit(0 if passed else 1)


if __name__ == '__main__':
    main()
