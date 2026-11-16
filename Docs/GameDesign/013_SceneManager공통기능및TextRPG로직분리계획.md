# SceneManager 공통 기능과 TextRPG 로직 분리 계획

> 상태: **로컬 구현·검토 마감, 빈 출력 이슈는 사용자 승인으로 보류.** 사용자 승인으로 공통 Manager 추출과 게임 파생 연결을 구현했다. `APP-D32-EMPTY-OUTPUT`은 이번 작업의 차단에서 제외하고 같은 현상 재발 시 추적한다. SCM-06은 이 예외를 명시하여 마감하며 전체 검사 PASS나 원인 해결을 뜻하지 않는다. 커밋·푸시는 별도 사용자 승인으로 진행한다.
> 계획 기준: 2026-11-16, `2782feb656713fde315926eb8a85d3ea11c10bf8`. §1~10은 당시 역할 분석과 승인된 계획의 기록이며, 현재 구현 API·실행 결과·남은 차단은 §12를 따른다.
> 적용 우선순위: [010 라이브러리 분리](010_게임코어및콘텐츠라이브러리분리설계안.md)의 콘텐츠 독립 원칙은 유지하되, 공통 SceneManager를 어디까지 추출할지와 상속 관계는 이 문서를 따른다. [008 입력/Layer](008_InputManager및SceneUILayer설계안.md)·[009 진행/저장](009_InGameData및저장복원설계안.md)·[011 Frame](011_Frame루프및상속형게임실행설계안.md)의 소유권·안전점·실패 보존 계약은 약화하지 않는다.

## 1. 목표와 비목표

### 확정한 방향

- ConsoleGameCore가 **Scene 소유·실행·전환 요청·EndFrame 게시·지연 파괴·수명 보호**를 담당한다.
- TextRPG는 **Scene 생성·게임 정의 조회·게임 행동·진행 결합 게시·Continue/저장·게임 전용 UI 정책**을 담당한다.
- 게임별 클래스 이름은 사용자 표현대로 `FTextRPGSceneManager`를 사용한다. 공통 기반의 기술 기본안은 기존 `TScene<TAction>`과 맞는 `TSceneManager<TAction>`이다.
- 기존 Manager를 통째로 이동하거나, 기반 클래스를 빈 선언으로 만들고 기존 전환 본문을 파생 클래스에 그대로 남기는 방식은 채택하지 않는다.
- 다른 콘솔 게임은 GameData·FDataScene·InGameData·TextRPG 행동 Enum 없이 자기 Scene/행동 타입으로 공통 Manager를 사용할 수 있어야 한다.

### 이번 분리에서 하지 않을 일

- 새 전투·상점·장비·설정 기능, Scene stack/overlay 전환, 멀티스레드 Scene 관리.
- 저장 형식·키·자동 저장 시점, 메뉴 개수·키 바인딩·화면 좌표·팝업 정책 변경.
- 범용 SaveData/게임 상태 프레임워크, 임의 lambda 전역 작업 큐, 게임 객체를 `void*`나 정수에 숨기는 우회.
- 별도 SceneManager 프로젝트/DLL, 불필요한 namespace 일괄 개명, PCH 재분리.

## 2. 현재 구현과 분리 근거

