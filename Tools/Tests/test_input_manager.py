"""Build production input tests incrementally with MSVC/PCH and run isolated checks."""
import argparse
import json
from pathlib import Path
import subprocess
from native_build import build_native_test, run_directory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--label', default='run')
    parser.add_argument('--arch', choices=['x64', 'x86', 'both'], default='both')
    parser.add_argument('--configuration', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--analyze', action='store_true', help='Use the separate static-analysis build cache')
    args = parser.parse_args()
    work = run_directory(args.scratch_root, args.label)
    results = []
    for arch in (['x64', 'x86'] if args.arch == 'both' else [args.arch]):
        out = work / arch
        out.mkdir()
        with build_native_test('InputManager', arch, args.configuration, out, analyze=args.analyze) as (build, env):
            entry = dict(build, arch=arch, configuration=args.configuration, runExit=None, checks=0)
            if build['compileExit'] == 0 and build['warnings'] == 0:
                startup = subprocess.STARTUPINFO()
                startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
                startup.wShowWindow = subprocess.SW_HIDE
                executed = subprocess.run([build['executable']], cwd=out, env=env, capture_output=True, timeout=30,
                                          creationflags=subprocess.CREATE_NEW_CONSOLE, startupinfo=startup)
                output = executed.stdout + executed.stderr
                (out / 'run.log').write_bytes(output)
                entry['runExit'] = executed.returncode
                import re
                count = re.search(rb'checks=([0-9]+)', output)
                entry['checks'] = int(count.group(1)) if count else 0
                print(output.decode('utf-8', errors='replace'))
            else:
                print((out / 'compile.log').read_text(encoding='utf-8', errors='replace'))
            results.append(entry)
    (work / 'results.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
    print(json.dumps({'directory': str(work), 'results': results}, indent=2))
    raise SystemExit(0 if all(e['compileExit'] == 0 and e['warnings'] == 0 and e['runExit'] == 0 and e['checks'] > 0 for e in results) else 1)


if __name__ == '__main__':
    main()
