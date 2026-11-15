# InputManager 상태 조회와 Scene UI Layer 설계

> **현재 라이브러리 경계 — 구현/실행 확인:** InputManager·공통 UI/Scene 수명·Frame 기반은 ConsoleGameCore로 분리했다. 코어 FUIManager는 IUiInputTarget만 연결하고 FDataScene 연결/게임 행동은 콘텐츠 bridge가 담당한다. Layer 소유·최상위 입력 및 진행/수명 보존 정책은 유지한다. 실제 경로와 네 구성 실행/최종 검토 범위는 [010 §10](010_게임코어및콘텐츠라이브러리분리설계안.md)을 따른다. 아래 분리 이전 FUIManager→FDataScene 설명과 검증 수치는 당시 이력이며 현재 공통 API 계약이 아니다.


> **최신 앱/진행 전환 경계 — 2026-11-15:** [009 §9](009_InGameData및저장복원설계안.md)의 `Input → Update → Render/Present → EndFrame` 결합 게시와 공통 Scene NVI/실제 UI root busy 보호를 구현·검증했다. 요청 수락(Queued/Continue bool)과 EndFrame 뒤 완료를 구분하며 최초 virtual 조회~incoming 정리·취소 이월·OnExit throw 복구/최종 detach·destructor Quit를 포함한다. 최신 실행은 네 구성 각 full86/legacy73/progress24 group·Input168 checks·optional1 group·게임 분석 경고0·실제 EXE default23/진행5 case다. 과거 아래 수치는 당시 INP 이력이며 범위가 겹치는 묶음은 합산하지 않는다. InputManager의 상태 전용 책임과 Scene Layer 소유권은 그대로다. 모든 기존 Layer flush 이관·전체 콘텐츠/수동 호스트 인증을 완료한 것은 아니다.

> 상태: **INP-01~05 구현·통합 검증 완료.** InputManager는 기기 상태만, Scene은 UILayer/UITopLayer와 최상위 입력 허용을 소유한다. 최종 수정본 네 구성 빌드/게임 분석 경고·오류 0, 실제 EXE 각각18 시나리오, PCH-off 네 구성, x64/x86 UI 각62 checkpoint 및 변경 다섯 파일 집중 재리뷰 PASS를 확인했다. 전체 프로젝트/P3 콘텐츠 인증은 아니며 최초 설계/이전 실패 기록과 현재 결과를 구분한다.
> 우선순위: 입력 상태 서비스, Scene의 `UILayer`/`UITopLayer`, 최상위 Layer 전용 입력 정책은 이 문서가 [007](007_Scene별UI객체및입력분리설계안.md)의 충돌하는 부분보다 우선한다. 007의 화면 배치·UI 소유권·좌표·수명 안전과 기존 실행 기록은 유지한다.

## 1. 확정한 방향과 범위

- **InputManager는 입력 상태만 관리한다.** 키와 마우스 버튼의 `None`/`Pressed`/`Held`/`Released`, 포인터 위치·이동, 휠 및 입력 전이 기록을 제공한다.
- **Scene이 `UILayer`와 `UITopLayer`를 소유한다.** UI 구성·팝업 열기/닫기·최상위 갱신과 하위 Layer 입력 차단은 Scene 책임이다.
- Scene은 열린 최상위 Layer에만 키보드·마우스·휠 입력 처리를 허용한다. 최상위에서 처리하지 않은 입력도 아래 Layer에 넘기지 않는다.
- `Update()`의 상태/모델 갱신과 `HandleInput()`의 입력 처리를 구분한다. 하위 Layer는 갱신·렌더될 수 있지만 입력을 읽고 행동하지 않는다.
- UI 내부의 포커스·hit-test·클릭·드래그 판정은 Scene/UI 측 책임이다. InputManager에는 UI ID·Layer 번호·버튼 영역·포커스·캡처를 넣지 않는다.
- 이 문서의 Scene은 **런타임 객체**다. `UILayer`와 `UITopLayer`를 Excel 또는 생성 `GameData::Scene`에 추가하지 않는다. 정적 Scene은 현재 `SceneId`/`SceneType`/`IsStartupScene` 세 필드 계약을 유지한다.

