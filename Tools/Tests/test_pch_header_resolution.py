"""평가된 VS 프로젝트 검색 경로로 실제 소스를 PCH 없이 해석하는 회귀 검사."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--configuration', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--platform', choices=['x64', 'Win32'], default='x64')
    args = parser.parse_args()
    args.scratch_root.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix='pch-resolution-', dir=args.scratch_root.resolve()))
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
    installation = Path(subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], encoding='utf-8-sig').strip())
    msbuild = installation / 'MSBuild/Current/Bin/MSBuild.exe'
    command = [str(msbuild), str(ROOT / 'TextRPG/TextRPG.vcxproj'),
        f'-p:Configuration={args.configuration}', f'-p:Platform={args.platform}', '-getItem:ClCompile']
    normal = json.loads(subprocess.check_output(command, encoding='utf-8-sig'))['Items']['ClCompile']
    design = json.loads(subprocess.check_output(command + ['-p:DesignTimeBuild=true', '-p:BuildingInsideVisualStudio=true'],
                                               encoding='utf-8-sig'))['Items']['ClCompile']
    (work / 'evaluated-items.json').write_text(json.dumps({'build': normal, 'designTime': design}, indent=2), encoding='utf-8')
    sources = [item for item in normal if item.get('PrecompiledHeader') == 'Use']
    assert sources and any(Path(item['Identity']).parent != Path('.') for item in sources)
    by_name = {item['Identity']: item for item in design}
    lines = ['@echo off', f'call "{installation / "VC/Auxiliary/Build/vcvarsall.bat"}" {"x86" if args.platform == "Win32" else "x64"}',
             'if errorlevel 1 exit /b %errorlevel%']
    for index, item in enumerate(sources):
        for key in ['AdditionalIncludeDirectories', 'PreprocessorDefinitions']:
            assert item.get(key) == by_name[item['Identity']].get(key), (item['Identity'], key)
        flags = ['/nologo', '/std:c++20', '/utf-8', '/EHsc', '/W3', '/WX', '/Zs', '/Y-']
        for path in item.get('AdditionalIncludeDirectories', '').split(';'):
            if path:
                flags.append(f'/I"{path.rstrip(chr(92))}"')
        for definition in item.get('PreprocessorDefinitions', '').split(';'):
            if definition:
                flags.append(f'/D"{definition}"')
        flags.append(f'"{item["FullPath"]}"')
        response = work / f'compile-{index}.rsp'
        response.write_text('\n'.join(flags), encoding='utf-8')
        lines += [f'cl @"{response}"', 'if errorlevel 1 exit /b %errorlevel%']
    (work / 'compile.cmd').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    result = subprocess.run([os.environ.get('COMSPEC', 'cmd.exe'), '/d', '/c', str(work / 'compile.cmd')],
                            cwd=work, capture_output=True, timeout=120)
    (work / 'compile.log').write_bytes(result.stdout + result.stderr)
    summary = {'passed': result.returncode == 0, 'configuration': args.configuration, 'platform': args.platform,
               'translationUnits': len(sources), 'mode': 'PCH disabled; evaluated build/design-time paths equal',
               'exit': result.returncode, 'logDirectory': str(work)}
    (work / 'results.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary))
    if result.returncode:
        print((result.stdout + result.stderr).decode('utf-8', errors='replace'))
    raise SystemExit(result.returncode)


if __name__ == '__main__':
    main()