| 실제 소스 | 현재 역할 | 이번 계획에서의 판단 |
|---|---|---|
| [Core FSceneHost.h](../../ConsoleGameCore/Scene/FSceneHost.h):4-36 및 [구현](../../ConsoleGameCore/Scene/FSceneHost.cpp):5-12 | 작업 깊이·EndFrame scope·현재/후보 Scene의 busy 판정 | 공통 수명 기반이 이미 있다. **현재 Scene 소유/전환 Manager 전체가 Core에 있는 것은 아니다.** 이를 재사용한다. |
| [Core TScene.h](../../ConsoleGameCore/Scene/TScene.h):6-55 | 행동 타입을 인자로 받는 Scene NVI와 입력/갱신/렌더 수명 보호 | 공통 Manager도 같은 `TAction` 경계를 따른다. |
| 기준 커밋의 `TextRPG/Scene/FSceneManager.h`:11-75 | 현재/후보 Scene, pending 진행, 완료 상태, 종료·입력 gate와 게임 표시 상태 보유 | 공통 소유/전환 상태와 게임별 상태를 나눈다. |
| 기준 커밋의 `TextRPG/Scene/FSceneManager.cpp`:63-135 | 시작 Scene 검색, ID로 FDataScene 생성/로드 | 게임 데이터 조회·팩토리이므로 TextRPG에 남긴다. |
| 같은 구현:159-332 | 진행 후보 수락과 Scene/진행 결합 게시, 취소·실패 처리 | 전환 절차는 Core로, typed 진행 검증/교체는 TextRPG 확장으로 나눈다. |
| 같은 구현:335-442 | Continue 표시, 게임 확인키/ESC 의미, 현지화 메시지·132×43 화면 조건 | Core가 게임 규칙/화면을 해석하지 않게 한다. |
| [게임 IScene.h](../../TextRPG/Scene/IScene.h):4-20 | StartGame/Continue/Quit 결과와 구체 Manager owner 연결 | 행동 의미는 유지하고 Manager 결합만 새 상속 경계에 맞춘다. |
| [FGameSession.cpp](../../TextRPG/InGameData/FGameSession.cpp):95-196 | NewGame/Continue 후보, 실제 완료 소비와 NewGame 자동 저장 | 앱 수명의 게임 정책이며 Core로 이동하지 않는다. |
| [FTextRpgFrame.cpp](../../TextRPG/Frame/FTextRpgFrame.cpp):31-49,125-239 | Store/Session/UI/Manager 수명, 게임 행동 처리·정상 Quit 저장·프레임 구동 | 기존 생성/역순 파괴 및 저장 시점을 유지하고 Manager 타입/호출만 연결한다. |

현재 결합은 단순 include 문제가 아니다. `FSceneManager`가 `dynamic_cast<FDataScene*>`와 private 상태 직접 접근, `FGameDataStore`·생성 Scene·InGameData 핸들, 현지화/화면 좌표를 함께 사용한다. 전방 선언이나 파일 이동만으로는 역할이 분리되지 않는다.

## 3. 목표 구조와 의존 방향

```text
ConsoleGameCore/Scene
  FSceneLifetime                 기존: Scene/UI busy와 작업 수명
  FSceneHost                     기존: Manager 작업/EndFrame 안전점 기반
  TScene<TAction>                기존: 공통 Scene 실행 NVI
  TSceneManager<TAction>          신규 제안: 실제 공통 소유·실행·전환 Manager
             ▲ public 상속
TextRPG/Scene
  FTextRPGSceneManager            신규 제안: TSceneManager<SSceneAction>
  IScene : TScene<SSceneAction>   기존 게임 Scene 경계 유지·owner 연결 조정
  FDataScene                     기존 데이터 기반 게임 Scene

TextRPG Frame / FGameSession
  → FTextRPGSceneManager
  → 공통 Manager를 통해 Scene 실행/전환
  → 실제 완료를 확인한 뒤 게임 정책에 따라 저장
```

- `ConsoleGameCore → GameData/TextRPG/생성 코드` 역의존은 금지한다.
- Core 소스에는 `ESceneAction`, `StartGame`, `Continue`, `FDataScene`, `InGameData`, 생성 Scene ID/현지화 키를 넣지 않는다. `TAction`의 필드도 해석하지 않는다.
- `TAction`은 현재 TScene과 같이 기본 생성과 중립적인 bool 변환을 지원하는 기술 계약을 사용한다. 게임이 행동의 의미를 정의한다.
- 템플릿은 Core 소유 헤더에 정의되어 소비자에서 인스턴스화될 수 있다. 이를 이유로 구체 게임 CPP를 Core 프로젝트에서 직접 컴파일하지 않는다.
- 기존 namespace의 `TextRPG` 표기는 이번 작업에서 일괄 변경하지 않는다. 파일/프로젝트 소유권과 실제 의존성을 기준으로 Core 독립성을 판정한다.

## 4. 책임과 단일 소유권

