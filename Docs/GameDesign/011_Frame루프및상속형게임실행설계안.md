# Frame 루프·상속형 게임 실행 개선 계획

> 상태: **상속형 Frame 구현·TextRPG 이관·필수 실행 및 독립 소스 검토 완료.**
> §1~11은 구현 전에 작성한 계획 이력이다. 사용자가 별도 재승인 없이 완성까지 진행하도록 승인했으며, 실제 채택 API·기본값·실행/검토 범위 및 검증 실패 이력은 **§12**가 우선한다. 당시 제안/미정 문구를 현재 미구현 상태로 해석하지 않는다.
> 기존 [010 라이브러리 경계](010_게임코어및콘텐츠라이브러리분리설계안.md), [008 입력/Layer 책임](008_InputManager및SceneUILayer설계안.md), [009 진행·저장·EndFrame 수명](009_InGameData및저장복원설계안.md)은 유지한다. 의도적 렌더 생략에서도 안전점을 실행하는 새 Frame 정책을 실제 구현했다.

## 1. 의견과 목표

**콘텐츠 작업 전에 상속형 Frame 기반을 정리하는 방향을 권장한다.** 현재 문제는 람다 문법 자체보다 Main이 루프·시간 측정·게임 초기화·행동·저장·종료 상태를 함께 연결하는 데 있다.

- ConsoleGameCore는 실행 순서와 시간 정책을 제공한다.
- TextRPG는 Frame을 상속하여 자기 게임의 초기화·입력·업데이트·렌더·진행 게시·저장을 연결한다.
- 코어는 구체 SceneManager, InGameData, 생성 행동 Enum, 저장 서비스, GameData.lib를 알지 않는다.
- UI 버튼 등 적절한 지역 콜백까지 전부 상속으로 바꾸는 작업은 아니다. 제거 대상은 Main의 프레임 단계 조립용 람다와 외부 루프 상태다.

## 2. 사용자 요구와 이번 권고안

### 사용자 요구

1. Frame 클래스와 실제 루프 코드를 ConsoleGameCore에 둔다.
2. 기본 Frame / Fixed Update / Frame Skip 세 방식을 지원한다.
3. 게임 초기화 시 옵션을 정하고 실행 중에는 변경하지 못한다.
4. Frame의 Run을 호출하면 라이브러리가 루프를 실행한다.
5. TextRPG는 Frame을 상속하고 필요한 OnStart, OnStartFrame, OnInput, OnUpdate, OnEndFrame 등의 훅만 재정의한다.
6. 필요 없는 훅은 구현하지 않아도 된다.
7. 이전 TextShootingGame 구현을 참고한다.

### 추가 권고안 — 아직 사용자 확정 아님

- 기존 명명 규칙에 맞춰 `FFrame`, `SFrameOptions`, `EFrameMode`, 게임 구현은 `FTextRpgFrame`으로 제안한다.
- `Run()`은 virtual이 아닌 공통 진입점, 훅은 protected virtual이며 기본 동작은 빈 함수로 둔다. 훅을 pure virtual로 강제하지 않는다.
- `OnRender()`와 `OnStop()`을 추가하여 출력과 종료 정리도 이름 있는 단계로 분리한다.
- 처음에는 **같은 OnUpdate가 모드에 맞는 delta를 받는 구조**로 간소화한다. 별도 OnUpdate와 OnFixedUpdate를 동시에 호출하는 혼합 모드는 이번 범위에 넣지 않는다.
- Fixed Update의 따라잡기 횟수와 Frame Skip의 연속 렌더 생략 횟수에 상한을 둔다.
- TextRPG의 최초 적용 모드는 기존 실행과 비교하기 쉬운 Basic으로 제안한다. 목표 FPS와 상한의 구체 기본값은 API 검토 때 확정한다.

## 3. 확인한 소스와 이전 구현

### 현재 TextRPG