스탯 팝업은 Layer 정책 설명용 예시다. 실제 팝업 문구·선택지·닫기 UX·전투 pause를 새로 확정하거나 구현하는 요청으로 해석하지 않는다. 기존 Scene 선택지는 비활성 포함 정확히 네 개다.

## 2. 최초 설계 작성 시점에 확인한 상태 — 현재 구현은 §11

| 근거 | 현재 구현 / 목표와의 차이 |
|---|---|
| [IInputSource.h](../../ConsoleGameCore/Input/IInputSource.h) | 실제 API는 `ReadEvent(SInputEvent&)`이며 Event/Timeout/Closed/Error를 구분한다. |
| [FConsoleInput.h](../../ConsoleGameCore/Input/FConsoleInput.h), [FConsoleInput.cpp](../../ConsoleGameCore/Input/FConsoleInput.cpp) | Windows 입력을 공통 이벤트로 변환한다. 키/왼쪽 버튼의 이전 상태와 배치/viewport 정보를 가진다. 별도 상태 조회 Manager는 없다. |
| [IScene.h](../../TextRPG/Scene/IScene.h) | `EInputKey`, `EInputEventType`, `SInputEvent`가 아직 Scene 헤더에 있다. Scene 입력 API는 현재 이벤트 한 개를 받는다. |
| [Main.cpp](../../TextRPG/Main.cpp) | 공급자 읽기, Escape 종료, Scene 입력 전달과 전환 후 잔여 배치 폐기를 수행한다. |
| [FDataScene.h](../../TextRPG/Scene/FDataScene.h) | Scene이 UI 루트를 단독 소유한다. 이 문서의 명시적 `UILayer`/`UITopLayer` 상태 서비스 연동은 아직 없다. |
| [FSceneUiRoot.h](../../ConsoleGameCore/UI/Common/FSceneUiRoot.h) | Base/Popup 저장소, Modal/NonModal/PassThrough와 포커스·캡처·지연 명령이 있다. 최상위 Layer만 허용하는 새 정책과 동일한 구현이라고 간주하지 않는다. |

`TextRPG/Input/InputTypes.h`, `FInputManager.h/.cpp`는 최초 설계 작성 때는 없던 추가 예정 파일이었다. 현재는 구현됐으며 정확한 공개 API와 검증 범위는 실제 헤더 및 §11을 따른다.

## 3. 책임·소유권·수명

| 구성 | 소유하고 결정할 것 | 가지지 않을 것 |
|---|---|---|
| 앱 조립부 | 입력 공급자와 Manager의 수명, 프레임 순서, 기존 전역 종료 처리 | UI Layer별 입력 정책 |
| `FConsoleInput : IInputSource` | Windows 콘솔 핸들/모드 복원, 기기 레코드 읽기와 정규화 | 게임 행동, UI 구조 |
| 제안 `FInputManager` | 기기 상태의 단일 원본, 프레임 전이·반복·이벤트 순서 | Scene 참조, `UITopLayer`, UI 대상/차단 판정 |
| 런타임 Scene | `UILayer`, `UITopLayer`, 최상위 입력 허용, Layer 변경·Scene 전환 경계 | OS 입력 직접 조회, 기기 상태 복사본 |
| Layer 내부 UI 처리부 | 포커스·hit-test·클릭/드래그·캡처와 유효 대상 판정 | 다른 Layer의 입력 허용 결정 |
| 개별 UI/게임 객체 | 허용된 입력에 대응하는 자기 기능, 표현/모델 갱신 | 일반 Update를 통한 입력 차단 우회 |

앱이 공급자를 소유하고 Manager가 공급자를 비소유 참조하는 구성을 기본 제안으로 한다. 공급자는 Manager보다 오래 살아야 하며 Scene/UI는 공급자를 직접 읽지 않는다. Manager의 프레임 뷰/참조는 다음 프레임 갱신 전에만 유효하다. 가변 전역 싱글톤은 필수 조건이 아니다.