| 대상/상태 | 소유·실행 주체 | 게임별 결정 또는 입력 | 금지 사항 |
|---|---|---|---|
| 현재 Scene·대기 Scene | Core 공통 Manager의 유일한 `unique_ptr` 소유 | 콘텐츠가 준비한 Scene 전달 | 파생 Manager에 같은 Scene 소유 포인터/별도 큐 복제 금지 |
| 전환 수락·첫 요청 우선·snapshot·완료 상태 | Core 공통 Manager | 콘텐츠 적합성 검사 결과 | 같은 요청을 게임 큐와 Core 큐에 따로 게시 금지 |
| Input/Update/Render/EndFrame 수명 보호 | Core 공통 Manager + 기존 Scene 수명 기반 | 제한된 게임별 확장 지점 | 파생 클래스가 공개 실행 절차를 통째로 override하여 보호 우회 금지 |
| 시작 Scene 선택·ID 조회·FDataScene 생성 | FTextRPGSceneManager | FGameDataStore·FGameUiBridge·게임 진행 조회 | Core의 데이터 Store/Excel/생성 Enum 조회 금지 |
| 활성 진행 원본 | 기존 FGameSession | NewGame/Continue/게임 로직 | Scene/Core/UI가 진행 원본을 추가로 소유·복제하지 않음 |
| 대기 진행 후보·진행 대상 owner·진행 완료 해석 | TextRPG의 typed 전환 문맥 | 기존 InGameData 후보와 안정된 Session 핸들 | Core에 진행 필드·저장 API·게임별 완료 Enum을 추가하지 않음 |
| UI 트리·Layer·UITopLayer | 해당 Scene/UI | 기존 최상위 Layer 입력 정책 | Core Manager나 InputManager가 Layer 목록/최고값을 복제하지 않음 |
| 전환 직후 잔여 입력 무효화 | Core의 전환 경계 + Scene 입력 허용 경계 | TextRPG 확인키/마우스/ESC 정책 | 메뉴 네 슬롯·Digit1~4를 Core 공통 규칙으로 고정하지 않음 |
| Continue 가능 여부·오류 textKey·게임 전용 표시 | TextRPG 확장과 기존 게임 UI | 게임 저장 상태·현지화 | Core Render에 현지화 키·88/40 같은 게임 좌표를 넣지 않음 |
| 실제 NewGame 자동 저장·정상 Quit 저장 | FGameSession / FTextRpgFrame | 실제 게시 결과·종료 원인 | Core에서 Save/Load 또는 파일 덮어쓰기 결정 금지 |

`FTextRPGSceneManager`는 기존 `FSceneManager`의 게임 전용 확장/조정자이지 두 번째 Scene 엔진이 아니다. 공통 Manager가 실제 활성/대기 Scene과 전환 상태를 소유해야 한다.

## 5. 공통 API와 콘텐츠 확장 경계 — 구현 기본안

이 절은 **제안된 API 역할**이며 이미 구현된 선언이 아니다. 구체 hook 서명은 SCM-01에서 현재 회귀와 함께 고정하는 내부 구현 작업이며, 추가 게임 정책 결정을 사용자에게 떠넘기는 단계가 아니다.

### Core의 공개 실행 경로

- Scene 요청 수락, 현재 Scene의 입력/프레임 입력·Update·Render, EndFrame 처리, pending/종료 상태 조회.
- 일반 전환 결과는 수락/대기/실제 게시/실패/취소를 구분한다. 기존 bool 수락을 실제 게시 성공으로 바꾸지 않는다.
- 종료 요청은 중립적인 수명 제어다. 어떤 키/게임 행동이 그 요청을 만드는지는 콘텐츠가 결정한다.
- 공개 실행 경로의 guard·snapshot·실패 처리·완료 게시 순서는 기반 클래스가 통제한다. 파생 클래스는 좁은 protected 확장 지점만 사용한다.

### TextRPG가 제공하는 좁은 협력 동작

1. Scene 팩토리와 typed 후보 적합성 확인: Store, 진행, SceneId, UI owner/준비 상태.
2. 기존 Scene의 UI 입력 무효화·퇴장·최종 retire 및 준비된 Scene의 바인딩 연결.
3. 진행 교체가 필요한 요청에만 적용하는 typed 상태 교환과 게임별 완료 결과 연결.
4. 게임 행동/ESC/확인키 의미와 메시지 표시 요청. 실제 화면 출력은 TextRPG 표시 단계에 남긴다.

게임 `IScene`의 `AllowInput`/`AllowInputFrame` 정의가 현재 Manager CPP에 섞여 있는 부분도 함께 정리한다. 직접 Scene 진입이 Manager 보호를 우회하지 않도록 owner 연결과 비소유 호출 계약을 명시하고, 구체 Manager private 필드에 대한 광범위한 friend 접근을 중립 접근 경계/좁은 게임 전용 메서드로 줄인다. 단순히 `friend` 대상 이름만 교체하여 결합 제거 완료로 표시하지 않는다.