- [Main.cpp](../../TextRPG/Main.cpp):17–61에서 데이터/세션/Scene/입력/렌더러를 준비한다.
- 같은 파일:62–173에서 isRunning, startingNewGame, 이전 시간, 루프와 네 람다를 조립한다. 업데이트 delta는 steady_clock으로 측정하고 작업 뒤 고정 sleep_for를 호출한다.
- 당시 `ConsoleGameCore/Frame/FFramePipeline.h`:8–22는 한 프레임의 Input → Update → Present → EndFrame만 실행했고 전체 루프·FPS·실행 모드는 제공하지 않았다. 해당 헤더는 이관 후 삭제했으며 현재 구현은 [FFrame.cpp](../../ConsoleGameCore/Frame/FFrame.cpp)다.
- [FSceneManager.cpp](https://github.com/MyungHyun-Ahn/TextRPG/blob/2782feb656713fde315926eb8a85d3ea11c10bf8/TextRPG/Scene/FSceneManager.cpp):411–414의 Update는 현재 Scene으로 전달한다. Fixed 모드에서 단순히 여러 번 호출하면 같은 Scene이 반복 갱신되므로 전환 수락 뒤 추가 갱신을 별도로 차단해야 한다.
- [코어 소비자 테스트](../../Tests/ConsoleGameCore/Main.cpp)는 FramePipeline의 순서·중단·재진입/예외 복구 경계를 사용한다. 실제 이관 시 기존 소비자를 함께 검색한다.

### 사용자 제공 참고 코드 — 읽기 전용

기준 루트: `C:\PROCADEMEY\course1\과제\TextShootingGame\TextShootingGame`

| 파일 | 확인한 방식 | 이번 설계에 반영할 점 |
|---|---|---|
| `FrameSkip/Main.cpp`의 Skip/main | update는 실행하고 예정 시각보다 늦었으면 render만 생략한다. 예정 시각은 한 주기씩 전진한다. | Frame Skip은 로직 생략이 아니라 출력 생략이다. |
| `fixedUpdate/Main.cpp`의 fixedUpdate/main | 누적된 시간 구간마다 추가 갱신하고 render는 외부 루프에서 실행한다. | 한 번의 렌더 주기에서 여러 고정 갱신을 수행한다. |
| `MyShootingGame/TimeManager.h`의 TimerFixedUpdate/TimerRenderCheck | 밀린 갱신 + 기본 갱신, 렌더 뒤 남은 시간 대기를 공통 처리한다. | 시간 제어를 Scene 밖으로 이동한다. 새 구현은 누적 시간에서 갱신 횟수를 한 번 결정하고 무조건 기본 1회를 덧붙이지 않는다. |
| `MyShootingGame/SceneManager.cpp`의 SceneLobby/SceneGame | Scene마다 입력 → 고정 갱신 → 렌더 → 시간 제어를 반복한다. | 반복 절차는 Frame이 한 곳에서 제공하고 Scene은 자기 동작만 수행한다. |

참고 샘플은 50 FPS 기준이며 주기는 20 ms다. 이 값을 새 게임 기본값으로 확정한 것은 아니다. 코드의 의도를 참고하되 timeGetTime 전역 상태·정수 나눗셈·무한 따라잡기·종료 뒤 도달하지 않는 timeEndPeriod 호출을 그대로 복사하지 않는다. 기존 파일은 변경하지 않는다.

## 4. 소유권과 역할

| 구성 | 소유/결정 | 소유하지 않을 것 |
|---|---|---|
| ConsoleGameCore의 FFrame | 불변 옵션 사본, 실행 상태, 시간 측정, 갱신 횟수, 렌더 생략, 대기, 훅 순서, 종료 결과 | 게임 Scene/진행, 저장 시점, 게임 입력 의미 |
| TextRPG의 FTextRpgFrame | Store/Session/Scene/UI bridge/입력 공급자·Manager/렌더러의 앱 수명과 연결 | 루프·타이머 알고리즘 복제 |
| InputManager | 기기 상태·정규화한 프레임 이벤트 | Layer 허용·게임 명령·Frame 실행 모드 |
| Scene/SceneManager | 최상위 Layer 허용, UI/Scene 수명·전환 요청과 안전점 게시 | OS 입력 재조회·Frame 옵션 변경 |
| FGameSession/InGameData | 진행 단일 원본·시작/복원/저장 정책 | 화면·루프 알고리즘 |

- TextRPG는 기존 역순 파괴 계약을 유지한다. Scene/UI 비소유 뷰를 정리한 뒤 진행 루트를 해제하고 Store는 소비자보다 오래 살아야 한다.
- 상속형 Frame은 거대한 공용 Game 클래스나 Manager 싱글톤을 도입하는 이유가 아니다.

## 5. API 개요 — 선언 제안, 제품 코드 아님

```cpp
class FFrame
{
public:
    explicit FFrame(const SFrameOptions& options);
    virtual ~FFrame();

    int Run();
    const SFrameOptions& GetOptions() const noexcept;

protected:
    virtual void OnStart() {}
    virtual void OnStartFrame() {}
    virtual void OnInput() {}
    virtual void OnUpdate(float) {}
    virtual void OnRender() {}
    virtual void OnEndFrame() {}
    virtual void OnStop() {}

    void RequestExit(int exitCode = 0) noexcept;
    void SkipRemainingUpdates() noexcept;

private:
    const SFrameOptions m_options;
};
```

- 실제 구현은 `.cpp`에서 lib로 컴파일한다. Run의 while·시간 분기·대기 코드를 템플릿 헤더나 게임 Main에 남기지 않는다.
- 생성자에서 옵션을 **값으로 복사해 불변 보관**한다. 원래 options 변수를 바꿔도 적용되지 않으며 모드/FPS setter는 제공하지 않는다. Frame 복사·이동은 금지한다.
- 생성자/소멸자에서는 게임 훅을 호출하지 않는다. 완전히 생성한 파생 객체에 Run을 호출한다.
- RequestExit는 정상 종료와 오류 종료를 구분한다. 이미 기록한 실패를 후속 정상 종료 요청이 성공으로 덮지 못한다.
- 각 훅 반환 뒤 종료를 확인한다. OnInput에서 종료하면 Update/일반 Render/EndFrame으로 진행하지 않는다.
- SkipRemainingUpdates는 이번 프레임의 남은 갱신만 중단한다. 게임 종료와 다르며 정상 Render/의도적 Skip/EndFrame 경로는 유지한다. 전환 수락 시 남은 Fixed catch-up이 이전 Scene에 전달되지 않도록 사용하는 제안이다.
- Run 재진입과 종료한 객체의 두 번째 Run은 거부한다. 거부된 내부 호출이 바깥 Run 상태나 종료 정리를 건드리지 않는다.
- 훅 예외는 명시된 Run 오류 경계에서 실패로 기록하고 종료 정리를 한 번만 수행한다. OnStop 실패도 성공으로 숨기지 않는다. 구체 진단 조회 API는 구현 전 계약에 함께 고정한다.
- OnStart에 진입한 뒤에는 초기화 중단/실패를 포함해 OnStop을 한 번 호출한다. OnStop은 부분 초기화를 처리할 수 있어야 하며 RAII를 대신하지 않는다. 옵션/재진입 거부로 OnStart 자체에 진입하지 않았다면 호출하지 않는다.
- 단일 실행 스레드가 기본이다. 실행 중 Frame 자기 파괴와 다른 스레드의 제어는 지원 범위에 넣지 않는다.

## 6. 세 모드의 의미

여기서 **외부 프레임**은 OnStartFrame부터 OnEndFrame까지의 한 회차이고, **고정 갱신**은 그 안의 OnUpdate 한 번이다. 둘을 동일한 횟수라고 가정하지 않는다.

| 모드 | 외부 프레임당 OnUpdate | 전달 delta | OnRender |
|---|---|---|---|
| Basic | 1회 | 실제 외부 프레임 시작 간 경과 시간 | 1회 |
| FixedUpdate | 누적 시간에 따라 0~상한 회 | 매회 동일한 고정 시간 | 1회 |
| FrameSkip | 1회. 전환/종료 중단은 예외 | 고정 시간 | 지연 시 생략 가능 |

### Basic

- 현재 변수 delta 기반 갱신을 유지한다.
- 작업 후 무조건 일정 시간을 더 자는 대신 목표 마감 시각까지 남은 시간만 기다리는 방향을 권장한다.
- 목표 FPS를 OS가 보장하는 정확한 실시간 주기로 표현하지 않는다.

### FixedUpdate

- 누적 경과 시간에서 고정 주기만큼 차감하며 OnUpdate를 호출한다. 시간이 부족하면 0회이며, 충분하지 않은데도 보너스 1회를 호출하지 않는다.
- 렌더는 갱신 묶음 뒤 한 번 수행한다. 보간 렌더링이나 별도 variable-update 훅은 후속 확장으로 남긴다.
- 외부 프레임당 최대 catch-up 횟수와 수용할 최대 경과 시간을 제한한다. 상한을 넘는 오래된 전체 tick 부채는 버리고 소수 주기 잔여만 보존하는 방식을 제안한다.
- 이 정책은 무제한 실시간 따라잡기/결정적 시뮬레이션을 보장하지 않는다. 버린 시간을 진단상 구분한다.
- Scene 전환을 수락하거나 갱신 도중 전환이 요청되면 게임 어댑터가 SkipRemainingUpdates를 사용해 이전 Scene 추가 갱신을 막는다. 미적용된 이번 갱신 부채를 새 Scene에 몰아서 전달하지 않는 정책도 함께 검증한다.

### FrameSkip

- 이전 Skip의 의미대로 **입력·갱신은 유지하고 렌더만 생략**한다. 늦어진 예정 시각을 한 고정 주기씩 전진시키며 따라잡는다.
- 밀린 분량을 하나의 거대한 delta로 전달하거나 입력까지 통째로 생략하지 않는다.
- 연속 생략 상한에 도달하면 한 번 출력하고 오래된 부채를 재동기화하는 방식을 제안한다. 출력 기아와 무한 따라잡기를 막기 위한 제한이며 시간을 버릴 수 있음을 명시한다.
- 단일 목표 FPS를 고정 갱신 주기/렌더 기한의 기준으로 시작한다. 독립적인 렌더 FPS·논리 UPS 설정은 이번 기본 작업 범위에 넣지 않는다.

### 공통 시간/옵션

- 현재 사용 중인 steady_clock 기반 단조 시간 측정을 유지하고 경과 시간·누적기·예정 기한은 코어가 단일 소유한다.
- 옵션 후보는 Mode, TargetFps, MaxCatchUpUpdates, MaxConsecutiveRenderSkips, MaxAccumulatedElapsed다. 알고리즘 설정이며 Excel 게임 밸런스 데이터가 아니다.
- 유효 모드, 양수·유한 FPS, 표현 가능한 양수 고정 주기, 유효한 횟수/시간 상한을 첫 훅 호출 전에 검증한다.
- 측정 시계와 대기는 구분한다. timeBeginPeriod를 사용한다면 성공한 요청만 RAII로 같은 주기의 timeEndPeriod와 짝짓는다. 단순 이관만을 이유로 도입하지 않는다.
- 첫 경과 시간, 정확한 주기 경계, 장기 지연, 중단 뒤 잔여 부채 처리를 결정적 시간 테스트로 고정한다. Basic의 실제 delta와 Fixed/Skip의 보정된 시뮬레이션 시간을 혼동하지 않는다.

## 7. 훅 순서·입력·안전점

```text
OnStart                              한 번
  반복
    OnStartFrame                     viewport 등 이번 프레임 준비
    OnInput                          입력 수집/의미 처리 한 번
    OnUpdate                         모드별 횟수
    OnRender                         실행 또는 정책상 생략
    OnEndFrame                       정상 외부 프레임당 한 번
    다음 프레임 기한까지 대기
OnStop                               종료 정리 한 번
```

### 입력 중복 방지

- InputManager.Update와 Scene 입력 배치 전달은 외부 프레임당 한 번이다. Fixed catch-up 횟수만큼 같은 Enter/클릭 이벤트를 다시 전달하지 않는다.
- 고정 갱신이 0회인 프레임에서도 입력 수집·UI 반응·종료 처리를 막지 않는다.
- 일회성 행동은 OnInput/명시적 게임 명령 소비에서 처리하고, 반복 OnUpdate가 전역 Pressed 상태를 다시 읽어 재실행하지 않는다. 이후 고정 시뮬레이션 전용 입력 명령이 필요하면 0회 갱신 동안 보존/최초 tick 한 번 소비 계약을 콘텐츠 측에서 추가한다.
- Held 상태와 단발 전이를 구분하고 기존 release gate·잔여 배치 무효화·Layer 차단을 유지한다. Frame이 InputManager 상태나 Layer 정책을 대신 소유하지 않는다.
- Frame Skip 중에도 최신 viewport를 준비하고 입력 좌표를 변환해야 한다. TextRPG의 기존 renderer.BeginFrame/입력 BeginBatch 준비를 출력 생략과 함께 무조건 건너뛰지 않는다.

### EndFrame 권고 정책 — 구현 승인 전 확인할 핵심

- Basic 및 렌더하는 Fixed/Skip 회차는 기존 Input → Update → Render/Present → EndFrame 순서를 유지한다.
- **정책상 출력 생략은 Present 실패가 아니다.** 의도적으로 Render를 생략한 회차도 모든 작업이 반환한 뒤 OnEndFrame을 실행하여 전환·정리가 영구 대기하지 않도록 하는 방식을 권장한다.
- 반대로 viewport/input/실제 Present 오류나 훅 예외로 중단한 회차에는 정상 OnEndFrame 게시를 실행하지 않는다. OnStop의 종료 정리와 구분한다.
- FixedUpdate에서는 개별 tick마다 Scene을 즉시 바꾸지 않고 갱신 묶음과 선택적 출력 뒤 외부 프레임 끝에서 한 번 게시한다. 전환 수락 뒤 남은 tick 차단을 이 경계와 함께 구현한다.
- 코어는 실제 Present 성공을 직접 알지 못한다. TextRPG.OnRender가 renderer.Present 실패를 RequestExit(nonzero)로 반드시 전달한다. 기본 빈 OnRender는 출력이 필요 없는 소비자의 정상 no-op이다.
- 일반 Scene/UI busy guard와 최초 virtual~정리 반환 수명 보호는 유지한다. 새 훅 순서만으로 기존 보호를 대체했다고 판단하지 않는다.

## 8. TextRPG 이관

| 현재 Main의 역할 | 이관 대상 |
|---|---|
| 패키지 경로/키/Store 로딩, Continue 판정, 시작 Scene·입력 초기화 | FTextRpgFrame.OnStart |
| renderer.BeginFrame, viewport/backbuffer 준비 | OnStartFrame |
| BeginBatch, InputManager.Update, Scene 행동, Escape/Quit 분기 | OnInput |
| sceneManager.Update(deltaSeconds) | OnUpdate |
| Scene 렌더와 실제 Present/오류 전달 | OnRender |
| sceneManager.EndFrame, TakeStartResult, 게시 결과·autosave 결과 소비 | OnEndFrame |
| 종료 정리·자원 수명 종료 보조 | OnStop 및 RAII |
| while, 시간 계산, sleep, frameCompleted/frameExitCode 조립 | FFrame 내부 |

- Main은 옵션 구성 → FTextRpgFrame 생성 → Run 반환 코드 전달만 남기는 것을 목표로 한다.
- 정상 Quit의 CancelPendingTransitions/현재 진행 저장은 **TextRPG 정책으로 유지**한다. OnStop에서 모든 종료에 무조건 Save하지 않는다.
- 초기화 실패·입력 종료/오류·출력 실패·EndFrame의 destructor Quit를 구분한다. 늦은 종료에서 추가 저장하거나 해제된 Scene을 다시 렌더하지 않는다.
- NewGame Queued/Continue 수락과 실제 EndFrame 게시 결과를 구분한다. 게시 후 autosave, 실패 보존, Continue read-only/키 정책은 바꾸지 않는다.
- 시작 Scene 준비를 위한 루프 진입 전 EndFrame은 기존 초기화 절차로 별도 유지한다. 이것을 외부 프레임 완료 횟수로 세지 않는다.
- FFramePipeline은 소비자를 먼저 이관한 뒤 제거한다. 같은 전체 루프를 가진 구/신 프레임 경로를 영구 병행하지 않는다. 일시 유지가 필요하면 한 실행 경계만 소유하게 하고 제거 조건을 명시한다.

## 9. 구현 작업 계획

아래는 **구현 전 미완료 작업**이다. 각 단계의 검증은 전체 재컴파일이 아니라 기존 타깃·PCH·증분 Build를 사용한다.

| ID | 작업/담당 경계 | 선행 | 완료 기준 |
|---|---|---|---|
| FRM-01 | 공통 API·세 모드·입력 횟수·Skip 안전점·중단/예외 계약 확정 | 검토 | 제안/승인 사항을 구분하고 옵션 기본값·실패 진단·부채 처리까지 한 계약으로 고정 |
| FRM-02 | 코어 FFrame Basic 수직 구현 + 최소 상속 소비자 | FRM-01 | 실제 lib Run, 선택적 훅, 순서·종료·실패·재진입·옵션 불변 검증 |
| FRM-03 | TextRPG 상속 이관, 기본 모드 실제 앱 확인 | FRM-02 | Main 조립 람다/외부 while 제거, 새 게임·Continue·Quit/실패·EndFrame 기존 동작 보존 |
| FRM-04 | FixedUpdate·FrameSkip 시간 정책 | FRM-02와 FRM-03의 안전점 확인 | 가짜 시계로 0/1/복수 tick·과부하·렌더 생략/재개·전환 뒤 추가 tick 차단 검증 |
| FRM-05 | 기존 테스트/프로젝트 소비자 이관·FramePipeline 정리 | FRM-03/04 | 기존 runner 유지, 실제 디렉터리와 프로젝트/filters 일치, GameData 없는 독립 소비자 |
| FRM-06 | 최종 영향 범위 검증·사용법/완료 기록 | FRM-05 | 필요한 제품 Build/분석 경고0, 실제 앱·세 모드 계약, 기존 수명/저장 회귀 보존 |

### 검증 범위와 비용

1. 현재 [Tests/ConsoleGameCore/Main.cpp](../../Tests/ConsoleGameCore/Main.cpp)와 [기존 테스트 진입점](../../Tools/Tests/README.md)을 우선 확장한다. 타이밍 검증은 제어 가능한 시계/대기 경계로 실행하고 실제 장시간 sleep에 의존하지 않는다. 새 중복 runner/소스 복사 빌드는 만들지 않는다.
2. 구현자는 작은 RED→GREEN과 영향 회귀, 통합 담당자는 합친 후보의 실제 앱·최종 빌드/분석을 맡는다. 단순 계획 작업에는 제품 빌드를 실행하지 않는다.
3. Basic 한 경로에서 게임 시작 → EndFrame 게시 → 저장/Continue → 종료를 먼저 통합한 뒤 다른 시간 모드를 붙인다.
4. 실제 모드 테스트는 초기화 옵션으로 시작한 상속 소비자로 수행한다. 테스트를 위해 실행 중 모드를 바꾸는 setter나 제품 디버그 메뉴를 추가하지 않는다.
5. 프로젝트 구성 변경은 기존 Debug/Release × x64/x86 경로와 필요한 분석을 최종 후보에서 확인한다. 일상 수정마다 전체 native/app matrix를 반복하지 않는다. 최종 실행 셀은 변경 의존성과 필요한 계약에 맞춰 FRM-01에서 고정한다.
6. Excel·아트·생성기·저장 포맷은 변경하지 않는다. 관련 없는 Store/암호화 전체 회귀를 프레임 변경마다 반복하지 않는다. 생략한 실행을 PASS로 기록하지 않는다.
7. 사용자 시안 Tests/BattleUiPreview/Main.cpp, FDataScene.h의 아트 주석, 원본 Excel/자산과 기존 실패/차단 이력을 보존한다. 새 Scene 전환 조회가 필요해도 아트 주석을 재작성하지 않는다.

## 10. 범위 밖

- 런타임 모드/FPS 변경, 멀티스레드 업데이트·렌더, 작업 스케줄러, 코루틴/이벤트 버스.
- 별도 variable/fixed 혼합 훅 모드, 렌더 보간, 독립 렌더 FPS·시뮬레이션 UPS, 리플레이/결정적 물리.
- UI dirty 최적화, 전투·상점·장비 콘텐츠, Layer 입력 정책 변경, 새로운 저장 UX/포맷.
- 타이머 샘플 원본 수정, 모든 람다 일괄 제거, PCH 정책 재설계, Clean/Rebuild·새 검증 프레임워크.

## 11. 참고와 현재 검증 상태

- 이 문서의 현재 동작 근거는 §3의 실제 소스이며, 세 모드/훅/API는 제안이다.
- Microsoft DirectXTK StepTimer 공식 문서의 fixed-step 설명도 누적 시간에 따라 여러 update 후 render하는 방식을 설명한다. 여기서는 해당 시간 개념만 참고하고 런타임 setter·callback API는 채택하지 않는다. 출처: `https://github.com/Microsoft/DirectXTK/wiki/StepTimer`.
- timeBeginPeriod/timeEndPeriod의 동일 주기 짝 요구는 Microsoft Learn 공식 문서를 참조한다. 출처: `https://learn.microsoft.com/en-us/windows/win32/api/timeapi/nf-timeapi-timebeginperiod`.
- 이번 작업에서는 계획·로드맵과 그 링크/변경 범위만 검사한다. Frame 구현·제품 빌드·세 모드 실행은 아직 수행하지 않았다.

## 12. 실제 구현과 검증 완료

### 구현된 경계와 실행 방법

- [FFrame](../../ConsoleGameCore/Frame/FFrame.cpp)의 Run이 라이브러리 내부에서 루프·세 모드·시간/대기·중단/예외·한 번의 정리를 실행한다. [FrameTypes](../../ConsoleGameCore/Frame/FrameTypes.h), [IFrameClock](../../ConsoleGameCore/Frame/IFrameClock.h)에 옵션/진단·시간 경계를 둔다.
- 선택적 훅은 OnStart / OnStartFrame / OnInput / OnUpdate(float) / OnRender / OnEndFrame / OnStop이다. default-empty이며 pure virtual을 강제하지 않는다. Run은 nonvirtual/one-shot이고 복사·이동·런타임 옵션 변경은 지원하지 않는다.
- 실제 기본값은 Basic, TargetFps=60, MaxCatchUpUpdates=5, MaxConsecutiveRenderSkips=5, MaxAccumulatedElapsedSeconds=0.25다. 유효 범위와 실패/통계 의미는 [코어 사용법](../../ConsoleGameCore/README.md#불변-옵션과-진단)에 기록했다.
- [FTextRpgFrame](../../TextRPG/Frame/FTextRpgFrame.cpp)이 앱 상태를 단독 소유한다. Main은 초기 옵션 읽기·객체 생성·Run/종료 결과 전달만 수행한다. 기존 FramePipeline 헤더와 프로젝트/소비자 연결은 제거했다.
- 초기 CLI는 `--frame-mode basic|fixed-update|frame-skip`이며 생략하면 Basic이다. 정확한 문자열과 인수 형태만 허용하고 잘못된 인수는 게임 생성 전 종료 코드2로 거부한다. 런타임 모드 변경용 기능이 아니다.
- 단발 입력 전달은 외부 프레임당 한 번이다. HasPendingTransition 읽기 전용 query와 SkipRemainingUpdates로 전환 수락 뒤 이전 Scene의 추가 Fixed tick을 막고 남은 Fixed 누적 부채를 버린다. Scene/UI/진행의 실제 교체는 기존 EndFrame에 남긴다.
- FrameSkip은 Render만 생략하고 정상 EndFrame을 실행한다. 실제 입력/출력 실패는 RequestExit(nonzero)로 중단해 정상 게시하지 않는다. BeginFrame/viewport·입력 준비는 생략하지 않는다.
- key 임시 버퍼는 조기 반환/예외에도 wipe한다. OnStop은 상태의 역순 RAII 정리만 하며 무조건 저장하지 않는다. 정상 Quit 저장·NewGame 게시 후 autosave·Continue read-only·late destructor Quit의 추가 저장 금지를 유지했다.

### 실제 확인한 범위

| 검증 | 실제 범위 |
|---|---|
| 제품 빌드 | Debug/Release × x64/x86 솔루션 Build, 경고0 |
| 코드 분석 | Core와 게임 네 구성 각각 실제 /analyze, 경고0 |
| 독립 Core 소비자 | 네 구성 일반/분석 각각15 checkpoint group, GameData 없는 실제 상속 소비자 |
| Scene 통합 | 네 구성 full93 group(UI62/EndFrame25/진행6), 세 모드 상속 Scene/안전점 포함 |
| 인접 경계 | Debug x64/x86 Input 각168 checks, optional 생성 헤더 부재 각1 group, Core 생성 헤더 include의 정확한 C1083 거부 |
| 실제 앱 | 최종 EXE 네 구성별 기본23 case, 각 세 모드별 진행5 case. HP7/Gold4321 Continue·정상 Quit round-trip·손상/쓰기 실패 보존 포함 |
| 구조/보고서 | 등록/동작 계약24 tests, 헤더/PCH 정책6 tests, 기존 보고서 control13 record, 화면 oracle21 control |
| 캐시/보존 | warm obj/PCH 변경0을 확인했다. 최종 Main EOF 빈 줄 정리 후에는 각 구성 Main.obj만 증분 재컴파일하고 PCH를 재사용했다. Excel·아트·생성물·보호 시안/주석은 불변 |

Core 구현의 scoped TDD 기록에는 정상 실행과 assertion RED, missing-API RED 및 별도의 fixture 설정 오류를 구분했다. 위의 group/case/check/control 수는 서로 겹치거나 다른 단위이므로 총 테스트 수로 합산하지 않는다. 변경 없는 Store/crypto 전체 suite 및 전체 수동 DPI/글꼴 인증을 새로 실행한 것은 아니다.

### 독립 검토와 정리

- 동결 후보 `211cb4ee46582556f1ed114eba37c7f20e2251f6abb10c4b058dd3e5b2157ae5`에서 Core9 경로(삭제1 포함)와 콘텐츠10 경로를 분리해 검토했다. 두 판정 모두 보안/논리 차단0이다. 큰 Scene fixture와 계약 테스트 파일의 변경 hunk/context 검토를 파일 전체 검토로 확대하지 않는다. 검토자는 실행하지 않았으며 실제 실행은 통합 담당 증거다.
- 검토 후 Main 끝의 새 빈 줄 한 개만 제거했다. 검토본과 최종본의 실행/전처리 행은 바이트 동일하며 단일 LF 제거를 별도 기록했다. 이 정리 뒤 새 EXE의 관련 앱 검증을 갱신했고 Core/Scene 실행 코드는 바꾸지 않았다.
- parser의 더 넓은 단위 음성 조합과 runner 인수 전달의 전용 경량 회귀를 추가하라는 비차단 제안2건은 후속 개선으로 남긴다. 이번 실제 세 모드 앱 실행/기존 control 통과와 혼동하지 않는다.

### 검증 실패 이력과 한계

최종 Main 정리 뒤 Debug/x86 기본 앱 검사 한 회차에서 broken-reference 자식 출력이 비어 전체 호출이 실패했다. 별도의 진단 회차에서는 invalid-utf8 fixture의 32비트 패커가 빈 출력으로 실패했다. 당시 원래 runner는 해당 실패 자식의 종료 코드를 별도 보존하지 않았으므로 최초 원인은 미확정이다. 관련 로그와 실패 기록은 지우거나 PASS로 덮지 않았다.

같은 입력의 좁은 실행, 제한된 패커 관측 및 모든 자식 종료를 읽기 전용으로 기록한 기존 실제 __main__/CLI에서는 재현되지 않았고, 최종23 case 전체가 통과했다. 이를 제품 버그를 수정했다거나 외부 원인이 확정됐다는 뜻으로 보고하지 않는다. 이 관측은 최종 성공 기록과 별도로 유지한다.

현재 증거 루트는 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/frame-b0593321`이다. 최종 소스/산출물·현재/승계 범위·두 source-only coverage/verdict·실패 관측은 `current-evidence-index.json`과 `final-acceptance.json`으로 연결한다. 커밋·푸시는 수행하지 않았다.
