"""Isolated real-store package-reader exception contract regression (no path mutations)."""
import argparse
import hashlib
from pathlib import Path
import subprocess
from native_build import (ROOT, build_native_test, run_directory, native_report, native_cli,
                          native_contract_sources, generated_source_paths)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--architecture', choices=['x64', 'x86'], required=True)
    parser.add_argument('--configuration', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--label', required=True)
    parser.add_argument('--only', choices=['bad-alloc-empty'])
    parser.add_argument('--mode', choices=['normal', 'analyze', 'both'], default='both')
    args = parser.parse_args()
    work = run_directory(args.scratch_root, args.label)
    report = {'passed': False, 'architecture': args.architecture, 'configuration': args.configuration, 'records': []}
    with native_report(work, report, lambda: {'directory': str(work), 'recordCount': len(report['records']),
                                             'passed': report['passed']}):
        sources = [ROOT / 'Tests/SceneUiRuntime/GameDataStoreExceptions.cpp']
        sources += [ROOT / 'GameData/Runtime/GameData' / name for name in
                    ('FGameDataPackage.cpp', 'FGeneratedYaml.cpp', 'FGameDataStore.cpp')]
        sources += [ROOT / 'GameData/Runtime/GameData/GameDataPch.h']
        sources += native_contract_sources('GameDataStoreExceptions')
        hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(set(sources))}
        report['sourceSha256'] = hashes
        records = report['records']
        for mode in (('normal', 'analyze') if args.mode == 'both' else (args.mode,)):
            out = work / mode
            out.mkdir()
            with build_native_test('GameDataStoreExceptions', args.architecture, args.configuration, out,
                                   analyze=mode == 'analyze') as (build, env):
                clean = build['compileExit'] == 0 and build['warnings'] == 0
                records.append(dict(build, name=mode + '-compile', command=build['buildCommand'],
                                    exitCode=build['compileExit'], passed=clean))
                if not clean:
                    print((out / 'compile.log').read_text(encoding='utf-8', errors='replace'))
                    continue
                cases = [(1, 'empty')] if args.only else [(fault, state) for fault in (1, 2) for state in ('empty', 'populated')]
                for fault, state in cases:
                    name = f'{mode}-fault{fault}-{state}'
                    command = [build['executable'], str(ROOT / 'GameData'), str(out / name), str(fault), state]
                    try:
                        executed = subprocess.run(command, cwd=out, env=env, capture_output=True, timeout=60)
                        output = executed.stdout + executed.stderr
                        exit_code = executed.returncode
                    except subprocess.TimeoutExpired as error:
                        output = (error.stdout or b'') + (error.stderr or b'') + b'\nFAIL native test timed out after 60 seconds\n'
                        exit_code = 124
                    groups = sum(line.startswith(b'PASS ') for line in output.splitlines())
                    records.append({'name': name, 'command': command, 'exitCode': exit_code,
                                    'passCheckpointGroups': groups, 'passed': exit_code == 0 and groups > 0})
                    (out / (name + '.log')).write_bytes(output)
                    print(name, 'exit=', exit_code, output.decode('utf-8', errors='replace').strip())
        drift = [name for name, digest in hashes.items() if hashlib.sha256((ROOT / name).read_bytes()).hexdigest() != digest]
        report['generatedSourceSha256'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                                          for path in generated_source_paths('GameDataStoreExceptions')}
        report.update(passed=bool(records) and all(record['passed'] for record in records) and not drift,
                      recordCount=len(records), sourceDrift=drift)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(native_cli(main))