`dynamic_cast<FDataScene*>`가 필요한 게임별 적합성/retire 처리는 TextRPG 확장에만 둔다. Core는 공통 Scene 수명·소유 UI busy 계약으로 처리한다. 실제 화면의 메시지 문구·위치·Continue 활성 상태는 구조 분리 전후 동일하게 유지한다.

## 6. 가장 중요한 경계: Scene과 진행의 결합 게시

### 6.1 준비와 요청 수락

1. FGameSession이 새 진행 후보를 만들거나 인증된 저장을 복원한다. 이때 활성 진행·파일은 변경하지 않는다.
2. FTextRPGSceneManager가 해당 후보를 비소유 조회하는 FDataScene/UI를 준비한다. 실제 활성 UI bridge를 선행 교체하지 않는다.
3. Core의 공통 전환 충돌 검사와 게임별 후보 검증을 같은 보호 경계에서 완료한다.
4. Scene과 typed 진행 문맥은 **같은 전환 요청 단위**로 수락한다. 한쪽 소유권만 넘어간 뒤 예외/콜백으로 반쪽 요청이 남지 않게 한다. 검증 후 실제 이동 구간은 실패·사용자 콜백 없는 소유권 이동으로 한정한다.
5. 수락 즉시 남은 행동/고정 갱신을 차단하되 실제 Scene/진행 교체와 물리 삭제는 EndFrame까지 미룬다.

### 6.2 EndFrame의 고정 순서

| 단계 | 공통 Manager | TextRPG 협력과 보존 조건 |
|---|---|---|
| 안전점 확인 | Manager·현재/후보 Scene·실제 소유 UI 작업이 모두 반환했는지 확인 | 최초 가상 busy 조회부터 재진입 보호. busy면 파괴/게시/완료 공개 없이 대기 |
| snapshot 확정 | 한 요청만 로컬 snapshot으로 이동하고 멤버 pending 소유권 분리 | 이후 콜백의 새 요청은 다음 프레임 대상 |
| 진입 준비/기존 Scene 퇴장 | 기존 generic OnEnter/OnExit 실패 경계와 실행 순서를 보존 | FDataScene의 사전 UI 준비와 활성 Bind를 구분. 기존 활성 bridge를 조기 변경하지 않음 |
| 이전 UI/Scene 정리 | 기존 Scene을 안전점에서 해제하고 재진입/종료를 재확인 | 이전 UI 비소유 뷰가 이전 진행보다 먼저 사라져야 함. old destructor의 Quit는 후보 게시를 차단 |
| 최종 결합 게시 | 준비된 Scene 소유권을 공통 Manager의 활성 슬롯으로 이동 | 진행 owner 교환과 Scene 게시 사이에 할당·실패·외부 콜백을 끼우지 않음 |
| 새 바인딩·정리·완료 | 일관된 활성 상태에서 후속 연결과 완료를 한 번 공개 | UI Bind/완료/정리의 재진입·종료를 보호. 이미 게시된 상태와 취소/실패를 혼동하지 않음 |
| 게임 완료 소비 | 공통 결과 제공만 수행 | FGameSession이 기존 EGameStartResult로 해석. 실제 NewGame 게시 뒤 자동 저장, Continue는 저장하지 않음 |

**금지:** Core가 먼저 Scene만 게시하고 다음 Frame/별도 게임 큐에서 진행을 바꾸는 구조.

기술 기본안은 Core가 전환 절차를 소유하고 TextRPG가 좁은 typed 게시 협력 함수를 제공하는 방식이다. 가상 게임 확장 지점의 진입 자체와 실제 콜백 없는 최종 게시 구간을 구분한다. 최종 구간은 직접적인 `unique_ptr` owner 교환과 Core의 비가상·비예외 Scene 게시 helper로 구성하고, 그 사이에 추가 게임 hook/사용자 getter/destructor가 실행되지 않게 한다. 이를 표현하기 위해 범용 게임 상태 저장 계층을 새로 만들지 않는다.

### 6.3 실패·취소·파괴 보장