UI 객체는 Scene 측에서 한 번만 소유한다. 기존 단일 `m_uiRoot`의 Base/Popup 목록과 새 `UILayer` 목록에 동일 객체·겹침 순서를 이중 보관하지 않는다. 이관 시 Scene의 Layer 구성을 유일한 원본으로 삼고 기존 루트 기능을 Layer 내부 UI 처리에 재사용·조정한다. 실제 Layer 자료형과 루트 재사용 방식은 구현 단계에서 확정한다.

## 4. 입력 상태 계약

### 네 상태

| 이전 프레임 눌림 | 현재 프레임 눌림 | 단순 전이의 조회 결과 |
|---|---|---|
| 없음 | 없음 | `None` |
| 없음 | 있음 | `Pressed` |
| 있음 | 있음 | `Held` |
| 있음 | 없음 | `Released` |

```text
None → Pressed → Held → Held → Released → None
```

- `None`은 해당 키/버튼이 눌리지 않았다는 뜻이다. 모든 기기의 무입력을 뜻하지 않는다.
- `Pressed`와 `Released`는 전이가 발생한 프레임의 상태다. 다음 프레임에 전이가 없으면 각각 Held/None으로 진행한다.
- Held는 최초 Pressed 프레임을 제외한다. 누르는 동안의 동작은 Pressed와 Held를 함께 보거나 현재 눌림 조회를 사용한다.
- 조회는 const 동작이며 OS 읽기·입력 소비·상태 변경을 하지 않는다. 같은 프레임의 반복 조회 순서에 따라 결과가 달라지지 않는다.
- 입력 이벤트가 없는 프레임에도 눌림은 유지된다. OS 반복 KeyDown을 새 최초 누름으로 처리하지 않으며 반복 횟수는 보존한다. 메뉴 탐색 반복 주기는 Scene/UI 정책이다.
- 같은 의미로 매핑할 키라도 서로 다른 기기 키 식별을 먼저 보존한다. 일반 숫자/숫자패드의 중첩 눌림·해제에서 다른 키의 눌림을 잘못 지우지 않도록 검증한다. 기존 조작에 새 단축키를 임의로 추가하지 않는다.

### 한 프레임 안의 짧은 눌림

단일 enum은 같은 프레임의 눌림과 해제를 모두 표현하지 못한다. 프레임 시작/끝의 bool만 비교해서 입력을 복원하지 않는다.

- Manager는 순서 있는 입력 전이와 `PressedThisFrame`/`ReleasedThisFrame`에 해당하는 정보를 함께 보존한다.
- 단일 상태 조회의 기본 제안은 최종 눌림이 있으면 새 눌림 유무에 따라 Pressed/Held, 최종 눌림이 없으면 해제 유무에 따라 Released/None이다.
- 따라서 같은 프레임의 down→up은 최종 Released이면서 두 전이 정보가 모두 존재할 수 있다. 빠른 확정·클릭은 단일 enum만 보지 않고 전이 기록으로 정확히 한 번 처리한다.
- 한 프레임의 다중 누름 순서가 필요한 경로는 이벤트 목록을 사용한다. 조회 플래그를 클릭 횟수로 해석하지 않는다.

### 개념적인 조회 API

```cpp
// 구현된 핵심 조회 API 요약. noexcept 등을 포함한 정확한 선언은 Input/FInputManager.h를 따른다.
EKeyStatus GetKeyStatus(EInputKey key) const;
bool WasKeyPressed(EInputKey key) const;
bool WasKeyReleased(EInputKey key) const;
EKeyStatus GetMouseButtonStatus(EMouseButton button) const;
```

어느 입력 상태 API에도 Layer 인자를 요구하지 않는다. Manager가 뒤 화면에 None을 합성하는 구조가 아니다. 실제 기기 상태는 동일하게 제공하고, **Scene이 하위 Layer의 입력 처리 자체를 호출하지 않는다.**

포인터 위치·이동량은 문자 셀, 휠은 기기에서 정규화한 delta 단위를 명시한다. 콘솔 buffer→viewport 원점 보정은 공급자에서, viewport→Scene/부모 UI 좌표 변환은 Scene/UI에서 한 번씩 수행한다. 기존 렌더/hit-test의 공통 rect/clip 계산을 보존한다.

