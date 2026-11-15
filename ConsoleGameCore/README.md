# ConsoleGameCore

Windows/C++20 정적 라이브러리다. 기존 중립 API의 `TextRPG` namespace는 소스 호환을 위해 유지하지만, 이 프로젝트는 TextRPG 콘텐츠/생성 헤더/GameData.lib를 참조하지 않는다.

- `Input/`: 기기 프레임 상태와 Win32 입력 정규화.
- `Rendering/`: 문자 셀 framebuffer와 콘솔 출력.
- `UI/`: IUI/FUiElement 상속, Panel/Button/TextArtView 조합, 포커스/캡처/기하, 중립 입력 대상에 연결하는 FUIManager.
- `Scene/`: TScene<TAction> 공개 NVI의 직접/관리 호출 수명 guard, 최초 busy query 보호, FSceneHost의 공통 작업/안전점/재귀 EndFrame guard. 결과 타입은 bool 변환으로 처리 완료만 알리며 게임 명령은 정의하지 않는다.
- `Frame/`: FFramePipeline이 Input → Update → Render/Present → EndFrame을 실행한다. 입력/출력 실패와 재귀 실행에서는 EndFrame을 실행하지 않는다.

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