- 후보 생성/검증/OnEnter·OnExit 실패 시 기존 진행·정상 세이브/키와 아직 살아 있는 기존 Scene을 보존한다. framework 입력/바인딩은 복구하지만 외부 콜백의 임의 전역 변경 전체를 rollback한다고 약속하지 않는다.
- old Scene이 이미 파괴된 뒤 발생한 Quit에서는 그 Scene을 복원했다고 하지 않는다. 후보 게시·Started·추가 저장을 차단하고 진행/파일 보존과 종료를 유지한다.
- 취소 요청의 논리 입력 무효화는 즉시, busy 후보의 실제 파괴는 안전점 이후다. 이월된 취소는 callback 반환 뒤 idle 재취소로도 앞당기지 않는다.
- 후보 Scene/UI를 먼저 정리하고 그 뒤 후보 진행을 해제한다. 완료 결과는 관련 destructor 정리보다 먼저 외부에 노출하지 않는다.
- **상속 파괴 순서도 계약이다.** FTextRPGSceneManager의 typed 진행 문맥이 소멸하기 전에 게임 Scene/UI와 Core의 대기 Scene을 정상 idle 안전점에서 정리한다. 이를 파생 destructor/명시 shutdown 경로에서 수행하고, base destructor에서 이미 파괴된 파생 멤버나 게임 가상 hook을 호출하지 않는다. Manager를 자기 callback 안에서 파괴하는 사용법은 지원하지 않는다.
- Frame의 기존 Store → Session → UI bridge → Manager 생성 및 역순 파괴를 유지한다. Session의 안정된 진행 owner 핸들은 Manager보다 오래 살아야 한다.

## 7. 입력·게임 행동·화면 정책의 처리

- Core는 `SSceneAction::Type`/`TargetSceneId`를 해석하지 않는다. StartGame/Continue/Quit 분기는 게임 Frame/Session에 남긴다.
- ESC→Quit와 정상 Quit의 저장 여부는 TextRPG 정책이다. 다른 게임이 ESC를 팝업 닫기나 뒤로 가기로 쓰는 것을 Core가 막지 않는다. 이번 앱의 실제 ESC 동작은 바꾸지 않는다.
- 입력의 기기 상태는 계속 InputManager 소유다. Scene 교체/Layer 경계의 잔여 batch 차단·release gate는 Scene/Manager/UI의 입력 권한 처리이며 물리 상태를 중복 수집하지 않는다.
- 숫자 네 키·Enter·왼쪽 마우스의 게임 확인 의미는 TextRPG에 남긴다. 기존 same-batch 잔여 입력 차단, 실제 release 뒤 재확정, 보조 버튼/물리 alias·focus loss 처리와 직접 Scene 입력 경로를 보존한다.
- Layer·UITopLayer와 하위 입력 차단은 각 Scene/UI가 소유한다. 상속을 이유로 Manager가 Layer manager까지 흡수하지 않는다.
- 현지화 textKey·메시지 표시 및 기존 화면 크기/좌표 특례는 TextRPG 표시 코드로 남긴다. 공통 Render는 게임별 UI 표시 협력 지점을 제공하되 게임 문구나 좌표를 알지 않는다.

## 8. 파일·소비자·프로젝트 이관 범위

| 구분 | 구현 시 변경 대상 | 처리 |
|---|---|---|
| 신규 Core 기반 | `ConsoleGameCore/Scene/TSceneManager.h` 및 필요한 최소 공통 보조 소스 | 제안 경로. 기존 FSceneHost/FSceneLifetime/TScene 재사용. 템플릿·소스 소유 방식은 SCM-01 계약에 맞춤 |
| 게임 확장 | 기존 `TextRPG/Scene/FSceneManager.h/.cpp` → `FTextRPGSceneManager.h/.cpp` | 제안된 rename. 공통 구현을 제거하고 콘텐츠 확장만 남김 |
| Scene 연결 | `TextRPG/Scene/IScene.h`, `FDataScene.h/.cpp` | owner/friend/입력 허용·retire/바인딩 협력 경계 정리 |
| 앱·진행 | `TextRPG/Frame/FTextRpgFrame.cpp`, `InGameData/FGameSession.h/.cpp` | 파생 Manager 사용, 생성 순서·결합 게시·완료/저장 정책 유지 |
| 빌드·IDE | Core/TextRPG 프로젝트와 filters, NativeTests 프로젝트/filters, 관련 PCH | 새 Core 파일/rename 실제 경로 일치. 기존 ProjectReference 방향·기본 PCH·캐시 유지 |
| 기존 회귀 | `Tests/ConsoleGameCore/Main.cpp`, `Tests/SceneUiRuntime/Main.cpp`, `OptionalCatalog.cpp`, 관련 기존 Python 계약/runner | 독립 Core Manager 사용, 게임 통합·직접 API·실패 주입·코드 경로/소스 fingerprint와 기존 assertion 갱신 |