## 5. Scene의 UILayer / UITopLayer

- `UILayer`: Scene이 소유하는 Layer별 UI 구성 목록이라는 의미다. 모든 UI 위젯 하나하나를 별도 Layer로 만들지 않는다.
- `UITopLayer`: 열린 상태로 표시되는 최상위 Layer 선택이다. 실제 구현의 `GetUiTopLayer()`는 안정 Layer ID, `GetUiTopLevel()`은 그 Layer의 순서 Level을 반환한다. 두 값은 다를 수 있으며 모두 Scene의 동일 목록에서 결정한다. 0 ID/Level은 유효한 Layer 없음이다.
- Layer 1은 기본 배틀필드 UI 세트, 그 위 Layer 2는 스탯 팝업이라는 구성이 가능하다. 그 위 팝업은 다음 Level을 사용한다.
- 닫기/숨김/중간 Layer 제거 후에는 실제 목록에서 최상위를 다시 결정한다. 최고 번호를 무조건 감소시키거나 컨테이너 크기를 Layer 번호로 간주하지 않는다.
- 유효한 최상위 Layer가 없으면 일반 UI 입력 처리를 생략한다. 최상위에 활성 버튼/포커스 대상이 없더라도 아래 Layer로 입력을 넘기지 않는다.
- Layer Level은 표시/입력 순서이고 객체 정체성은 아니다. 수명이 끝난 Layer/UI를 가리키는 요청에는 안정 ID/유효성 확인을 사용하며 vector index나 파괴된 raw pointer를 보관하지 않는다.
- `UITopLayer`는 Scene의 Layer 목록에서 결정되는 값이다. Manager나 FUIManager에 별도 최고 Layer 복사본을 두지 않는다. Scene 내부에서 저장한다면 목록 변경 commit 때 함께 갱신한다.

```text
Scene
├─ Layer 1 : 배틀필드 UI — 갱신/렌더, 입력 처리 없음
├─ Layer 2 : 스탯 조회   — 갱신/렌더, 입력 처리 없음
└─ Layer 3 : 상세 팝업   — 갱신/렌더 + 입력 처리
```

Layer 3을 닫으면 실제 남은 목록의 최상위인 Layer 2가 다음 입력 대상이다. 팝업을 닫아도 아래 UI의 선택·로그·표시 대상 상태는 재생성하지 않는다. 입력 차단은 전투 모델의 pause를 뜻하지 않는다.

## 6. 입력 단계와 일반 Update 분리

```text
프레임 시작: 이전 transient 전이/이동·휠 누적 초기화
  → 공급자에서 입력 수집, Manager 상태/전이 기록 갱신
  → 입력 오류·닫힘과 기존 전역 Escape 종료 처리
  → 현재 Scene.HandleInput(입력 프레임)
       → Scene이 UITopLayer 결정
       → 해당 Layer의 입력 처리만 실행
       → Layer 내부 포커스/hit-test/gesture 판정
  → 안전 지점에서 Layer/Scene 변경 commit
  → 일반 Update(deltaSeconds)
  → 렌더링
```

이 흐름으로 Main과 Scene을 연결했다. `IScene`/`FSceneManager`의 `HandleInputFrame(const std::vector<SInputEvent>&)`가 전체 프레임을 전달하며 FDataScene이 이를 override한다. 기존 이벤트 단위 API는 호환 경로로 유지한다. 최종 EXE 검증은 §11에서 별도로 기록한다.

