# TextRPG 작업 로드맵

### SceneManager 공통 기능·TextRPG 로직 분리 — 로컬 마감, 빈 출력 이슈는 사용자 보류

- [013 §12 실제 구현/검증](GameDesign/013_SceneManager공통기능및TextRPG로직분리계획.md#12-실제-로컬-구현과-검증-상태)에 사용자 승인으로 구현한 **TSceneManager<TAction> → FTextRPGSceneManager 상속**과 사용자 승인 보류 항목을 기록했다. 로컬 작업 마감과 전체 검사 PASS를 구분한다.
- Core는 현재/후보 Scene 소유·실행·전환 요청·EndFrame 게시·수명 보호를 실제 담당한다. TextRPG는 GameData/FDataScene 생성, 행동·ESC/확인키, typed 진행 결합 게시, Continue·저장·현지화/화면 정책을 담당한다. 두 Manager에 같은 소유 큐/전환 엔진을 복제하지 않는다.
- 활성 InGameData는 Session 소유, Layer/UITopLayer는 Scene/UI 소유다. Scene과 진행의 결합 게시, old UI/Scene→진행 정리 순서, 요청/완료 구분·취소/예외·destructor Quit 및 상속 파괴 순서를 보존한다. 기존 라이브러리 경계/완료 이력과 UI-08/09 등 다른 작업 ID는 변경하지 않는다.

| ID | 작업 | 선행 및 완료 기준 |
|---|---|---|
| SCM-D01 | 역할·상속·소유권·실패/검증 계획 문서화 | **문서 완료.** 013과 010의 후속 우선순위 및 이 로드맵 동기화. 코드 구현 완료가 아님 |
| SCM-01 | 최소 Core API·콘텐츠 hook·결합 게시/파괴 계약 | SCM-D01. 실제 호출/소유권·직접 API·최초 콜백·실패/종료 경계와 작은 회귀 확정 |
| SCM-02 | Core TSceneManager 구현·독립 소비자 | SCM-01. GameData 없는 별도 행동/Scene의 실제 전환·수명 동작 및 금지 의존 음성 |
| SCM-03 | FTextRPGSceneManager 상속·진행 결합 게시 | SCM-02. NewGame/Continue의 작은 실제 연결부터 단일 루트·안전점·완료/저장 보존 확인 |
| SCM-04 | 게임 입력·UI 표시·Scene owner 결합 정리 | SCM-03. Core의 구체 게임 타입/확인키/현지화/화면 특례 제거와 실제 동작 보존 |
| SCM-05 | 소비자·프로젝트/filters·기존 runner 이관 마감 | SCM-03/04. 구형 경로·중복 상태 제거, optional/fault 소비자·fingerprint 일치. 필요한 초기 호출부는 SCM-03부터 함께 갱신 |
| SCM-06 | 최종 후보 일괄 검증·독립 source 검토·문서 마감 | SCM-05. 네 구성 Build/분석 경고0·Core 독립성·실제 앱/진행·수명/입력 회귀·원본/산출물 정체성 확인 |

- [x] SCM-D01 계획 문서 작성.
- [x] SCM-01 최소 API/수명·게시 계약 및 작은 회귀.
- [x] SCM-02 공통 Manager 구현/독립 사용.
- [x] SCM-03 게임 파생 Manager/진행 연결.
- [x] SCM-04 게임 입력·UI·owner 정책 분리.
- [x] SCM-05 모든 소비자·프로젝트·runner 마감.
- [x] SCM-06 로컬 검증·검토·문서 마감 — Debug/Win32 빈 출력 이슈는 사용자 승인으로 별도 보류. 전체 검사 PASS가 아님.

네 구성 Build/제품 분석은 경고0이며 각각 Core31·Scene98·model137·Save28 groups, Input168 checks를 확인했다. 실제 진행/저장/Continue는 네 구성 각각5 case, 일반 앱은 Debug/x64·Release/x64·Release/Win32 각각23 case 통과다. Debug/Win32 일반 앱은 `broken-reference` 자식 출력0 bytes로 실패했으며 같은 EXE/dat/cwd의 단건 진단은 정상이나 전체 성공/원인 해결로 승격하지 않는다. 사용자 지시로 `APP-D32-EMPTY-OUTPUT`은 이번 작업의 차단에서 제외하고 재발 시 추적한다. 추가 진단·재현·수정은 현재 중지하며 최초 FAIL과 원인 미확정 기록은 보존한다. Core 음성·optional·구조/include 경계와 독립 소스 검토는 완료했다. 상세 범위와 최초 FAIL은 013 §12와 `scenemanager-impl-bb83fdfd/execution-index.json`에 남겼다. 기존 원본/PCH/증분 캐시·보호 입력·이전 이력은 유지하며 커밋·푸시는 별도 사용자 승인으로 진행한다.

### 지정 불필요 코드 제거·TextRPG 단일 PCH — 로컬 완료, 2026-11-16

- TextRPG의 `GameDataPch.cpp`와 파일별 보조 PCH 설정을 제거했다. 진행·Custom·저장 CPP는 기본 `pch.h`를 사용한다. `GameDataPch.h` 공유와 GameData 라이브러리·native 생성 fixture의 타깃별 PCH는 유지한다.
- `ResolveGameDataRoot`·전용 경로 probe·의미 없이 전달되던 `gameDataRoot` 인수, 미도달 구형 도메인 검증 군집과 `MovementValidation.cs`, 미사용 `FGameDataText.QuoteYaml`, `FDataScene::m_options`·`FUiTextArt::m_artId`, `MouseLeftPressed`를 제거했다. 호출부·프로젝트/필터·기존 회귀도 함께 맞췄다.
- `OptionId`, `SourceRelativePath`, 활성 C#/이름 제한·범용 검증, 실제 YAML emitter, 일반 확장점·double buffer는 이번 제거에 포함하지 않았다. 기존 InGameData 구현·Excel/Data/Asset/생성물·사용자 아트 주석과 Preview를 보존했다.
- 최종 후보의 네 구성 solution `Build` 및 제품 native 분석은 경고0이다. 네 구성 각각 Input168 checks / Save28·model137·Scene94 checkpoint groups, 실제 앱 일반23·명시 진행5 case를 확인했다. Core-only28 group은 Debug/x64만 새 실행했고 optional TextArt-부재1 group은 Debug 두 아키텍처에서 확인했다. 겹치는 수치는 합산하지 않는다.
- 생성기 Release self-test/원본 출력 `--check`, 기존 build 계약48·헤더 정책8, Debug/x64 실제 PCH-off include 경계를 확인했다. 원본 CPP·기존 obj/PCH 캐시와 증분 `Build`를 사용했고 Clean/Rebuild·중복 runner·커밋·푸시는 하지 않았다.
- 독립 source 검토는 app/build/input29파일 및 generator3파일에서 코드 차단 결함0이다. app 검토의 부가 diff 해시 오기는 원래 canonical FAIL을 보존하고 조정자의 원본 해시·coverage 대조 및 별도 identity erratum으로 처리했다. 원래 검토를 PASS로 덮거나 제품 빌드/소스 검토를 반복하지 않았다.
- 근거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/textrpg-retire-d33f723a/`의 `execution-index.json`, `verified-review-composition.json`, `review/app-build-input-identity-erratum.json`, 최종 `final-acceptance.json`. 아래 완료·FAIL/BLOCKED·보조 PCH 구성 등은 각 이전 후보의 이력이다.

### InGameData YAML·ISerializable 코드 자동 생성 — 로컬 구현·실행 근거 확인, 2026-11-16

- [012 설계안](GameDesign/012_InGameDataYaml정의및자동생성설계안.md)에 YAML 구조 정의 → ISerializable 기반 타입/Serialize/Deserialize 자동 생성 → CustomInGameData 상속 확장을 정리했다. YAML은 개발용 정의 입력이며 실제 세이브는 byte 인증 저장 경로를 유지한다.
- 생성 출력은 GameData/Generated/GameData와 GameData/Generated/InGameData, 수작업 코드는 TextRPG/CustomInGameData다. 기존 정적 Class 전체13파일(소유 마커 포함)은 바이트 그대로 전자로 이동했다. **제품 생성 CPP는 GameData.lib만 컴파일·소유**하며 일반 TextRPG/Packer/native 소비자는 링크한다. optional/altered-generated fixture의 별도 원본 variant는 유지한다. 생성 선행·신규 타입 첫 컴파일/삭제를 실제 Build로 확인했다.
- 개발자가 선언한 멤버 타입을 따르고 전역 단독 소유 규칙/별도 소유권 설정은 두지 않는다. shared_ptr의 null/반복 참조와 vector/map/unordered_map의 재귀 조합을 구현·검증했다. `InGameData`는 private 생성 `SRootInGameData` 하나를 실제 가변 원본으로 직접 쓰며 하위 타입은 생성 alias다. 별도 DTO/수동 진행 사본은 없다. 실제 shared 캐릭터와 Custom hook/factory를 연결하고 UI에는 const·비소유 조회만 제공한다. Excel 정적 필드 모드의 기본형·wstring·enum 지원과 진행 구조 모드를 구분한다.
- 구조체는 추가/삭제 가능하지만 이름 변경은 금지한다. 멤버도 추가/삭제 가능하지만 기존 이름·타입은 변경하지 않는다. 구조체명·멤버명으로 식별하고 숫자 식별자나 별도 호환 lock 파일은 두지 않는다. 현재 스키마만으로 과거 변경을 자동 감지한다고 하지 않는다.
- 현재 writer는 이름/signature/길이 기반 **CGSA wire1**이며 **IGD1 v1은 읽기 전용 이관**이다. Continue는 저장하지 않고 이후 정상 Quit 등 저장에서 새 포맷을 쓴다. since 이전 missing default·추가/삭제 교차 버전·삭제 필드의 shared identity 복원을 검증했다. 인증된 미래 CGSA schema/wire/IGD1 format 슬롯은 Save/NewGame 덮어쓰기를 거부하고 키/암호문·실패 Continue 활성 진행을 보존한다. unknown-field 재저장 보존·프로세스 간 CAS·임의 버전 이관은 보장하지 않는다.
- [x] SDG-D01: 확정 경로·타입 선택·이름 불변·GameData 컴파일·생성/확장·호환/실패 수용 기준을 설계 문서와 기존 계약에 반영. 문서 완료만 뜻한다.
- [x] SDG-01: YAML 문법·선언 타입·이름 기반 포맷·누락/버전·기존 파일 이관 계약 구체화.
- [x] SDG-02: YAML 입력/위치·중복 이름·참조·지원 타입/조합 검증과 입력/기존 출력 보존.
- [x] SDG-03: ISerializable·생성 타입·양방향 코드·선언 포인터·vector/map/unordered_map의 실제 컴파일/회귀.
- [x] SDG-04: CustomInGameData 상속 확장·구조체명 기반 중첩 팩토리·사용자 코드/후보 실패 보존.
- [x] SDG-05: Generated 두 출력/집계·GameData의 생성 CPP 컴파일·VS filters/첫 빌드·Core 단방향 관계·Packer 영향·warm 재사용.
- [x] SDG-06: 이름 기반 추가/삭제 교차 버전·기존 파일 보존/명시 이관·실제 Continue/정상 Quit·필요 구성 증분 Build/분석 경고0·독립 검토.
- 순서: SDG-01 → SDG-02/공통 인터페이스 계약 → SDG-03 → SDG-04/05 → SDG-06. 위 체크는 아래 실행 근거를 갖춘 로컬 구현 범위이며 문서만 완료했다는 뜻이 아니다. 원본 Excel/아트·실제 사용자 세이브/키를 보존하며 스테이징·커밋·푸시하지 않는다.

| 현재 검증 범위 | 제공된 최종 실행/검토 근거 |
|---|---|
| 네 구성 solution Build + native 분석 | Debug/Release × x64/x86(프로젝트 Win32) 각각 성공·경고0 |
| Core/model/Scene native | 네 구성 각각 **Core28 / model137 / Scene94 checkpoint groups**. model137은 고유64라벨의 반복 raw checkpoint occurrences로 고유 단위 테스트137개가 아님 |
| 실제 앱 | 네 구성 각각 일반23 / 명시 진행5 case PASS. missing-package 반복을 포함하므로 고유28개로 합산 금지 |
| 추가 Frame/화면 | Debug/x64 fixed-update·frame-skip 진행 각5 case; screen-oracle21은 게임 미실행 control로 별도 범위 |
| 최초 생성/증분 | 기본 정적 출력 누락 optional 실제빌드1 group, 별도 schema missing→add→remove 실제 CPP 컴파일/삭제 및 안정 PCH 보존. warm model compiledObjects0/PCH 재생성0·생성물 bytes/mtime/EXE 불변(runtime 승계, 새 분석 아님) |
| 독립 source coverage | 원본 검토 + core-v2 변경3/불변4, generator-v2 변경2/불변8, integration-v2 변경6/불변23의 집중 재검토 조합. 전체 신규 전수 재검토/원격 승인 아님 |

- Core 한도는 **bytes1 MiB / depth64 / container4096 / shared-table4096 / object8192 / structural-value visits65536**로 분리한다. 객체 예산에 컨테이너 노드를 모두 합산하지 않는다. 모델 ID256 UTF-16 units·byte 저장 서비스16 MiB·YAML/AST 입력 제한도 별개다. 비순환 완전 객체/DAG만 지원하며 weak_ptr·소유 순환·aliasing/서브객체는 미지원이고 숨은 aliasing 완전 검출을 주장하지 않는다.
- 증거 루트: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/ingamedata-41751db1/`. `current-evidence-index.json`, `build/current-final-grid.json`, `app/Debug-x64-final-modes.json`, `native/optional-first-missing-preservation.json`, `build/dynamic-types-first-add-remove.json`, `native/model-warm-final/results.json` 및 `review/*-v2-independent/{coverage,verdict}.json`을 구분한다. 사용법은 [Generator](../Tools/GameDataGenerator/README.md#ingamedata-구조-yaml-모드)·[Core 직렬화](../ConsoleGameCore/Serialization/README.md)·[테스트](../Tools/Tests/README.md) README, 상세 현재 계약은 [009 §9](GameDesign/009_InGameData및저장복원설계안.md)·[012 §11](GameDesign/012_InGameDataYaml정의및자동생성설계안.md#11-구현검증-상태-관리)을 따른다.
- 현재 source/실행 gate는 확보했으나 최종 보호 입력·소스/산출물 정체성·문서/전체 acceptance 집계는 조정자가 별도로 마감한다. 이 문서가 최종 수용/작업 상태를 대신 게시하지 않는다. publication은 없으며 로컬만 반영했다. 과거 FAIL/BLOCKED·Frame·승인 차단·미구현 콘텐츠 이력은 아래 그대로 보존한다. UI-08/09 전체 콘텐츠·수동 DPI/글꼴 인증·배포 키 정책까지 완료한 것은 아니다. 아래 Class/IGD1 writer/127·93 등은 각 이전 후보 시점의 기록이며 현행 생성 경로/모델 상태보다 우선하지 않는다.

### 상속형 Frame·세 가지 루프 방식 — 구현·통합·검증 완료

- ConsoleGameCore의 FFrame.Run이 실제 루프·시간 제어를 제공하고, const 사본으로 보관한 초기 Basic/FixedUpdate/FrameSkip 옵션은 런타임에 변경하지 않는다. TextRPG의 FTextRpgFrame이 필요한 훅을 구현하며 Main의 외부 while·프레임 조립 람다와 기존 FFramePipeline을 제거했다.
- 선택적 훅은 Start/StartFrame/Input/Update/Render/EndFrame/Stop이다. 입력은 외부 프레임당 한 번이며 Fixed catch-up 중 전환 수락 뒤 이전 Scene 추가 갱신을 차단한다. 의도적 Render 생략은 EndFrame을 실행하고 실제 실패 프레임은 게시하지 않는다. 기존 정상 Quit 저장·Continue·late-shutdown 계약은 유지한다.
- [x] FRM-01: API·세 모드 의미·옵션 기본값·입력 횟수·Skip 안전점·종료/예외 계약.
- [x] FRM-02: Core FFrame Basic·선택적 훅·작은 독립 상속 소비자 구현/검증.
- [x] FRM-03: TextRPG 상속 이관·Main 프레임 조립 람다 제거·실제 앱 보존.
- [x] FRM-04: FixedUpdate/FrameSkip·과부하 상한·전환 뒤 추가 tick 차단·결정적 시간 검증.
- [x] FRM-05: 기존 소비자/테스트/프로젝트·filters 이관 및 FramePipeline 제거.
- [x] FRM-06: 네 구성 Build·Core/게임 분석 경고0, 실제 앱·수명/저장 회귀, 독립 소스 검토 및 사용법 마감.
- 네 구성 각각 독립 Core 일반/분석15 group, Scene full93 group(UI62/EndFrame25/진행6), 최종 EXE 기본23 case와 각 세 모드 진행5 case를 확인했다. Debug 두 아키텍처 Input 각168 checks·optional 각1 group, 독립 Core의 정확한 C1083 거부도 확인했다. 겹치는 group/case는 합산하지 않는다.
- 독립 Core9 경로(삭제1 포함)/콘텐츠10 경로 source-only 검토 모두 차단0이다. 검토 후 Main EOF 빈 줄만 정리했고 각 구성 Main.obj만 증분 재컴파일했다. 최종 앱 산출물의 관련 검증을 갱신했으며 PCH와 Excel·아트·생성물·보호 시안/주석을 보존했다.
- **실패 관측 보존:** Debug/x86 재검증 중 게임 자식과 별도 패커의 빈 출력 실패가 있었고 최초 원인은 미확정이다. 좁은 관측과 자식 종료를 기록한 기존 실제 CLI의 최종23 case에서는 재현되지 않았다. 이를 수정 완료한 제품 버그나 확정된 외부 원인으로 보고하지 않는다. 초기 실패를 PASS로 덮지 않았다.
- 실제 API·검토/실행 범위·비차단 제안·실패 이력은 [011 §12](GameDesign/011_Frame루프및상속형게임실행설계안.md#12-실제-구현과-검증-완료), 사용법은 [코어 README](../ConsoleGameCore/README.md#상속형-frame-사용)를 따른다. 증거 루트는 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/frame-b0593321`의 current-evidence-index.json / final-acceptance.json이다. 커밋·푸시는 하지 않았다.

### Runtime·집계 헤더·PCH 공유 정리 — 완료

- `GameData/Source`를 `GameData/Runtime`으로 이동하고 기존 `GameData/...` include spelling을 유지했다. GameData의 Class/Excel/Data/Asset/README·소유 마커까지 프로젝트/필터에 등록했다(자기 프로젝트 파일·캐시·Office 잠금 제외).
- Generator가 `GameData/Class/GameDataAll.h`를 자동 생성한다. 표준 전제 → 존재하는 생성 Enum → Client 타입을 묶으며 개별 `.g.h`는 include-free다. `Main.cpp`가 집계 헤더를 사용하며 PCH에는 넣지 않는다.
- `GameDataPch.h`/`ConsoleGameCorePch.h`는 안정 전제 공유용이다. 각 라이브러리의 `pch.h`와 TextRPG의 `pch.h`는 해당 공유 헤더를 사용한다. TextRPG는 다른 라이브러리의 `pch.h`를 직접 포함하지 않으며 compiled PCH는 타깃별로 독립한다. 기존 모델/저장 CPP용 보조 PCH에는 TextRPG 소유 creator를 제공했다.
- 현재 네 구성 제품 Build/분석8건 경고0, Debug/x64 native11 cell 및 실제 게임 기본23/진행5 case를 확인했다. 생성기 self-test·단독 집계 include 소비자/기존 개별 헤더 소비자의 실제 MSVC 분석을 확인했다. 생성 출력의 기존 leaf/YAML은 불변이다.
- 독립 검토에서 발견한 `GameDataAll.g.h` 오등록 두 곳을 실제 `GameDataAll.h`로 수정했다. 사용자 요청에 따라 등록 일치 검사와 Debug/x64 증분 Build 한 번으로 마감했다(경고0·obj/PCH 재생성0·EXE 해시 불변). 최초 독립 FAIL은 보존하며 수정 후 독립 재검토 PASS를 주장하지 않는다. 추가 세 구성 native/게임 전체 재검사는 생략했고, 이전 후보 결과를 새 실행으로 승격하지 않는다. 승인 시간 만료의 NOT_RUN 기록도 보존한다.
- 현재 증거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/gamedata-runtime-umbrella-be03cc55/`. `product-matrix.json`, `native-Debug-x64.json`, `blocked-native-matrix.json`, `generator/completion.json` 및 `review`를 구분한다. 아래 LIB-01~06은 이전 분리 후보의 완료 이력이다.


### 게임 코어·콘텐츠 라이브러리 분리 — 완료

- **TextRPG 전용 GameData.lib + 콘텐츠 독립 ConsoleGameCore.lib**를 실제 분리·링크했다. 구체 Scene/UI와 게임 행동·InGameData/세션/저장 시점은 TextRPG에 남는다. 실제 타입/경로/검증 범위는 [010 §10](GameDesign/010_게임코어및콘텐츠라이브러리분리설계안.md)을 따른다.
- 코어 FUIManager는 중립 IUiInputTarget만 참조하며 게임별 FGameUiBridge/SUIOption은 콘텐츠에 둔다. FMainMenuUi/FInGameStatusUi/FUiTextArt는 코어 UI 기반을 상속한다. 일반 Scene/프레임 안전점은 코어에, 게임 진행 결합 게시 정책은 콘텐츠에 둔다.
- [x] LIB-01: 소비자·소유권/callback·실패 경계 및 중립 UI/Scene 계약.
- [x] LIB-02: 콘텐츠 역의존 제거, 실제 행동→안전점 전환/실패·공개 직접 작업/정리 회귀.
- [x] LIB-03: GameData.lib·Packer/게임 참조·생성 단일 소유·원본 주입 variant 보존.
- [x] LIB-04: ConsoleGameCore.lib 및 GameData/생성 타입 없는 독립 상속 UI/Frame/Scene 소비자 실행.
- [x] LIB-05: 양 lib를 링크한 TextRPG의 저장·Continue·Layer·Scene/진행 결합 게시 통합.
- [x] LIB-06: 네 구성 Build/분석·native/실제 EXE·증분 실행과 최종 runner 게시 경계의 집중 실행/독립 검토를 완료했다. 단일 수용 기록은 증거 루트의 `parent/final-acceptance.json`이다.
- 현재 실행은 네 구성 각 core4/full90/progress28/legacy77/optional1 group, Input168 checks, Save28/Model127/Store330 checkpoint occurrences, Exceptions10 command records 및 실제 EXE default23/진행5 case다. 겹치는 묶음·기록 단위를 합산하지 않는다. 보호 입력54파일·사용자 아트 주석 불변, warm obj/PCH 재생성0·EXE 해시 불변을 확인했다.
- 기존 full86/legacy73/progress24는 그대로 포함한다. 초기 FAIL·부모 fixture/환경/플랫폼 오입력·Release 링크 실패 및 하위 인계 파일 쓰기 승인 대기는 별도로 보존한다. 코드 검토 PASS와 실제 실행을 구분한다. 증거 루트는 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/static-libraries-b797a2a9/`, 최신 실행/남은 gate는 `parent/current-evidence-index.json`이다.
- 다음 콘텐츠 작업은 기존 UI-08/09의 캐릭터/마을·몬스터 실제 바인딩이다. 이번 분리를 전투·상점·장비 콘텐츠나 수동 글꼴/DPI 인증 완료로 확대하지 않는다.

### InGameData 최소 모델·저장/Continue·프레임 끝 게시 — 2026-11-15

- **구현·실제 실행 게이트 및 독립 focused source review 확인.** `FGameSession`이 앱 수명에서 `unique_ptr<InGameData>`를 소유하며 캐릭터/HP·Stats 보정·Gold·SceneId·인벤토리·업적·ContentVersion을 저장한다. Scene/UI는 비소유 조회다. `InGameData.xlsx`의 CharacterDefinition/LevelDefinition/ItemDefinition/AchievementDefinition/NewGameProfile은 최소 초기 정의이며 최종 밸런스나 모든 콘텐츠 완료가 아니다. 정확한 API·값 필드·수용 범위는 [009 §9](GameDesign/009_InGameData및저장복원설계안.md)를 따른다.
- 모델 codec `IGD1` v1의 전체 상한1 MiB와 저장 byte 서비스의 평문 상한16 MiB를 구분한다. `TRSV` v1 AES-256-GCM은 header AAD·매회 랜덤 nonce·전용 랜덤32-byte 키를 사용한다. 기본 단일 슬롯은 `%LOCALAPPDATA%/TextRPG/Saves/Default/{SaveData.key,InGameData.sav}`, 테스트 override는 `TEXT_RPG_SAVE_ROOT`다. 키는 현재 Windows 사용자 DPAPI 보호 blob으로 별도 보관하고 콘텐츠 패키지 키와 공유하지 않는다. v1 지원은 같은 PC/Windows 사용자이며 다른 PC 이동·키 복구/회전·rollback 방지·완전 치팅 방지·전원 차단 durability를 보장하지 않는다.
- Continue는 read-only 후보 전체 복원/검증/Scene 준비 후 게시하며 rekey·새 게임 기본값 대체·autosave를 하지 않는다. NewGame의 `Queued`/Continue의 true는 수락이고 완료는 `Input → Update → Render/Present → EndFrame → TakeStartResult`로 구분한다. 실제 게시 후 NewGame autosave와 정상 Quit 저장을 구현했다. `StartedSaveFailed`는 진행 시작 성공/저장 실패다.
- typed request·first-wins·snapshot·재귀 guard·다음 프레임 이월, 공통 Scene NVI 및 root 최초 virtual부터 incoming 인수 정리까지 busy 보호를 구현했다. 논리 Quit/입력 무효화는 즉시이며 busy 취소/이미 이월된 idle 재취소의 파괴는 EndFrame까지 기다린다. 이전 비소유 UI/Scene을 먼저 정리하고 이전 루트는 늦게 해제한다. 실제 OnExit 취소 callback throw의 RAII bridge 복구/최종 detach, old destructor Quit 보존 및 late shutdown 추가 저장 금지도 확인했다. 임의 callback 전역 상태 rollback·Manager 자기 삭제·모든 기존 Layer flush의 프레임 끝 이관이나 무제한 lambda queue는 범위가 아니다.
- 최종 영향 실행 게이트는 **Debug/Release × x64/x86(네이티브 프로젝트 Win32)** 각 full86 group(UI62/EndFrame18/progress6), legacy73(62/11/0), progress24(0/18/6), TextArt 생성 헤더 부재 각1 group, Input 각168 checks, 게임 분석 네 구성 경고0이다. full/legacy/progress는 겹치는 묶음이므로 합산하지 않는다. 실제 최종 EXE는 각 구성 default23 case 및 명시 InGameData5 case를 확인했다. HP7·Gold4321의 재시작 Continue/정상 Quit 재직렬화, 손상 세이브 보존·저장 실패/현지화/nonzero를 포함한다. app/cwd/TMP 정밀 감시와 별도 save-root를 유지했다.
- runner 제어 흐름 meta86 record·cleanup5·순수 화면 oracle21은 실제 제품 시나리오 수와 합산하지 않는다. normal/-O의 fixture 누락/잘못된 경로 preflight4건은 exit2·child0이다. 실패 보고서 기존 incomplete 갱신도 owned pending write-close-replace로 게시하며 실제 truncate/양수 partial write/close 완료 후 주입/replace 실패에서 이전 canonical bytes를 보존하는 control을 확인했다. 이는 OS close 자연 실패·C++ SAVE의 실행 증거가 아니다. [기존 runner 사용법](../Tools/Tests/README.md)을 갱신했다.
- 기존 서비스의 최신 실제 실행은 각 네 구성 SaveData28·InGameData127·Store330 **checkpoint occurrences**, Exceptions10 **command record**(compile2+fault execution8)로16개 suite/config cell을 승계 검증했다. 모델127은 고유 라벨54의 반복 보존 검사를 포함하며 고유 테스트127개가 아니다. Save Debug/x64의 제외 CPP PCH metadata/IDE header 등록 차이는 활성 compilation projection 동일로 확인했다. 모델 private50파일 중49는 바이트 동일하고 Localization은 기존 bytes 뒤 앱 오류 두 행만 추가됐다(현재 live UI/앱 matrix로 검증). 전체 XML/전체 데이터 바이트 동일이나 서비스 재실행을 주장하지 않는다. 보호 아트28+Preview+AGENTS 총30파일 불변이다.
- 독립 소스 검토는 `review-core-v5-entry/{canonicalcoverage,verdict}.json` 및 `review-report-v5-atomic/{coverage,verdict}.json` PASS다. Save-v2/model-v2/exception-cohort-v1도 지정 동결 소스 범위 PASS이며 실행 검증과 구분한다. 초기 FAIL·NOT_RUN·보호 `AGENTS.md` 쓰기 차단 이력은 그대로다. 선택적 제안은 새 완료 gate가 아니다.
- **남은 범위:** UI-08/09의 전투·장비·몬스터 교체/스킨·마을/지역·로그·업적 전용 UI/게임플레이 전체, dirty 출력 스케줄링, 전체 DPI/글꼴/수동 호스트 시각 인증 및 배포 콘텐츠 키 전달 정책. 완료한 것은 최소 진행 값과 UI/앱 상태 루트 일관성이지 P3/P4 전체 콘텐츠가 아니다.
- 증거 루트: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/ingamedata-implementation-20261115/`. 실제 현재 실행은 `entry-publication-repair/execution-gates.json`과 `{core,report}/completion.json`, 서비스의 정확한 결과 경로/해시·source/config 승계는 `entry-publication-repair/inherited-service-evidence.json`의 `cells`, 현재 source identity는 최신 두 review manifest를 따른다. 초기 `save-validated-matrix.json`/`model-validated-matrix.json`은 과거 기록으로 보존한다. **최종 수용 기록 경로**는 `entry-publication-repair/final-acceptance.json`이며 조정자 최종 집계 목적 경로로 연결한다(이 문서만으로 작성/전체 수용 완료를 선언하지 않음). 이번 작업은 문서만 수정하고 빌드/제품 실행/네트워크/키 읽기를 하지 않았다.

### InGameData 소유권·저장/복원 계약 확정 — 초기 문서 전용 이력, 2026-11-15

아래 네 항목은 구현 승인 전의 당시 기록이다. 현재 구현/정책/실행 상태는 위 완료 범위와 009 §9가 우선하며 당시 미구현·미정이 현재까지 계속된다는 뜻이 아니다.

- **설계만 확정, 런타임은 미구현.** 게임 내 모든 정적 정의/고정 수치/초기값은 Excel에서 관리한다. 레벨별 필요 경험치·Monster·ArtId↔경로 매칭을 포함한다. 진행 상태는 단일 루트 `InGameData`와 모든 내부 직렬화/역직렬화 가능한 데이터 객체가 소유한다. 구체 캐릭터·인벤토리·업적 필드를 이번에 만들지는 않았다.
- HP·Stats도 UI가 소유하는 표시 상태가 아니라 진행 데이터다. Scene/UI는 비소유 조회·표현만 하고 선택·스크롤·포커스 등 UI 조작 상태를 소유한다. Scene 수명과 진행 루트 수명을 분리한다. 과거 UI의 '현재 상태/HP 멤버' 설명은 독립 진행 원본이 아닌 표시 바인딩으로 정정한다.
- 저장은 전체 진행 직렬화→인증 암호화→안전한 파일 교체, MainMenu Continue는 복호화/인증→전체 후보 역직렬화→정의·상태 검증→재개 Scene 준비→안전 지점의 진행/바인딩 교체다. 실패 시 기존 정상 세이브·활성 진행/MainMenu를 보존한다. 세이브 키를 콘텐츠 개발 키와 자동 공유하지 않는다.
- [009_InGameData및저장복원설계안](GameDesign/009_InGameData및저장복원설계안.md)에 확정 경계·미정 항목·IGD-01~06 수용 기준을 기록했다. 005/007·기획 지침을 동기화했다. 직렬화 포맷·구체 필드·저장 경로/슬롯·키 공급/이동 정책은 미정이며 실제 저장/Continue는 구현 완료로 표시하지 않는다. 이번 범위는 문서만이며 C++·Excel·아트·생성물·runner/프로젝트를 수정하거나 재빌드하지 않는다.

### 타깃별 PCH + Asset 내장 GameData.dat v2 — 2026-11-15

- **요청 기능·검증 보강 수용 완료.** 사용자 추가 승인으로 화면 식별·일시적 파일 생성/삭제 관찰을 보강했고 실제 실행과 독립 집중 검토를 통과했다. `AGENTS.md` 보호 파일 갱신 미적용과 과거 차단 진단의 NOT_RUN은 아래대로 별도 유지한다. 게임/Packer/Store/Yaml/reader 예외 타깃은 안정 표준·Windows SDK만 담은 `TextRPG/GameData/pch.h`를 사용하며 compiled PCH와 creator obj는 타깃·구성·아키텍처·분석별로 격리한다. 기존 게임/Input/Scene 루트 PCH와 증분 `Build`·원본 직접 참조를 유지한다. 치환 reader·매크로·포함 구현은 안정 PCH 뒤에 둔다.
- `GameData/Data/Client`와 모든 `GameData/Asset` 원시 바이트를 `Data/Client/...`, `Asset/...` 전체 상대 경로로 같은 인증 암호화 패키지에 넣는다. 컨테이너 TGDP v2, YAML 데이터 프로토콜 1이다. 디렉터리 개수 헤더·Excel/타입 헤더/schema/키 동봉은 없다. v1 dat는 새 패커로 재패킹한다.
- Store가 지속 소유한 바이트를 UI가 메모리에서 디코딩한다. 참조 아트·테이블 후보를 모두 검증한 뒤 함께 게시하고 실패 시 기존 행·자산 객체/버퍼 주소와 값을 보존한다. exe 옆 dat만 사용하며 txt/YAML 추출·loose Asset 루트 탐색/fallback·`TEXTRPG_GAMEDATA_ROOT` 의존은 없다. 게임은 시작 시 로드하며 hot reload를 추가한 것은 아니다.
- 자산28개를 기존 디렉터리의 `Category_Title_Words.ext`로 통일했다. 예: `Asset/Monster/Monster_Stone_Golem.txt`. 원시 바이트·ID·36×36/83×41 규격을 보존하고 Excel TextArt C14:C17·생성 YAML·VS 항목/필터·회귀 경로를 동기화했다. 실제 OOXML의 다른 셀·스타일·시트는 보존했으며 사용자 preview/FDataScene.h 아트 주석도 보존했다.
- 네 구성 솔루션 `Build`, 게임/Packer 네이티브 분석8건에서 경고/오류0·실제 `/analyze`를 확인했다. 기존 네 suite × Debug/Release × x64/x86의16조합, Exceptions 양 모드를 포함한20 build-mode record 통과. 각 구성 Input168 checks, Scene62 group, Store330 group; 예외의 실제 fault-hit·테이블/자산 롤백·회복을 보존했다. optional 생성 헤더 부재 x64/x86 및 PCH-off 네 구성도 통과했다.
- 실제 최종 EXE는 현재 검사 기준 **구성별22개 시나리오** 통과했다. exe+dat 배포본·다른 cwd·loose-root 환경변수 부재/존재하지 않는 경로·정확한 로고 셀·종료 후 잔여 추출 파일 없음·동일 EXE의 데이터만/아트만 재패킹 후 재시작·참조 아트 누락/손상 차단·복구를 확인했다. 추가 보강에서는 실제 패킹 fixture의 문구로 MainMenu/Town의 네 선택지 전체·테두리·물리 좌표와 Town 고유 배치를 판정하고 화면 반례21/21을 통과했다. 기존 검사에서 오탐하던17개 반례는 실제 RED 후 거부로 전환했다. app/cwd/tmp 하위 경로를 자식 시작 전부터 회수 뒤까지 감시한 최종 네 구성 각22개, 총88회 실행에서 예상치 못한 파일 이벤트0·감시 오류0을 확인했다. 감시 정상 확인용528개 이벤트는 테스트가 자식 종료 후 만든 고유 파일의 생성/삭제로 별도 기록했으며 게임의 임시 자산 추출이 아니다. 고정 SHA256의 기본4아트 보호는 유지했다. static34개, 생성기 CLI49개, Release self-test를 확인했다. 수동 글꼴/DPI/모든 호스트 시각 인증이나 UI-08/09 엔티티 바인딩 완료는 아니다.
- x64 Debug Store의 변경 없는 warm 빌드0.244초/obj0/PCH 재생성0, parser CPP mtime 변경은 obj1개·일반 parser 헤더는 의존 obj3개만 재생성했다. bytes/mtime을 복구했다. 최초 재사용 시점의 필요 obj2개 갱신은 warm으로 세지 않고 보존했다. 단회 빌드 측정이며 전체 작업 가속률이 아니다.
- 최초 독립 검토20+72경로의 A-001은 기존 키 runner126/126·컴파일12건 경고0 및 새 집중 정적 검토 PASS로 보완했다. APP-001/002의 cwd inventory·고정 sleep은 실제 물리 셀 읽기·범위 관찰·제한 시간 polling으로 보완하고 같은 EXE 네 구성에서 각22개 및 cleanup5/meta3를 확인했다. 보완 전 집중 검토 `review-v4-application`은 APP-STATIC-01(숫자 부분 문자열/프레임 차이만으로 Town·네 옵션 식별 부족), APP-STATIC-02(종료 후 inventory로 일시적 추출 부재를 입증할 수 없음)로 FAIL이다. 초기 FAIL은 그대로 보존했다. 최종 수용은 원본20+변경 없는71경로의 실제 이전 검토와 key consumer1·observer/meta2·최종 application1의 독립 집중 검토를 명시적으로 합성한95개 고유 변경 경로 범위다. 새95파일 전체 재검토라고 주장하지 않는다. `acceptance-extension-83af0d7a/observer-review-v1`과 `application-review-v2`가 각각 PASS이며 미해결 기능 지적은 없다. 실제 생성/읽기/삭제 자식과 빈 최종 snapshot을 대조한 observer9개, 기존 meta3개와 cleanup5개도 확인했다. 제품/EXE는 추가 검증 동안 재컴파일하지 않았다.
- `AGENTS.md` 계약 갱신은 보호 파일 승인 시간 초과로 **미적용**이다. 차단된 AST 추출 음성 진단도 실행하지 않았으며 재시도/대체 실행하지 않는다. 해당 진단의 NOT_RUN은 통과로 바꾸지 않는다. 현재 동작·CLI는 [Packer README](../Tools/GameDataPacker/README.md), PCH 정책은 [C++ 컨벤션](Conventions/cpp.md), runner는 [테스트 README](../Tools/Tests/README.md)를 따른다.
- 증거 루트: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/pch-assets-0c8fb37c/`. 현재 수용은 `final-status-v2.json`과 `acceptance-extension-83af0d7a/final-acceptance.json`이며 이전 보류 상태와 승인 차단 기록은 이력으로 보존한다. `execution-acceptance.json`, `validated-native-matrix.json`, `final-application-results.json`, `asset-rename-proof.json`, `cache-probe-results.json`, `review-v1/`, `review-v2-application/`, `restricted-probe-not-run.json`을 구분한다. 아래 과거 Asset 미동봉/PCH 미사용·322/521/18 등의 수치는 당시 이력이며 이 항목보다 우선하지 않는다.

### Store/Exceptions 증분 편입·운영 절차 개선 완료 — 2026-11-15

- 기존 `test_gamedata_store_runtime.py`와 `test_gamedata_store_exceptions.py`를 기존 `native_build.py`/`NativeTests.vcxproj`에 편입했다. 새 runner/일회용 검증 Python 파일은 0개다. 원본 CPP 직접 참조·고정 suite/variant/config/platform obj·Build·실행까지 유지하는 캐시 잠금을 사용한다. Store4CPP/Exceptions3CPP를 격리하고, 예외 드라이버의 store 구현 직접 포함과 standalone PCH 미사용 계약을 유지했다.
- 기존 CLI·store 기본 분석·exceptions 기본 normal+analyze/all4fault-state·Release assertion을 보존했다. 명시적 `--no-analyze`, `--mode`와 구성 선택을 추가했다. 빌드 실패/경고/timeout 뒤 stale EXE를 실행하지 않고 runtime timeout도 실패 JSON/로그로 기록한다. 사용법은 [기존 runner README](../Tools/Tests/README.md)를 따른다.
- 기존 계약 검사에서 RED→GREEN과 XML/등록·소스 그래프·실패/timeout·기본 fault 커버리지 **11개 검사**를 확인했다. 한 x64 실제 Store322 group/Exceptions10 record를 먼저 통과시킨 뒤 누락 구성만 확장했다. 최종 Input/Scene/Store/Exceptions × Debug/Release × x64/x86의 **16개 고유 조합**, Exceptions 양 모드를 포함한 **20개 빌드-mode record**가 경고0으로 통과했다. 각 구성 Input168 checks, Scene62 group, Store322 group, Exceptions 빌드2+fault8의10 record다. 분석 모드의 실제 `/analyze`도 확인했다.
- 같은 x64 Debug cold→warm 빌드 측정은 Store16.989→0.823초, Exceptions normal4.778→0.814초·analyze12.251→0.838초다. warm 재컴파일 obj0. 소스 드라이버 변경은 각 suite/mode의 obj1개, `FGeneratedYaml.h` 변경은 Store3개/Exceptions모드별2개만 재생성했고 나머지 obj를 재사용했다. mtime 시험 후 원래 값을 복구하고 바이트 불변을 확인했다. 단회 샘플이지 전체 작업 가속률 보장이 아니다. 헤더 probe 예상 개수 누락은 집계만 정정하고 통과한 실행을 반복하지 않았다.
- 공통 `efficient-development` 스킬(default profile)과 [Agents/qa.md](../Agents/qa.md)에 보호 불변식·최소 XML/실제 진단 gate·한 x64 선행·소유권 해제 후 즉시 통합·단일 최종 집계/문서/상태 마감을 반영했다. 모델의 첫 수정 무결함을 보장하는 강제 런타임 설정은 아니다. Worker의 통과2조합을 승계하고 부모가 누락14개만 수행했으며 독립 검토와 matrix를 병행했다.
- 최종 독립 소스 검토는 할당7파일 전체와 실제 frozen 참조18개를 검토한 canonical coverage/verdict PASS다. 후보 `a374b0ecb594be0069e557964be675085f92688572e1ece987b95937302d2f41`. 생산 C++·원본 workbook/아트·생성물·기존 native assertion을 포함한 보호116파일 해시 불변을 확인했다. 게임 동작이 변경되지 않아 게임 EXE 재실행·C# 전체 솔루션 재빌드는 반복하지 않았으며 영향을 받는 NativeTests 네 구성은 실제 검증했다.
- 증거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/store-runner-619fced8/`의 `final-status.json`, `validated-results.json`, `matrix-results.json`, `dependency-probe-results.json`, `review-v1/coverage.json`·`verdict.json`. 이 완료는 다른 직접 cl 기반 보조 runner 전부의 전환을 뜻하지 않는다.

> **최신 Scene 계약 변경:** 사용자 승인으로 `Asset/Scene`의 레거시 배치 txt와 `Scene.AssetPath`를 제거했다. 정적 Scene은 `SceneId`·`SceneType`·`IsStartupScene` 세 필드만 허용한다. TextArt.AssetPath는 유지하며 원본 아트 이름에 맞춘 경로만 수정한다. 아래 예전 네 필드/레거시 자산 유지 설명과 검증 기록은 당시 이력이며 현재 계약보다 우선하지 않는다. Asset 자체의 dat 동봉은 별도 미구현 제안이다. 이번 전환의 최종 검증을 완료했다. 현재 결과는 로드맵의 2026-11-15 Scene 레거시 제거 완료 기록을 따른다.

### Scene 레거시 제거 완료 — 2026-11-15

- 사용자 승인으로 `GameData/Asset/Scene/{Battle,MainMenu,Town}.txt`와 폴더를 삭제하고 `Scene.AssetPath`를 원본 Excel·생성기 whitelist/회귀·생성 C++/Client YAML·인증 런타임 파서·Scene 준비·VS 프로젝트/필터에서 제거했다. 현재 Scene 필드는 정확히 `SceneId`/`SceneType`/`IsStartupScene`이다. 폐기한 AssetPath와 임의 추가 필드는 Scope와 무관하게 거부한다.
- TextArt 네 경로를 `Asset/Logo/TextRPG.txt`, `Asset/Character/Swordswoman.txt`, `Asset/Monster/Dragon.txt`, `Asset/Town/Windwood.txt`로 정정했다. ID·83×41/36×36 규격·아트 원시 바이트를 보존했다. 전체 남은 아트 28개, 사용자 preview·FDataScene.h·템플릿을 포함한 보호 입력 31개의 사전 해시를 대조했다. Workbook은 Scene D열 삭제·필터 B13:D16·TextArt C14:C17 경로 변경 외의 값/스타일/시트 보존을 확인했다.
- 최종 솔루션 `Build`: Debug/Release × x64/x86 네 구성 성공. 게임 네이티브 코드 분석도 네 구성에서 실제 `/analyze`를 실행해 경고·오류 0을 확인했다. 최종 Release 생성기 self-test와 실제 원본/생성물 `--check` 통과, 동일 Debug DLL의 기존 CLI 49/49 및 데이터 Python 11/11·프로젝트 계약 6/6·헤더 정책 3/3 통과.
- 기존 runner의 Scene/UI 회귀는 네 구성 각각 62 checkpoint group, TextArt 생성 헤더 자체 없는 소비자는 x64/x86 각각 1 group 통과했다. PCH를 재사용했고 변경 의존 obj만 재컴파일했다. 인증 store는 x64/x86 각각 322 group, TextArt 경로 행렬은 각각 거부158/허용39, 예외·롤백은 각 아키텍처 10 command record 통과다. 예전 store 521 수치는 Scene 경로/필드 검사가 남아 있던 당시 기록이며 현재 수치로 재사용하지 않는다.
- 실제 최종 EXE는 네 구성 각각 18개 시나리오 통과. Asset만 둔 배포 fixture·다른 cwd·exe 옆 dat·재컴파일 없는 데이터 변경·실패 차단·선택 TextArt 부재를 확인했고 최종 exe SHA256을 실행 기록과 대조했다. 수동 글꼴/DPI/모든 호스트 시각 인증을 의미하지 않는다.
- 독립 최초 20경로 검토에서 R1(기본 아트의 정확 바이트 회귀 검사 약화) 1건을 발견했다. 동일 크기 픽셀 변경이 통과하는 반례를 확인한 뒤 Python 테스트 한 파일에 승인된 원시 SHA256 비교를 복구했다. 변조 반례는 exit0→exit1, BOM/CRLF 변형8건은 일반 형식 허용과 기본 자산 동일성 거부를 각각 확인했다. 정상 원본 전체 데이터 검사11/11 통과. 일반 카탈로그 형식 검증과 preview 독립성은 유지한다.
- 최초 canonical FAIL은 보존했다. 최종 수용은 변경 없는 19경로의 실제 이전 검토와 변경한 1파일의 새 독립 집중 검토 PASS를 명시적으로 합성한 결과다. 새 20파일 전체 재검토라고 주장하지 않는다. 후보 `d108d9fb620e1f927cdc417db6c8eb76dfaf8ffa1667b52a0e7c3601963c2b1e`, 현재 미해결 지적 없음. R1 보완은 Python만 변경했으므로 C++/의존성·생성물·EXE 해시 불변을 확인해 통과한 네이티브 증거를 승계했다.
- 증거 루트: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/remove-scene-assets-32bd9093/`. 최초 `review-v1/`, 최종 `review-v2-r1/`, `r1-repair/`, `final-status.json`을 구분한다. 환경 PATH 길이 문제와 잘못된 테스트 진단 기대값으로 실패했던 중간 로그도 보존했다. 검증 runner/일회용 검증 스크립트는 추가하지 않았다.
- Asset 바이트의 dat 동봉과 UI-08/09 Town/Battle 현재 대상 아트·이름·상태 바인딩은 이번 범위가 아니며 미구현 상태를 유지한다.


> **INP-01~05 완료:** [008](GameDesign/008_InputManager및SceneUILayer설계안.md). InputManager는 입력 상태만, Scene은 UILayer/UITopLayer와 최상위 입력 처리를 소유한다. 상태 네 구성 각168 checks, 최종 Scene/UI x64/x86 각62 checkpoint, 수정본 네 구성 빌드/게임 분석 경고·오류 0·PCH-off, 실제 EXE 각각18 시나리오를 확인했다. 초기 SCENE-01/02 FAIL은 보존하고 행동 RED→GREEN 뒤 마지막 다섯 파일 집중 재리뷰 PASS로 해결했다. 전체 P3 콘텐츠/호스트 인증은 별도다.

> **검증 runner 경량화 — 구현/실행·독립 소스 검토 완료:** 기존 Input/Scene 두 진입점을 공유 NativeTests MSBuild 타깃으로 연결했다. 원본 소스 직접 참조, 구성별 고정 obj/PCH, 기본 증분 Build와 명시적 `--analyze`를 사용한다. 변경 없는 재실행은 약0.83초·재컴파일0, CPP 한 파일 변경은 Main.obj만, 구체 UI 헤더 변경은 관련4개 obj만 재생성했고 PCH는 유지했다. 두 타깃의 네 구성 분석과 기존168 checks/62 checkpoint, 선택 TextArt 부재 x64/x86 및 솔루션 네 구성 Build 경고·오류0을 확인했다. 사용법은 [Tools/Tests/README.md](../Tools/Tests/README.md). 독립 검토는 동일한 기존8개와 constexpr 보완1개의 전체 소스/delta를 확인해 차단 지적이 없었다. 이전8개 coverage와 최종9개 verdict의 canonical 후보 불일치/부정 상태는 덮어쓰지 않고, 해시로 확인한 8+1 검토의 조정자 통합 기록을 별도로 남겼다. 새로운 전수 리뷰나 원래 canonical pair PASS로 표시하지 않는다. 게임 콘텐츠 바인딩은 시작하지 않았다.

> 최신 schema 출력 정정: 별도 스키마 YAML은 생성·배포하지 않는다. C++ 타입과 Client 데이터 YAML을 사용하고 원본 Excel/런타임 데이터 검증은 유지한다. 아래 과거의 Schema 생성 수치/검증 기록은 당시 결과로 보존한다. 현재 schema 출력 제거가 DATA-02의 행 배열 제거·DATA-04의 dat 시작 로딩까지 완료했다는 뜻은 아니다.

> 작성 기준: 2026-11-13 초기 코드 리뷰 / 2026-11-14 Scene별 UI 정책 및 사용자 지적에 따른 정적 Scene 표시 바인딩 폐기·런타임 컴포넌트 소유 정정.
> 상태: 작업 정리 및 실행 계획. 체크되지 않은 항목은 미완료이며, 제안은 사용자 승인 없이 확정 정책이나 구현 완료로 취급하지 않는다. **최신 P3 실행 현황은 §6의 IUI 구현 기록을 따른다.**
> 완료: P0 파일 보호·DATA-01은 유지한다. P2는 현재 표시 대상을 런타임 UI 컴포넌트에 두도록 정정했다. 이전 P1 증거는 사용자가 거부한 Scene 확장 스키마의 역사이며 정정본 검증 완료를 뜻하지 않는다. 정정 P1 수정본의 self-test·Python·생성/check·실제 C++ 소비자 기술 검증은 통과했다. 독립 정정 리뷰의 임의 다섯 번째 Scene 필드 수용 결함은 cycle1에서 보완하고 같은 반례의 GD2133/F13 거부와 최종 산출물 재검사를 확인했다. 사용자 저장·닫기/지침 수정 재승인 후 원본·생성물·AGENTS.md를 반영했고 전체 솔루션 네 구성 경고/오류 0 및 실제 소비자를 확인했다. 공식 리뷰 기록 완료와 P3 구현은 별도 미완료다. 이전 공식 부정 판정/기록 갱신 거부 상태는 변경하지 않는다. 과거 P1 정정 시점에는 FUIManager뿐이었다. 최신 IUI/Scene UI·TextArt 로더·입력 어댑터 구현과 남은 항목은 §6 IUI 구현 기록을 따른다.

## 1. 목표와 우선순위

1. 데이터 생성·패킹 과정에서 기존 파일이 손실되지 않게 한다.
2. 생성기가 성공한 데이터는 소비자 계약을 만족하도록 검증을 강화한다.
3. UI의 책임, 상태, 입력, 배치 정책을 먼저 합의하고 구현한다.
4. 현지화·Scene 연결을 안정화한 뒤 게임 콘텐츠를 확장한다.
5. 실제 실행 검증과 자동화된 회귀 테스트를 남긴다.

진행 순서: **P0 파일 보호 → P1 데이터 계약 → P2 UI 정책 확정 → P3 UI 구현 → P4 현지화·콘텐츠 연결 → P5 통합 검증**.
P0는 완료했고 P2 소유권 정책은 사용자 정정을 반영했다. P1 정적 Scene 바인딩 제거·독립 TextArt 정정은 실제 원본·생성물 게시와 통합 기술 검증을 완료했다. 공식 리뷰 기록은 미완료로 보존한다. P3의 입력 계약·UI 기본 객체 착수는 P1과 병행할 수 있다. P3 세부 순서는 §6을 따르며 전체 InputManager 완성을 UI 착수 조건으로 삼지 않는다.
단계 번호는 실행 순서이며 코드 리뷰의 결함 심각도 등급과 동일하지 않다. 일정과 담당자는 아직 지정하지 않았다.

### schema YAML 출력 제거 단계 당시 결과 — dat 전환 이전

- 별도 테이블/Enum schema 생성 함수와 Schema 라우팅을 제거했다. 실제 생성기를 통해 소유한 `GameData/Data/Schema`의 기존 여섯 파일과 디렉터리를 정리했다. Excel 원본의 타입·필수·Enum·참조 검증은 유지한다.
- 기존 C++ 헤더 여섯 개와 Client YAML 다섯 개, 원본 workbook/아트는 변경 전과 byte-identical이다. 소유권 마커를 포함한 실제 출력은 Class 일곱 파일 + Data 여섯 파일이며 CLI 논리 생성 레코드 수 12와 구분한다.
- 수정 전 schema 없음 assertion과 옛 라우팅 수용 반례를 RED→GREEN으로 확인했다. `--check`는 옛 schema를 drift로 검출하되 읽기 전용이고, `--validate-only`는 생성/정리하지 않는다. 두 출력 게시 실패 시 옛 schema를 포함해 rollback하는 test-hook self-test도 통과했다.
- 부모 Release 생성기 Rebuild(경고/오류 0), 실제 `--check`/`--validate-only`/`--self-test`, CLI 45/45, 헤더 정책 3개, 실제 native UI 25 checkpoint group이 성공했다. 초기 옛 Release DLL/누락 의존성 시도는 보존했고, 명시적 MSBuild Rebuild 뒤 실제 산출물을 재검사했다. C++ 출력/런타임 코드는 바꾸지 않았으므로 전체 솔루션 네 구성 매트릭스는 이번에 재실행하지 않았다.
- 증거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/remove-schema-output/completion.json`, `worker/publication-results.json`, `parent-cli/`, `parent-release-selftest.log`, `parent-native/`. C++ 실제 행 배열 제거 및 `GameData.dat` 런타임 사용은 별도 미구현 목표다.

## 다음 목표 — GameData.dat 기반 런타임 데이터 전환

> **GameData.dat 런타임 연결과 승인된 RUNTIME-001 보완을 완료했다. DATA-02~08 수용 기준을 검증했다.** 최종 x64/x86 store 각 521 checkpoint, 네 구성 빌드·분석 경고/오류 0, 실제 EXE 각 18개 시나리오를 확인했다. 예외/정리·회귀 보완 여섯 파일과 마지막 경로 두 파일의 focused 정적 리뷰가 각각 PASS다. 전체 프로젝트 재인증이나 P3 콘텐츠/제품 키 배포 완료는 아니다. 과거 실패·보류 기록은 당시 역사로 보존한다.

### 목표 흐름

```text
GameData/Excel
  → Generator 검증
     ├─ GameData/Class: 구조체·Enum 등 타입 정의만 (실제 행 배열 없음)
     └─ GameData/Data/Client: 실제 데이터 YAML만 (스키마 출력 없음)
          → 기존 GameDataPacker/AES-256-GCM
          → $(TargetDir)GameData.dat

게임 시작
  → 실행 파일 디렉터리/GameData.dat 읽기
  → 기존 FGameDataPackageReader로 인증·복호화
  → 메모리 내 YAML 엔트리 unpack (평문 파일 추출 없음)
  → YAML 파싱·타입 변환·데이터 검증
  → 런타임 테이블을 한 번에 게시
  → Scene / Localization / TextArt / UI에서 조회
```

- 최종 배포 파일명은 정확히 **`GameData.dat`**이며 게임 exe와 같은 디렉터리에 생성한다. 기존 구성별 예: `Out/TextRPG/Debug/x64/GameData.dat`, `Out/TextRPG/Release/Win32/GameData.dat`. 모든 Debug/Release × x64/Win32에 같은 규칙을 적용하며 공용 파일 하나를 덮어쓰지 않는다.
- 경로는 **exe 위치 기준**이다. 현재 작업 디렉터리나 개발 PC의 Excel/생성 YAML 경로를 검색하여 대체 로드하지 않는다. `TEXTRPG_GAMEDATA_ROOT`의 기존 아트 루트 역할과 패키지 탐색을 혼동하지 않는다.
- 생성 헤더에는 구조체/Enum/필드 기본 초기화 등 타입 계약을 유지하지만 `k<Table>Rows`, Excel 실제 값 리터럴, 그 정적 배열을 순회하는 `Find<Table>` 구현은 제거한다. 필요한 조회 API는 런타임 테이블 쪽으로 이관한다. 헤더만 비워 런타임을 깨뜨리는 부분 완료로 끝내지 않는다.
- 패키지 입력은 `GameData/Data/Client`의 실제 데이터 YAML만이다. 스키마 YAML 출력·포함은 하지 않으며 C++ 타입/로더가 해석·검증을 담당한다. 데이터 형식 버전 호환 정책은 DATA-03에서 명시한다. 원본 Excel, txt 아트, 키·소스 파일은 묶지 않는다. 아트 파일 배포는 별도이며 dat만으로 아트까지 내장됐다고 간주하지 않는다.
- 기존 인증 암호화·경로/크기 제한·원자적 파일 게시·입력 보호를 재사용한다. 인증 실패 데이터는 파싱하거나 부분 노출하지 않는다. YAML 문법을 읽는 것과 필수 테이블·중복 ID·Enum·범위·교차 참조 검증을 별도로 다룬다.
- 전체 필수 데이터 준비/검증 성공 뒤에만 런타임 저장소를 게시하고 첫 Scene을 연다. 누락·잘림·변조·잘못된 키·지원하지 않는 버전·YAML 오류는 명시적 시작 실패다. 생성 C++ 행/평문 YAML/Excel로 조용히 fallback하지 않는다. TextArt 등 기존 선택적 카탈로그 정책과 필수 테이블 정책은 구분한다.

### GameData.dat 연결 실행 결과와 남은 게이트

- 실제 생성기로 Class/Data 두 소유 루트를 게시했다. 헤더 여섯 개는 타입/Enum만 남고, Client YAML 다섯 개는 `# TextRPG.GameData 1` 추가 외 기존 본문 바이트를 보존했다. Excel·아트·FDataScene.h 원래 아트 주석은 보존했다.
- Main은 exe 옆 dat와 공통 키 공급자를 사용하고, 인증 후 bounded generated-YAML subset 파싱·typed candidate 검증을 마친 뒤 첫 Scene을 생성한다. Scene/Localization/Panel 제목/TextArt/MainMenu는 전역 store 포인터 없이 명시적 저장소를 받는다. 평문 YAML/Excel/정적 행 fallback은 없다.
- 로컬 개발 키는 최초 빌드의 명시적 `prepare-development-key`로 DPAPI 보호 생성/재사용한다. 환경 변수 우선 및 정의된 빈 값/오류의 fallback 금지, 런타임 비생성, 제품 배포 키 미결정 범위를 분리했다.
- 부모는 Debug/Release × x64/Win32의 솔루션 Rebuild 네 번과 네이티브 game/packer 분석 여덟 번, 구성별 package verify 네 번을 실행했다. 총 16개 실행 레코드 모두 성공, 일반/분석 경고·오류 0이다. 추가 TextRPG 개별 프로젝트 기본 Build도 의존성 포함 성공했다. 키 오류로 pack target이 실패하면 빌드 실패를 전달하고 기존 dat 바이트를 보존하는 반례도 확인했다.
- 실제 EXE는 구성별 18개 시나리오를 검증했다. Asset만 있는 사설 배포/다른 cwd에서 MainMenu→Town→Escape, exe 재컴파일 없이 dat 재패킹만으로 메뉴 값 변경, 선택 TextArt 부재 및 누락/변조/키/패키지·데이터 버전/필드/Enum/참조/UTF-8 실패를 포함한다. 네 구성의 중복 집계가 아닌 각 결과 배열 길이 18을 대조했다. Debug x64는 개별 프로젝트 빌드 뒤 새 EXE 정체성으로 다시 확인했다.
- Generator normal/test-hook self-test·CLI 45/45·생성 C++ x64/x86 소비자, 공유 키 공급 x64/x86 테스트 126개 실행 레코드와 기존 package safety, 실제 UI 25 checkpoint group 및 선택 카탈로그 헤더 부재 검증은 각각 별도 범위다. UI checkpoint를 단위 테스트 개수로 합산하지 않는다. 부모는 갱신된 crypto 분석/왕복/packer-safety, include 정책 3개·build 계약 5개·PCH-off 실제 소스 검사도 실행했다.
- **추가 승인 전 보류 단계의 역사:** 당시 AssetPath validator가 `CONIN$`/`CONOUT$` 및 superscript COM/LPT 장치 숫자를 생성기와 똑같이 거부하지 않는다. 추가한 `path-console-device`, `path-superscript-device` 실행이 도구 승인 대기여서 실행·우회하지 않았고, 이 두 반례의 RED→보완→GREEN 및 최신 전체 store suite를 완료로 표시하지 않는다. 이전 성공 로그는 이 두 테스트 추가 전이다. 독립 정적 리뷰는 P2 세 건으로 FAIL이다. RUNTIME-001은 위 경로 검증, RUNTIME-002는 package reader.Load가 store try/catch 밖에 있어 std::exception이 bool/error 계약을 벗어나는 문제, INTEGRATION-001은 smoke 초기화/입력 실패 시 소유 게임 프로세스 정리를 놓치는 문제다. 후자 두 건은 repair1에서 RED→GREEN을 확인했다. reader 생성/Load를 store try 안으로 넣고, 소유 console child의 종료/회수와 handle close/detach를 초기화 성공 여부와 분리했다. 다섯 변경 파일 재리뷰가 찾은 cleanup 회귀 false-positive는 repair2에서 보완했다. 최신 제품 후보 네 구성/실제 EXE 검증과 여섯 파일 focused 최종 정적 재리뷰는 성공했지만 RUNTIME-001은 그대로 미해결이다. runtime 리뷰 canonical 파일은 초기 fail-closed 기록만 저장됐고 최종 보충 파일에 기술 지적이 있어 기록 완결성도 PASS가 아니다. 원래 부정 판정·보충 파일을 보존한다.
- 증거 루트: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/dat-runtime/`. `matrix1/results.json`, `application-*/results.json`, `application-Debug-x64-direct/results.json`, `failed-build-preserves-dat.json`, `generator/result-manifest.json`, `runtime/handoff.json`, `key/completion.json`, `parent-crypto/`, `parent-pch/`를 범위별로 구분한다. 과거 실패/승인 대기 기록을 새 PASS 파일로 대체하지 않는다.
- 사용법: [Generator README](../Tools/GameDataGenerator/README.md), [Packer README](../Tools/GameDataPacker/README.md). 둘 다 실제 연결과 실행 검증 후 작성했다. 수동 호스트/DPI 시각 인증, P3 엔티티 아트/상태/로그 바인딩 및 제품 키 배포는 이 작업 완료 범위가 아니다.

### repair1 — 예외 경계·소유 테스트 프로세스 정리

- RUNTIME-002: 실제 store 구현에서 reader 구성/Load도 예외 경계 안으로 옮겼다. `std::bad_alloc`/`std::runtime_error` 주입만 제어하고 나머지는 실제 패키지/reader를 사용한다. x64/x86의 일반·분석 네 빌드와 실행 16개가 성공했으며, 빈/게시된 저장소·다섯 테이블의 42 fixture row 포인터/값 보존·이후 정상 재로드를 검사했다. 최초 helper C6011은 억제 없이 명시적 null 분기로 고치고 원래 실패 로그를 보존했다.
- INTEGRATION-001: attach·첫/둘째 handle·cleanup key·cleanup wait 실패 다섯 가지에서 소유한 실제 무해 child가 회수되고 handle/detach가 수행되는지 확인했다. 원래 RED의 누수는 독립 safety net만 회수했고, GREEN에서는 smoke 함수 자체가 회수했다.
- 변경은 `FGameDataStore.cpp`, `test_game_data_application.py`와 전용 테스트 세 파일뿐이다. AssetPath validator와 승인 대기 두 테스트, crypto/key/원본/아트/생성기/생성물은 바꾸지 않았다. 코드·로그 SHA는 `repair1/completion.json`과 `changed-file-manifest.json`으로 대조했다. 원래 두 부정 리뷰를 PASS로 덮어쓰지 않으며 이 단계 재리뷰는 당시 다섯 파일의 focused 범위였으며 최종 여섯 파일 판정은 repair2 절을 따른다.
- 이 절의 단위 회귀는 승인 대기 reserved-device 실행 또는 최신 store suite 전체의 성공이 아니다. 부모는 repair1 후보로 네 구성 솔루션/네이티브 분석/verify 16개 실행 레코드(경고·오류 0)와 실제 EXE 구성별 18개 시나리오를 다시 통과시켰다. 최종 실행 파일 SHA를 결과와 대조했다. `matrix-repair1/results.json`, `application-repair1-*/results.json`, `parent-repair1-status.json`이 이 후보의 증거다. 다섯 파일 focused 재리뷰는 제품 예외/정리 수정의 정적 계약 적합성을 확인했지만, cleanup 회귀가 실제 주입 지점에 도달하지 않아도 통과할 수 있다는 medium 지적이 있어 FAIL이다. final canonical 파일도 partial-view guard로 초기 fail-closed 상태만 남아 있다. 마지막 repair2 주기에서 테스트 한정 의도한 fault-hit/호출·획득 수/오류 정체성 강화와 5개 실제 주입·3개 미도달 대조군 검증을 마쳤고, 여섯 파일 fresh focused 최종 정적 재리뷰가 PASS했다(전체 프로젝트 재인증 아님). 제품 소스는 추가 변경하지 않는다. 승인 대기 경로 검증/최신 store suite 전체는 여전히 미완료다.

### repair2 — cleanup 회귀의 의도한 fault 도달 증명

- 제품 코드는 변경하지 않았다. `test_game_data_application_cleanup.py`의 PASS는 의도한 fault-hit, 필요한 실제 attach 성공, CreateFileW 호출 순서·횟수, 획득/반환 수, 정확한 OSError 타입/errno/레이블, 소유권 위반 없음까지 요구한다.
- 새 meta 검사에서 attach 불가→첫/둘째 handle 사례와 첫 open 불가→둘째 handle 사례의 false-positive를 실제 worker로 RED 재현했다(기존 exit0). 같은 세 대조군이 보완 뒤 exit1·faultHit0·`intended fault was not reached`로 거부되고 meta 검사는 성공했다. 원래 다섯 의도된 실패 사례도 성공했다. 다섯 양성 회귀와 세 expected-negative 대조군을 구분하며 RED 세 레코드를 새 성공으로 합산하지 않는다.
- 기록: `repair2/cleanup-meta-red-01`, `cleanup-meta-green-01`, `cleanup-intended-green-01`, `cleanup-reliability-handoff-02/handoff.json`. 부모가 현재 소스·모든 증거 해시와 3/5 배열 길이를 대조했다. 실제 EXE 네 구성 SHA는 repair1 통합 증거와 동일해서 제품 빌드를 불필요하게 다시 실행하지 않았다.
- 자동 repair 두 주기는 소진했다. 여섯 파일의 fresh focused 최종 정적 재리뷰는 PASS이며 부모가 113개 정체성 해시와 canonical 최종 coverage/verdict를 읽어 확인했다. 추가 결함은 새 승인/판정 없이 다음 repair로 확대하지 않는다. 이전 incomplete/FAIL canonical 기록은 그대로 보존했다. 새 후보의 완결된 최종 기록은 `review-repair2-results/coverage.json`, `verdict.json`이며 여섯 파일의 정적 범위만 판정한다. 종합 현재 상태는 `final-status.json`을 따른다. RUNTIME-001 경로 실행 승인·보완·최신 store suite 전체는 별도 미완료다.

### RUNTIME-001 추가 승인 후 — 보완·최종 검증 진행

- 사용자가 예약장치 경로 보완/실행을 추가 승인했다. 이전 두 자동 repair 주기를 초기화한 것이 아니라 RUNTIME-001 한정 추가 보완이다. 기존 실패·incomplete·scoped PASS 기록은 모두 보존한다.
- 원래 보류했던 전체 store probe를 정상 terminal 경로로 실행했다. 컴파일 후 `path-console-device: loaded=1 diagnostic=`으로 의도한 RED를 확인했다. 네이티브 종료 코드는 3221226505이며 셸 wrapper 반환 127을 진단 원인으로 혼동하지 않는다.
- Generator와 runtime의 예약장치 이름 분류(콘솔 별칭·superscript 숫자·확장자 앞 공백) 정합성만 고치고 원래 AssetPath 값을 정규화/복구하지 않는다. Scene/TextArt의 실제 인증 패키지 회귀·정상 near-miss·롤백·x64/x86 전체 store suite와 최종 네 구성 빌드/분석/EXE 통합을 확인한 뒤에만 최종 수용을 닫는다.
- 증거 루트: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/dat-runtime/approved-runtime001/`. runtime 분류 보완과 x64/x86 최신 전체 store suite가 성공했다(각 521 checkpoint group, 예약 경로 음성 316/원문 보존 양성 78, 게시 행 42개 rollback). 부모는 최신 네 구성 solution/분석/verify 16개 실행 레코드의 경고·오류 0 및 실제 EXE 각 18개 시나리오를 확인했다. Generator check/validate, header/build 계약, pack target 실패 시 기존 dat 보존도 다시 확인했다. 입력 Excel·아트는 원래 해시와 같다. `parent-final-verification.json`, `worker/completion.json`, `matrix-approved-runtime001/results.json`, `application-final-*/results.json`이 현재 후보의 증거다. 남은 게이트는 두 변경 파일의 fresh focused 독립 리뷰뿐이며 아직 전체 완료 판정은 아니다.

### 최종 완료 판정 — DATA-02~08

- RUNTIME-001을 포함한 이번 연결의 blocking finding은 모두 해결했다. 마지막 두 파일의 독립 focused 정적 리뷰 PASS와 canonical coverage/verdict를 부모가 읽고 113개 해시·두 파일 범위를 대조했다. 이전 여섯 파일 scoped PASS와 구분하며 초기 incomplete/FAIL 기록은 보존했다.
- 최신 전체 store 회귀는 x64/x86 각각 521 checkpoint group(예약 경로 음성 316, 원문 보존 양성 78 포함), 다섯 타입의 42개 현재 fixture row rollback을 통과했다. checkpoint를 별도 단위 테스트 총수로 부풀리거나 두 아키텍처를 중복 합산하지 않는다.
- 최종 네 구성 solution Rebuild 및 native game/packer 분석·package verify 16개 실행 레코드가 경고/오류 0으로 성공했다. 실제 EXE 각 18개 시나리오와 최종 SHA를 대조했다. 다른 cwd/Asset-only 배포·재컴파일 없는 dat 값 변경·오류 거부·UI 이동 및 pack target 실패 시 기존 dat 보존도 확인했다.
- AGENTS·Generator/Packer README를 반영했고 원본 Excel·아트는 보존했다. Class는 타입/Enum 전용이고 Client YAML 본문은 버전 표식 외 기존 값을 유지한다. 별도 schema 출력·평문/헤더 fallback은 없다.
- 최종 증거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/dat-runtime/final-status.json`, `approved-runtime001/review-results/coverage.json`, `approved-runtime001/review-results/verdict.json`, `approved-runtime001/worker/completion.json`, `matrix-approved-runtime001/results.json`, `application-final-*/results.json`.
- 이번 런타임 연결의 미완료 게이트는 없다. P3 엔티티 아트·상태·로그/수동 DPI 인증 및 제품 키 배포·아트 내장은 별도 범위다. 동일 Windows 사용자 DPAPI 개발 편의와 제품 키 배포를 혼동하지 않는다.

### 구현 순서와 수용 기준

| ID | 작업 | 선행 / 완료 기준 |
|---|---|---|
| DATA-02 | C++ 타입 전용 생성 계약과 회귀 검사 | 실제 행 배열·데이터 리터럴 없음, Enum/구조체/필드 타입·헤더 include-free 유지. YAML에는 실제 행이 유지되고 생성/check/self-test 갱신 |
| DATA-03 | YAML 파서·타입 역직렬화·런타임 테이블 계약 | DATA-02의 타입 계약. 기존 의존성 확인 후 파서/지원 YAML 계약을 명시하고 ko/en 문자열·null·정수 범위·enum·중복/참조 검증. 누락된 파서를 임시 상수 데이터로 대체하지 않음 |
| DATA-04 | GameData.dat 시작 로더와 기존 복호화 연동 | DATA-03. exe 기준 경로, 인증 먼저, 메모리 unpack/파싱/전체 검증/일괄 게시, 오류 시 게임 진입 차단. 저장소가 조회 포인터·참조보다 오래 생존 |
| DATA-05 | Scene·현지화·TextArt 등 실제 소비자 이관 | DATA-04. 모든 생성 정적 배열 직접 참조 제거. 같은 Scene/선택지/문구/아트 참조를 런타임 저장소에서 조회하며 정적 Scene 네 필드·네 선택지 계약 유지 |
| DATA-06 | 구성별 exe 옆 GameData.dat 자동 생성 | Generator→Packer 준비→pack의 명시적 빌드 순서. `$(TargetDir)GameData.dat`, 패킹 실패를 빌드 실패로 전달, 기존 dat 보존, 패키지 게시 없이 오래된 파일로 성공 처리 금지 |
| DATA-07 | 개발 키 공급과 배포 키 정책 분리 | DATA-04/06 전에 공급 계약 고정. 개발 시 기존 `TEXT_RPG_GAMEDATA_KEY`를 pack/런타임에 공급하는 안을 기본으로 검토. 실제 키는 문서/코드/로그/명령행에 쓰지 않으며 키 누락·형식 오류는 실패. 배포 키 전달 방식은 미결정 |
| DATA-08 | 실제 exe·패키지 통합 회귀 | DATA-02~07. 네 구성 빌드/분석 경고 0, 다른 cwd 실행, 옆 dat 값 변경 반영(재컴파일 없이), 헤더 값 fallback 없음, 실패 패키지/키/YAML/참조 거부 및 기존 출력 보존 |

착수 순서: **DATA-02/03 계약·DATA-07 개발 키 계약 → DATA-03/04 구현 → DATA-05 소비자 통합 → DATA-06 자동 게시 → DATA-08 전체 검증**. 생성기 변경은 private fixture에서 먼저 확인하고 실제 타입 전용 생성물 게시는 소비자 이관과 함께 통합한다. 초기 개발 키 환경변수 안은 배포 키 제공의 확정 정책이 아니다. 실행 파일에 키를 넣어도 추출 가능하므로 클라이언트 암호화를 완전한 비밀 보호로 설명하지 않는다.

- [x] DATA-02 타입 전용 헤더 생성과 실제 값 YAML 유지.
- [x] DATA-03 YAML 파싱·타입 변환·런타임 테이블/조회·전체 검증.
- [x] DATA-04 기존 패키지 reader와 시작 로더 연동.
- [x] DATA-05 게임 소비자 전부 런타임 데이터 조회로 이관.
- [x] DATA-06 exe 옆 GameData.dat 구성별 빌드 자동 생성.
- [x] DATA-07 개발 키 공급 계약과 배포 키 미결정 사항 정리.
- [x] DATA-08 재컴파일 없는 값 반영/실패 거부/네 구성/다른 cwd 통합 검증.

관련 상세: [006 YAML·GameData.dat](GameDesign/006_YAML내보내기이식설계안.md). DATA-02~08은 최종 검증을 완료했으며 Generator/Packer README에 실제 사용법을 기록했다. 별도 P3 항목과 제품 키 배포까지 완료한 것은 아니다.

## 2. 현재 확인한 상태

| 영역 | 확인한 내용 | 상태 / 한계 |
|---|---|---|
| 프로젝트 | Windows C++20, MSVC v145, C# GameDataGenerator, C++ GameDataPacker | 지정된 GameData의 상위 TextRPG 솔루션까지 검토 |
| 게임 데이터 | 이전 P1은 Scene·SceneOption·SceneTypeConfig·TextArt·Enums·Localization을 게시했다. 현재 계약은 네 정적 Scene 필드와 독립 선택적 TextArt | 이전 파일 수는 아래 역사 기록만 참조. 실제 정정 원본 6 tables/18 generated files·통합 기술 검증은 아래 실행 기록 참조 |
| Scene | MainMenu·Town·BattleField 데이터 및 Scene 로딩 경로 존재 | BattleField 콘텐츠·선택 액션 완성 여부와 별개 |
| UI | IUI/Panel/Button/Scene 소유 루트·팝업 API와 MainMenu 로고 구현. 세 Scene은 132×43 Panel/네 Button 기하 사용 | 캐릭터/몬스터/마을 상태·아트·로그 바인딩은 미완료. 1240×940 일반 창·DPI/글꼴 수동 검증은 남음 |
| 입력 | Input/IInputSource·FConsoleInput 분리 및 SInputEvent 확장 | 독립 FInputManager 서비스 없음. 실제 레코드 정규화는 fixture 검증 |
| 현지화 | 생성 데이터 조회 함수 및 한국어·영어 데이터 경로 존재 | 선택지 한국어 고정, 설정 기반 언어 전환 미완성 |
| 패키지 | AES-GCM 패킹, 인증·복호화 및 엔트리 제공 | YAML의 게임 객체 파싱·런타임 적용 완료를 의미하지 않음 |
| 버전 관리 | 해당 사본에서 Git 저장소를 확인하지 못함 | 변경 이력·회귀 도입 시점·dirty baseline 판정 불가 |

### 기존 리뷰에서 실제 수행한 검증 — 새 P1/P2 UI 증거 아님

- VS 2026 MSBuild: `TextRPG.sln -restore -m -p:Configuration=Debug -p:Platform=x64 -verbosity:minimal -nologo` — 종료 코드 0.
- 생성기 `--self-test` — 통과.
- 현재 workbook `--validate-only` — 5개 테이블 통과.
- 현재 생성물 `--check` — 15개 파일 일치.
- scratch에서 실제 런타임 클래스·생성 데이터를 연결한 입력/Scene probe 수행.
- scratch workbook 복사본으로 잘못된 입력을 주입하고 생성기 종료 코드 및 C++ 컴파일 결과 확인.
- 기존 Release x64 패커로 정상 패킹·검증 및 변조 태그·잘못된 키·잘린 패키지 거부 확인.
- 현지화 참조 수명 probe는 VS 2022 v143 AddressSanitizer에서 `stack-use-after-scope` 검출. VS 2026 전체 앱 ASan 검증 결과는 아님.

빌드 과정에서 존재하지 않는 ATL/MFC LIB 경로 관련 C# 경고 CS1668이 출력됐다. 빌드 실패는 아니며, 원인 확인 전 소스 결함이나 새 회귀로 단정하지 않는다.
전체 Debug/Release × Win32/x64 재검증과 실제 콘솔 시각·마우스 수동 검증은 이번 리뷰에서 수행하지 않았다.
리뷰 중 소스·원본 workbook은 수정하지 않았으며, 부모 검증 빌드로 Out의 빌드/생성 산출물은 갱신됐다.

## 3. P0 — 패키지 파일 보호

### PKG-01: 입력 YAML과 출력 패키지의 충돌 방지

- [x] `TextRPG/GameData/FGameDataPackage.cpp`의 입력 수집/출력 경계를 보완한다.
- 수정 전 문제: `pack <입력 폴더> <입력 폴더>/one.yaml`이 성공 코드 0으로 입력 YAML을 패키지로 교체했다.
- 출력 경로가 수집된 입력 파일과 같으면 파일 쓰기 전에 실패시킨다. 상대/절대 경로, 대소문자, 경로 별칭의 동일성 처리 범위를 정의한다.
- **완료 기준:** 충돌 입력은 실패 종료, 입력 파일 바이트 및 기존 출력 보존, 정상 패킹·verify 유지.

### PKG-02: 임시 파일 소유권과 원자적 게시

- [x] `TextRPG/GameData/FGameDataPackage.cpp`의 고정 `<출력>.tmp` 사용을 교체한다.
- 수정 전 문제: 기존 `.tmp`를 trunc로 덮어쓰며, 게시 실패 시 해당 파일까지 삭제했다.
- 임시 파일은 충돌 없이 배타적으로 생성하고, 자신이 생성한 파일만 정리한다. 생성·쓰기·게시 실패를 구분한다.
- **완료 기준:** 기존 `.tmp` 보존, 실패 시 입력/기존 최종 출력 보존, 성공 시 완전한 패키지 게시. 같은 출력에 대한 동시 실행 정책도 명시한다.

### DATA-01: 생성 헤더·YAML 출력 경로 분리 (추가 승인)

- [x] 생성 C++ 헤더의 기본 위치를 `GameData/Class`로 변경한다. `Common`과 테이블별 하위 폴더를 직접 둔다.
- [x] 데이터 YAML은 `GameData/Data/Client`, 스키마 YAML은 `GameData/Data/Schema`에 생성한다.
- [x] `--output-root`는 C++ 루트, `--yaml-output-root`는 YAML 루트를 재정의하도록 한다.
- [x] C++ include 경로와 빌드 전 생성 명령을 새 기본 경로와 일치시킨다.
- [x] 두 출력의 경계·소유권 및 물리 경로 별칭을 먼저 검사한다. 게시 전 실패는 양쪽 기존 출력을 복원하고, 게시 완료 후 백업 정리 실패는 커밋된 상태와 남은 경로를 별도 보고한다.
- [x] 추가 독립 리뷰 지적을 반영한 최종 self-test, check/validate-only, 실패·rollback 검사 및 Debug/Release × Win32/x64 솔루션 빌드로 확인한다.
- 컴파일 결과와 중간 파일은 기존처럼 `Out`에 둔다. Excel 원본과 UI 코드는 이번 변경 대상이 아니다.
- **완료 기준:** 실제 기본 생성 실행 후 새 Class/Data 경로에만 새 산출물이 생성되고, 해당 헤더로 전체 구성 빌드가 성공한다. 예전 Out 생성물은 자동 삭제하지 않고 별도로 식별·보고한다.

## 4. P1 — 생성기와 소비자 계약 강화

| ID | 작업 | 소스 위치 | 완료 기준 |
|---|---|---|---|
| GEN-01 | 세 정적 Scene 필드의 Client 포함·금지 컬럼 진단 및 독립 TextArt 검증 | `Tools/GameDataGenerator/SceneDataValidation.cs` | SceneId/SceneType/IsStartupScene만 허용. 폐기한 AssetPath·임의 추가 필드는 Scope와 무관하게 거부. 제거한 네 표시 컬럼은 명시 진단. TextArt 없을 때 Scene 정상, 있으면 카탈로그 네 필드/자산 독립 검증 |
| GEN-02 | 모든 SceneType의 설정·정확히 네 옵션 검증 | `SceneDataValidation.cs` | 설정 누락, 전체 행 수가 4가 아님, order가 1~4가 아님, MaxVisibleOptions≠4 또는 Scroll이면 실패. 전부 비활성 네 행은 정상 |
| GEN-03 | SceneOption 목적지·레이블 현지화 필수 참조 및 제한적 nullable 계약 유지 | `SceneDataValidation.cs`, `GameDataValidator.cs` | TargetSceneId→Scene.SceneId/OptionTextKey→Localization.TextKey는 #Reference 제거로 우회 불가. TargetSceneId만 빈 값/기본값 예외, 지정값은 비활성이어도 유효·활성 StartGame 목적지 필수. Scene→아트/제목/지역 참조·역할 검사는 제거 |
| GEN-04 | 중첩 Excel 폴더에서 자산 루트 일관화 | `SceneDataValidation.cs`, `WorkbookParser.cs` | parser 입력 루트의 부모를 기준으로 Excel/Sub에서도 기존 Scene/새 TextArt의 상대 경로를 동일하게 검증 |
| GEN-05 | 생성 C++의 네임스페이스 이름 가림 방지 | `GameDataGenerators.cs:249–261` | std 테이블명 입력에 대해 명확한 검증 실패 또는 컴파일 가능한 ::std 한정 출력 |

- [x] GEN-01 정정 회귀: 네 필드 Scene 정상·TextArt 없음 정상·금지 표시 컬럼별 명시 진단·존재하는 카탈로그 독립 검증.
- [x] GEN-02 기존 네 옵션/모든 SceneType 설정·4/NoScroll 계약 재검증.
- [x] GEN-03 SceneOption 참조/활성 StartGame·TargetSceneId만 nullable 예외 유지, Scene 표시 참조/SceneType 역할/nullable Scene 예외 제거 확인.
- [x] GEN-04 중첩 Excel 자산 루트·기존 경로 안전성 회귀 재검증.
- [x] GEN-05 ::std 이름 가림 보완과 기존 P0 출력 보호 보존 재검증.
- [x] 실패 시 입력/양쪽 생성 출력의 경로·내용 보존과 workbook·시트·행·열 진단을 정정본으로 확인.
- [x] Scene.xlsx에서 네 표시 컬럼만 제거하고 네 정적 필드·기타 시트/키·ID·번역·네 옵션·Enum append·추출 자산을 보존. 원본 저장/닫기 확인과 재읽기 후 게시.
- [x] 정정 Python 데이터/CLI 검사·생성기 self-test·validate/check·실제 소비자/native probe 결과를 별도 기록.

**P1 상태:** 이전 승인 범위의 독립 안전성 보완·네 옵션·자산을 통째로 되돌리지 않는다. 정정 원본·생성물 게시 및 실제 실행 결과는 아래 별도 통합 기록을 따르며 과거 테스트 수/통과를 복사하지 않는다. 기술 구현/검사는 완료했고 공식 검토 기록은 별도 미완료다.

- [ ] 정정 후보의 최종 리뷰를 별도로 수행한다. 과거 review-state/verdict는 갱신 거부·재시도 금지 상태이며 다른 도구/파일로 공식 승인을 우회 재구성하지 않는다. 기존 passed=false는 유지한다.

### 이전 P1 실행 결과 (2026-11-14) — 사용자가 거부한 스키마의 역사

> **역사 기록:** 이 절과 바로 다음 과거 독립 검토 절은 잘못된 정적 Scene 표시 바인딩을 포함했던 당시 후보의 결과다. 사용자 지적으로 해당 스키마/참조/역할/nullable Scene 예외는 폐기했다. 아래 명령 결과·수치·당시 진단은 정정본 검증이나 현재 설계의 근거로 재사용하지 않으며 UI 런타임 구현 증거도 아니다. 당시 공식 PASS는 미완료였다.

- 실제 `Scene.xlsx`: Scene 3행, SceneOption 12행(각 타입 네 개), Config 3행, TextArt 4행, Localization 20행. 기존 Enum 값 0~8과 유지할 ID/번역 키를 보존하고 새 행동 값 9~13을 뒤에 추가했다.
- 새 Scene 필드: PlayerArtId/ContentArtId/TitleTextKey/RegionTextKey. TextArt는 ArtId/AssetPath/Width/Height이며 `ArtId #Scope=Shared`, 나머지는 Client다. optional 참조 세 필드는 값/기본값을 모두 비울 수 있다.
- 시안 literal은 변경하지 않고 로고 83×41과 캐릭터/몬스터/마을 36×36 세 파일을 005/007의 경로에 추출했다. 실제 세 시안 모두 132×43이며 아트 양쪽 여백에 잘려 나간 픽셀이 없음을 검사했다.
- 기존 C++ 런타임 소스·시안·ASCII 자산·템플릿·P0 출력 관리자 소스를 보존했다. 새 자산 네 개의 실제 디렉터리와 vcxproj/filters 등록을 맞췄다.
- 생성 헤더는 Class에 6개, Client YAML 5개와 schema YAML 6개는 Data에 생성했다. `--check`는 논리 생성 항목 18개를 보고한다(분리 출력 루트 소유권 마커는 각각 유지).

| 실행 | 실제 결과 | 범위 |
|---|---|---|
| 생성기 Release 빌드 및 `--self-test` | 종료 0, SceneContract PASS 121항목, 기존 self-test도 통과 | 필수 Client 필드/참조/4개/루트/아트·실패 출력 보존 등. PASS 항목 수는 서로 다른 검증을 포함 |
| `python Tools/Tests/test_scene_generator_contract.py --generator Out/GameDataGenerator/Release/GameDataGenerator.dll --scratch-root <소유한 임시 경로>` | 35 사례 통과 | 유효 데이터, 불량 metadata/값/UTF-8/크기/파일·경로, 실패 시 입력/양쪽 출력 보존, BOM·CRLF·nested workbook |
| `python Tools/Tests/test_scene_ui_data.py` | unittest 9개 통과 | 원본 crop 일치·네 옵션·참조·현지화·프로젝트 필터·UTF-8 설정. openpyxl 필요 |
| 실제 기본 데이터 `--validate-only` → 생성 → `--check` | 모두 종료 0, 6개 테이블/18개 논리 항목 | 실제 Class/Data 적용. 원본 workbook 직접 변경은 승인된 마이그레이션뿐 |
| 실제 FDataScene/FUIManager/FFrameBuffer 소비자 probe | 3 Scene 로드/렌더/네 슬롯·NewGame→Town/4번 Quit·비활성 확정 차단 통과 | 기존 런타임과 새 생성 데이터 호환. Unicode TextArt 표시/새 UI 입력 구현 검증 아님 |
| std 테이블 생성→MSVC v145 빌드→실행 | 최종 종료 0 | 수정 전 생성은 성공하지만 C++ C2039로 실패했고, ::std 한정 후 컴파일/실행 성공 |
| 별도 logo ID/83×41 파일을 Excel에 설정→생성 | 종료 0, 생성 Scene/TextArt 참조 갱신 | 코드 경로 리터럴 없이 새 로고 구성 가능. 실제 화면 표시는 P3 |
| 전체 솔루션 Rebuild | Debug/Release × x64/Win32 모두 종료 0, 경고/오류 0 | 솔루션 플랫폼 x86은 프로젝트 Win32에 대응 |

초기 Debug x64에서 기존 UTF-8 `FDataScene.h` 시안 주석을 CP949로 해석한 **C4819 두 진단**을 확인했다. `/we4819`로 실제 컴파일 실패를 재현한 뒤 공통 `/utf-8`을 지정해 PCH/소스 문자 집합을 일치시켰다. 사용자 주석을 바꾸거나 경고를 억제하지 않았으며 최종 네 구성 모두 같은 warning-as-error 조건으로 통과했다.

직접 dotnet 빌드 후보와 솔루션 MSBuild 재빌드의 DLL 바이트가 달라 최종 Out/Release 산출물을 따로 복사해 self-test/CLI/생성 check/std probe를 다시 검증했다. 최종 산출물과 그 검사본의 해시 일치를 확인했다. 서로 다른 실행 묶음의 검사 수를 합산해 전체 테스트 수라고 하지 않는다.

상세 임시 증거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/p12-ui-contract/`의 `integration-published`, `integration-final`, `parent-final-audit.json`, `generator/evidence.json`. 독립 리뷰 결과와 제약은 아래에 기록한다. P0의 기존 실행 기록은 아래 원문 그대로 보존한다.

### 과거 독립 검토 결과와 당시 한계 — 거부된 스키마의 역사

- 동결 후보 52개 파일과 컴파일 설정/회귀 검사 보완 2개 파일을 검토했다. 최종 verdict의 `security_concerns`/`logic_errors`는 비어 있으며 **신규 제품 차단 결함은 확인하지 못했다.** 이것은 P3 또는 전체 프로젝트 무결함 보장이 아니다.
- `review-state.json`의 증분 갱신이 전체 읽기 판정 문제로 거부된 뒤 재호출은 승인 타임아웃으로 차단됐다. 명시된 재시도/다른 도구 우회 금지를 준수했고 초기 상태 파일을 완료 상태로 재작성하지 않았다. 별도로 저장된 `review-candidate-1/verdict.json`의 **passed=false를 그대로 유지**한다. 기술 결과와 검토 산출물 완결성을 구분하며 공식 PASS 체크는 닫지 않는다.
- 문서의 남아 있던 `TargetSceneId Default MainMenu` 설명은 실제 nullable 계약(기본값 없음/YAML null/C++ 빈 wstring)으로 정정했다. 코드·원본 데이터·생성물은 이 정정에서 바꾸지 않았다.
- **기존 검증 한계:** 레거시 `Scene.AssetPath`는 경로/존재 여부를 확인하지만 파일 내용의 ASCII 전용 여부까지 생성기에서 검사하지 않는다. Unicode Town 아트 경로를 넣으면 생성기는 통과할 수 있지만 `FTextScene`은 비ASCII를 거부한다. 신규 회귀는 아니며 현재의 세 ASCII 경로/파일과 소비자 probe는 정상이다. 당시 새 그림을 `ContentArtId/PlayerArtId → TextArt`로 연결하라는 지침은 사용자 정정으로 폐기했다. 현재는 컴포넌트의 런타임 대상/아트 멤버로 공급한다. 레거시 콘텐츠 검증 보강 또는 해당 경로 폐기는 P3 로더 이관 때 별도로 처리한다.

### Scene 런타임 바인딩 정정 기록 (2026-11-14) — 원본·지침 반영 및 통합 기술 검증 완료

- 사용자 정정에 따라 실제 Scene.xlsx와 생성 Scene 데이터는 **SceneId/SceneType/AssetPath/IsStartupScene 네 정적 필드만** 유지한다. 네 표시 필드는 GD2132, 그 외 추가 필드는 Scope와 무관하게 GD2133 및 실제 metadata 위치로 거부한다. Scene→TextArt/제목·지역 필수 참조·역할 매핑·nullable Scene 예외는 제거했고 TargetSceneId 예외는 유지했다.
- 현재 아트·이름·지역·상태·로고는 런타임 UI 구성 객체의 현재 대상/멤버가 소유하는 설계다. TextArt는 독립 선택적 metadata이고 로고 83×41/캐릭터·몬스터·마을 36×36은 컴포넌트 슬롯 계약과 구분한다. 실제 UI 구성 클래스·Unicode 로더·InputManager는 P3 미구현이다.
- 사용자가 `Scene.xlsx 닫았고 AGENTS.md도 수정해`라고 명시적으로 승인한 뒤 잠금 해제 및 최신 원본 해시 불변을 확인했다. 검증한 수정본을 원본에 게시하고 다시 열어 확인했으며 다른 다섯 시트와 기존 네 정적 필드 값은 보존했다. AGENTS.md의 잘못된 Scene 표시 참조 문구도 같은 승인 범위에서 정정했다.

증거 루트는 소유한 scratch의 `scene-runtime-binding-correction/`이며 이번 실제 원본 통합 결과는 `live-integration/`에 별도 보관한다. staged/private 검사와 실제 원본 게시 검사는 구분한다.

| 검증 항목 | 실제 결과 / 증거 |
|---|---|
| 실제 원본 workbook 게시 | 완료. 네 정적 필드·나머지 시트 값 보존·재읽기. `live-integration/publication.json`. 게시 SHA-256 `66c42ab082eff163d8c69f722c49037c0d562f9212ff977f0a493a92624a0928` |
| 실제 C++/YAML 생성·validate/check | 종료 0, 6 tables / 18 generated files. `GameData/Class`, `GameData/Data/Client`, `GameData/Data/Schema`에 게시. `live-integration/validate.log`, `check.log` |
| 실제 원본 Python 데이터/자산·빌드 의존성 회귀 | **11/11 PASS**. `live-integration/data-tests-after-dependency.log`. 앞선 수정본 10개 검사에 솔루션 생성기 선행 의존성 회귀 1개를 추가 |
| C# Release·전체 self-test | 빌드 경고/오류 0, 전체 self-test 종료 0. `final-tool-build.log`, `final-self-test.log`. SceneContract PASS 표시는 117개이며 출력 보존 별도 표시를 포함하므로 독립 사례 수로 합산하지 않음. P0의 의도적 backup-cleanup 경고는 컴파일 경고가 아님 |
| 실제 CLI 회귀·입력/두 출력 보존 | **41/41 PASS**, 음성 34사례 보존. `final-cli.log`의 logDirectory와 results.json. 검사 DLL과 실제 최종 Release DLL은 byte-identical이고 실제 게시 workbook도 검사한 수정본과 byte-identical |
| 실제 게시 헤더·데이터 C++ 소비자 | MSVC 빌드/실행 종료 0. 실제 FDataScene 3개·TextArt 4행, 네 표시 멤버 없음 compile-time 확인. `live-integration/consumer-build.log`, `consumer-run.log` |
| 전체 실제 솔루션 병렬 Rebuild | **Debug/Release × x64/x86(프로젝트 Win32) 4/4 성공**, 각 경고 0·오류 0. `live-integration/matrix-after-dependency/matrix.json`과 네 로그 |
| 병렬 빌드 통합 보완 cycle2 | 최초 Debug x86에서 ClosedXML 누락 발생. CopyLocal은 이미 true였고 두 경로가 같은 output/obj를 사용하며 솔루션 선행 의존성이 없었다. 동일 구성의 직렬 진단 빌드는 성공. TextRPG.sln에 기존 GameDataGenerator GUID 의존성만 추가하여 중복 clean/build 순서를 제어했고 이후 네 구성을 기본 병렬로 검증. GUID·기존 구성/매핑은 보존. 최초 실패·직렬 진단·dependency-test RED 로그는 삭제하지 않음 |
| 지침 동기화 | 사용자 재승인으로 AGENTS.md 정정 완료. 이전 승인 타임아웃 기록은 역사로 보존하며 그때 우회하지 않았음 |
| 보완 코드 기술 재확인 | cycle1의 임의 다섯 번째 필드 수용은 whitelist로 보완. 같은 반례 exit0→GD2133/Scene F13 exit1을 확인했고 보완 3소스 읽기 전용 재검토에서 구체 잔여 결함 미발견. cycle2 빌드 순서도 동결 5파일 해시/구성을 대조한 읽기 전용 재확인을 완료했으며 구체적인 잔여 결함은 발견하지 못함. 정상 회귀와 의존성 삭제/잘못된 GUID 등 음성 대조 5개를 확인했고 별도 독립 빌드 프로세스 간 전역 잠금으로 확대 해석하지 않음 |
| 공식 리뷰 기록 | **미완료 상태 보존**. 과거 p12와 independent-code-review coverage 갱신 거부/부정 verdict를 수정·재시도·우회하지 않음. 이번 원본/지침 승인은 해당 리뷰 기록 수정 승인으로 해석하지 않음. 기술 검사/제한된 재확인을 공식 전체 PASS로 바꾸지 않음 |

최종 검사본과 실제 Release DLL SHA-256은 `deb5865ca04750ce466397b09ba17ccdf1df3c6376a1dac60a93bcc5a348082c`다. staged 단계의 `final-parent-audit.json`은 당시 차단 상태의 역사로 유지하고 실제 원본 통합/보존 정체성은 `live-integration/live-audit.json`으로 기록한다. 위 결과는 P3 UI 구현이나 몬스터/스킨 실제 화면 교체 기능 완료가 아니다.

## 5. P2 — Scene 소유 UI와 교체 가능한 입력 설계

UI/배치 기준은 [007_Scene별UI객체및입력분리설계안.md](GameDesign/007_Scene별UI객체및입력분리설계안.md)와 [005 데이터 계약·편집 예시](GameDesign/005_Excel게임데이터및경로관리설계안.md)다. 입력 상태/Layer의 최신 책임은 [008](GameDesign/008_InputManager및SceneUILayer설계안.md)이 우선한다. **아래 체크는 정책·데이터 설계 확정만 뜻한다. 런타임 구현/실제 호스트 통과는 P3 미완료**로 남긴다. 004의 하단 패널·옛 해상도 정책은 충돌 범위에서 대체한다.

### UI-D01: 출력 방식과 좌표 단위

- [x] Windows 콘솔/Win32 사용, 세 Scene의 논리 화면 132×43을 확정한다.
- [x] 기존 1240×940 일반 창을 선호값으로 유지한다. 최대화/전체화면이 아니며 가용 셀 수 보장이 아니다.
- [x] UI 셀/screen buffer/창 픽셀 좌표와 viewport 원점 변환을 분리한다.
- [x] 기본 수용 호스트는 classic conhost/Consolas, Windows Terminal은 best effort로 정한다. 132×43 미만은 현지화 안내와 키/마우스 탐색·확정 차단을 적용한다. 실제 폰트/DPI/호스트 검증은 P3 UI-01이다.
- 1920×1080·120×40/80×25는 과거 정책이며 현재 기준으로 자동 적용하지 않는다.

### UI-D02: 책임·소유권·위치

- [x] Scene이 UI 루트/하위를 단독 소유하고 각 UI가 부모 콘텐츠 기준 좌측 상단 위치·크기를 가진다.
- [x] 루트 포커스/캡처, 버튼 hover/누름, 선택 패널 selected OptionId(없음 허용), 로그 스크롤/추적의 단일 원본을 확정한다. 선택지 페이지 상태는 없다.
- [x] 렌더/hit-test가 같은 rect/clip을 공유하며 게임 액션 해석은 Scene에 둔다. FUIManager는 비소유 연결/공통 라우팅으로 축소한다.
- [x] 새 Scene/아트/UI는 prepare/validate 후 전환하고 실패하면 이전 Scene/UI를 유지한다. dispatch 종료 후 전환·비소유 연결/캡처 해제·트리 파괴의 순서를 정한다. 코드 구현은 P3 UI-06이다.
- 기존 `UI/MainMenu`, `UI/Town`, `UI/BattleField`를 재사용한다. `UI/Common`/`Input`은 P3 제안 경로다.

> 이전 정책 검토 기록(Modal/NonModal 관통 범위는 008로 대체): 2026-11-14 추가 검토: [007 §4](GameDesign/007_Scene별UI객체및입력분리설계안.md)의 IUI/팝업 설계는 권장안이다. `IUI` 필수 Render/선택 입력 콜백, `FSceneUiRoot`의 `vector<SUiEntry>` + `unique_ptr<IUI>`, Base→Popup 렌더/역순 hit-test, Modal 경계·지연 삭제·포커스 복원을 제안한다. 사용자 요구 방향과 세부 제안/미구현을 구분하며 UI-06을 구체화하고 UI-10을 후속 검증으로 추가한다. 기존 네 선택지·Escape 즉시 Quit·정적 Scene 계약은 변경하지 않는다.

### UI-D03: 선택지 상태

- [x] 모든 SceneType의 **전체 옵션은 비활성 포함 정확히 네 개**, order 1~4다. 숨김·다섯째 항목·페이지/페이지 버튼/초과 스크롤은 없다.
- [x] 미구현 행동은 visible/disabled/회색으로 표시하고 확정하지 않는다. 데이터 IsEnabled와 구현/게임 상태를 AND한 것이 유효 활성이다. 활성 무응답을 허용하지 않는다.
- [x] 초기 선택은 첫 활성, 비활성화되면 다음 활성으로 순환 복구, 전부 비활성이면 selected OptionId 없음으로 정한다.
- [x] Up/Down은 활성 항목만 순환하고 숫자 1~4는 고정 order에 대응한다. 비활성 숫자는 no-op이며 다른 옵션으로 재매핑하지 않는다. 구현·상태 전이 검증은 P3 UI-03/09다.

### UI-D04: InputManager 교체 경계

- [x] InputManager는 입력 상태만 관리하고 const GetKeyStatus 조회를 제공한다. Scene은 UILayer/UITopLayer의 유일한 소유자로 최상위 Layer의 HandleInput만 호출한다. 일반 Update/Render에서의 입력 조회 우회를 금지한다.
- [x] None/Pressed/Held/Released 네 상태와 같은 프레임의 짧은 눌림/해제 전이 보존을 정의한다. 조회는 입력을 소비하거나 Layer 차단을 수행하지 않는다.
- [x] 키·마우스·휠 모두 최상위 Layer만 처리한다. 빈 공간·비활성·Unhandled도 아래로 전달하지 않으며 포커스/hit-test/gesture는 Scene/UI 측에 둔다. 설계 확정과 별개로 INP-03~05의 이번 구현/통합 검증도 완료했으며 근거는 008 §11을 따른다.
- [x] 공통 이벤트는 누름/해제·반복, 숫자 1~4, 마우스·휠·viewport/포커스 변경을 구분한다. 코드화는 INP-01~03이다.
- [x] Enter/숫자는 최초 KeyDown 한 번, 클릭은 같은 활성 버튼의 press/release 완료 시 한 번이다. 포커스 손실·viewport/레이아웃 변경·전환 시 눌림/캡처를 취소한다.
- [x] Scene 전환 후 입력 배치 잔여 이벤트 폐기 및 누르고 있던 확정 키/버튼 release 전 재확정 금지로 관통을 막는다. Escape는 기존 즉시 앱 Quit, 새 확인 모달 없음으로 확정한다. 미래 하위 메뉴 Back은 별도 범위다.

### UI-D05: 승인 배치와 txt 교체

- [x] 007 §5의 배틀/마을 3열과 MainMenu 로고 왼쪽/네 선택지 오른쪽 좌표를 확정한다.
- [x] 캐릭터/몬스터/마을은 정확히 36×36, MainMenu 로고는 정확히 83×41 TextArt이며 성공 후 교체/실패 시 기존 아트 유지로 정한다.
- [x] TextArt는 strict UTF-8·선두 BOM 선택·LF/CRLF·마지막 줄바꿈 하나·후행 공백 보존·명시적 단일 셀 허용 집합으로 검증한다. Town 시계 `○ U+25CB`도 명시 허용하며 임의 Unicode는 허용하지 않는다. 잘라내기/패딩/자동 크기 맞춤은 없다.
- [x] 긴 문구는 표시 셀 경계 word/char wrap 후 패널/슬롯 행 예산으로 clip한다. CJK를 반으로 나누지 않고 원문은 현지화에 보존하며 옵션 수/페이지는 늘리지 않는다.
- [x] 로그 본문 8행, 로그 위 wheel만 처리, 맨 아래면 추적/과거면 유지, trim/resize 후 clamp로 정한다.
- [x] renderer가 단일 프레임을 지우고 합성하며 최초/입력/UI·게임 상태/resize 시 dirty redraw하는 정책이다. 기존 front/back·주기적 출력 제거는 P3 UI-05다.

### UI-D06: 데이터·표현 정책 경계

- [x] 운영 문구/콘텐츠/아트 참조는 Excel, 그림은 txt, 좌표·테두리·클리핑·입력 알고리즘은 엔진 계약이다. 한국어/영어 번역을 모두 제공한다.
- [x] 사용자 추가 승인으로 Scene은 세 정적 필드(SceneId/SceneType/IsStartupScene)만 유지한다. 네 표시 컬럼·Scene 표시 참조·SceneType 역할/지역 필수·nullable Scene 예외를 폐기한다. TextArt(ArtId/AssetPath/Width/Height)는 독립 선택적 카탈로그이며 있으면 36×36/83×41을 검증한다. SceneOption의 목적지/현지화 필수 참조와 TargetSceneId만 nullable 예외는 유지한다.
- [x] `Scene.AssetPath`와 Asset/Scene 레거시 배치 txt를 제거하고 Unicode `TextArt.AssetPath`의 검증은 유지한다. 요청 로고의 원본 바이트·브랜딩은 보존하되 UI 조작 문구는 현지화로 관리한다.
- [x] Scene 인스턴스가 UI를 소유하되 SceneType별 네 옵션 데이터를 공유하며 `MaxVisibleOptions=4`/`NoScroll`이다. MainMenu New/Load/Options/Exit, Town Talk/Shop/Inn/Move, BattleField Attack/Skill/Item/Defend 순서와 기존 ID/Enum 값 보존·추가 규칙은 005에서 관리한다.
- [x] 005/007에 세 정적 Scene 필드·독립 카탈로그 편집 예·현재 몬스터 교체의 개념 멤버 예·원본 crop 이력/레거시 폐기를 명시한다. 실제 정정 파일/회귀 결과는 P1 통합 실행 기록, 런타임 바인딩 구현은 P3 UI-08/09로 분리한다.
- [x] FCharacterUi/FMonsterUi/FTownUi/FMainMenuUi의 현재 대상/아트/이름·지역 멤버 소유 정책과 같은 Scene의 랜덤 조우/스킨 변경을 명시한다. 이들은 제안 클래스이며 현재 구현됐다고 주장하지 않는다. 정적 Scene→엔티티 배정표로 대체하지 않는다.

**P2 상태: 사용자 지적에 따른 소유권·스키마 정책 정정 반영.** 정책 승인과 구현 완료는 다르다. 아직 없는 컴포넌트/버튼/Scene UI/TextArt 로더/입력/호스트 동작을 완료로 표시하지 않는다. 정정 P1 실행 검증과 공식 리뷰 미완료 상태는 §4의 통합 실행 기록을 따른다.

## 6. P3 — UI 객체 구현과 입력 교체

### 우선 실행할 순서

**완료한 입력 이관 순서: INP-01 → INP-02 → INP-03 → INP-04 → INP-05**. 기존 일부 구현인 공급자/UI 기반을 재사용하며 008의 상태 서비스·Scene Layer 경계로 정리한다. UI-06/07/08/09와 UI-01~05의 나머지 기준은 별도로 유지한다.
렌더링 전용 UI는 INP-02와 병행할 수 있다. 전체 InputManager를 먼저 완성할 필요는 없다. 실제 마우스/휠/반복 처리는 최소 Manager 연결 후 검증한다. P1 데이터 계약 수정은 병행 가능하되 영향을 받는 실제 콘텐츠 통합 전에 완료한다.

추가 확장 UI-10은 UI-06과 INP-03 뒤에 진행하고 UI-05의 dirty 합성과 통합한다. 테스트 입력 공급자로 구조를 먼저 검증할 수 있으나 실제 마우스/휠 완료 표시는 INP-03 연결 뒤다. 팝업을 이유로 기본 세 Scene UI 조립이나 정적 데이터에 새 스키마를 선행 강제하지 않는다.

| ID | 작업 | 선행 / 완료 기준 |
|---|---|---|
| INP-01 | 공통 입력 타입/상태·기존 공급자 계약 정리 | Input 헤더 경계, 네 상태/전이, Event/Timeout/Closed/Error 보존. 기존 부분 구현과 목표 완료를 구분 |
| INP-02 | Windows 공급자와 Manager 상태 축적 경계 정리 | INP-01. OS 조회는 공급자에만, 기존 키/마우스/좌표·오류 의미 보존 |
| UI-06 | IUI/FUiElement/FUiPanel/FSceneUiRoot·좌표·Scene 단독 소유 구현 | 입력 계약 확정 후. Render 필수/입력 기본 처리, vector entry + unique_ptr, Base/Popup 순서·역순 hit-test, 안정 UI ID·지연 변경, 부모/자식 좌표와 렌더/hit-test 일치 |
| UI-07 | Unicode TextArt 로더·36×36/83×41 검증·안전 교체 | UI-06. 크기/인코딩/경로 오류 거부, 기존 아트 보존 |
| UI-08 | MainMenu/Town/BattleField UI 조립 | UI-06/07. 승인된 132×43과 로고 왼쪽/선택지 오른쪽 재현 |
| INP-03 | 상태 전용 FInputManager 구현 | INP-01/02. 네 상태·비소비 조회·짧은 전이/반복·Held·마우스/휠 검증, UI/Layer 의존 없음 |
| INP-04 | Scene UILayer/UITopLayer와 입력 단계 분리 | INP-03 및 UI-06 기반. 최상위 Layer만 처리, 키/마우스/휠 동일 차단, 일반 Update와 분리 |
| INP-05 | 실제 Scene/UI 통합·경계 회귀 | INP-03/04 및 UI-02/04/10. 008 수용 사례·기존 OptionId/좌표/전환/release gate·최종 EXE와 필요한 빌드/분석 검증 |
| UI-09 | 기존 FUIManager 상태 이관·현재 대상 컴포넌트 바인딩 | 009의 InGameData 진행 원본에 비소유로 연결. FCharacterUi/FMonsterUi/FTownUi는 HP·Stats를 소유하지 않고 조회·표현하며 로고 등 정적 정의는 Excel에서 공급. UI 조작 상태·표시 모델만 소유. OptionId·네 슬롯 유지, Scene 행은 현재 엔티티를 배정하지 않음 |
| UI-10 | 팝업 수명·입력 범위·복원·클릭/드래그 경계 | UI-06 및 INP-03 뒤, INP-04/05와 연계. 008의 최상위 Layer 전용 정책과 007의 수명/gesture 사례 검증. UI-05 합성·실제 팝업 콘텐츠/UX/pause는 별도 |

- [x] INP-01 공통 입력 계약 코드화.
- [x] INP-02 기존 입력 어댑터 분리.
- [ ] UI-06 IUI·UI 기본 객체·좌표·Scene 루트의 vector/unique_ptr 소유권·안정 ID·지연 명령 구현.
- [ ] UI-07 Unicode txt 로더와 안전한 아트 교체 구현.
- [ ] UI-06/UI-05 Panel 상단 테두리 안의 선택적 현지화 제목 구현. `┌─`처럼 왼쪽 모서리 뒤 가로선 한 셀 다음에 표시하고 셀 폭 기준 한 줄 clip·양쪽 모서리와 앞쪽 가로선 보존·기존 콘텐츠 영역 유지·제목 변경 잔상 제거를 검증한다. 세부 기준은 007 §4의 Panel 상단 테두리 제목을 따른다.
- [ ] UI-08 Scene별 UI 조립 구현.
- [x] INP-03 상태 전용 InputManager와 네 상태·비소비 조회·빠른 전이/반복·오류 보존 검증.
- [x] INP-04 Scene UILayer/UITopLayer, 최상위 전용 입력과 일반 Update 분리, UI/Manager 간 상태 중복 없음 검증.
- [x] INP-05 실제 Scene/UI 통합, 008 수용 사례·기존 OptionId·좌표·전환/release gate 및 필요한 native/EXE 검증.
- [ ] UI-09 상태·현지화·콘텐츠/자산 데이터 바인딩 이관.
- [ ] UI-10 팝업 중첩/중간 닫기·최상위 Layer 전용 입력·disabled/Unhandled 관통 차단·포커스 복원·동일 입력 배치/release gate·재합성/잔상 제거 검증. Base 객체/선택/로그 유지와 준비 실패 보존을 확인한다.
- [ ] UI-10 클릭/드래그 상호 배제·영역 밖 release·포커스/resize/삭제 취소·콜백 중 자기 닫기·vector 재할당 후 ID 유효성을 검사한다. 실제 콘텐츠 팝업은 이 구조 검증과 구분한다.
- [ ] UI-08/09 같은 Scene에서 InGameData의 조우 몬스터 A→B·캐릭터 스킨 변경을 비소유 대상/아트/이름·HP/Stats 표시에 반영하고, 실패 시 이전 진행/표시/참조 수명을 보존한다. UI가 진행 원본을 소유/복제하지 않는지 검사한다. FTownUi 현재 마을/지역 및 FMainMenuUi 83×41 로고 바인딩도 검증한다.
- [ ] UI-D01 호스트/DPI/글꼴·132×43 viewport 부족 현지화 안내와 탐색/확정 차단을 실제 conhost/Consolas에서 확인한다. Windows Terminal 결과는 best effort로 별도 기록한다.
- [ ] UI-D02 Scene 전환 prepare/validate/commit, 비소유 연결 해제/캡처 취소/이전 Scene 유지 절차를 구현·검증한다.
- [ ] UI-D04 Enter/숫자 최초 누름·동일 활성 버튼 release 확정·포커스/viewport/레이아웃 변경 취소·전환 잔여 배치 폐기/release gate·Escape 즉시 Quit을 구현·검증한다.

### IUI 구현 기록 — 2026-11-14, 기반 구현/보완 검증 완료

- 구현: `UI/Common`의 IUI/FUiElement/FUiPanel/FUiButton/FSceneUiRoot/FUiTextArt, `UI/MainMenu/FMainMenuUi`, `Input/IInputSource`와 FConsoleInput. FDataScene이 루트를 단독 소유하고 FUIManager는 비소유 bridge로 축소했다.
- UI-06/UI-10 기본 구조: vector entry/unique_ptr·안정 ID·지연 팝업 명령, Modal/NonModal/PassThrough, 포커스 복원·캡처/드래그 취소·동일 입력 배치/release gate. 실제 팝업 콘텐츠/문구·pause 정책은 추가하지 않았다.
- 제목/버튼: Panel 상단 `┌─제목──┐`, 셀 폭 clip/모서리 보존, 부모 기준 버튼 좌표·동일 기하 hit-test. 운영 제목은 현지화 키로 공급한다. 일반 부모 이벤트 bubbling·hover/눌림별 시각 테마·자식 동적 재조립은 아직 확장하지 않았다.
- INP-01/02 기본 경계: Main 내부 입력 코드를 IInputSource 구현체로 분리하고 기존 SInputEvent를 확장했다. 실제 Win32 레코드의 key down/up·숫자·repeat·포인터·wheel·focus/viewport를 정규화한다. 공통 Input 헤더 이동과 완성 FInputManager 서비스는 아직 없다.
- UI-07 부분: TextArt strict UTF-8/허용 문자·정확한 크기·경로 검사와 실패 시 기존 아트 보존, MainMenu의 런타임 로고 멤버·카탈로그 부재 구성을 검증했다. UI-08/09의 Town/Battle 실제 엔티티 아트/상태/로그는 빈 Panel 단계이며 P3 전체 완료가 아니다. 레거시 ASCII 자산은 준비 단계 검증만 하고 새 화면에 합성하지 않는다.
- 부모 실행: `test_scene_ui_runtime.py` runtime 17 checkpoint group 및 `--without-textart` fixture 종료 0. 네 구성 솔루션 Rebuild/게임 native `/analyze` 경고·오류 0, 헤더 정책과 네 구성 PCH-off 성공. 증거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/iui-implementation/parent-fixtures/`, `parent-verification/results.json`.
- 실제 Debug x64 실행 파일의 별도 콘솔 smoke에서 132×43 viewport·메인메뉴 네 선택지·Enter Town 전환/네 선택지·Escape 정상 종료를 확인했다. `iui-implementation/console-smoke/result.json`과 실제 콘솔 텍스트 덤프를 보존했다. 시각/DPI 수동 검증을 대체하지 않는다.
- 원본 Excel·사용자 시안·FDataScene.h의 기존 아트 주석은 보존했다. 파일/VS 필터 일치를 확인했다. 최초 독립 검토에서 다섯 결함이 재현되어 FAIL이었고 초기 판정/반례는 보존했다. 1차 보완 뒤 부모 runtime 24 checkpoint group 및 카탈로그 부재 fixture, 네 구성 빌드/분석/PCH-off, 실제 EXE 콘솔 smoke가 통과했다. 최신 증거는 `iui-implementation/parent-verification-repair1/`, `parent-repair1-fixtures/`다. 변경 7파일의 독립 집중 재검토는 scoped PASS로 구체 잔여 차단 결함을 발견하지 못했다. frozen 41 source 해시와 live 일치를 확인했다. 최초 검토/FAIL과 보완 재검토를 별도 보존하고 전체 P3/전수 재인증으로 확대하지 않으며 실제 conhost/Consolas/DPI/창 크기 수동 검증과 dirty 스케줄링은 미완료다. 아래 체크는 전체 수용 기준이 충족될 때만 완료로 바꾼다.

### 기존 결함·정책 구현 체크 유지

- [ ] UI-01: 최소 viewport 미만에서 렌더 경고와 키/마우스 입력 차단을 일치시킨다. 현재 시안 기준은 132×43이며 기존 하단 패널의 80×25 조건을 새 배치에 그대로 적용하지 않는다.
- [ ] UI-02: 방향키 반복 횟수를 보존하고 확정 반복·Scene 관통을 차단한다. INP-03에서 해결하되 기존 재현 조건을 회귀 검사한다.
- [ ] UI-03: 정확히 네 슬롯과 안정적 OptionId, 비활성 회색/확정 불가·숨김 없음, 초기/비활성화 선택 복구·전부 비활성 선택 없음, 고정 숫자키/no-op 계약을 구현한다.
- [ ] UI-04: 동일한 계산 rect/clip을 렌더와 hit-test가 공유한다.
- [ ] UI-05: 긴 ko-KR/en-US 문구의 셀 경계 wrap/clip, 로그 8행·wheel/추적·trim/resize clamp, 단일 합성·dirty 렌더/잔상 제거를 구현한다. 옵션 페이지는 만들지 않는다.
- [ ] 파일을 추가할 때 실제 디렉터리·vcxproj·filters를 함께 맞춘다. 기존 UI 폴더를 불필요하게 재배치하지 않는다.

입력 이관 완료 기준은 [008](GameDesign/008_InputManager및SceneUILayer설계안.md)의 INP-01~05/수용 사례이며, 다른 UI 기준은 [007 §10](GameDesign/007_Scene별UI객체및입력분리설계안.md)의 정상·실패·경계 사례를 유지한다. **문서와 정적 시안만으로 InputManager/버튼 이벤트/아트 교체가 구현됐다고 표시하지 않는다.**

## 7. P4 — 현지화·Scene·콘텐츠 연결

### InGameData 기반 — 정책과 실제 구현 구분

- [x] IGD-D01: 모든 정적 정의는 Excel, 모든 진행 상태는 InGameData 단일 소유 루트, Scene/UI는 진행 비소유 조회라는 계약과 전체 내부 객체 직렬화·암호화 저장·Continue 복원 요구를 [009](GameDesign/009_InGameData및저장복원설계안.md)에 확정한다. 문서 완료이며 기능 완료가 아니다.
- [x] IGD-01: 최소 InGameData/하위 값 모델·FGameSession 앱 수명 소유·Excel 기반 새 게임 초기화 구현. Scene 교체로 진행을 초기화하지 않는다(009 §9.1).
- [x] IGD-02: 현재 루트와 모든 내부 값의 bounded 직렬화/역직렬화·버전/범위/참조 검사·오류 시 출력/활성 상태 보존 구현·검증. 미래 도메인/마이그레이션까지 완료한 뜻은 아니다.
- [x] IGD-03: 같은 PC/Windows 사용자 DPAPI 전용 키·AES-256-GCM·평문 파일 없는 안전 저장과 변조/키/I/O 실패 보존 구현·검증. 키 이동/복구 및 전원 차단 보장은 제외한다.
- [x] IGD-04: MainMenu Continue의 전체 후보 복원·검증·재개 Scene 준비·EndFrame 결합 게시, 실제 별도 프로세스 HP7/Gold4321 복원과 Quit round-trip·실패 보존 검증.
- [x] IGD-05: FInGameStatusUi의 HP·파생 Stats·Gold·인벤토리/업적 개수 비소유 조회 및 진행 교체 참조 수명 검증. UI-08/09의 모든 엔티티 아트·전투/장비·업적 전용 UI/게임플레이는 별도 미완료다.
- [x] IGD-06: 기존 runner/native MSBuild·원본 직접 참조·타깃 PCH/obj를 유지해 모델/저장/프레임 경계와 실제 EXE 회귀를 편입·검증. source-only PASS와 제품/제어 흐름 실행을 분리한다. 최종 전체 집계 기록은 상단 경로를 따른다.

최소 진행 모델/정적 정의→codec·인증 저장→Continue/UI 비소유 조회를 연결했다. 구체 필드·v1 포맷·Default 슬롯·DPAPI 키·autosave는 009 §9에 확정했다. 다른 PC/사용자 이동·키 복구/회전·버전 마이그레이션·전투 저장/정확한 재개 및 확장 콘텐츠는 미보장/후속 범위다. 인벤토리·업적 전체 콘텐츠나 범용 저장 프레임워크 완료로 확대하지 않는다. 위 체크는 실제 코드·해당 실행 게이트 확인이며 조정자 최종 전체 수용 기록과 구분한다.

### LOC-01: 현지화 반환 수명

- [ ] `TextRPG/Localization/Localization.cpp:8–15`의 누락 키 경로를 수정한다.
- 문제: 임시 std::wstring 인자를 그대로 const 참조로 반환하여 호출 문장 종료 뒤 수명 위반 가능.
- 값 반환 또는 수명이 보장되는 fallback 저장소 등으로 API 계약을 정리한다.
- 완료 기준: 등록/미등록 키, 리터럴/임시 문자열/장수명 문자열 호출의 결과가 안전하고 수명 probe 통과.

### 현지화 일관성

- [ ] 선택지의 한국어 고정 경로를 언어 정책과 연결한다.
- [ ] 설정에서 언어 전환 시 현재 Scene·포커스를 유지하며 문구를 갱신한다.
- [ ] 키 누락·번역 누락·포맷 인자 불일치·긴 문구 정책을 정리하고 검사한다.
- [ ] 설정 보존 방식은 별도 합의 후 구현한다. 임의의 저장 포맷을 선구현하지 않는다.

### 콘텐츠 연결

- [ ] Town 대화·상점·여관·이동의 실제 기능을 순서대로 구현/연결한다. 현재 네 옵션 밖의 가방은 자동 재노출하지 않는다.
- [ ] 미구현 기능의 노출 상태를 UI-D03에 맞춘다.
- [ ] BattleField 콘텐츠·상태·선택 액션을 공통 UI 계약으로 연결한다.
- [x] Continue는 IGD-04의 read-only 전체 후보 검증/EndFrame 게시와 실제 재시작 복원을 구현·검증했다. 데이터 허용과 실제 저장/Scene 검증 성공을 함께 만족할 때만 활성화한다.
- [ ] Settings의 구현 및 비활성 해제 기준은 별도다. Credits는 현재 MainMenu 네 옵션에서 제외하며 Enum/기존 번역 키만 보존한다.
- [ ] YAML 패키지 사용 범위를 확정한다. 인증·복호화만으로 게임 데이터 파싱/적용 완료라고 하지 않는다.

Town 액션과 BattleField 행동 미구현은 P4에 남긴다. 완성 기능의 회귀로 분류하지 않되, 데이터 계약은 이를 false로 두고 P3 UI에서 보이는 비활성 네 슬롯으로 처리하는 것이다. 실제 workbook 반영은 P1 부모 기록을 따른다.

## 8. P5 — 통합 검증과 문서 정합성

- [ ] P0/P1의 실패·경계 사례를 기존 self-test 또는 적절한 프로젝트 테스트에 편입한다.
- [ ] 선택지 전체 0/1/3/5/8개·누락/중복 order는 데이터 실패, 정확히 네 개·전부 비활성은 데이터 정상/선택 없음, 첫 활성 위치와 활성 변경 복구를 검사한다.
- [ ] 키보드·마우스·키 반복·확정 중복·Scene 전환·최소 크기 전후·리사이즈를 검사한다.
- [ ] 한국어/영어, 긴 문자열, 전각 문자, 지원할 Unicode 범위를 검증한다.
- [ ] 실제 지원 콘솔에서 배치·hover·click·클리핑·잔상을 수동 확인한다.
- [ ] 빌드 설정/경로 변경 시 Debug/Release × Win32/x64 및 전체 솔루션 빌드를 확인한다.
- [ ] 일반 기능 변경은 영향 범위 테스트와 필요한 빌드를 선택하고, 전체 매트릭스 미실행 여부를 명시한다.
- [ ] CS1668의 실제 LIB 설정 원인을 확인하고 처리 여부를 기록한다. 경고를 숨기기 위한 무조건 억제는 하지 않는다.
- [ ] 기존 설계 문서의 오래된 구현 상태·출력 경로를 현행 계약과 대조한다.
  - 예: GameData/Class 및 GameData/Data로 분리된 현행 경로와 과거 문서의 Build/Out 통합 생성 경로를 구분한다.
  - UI 설계의 제안/완료/미검증 표현을 실제 실행 증거와 맞춘다.

## 9. 작업 수행 규칙과 완료 기록

- 작업 단위는 위 ID 또는 단계 내 체크 항목을 사용한다. 리뷰 요청만으로 수정·커밋 권한을 추정하지 않는다.
- 원본 Excel과 소스, 생성물, 빌드 캐시를 구분한다. 생성물을 수작업 수정해 입력 문제를 가리지 않는다.
- 기존 작업을 stash/reset하거나 Git 초기화하지 않는다. 커밋·푸시는 별도 요청 범위다.
- 빌드·중간 산출물은 프로젝트 Out 아래, 일회성 probe는 소유한 scratch 아래에 둔다.
- 키·토큰·개인정보를 명령행·문서·로그에 기록하지 않는다.
- 작업 완료 시 변경 경로, 실제 명령/종료 결과, 해결한 ID, 남은 문제, 미검증 범위를 기록한다.
- 체크 완료는 해당 항목의 완료 기준을 따른다. P2 체크는 정책·데이터 설계 확정이며 런타임 통과 표시가 아니다. P1은 승인 범위지만 부모 실행 증거 전까지 미체크로 두고 P3~P5는 미구현/미검증으로 남긴다.

## P0 / DATA-01 실행 결과

- 경로: C++ 헤더 5개는 `GameData/Class`, YAML 9개는 `GameData/Data/Client` 및 `Schema`에 실제 생성했다. 각 생성 루트에는 소유권 마커가 별도로 있다.
- PKG-01: 같은 입력 파일의 절대/상대/대소문자/하드링크 출력 별칭을 거부한다.
- PKG-02: Win32 CREATE_NEW로 난수 임시 파일을 독점 생성하고 전체 쓰기·flush·close 성공 후 교체한다. 기존 `.tmp`는 건드리지 않는다. 동시 실행은 독립 임시 파일을 사용하며 마지막 성공 게시가 최종 결과다.
- `Tools/Tests/test_package_safety.py`: 구성별 10개 테스트, Debug/Release × x64/Win32 모두 통과(각각 skip 없음). 대상 잠금에 의한 게시 실패, 기존 파일 보존, 동시 게시, 인증 변조 거부 포함.
- 생성기: 기본/override 경로, check의 C++·YAML drift 및 읽기 전용, validate-only 무생성, 중첩 루트/비소유 출력 거부, 뒤쪽 구분자, 테스트 hook을 통한 두 번째 출력 게시 실패 및 양쪽 rollback을 검사했다.
- VS 2026 전체 솔루션 Debug/Release × x64/x86 빌드 통과. 솔루션 x86이 C++ Win32에 매핑된다. 최종 빌드 로그의 경고·오류 진단은 0건이다.
- 처음 솔루션 Platform=Win32를 사용한 검증 명령은 MSB4126으로 실패했다. 실제 솔루션 매핑을 확인해 x86으로 바로잡았으며 제품 구성은 변경하지 않았다.
- 새 한글 주석으로 노출된 C4819는 두 프로젝트의 해당 CPP 컴파일에 /utf-8을 명시하여 해결했다. 기존 CS1668은 managed Generator Exec에 native LIB 환경을 넘기지 않도록 한정해 해결했다. 경고 억제 옵션은 사용하지 않았다.
- 실제 생성 YAML 9개를 임시 키로 패킹·verify하여 엔트리 수를 확인했다. 키는 기록하지 않았다. YAML 게임 객체 파싱·게임 루프 연결을 검증한 것은 아니다.
- 원본 GameData 파일의 SHA-256은 작업 전과 일치한다. 이전 `Out/GameData/Generated`는 삭제하지 않았으며 새 생성 경로로 사용하지 않는다.
- 런타임 강제 종료나 악의적 동시 경로 교체까지 복구/방어하는 계약은 아니다. 두 생성 루트는 처리된 실패에 rollback하지만 프로세스/전원 중단에 대한 다중 디렉터리 원자성을 보장하지 않는다. 패커도 강제 종료 시 소유 임시 파일이 남을 수 있다.
- 1차 증거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/p0-integration/`의 빌드·테스트 로그와 `final-audit.json`. 아래 보완 전 후보에 대한 결과이며 최종 통과를 의미하지 않는다.
- 독립 리뷰 결과: PKG-01/02 차단 문제 없음. 생성기에서는 (1) 일반/extended Windows 경로 별칭이 같은 폴더를 가리킬 때 두 출력 경계 검증 우회, (2) 게시 후 읽기 전용 백업 삭제 실패가 일반 생성 실패로 반환되는 문제가 재현됐다. 두 항목은 별도 실패 테스트를 추가하여 보완했고, 부모의 hook self-test 및 전체 솔루션 구성/패커 테스트를 다시 통과했다. 이후 재리뷰에서 미존재 조상 아래의 잘못된 Windows 이름 검증 누락 1건을 추가 발견했고, 2차 수정에서 차단했다. 최신 두 C# 파일은 마지막 독립 재리뷰를 통과했다. 이전에 확인한 다른 6개 소스/구성 파일의 내용은 동일하며 해당 독립 검토 범위를 이어받았다.
- 후속 계약: 게시 완료 전 실패와 게시 완료 후 정리 문제를 구분한다. 물리적으로 겹치는 루트를 거부하며, 지원하지 않는 Windows 네임스페이스 경로는 쓰기 전에 거부한다.

- 보완 후보 추가 검사: 일반/extended·device·UNC 경로 거부, 기존 8.3 짧은 이름·SUBST 매핑의 동일/중첩 경로, 존재하지 않는 접미사 물리 경계 검사 통과. 해석할 수 없는 미래 짧은 이름(~ 포함)은 보수적으로 거부한다. 정션 fixture는 이번 보완에서 새로 실행하지 않았고 기존 reparse 거부 로직은 유지했다.
- 기존 조상을 Windows 핸들로 열어 volume GUID 및 정규화 경로로 비교한다. 입력·출력 트리와 드라이브 매핑은 검사부터 게시·정리까지 외부 변경 없이 안정적이어야 한다. 같은 트리의 동시 Generator 실행은 호출자가 직렬화한다.
- 두 출력 모두 게시된 후 백업 정리 실패는 `transactionState=committed`, `severity=warning`, `remainingBackupPaths`, `backupContentsMayBePartial=true`로 보고하며 성공 종료한다. 남은 백업이 온전한 이전 출력이라는 보장은 하지 않는다. 게시 전 실패의 rollback/실패 종료 계약과 구분한다.

- 2차 수정: 미존재 조상 아래 잘못된 문자/제어 문자/예약 장치 이름(확장자 및 COM/LPT 상첨자 숫자 포함) 20개 사례를 직접 루트 검사와 CLI에서 거부한다. 회귀 테스트가 실제로 실패하는 것을 확인한 뒤 구현했고, 파일뿐 아니라 빈 디렉터리도 거부 전후 스냅샷에 포함했다.
- 2차 후보의 hook self-test, 전체 4구성 솔루션 빌드 및 구성별 패커 10개 테스트, 실제 생성물 check/validate-only, YAML 9개 pack/verify를 다시 통과했다. 최종 감사는 원본 보존과 빌드 경고·오류 진단 0건을 확인했다.

- 최종 독립 재리뷰: 차단 항목 없음. 실제 소스 기반 별도 probe에서 잘못된/예약 이름 거부와 정상 이름 허용, 파일·디렉터리 보존을 확인했다. 소스/구성 8개 해시가 동결 후보와 일치함을 확인했다. 전체 소스 신규 전수 재리뷰가 아니라, 기존 검토와 변경된 두 파일의 집중 재검토를 결합한 판정이다.
- 비차단 후속 제안: 원본 표기의 trailing dot/space 정규화 경계, 확장자 앞 공백이 있는 예약 이름, COM0/COM10/CONSOLE 같은 정상 유사 이름을 영구 self-test에도 추가한다. 이번 P0 완료를 막는 항목은 아니며 임의 추가 구현하지 않았다.
- 최종 독립 판정 파일: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/p0-cycle2-independent-review.json`. 최종 빌드·테스트·원본 보존 증거는 `p0-integration/final-audit.json` 및 같은 폴더 로그를 참조한다.

## 10. 관련 문서

- [프로젝트 공통 지침](../AGENTS.md)
- [작업별 지침](../Agents/README.md)
- [검증 지침](../Agents/qa.md)
- [기반 게임 설계](GameDesign/001_GameDesign.md)
- [현지화 계약](GameDesign/003_다국어문자열및Excel관리원칙.md)
- [이전 공통 하단 패널 설계 — 과거 기록](GameDesign/004_UIManager및화면해상도설계안.md)
- [현재 Scene별 UI 객체·36×36 아트 기반 설계](GameDesign/007_Scene별UI객체및입력분리설계안.md)
- [최신 InputManager 상태·Scene UILayer 설계](GameDesign/008_InputManager및SceneUILayer설계안.md)
- [진행 데이터 InGameData 소유권·직렬화·암호화 저장/복원 설계](GameDesign/009_InGameData및저장복원설계안.md)
- [Excel 게임 데이터·경로 계약](GameDesign/005_Excel게임데이터및경로관리설계안.md)
- [YAML·패키지 계약](GameDesign/006_YAML내보내기이식설계안.md)