위 표는 계획 당시의 이관 범위다. 현재 실제 생성·이름 변경은 §12를 따른다. 일반 헤더 include 금지, 표준/안정 전제 PCH 집약, 변경 잦은 구체 클래스·생성 타입 PCH 제외를 유지한다. 제거한 보조 GameDataPch.cpp를 되살리지 않았으며 원본 Excel/Data/Asset/진행 스키마·생성 데이터·Preview/아트 주석은 보존한다.

## 9. 구현 작업과 선행 관계

| ID | 작업 | 선행 | 관찰 가능한 완료 조건 |
|---|---|---|---|
| SCM-D01 | 역할·상속·소유권·실패/검증 계획 문서화 | 사용자 방향 | 이 문서·010 우선순위·로드맵 동기화. **문서 완료만 의미** |
| SCM-01 | 최소 Core API·콘텐츠 hook·결합 게시/파괴 계약 고정 | SCM-D01 | 실제 필드/호출자 매핑, 직접 Scene/root 진입·최초 virtual·종료·예외·destructor 경계, 공개 결과/owner 수명 명시. 작은 실패 회귀 준비 |
| SCM-02 | Core TSceneManager 구현·독립 소비자 | SCM-01 | 실제 현재/후보 Scene 소유와 실행/EndFrame 절차가 Core에서 동작. 자체 TAction과 Scene으로 GameData 없는 양성 및 금지 의존 음성 확인 |
| SCM-03 | FTextRPGSceneManager 상속·진행 결합 게시 연결 | SCM-02 | NewGame/Continue 최소 실제 흐름에서 Session 단일 루트·Scene/UI 참조·요청/완료·실패 보존 확인. 필요한 호출부는 이 단계부터 함께 갱신 |
| SCM-04 | 게임 입력·UI 표시·Scene owner 결합 정리 | SCM-03 | Core에 게임 Enum/동적 cast/확인키 네 슬롯/현지화/좌표/진행 타입 없음. 실제 기존 조작·표시·수명 회귀 보존 |
| SCM-05 | 모든 소비자·프로젝트/filters·기존 runner 이관 마감 | SCM-03/04 | 남은 구형 Manager 경로/중복 상태·전환 엔진 제거, optional/fault/source-including 소비자와 fingerprint 일치. 최초 연결을 이 단계까지 미루지 않음 |
| SCM-06 | 최종 후보 일괄 검증·독립 source 검토·문서 마감 | SCM-05 | 아래 필수 수용 기준 충족, 원본 보존과 코드/산출물 정체성에 연결된 최종 기록 |

기본 순서는 SCM-01 → SCM-02 → SCM-03 → SCM-04 → SCM-05 → SCM-06이다. 분담 시 공통 API를 먼저 고정하고 코드 소유 경로를 겹치지 않게 한다. 공유 생성기/라이브러리 출력과 전체 소스 fingerprint를 읽는 검증은 소스 작성자 해제 뒤 한 담당자가 실행한다.

## 10. 구현 수용 기준과 검증 주기

### 필수 수용 기준

- **실제 공통화:** 다른 행동 타입·Scene으로 Core Manager를 사용하고 전환/수명/종료를 실행할 수 있다. 빈 기반 클래스·이름 이동·단순 금지 문자열 검색만으로 완료하지 않는다.
- **Core 독립성:** TextRPG/생성 헤더 검색 경로와 GameData.lib 없는 기존 Core 소비자의 실제 빌드·실행 양성. 의도적으로 금지 의존을 추가한 기존 음성 경계도 거부를 확인한다.
- **단일 소유:** 활성/대기 Scene과 공통 전환 상태는 Core 한 곳, 활성 진행은 Session 한 곳. 콘텐츠 확장에 같은 소유 큐/상태 기계를 복제하지 않는다.
- **직접/재진입 경계:** Manager·직접 Scene·실제 UI root 입력/렌더/query/OnEnter/OnExit/취소/destructor에서 최초 콜백 도달과 반환 전 생존을 관찰한다. 중첩 EndFrame/취소/새 전환, 이전 destructor Quit, 거부 인수 정리 및 파생/base 파괴 순서를 포함한다.
- **결합 게시·저장:** 요청 수락과 실제 완료를 구분하고 후보 실패/취소 시 원본 보존. 새 Scene이 옛 진행을 가리키는 중간 상태를 외부 callback에 노출하지 않는다. NewGame 실제 게시 후 자동 저장, Continue 비저장, 정상 Quit 저장·늦은 종료 추가 저장 금지 유지.
- **입력·UI:** 최상위 Layer만 입력, 잔여 batch 차단, release gate/alias/보조 버튼/focus loss, 기존 네 선택지·문구·아트·메시지 화면 동작 유지.
- **Frame:** Basic/FixedUpdate/FrameSkip에서 전환 뒤 추가 갱신 차단. 의도적 Render 생략은 EndFrame 수행, 실제 입력/출력 실패는 게시 금지.
- **빌드/원본:** Debug/Release × x64/x86(solution x86/native Win32) 증분 Build·실제 native 분석 경고0. 관련 Core/Input/Scene/모델/저장·optional/fault 소비자, 최종 EXE 저장·재시작·Continue를 현재 artifact identity로 검증. 원본/주석·실패 진단·회귀 assertion을 약화하지 않는다.

