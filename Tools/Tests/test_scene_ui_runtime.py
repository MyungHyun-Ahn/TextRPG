"""Build the real Scene/UI tests incrementally, then execute the existing fixture."""
import argparse
import hashlib
from pathlib import Path
import subprocess
from native_build import (ROOT, build_native_test, run_directory, core_dependency_verdict,
                          native_report, write_native_report, native_cli, native_contract_sources,
                          generated_source_paths)


def _core_positive_control(args, work):
    out = work / 'standalone-control'
    out.mkdir()
    with build_native_test('ConsoleGameCore', args.architecture, args.configuration, out,
                           analyze=args.analyze) as (build, env):
        result = dict(build, scope='stage-observation', runExit=None, passCheckpointGroups=0)
        if build['compileExit'] == 0 and build['warnings'] == 0:
            command = [build['executable']]
            result['runCommand'] = command
            try:
                executed = subprocess.run(command, cwd=out, env=env, capture_output=True, timeout=30)
                output = executed.stdout + executed.stderr
                result['runExit'] = executed.returncode
            except subprocess.TimeoutExpired as failure:
                output = (failure.stdout or b'') + (failure.stderr or b'')
                result['runExit'] = 124
            (out / 'run.log').write_bytes(output)
            result['passCheckpointGroups'] = sum(line.startswith(b'PASS ') for line in output.splitlines())
    # 양성 관측도 lock 반환 뒤 게시한다. 전체 호출 수용은 바깥 native_report만 소유한다.
    write_native_report(out / 'results.json', result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scratch-root', required=True, type=Path)
    parser.add_argument('--architecture', choices=['x64', 'x86'], default='x64')
    parser.add_argument('--configuration', choices=['Debug', 'Release'], default='Debug')
    parser.add_argument('--label', default='run')
    parser.add_argument('--generated-root', type=Path)
    parser.add_argument('--yaml-root', type=Path)
    parser.add_argument('--without-textart', action='store_true')
    parser.add_argument('--analyze', action='store_true', help='Use the separate static-analysis build cache')
    parser.add_argument('--core-dependency-negative', action='store_true', help='Core consumer must reject a generated-content include')
    parser.add_argument('--core-only', action='store_true', help='Standalone content-independent library consumer')
    parser.add_argument('--progress-tests', action='store_true', help='Run the app-lifetime progress/Scene transaction cases')
    parser.add_argument('--boundary-tests', choices=['01', '02', '03'], help='Run one approved boundary regression slice')
    parser.add_argument('--root-entry-tests', choices=['getid', 'cancel-find', 'find', 'incoming'],
                        help='Run one first-virtual root lifetime regression')
    args = parser.parse_args()
    if args.core_dependency_negative:
        args.core_only = True
    if args.core_only and (args.progress_tests or args.boundary_tests or args.root_entry_tests or args.without_textart or args.yaml_root or args.generated_root):
        parser.error('--core-only cannot use game catalog or game regression modes')
    if args.root_entry_tests and (args.progress_tests or args.boundary_tests or args.without_textart or args.yaml_root):
        parser.error('--root-entry-tests requires the complete default runtime catalog and no other mode')
    if args.progress_tests and (args.without_textart or args.yaml_root):
        parser.error('--progress-tests requires the complete default runtime catalog')
    if args.boundary_tests and (args.progress_tests or args.without_textart or args.yaml_root):
        parser.error('--boundary-tests requires the complete default runtime catalog and no other mode')
    work = run_directory(args.scratch_root, args.label)
    result = dict(passed=False, architecture=args.architecture, configuration=args.configuration,
                  withoutTextArt=args.without_textart, coreDependencyNegative=args.core_dependency_negative)
    # 모든 모드는 같은 initial/execution/lock/output/drain/commit 경계를 통과한다.
    with native_report(work, result, lambda: {key: value for key, value in result.items() if key != 'sourceHashes'}):
        originals = list((ROOT / 'TextRPG').rglob('*.cpp')) + list((ROOT / 'TextRPG').rglob('*.h'))
        originals += list((ROOT / 'ConsoleGameCore').rglob('*.cpp')) + list((ROOT / 'ConsoleGameCore').rglob('*.h'))
        originals += list((ROOT / 'GameData/Runtime').rglob('*.cpp')) + list((ROOT / 'GameData/Runtime').rglob('*.h'))
        originals += list((ROOT / 'Tests/ConsoleGameCore').glob('*.cpp'))
        originals += [ROOT / p for p in ['NativeTests.vcxproj', 'ConsoleGameCore/ConsoleGameCore.vcxproj', 'Tools/Tests/native_build.py']]
        originals += list((ROOT / 'Tests/SceneUiRuntime').glob('*.cpp')) + list((ROOT / 'Tests/SceneUiRuntime').glob('*.h'))
        originals += native_contract_sources('ConsoleGameCore' if args.core_only else 'SceneUiRuntime', args.generated_root)
        fingerprints = {str(path): hashlib.sha256(path.read_bytes()).hexdigest() for path in sorted(set(originals))}
        positive_control = _core_positive_control(args, work) if args.core_dependency_negative else {}
        with build_native_test('ConsoleGameCore' if args.core_only else 'SceneUiRuntime', args.architecture, args.configuration, work,
                               generated_root=args.generated_root, without_textart=args.without_textart,
                               analyze=args.analyze, core_dependency_negative=args.core_dependency_negative) as (build, env):
            result.update(build, runExit=None, passCheckpointGroups=0)
            compile_output = (work / 'compile.log').read_bytes()
            negative_verdict = core_dependency_verdict(build, compile_output, positive_control) if args.core_dependency_negative else None
            negative_passed = negative_verdict is not None and negative_verdict['passed']
            if args.core_dependency_negative:
                # 성공한 진단은 stage 관측이다. passed/status만 전체 호출 수용을 뜻한다.
                negative_verdict['scope'] = 'stage-observation'
                result.update(positiveControl=positive_control, dependencyVerdict=negative_verdict,
                              expectedDependencyRejectionScope='stage-observation')
            result['expectedDependencyRejection'] = bool(negative_passed)
            if not args.core_dependency_negative and build['compileExit'] == 0 and build['warnings'] == 0:
                command = [build['executable']] if args.core_only else [build['executable'], str(ROOT / 'GameData'), str(work)]
                if args.progress_tests:
                    command.insert(1, '--progress-tests')
                if args.boundary_tests:
                    command[1:1] = ['--boundary-tests', args.boundary_tests]
                if args.root_entry_tests:
                    command[1:1] = ['--root-entry-tests', args.root_entry_tests]
                if args.yaml_root:
                    command.append(str(args.yaml_root.resolve()))
                result['runCommand'] = command
                try:
                    executed = subprocess.run(command, cwd=work, env=env, capture_output=True, timeout=30)
                    output = executed.stdout + executed.stderr
                    result['runExit'] = executed.returncode
                except subprocess.TimeoutExpired as failure:
                    output = (failure.stdout or b'') + (failure.stderr or b'')
                    result['runExit'] = 124
                    result['runTimedOut'] = True
                (work / 'run.log').write_bytes(output)
                result['passGroups'] = [line.decode('utf-8', errors='replace') for line in output.splitlines() if line.startswith(b'PASS ')]
                result['passCheckpointGroups'] = len(result['passGroups'])
                result['runtimeMode'] = ('standalone-core' if args.core_only else 'root-entry-' + args.root_entry_tests if args.root_entry_tests else
                                         'boundary-' + args.boundary_tests if args.boundary_tests else
                                         'progress-only' if args.progress_tests else
                                         'without-textart' if args.without_textart else
                                         'legacy-scene-ui' if b'MODE legacy-scene-ui' in output else 'full-catalog-scene-ui')
                result['checkpointCounts'] = {
                    'sceneUi': sum(not line.startswith(('PASS APP01 ', 'PASS IGD ')) for line in result['passGroups']),
                    'endFrame': sum(line.startswith('PASS APP01 ') for line in result['passGroups']),
                    'progress': sum(line.startswith('PASS IGD ') for line in result['passGroups']),
                }
                print(output.decode('utf-8', errors='replace'))
            else:
                print((work / 'compile.log').read_text(encoding='utf-8', errors='replace'))
        # __exit__/unlock에 실패한 빌드를 전체 성공으로 게시하지 않는다.
        drift = [path for path, digest in fingerprints.items() if hashlib.sha256(Path(path).read_bytes()).hexdigest() != digest]
        result.update(sourceHashes=fingerprints, sourceDrift=drift)
        result['generatedSourceSha256'] = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                                          for path in generated_source_paths('SceneUiRuntime', args.generated_root)}
        result['passed'] = (negative_passed if args.core_dependency_negative else
                            result['compileExit'] == 0 and result['warnings'] == 0 and result['runExit'] == 0
                            and result['passCheckpointGroups'] > 0) and not drift
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(native_cli(main))
