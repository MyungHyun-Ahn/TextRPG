# ConsoleGameCore

Windows/C++20 정적 라이브러리다. 기존 중립 API의 `TextRPG` namespace는 소스 호환을 위해 유지하지만, 이 프로젝트는 TextRPG 콘텐츠/생성 헤더/GameData.lib를 참조하지 않는다.

- `Input/`: 기기 프레임 상태와 Win32 입력 정규화.
- `Rendering/`: 문자 셀 framebuffer와 콘솔 출력.
- `UI/`: IUI/FUiElement 상속, Panel/Button/TextArtView 조합, 포커스/캡처/기하, 중립 입력 대상에 연결하는 FUIManager.
- `Scene/`: TScene<TAction> 공개 NVI의 직접/관리 호출 수명 guard, 최초 busy query 보호, FSceneHost의 공통 작업/안전점/재귀 EndFrame guard. 결과 타입은 bool 변환으로 처리 완료만 알리며 게임 명령은 정의하지 않는다.
- `Frame/`: 상속형 `FFrame.Run()`이 전체 게임 루프·시간 제어를 제공한다. 초기화 시 Basic/FixedUpdate/FrameSkip을 선택하고 실행 중 변경하지 않는다. 기존 FFramePipeline은 소비자 이관 후 제거했다.

`ConsoleGameCorePch.h`에 안정 표준/SDK·공통 인터페이스를 모으고 라이브러리의 `pch.h`가 이를 포함한다. TextRPG의 `pch.h`도 라이브러리의 `pch.h` 대신 이 이름 있는 공유 헤더를 포함한다. 생성 `GameDataAll.h`와 개별 생성 타입은 어떤 PCH에도 넣지 않는다.

일반 헤더에는 include를 넣지 않는다. 소비 CPP는 `pch.h`(표준/SDK/안정 인터페이스), 완전한 정의가 필요한 중립 헤더를 의존 순서대로 포함한다. PCH/obj는 소비 타깃별로 독립 생성한다.

FUIManager는 IUiInputTarget을 비소유 연결한다. 입력은 그 대상의 허용 경로로만 전달한다. Scene 소유 Layer 정책을 root 직접 dispatch로 우회하지 않는다. Clear의 취소 예외에서는 기존 연결을 복원하되 callback이 게시한 새 연결은 덮어쓰지 않는다. 대상 소유자가 파괴 전에 Detach해야 한다.

FSceneHost는 소유권을 임의 큐로 이동하지 않는다. TryEndFrame은 현재/후보 Scene 및 그 소유 UI가 모두 idle일 때만 typed 소유자가 제공한 commit을 실행한다. 같은 안전점 범위의 재진입을 거부하고 throw 뒤 guard를 복구한다. 소유자는 old UI/Scene 파괴와 도메인 상태 게시/취소를 자신의 typed handle로 조정한다. TextRPG의 Scene/진행 결합 게시와 저장 정책은 콘텐츠에 남는다.

TextRPG의 FGameUiBridge/SUIOption, 현지화 제목 해석, FUiTextArt typed Store 로드는 콘텐츠 adapter다. FMainMenuUi/FInGameStatusUi는 실제 코어 UI 기반을 상속한다.

기존 runner의 독립 소비자:

```text
python Tools/Tests/test_scene_ui_runtime.py --core-only --scratch-root <owned-absolute-path> --label core
python Tools/Tests/test_scene_ui_runtime.py --core-only --analyze --scratch-root <owned-absolute-path> --label core-analysis
python Tools/Tests/test_scene_ui_runtime.py --core-dependency-negative --scratch-root <owned-absolute-path> --label core-negative
```

음성 모드는 생성 콘텐츠 헤더 include가 정확히 C1083으로 거부돼야 성공한다. 일반/분석 소비자는 TextRPG/GameData include 경로나 GameData.lib 없이 파생 UI의 입력/셀 렌더링 및 Scene/프레임 수명을 실행한다. 최종 네 구성/게임 EXE 수용 여부는 별도 통합 증거로 판정한다.

## 상속형 Frame 사용

소비 CPP는 자신의 PCH 다음 `Frame/FrameTypes.h` → `Frame/IFrameClock.h` → `Frame/FFrame.h` → 파생 클래스 헤더 순으로 필요한 정의를 포함한다. 일반 헤더에는 include를 추가하지 않는다.