- UI 입력 조회와 입력으로 인한 행동은 지정된 HandleInput 경로에서만 수행한다. 일반 Update/Render에서 Manager를 직접 조회하여 하위 Layer 입력을 실행하는 우회를 금지한다.
- 같은 Layer에서도 Enter는 포커스 대상, 숫자키는 기존 고정 슬롯, 마우스는 공유 기하의 대상에만 적용한다. 모든 버튼이 동일한 Enter를 독립 조회·확정하지 않는다.
- 최상위의 빈 영역·비활성 버튼·미처리 입력은 하위 Layer로 전달하지 않는다. NonModal/PassThrough에 의한 Layer 간 관통은 새 기본 경로에 넣지 않는다.
- resize·포커스 상실·입력 소스 종료 같은 시스템 상태는 일반 UI 입력과 구분한다. 가려진 UI에도 필요한 취소/기하 갱신을 수행하되 사용자 조작을 전달하지 않는다.
- Escape 즉시 앱 Quit은 기존 명시적 전역 정책으로 유지한다. 새 팝업 닫기/Back/종료 확인창으로 바꾸지 않는다. Manager는 Escape 상태만 제공하며 실제 종료 판단/실행은 앱 측에 둔다.

### Layer·Scene 변경 경계

1. 열기/닫기/전환은 요청으로 기록하고 현재 입력 dispatch 뒤 안전 지점에 적용한다. 콜백 중 자기 객체/Scene을 즉시 파괴하지 않는다.
2. 새 팝업/Scene 준비 실패는 이전 UI 목록·최상위·선택/포커스를 보존한다.
3. 최상위가 바뀌면 이전 입력 대상의 눌림·드래그·캡처를 취소하고 현재 프레임/배치의 잔여 UI 입력을 새 Layer에 재전달하지 않는다.
4. 새 입력 범위에서 Enter·숫자 확정 키·눌려 있던 확정용 마우스 버튼은 실제 release를 확인한 뒤 새 누름부터 확정한다. 이 release gate는 Scene/UI 경계의 상태이며 Manager에 UI 정책으로 넣지 않는다.
5. 입력 전달을 중단해도 Manager는 실제 기기 상태와 해제를 계속 추적한다. Layer 변경을 이유로 Manager의 키 상태를 임의로 None으로 초기화하지 않는다.
6. 삭제·비활성·clip 변경 및 콜백 이후에는 대상 ID/가용성/기하를 다시 확인한다. 포커스 복원 체인·제스처 취소의 재진입 보호를 유지한다.

## 7. 기존 구현 이관 계획

- `EInputKey`, 공통 이벤트·상태 계약을 제안 `Input/InputTypes.h`로 이동하여 Scene 헤더 종속을 없앤다. UI용 유사 키 타입을 별도로 복제하지 않는다.
- `IInputSource`와 기존 Windows 공급자를 재사용한다. Manager는 공급자 출력으로 기기 상태를 축적한다. 공급자의 레코드 변환용 정보와 Manager의 공개 눌림 상태 책임을 구분하여 중복 판정을 정리한다.
- `Event`/`Timeout`/`Closed`/`Error` 구분을 보존한다. 읽기 실패를 None이나 정상적인 무입력으로 숨기지 않는다. 자원 정리와 오류 보고는 앱 경계에서 수행한다.
- Scene에 실제 Layer 목록과 최상위 결정을 도입하고 기존 루트의 입력 후보 선택을 그 범위 안으로 제한한다. 기존 NonModal 지원 코드/검증 이력은 새 배타 정책의 완료 증거가 아니다.
- 기존 IUI의 선택적 콜백과 공유 기하/gesture 기능은 재사용할 수 있다. 단일 상태 조회만으로 기존 순서 기반 클릭·짧은 확정·반복·관통 차단을 대체하지 않는다.
- 실제 native 포커스 상실 감지의 호스트별 신뢰성, 입력 재활성화와 누락된 해제의 복구는 구현/실제 콘솔 검증 항목이다. 이미 보장된 기능으로 기록하지 않는다.
- 구현 시 일반 헤더 include 금지, 안정된 공통 계약/PCH, 구체 타입의 CPP 의존 순서, 파일/VS 필터 일치와 경고 0 정책을 따른다. 최초 문서 작성 단계에는 프로젝트 파일을 변경하지 않았으며, 구현 단계에서 Manager/공통 타입의 프로젝트·필터 등록을 반영했다.

## 8. 구현 순서와 로드맵 연결

기존 task ID를 재사용하고 Scene Layer/통합 검증은 새 ID로 분리한다. 담당자와 일정은 미정이다.

