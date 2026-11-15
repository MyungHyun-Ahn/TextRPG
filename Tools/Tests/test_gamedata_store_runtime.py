"""Build and run native authenticated GameData semantic tests incrementally."""
import argparse
import hashlib
from pathlib import Path
import subprocess
from native_build import ROOT, build_native_test, run_directory, native_report, native_cli


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--suite', choices=['GameDataStore', 'SaveData', 'InGameData'], default='GameDataStore')
    parser.add_argument('--architecture', choices=['x64', 'x86'], default='x64')
    parser.add_argument('--configuration', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--label', default='store')
    parser.add_argument('--generated-root', type=Path)
    parser.add_argument('--data-root', type=Path)
    parser.add_argument('--no-analyze', action='store_true', help='Skip the default static-analysis build')
    args = parser.parse_args()
    work = run_directory(args.scratch_root, args.label)
    result = dict(passed=False, suite=args.suite, architecture=args.architecture, configuration=args.configuration)
    summary_keys = ('passed', 'compileExit', 'runExit', 'passCheckpointGroups', 'buildSeconds',
                    'compiledObjects', 'pchRebuilt', 'analysisObserved')
    with native_report(work, result, lambda: {key: result[key] for key in summary_keys}):
        sources = [ROOT / 'Tests/SceneUiRuntime/GameDataStore.cpp']
        sources += [ROOT / 'GameData/Runtime/GameData' / name for name in
                    ('FGameDataPackage.cpp', 'FGameDataStore.cpp', 'FGeneratedYaml.cpp')]
        if args.suite == 'SaveData':
            sources = [ROOT / name for name in ('Tests/SaveData/Main.cpp', 'TextRPG/InGameData/FSaveDataStore.cpp',
                                               'TextRPG/InGameData/FSaveDataStore.h', 'GameData/Runtime/GameData/pch.h',
                                               'GameData/Runtime/GameData/pch.cpp', 'NativeTests.vcxproj', 'NativeTests.vcxproj.filters',
                                               'Tools/Tests/native_build.py', 'Tools/Tests/test_gamedata_store_runtime.py')]
        if args.suite == 'InGameData':
            sources = [ROOT / 'Tests/InGameData/Main.cpp', ROOT / 'TextRPG/InGameData/InGameData.cpp',
                       ROOT / 'TextRPG/InGameData/InGameData.h'] + sources[1:]
        sources += [ROOT / 'GameData/Runtime/GameData/GameDataPch.h']
        hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
        result['sourceSha256'] = hashes
        with build_native_test(args.suite, args.architecture, args.configuration, work,
                               generated_root=args.generated_root, analyze=not args.no_analyze) as (build, env):
            result.update(build, runExit=None, passCheckpointGroups=0)
            if build['compileExit'] == 0 and build['warnings'] == 0:
                command = [build['executable'], str((args.data_root or ROOT / 'GameData').resolve()), str(work)]
                if args.suite == 'SaveData':
                    command = [build['executable'], str(work / 'service')]
                    env['TEXT_RPG_SAVE_ROOT'] = str(work / 'service')
                    env['LOCALAPPDATA'] = str(work / 'local-app-data')
                    env['TEXT_RPG_GAMEDATA_KEY'] = 'invalid-content-key-must-not-be-consumed'
                result['runCommand'] = command
                try:
                    executed = subprocess.run(command, cwd=work, env=env, capture_output=True, timeout=60)
                    output = executed.stdout + executed.stderr
                    result['runExit'] = executed.returncode
                except subprocess.TimeoutExpired as error:
                    output = (error.stdout or b'') + (error.stderr or b'') + b'\nFAIL native test timed out after 60 seconds\n'
                    result['runExit'] = 124
                result['passCheckpointGroups'] = sum(line.startswith(b'PASS ') for line in output.splitlines())
                if args.suite == 'SaveData':
                    result['checkpoints'] = [line.decode('utf-8', errors='replace') for line in output.splitlines()
                                             if line.startswith(b'PASS ') or line.startswith(b'FAIL ')]
                (work / 'run.log').write_bytes(output)
                print(output.decode('utf-8', errors='replace'))
            else:
                print((work / 'compile.log').read_text(encoding='utf-8', errors='replace'))
        # build lock 해제와 원본 drift 검사도 전체 성공 게시보다 먼저 끝난다.
        drift = [name for name, digest in hashes.items() if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest]
        result['sourceDrift'] = drift
        result['passed'] = (result['compileExit'] == 0 and result['warnings'] == 0 and result['runExit'] == 0
                            and result['passCheckpointGroups'] > 0 and not drift)
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(native_cli(main))