### 실행 주기

1. 작업 중에는 작은 RED/GREEN과 최소 XML/선언·소비자 검사만 수행한다.
2. SCM-02에서 작은 독립 Core 소비자, SCM-03에서 실제 게임/진행 연결을 조기에 확인한다. 전체 기능을 다 옮긴 뒤 첫 연결 오류를 발견하는 방식을 피한다.
3. 완성 후보에서 필요한 네 구성 빌드·분석·회귀·독립 source 검토를 한 묶음으로 실행한다. 실패 시 영향받은 검사만 재실행한다.
4. 기존 `native_build.py`/NativeTests/Scene·Input·Store runner를 확장하며 새 중복 runner·일회용 검증 스크립트·소스 복사 빌드·Clean/Rebuild를 만들지 않는다. 기존 obj/PCH 캐시는 호환 경계별로 유지한다.
5. source-only·구조 검사와 실제 컴파일/실행을 구분하고 실제 group/case 단위를 보존한다. 과거 실행 횟수를 새 아키텍처의 결과로 복사하지 않는다.
6. 문서/집계만 마감할 때 제품 검증을 반복하지 않는다. 상세 구현 검증은 [Agents/qa.md](../../Agents/qa.md)를 따른다.

## 11. 현재 완료 상태와 남은 범위

- [x] SCM-D01: 소스 기반 역할 분리·상속 방향·단일 소유권·결합 게시·작업/검증 계획 작성.
- [x] SCM-01: 최소 실행 API와 경계 회귀 고정.
- [x] SCM-02: 공통 SceneManager 구현/독립 사용.
- [x] SCM-03: FTextRPGSceneManager 상속과 진행 연결.
- [x] SCM-04: 게임 입력·UI 표시·owner 결합 정리.
- [x] SCM-05: 모든 소비자/프로젝트/runner 마감.
- [x] SCM-06: 소스 검토·빌드·native·저장/복원 검증과 문서 마감. Debug/Win32 빈 출력 실패는 사용자 승인으로 별도 보류하며 전체 검사 PASS로 표시하지 않는다.

추가 게임 규칙·저장 UX를 도입하지 않았다. 구현과 필수 검증의 통과 범위를 최종 수용과 구분한다.

## 12. 실제 로컬 구현과 검증 상태

### 실제 경계와 API

- [TSceneManager.h](../../ConsoleGameCore/Scene/TSceneManager.h)가 현재/후보 `TScene<TAction>`의 유일한 소유자다. `SetScene`, `EndFrame`, `CancelPendingTransitions`, `Shutdown`, 입력/갱신/렌더 공개 NVI를 final로 통제하고 snapshot·지연 취소·종료·완료 결과를 관리한다. `FSceneHost`·`FSceneLifetime`·`TScene`을 재사용한다.
- [FTextRPGSceneManager.h](../../TextRPG/Scene/FTextRPGSceneManager.h)와 [구현](../../TextRPG/Scene/FTextRPGSceneManager.cpp)은 `TSceneManager<SSceneAction>`을 상속한다. Scene factory/Store, typed 진행 문맥·SceneId 검증, 실제 UI retire/bind, 게임 입력/현지화/표시 정책만 협력한다. 구형 Manager 파일과 소비자 경로는 제거했다.
- 수락 시 준비 수명을 고정하여 후보 reload·Layer close/flush를 차단하고, 이전 OnExit·retire·실제 destructor 이후에도 게시 문맥을 대조한다. 직접 Scene/root callback·query·busy 취소는 blanket gate로 막지 않는다. `SPublication::Publish()`는 비가상 noexcept이며 typed root swap과 Scene 게시 사이에 할당·콜백·파괴를 넣지 않는다.
- `IScene`의 concrete Manager friend/owner 대신 입력 전용 비소유 `ISceneInputPolicy`를 연결한다. 파생 destructor는 typed 멤버가 소멸하기 전에 `Shutdown()`으로 Core 소유 Scene/UI를 정리한다.
- 전체 `FDataScene::OnExitImpl`의 예외 경계가 두 번째 입력 취소까지 보호한다. 원래 연결이 있었고 현재 연결이 비었을 때만 복구하며, 콜백이 게시한 새 연결과 최종 retire의 detach는 보존한다.