| 순서 / ID | 작업 | 선행 / 완료 기준 |
|---|---|---|
| 1 / INP-01 | 공통 입력 타입·기존 공급자 계약 정리 | 입력 타입을 Input 경계에 두고 기존 읽기 결과 의미 보존. 현재 일부 구현과 목표 미완료를 구분 |
| 2 / INP-02 | Windows 공급자와 상태 축적 경계 정리 | INP-01. 직접 OS 조회는 공급자에만 존재, 기존 키/마우스/좌표/오류 보존 |
| 3 / INP-03 | 상태 전용 FInputManager 구현 | INP-01/02. 네 상태·비소비 조회·짧은 전이·반복·Held·마우스/휠 검증, UI/Layer 의존 없음 |
| 4 / INP-04 | Scene UILayer/UITopLayer와 입력 단계 분리 | INP-03 및 기존 UI-06 기반. 최상위에만 입력 처리, 키/마우스/휠 동일 차단, UI 수명/일반 Update 유지 |
| 5 / INP-05 | 실제 Scene/UI 통합·경계 회귀 | INP-03/04 및 UI-02/04/10. 기존 조작·좌표·전환/release gate, 실제 공급자와 최종 EXE 검증 |

- [x] InputManager는 상태만, Scene은 UILayer/UITopLayer와 차단을 소유한다는 설계를 기록했다.
- [x] 네 상태·조회 불변성·최상위 전용 입력·Update 분리·전환 경계와 이관 계획을 기록했다.
- [x] INP-01 구현/기존 부분 이관 완료.
- [x] INP-02 공급자 경계 정리 완료.
- [x] INP-03 상태 전용 Manager 구현·회귀 완료.
- [x] INP-04 Scene Layer 입력 제어 구현·회귀 완료.
- [x] INP-05 실제 통합 및 필요한 빌드/분석/콘솔 검증 완료.

기존 UI-06/07/08/09, UI-01~05, UI-10의 다른 완료 기준과 데이터 작업의 증거는 유지한다. 기존 전체 P3를 이 문서 작성으로 완료 처리하지 않는다.

## 9. 수용 사례 — 구현 때 실행할 검증

| 사례 ID | 입력/조건 | 기대 결과 |
|---|---|---|
| KEY-01 | 키 누름→유지→해제 | None→Pressed→Held→Released→None, 최초/해제 프레임 한정 |
| KEY-02 | 같은 프레임 여러 번 조회, 이벤트 없는 다음 프레임 | 조회가 소비되지 않으며 눌림은 Held로 유지 |
| KEY-03 | 한 프레임 down→up / up→down | 순서와 두 전이 정보 보존, 빠른 확정 누락/중복 없음 |
| KEY-04 | 반복 KeyDown·묶인 반복, 숫자/숫자패드 중첩 | 최초 확정 한 번, 탐색 반복 보존, 다른 키 해제로 눌림 소실 없음 |
| LAYER-01 | Layer 1 위 Layer 2에서 Enter·숫자·클릭·휠 | Layer 2만 처리. Layer 1 입력 콜백/행동 없음 |
| LAYER-02 | 최상위 빈 영역·전부 비활성·포커스 없음·Unhandled | 아래 Layer로 입력 전달 없음 |
| LAYER-03 | 같은 Layer에 여러 버튼 | 포커스/단축키/마우스 대상만 확정하며 다른 버튼 중복 실행 없음 |
| LAYER-04 | 최상위/중간 Layer 닫기, 번호 공백, UI 없음 | 실제 목록의 UITopLayer/없음과 일치, 잘못된 index 접근 없음 |
| LAYER-05 | Enter/클릭으로 팝업 열기·닫기, 누른 채 유지 | 같은 입력/잔여 배치가 아래를 실행하지 않으며 release 뒤 새 누름부터 확정 |
| LIFE-01 | 콜백 중 자기 닫기/Scene 전환·대상 비활성·삭제 | 지연 적용과 ID/기하 재검증, 취소 후 클릭/드롭 없음 |
| LIFE-02 | 팝업/Scene 준비 실패 | 이전 UI·최상위·선택/포커스 보존 |
| SYS-01 | 포커스 상실·viewport/clip 변경·입력 종료/오류 | 취소/기하 갱신과 기기 상태 복구, 오류를 정상 무입력으로 숨기지 않음 |
| COMPAT-01 | 기존 세 Scene·네 옵션·작은 viewport·다른 cwd | 기존 OptionId/숫자 고정·비활성·공유 rect/clip·Escape 즉시 종료와 dat 시작 계약 보존 |

