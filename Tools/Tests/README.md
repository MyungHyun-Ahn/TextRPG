# Native 런타임 테스트 runner

## SceneManager 추출의 현재 검사 — 로컬 마감, 빈 출력 이슈는 사용자 보류

- 기존 runner/NativeTests/native_build와 원본 소스·PCH/증분 캐시를 유지한다. 네 구성 각각 제품 Build/실제 native 분석 경고0, Core31·Scene98·model137·Save28 checkpoint groups 및 Input168 checks를 확인했다. Input 분석 모드의 변경 없는 캐시 사용을 새 analyzer 실행으로 세지 않는다.
- Debug x64/x86 optional TextArt 헤더 부재 각1 group, Debug/x64 Core 양성 후 정확 C1083 음성, build 계약49·header 정책8 및 PCH-off 실제14 TU include 경계를 확인했다. 최초 긴 PATH의 SDK 검색 실패는 보존하고 자식 전용 짧은 PATH로 해당 검사만 통과시켰다. 일반 검증의 PCH는 끄지 않았다.
- 실제 EXE 명시 진행은 네 구성 각각5 case이며 Debug/x64 fixed-update·frame-skip 진행 각5 case를 추가 확인했다. 일반 앱23 case 전체는 Debug/x64·Release/x64·Release/Win32에서 통과했다. Debug/Win32는 `broken-reference` 자식 출력0 bytes의 실패가 남아 전체 PASS가 아니다. 같은 EXE/dat/cwd 단건은 exit1/정상 진단이나 원인 해결/전체 검사 PASS로 해석하지 않는다. 사용자 승인으로 `APP-D32-EMPTY-OUTPUT`은 작업 차단에서 제외하고 별도 보류한다. 현재 추가 진단·재현·수정은 중지하며 재발 시 종료 코드·출력·실행 조건을 보존해 추적한다.
- source 검토는 최초 runtime15/build8, 변경7 집중 검토, 마지막 로그2행/1파일 검토의 정체성 조합이다. 현재 소스 차단 결함은 없으며 최초 canonical FAIL은 유지한다. 이번 로컬 마감은 이 보류 예외를 명시한 처리이며 [013 §12](../../Docs/GameDesign/013_SceneManager공통기능및TextRPG로직분리계획.md#12-실제-로컬-구현과-검증-상태)와 근거 루트 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/scenemanager-impl-bb83fdfd/`의 `execution-index.json`·`review/composition.json`을 따른다. 기존 이력·실제 group/case 단위를 보존하고 합산하지 않는다. 커밋·푸시는 별도 사용자 승인으로 진행한다.

## 지정 불필요 코드 제거 후 현재 검사 — 2026-11-16

- TextRPG의 진행·Custom·저장 CPP는 기본 `pch.h`를 사용한다. NativeTests의 Scene/InGameData/SaveData suite는 같은 원본을 위한 루트 PCH를 타깃별로 생성한다. GameData 구현·생성 fixture의 별도 `GameData.pch`는 유지하며, SaveData fault 매크로와 원본 직접 포함은 PCH 뒤에 둔다.
- 폐기한 `ResolveGameDataRoot` 전용 `test_gamedata_paths_analysis.py`는 제거했다. exe 옆 `GameData.dat` 선택·다른 cwd·무효 loose-root 환경변수 무시 경로는 실제 앱 일반 회귀에서 유지한다. 새 runner나 복사 빌드는 추가하지 않았다.
- 네 구성 각각 solution Build/제품 native 분석 경고0, Input168 checks, Save28·model137·Scene94 checkpoint groups 및 실제 앱 일반23·진행5 case를 확인했다. Core-only28 group은 Debug/x64, optional TextArt-부재1 group은 Debug x64/x86에서 새 실행했다. 기존 build 계약48·헤더 정책8과 Debug/x64 실제 PCH-off include 경계도 확인했다. group/case는 겹치므로 합산하지 않는다.
- 근거 루트는 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/textrpg-retire-d33f723a/`다. `execution-index.json`·`native/*/results.json`·`app/final-grid.json`을 따른다. independent source coverage는 변경 app/build/input29파일과 generator3파일이며 코드 차단 결함0이다. app canonical의 잘못된 부가 diff 해시/FAIL은 보존했고 `review/app-build-input-identity-erratum.json`과 `verified-review-composition.json`에 정체성 대조를 분리했다. 최종 로컬 수용 기록은 `final-acceptance.json`이다.
- 아래의 '현재 검사'와 Frame/생성 최초 빌드/warm 수치는 각 이전 후보 당시의 기록이다. 이번 작업에서 무관한 전체 Store/Exceptions·추가 Frame 모드·수동 DPI/글꼴 검사를 반복한 것으로 해석하지 않는다.

## 생성 InGameData 통합의 현재 검사 — 2026-11-16

기존 진입점과 `native_build.py`/`NativeTests.vcxproj`를 사용한다. 아래는 제공된 최종 실행 기록이며 이번 문서 작업이 빌드·코드 감사·추가 재검토를 실행했다는 뜻은 아니다. 현재 근거 루트는 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/ingamedata-41751db1/`다.

| 범위 | 실제 확인 결과 | 기록(근거 루트 기준) |
|---|---|---|
| 네 구성 제품 Build/분석 | Debug/Release × x64/x86(프로젝트 Win32) 각각 solution Build + native 분석 성공·경고0 | `build/current-final-grid.json` |
| native checkpoint groups | 네 구성 각각 **Core28 / model137 / Scene94**. Scene94는 UI62/EndFrame25/진행7. model137은 고유64라벨의 반복 raw checkpoint occurrences이며 고유 단위 테스트137개가 아님 | 같은 grid의 coreResult/modelResult/sceneResult |
| 실제 앱 | 네 구성 각각 일반23 / 명시 진행5 case PASS | 같은 grid의 fullAppResult/progressAppResult |
| 추가 모드/화면 | Debug/x64 fixed-update·frame-skip 진행 각5 case. screen-oracle21은 게임 미실행 control | `app/Debug-x64-final-modes.json` |
| optional 첫 빌드 | 기본 정적 출력 전체 누락→실제 생성→TextArt 헤더 제외 소비자 컴파일/실행1 group·경고0, 원본 생성물 bytes/mtime 복구 | `native/optional-first-missing-preservation.json` |
| 동적 생성 CPP | 별도 schema/output의 missing→add→remove 실제 GameData Build 각각 경고0. 추가 타입 첫 컴파일/삭제 타입 lib 잔존 없음·안정 PCH bytes/mtime 보존 | `build/dynamic-types-first-add-remove.json` |
| warm Build | model compiledObjects0/PCH 재생성0·생성물 bytes/mtime/EXE 불변. runtime은 동일 EXE 결과 승계이며 새 분석 실행 아님 | `native/model-warm-final/results.json` |

- 일반23/명시 진행5/각 Frame 모드5에는 missing-package 등 같은 case가 반복된다. 이를 고유28개 또는 모드 합계로 부풀리지 않는다. native group·제품 case·screen control도 합산하지 않는다.
- 생성 제품 CPP는 **GameData.lib만 컴파일·소유**하고 일반 Scene/model 소비자는 lib를 링크한다. 구조 원본은 `GameData/InGameDataSchema/InGameData.yaml`, 진행 생성물은 `GameData/Generated/InGameData`다. 정적 헤더 전체13파일은 바이트 그대로 `GameData/Generated/GameData`로 이동했다. optional/altered-generated/source-including fault fixture는 원본의 별도 캐시 variant를 유지한다. Custom CPP는 TextRPG 소유다.
- 모델 검사는 생성 단일 가변 루트 직접 사용·shared_ptr null/반복 참조·재귀 vector/map/unordered_map·Custom hook/factory·추가/삭제 교차 버전·IGD1 read-only→CGSA 쓰기·미래 버전 Save/NewGame 거부를 포함한다. 기존 IGD1 독립 wire builder/정확 진단/prefix/출력 보존은 구형 입력에 유지하고 신형 framing/round-trip을 별도로 확인한다.
- Core 예산은 **bytes1 MiB / depth64 / container4096 / shared-table4096 / object8192 / structural-value visits65536**다. 객체/구조 방문을 하나의 카운터로 합산하지 않는다. 모델 ID256 UTF-16 units와 저장 byte 서비스16 MiB, 생성기 YAML/AST 입력 제한은 별개다. aliasing/순환 소유는 지원 보장이 아니며 관측하지 못한 alias provenance의 완전 검출도 주장하지 않는다.
- 독립 source coverage는 원본 검토 + core-v2 변경3/불변4, generator-v2 변경2/불변8, integration-v2 변경6/불변23의 집중 재검토 조합이다. `current-evidence-index.json` 및 `review/*-v2-independent/{coverage,verdict}.json`을 따르며 전체 신규 전수 재검토/원격 승인으로 표시하지 않는다. 초기 FAIL/BLOCKED와 아래 Frame/보고서/이전 실행 범위를 보존한다. 조정자의 최종 acceptance 집계는 별도이며 publication·커밋·푸시는 없다.

### 현재 CLI로 모델·Core를 실행하기

```text
python Tools/Tests/test_gamedata_store_runtime.py --scratch-root <owned-log-root> --label model --suite InGameData --architecture x64 --configuration Debug
python Tools/Tests/test_scene_ui_runtime.py --scratch-root <owned-log-root> --label core --core-only --architecture x64 --configuration Debug --analyze
```

전체/좁은 application 명령과 fixture 선행조건은 아래 사용법을 그대로 따른다. 기록의 실행 파일은 해당 `results.json`의 `executable`/`runCommand`로 선택하며 캐시 해시 경로를 추측하지 않는다. 모델/저장에 키 실값을 인자로 넣거나 문서에 기록하지 않는다. 자세한 제품 경계는 [012 §11](../../Docs/GameDesign/012_InGameDataYaml정의및자동생성설계안.md#11-구현검증-상태-관리)과 [Core 직렬화 README](../../ConsoleGameCore/Serialization/README.md)를 따른다.

## 상속형 Frame 개선 당시 검사 — 이전 후보 이력

- 기존 `--core-only`는 실제 lib의 FFrame 상속·선택적 훅·Basic/FixedUpdate/FrameSkip·시간 경계/과부하·실패/정리/재진입을 검사한다. 가짜 시계로 시간을 제어하며 실제 장시간 sleep이나 새 runner를 추가하지 않았다. 네 구성의 일반/분석 소비자에서 각각15 checkpoint group, 실제 분석 경고0을 확인했다.
- 기존 Scene full은 각 네 구성93 group(UI62/EndFrame25/진행6)이다. 새로운 상속 Scene 프레임 검사는 세 모드의 입력 1회·전환 뒤 이전 Scene 추가 갱신 차단·렌더 생략 EndFrame을 포함한다. 과거90/86 등의 수치는 해당 이력이며 현재 group 수가 아니다. 묶음은 겹치므로 합산하지 않는다.
- 실제 게임 runner에 **초기화 옵션 전달용** `--frame-mode basic|fixed-update|frame-skip`을 추가했다. reject/일반 콘솔/중첩 Continue·저장 검사 모두 실제 게임 자식에 전달하며 패커·저장 fixture의 다른 CLI에는 전달하지 않는다. 옵션을 생략하면 기존 실행 인수를 그대로 유지한다.
- 네 구성의 최종 EXE별 기본23 case 및 각 세 모드의 진행5 case를 확인했다. 같은 실행 파일의 모드를 런타임에 바꾸지 않고 매번 새 프로세스를 초기화한다. 최종 source review 범위와 재현되지 않은 검증 실패 이력은 [011 §12](../../Docs/GameDesign/011_Frame루프및상속형게임실행설계안.md#12-실제-구현과-검증-완료)에 구분한다.

```text
python Tools/Tests/test_game_data_application.py --game <absolute-TextRPG.exe> --packer <absolute-GameDataPacker.exe> --scratch-root <new-owned-root> --only ingamedata --progress-fixture <absolute-existing-SceneUiRuntime.exe> --frame-mode fixed-update
```

`--only ingamedata`의 fixture 선행 검사·정확한 다섯 case·최종 출력/flush 뒤 성공 게시·실패 정제·소유 콘솔과 파일 감시 수명은 그대로다. Source-only review와 실제 실행은 별도 증거이며, 문서 수정 때문에 제품 검사를 반복하지 않는다.

## 정적 라이브러리 분리 당시 실행 범위 — 이전 후보 이력

- 일반 SceneUiRuntime은 ConsoleGameCore.lib와 GameData.lib를 링크한다. optional/custom-generated 및 reader/Win32/crypto 매크로 주입 fixture는 원본 CPP 직접 참조를 유지한다. GameData 소스는 `GameData/Runtime/GameData`, 공통 코어는 `ConsoleGameCore`로 이동했다. 기존 네 진입점과 공유 MSBuild/backend를 확장하며 새 실행 runner는 없다.
- 실제 네 구성 각 core4/full90/progress28/legacy77/optional1 group, Input168 checks, Save28/Model127/Store330 checkpoint occurrences, Exceptions10 command records를 확인했다. 실제 EXE는 각 default23/진행5 case다. 아래 분리 이전 수치는 당시 이력이며 현재 결과보다 우선하지 않는다.
- `--core-only`는 TextRPG/GameData include/link 없는 상속 UI·프레임·Scene 소비자다. `--core-dependency-negative`는 정상 독립 소비자 control 뒤 지정 consumer의 정확한 C1083를 검사하고 timeout/경고/다른 오류/미도달 주입을 거부한다. 현재 Scene의 모든 모드는 공통 native_report/native_cli 경계를 사용한다. 초기 incomplete 뒤 build context 종료·drift·최종 출력/양 stream flush를 마쳐야 성공을 원자 게시한다. native 관측과 전체 호출 완료를 구분하며 집중 실행/독립 source 검토를 완료했다.
- Store의 기존 5테이블 보존 fixture는 명시적 `--data-root <완전한 legacy GameData root>`를 사용한다. 전체 진행 테이블 데이터는 InGameData suite에서 검사한다. 이는 native assert 제거가 아니며 기본 전체 데이터를 Store fixture에 전달한 초기 실패도 보존한다.
- Release의 제품 GameData lib는 /GL을 사용한다. 해당 lib를 소비하는 NativeTests만 호환되지 않는 증분 링크를 끄며, PCH/obj 증분 Build 및 다른 variant는 유지한다. 솔루션 플랫폼 x86과 native 프로젝트 Win32를 구분한다.
- 최신 실행·소스 검토·제한은 [010 §10](../../Docs/GameDesign/010_게임코어및콘텐츠라이브러리분리설계안.md)과 [로드맵](../../Docs/ROADMAP.md) 상단을 따른다. 수동 글꼴/DPI 인증이나 전체 콘텐츠 완료를 뜻하지 않는다.


## InGameData·EndFrame·보고서 게시 사용법 — 2026-11-15 도입

기존 진입점/공유 `native_build.py`/`NativeTests.vcxproj`를 확장한 기능이다. 새 runner나 실행 스크립트를 추가하지 않는다. 아래 옵션은 현재 argparse와 native fixture 분기를 확인한 사용법이며 이번 문서 작업에서는 빌드/제품/runner를 재실행하지 않았다. 실제 결과는 [009 §9](../../Docs/GameDesign/009_InGameData및저장복원설계안.md)와 [로드맵](../../Docs/ROADMAP.md)의 최신 항목을 따른다.

### Scene/UI 모드와 호환성

```text
python Tools/Tests/test_scene_ui_runtime.py --scratch-root <log-root> --label progress --configuration Debug --architecture x64 --progress-tests
python Tools/Tests/test_scene_ui_runtime.py --scratch-root <log-root> --label boundary-02 --boundary-tests 02
python Tools/Tests/test_scene_ui_runtime.py --scratch-root <log-root> --label root-getid --root-entry-tests getid
```

| 선택 | 실제 범위/선행조건 |
|---|---|
| 기본 full | 완전한 기본 정의로 UI·EndFrame·진행을 모두 검사한다. 현재 full94 group = UI62/EndFrame25/progress7 |
| `--progress-tests` | 앱 수명 진행/Scene 결합 게시·취소·참조·저장 회귀의 좁은 모드. 전체 full과 겹치므로 별도 고유 합계로 세지 않음 |
| `--boundary-tests 01` | 공통 Scene NVI busy 경계 |
| `--boundary-tests 02` | 실제 FDataScene/direct root busy·최초 virtual/Find/incoming 정리·취소 안전점 |
| `--boundary-tests 03` | 실제 OnExit UI 취소 callback throw의 bridge 복구와 shutdown 최종 detach |
| `--root-entry-tests getid` | AddBase 첫 실제 GetId callback의 수명 경계 |
| `--root-entry-tests cancel-find` | 정상 capture/drag 준비 뒤 CancelInput의 첫 실제 Find |
| `--root-entry-tests find` | 직접 공개 const Find의 첫 virtual 진입 |
| `--root-entry-tests incoming` | 거부/AssignIds 예외의 incoming UI destructor 및 등록 할당 실패/복구 control |
| `--yaml-root <legacy-client-yaml>` | 진행 정의 다섯 cohort 전체 없는 레거시도 기존 UI 회귀를 유지한다. 이전 후보73 group = UI62/EndFrame11/progress0은 당시 실행 이력. 현재 slice 수는 해당 results.json을 따르며 부분 cohort는 정상 레거시가 아님 |
| `--without-textart` | TextArt 생성 헤더를 제외한 별도 소비 fixture, 현재1 group(진행 전용 검사 제외). 원본 생성물을 수정하지 않음 |

- `--root-entry-tests`는 `--progress-tests`/`--boundary-tests`/`--without-textart`/`--yaml-root`와 함께 사용할 수 없다. `--boundary-tests`도 progress/without-textart/yaml-root와 호환되지 않는다. `--progress-tests`는 without-textart/yaml-root와 호환되지 않는다. 모두 작업 로그 생성·빌드 전에 parser error다. `--generated-root`는 기존 옵션으로 유지하되 각 모드가 요구하는 완전한 타입/정의 계약을 충족해야 한다.
- `--analyze`, Debug/Release, x64/x86은 기존대로 사용한다. incoming의 실제 vector allocation 실패 주입은 **Debug CRT 전용**이며 Release의 SCOPE 안내를 실제 allocation fault 실행으로 세지 않는다. full/legacy/progress와 좁은 slice는 겹치므로 group 수를 더해 고유 테스트 총수로 만들지 않는다.
- `results.json`의 `passGroups` 실제 배열·`checkpointCounts`(sceneUi/endFrame/progress)·`runtimeMode`·`runCommand`·`runExit`·`sourceDrift`와 빌드 결과를 함께 확인한다. Queued/Continue bool은 수락이고 EndFrame 뒤 결과만 게시 완료다.

### 기존 Store runner의 SaveData/InGameData suite

```text
python Tools/Tests/test_gamedata_store_runtime.py --scratch-root <log-root> --label save --suite SaveData --architecture x64 --configuration Debug
python Tools/Tests/test_gamedata_store_runtime.py --scratch-root <log-root> --label model --suite InGameData --architecture x64 --configuration Debug
```

- `--suite GameDataStore|SaveData|InGameData`의 기본값은 GameDataStore다. 세 suite 모두 기본 분석이며 `--no-analyze`만 빠른 모드다. SaveData는 byte 서비스/DPAPI/쓰기 실패, InGameData는 실제 Store와 전체/하위 codec·정의 참조·오류 보존을 검사한다. SaveData의 `--data-root`는 native argv로 전달하지 않으며 자체 소유 service 디렉터리를 사용한다.
- 모델 최대1 MiB와 byte 서비스 평문 최대16 MiB를 혼동하지 않는다. SaveData는 전용 키·세이브를 소유 service에 만들고 콘텐츠 키를 소비하지 않는 control을 둔다. 실제 키 파일의 값을 문서/로그에 복사하지 않는다.
- 제품 lib는 원본 CPP에서 빌드하고 SaveData source-including fault fixture도 안정 PCH 뒤에 둔다. suite별 compiled PCH/obj는 격리하며 콘텐츠 `TextRPG/pch.h` 및 `GameData/Runtime/GameData/pch.h`, Core의 안정 PCH 계약을 사용한다. source 해시 목록을 컴파일 목록으로 오인하거나 포함 CPP/제품 생성 CPP를 별도 obj로 중복 링크하지 않는다.

### 실제 application 검사: 전체·좁은 진행 모드

```text
python Tools/Tests/test_game_data_application.py --game <absolute-TextRPG.exe> --packer <absolute-GameDataPacker.exe> --scratch-root <new-owned-root> --only ingamedata --progress-fixture <absolute-existing-SceneUiRuntime.exe>
python Tools/Tests/test_game_data_application.py --game <absolute-TextRPG.exe> --packer <absolute-GameDataPacker.exe> --scratch-root <another-new-owned-root> --only ingamedata-newgame
python Tools/Tests/test_game_data_application_cleanup_meta.py --scratch-root <log-root> --label report-controls --only acceptance --report-only
```

- application에는 `--label`이 없다. `--scratch-root` 자체가 새로 만들 소유 work 경로다. `--game`/`--packer`/`--scratch-root`는 모든 모드에서 필수다. `--only` 선택은 `missing-package|screen-oracle|ingamedata-newgame|ingamedata`다. 기본(default)은 현재23 case이며 명시 진행5 case 전체를 대신하지 않는다.
- `--only ingamedata`에는 **기존 SceneUiRuntime native test EXE**를 `--progress-fixture`로 지정해야 한다. runner가 이 EXE를 빌드하지 않는다. fixture 누락/존재하지 않는 경로는 work 생성·패킹·자식 시작 전에 argparse exit2다. normal/-O의 두 변형4건에서 child0을 확인했다. -O로 모든 제품 assertion을 검증했다는 뜻은 아니다.
- 명시 ingamedata 성공에는 정확한 순서의 다섯 case가 모두 strict true여야 한다: `missing-package`, `ingamedata-newgame-autosave`, `restart-continue-restores-changed-progress-and-quit-roundtrip`, `corrupt-save-keeps-exact-menu-localized-error-and-old-bytes`, `quit-save-failure-localized-nonzero-preserves-ciphertext`. production Store/codec/Save 서비스를 사용하는 fixture가 HP7/Gold4321 세이브를 준비하고 별도 실제 게임 Continue의 표시와 Quit 뒤 재역직렬화 값을 확인한다. Python 합성 세이브가 아니다.
- `--only ingamedata-newgame`은 위 앞 두 case만 요구하는 정상 좁은 경로다. 이를 전체 Continue/손상/저장 실패 수용으로 표시하지 않는다. `--only missing-package`는1 case, `screen-oracle`는 게임 자식을 실행하지 않는 물리 셀21 control이다.
- 모든 실제 spawn은 `TEXT_RPG_SAVE_ROOT`를 별도 소유 save-root로 고정한다. 합법적인 세이브/DPAPI 키 쓰기는 app/cwd/TMP 무추출 감시와 분리하며 자식 시작 전 arm→소유 자식 회수→health/drain까지 기존 수명을 유지한다. 실제 메뉴/Town의 문구·네 옵션·좌표/테두리·로고/진행값 오라클과 cleanup을 약화하지 않는다.

### 실패 보고서·CLI·control의 수용 단위

- 진행 모드는 최초 `passed=false/status=incomplete`를 만들고 전체 요청 범위 성공 뒤만 complete를 게시한다. 중간 실패는 `passed=false/status=failed`와 완료된 관측만 남긴다. 기존 incomplete 갱신도 `write_report`의 **owned pending write→close→replace**를 사용한다. write/close/replace 전 실패에서 이전 canonical raw bytes를 보존하고 소유 pending만 정리한다. 초기 파일 부재/최초 생성 실패는 기존 기록 보존과 다른 경우이며 성공이 아니다.
- 최종 stdout write/명시 flush가 끝난 뒤 성공 파일을 게시하며 게시 후 CLI가 중복 명시 flush하지 않는다. 예외 원문·cause·traceback 대신 타입/고정 code 및 허용한 Win32 stage/errno를 공개한다. imported main은 원래 예외를 다시 던지지만 실제 CLI는 정제한 nonzero로 종료한다. 실패한 출력 sink도 성공으로 바꾸지 않는다.
- meta `--only cleanup|observer|acceptance`는 제어 그룹 선택이다. `--report-only`는 **`--only acceptance`에서만** 허용하며 보고서 게시 control13 record로 좁힌다. 선택적 `--game`/`--packer`는 둘을 함께 주어야 하고 감사 preflight에만 사용하며 해당 제품을 실행하지 않는다. 내부 `--worker-*`는 문서상 일반 제품 검사 진입점이 아니다.
- 최신 전체 meta86 record, cleanup5, 화면21을 각각 확인했다. meta에는 fake child/console 경계·normal/-O 별도 프로세스·실제 main/CLI·원본 Path I/O control이 포함된다. actual truncate·양수 prefix write·실제 close 완료 후 합성 오류·replace 경계 오류와 pre-write 실패를 구분한다. exact fault stage/errno/hit/completion·canonical bytes/SHA·pending 정리·원문 비노출·동일 판정의 wrong-reason/unhit/uncompleted 음성을 요구한다. OS close 자연 실패나 C++ SAVE/실제 게임 실행으로 승격하지 않는다.
- application 성공은 요구 case 전체/최종 결과 게시와 exit0이 함께 필요하다. 실제 게임 저장 실패 case의 기대 nonzero와 메타 음성 worker의 기대 nonzero/프리플라이트 exit2는 상위 오라클이 확인한 성공 조건이며 임의 nonzero가 PASS는 아니다. 빌드 실패/경고·timeout(exit124)·불완전 JSON·source drift는 실패다.
- **이전 최소 모델 후보 이력:** Debug/Release × x64/x86 각 Scene full86/legacy73/progress24·optional1·Input168, 게임 분석4구성 경고0, 실제 EXE 각 default23/진행5였다. 당시 서비스는 같은 네 구성 각 SaveData28·InGameData127·Store330 checkpoint occurrences, Exceptions10 command record(compile2+fault execution8)를 승계 검증했다. 모델127은 고유 라벨54의 반복 검사다. `ingamedata-implementation-20261115/entry-publication-repair/`의 `execution-gates.json`, `inherited-service-evidence.json` 및 `final-acceptance.json` 목적 경로는 이력으로 보존하며 현재 생성 모델137/Scene94의 새 실행 근거로 승격하지 않는다. 현재 결과는 이 README 상단을 따른다. 제품 case·native group·meta record·순수 화면 oracle를 합산하지 않는다.
- 위 이전 서비스 승계는 source/config 차이 검토를 포함한다. Save Debug/x64는 전체 XML 바이트가 아니라 제외 CPP PCH metadata·IDE header 차이를 제외한 활성 compilation projection 동일이다. 당시 모델 private50파일 중49는 동일하고 Localization은 기존 bytes 뒤 앱 오류 두 행만 추가됐으며 당시 live UI/앱 matrix가 이를 검증했다. 전체 데이터 바이트 동일이나 서비스 재실행을 주장하지 않는다.

## 실제 EXE 화면·파일 수명 검사

기존 application/cleanup/meta 진입점을 유지한다. `application_observers.py`는 application과 meta가 함께 사용하는 비실행 helper이며 새 runner가 아니다. 게임 EXE 재컴파일 없이 검증한다.

```text
python Tools/Tests/test_game_data_application.py --game <absolute-TextRPG.exe> --packer <absolute-GameDataPacker.exe> --scratch-root <new-owned-log-root>
python Tools/Tests/test_game_data_application.py --game <absolute-TextRPG.exe> --packer <absolute-GameDataPacker.exe> --scratch-root <another-new-owned-log-root> --only screen-oracle
python Tools/Tests/test_game_data_application_cleanup_meta.py --scratch-root <owned-log-root> --label observer-controls --only observer
```

- `screen-oracle`는 게임을 실행하지 않는 물리 셀 양성/음성21개 검사다. 숫자 부분 문자열이나 임의 프레임 차이가 아니라 실제 패킹 fixture의 네 선택지 전체 문구·테두리·좌표와 MainMenu/Town 배치를 식별한다. CJK trailing 셀과 정확한83×41 로고 비교를 유지한다.
- 일반 application 검사는 모든 게임 시작 전에 소유 app/실행 cwd/runtime-temp의 subtree 감시를 준비하고 자식 회수 뒤 알림을 drain한다. 잠깐 생성했다 삭제한 파일도 기록한다. 종료 후 inventory/poison dat 확인도 유지한다. 각 사례의 `*-file-activity.json`에 roots/readiness/events/healthEvents/errors를 저장한다.
- 감시기는 게임 종료 뒤 정확히 소유한 고유 확인 파일을 생성·삭제해 알림 완료 경계를 검증한다. 이 테스트용 health 이벤트와 게임 실행 중의 예상치 못한 이벤트를 구분한다. 버퍼 손실/잘못된 레코드/오류/timeout이면 실패하며 전역 PID별 파일 쓰기 차단을 주장하지 않는다. 패커의 안전 게시용 임시파일은 게임 감시 수명 밖의 별도 도구 동작이다.
- 이전 Asset 보강 이력: 화면21/21, observer9개+당시 meta3개, cleanup5개, 동일 EXE 네 구성 각22개 통과. 당시 총88회 실행의 예상치 못한 이벤트0·감시 오류0이며 제품 재빌드는0회였다. 현재 InGameData 후보의 수치/수용 범위는 위 최신 절과 [로드맵](../../Docs/ROADMAP.md) 상단이 우선하며 이전 증거를 덮어쓰지 않는다.

기존 진입점 네 개를 유지한다. `native_build.py`는 직접 실행하지 않는 공통 MSBuild 지원 모듈이며 별도 runner가 아니다. 새 runner/일회용 검증 스크립트는 추가하지 않는다.

- `test_input_manager.py`: 입력 상태/Win32 공급자/할당 실패 회귀
- `test_scene_ui_runtime.py`: 실제 Scene/UI 수명·입력·렌더 회귀
- `test_gamedata_store_runtime.py`: 실제 인증 패키지·YAML·타입·경로·롤백 회귀
- `test_gamedata_store_exceptions.py`: 실제 reader를 유지한 예외 주입·행 포인터/값 보존·회복 회귀
- `NativeTests.vcxproj`: 네 타깃이 공유하는 C++20/v145 빌드 정의. 일반 게임 빌드에는 자동 포함하지 않으며 VS 솔루션에서 명시적으로 선택할 수 있다.

## 실행

솔루션 루트에서 실행한다. 로그 루트는 소유한 절대경로를 지정하고, 매 실행 서로 다른 `--label`을 사용한다. label은 로그 이름일 뿐 빌드 캐시를 나누지 않는다.

```text
python Tools/Tests/test_input_manager.py --scratch-root <log-root> --label input-1 --arch both --configuration Debug
python Tools/Tests/test_scene_ui_runtime.py --scratch-root <log-root> --label scene-1 --configuration Debug
python Tools/Tests/test_scene_ui_runtime.py --scratch-root <log-root> --label scene-2 --configuration Debug
```

Scene/Store/Exceptions의 `--architecture x86`과 Input의 `--arch x86`은 프로젝트의 Win32에 매핑된다. 모두 `--configuration Debug|Release`를 지정할 수 있다. 테스트에서는 Release 최적화/CRT를 사용하되 assertion을 활성화하며 게임 Release 설정은 변경하지 않는다.

## 빌드와 분석

- 원본 CPP·헤더와 `TextRPG/pch.h` / `GameData/Runtime/GameData/pch.h`, Core의 PCH를 사용한다. 일반 제품 라이브러리 링크와 특수 fixture의 원본 별도 컴파일을 구분한다. 소스 복사, include 치환, 일반 테스트의 PCH 비활성화는 하지 않는다.
- MSBuild `Build`를 사용한다. 빌드 산출물은 `Out/Tests/NativeRuntime/<suite>/<variant>/<configuration>/<platform>/`에 유지한다. 로그/실행 fixture는 별도 log-root/label 아래에 둔다.
- suite, 구성/아키텍처, generated-root/헤더 집합, 선택 카탈로그 여부, 분석 모드를 분리한다. MSBuild가 실제 소스/헤더/옵션 의존성을 추적한다. 변경 없는 재실행은 obj/PCH를 재사용한다.
- 동일 캐시의 빌드와 테스트 실행은 프로세스 간 잠금으로 직렬화한다. 서로 다른 suite/구성/플랫폼은 별도 캐시를 사용한다.
- 일반 실행도 컴파일 경고를 오류로 취급한다. Input/Scene 정적 분석은 `--analyze`로 명시하며 별도 캐시에서 실제 `/analyze`를 수행한다. Store는 기존대로 분석이 기본이고 Exceptions는 기존대로 normal+analyze 양 모드가 기본이다. 빠른 실행의 통과를 분석 통과로 간주하지 않는다. 최종 분석 gate는 생략하지 않는다.

```text
python Tools/Tests/test_input_manager.py --scratch-root <log-root> --label input-analysis --arch both --configuration Release --analyze
python Tools/Tests/test_scene_ui_runtime.py --scratch-root <log-root> --label scene-analysis --architecture x64 --configuration Release --analyze
```

공유 GameData CPP, Store/Yaml 드라이버 및 reader 예외 주입 타깃은 안정 std/Windows SDK를 모은 `GameData/Runtime/GameData/pch.h`(include spelling `GameData/pch.h`)를 사용한다. 타깃/variant/config/platform별 `GameData.pch`와 `GameDataPch.obj`를 생성한다. 게임/Input/Scene의 루트 PCH와 일반 Scene의 제품 lib 링크를 유지하고 특수 variant의 GameData CPP는 별도 PCH를 사용한다. 치환 reader·매크로·직접 포함 구현은 PCH 뒤에 둔다. 게임의 compiled obj/PCH를 테스트와 임의 공유하지 않는다. 생성 타입/umbrella는 PCH에 넣지 않는다. PCH-off/include 경계 검사는 별도 `test_pch_header_resolution.py`의 목적이며 일상 회귀의 기본 빌드 방식이 아니다.

## 선택 카탈로그·출력

`--without-textart`는 TextArt 생성 헤더 자체가 없는 소비자를 검증하는 명시적 예외다. 이 경우에만 캐시 안에 TextArt를 제외한 작은 생성 헤더 fixture를 유지한다. 원본 생성물은 변경하지 않고, 변경 없는 fixture 파일도 다시 쓰지 않는다. `--generated-root`/`--yaml-root`는 기존 소비자 옵션을 유지한다.

각 로그 폴더의 `compile.log`, `build-command.json`, `results.json`, `run.log`로 실제 결과를 확인한다. Input의 compile/run 로그는 아키텍처 하위에 있다. 결과에는 `buildSeconds`, `compiledObjects`, `pchRebuilt`, `analysisEnabled`, `analysisObserved`, 실제 checks/checkpoint 수가 포함된다. warm 분석 빌드가 캐시를 사용하면 `analysisObserved=false`일 수 있으며, 이를 새 분석 실행으로 표시하지 않는다.

정상 종료는 빌드/실행 성공·경고 없음·실제 체크 출력이 있어야 한다. 빌드 실패 뒤 이전 실행 파일로 테스트를 통과시키지 않는다. 같은 label의 기존 로그는 덮어쓰지 않는다.

## Store / Exceptions 사용과 호환성

```text
python Tools/Tests/test_gamedata_store_runtime.py --scratch-root <log-root> --label store-1 --architecture x64 --configuration Debug
python Tools/Tests/test_gamedata_store_exceptions.py --scratch-root <log-root> --label exceptions-1 --architecture x64 --configuration Debug
python Tools/Tests/test_gamedata_store_runtime.py --scratch-root <log-root> --label store-fast --architecture x64 --no-analyze
python Tools/Tests/test_gamedata_store_exceptions.py --scratch-root <log-root> --label exception-focused --architecture x64 --mode normal --only bad-alloc-empty
```

- Store의 기존 `--generated-root`/`--data-root`는 유지한다. 기본 분석을 건너뛰는 빠른 일반 빌드는 명시적 `--no-analyze`에서만 수행한다.
- Exceptions의 기존 `--only bad-alloc-empty`는 유지한다. 새 `--mode normal|analyze|both`의 기본값은 both이며, 기본 실행은 각 모드에서 fault1/2 × empty/populated 네 사례다. 기본 결과는 빌드2+fault8의 **10 command record**이고 단위 테스트10개가 아니다. `--mode normal --only bad-alloc-empty`는 빌드1+fault1의 2개 record다.
- Store는 원본 CPP4개와 shared PCH creator1개, Exceptions는 CPP3개와 creator1개만 빌드한다. Exceptions 드라이버가 reader 주입을 위해 `FGameDataStore.cpp`를 직접 포함하므로 이를 별도 obj로 중복 링크하지 않는다. 기존 native assertion·fault-hit·진단·롤백·회복 조건은 바꾸지 않는다.
- 각 runner는 `results.json`을 남긴다. Exceptions의 빌드/사례 로그는 normal/analyze 하위, Store 실행 로그는 `run.log`다. 빌드 실패·경고·빌드 timeout이면 이전 EXE를 실행하지 않는다. 두 Store runner의 runtime timeout은 exit124/실패 로그·JSON으로 남기고 PASS 처리하지 않는다.

## 선행 검사·인계·마감

[프로젝트 QA](../../Agents/qa.md)를 따른다. 기존 `python Tools/Tests/test_input_build_contract.py`로 XML/등록·중복·실패 처리 계약을 확인하고, 실제 한 x64 양성/음성 사례를 먼저 통과시킨 뒤 필요한 구성만 확장한다. Worker의 변경 경로/해시·실제 명령/결과·소유권 해제를 한 번에 받아 즉시 통합한다. 최종 실행·독립 검토를 한 수용 기록으로 집계한 뒤 문서·task·완료 보고를 묶어서 갱신한다. 필수 경고0·원본·기존 불변식 보존 기준을 낮추는 절차가 아니다.

## 이전 obj 증분 편입 측정 — 2026-11-15

아래는 shared PCH 적용 전의 이력이다. 최신 타깃별 PCH/Asset 수용 상태는 로드맵 상단 기록이 우선한다.

같은 머신 x64 Debug의 단회 빌드 측정이며 전체 작업 속도 보장이나 통계적 벤치마크는 아니다.

| 타깃/모드 | cold 빌드 | 변경 없는 warm | warm 재컴파일 |
|---|---:|---:|---:|
| Store / analyze | 16.989초 | 0.823초 | obj0 |
| Exceptions / normal | 4.778초 | 0.814초 | obj0 |
| Exceptions / analyze | 12.251초 | 0.838초 | obj0 |

원본 바이트를 바꾸지 않고 mtime으로 의존성을 시험한 뒤 원래 값을 복구했다. Store 드라이버 변경은 해당 obj1개, Exceptions 드라이버 변경도 모드별 해당 obj1개만 재생성했다. `FGeneratedYaml.h` 변경은 Store의 직접/간접 소비 obj3개와 Exceptions 모드별 obj2개만 재생성했고 package obj는 유지했다. Store 헤더 probe의 최초 예상2개는 드라이버 직접 include를 빠뜨린 집계 오류여서 실제3개로 정정했으며 이미 통과한 native 실행은 반복하지 않았다.

최종 4 suite × Debug/Release × x64/x86 = **16개 조합**을 확인했다(Exceptions 양 모드를 포함한 빌드-mode record20개). Worker가 먼저 검증한 Store/Exceptions x64 Debug는 동일 후보 해시로 승계하고 부모는 누락14개 조합만 실행했다. 상세 수용 범위·독립 검토는 [로드맵](../../Docs/ROADMAP.md)의 완료 기록을 따른다.

## 정적 라이브러리 소비 경계

`ConsoleGameCore/`가 공통 입력/렌더/UI/일반 Scene·Frame의 단일 구현 소유자다. 정상 SceneUiRuntime은 ConsoleGameCore.lib와 GameData.lib를 링크한다. `--without-textart` 및 다른 generated-root는 고정 GameData.lib 대신 `GameData/Runtime/GameData` 원본의 별도 캐시 variant를 컴파일하며 reader/SaveData source-including 주입도 유지한다.

`test_scene_ui_runtime.py --core-only`는 같은 NativeTests/backend에서 독립 core suite를 실행한다. `--core-only --analyze`는 별도 분석 캐시를, `--core-dependency-negative`는 생성 헤더 include의 정확한 C1083 거부를 검사한다. 기존 `--scratch-root`/`--label`을 사용하고 게임 catalog 옵션과 혼합하지 않는다. core suite에는 TextRPG/generated include 경로와 GameData.lib 참조가 없다.

ProjectReference는 전역 OutDir/IntDir/TargetName과 fixture 속성을 제거해 라이브러리의 자체 Out/PCH/obj를 사용한다. backend는 긴 사용자 PATH가 cmd/Exec 한계를 넘지 않도록 해당 자식만 .NET/Windows 도구 경로로 제한한다. 사용자 환경을 수정하지 않는다. 최종 네 구성·EXE·분석은 통합 소유자가 실행한다.