- `FFrame(const SFrameOptions&)`는 기본 steady-clock을 사용한다. `FFrame(const SFrameOptions&, IFrameClock&)`는 테스트/소비자가 공급한 시계를 참조하며, 그 시계는 Frame보다 오래 살아야 한다.
- `Run()`은 재정의하지 않는 단일 실행 진입점이다. 재진입 및 같은 객체의 재실행은 오류를 반환하고 기존 실행 상태는 변경하지 않는다. 생성자/소멸자에서는 게임 훅을 호출하지 않는다.
- `OnStart`, `OnStartFrame`, `OnInput`, `OnUpdate(float)`, `OnRender`, `OnEndFrame`, `OnStop`은 protected virtual이며 기본 구현이 비어 있다. 필요한 함수만 override한다.
- `RequestExit()`는 정상 종료, `RequestExit(nonzero)`는 실패 종료다. 각 훅 반환 뒤 종료를 확인하며 첫 실패 코드·단계·예외를 후속 정상 종료/정리 실패로 덮지 않는다. `GetFailureStage()`/`GetException()`으로 확인한다.
- OnStart 진입 뒤에는 부분 초기화/훅·시계 예외도 OnStop을 한 번 거친다. OnStop은 RAII 보조 정리 단계이며 자동 저장 기능이 아니다.
- `SkipRemainingUpdates()`는 현재 외부 프레임의 나머지 갱신만 중단한다. Fixed 모드에서는 이전 Scene의 남은 누적 부채도 버린다. Render/의도적 Skip/EndFrame은 계속 진행한다.

| 모드 | OnUpdate | OnRender |
|---|---|---|
| Basic | 실제 시작 간 경과 시간으로 1회, 최초 delta는 0 | 1회 |
| FixedUpdate | 누적 시간에 따라 고정 delta로 0~상한 회 | 갱신 묶음 뒤 1회 |
| FrameSkip | 고정 delta로 1회 | 예정 기한을 초과하면 생략 가능 |

외부 프레임은 StartFrame → Input 1회 → Update 묶음 → 선택적 Render → EndFrame이다. Fixed의 개별 tick과 외부 프레임을 같은 횟수로 보지 않는다. 시간 지연 보정은 코어가, 단발 입력 행동/명령 소비는 게임이 처리한다. 같은 Enter/클릭을 catch-up 횟수만큼 다시 전달하지 않는다.

**의도적 렌더 생략은 정상 EndFrame을 실행하지만 실제 입력/Present 실패나 훅 예외로 중단한 회차는 실행하지 않는다.** 실제 Present 결과는 파생 OnRender가 RequestExit(nonzero)로 연결해야 한다. 기본 빈 OnRender는 출력 없는 소비자의 정상 no-op이다.

### 불변 옵션과 진단

| 필드 | 기본값 | 유효 범위/역할 |
|---|---|---|
| Mode | Basic | Basic / FixedUpdate / FrameSkip |
| TargetFps | 60 | 유한 수 1~1000, 고정 주기와 대기 기준 |
| MaxCatchUpUpdates | 5 | 1~1000, Fixed 한 외부 프레임의 최대 갱신 |
| MaxConsecutiveRenderSkips | 5 | 1~1000, FrameSkip의 최대 연속 출력 생략 |
| MaxAccumulatedElapsedSeconds | 0.25 | 유한 수 0 초과~60 이하, Fixed 수용 경과 시간 상한 |

옵션은 생성 시 const 사본으로 보관한다. 원래 options를 수정해도 실행 객체에는 적용되지 않는다. 옵션 오류는 첫 게임 훅 전에 실패하며 런타임 setter는 없다. Frame 객체는 복사·이동하지 않는다. 단일 실행 스레드가 기본이며 실행 중 자기 파괴/다른 스레드 제어는 지원하지 않는다.

Fixed의 과도한 전체 tick 부채는 버리고 소수 주기 잔여만 유지한다. FrameSkip은 연속 생략 상한에서 출력을 강제하고 그 출력 직전 기한을 재설정한다. 따라서 강제 Render 자체가 오래 걸리면 다음 회차의 지연으로 반영된다. 무제한 따라잡기·결정적 시뮬레이션·OS의 정확한 FPS를 보장하지 않는다.

`GetStatistics()`는 CompletedFrames / UpdateCalls / RenderCalls / SkippedRenders / DroppedSimulationSeconds를 제공한다. 호출 수는 훅 진입 기준이고 완료 프레임은 오류 없이 반환한 EndFrame 기준이다. 게임 진행의 게시/저장 성공 횟수와 다르다.

실제 게임 어댑터는 [FTextRpgFrame](../TextRPG/Frame/FTextRpgFrame.cpp), 설계·구현 및 검증 범위는 [011 §12](../Docs/GameDesign/011_Frame루프및상속형게임실행설계안.md#12-실제-구현과-검증-완료)를 참고한다.