가짜 입력 공급자와 실제 production CPP 회귀를 먼저 사용하고, native 통합 후 필요한 Debug/Release × x64/Win32 빌드 및 코드 분석을 실행한다. 최종 EXE 콘솔 입력도 확인한다. **표는 실행 계획이며 현재 PASS 기록이 아니다.**

## 10. 비목표와 미결정 사항

- 새 팝업 콘텐츠·문구·선택지 수·Escape UX·전투 pause·키 설정 저장·임의 문자/IME 입력·게임패드·다른 OS 지원은 이번 설계의 구현 범위가 아니다.
- 자유 ZIndex, Layer 간 입력 관통, 별도 전역 UI 입력 Context 스택은 추가하지 않는다.
- 구현된 클래스/메서드·Layer 저장 구조·없음 표현은 §11과 실제 헤더를 따른다. 프레임 입력 처리량 제한 및 실제 호스트의 native 포커스 알림 신뢰성은 별도 검증/개선 범위다. 이를 이유로 Manager에 UI 정책을 다시 넣지 않는다.
- InputManager 개발을 이유로 Excel·아트·생성 데이터·GameData.dat 암복호화/키 정책·사용자 시안·FDataScene.h 원래 주석을 변경하지 않는다.



## 11. 구현 결과와 현재 검증 범위

### 구현된 경계

- `Input/InputTypes.h`에 공통 입력 타입을 옮기고 `FInputManager.h/.cpp`를 추가했다. 앱이 공급자를 소유하고 Manager가 참조한다. Manager는 Scene/Layer/UI 포커스·캡처를 모른다.
- `Update()`가 프레임 전이와 순서 있는 이벤트를 수집한다. `GetKeyStatus`/`WasKeyPressed`/`WasKeyReleased`/`IsKeyDown` 및 마우스 대응 조회는 const·비소비다. 다섯 마우스 버튼, viewport 셀 좌표/이동과 raw 휠 delta를 제공한다.
- `SInputEvent.PhysicalKey`와 `IsLogicalTransition`으로 숫자/숫자패드 같은 물리 alias를 보존한다. 중간 alias 해제는 UI 확정 latch를 해제하지 않는다. 기존 클릭 경로는 왼쪽 버튼만 사용한다.
- `FDataScene`의 `vector<SUiLayer> m_uiLayers`가 실제 Layer/UI 소유권의 원본이고 `m_uiTopLayer`는 그 목록의 최상위 안정 ID다. Level 조회는 같은 항목에서 계산한다. 같은 Level에서는 나중에 열린 Layer가 앞이다. Root의 독립 popup stack/Modal 정책은 제거하고 Layer 내부 UI 처리만 남겼다.
- `OpenUiLayer`, `AddUiToLayer`, `CloseUiLayer`, `SetUiLayerVisible`, `FlushUiLayerCommands`, `GetUiLayer`, `GetUiTopLayer`, `GetUiTopLevel`을 제공한다. 열기/닫기/표시 변경은 안전 지점에 commit한다. UI 없음은 nullable `GetUiBaseRoot()` 등으로 처리하며 기존 `GetUiRoot()` 참조 API는 기본 Layer가 있을 때만 사용한다.
- FUIManager는 FDataScene에 비소유 연결하여 입력을 Scene 경유로 전달한다. 별도 최고 Layer 복사본이 없다. Scene은 일반 Update와 입력 처리를 분리하고 하위 Layer에 입력을 호출하지 않는다.
- Main은 공급자 BeginBatch 뒤 Manager Update를 한 번 호출한다. Closed/Error의 부분 프레임은 UI에 보내지 않고 정상 Event 프레임만 SceneManager의 가상 frame 경로로 전달한다. 전환 때문에 Manager를 Reset하지 않는다. 기존 dat 선행 로딩과 Escape 종료 정책을 유지한다.