### 실제 실행 범위

| 범위 | 현재 결과 |
|---|---|
| Debug/Release × x64/x86 | solution 증분 Build 및 제품 native 분석 성공, 경고 0 |
| 같은 네 구성 native | 각각 Core31·Scene98·model137·Save28 checkpoint groups, Input168 checks |
| Core 독립성 / optional | Debug/x64 실제 양성 후 정확 C1083 음성, Debug x64/x86 TextArt 헤더 부재 소비자 각1 group |
| 실제 앱 일반 모드 | Debug/x64·Release/x64·Release/Win32 각23 case 통과. Debug/Win32는 아래 사용자 보류 항목 때문에 전체 PASS 아님 |
| 실제 저장/재시작/Continue | 네 구성 각각 명시 진행5 case 통과. Debug/x64 fixed-update·frame-skip 진행 각5 case 추가 확인 |
| 구조 / include 경계 | build 계약49·header 정책8 통과. Debug/x64 PCH-off 실제14 TU 해석 통과. 최초 긴 PATH의 SDK 검색 실패는 보존하고 자식 전용 짧은 PATH로 해당 gate만 정정 |
| 독립 소스 검토 | 최초 runtime15/build8 + 변경7 집중 검토 + 로그2행/1파일 집중 검토를 정체성으로 조합. 이전 canonical FAIL은 보존하며 현재 소스 차단 결함은 없음 |

group/case는 서로 겹치는 checkpoint 단위이며 고유 테스트 총수로 합산하지 않는다. Input 분석 모드는 기존 호환 캐시를 재사용했으며 새 analyzer 실행으로 표시하지 않는다. 원본 소스·기존 obj/PCH·증분 Build와 기존 runner를 사용했고 Clean/Rebuild·새 runner·경고 억제는 없다.

### 사용자 승인 보류·재발 시 추적 — APP-D32-EMPTY-OUTPUT

Debug/Win32 일반 앱 검사의 `broken-reference` 거부 사례에서 자식 출력이 0 bytes여서 CLI assertion으로 실패했다. 기존 일반 모드가 해당 실패의 정확한 자식 exit를 보존하지 않아 원래 exit는 미확인이다. 같은 EXE/dat/cwd의 단건 관측은 exit1과 정상 `TargetSceneId reference is missing` 진단을 반환했지만, 전체 호출 통과나 원인 해결을 입증하지 않는다. 통과를 얻기 위한 전체 반복 실행은 하지 않았다. 사용자 지시 “다음에 같은 문제 나오면 추적 ㄱㄱ 보류로 넘겨”에 따라 이번 작업 차단에서는 제외하고 별도 보류한다. 지금은 추가 진단·재현 실행·수정 없이 중지하며, 원래 FAIL과 원인 미확정 상태는 보존한다. 같은 빈 출력 현상이 다시 관측되면 해당 자식의 종료 코드(십진/16진), stdout/stderr, 실행파일·데이터 해시, 구성·cwd와 비밀값을 제외한 관련 환경을 먼저 보존하여 추적한다. 전체 검사 PASS나 해결 완료로 승격하지 않는다.

근거 루트: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/scenemanager-impl-bb83fdfd/`. `execution-index.json`, `review/composition.json`, `final/build`, `final/native`, `final/app`의 실제 기록과 산출물 해시를 따른다. 최초 재현 RED·검토 FAIL·승인 시간 초과는 이력으로 보존한다. 수동 DPI/글꼴 인증과 무관한 Store/reader-exception 전체 matrix를 새로 실행했다고 주장하지 않는다.