### 확인한 검사와 최종 판정

- 상태 서비스: `/W4 /WX /Y- /analyze`, Debug/Release × x64/x86 각각 168 checks, failures=0·경고 0. 실제 소유 콘솔의 입력/provider/manager·모드 복원 경로도 포함한다. 네 실행을 서로 다른 검사 총수로 합산하지 않는다.
- Scene/UI: x64/x86 각각 37 checkpoint group 및 TextArt 생성 헤더 제외 x64/x86 통과. Main 연결용 가상 frame 경로 RED→GREEN을 추가한 통합 native x64는 38 checkpoint group이다. checkpoint는 독립 단위 테스트 수가 아니다.
- 구조 검사: 일반/생성 헤더·PCH 정책 3개, 입력 프로젝트/필터/Main 계약 3개, 기존 dat 빌드 계약 5개가 통과했다.
- 최종 네 구성 solution Rebuild와 native game/packer 분석·package verify의 16개 실행 레코드 모두 성공, 일반/분석 경고·오류 0이다. 네 구성 PCH-off 실제 소스/DesignTimeBuild 경로 검사도 통과했다.
- 실제 최종 EXE는 네 구성 각각 18개 시나리오를 통과했다. 다른 cwd·dat 필수/오류 거부·재컴파일 없는 데이터 변경·실제 소유 콘솔의 MainMenu→Town·Escape와 선택 TextArt 부재를 포함하며 matrix 기록의 최종 EXE SHA와 일치한다. 픽셀/DPI 수동 인증은 아니다.
- **초기 후보의 검토 이력 — 최초 독립 리뷰: 입력 11파일/검증·프로젝트 8파일은 scoped PASS, Scene/UI 6파일은 FAIL이다.** SCENE-01은 UI 추가 중 포커스 콜백에서 Layer가 즉시 파괴될 수 있는 수명 경로, SCENE-02는 숫자 단축키의 포커스 콜백 뒤 조상 가용성을 재검증하지 않는 경로다. 기존 canonical 부정 판정을 보존했다. 첫 bounded 보완에서 두 문제의 행동 assertion RED→GREEN을 확인했고 수정본 x64/x86 UI 회귀는 각각 62 checkpoint group을 통과했다. Root 작업의 busy 수명 경계를 Scene commit/재로드가 존중하며, 포커스 콜백 뒤 안정 ID·세대·조상 가용성·clip을 재검증한다. 수정본 네 구성 solution Rebuild/게임 분석·package verify 12개 레코드도 경고·오류 0이며 실제 최종 EXE 각각 18개 시나리오를 통과했다. Packer 소스는 byte-identical하여 기존 분석 증거를 유지한다. 수정본 PCH-off 네 구성도 통과했다. 마지막 다섯 파일/19개 hunk의 독립 집중 재리뷰가 PASS이며 canonical coverage/verdict 전체와 후보 해시·할당 경로를 대조했다. 두 blocking finding은 모두 해결했고 INP-05를 완료했다. 초기 FAIL은 덮어쓰지 않는다.
- 증거 루트: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/input-runtime-d7c11f34/`. 상태 서비스의 `state-worker/completion.json`, Scene의 `layer-worker/completion.json`, 프레임 통합 `integration-frame-{red,green}/`, 최종 후보 `review-candidate/`는 서로 다른 범위다.
- 실제 폰트/DPI/픽셀 표시 및 호스트별 포커스 감지 신뢰성을 전부 인증한 것은 아니다. 새 스탯 팝업 콘텐츠·전투 pause·엔티티/로그 바인딩은 추가하지 않았다.

관련 문서: [로드맵](../ROADMAP.md), [Scene별 UI 기반 설계](007_Scene별UI객체및입력분리설계안.md), [데이터 계약](005_Excel게임데이터및경로관리설계안.md), [C++ 컨벤션](../Conventions/cpp.md).
