# Scene별 UI 객체·텍스트 아트·입력 분리 설계

> **현재 라이브러리 경계 — 구현/실행 확인:** ConsoleGameCore의 공통 UI·중립 FUIManager와 TextRPG의 FGameUiBridge/SUIOption을 분리했다. 메인메뉴·상태·typed 아트 UI는 코어 기반을 실제 상속한다. Layer 소유·최상위 입력 및 진행/수명 보존 정책은 유지한다. 실제 경로와 네 구성 실행/최종 검토 범위는 [010 §10](010_게임코어및콘텐츠라이브러리분리설계안.md)을 따른다. 아래 분리 이전 FUIManager→FDataScene 설명과 검증 수치는 당시 이력이며 현재 공통 API 계약이 아니다.


> **게임 진행 원본과 UI 표시의 경계:** HP·Stats·인벤토리·업적 등은 화면 표시 여부와 관계없이 `InGameData` 단일 루트와 그 내부 직렬화 가능한 진행 객체가 소유한다. Scene/UI는 진행 데이터를 소유/초기화/복제하지 않고 비소유로 조회한다. 아래 UI 멤버의 '상태/현재 대상'은 표시 바인딩·재생성 가능한 표시 모델이며 게임 원본이 아니다. 선택·스크롤·포커스·입력 캡처는 기존 Scene/UI 소유다. 정적 정의는 전부 Excel이며, 최소 모델·FGameSession 소유·v1 저장/Continue·FInGameStatusUi 조회·EndFrame의 앱/UI 상태 루트 일관성은 구현·검증했다([009 §9](009_InGameData및저장복원설계안.md)). 전체 엔티티 아트/조우/장비·업적 게임플레이/전용 UI와 수동 호스트 검증은 미완료다.

> **현재 Asset/PCH 계약 — 2026-11-15:** `GameData.dat` TGDP v2는 `Data/Client/<YAML 경로>`와 `Asset/<자산 경로>`의 원시 바이트를 함께 인증 암호화한다. 런타임은 저장소 소유 메모리에서 아트를 읽으며 txt/YAML 재생성·loose Asset 루트 탐색/fallback은 하지 않는다. 원본 디렉터리는 유지하고 파일명은 `Category_Title_Words.ext`로 통일했다. 아트28개 바이트·ID·규격은 그대로다. 아래 과거의 Asset 별도 배포/미구현·v1 컨테이너·GameDataRoot 설명보다 이 계약과 [Packer 사용법](../../Tools/GameDataPacker/README.md)이 우선한다. 최종 수용/검토 상태는 [로드맵](../ROADMAP.md) 상단을 따른다.

> **최신 Scene 계약 변경:** 사용자 승인으로 `Asset/Scene`의 레거시 배치 txt와 `Scene.AssetPath`를 제거했다. 정적 Scene은 `SceneId`·`SceneType`·`IsStartupScene` 세 필드만 허용한다. TextArt.AssetPath는 유지하며 원본 아트 이름에 맞춘 경로만 수정한다. 아래 예전 네 필드/레거시 자산 유지 설명과 검증 기록은 당시 이력이며 현재 계약보다 우선하지 않는다. Asset 동봉의 현재 계약은 상단 TGDP v2 설명을 따른다. 이번 전환의 최종 검증을 완료했다. 현재 결과는 로드맵의 2026-11-15 Scene 레거시 제거 완료 기록을 따른다.

> **최신 입력 책임/Layer 정책은 [008_InputManager및SceneUILayer설계안.md](008_InputManager및SceneUILayer설계안.md)가 우선한다.** InputManager는 기기 상태만, 런타임 Scene은 `UILayer`/`UITopLayer`와 최상위 Layer 전용 입력 허용을 담당한다. 아래 과거 Modal/NonModal/PassThrough 관통 정책과 이벤트 소비 전용 교체안의 충돌 부분은 대체됐다. 기존 구현/실행 이력은 보존한다. 새 Manager·Scene Layer와 Main 프레임 연결 및 INP-01~05 검증을 완료했다. 마지막 포커스 콜백 보완/집중 재리뷰와 최종 실행 근거는 008 §11과 로드맵을 따른다.

> 작성 기준: 2026-11-14. 현재 소스와 사용자가 확인한 콘솔 시안을 기준으로 한다.
> 상태: **P3 일부 구현: IUI/FUiElement/FUiPanel/FUiButton/FSceneUiRoot, Scene 소유 UI와 비소유 FUIManager 연결, FConsoleInput/IInputSource, FUiTextArt와 MainMenu 로고를 구현했다.** 제목 테두리는 `┌─제목──┐`이다. 입력 상태 Manager와 Scene Layer 이관은 완료했다. Town/Battle의 실제 엔티티 상태·아트·로그 바인딩, dirty 기반 출력 스케줄링과 수동 호스트 검증은 남아 있다. 정적 Scene은 세 필드(SceneId/SceneType/IsStartupScene)만 유지한다. 새 UI 검증은 아래 구현 현황 및 로드맵을 따르며 과거 거부된 스키마의 P1 증거·공식 부정 판정/기록 갱신 거부 상태를 변경하거나 재사용하지 않는다.
> 우선순위: 현재 UI의 새 설계는 이 문서를 기준으로 한다. [004 이전 설계](004_UIManager및화면해상도설계안.md)의 공통 하단 패널·120×40/80×25·1920×1080 목표와 충돌하면 이 문서의 승인된 배치가 우선한다.

> 최신 정정: 사용자 승인으로 **FTextScene은 폐기·삭제**했다. FDataScene의 기존 ASCII 레이아웃 읽기/내용 검증과 프로젝트·테스트 빌드 참조도 제거했다. 아래에서 레거시 로더 유지/검증을 설명하는 대목은 이전 구현의 역사이며 현재 동작이 아니다. 요청 범위대로 Scene.AssetPath 필드·생성기 metadata 검사·기존 txt 자산은 변경하지 않았다. 런타임은 Panel/Button/TextArt UI로만 화면을 구성한다.

## 1. 결정 사항과 InputManager 선행 여부

### 사용자와 확정한 방향

- Windows 콘솔을 사용한다. Win32/Windows SDK에 종속적인 코드 사용을 허용한다.
- 세 Scene의 논리 화면은 **132 문자 셀 × 43행**이다. 기본 표시 창은 **1240×940 픽셀의 일반 창**이며 최대화/전체화면이 아니다.
- 각 Scene은 그 Scene의 UI 객체를 소유한다. 화면의 사각형 박스는 UI 클래스로 관리한다.
- 각 UI 객체는 **자신의 좌측 상단 위치**를 멤버로 가진다. 위치와 크기의 단위는 문자 셀이다.
- 캐릭터·몬스터·마을 그림은 **36×36 유니코드 txt**를 읽어 해당 영역에 표시하고, 표시 대상이 바뀌면 아트를 교체한다.
- 기기 입력 상태와 UI 입력 정책을 분리한다. InputManager는 네 상태와 전이 기록만 관리하고 Scene은 UILayer/UITopLayer를 소유하여 최상위 Layer에만 HandleInput을 허용한다. UI 입력 의미·좌표·수명 보장은 유지하며 구체 이관은 008을 따른다.
- 메인메뉴는 **왼쪽 정확히 83×41 로고 / 오른쪽 세로 네 선택지**다. 새 게임·불러오기·옵션·나가기 순서이며 로고 참조는 제안 FMainMenuUi의 런타임 멤버다. TextArt 카탈로그는 재사용 자산 metadata이며 Scene 배정표가 아니다.
- **모든 SceneType은 비활성을 포함해 선택지 전체가 정확히 네 개**, 순서 1~4다. 네 슬롯을 항상 표시하고 숨김·다섯째 항목·선택지 페이지/페이지 버튼/초과 스크롤을 두지 않는다.

### 결론: 큰 InputManager부터 완성할 필요는 없다

**입력 이벤트 계약 → 작은 기존 입력 어댑터 → UI 기본 객체/Scene 조립 → InputManager로 교체 → 입력 기능 확장** 순서로 진행한다.

UI가 `ReadConsoleInputW`, `_getch`, `GetAsyncKeyState` 등을 직접 호출하지 않는 것이 핵심이다. 키보드만 임시 어댑터로 제공해도 UI 객체 구현을 시작할 수 있다. 다만 실제 마우스·휠·키 반복까지 연결할 때는 최소 InputManager 구현을 선행한다. 렌더링 전용 UI의 구현을 입력 기능 전체가 끝날 때까지 막지 않는다.

클래스 이름과 세부 메서드는 아래의 **구현 제안**이며 아직 존재하는 API라고 가정하지 않는다.

## 2. 현재 구현과 폴더 확인

- `Tests/BattleUiPreview/Main.cpp`: `kLayout`(배틀), `kLayout2`(마을), `kLayout3`(메인메뉴)가 각각 132셀 × 43행이다. 이들은 정적 시안이며 실제 메뉴/입력 이벤트 처리가 아니다. 정적 Town 페이지 표시는 운영 요구가 아니다.
- `TextRPG/UI/Common`: IUI/FUiElement/FUiPanel/FUiButton/FSceneUiRoot/FUiTextArt 구현. FDataScene이 Layer별 루트를 소유하고 최상위를 결정한다. FUIManager는 Scene 비소유 연결이다. MainMenu 로고는 FMainMenuUi에 연결했다. 엔티티 바인딩은 미구현이다.
- `TextRPG/Input`: IInputSource/FConsoleInput과 상태 전용 FInputManager를 구현했다. Main이 공급자와 Manager를 소유하고 정상 입력 프레임을 SceneManager에 전달한다. Layer 정책은 Scene에 있다. 최신 검증은 008 §11을 따른다.
- `Input/InputTypes.h`로 EInputKey/EInputEventType/SInputEvent를 이동하고 EKeyStatus/EMouseButton을 추가했다. `Scene/IScene.h`에는 Scene 행동과 가상 HandleInputFrame 경계를 둔다. 물리 alias와 논리 전이를 구분한다.
- `FTextScene.h/.cpp`: 사용자 승인으로 삭제했다. 기존 ASCII 화면 로더를 대체 구현하거나 다른 이름으로 남기지 않는다. TextArt 로더는 별도 UI 기능으로 유지한다.
- `TextRPG/Rendering/FConsoleRenderer.*`: 현재는 화면 전체를 매번 출력하고 front/back 메모리 버퍼를 교환한다. 이벤트 기반 합성 방식으로의 변경은 별도 구현 항목이다.
- 실제 UI 하위 폴더: Common·MainMenu와 InGameData에 구현이 있다. FDataScene은 Panel/네 Button 기하 및 진행이 있는 경우 FInGameStatusUi의 HP·파생 Stats·Gold·인벤토리/업적 개수 조회를 조립한다. 기존 Town/BattleField의 전체 엔티티 아트·로그 콘텐츠는 후속이다. 현재 런타임은 레거시 ASCII 레이아웃 파일을 읽거나 합성하지 않는다.

## 3. 소유권과 책임

```text
앱 실행부
├─ FGameDataStore (Excel 유래 정적 정의·자산)
├─ FGameSession → unique_ptr<InGameData> (Scene 밖의 단일 진행 루트)
├─ FSaveDataStore (인증 암호화·I/O, 진행 데이터와 분리)
├─ IInputSource 구현 객체 (현재: FConsoleInput)
├─ FInputManager (구현된 상태 서비스; 공급자를 참조)
├─ FSceneManager
│  └─ 현재 Scene
│     └─ Scene UI 루트 (단독 소유; 제안 FSceneUiRoot)
│        ├─ Base: Scene별 콘텐츠 UI
│        │  ├─ 상태/지역/로고 패널
│        │  ├─ 아트 패널 → FUiTextArt
│        │  ├─ 로그 패널
│        │  └─ 선택 패널 → 정확히 네 버튼 (페이지 없음)
│        └─ Popup: IUI 기반 overlay (향후 필요할 때; §4)
└─ FConsoleRenderer → 최종 콘솔 출력
```

| 요소 | 책임 | 하지 않는 일 |
|---|---|---|
| InGameData (현재 최소 모델) | 게임 진행의 단일 원본과 직렬화 가능한 하위 값 소유. FGameSession이 앱 수명에서 관리 | UI·입력·정적 정의 전체 소유, 파일 I/O/암호화 서비스 소유 |
| Scene | UI와 UILayer/UITopLayer 소유, 최상위 Layer에만 입력 처리 허용, 진행 데이터 비소유 조회·OptionId 해석 | InGameData/HP/Stats 소유·Scene 전환 시 진행 초기화, OS 직접 조회, 기기 상태 이중 소유 |
| Scene UI 루트 | Scene이 허용한 Layer 내부의 배치·포커스·입력 캡처·UI 이벤트 라우팅. UI 소유 구조는 008의 단일 원본 이관 기준을 따름 | 독립적인 Layer 간 관통 허용, 전투 계산·저장·이동 실행 |
| 개별 UI | 자기 위치·크기·UI 조작 상태·비소유 대상 바인딩·파생 표시 모델, 자기 영역 그리기, 필요한 경우 hit-test | HP·Stats·인벤토리 등 게임 원본 소유/독립 복제·직접 변경, 다른 Scene 수명 제어, 임의 전역 입력 조회 |
| FUIManager | 활성 Scene에 비소유 연결하고 입력은 Scene을 경유. base root 조회는 뷰일 뿐 최상위 권한이 아님 | Scene UI 트리·최고 Layer·동일 선택 상태 이중 소유 |
| FInputManager | 공급자에서 기기 상태 축적, None/Pressed/Held/Released·반복·순서 있는 전이와 const 조회 제공 | UILayer/UITopLayer·포커스·캡처·UI 차단·게임 액션 결정 |
| FSceneManager | Scene 생성·전환·수명. 이벤트 처리가 끝난 뒤 전환 적용 | UI 포커스 직접 소유 |
| FConsoleRenderer | 셀/속성 합성 결과를 Windows API로 출력 | UI 활성 조건·게임 행동 판단 |

- 단독 소유는 `std::unique_ptr` 또는 값 멤버로 표현한다. 초기에는 공용 캐시·shared_ptr를 도입하지 않는다.
- 상태의 단일 원본: 루트는 포커스/입력 캡처 대상, 버튼은 hover/누름, 선택 패널은 선택 OptionId(전부 비활성이면 없음; 페이지 상태 없음), 로그 패널은 스크롤/자동 추적을 소유한다. 상위 루트와 FUIManager가 같은 상태를 복제하지 않는다.
- 현재 generic `FDataScene`도 각 인스턴스가 자기 UI 루트를 가지면 된다. 처음부터 Scene 파생 클래스를 대량 도입할 필요는 없다.
- `FMainMenuUi`, `FTownUi`, `FBattleFieldUi`는 공통 부품을 조립하는 Scene별 콘텐츠 루트 제안이다. §4의 제안 FSceneUiRoot가 이를 Base entry로 소유하고 팝업과 함께 조정한다. `FCharacterUi`/`FMonsterUi`는 각 현재 표시 대상을 담는 하위 컴포넌트 제안이며 실제 기존 API가 아니다.
- 새 Scene의 데이터·필수 아트·UI를 모두 준비한 뒤 기존 Scene에서 전환한다. 준비 실패는 기존 Scene/UI를 유지한다.
- 전환/종료 시 비소유 UI 연결과 포커스/눌림 대상을 해제한 뒤 트리를 파괴한다. 콜백이 파괴된 Scene을 캡처하지 않게 한다.

### 정적 Scene 데이터와 현재 표시 대상의 분리

생성 `GameData::Scene` 행에는 **SceneId/SceneType/IsStartupScene 세 정적 필드만** 둔다. Scene 인스턴스가 UI를 소유한다는 것은 Scene.xlsx에 현재 그림/이름/지역을 고정한다는 뜻이 아니다. `PlayerArtId`, `ContentArtId`, `TitleTextKey`, `RegionTextKey`는 제거하며 재도입 시 metadata 진단으로 거부한다. Scene→TextArt/제목·지역 Localization 필수 참조, SceneType별 아트 역할/지역 필수 조건과 nullable Scene 예외도 제거한다.

| 제안 컴포넌트 | 런타임에 소유할 현재 바인딩/멤버 |
|---|---|
| `FCharacterUi` | InGameData 현재 캐릭터/스킨의 비소유 바인딩, 정의 조회에서 만든 아트·이름·스탯 표시 모델. 진행 원본은 소유하지 않음 |
| `FMonsterUi` | InGameData 현재 조우 몬스터의 비소유 바인딩, 정의 조회와 현재 HP 등으로 만든 표시 모델. HP·Stats 원본은 소유하지 않음 |
| `FTownUi` | InGameData 현재 마을/지역의 비소유 바인딩, 정의 조회에서 만든 그림·이름·지역 표시 모델 |
| `FMainMenuUi` | 로고 참조/검증된 로고 모델. **83×41은 이 슬롯/멤버 계약** |

다음은 **클래스 멤버/갱신 흐름의 개념 의사코드**이며 컴파일 가능한 선언·구현 또는 이미 존재하는 API가 아니다. 새 엔티티/workbook 스키마도 정의하지 않는다.

```text
class FMonsterUi (제안)
    m_currentTarget       // InGameData 대상에 대한 비소유 ID/유효 수명 뷰
    m_displayArt         // 대상의 정의 조회에서 준비한 표시용 아트 모델
    m_displayName        // 대상의 정의/현지화 조회에서 준비한 표시 문자열
    // HP·Stats 원본 멤버 없음. 진행 상태를 조회해 HP바/문구를 생성한다.

같은 BattleField Scene + 현재 몬스터 A
    → 게임 로직에서 다음 조우 B의 진행 상태를 준비·검증
    → UI는 B의 비소유 바인딩과 정의·진행 조회로 표시 모델 준비
    → 성공 시 진행 상태와 표시 바인딩을 안전하게 게시하고 dirty 표시
    → 실패 시 기존 진행 상태와 A의 표시/바인딩 보존
```

캐릭터 스킨 변경은 InGameData의 진행 상태를 갱신한 뒤 같은 Scene의 `FCharacterUi` 표시를 일관되게 바꾸는 후속 계약이다. 마을/지역 등 확장 진행 원본도 InGameData에 두고 로고·아트·이름·현지화 등 정적 정의는 Excel에서 공급한다. UI 멤버는 비소유 바인딩/표시 모델이지 독립 게임 상태가 아니다. **SceneId/SceneType→현재 엔티티 정적 매핑이나 이를 감춘 SceneUiBinding 표는 만들지 않는다.** 선택지의 SceneType별 공유/TargetSceneId 전환 계약은 유지한다. 현재 최소 진행 교체/비소유 조회와 EndFrame 참조 수명은 009 §9에 구현·검증했으나 위 전체 엔티티 바인딩/필드는 아직 미구현이다.

## 4. UI 기본 객체와 좌표 계약

### 4.1 IUI와 필요한 동작만 재정의하는 구조 — 검토 제안

**2026-11-14 사용자 제안:** 공통 `IUI`에 `Render`, `OnClick`, `OnDrag` 같은 가상함수를 두고 각 UI가 필요한 동작을 구현한다. Scene은 공통 타입으로 UI를 소유하고, 나중에 팝업을 위에 표시했다가 닫으면 아래 UI가 유지되게 한다.

**검토 결론:** 이 방향을 채택했다. 렌더 순서뿐 아니라 입력 차단·포커스 복원·수명 안전을 함께 다룬다. 아래 세부 API는 원래 설계안이며 실제 기본 구현과 남은 차이는 §10 구현 현황을 따른다. 구체 팝업 콘텐츠나 종료 확인창을 만드는 요청으로 확대하지 않는다.

- `IUI`: 가상 소멸자, 필수 `Render`, 공통 기하/hit-test 조회, 선택적 입력 콜백의 계약이다. Windows 입력 조회나 게임 행동을 실행하지 않는다.
- `FUiElement : IUI`: 위치·크기·표시/활성·hit-test 정책 등 공통 상태와 기본 처리를 제공한다. 버튼·패널·아트는 이 기본 클래스를 상속해 필요한 동작만 재정의한다. 이름만 다른 동일 상태를 IUI와 FUiElement 양쪽에 보관하지 않는다.
- `Render`는 순수 가상함수로 두고 구체 UI가 구현한다. 입력 콜백은 기본 `Unhandled`/no-op으로 두어 아트·레이블까지 클릭/드래그 함수를 억지로 구현하지 않게 한다. 패널은 자기 배경/테두리 다음 자식을 렌더하며 루트가 자손을 다시 평탄 순회하지 않는다.
- `OnClick`만으로 누름 상태를 처리하지 않는다. `OnPointerDown/Move/Up`, `OnPointerCancel`, `OnKeyDown/Up`, `OnWheel`을 기반으로 하고 클릭·드래그는 정규화된 포인터 전이에서 파생한다. `OnDrag`는 Begin/Move/End/Cancel 단계를 가진다. 실제 드래그 기능은 필요한 위젯에서만 켠다.
- 클릭·드래그 판정 코드는 개별 버튼마다 복제하지 않고 기본 UI/공통 gesture 처리부에 둔다. 활성 캡처 대상의 단일 원본은 Scene UI 루트이며 위젯의 눌림/드래그 진행 상태와 역할을 구분한다.

```text
개념 계약 — 컴파일 가능한 헤더/확정 API가 아님
IUI
    virtual destructor
    Render(renderContext) const                   // 필수
    Geometry / Visible / Enabled / HitTestPolicy  // 공통 조회
    OnPointerDown/Move/Up(event) -> eventReply     // 기본 Unhandled
    OnPointerCancel(reason)                      // 기본 no-op
    OnClick(event) -> eventReply                  // 기본 Unhandled
    OnDrag(phase, event) -> eventReply            // 기본 Unhandled
    OnKeyDown/Up(event), OnWheel(event) -> eventReply

renderContext = 합성 버퍼 + 누적 콘텐츠 원점 + clip
이벤트 좌표 = Scene 셀 위치 + 공통 변환으로 얻은 대상 로컬 셀 위치
이벤트 결과 = 처리 여부 + 캡처/포커스 요청 + UI 명령
UI 명령/OptionId -> 루트/Scene이 dispatch 종료 후 적용
```

`eventReply`의 `Unhandled`와 **아래 UI로 입력 통과 허용**은 별개다. 처리하지 않은 이벤트를 겹친 뒤쪽 버튼에 자동 재전달하면 팝업 관통이 생긴다. 아래 §4.3의 hit-test/입력 범위 규칙을 우선한다. 인터페이스가 안정되면 `IUI`와 안정적인 공통 계약을 `pch.h`에 두되, 일반 헤더에는 include를 넣지 않고 구체 UI/생성 데이터는 소비 `.cpp`에서 의존 순서대로 포함한다. 정의 위치·구체 반환 타입은 구현 시 컨벤션에 맞춰 확정한다.

### 4.2 저장 자료구조와 그리기 순서

권장 저장소는 **`std::vector<SUiEntry>`**, 각 entry의 실제 객체 소유권은 **`std::unique_ptr<IUI>`**다. 메타데이터가 필요 없는 일반 패널 자식은 `std::vector<std::unique_ptr<IUI>>`로 충분하다. 기본 화면과 팝업을 서로 다른 전역 Manager가 중복 소유하지 않는다.

```text
Scene
└─ unique_ptr<FSceneUiRoot>              // Scene당 한 개, 포커스/캡처 조정자
   └─ vector<SUiEntry>                  // 아래 → 위 순서의 유일한 목록
      ├─ Base: unique_ptr<IUI>           // FMainMenuUi / FTownUi / FBattleFieldUi
      │  └─ 패널·아트·선택지 등 자식 IUI
      ├─ Popup: unique_ptr<IUI>          // 팝업 A, 필요할 때 생성
      └─ Popup: unique_ptr<IUI>          // 팝업 B, A보다 위

SUiEntry = 안정적 UI ID + Layer(Base/Popup) + 입력 정책
           + 복원할 포커스 ID + unique_ptr<IUI>
```

여기서 `FSceneUiRoot`는 Scene별 콘텐츠 루트와 팝업을 함께 감싸는 **제안 호스트**다. 기존 §3의 Scene UI 루트 책임을 구체화한 것이며 `FMainMenuUi` 등을 별도의 포커스/캡처 Manager로 만들지 않는다. UI 트리의 각 객체는 부모 또는 이 entry 중 정확히 한 곳에서만 소유한다.

| 후보 | 검토 |
|---|---|
| `vector` | 화면/팝업 수가 많지 않고 매번 그리기·역순 hit-test를 하는 현재 설계에 우선 채택. 중간 삭제 비용은 허용하며 실제 병목 측정 없이 다른 구조를 도입하지 않음 |
| `list` | 안정적인 iterator와 잦은 중간 삽입/삭제가 꼭 필요한 경우 재검토. 현재는 순회 중 변경을 지연하므로 이를 위해 채택하지 않음 |
| `stack` | 팝업의 사용 규칙은 LIFO에 가깝지만 전체 화면 순회·ID로 닫기·Base/Popup 분리가 필요하므로 저장소로 쓰지 않음 |
| `map`/`unordered_map` | ID 검색 보조 인덱스가 필요할 때만 별도 검토. ID 키의 순서나 hash 순서를 화면 겹침 순서로 사용하지 않음 |

- 순서는 **Base 먼저, Popup 나중**, 같은 Layer에서는 등록 순서로 정한다. 일반 자식도 등록 순서다. 나중 항목이 위에 그려진다. 자유로운 숫자 ZIndex/매 프레임 sort는 초기 범위에 넣지 않는다.
- Base를 늦게 등록해도 Popup 뒤에 append하지 않고 Base 구간 끝에 넣는다. 팝업 열기는 Popup 구간 끝에 추가한다. 자동 bring-to-front는 없고 필요하면 명시적 재배치 명령으로 확장한다.
- 그리기는 앞→뒤, 포인터 대상 탐색은 뒤→앞이다. 중첩 패널도 같은 규칙을 적용한다. 팝업은 콘텐츠 패널의 자식으로 넣지 않고 호스트에 등록하여 그 패널 clip에 잘리지 않게 한다. 화면/Scene clip은 유지한다.
- vector 재할당/erase는 iterator·entry 참조를 무효화할 수 있다. 포커스·캡처·닫기 대상에 vector index나 iterator를 저장하지 않는다. 단독 소유 객체를 보관하는 unique_ptr의 이동과 객체 자체 삭제를 구분한다. [C1][C2]
- 모든 UI에 Scene 수명 동안 재사용하지 않는 ID를 부여하고 루트에서 트리를 조회한다. 초기는 선형 검색으로 충분하다는 설계 가정이다. Scene 교체 시 큐/ID 참조를 폐기하며 다른 Scene ID와 혼용하지 않는다. 파괴된 객체의 raw pointer를 이벤트 배치/콜백 밖에 보관하지 않는다.
- 콜백/Render/순회 도중 컨테이너를 수정하거나 `delete this` 하지 않는다. 열기·닫기·순서 변경은 명령 큐에 요청하고 **현재 이벤트 dispatch가 모두 끝난 안전 지점**에 적용한다. 렌더는 트리 변경 명령을 만들지 않는 읽기 동작으로 유지한다.

### 4.3 입력 대상·모달·드래그 규칙

> **Layer 간 정책 정정:** 아래 Modal 경계 위 여러 entry 허용/NonModal 관통/입력 투과 overlay 설명은 이전 제안 및 현행 구현의 역사다. 새 목표에서는 008에 따라 Scene이 최상위 Layer 하나만 허용한다. Layer 내부의 공통 기하·포커스·클릭/드래그·취소·전역 Escape 보존은 유지한다.

- **기하와 입력 가림:** Visible=false는 그리기/hit-test에서 제외한다. Enabled=false는 보이지만 활성화되지 않는다. 그 영역이 아래를 가리는지는 별도 hit-test 정책으로 판단한다. 기본 입력 표면은 가림이며 비활성 버튼도 뒤 버튼으로 클릭을 넘기지 않는다. 장식/아트의 pass-through는 명시한다. 패널은 자기 표면 제외와 자식 탐색 제외를 구분한다.
- **대상 선택:** 공유 rect/clip으로 가장 위의 입력 대상 하나를 정한다. 필요하면 같은 소유 트리의 부모로만 Unhandled를 전달한다. 아래 형제/다른 entry로 다시 hit-test하지 않는다. 처음부터 pass-through인 표면은 대상 선정에서 제외한다.
- **모달:** 앞으로 추가할 조작용 팝업은 기본 Modal을 권장한다. 가장 위의 표시된 Modal entry를 경계로 삼아 그 entry와 위쪽 entry만 일반 UI 입력 후보로 두고 아래 Base/Popup은 키·숫자·마우스·휠을 차단한다. 모달 바깥 빈 영역을 눌러도 아래로 전달하지 않으며 자동 닫지도 않는다. 모달 전체가 disabled여도 표시된 동안 입력 장벽은 유지한다.
- **비모달:** 필요할 때만 명시적으로 NonModal을 선택한다. 가리지 않는 곳은 아래 UI를 조작할 수 있지만 그 아래 Modal 경계를 넘을 수 없다. 단순 툴팁처럼 입력을 전혀 받지 않는 overlay도 별도 명시한다.
- **키보드:** 활성 입력 범위 안의 포커스 대상으로 전달하고 해당 entry의 콘텐츠 루트까지만 미처리 이벤트를 올린다. 호스트를 통해 다른 entry로 다시 배포하지 않는다. 포커스가 없으면 그 범위의 기본 포커스를 선택하며 없으면 UI 키를 무시한다. Popup에서 처리하지 않은 Enter/숫자를 Base 선택 패널로 흘리지 않는다.
- **클릭:** 같은 활성 대상에서 press/release가 완료되었을 때만 한 번 발생한다. 캡처로 밖의 release를 수신해도 대상 밖이면 클릭하지 않는다. release 때도 현재 활성/표시·clip을 확인한다. 키보드 Enter/숫자는 같은 의미 액션을 호출하되 가짜 마우스 좌표로 OnClick을 호출하지 않는다.
- **드래그:** 지원 위젯이 down에서 캡처를 요청한 경우에만 시작 후보가 된다. 제안 기본 임계값은 시작 셀과 다른 셀로 이동했을 때(`max(abs(dx), abs(dy)) >= 1`)다. Begin 이후 Move/End를 캡처 대상에 전달하며 그 제스처에서는 OnClick을 만들지 않는다. 드래그를 지원하지 않는 버튼의 이동은 드래그를 시작하지 않는다.
- **취소:** 포커스 손실, viewport/레이아웃 변경, 대상 숨김/비활성/삭제, 입력 범위를 바꾸는 팝업 열기/닫기, Scene 전환 시 기존 눌림·드래그·캡처를 취소한다. Cancel은 종료 동작과 구분하며 클릭/드롭을 발생시키지 않는다.
- **기존 Escape 보존:** §8의 전역 Escape 즉시 앱 Quit은 유지한다. Modal이 차단하는 것은 아래 UI 입력이지 명시적 전역 종료 정책이 아니다. Escape로 팝업 닫기를 새 기본값으로 바꾸거나 종료 확인 모달을 추가하지 않는다. 향후 Back 우선 처리를 원하면 별도 UX 변경으로 합의한다. 포커스 손실/resize 같은 시스템 알림도 모달 입력 차단과 별도로 처리한다.

### 4.4 팝업 열기·닫기·아래 화면 유지

> 008의 Scene UILayer/UITopLayer가 새 입력 범위의 원본이다. 아래 투과 팝업 예외는 이전 정책이며 새 Layer 경로에 적용하지 않는다. 준비 실패 보존·지연 적용·아래 UI 유지·안전한 포커스 복원·잔여 배치 폐기와 release gate는 유지한다.

1. **열기:** 팝업 모델/자산/UI를 임시 준비·검증한다. 실패하면 기존 트리·포커스·캡처를 바꾸지 않는다. 성공한 명령만 dispatch 뒤에 commit하여 위에 추가한다. 조작용 팝업은 기존 포커스 ID를 entry에 저장하고 기존 포인터 제스처를 취소한 뒤 유효한 팝업 기본 포커스로 이동한다. 초점 대상이 없으면 모달 범위 안에서 포커스 없음으로 둔다. 명시적인 입력 투과 툴팁은 포커스/캡처/입력 범위를 바꾸지 않으며 아래 5의 입력 배치 폐기 대상에서도 제외한다.
2. **유지:** 아래 UI를 제거하거나 매번 재생성하지 않는다. 선택 OptionId·로그 스크롤·현재 아트/대상 바인딩은 기존 객체에 남는다. 입력이 막히는 것과 게임 시뮬레이션/모델 갱신 정지는 별개이며 팝업이 전투를 자동 pause한다고 가정하지 않는다. 실제 pause 요구는 별도 기획 사항이다.
3. **닫기:** 명령에 UI ID를 담는다. 살아 있는 정확한 entry만 제거하며 이미 닫힌 ID는 no-op이다. 제거 전 해당 하위 트리의 hover/눌림/캡처를 취소하고 참조를 해제한다. 최상위가 아닌 팝업을 닫을 수도 있지만 현재 포커스가 위쪽의 유효한 UI에 있으면 빼앗지 않는다.
4. **복원:** 최상위 팝업을 닫으면 저장한 포커스 ID가 현재 입력 범위 안에 있고 visible/enabled/focusable인 경우에만 복원한다. 아니면 남은 최상위 입력 범위의 기본 대상으로, 그것도 없으면 없음으로 복원한다. 원래 버튼/하위 팝업이 삭제되었어도 dangling 참조를 사용하지 않는다.
5. **입력 관통 방지:** 열기/닫기 commit 후 같은 입력 배치의 잔여 UI 입력은 폐기한다. 입력 공급자는 물리 상태를 계속 추적하고, 그때 눌려 있던 확정 키/포인터 버튼은 release를 확인할 때까지 새 범위에서 재확정하지 않는다. 닫기를 요청한 click/release를 아래 UI에 재전달하지 않는다.
6. **재합성:** 성공한 열기/닫기/이동은 dirty로 표시한다. 다음 프레임에서 버퍼를 비운 뒤 Base→남은 Popup 순서로 다시 그린다. 팝업 사각형만 지우거나 오래된 화면 캡처를 복원하지 않는다. 따라서 팝업 뒤에서 모델이 바뀌었다면 최신 모델이 보이고 잔상이 남지 않는다.
7. **Scene 전환:** 팝업도 현재 Scene의 수명에 속한다. 새 Scene 준비 실패는 기존 UI/팝업을 유지하고, 성공 시 기존 이벤트/명령 큐·비소유 연결·캡처를 정리한 뒤 트리 전체를 파괴한다. 같은 dispatch에서 Scene 전환과 팝업 변경이 함께 요청되면 Scene 전환을 우선하고 옛 Scene의 나머지 팝업 명령을 폐기한다.

**범위 제한:** 네 SceneOption 슬롯·숨김 금지·페이지 없음·132×43·아트 규격·네 정적 Scene 필드는 그대로다. 팝업을 다섯 번째 SceneOption이나 새로운 정적 Scene 바인딩으로 만들지 않는다. 별도 팝업의 선택지 구성도 임의로 2개/가변 개수라고 확정하지 않는다. 팝업의 목적·문구·닫기 UI·행동 수·pause 여부는 콘텐츠 구현 전에 기존 네 선택지 요구와 함께 확인한다. 이번에는 확장 계약과 검증 사례만 남긴다.

### 공통 멤버 제안

```cpp
// 설계용 개념 예시. 실제 구현 파일이나 확정된 기존 API가 아니다.
class FUiElement : public IUI
{
protected:
    SCellPoint m_topLeft; // 부모의 콘텐츠 영역 기준, x/y, 문자 셀
    SCellSize m_size;     // 폭/높이, 문자 셀
    bool m_isVisible;
    bool m_isEnabled;
};
```

- 좌표 계산은 signed int 기반으로 하고 최종 Win32 `COORD`/`SMALL_RECT` 변환 전에 범위를 검사한다.
- 루트 패널의 `m_topLeft`는 Scene 원점 기준이다. 중첩된 UI는 부모 콘텐츠 영역 기준이다.
- 절대 Scene 좌표 = 부모 콘텐츠 원점 + 자기 `m_topLeft`. 중첩 깊이만큼 합산한다.
- 테두리 포함 outer rect와 테두리/여백을 뺀 content rect를 구분한다. 자식 위치는 content rect에 상대적이다.
- 도형 계산과 hit-test는 반열린 구간 `[x, x+width) × [y, y+height)`를 쓴다. Win32 사각형으로 보낼 때만 우측/하단을 각각 1 줄인다.
- 렌더와 마우스 hit-test는 같은 계산된 rect/clip을 사용한다. 화면에 그린 좌표를 입력 코드에서 따로 재계산하지 않는다.
- 패널/아트는 부모 clip 밖으로 쓰지 않는다. 한글 등 2셀 문자를 반쪽만 잘라 쓰지 않는다.
- 기본 겹침 순서는 자식 등록 순서다. 호스트의 Base/Popup 계층은 §4.2를 따른다. 입력은 §4.3에 따라 최상위 대상/Modal 경계를 먼저 정하며, Unhandled여도 뒤 형제로 다시 전달하지 않는다.

### Panel 상단 테두리 제목 — 사용자 요구

- Panel은 `┌─전투필드────────────┐`처럼 **상단 테두리 선 안에 제목을 표시**할 수 있어야 한다. 별도 제목 행을 추가하지 않으며 기존 ContentRect와 자식 버튼 배치는 유지한다. 이 문자열은 모양 설명용 예시이지 코드에 넣을 운영 문구가 아니다.
- 제목은 Panel의 런타임 멤버인 현지화 키로 지정하고 현재 언어의 문자열을 조회한다. 현재 대상에 따른 제목 변경도 Panel/UI 구성 객체에서 갱신하며 정적 Scene 데이터에 제목 필드를 추가하지 않는다.
- 공통 테두리 그리기 기능은 선택적 제목을 받는다. 기본 배치는 왼쪽 위 모서리 뒤에 가로선 한 셀을 둔 `┌─` 다음부터이며 남은 상단 영역은 `─`로 채우고 오른쪽 모서리 `┐`를 보존한다. 제목 가용 폭은 양쪽 모서리와 앞쪽 가로선 한 셀을 제외한 영역이다. 제목이 없거나 제목 문자가 온전히 들어갈 공간이 없으면 일반 테두리로 그린다.
- 제목은 한 줄로 표시한다. 양쪽 모서리 사이의 가용 **셀 폭**을 넘으면 문자 경계에서 잘라 표시하며 한글 등 두 셀 문자의 반쪽은 출력하지 않는다. 줄바꿈 문자는 표시할 때 공백으로 취급하고 원본 현지화 문자열은 변경하지 않는다. 모서리를 덮거나 자식 영역으로 줄바꿈하지 않는다.
- 제목/언어/크기 변경 시 dirty 재합성하여 이전의 긴 제목이 남지 않게 한다. 제목은 장식이며 별도 버튼이나 포커스 대상으로 취급하지 않는다. 기존 외부 clip도 그대로 적용한다.
- 구현 수용 기준: 제목 없음, 짧은 한글/영문, 가용 폭에 맞는 제목, 너무 긴 제목, 한글이 경계에 걸리는 경우, 긴 제목→짧은 제목/제목 없음 전환에서 모서리·배치 보존과 잔상 없음을 확인한다. **요구사항 기록이며 아직 구현/실행 검증 전이다.**

### 권장 공통 부품

| 클래스 제안 | 역할 |
|---|---|
| `IUI` | 가상 소멸자·필수 Render·선택적 이벤트 콜백 계약 |
| `FUiElement` | IUI 기본 구현, 위치·크기·표시/활성·클리핑·공통 입력 처리 |
| `FSceneUiRoot` | Base/Popup entry 단독 소유, 순서·포커스/캡처·입력 범위·지연 명령 처리 |
| `FUiPanel` | 박스 테두리·제목·콘텐츠 영역·자식 소유 |
| `FUiTextArt` | 검증된 txt의 셀 데이터와 표시·교체 |
| `FUiStatusPanel` | 플레이어/몬스터의 이름·레벨·HP바·상태 |
| `FUiLogPanel` | 현지화된 로그, 보이는 행, 스크롤 상태 |
| `FUiOptionPanel` | 정확히 네 세로 선택지·선택 OptionId·숫자키 1~4 매핑(페이지 없음) |
| `FUiButton` | 같은 디자인의 네 선택지 버튼, 활성 OptionId 확정 요청 |

캐릭터/몬스터마다 같은 파일 로더를 복제하지 않는다. 상태 모델과 asset 참조만 바꿔 같은 `FUiTextArt`를 사용한다. 모든 UI가 입력 기능을 구현할 필요는 없다. 공통 `m_isVisible`은 일반 UI용이며 네 옵션 슬롯을 숨기는 수단으로 쓰지 않는다.

## 5. 승인된 Scene 배치

모든 값은 **Scene 원점 기준 (x, y, width, height)**, 테두리 포함이다. 0부터 시작한다. 창 픽셀 좌표와 다르다.

### BattleField / Town: 3열

| 영역 | 사각형 | 내용 |
|---|---|---|
| 플레이어 상태 | `(0, 0, 42, 5)` | 두 Scene 공통 |
| 플레이어 아트 패널 | `(0, 5, 42, 38)` | 내부의 36×36 아트 |
| 몬스터 상태 / 지역·마을 정보 | `(43, 0, 42, 5)` | Scene 맥락에 맞게 교체 |
| 몬스터 / 마을 아트 패널 | `(43, 5, 42, 38)` | 내부의 36×36 아트 |
| 로그 | `(86, 0, 46, 11)` | 로그 8행 + 스크롤/위치 안내 |
| 선택지 | `(86, 11, 46, 32)` | 비활성을 포함한 세로 4개 고정 슬롯, 페이지 없음 |

- 1열/2열 패널 사이와 2열/3열 패널 사이에 각각 1셀 간격을 둔다.
- 아트 패널 내부는 40×36이다. 양옆 2셀 여백으로 아트 36×36을 배치한다.
- Scene 절대 아트 시작점은 플레이어 `(3, 6)`, 몬스터/마을 `(46, 6)`이다. 이를 각 아트 객체에는 부모 콘텐츠 기준 상대 위치로 저장한다.
- Town은 대화/상점/여관/이동, BattleField는 공격/스킬/아이템/방어 순서의 정확히 네 슬롯이다. 미구현 행동은 비활성으로 표시한다. Town 시안의 `1/3`은 버리는 정적 샘플이며 페이지 영역/버튼/상태/입력을 구현하지 않는다. 지역/마을명과 실제 문구는 Excel 키에서 공급한다.

### MainMenu: 로고 + 선택지

| 영역 | 사각형 | 내용 |
|---|---|---|
| 왼쪽 로고 패널 | `(0, 0, 85, 43)` | 원본 브랜딩 보존 로고, 정확히 83×41 (시작점 `(1,1)`) |
| 오른쪽 선택지 패널 | `(86, 0, 46, 43)` | 1. 새 게임 / 2. 불러오기 / 3. 옵션 / 4. 나가기 |

버튼 테두리·선택 표시 디자인은 다른 Scene과 공통 부품을 쓴다. 로고 크기는 임의 가변/자동 맞춤이 아니라 **제안 FMainMenuUi 로고 슬롯의 83×41 고정 계약**이다. 현재 로고 참조/모델은 그 UI의 멤버이며 Scene 행의 필드가 아니다. TextArt의 유효 83×41 항목을 재사용할 수 있지만 카탈로그에 MainMenu/SceneType 배정을 넣지 않는다. 로고 crop에 포함된 원본 브랜딩은 보존하지만 UI 조작 문구는 현지화 데이터로 관리한다.

## 6. Windows API로 좌측 상단부터 그리기

사용자가 말한 기능은 **`WriteConsoleOutputW`의 출력 사각형**으로 구현할 수 있다. 이 함수는 문자·속성 셀 배열을 콘솔 screen buffer의 지정된 사각형에 쓴다. 소스 배열 크기는 `dwBufferSize`, 배열 내 시작은 `dwBufferCoord`, 출력 위치는 `lpWriteRegion`으로 구분한다. [W1]

예를 들어 36×36 아트를 `(x,y)`에 직접 게시할 때의 대상은 `{x, y, x+35, y+35}`다. 이 좌표는 screen buffer 좌표이므로 viewport 원점을 더해야 한다. UI 내부 위치를 곧바로 화면 픽셀 좌표로 넘기지 않는다. [W1]

### 권장 출력 경로

```text
UI.Render(context)
  → 부모 콘텐츠 원점 + m_topLeft 계산
  → 공통 clip 적용
  → 단일 화면 합성 버퍼에 문자·속성 기록
Scene/UI 변경 시 renderer.Present()
  → WriteConsoleOutputW로 게시
```

- 각 UI가 위치를 갖는 것과 각 UI가 콘솔 핸들을 소유하는 것은 다르다. **위치는 UI가, 실제 Windows API 호출은 렌더러가** 담당하는 안을 권장한다.
- 필요하면 렌더러에 `DrawRegion`/`PresentRegion`을 추가할 수 있지만 최초 구현은 전체 132×43 합성 버퍼 하나로 충분하다. 위치를 갖춘 UI 구조가 영역별 API 출력을 막지 않는다.
- 정적 선택형 화면은 최초 출력·선택/hover 변경·로그 추가·상태 변경·아트 교체·리사이즈 등에서만 dirty 렌더링하는 정책을 채택한다.
- 연속 애니메이션이 없으면 상시 프레임 재출력과 front/back 메모리 버퍼 두 개를 필수로 하지 않는다. 이 문서 작성만으로 기존 더블 버퍼 코드를 제거한 것은 아니다.
- 문구가 짧아지거나 아트가 바뀔 때 이전 셀이 남지 않도록 해당 프레임/영역을 먼저 공백과 기본 속성으로 채운다.

### 3가지 좌표를 구분

1. UI/Scene 논리 좌표: 132×43 셀. viewport가 더 크면 Scene 원점을 중앙에 둘 수 있다.
2. 콘솔 screen buffer 좌표: 논리 좌표 + Scene 원점 + 현재 viewport 원점.
3. 호스트 창 픽셀 좌표: `SetWindowPos`의 위치와 폭/높이. 선호 외곽 창 크기는 1240×940이다. 폰트/DPI에 따라 셀 수가 달라지므로 픽셀 크기만으로 132×43 가용 공간을 보장하지 않는다. [W5]

Windows Terminal의 pseudoconsole에서는 `GetConsoleWindow()`의 HWND가 표시 창이 아닐 수 있다. 테스트에서 쓴 고유 제목 기반 표시 HWND 탐색은 로컬 호스트에 한정된 방법이며 모든 실행 환경 보장으로 확대하지 않는다. 다른 터미널/탭을 임의로 키우지 않고 실패를 진단한다. [W4]

기본 수용 호스트는 **classic conhost / Consolas**, 선호 창은 기존 1240×940 일반 창이다. 실제 viewport가 132×43보다 작으면 현지화된 크기 부족 안내를 표시하고 키/마우스 메뉴 탐색·확정을 함께 차단한다. 크기 회복 시 다시 합성하고 눌림 캡처를 복원하지 않는다. Escape의 앱 종료 정책은 유지한다. Windows Terminal은 best effort이며 글리프/픽셀 동일성을 보장하지 않는다. 글꼴 fallback·DPI·viewport 원점/창 크기와 차단 동작의 실제 수용 검증은 P3에 남긴다.

## 7. 유니코드 txt 아트의 로딩·교체 계약

### Excel 매핑과 레거시 전환

상세 스키마와 실제 workbook 편집 예시는 [005 §4.1](005_Excel게임데이터및경로관리설계안.md)을 따른다. 아래는 P2에서 확정한 계약이며 P1의 파일 검증/생성 성공 기록이 아니다.

| 데이터 | 확정 계약 |
|---|---|
| Scene 정적 필드만 유지 | `SceneId`, `SceneType`, `IsStartupScene`. 자산 경로·현재 표시 대상/아트/이름/지역 필드 없음 |
| 독립 선택적 TextArt 카탈로그 | `#Target=Client`. `ArtId` string Primary/Required/Scope Shared, `AssetPath` string 및 `Width`/`Height` uint32는 Required/Scope Client. 있으면 Scene 유무와 무관하게 검증 |
| 현재 바인딩 | 제안 FCharacterUi/FMonsterUi/FTownUi/FMainMenuUi의 런타임 멤버. 구현은 P3 |
| SceneOption 유지 참조 | `TargetSceneId → Scene.SceneId`, `OptionTextKey → Localization.TextKey` |

- 모든 경로는 GameData 기준 상대 파일 경로다. 현재 대상의 아트 참조/현지화 키를 UI 컴포넌트에 공급하며 구체 경로·이름·지역 문구를 코드에 하드코딩하지 않는다.
- **TextArt는 Scene의 필수 의존성이 아니다.** 다른 Scene/옵션/설정/현지화 계약을 충족하면 카탈로그 없는 Scene workbook도 유효하다. 카탈로그가 있으면 36×36/83×41·UTF-8·glyph·경로를 독립 검증한다. 유효 ArtId/파일의 추가·변경이 Scene 행 편집을 요구하지 않으며 SceneType별 역할은 검사하지 않는다.
- **Scene.AssetPath와 Asset/Scene의 레거시 배치 txt는 폐기했다.** Panel/Button/TextArt로 화면을 구성하며 TextArt.AssetPath의 경로·내용 검증은 유지한다. 아트의 실제 파일명 정정은 원본 바이트·ID·규격을 바꾸지 않는다.
- 새 TextArt 경로는 빈 값·rooted/절대 경로·잘못된 Windows 경로·colon/ADS·`..` 루트 이탈·누락 파일·reparse/symlink 구성요소를 거부한다. rooted 경로를 묵시적으로 고치지 말고 진단한다. Generator는 parser 입력 루트의 부모를 GameData로 일관되게 사용하여 Excel/Sub의 workbook도 같은 자산을 참조한다.
- SceneOption 목적지/레이블 현지화 참조는 `#Reference`를 지워도 우회할 수 없는 유지 계약이다. TargetSceneId는 값이 있으면 비활성이어도 유효해야 하며 활성 StartGame의 유효 목적지는 필수다. Scene 아트/제목/지역 참조 계약은 없다.
- `SceneOption.TargetSceneId`만 값과 `#Default`를 모두 비워 참조 없음으로 표현할 수 있는 제한적 optional 문자열 참조 예외다(YAML null / C++ 빈 wstring). 일반 optional/default 규칙은 바꾸지 않으며 제거한 Scene 필드에 예외를 남기지 않는다.
- Enum 값은 Enums 시트가 단일 원본이므로 Config.OptionOverflowPolicy의 `#Allowed`는 비운다. 네 옵션/4/NoScroll은 데이터 검증으로 강제한다.
- 새 데이터와 아트의 생성·검증은 P1, 아래 런타임 로딩·교체·표시는 P3다. 미사용 현지화/Enum은 마이그레이션 편의를 위해 임의 삭제하지 않는다.

### 추출 대상과 출처

아래 literal과 0 기반 `(x,y,width,height)` crop은 최초 추출의 출처 기록이다. 현재 경로는 사용자 아트의 실제 파일명으로 정정했으며 과거 시안과의 바이트 일치를 요구하지 않는다. 이번 변경에서 아트 내용·ID·규격과 `Tests/BattleUiPreview/Main.cpp`는 보존한다.

| ArtId | 원본 / crop | TextArt.AssetPath | 크기 |
|---|---|---|---|
| MainLogo | `kLayout3` / `(1,1,83,41)` | `Asset/Logo/Logo_Text_Rpg.txt` | 83×41 |
| PlayerSwordswoman | `kLayout` / `(3,6,36,36)` | `Asset/Character/Character_Swordswoman.txt` | 36×36 |
| MonsterDragon | `kLayout` / `(46,6,36,36)` | `Asset/Monster/Monster_Dragon.txt` | 36×36 |
| TownWindwood | `kLayout2` / `(46,6,36,36)` | `Asset/Town/Town_Windwood.txt` | 36×36 |

브랜딩이 들어 있는 로고는 원본대로 보존한다. 그 외 이름·설명·상태·버튼·로그·크기 부족 안내는 한국어/영어 Excel 현지화 키에서 가져온다. 기존 Asset/Scene ASCII 파일을 위 파일들로 일괄 변환하지 않는다.

### strict TextArt 검증

1. 바이트를 strict UTF-8로 디코딩한다. 선두 BOM 하나는 허용하고 LF 또는 CRLF를 처리한다. 고립된 CR/제어 문자는 거부한다.
2. 크기 쌍은 정확히 **36×36 또는 83×41**이며 각 행의 셀 폭도 선언과 같아야 한다. 마지막 줄바꿈 하나만 파일 종료로 허용하고 추가 빈 행은 거부한다. 후행 공백은 그대로 보존한다.
3. 허용 집합은 printable ASCII `U+0020..U+007E`, box drawing `U+2500..U+257F`, block elements `U+2580..U+259F`, arrows `U+2190..U+2193`, triangles `U+25B2/U+25B6/U+25BC/U+25C0`, town clock `○ U+25CB`다. 이 계약에서 각 문자는 1셀이다. 탭·제어·CJK·결합 문자·ZWJ·이모지·그 밖의 문자는 거부한다.
4. 자동 자르기·패딩·확대/축소는 없다. 작은 16×16/24×24도 실패다. 카탈로그는 Width/Height 각각의 범위가 아니라 허용 크기 쌍을 검사한다. P3에서 컴포넌트가 현재 아트를 받을 때 자기 슬롯 규격을 확인하며 카탈로그와 SceneType을 연결하지 않는다.
5. 폭 검증과 실제 셀 기록은 동일한 허용 집합/셀 규칙을 쓴다. UTF-8 바이트 수로 셀 폭을 세지 않는다. 임의 Unicode 폭 지원을 주장하지 않는다. 실제 호스트/글꼴의 글리프 지원과 배치는 P3 시각 검증 대상이다.

### 교체는 성공한 경우에만

- `Load/SetArt/ReloadArt`는 임시 데이터에 로드·검증하고, 전부 성공하면 기존 아트와 교환한 뒤 UI를 dirty로 만든다.
- 파일 누락·오류 시 기존 아트와 UI 모델을 보존한다. 최초 필수 아트가 없으면 Scene 준비 실패로 처리하고 이전 Scene을 유지한다.
- 데이터 루트 이탈, reparse/링크로 인한 실제 루트 이탈, 잘못된 크기/문자를 검사한다. 개발자 진단에는 파일·행·예상/실제 크기를 포함한다.
- 매 프레임 파일을 읽지 않는다. Scene 진입·표시 대상 변경·명시적 reload 시 로드한다. 파일 감시/hot reload/공용 캐시는 현재 필수가 아니다.
- 같은 Scene에서 표시 대상이 몬스터 A에서 B로 바뀌면 FMonsterUi의 현재 대상·이름 키·HP·아트를 함께 준비한 뒤 성공 시 적용하는 P3 계약이다. FCharacterUi 스킨 변경도 동일하다. Scene 행/SceneId 변경이나 정적 엔티티 배정은 필요하지 않다.

## 8. 입력 상태 서비스와 Scene Layer 경계

최신 상세 계약은 [008_InputManager및SceneUILayer설계안.md](008_InputManager및SceneUILayer설계안.md)를 따른다. 이 절은 설계이며 해당 이관의 구현/런타임 PASS 기록이 아니다.

```text
FConsoleInput : IInputSource → FInputManager 상태/전이 갱신
  → 앱의 오류·전역 Escape 처리
  → Scene.HandleInput
       → Scene의 UILayer/UITopLayer로 최상위 Layer만 선택
       → 해당 Layer 내부 포커스/hit-test/gesture/UI 행동
  → 안전 지점의 Layer/Scene 변경 → 일반 Update → 렌더
```

- Manager는 None/Pressed/Held/Released, 마우스 위치·이동·휠, 순서 있는 전이 기록을 제공한다. 조회는 입력을 소비하지 않는다. Layer 번호를 인자로 받거나 하위 Layer에 None을 합성하지 않는다.
- Scene은 런타임 UILayer/UITopLayer와 입력 허용을 소유한다. 하위 Layer의 HandleInput은 호출하지 않으며 일반 Update/Render를 통한 전역 입력 조회 우회를 금지한다. 같은 Layer의 여러 버튼에는 포커스/단축키/hit-test로 대상을 한정한다.
- 이전의 “UI/Scene은 SInputEvent만 소비”와 “FInputManager가 공급자를 동일 인터페이스로 대체하므로 UI/Scene 수정 불필요”는 새 목표가 아니다. 기존 공통 이벤트와 IUI 콜백은 내부 전이/gesture 경로에 재사용할 수 있다.
- 실제 IInputSource API는 `ReadEvent(SInputEvent&)`다. Event/Timeout/Closed/Error를 유지하고 Scene/UI는 공급자를 직접 읽지 않는다. Manager가 공급자를 참조하는 수명/프레임 계약은 008에서 정의한다.
- 입력 타입은 Scene/IScene.h에서 공통 Input 헤더로 옮기는 목표다. 같은 프레임의 down/up을 단일 상태 enum만으로 복원하지 않고 전이 기록을 보존한다.
- 콘솔 buffer→viewport 원점 보정은 공급자, viewport→Scene/UI 변환은 UI 측에서 한 번씩 수행한다. 렌더/hit-test의 rect/clip을 공유한다.
- 입력 차단은 키·마우스·휠에 동일 적용한다. 최상위의 빈 공간·비활성·Unhandled도 하위에 전달하지 않는다. Manager의 실제 해제 추적은 계속하며 Scene/UI 경계에서 잔여 입력과 확정 키/버튼 release gate를 관리한다.

### UI 쪽 확정 기본 동작

- **항상 정확히 네 슬롯:** `OptionOrder=1..4`, `MaxVisibleOptions=4`, `OptionOverflowPolicy=NoScroll`. 네 개는 전체 행 수이며 false도 포함한다. 숨김·페이지·다섯째 항목은 없다. 미구현 행동은 false/회색으로 표시한다. 최종 활성은 데이터 IsEnabled와 구현/게임 상태 조건의 AND다.
- **선택 복구:** 초기에는 order가 가장 작은 활성 항목을 선택한다. 선택 항목이 비활성화되면 그 다음 order부터 순환해 활성 항목을 선택한다. 전부 비활성이면 selected OptionId를 비우고 확정하지 않는다. 항목 제거로 네 행이 아니면 유효한 UI 모델이 아니다.
- **키보드:** Up/Down은 활성 항목만 대상으로 순환하며 반복 횟수를 보존한다. 숫자 1~4는 항상 같은 order/OptionId에 대응해 선택·확정한다. 해당 슬롯이 비활성이면 no-op이며 다른 슬롯으로 재매핑하지 않는다. Enter는 현재 선택만 확정한다. Enter/숫자는 최초 KeyDown 한 번만, 반복 이벤트/동일 누름은 다시 확정하지 않는다.
- **마우스:** hover는 키보드 선택과 별도 상태이며 실제 포인터 이동 때만 갱신한다. 같은 활성 버튼에서 press 후 release한 경우에만 한 번 확정한다. 버튼 밖 또는 다른 버튼에서 release하면 확정하지 않는다. 포커스 손실·viewport/레이아웃 변경·Scene 전환 시 누름/캡처를 취소한다.
- **전환 경계:** 이벤트 dispatch가 끝난 뒤 Scene 전환을 적용한다. 전환이 적용되면 그 입력 배치의 잔여 이벤트를 버리고 누르고 있던 확정 키/버튼의 release 전에는 새 화면에서 다시 확정하지 않는다. 준비 실패 시 이전 Scene/UI를 유지한다.
- **Escape:** 기존처럼 즉시 앱 Quit을 유지한다. 새 종료 확인 모달은 넣지 않는다. 향후 하위 메뉴의 명시적인 Back 소비는 별도 범위이며 현재 세 Scene에 선구현하지 않는다.
- **긴 문구:** 표시 셀 경계에서 단어 단위, 단어가 넘치면 문자 단위로 wrap한다. CJK를 반으로 나누지 않고 패널/슬롯 행 예산으로 clip한다. 원문은 현지화 데이터에 보존한다. 긴 문구 때문에 다섯째 선택지나 페이지를 만들지 않는다.
- **로그:** 보이는 본문은 8행이다. 로그 위의 wheel만 로그에 전달한다. 맨 아래를 보고 있으면 새 행을 따라가고, 과거를 보고 있으면 스크롤 위치를 유지한다. 로그 trim/resize 후 유효 범위로 clamp한다. 로그 스크롤은 선택지 스크롤이 아니다.
- **렌더:** 최초/입력/UI·게임 상태 변경/resize 때 dirty로 한 프레임을 합성한다. 먼저 공백·기본 속성으로 지워 잔상을 없앤 뒤 게시한다. 현재 front/back·주기적 출력이 이미 제거된 것은 아니다.

MainMenu/Town/BattleField의 OptionId·ActionType·현지화 및 기존 Enum 값 보존 정책은 [005 §4.1](005_Excel게임데이터및경로관리설계안.md)을 따른다. 이 절은 **정책 결정**이며 이벤트 처리/버튼/로그 구현 완료가 아니다.

## 9. 실제 디렉터리와 구현 순서

기존 Scene별 UI 폴더를 재사용한다. `Common`, `Input`, MainMenu의 기본 구현은 생성했다. 아래 구조의 Town/BattleField 전용 조립 클래스와 완성 InputManager는 남은 목표 구조다.

```text
TextRPG/
├─ Input/                    # 제안: 공통 이벤트·IInputSource·입력 어댑터/Manager
├─ UI/
│  ├─ Common/                # 제안: 좌표·기본 UI·패널·버튼·TextArt·로그/선택 부품
│  ├─ MainMenu/              # 기존 폴더: FMainMenuUi 조립
│  ├─ Town/                  # 기존 폴더: FTownUi 조립
│  ├─ BattleField/           # 기존 폴더: FBattleFieldUi 조립
│  └─ FUIManager.*           # 기존 클래스, 활성 루트 연결/공통 지원으로 점진 축소
├─ Scene/                    # 각 Scene이 자기 UI 루트를 소유
└─ Rendering/                # 셀 합성·클리핑·Windows API 출력
```

| 순서 / 로드맵 ID | 작업 | 의존성 / 완료 기준 |
|---|---|---|
| 1 / INP-01 | 공통 입력 이벤트·IInputSource 계약 | 기존 입력을 감싼 공급자와 테스트 공급자가 같은 이벤트를 전달할 수 있음 |
| 2 / INP-02 | 기존 FConsoleInput의 어댑터 분리 | UI의 직접 키 조회 0개, 기존 기능 유지, 오류/입력 없음 구분 |
| 3 / UI-06 | IUI·좌표·FUiElement·FUiPanel·FSceneUiRoot·Scene 소유권 | 필수 Render/선택 입력, vector entry/unique_ptr 단독 소유, 부모 좌표·역순 hit-test·안정 ID·지연 변경. 팝업 동작은 UI-10에서 검증 |
| 4 / UI-07 | Unicode TextArt 로더와 교체 | 정확한 36×36/83×41 적용, 오류 시 기존 아트 보존, 루트 이탈/잘못된 폭 거부 |
| 5 / UI-08 | Scene별 조립과 데이터 바인딩 | 세 시안의 132×43 동일 배치, 실제 문구/자산 참조는 Excel에서 공급 |
| 6 / INP-03 | 상태 전용 FInputManager 구현 | INP-01/02. 네 상태·비소비 조회·짧은 전이/반복·마우스/휠 제공, UI/Layer 의존 없음 |
| 6 이후 / INP-04 | Scene UILayer/UITopLayer와 입력 단계 분리 | INP-03 및 UI-06 기반. 최상위 Layer만 키·마우스·휠 처리, 일반 Update와 분리 |
| 통합 / INP-05 | 실제 Scene/UI와 Manager 연결·회귀 | INP-03/04 및 UI-02/04/10. 008 수용 사례와 기존 OptionId/좌표/전환/release gate 검증 |
| 7 / UI-01~05, UI-09 | 네 선택지·로그·dirty 렌더와 데이터 계약 이관 | 작은 화면 차단, 비활성 확정 불가·숨김 없음, 한 입력 한 확정, 잔상 없음 |
| 확장 / UI-10 | 팝업 수명·입력 범위·복원·클릭/드래그 경계 | UI-06 및 INP-03 뒤. UI-05 dirty 합성과 통합. §10의 UI-10 사례를 fixture UI로 검증하며 실제 팝업 콘텐츠/UX는 별도 합의 |

INP-01은 완성된 Manager 구현이 아닌 계약이다. INP-02는 작은 기존 입력 어댑터에 한정하며 순서 3의 렌더링 전용 UI 구현과 병행할 수 있다. INP-03을 시작하기 전에 키 입력 UI가 이미 그려질 수 있다. Scene 인스턴스가 UI를 소유하되 SceneType별 정확히 네 선택지를 공유한다. `MaxVisibleOptions=4`/`NoScroll`, 네 정적 Scene 필드·독립 선택적 TextArt·SceneOption 현지화 계약과 컴포넌트 현재 대상 소유 정책을 따른다. UI-09는 이 런타임 바인딩을 구현한다. UI/코드에 새 콘텐츠 원본을 만들지 않는다.

## 10. P2 완료 범위와 P3 검증 항목

**P2 완료 = 위 소유권·입력·배치·네 옵션·TextArt 데이터·레거시 전환 정책의 결정/문서 동기화**다. 정정 P1 실제 파일·검증 명령·종료 결과·테스트 수는 로드맵 통합 기록에 게시했고 공식 리뷰 기록 미완료와 구분한다. 이전 거부된 스키마의 결과를 새 계약 검증으로 재사용하지 않는다. **P3는 일부 구현**이며 아래 체크는 전체 완료 기준이다. 제목/Panel/입력/팝업의 구현·테스트와 아직 없는 엔티티/로그·호스트 동작을 구분한다.

- [ ] UI-06/08: Scene별 132×43, 정확한 outer/content rect, 부모 이동/viewport 원점/중앙 배치, 수명·비소유 연결 해제와 실패 시 이전 Scene 유지.
- [ ] UI-07/08: 정확한 36×36/83×41 txt 로드·안전 교체, 누락/빈/짧은/긴/잘못된 UTF-8·문자·경로 거부, 초기 필수 아트 실패, 기존 아트 보존.
- [ ] UI-08/09: FMonsterUi에서 같은 Scene의 랜덤 몬스터 A→B 교체, FCharacterUi 스킨 변경, FTownUi 현재 마을/지역 및 FMainMenuUi 로고 멤버 바인딩을 구현·검증한다. 아트/이름/상태 동시 갱신·실패 보존·참조 수명을 확인하며 Scene 행에 현재 대상을 고정하지 않는다.
- [ ] UI-03/09: 모든 Scene의 비활성 포함 네 슬롯, order 1~4와 고정 숫자키, 0/1/3/5/8행 거부, 전부 비활성 선택 없음, 상태 변경 후 선택 복구, 숨김/페이지 없음.
- [x] INP-04/05: Scene UILayer/UITopLayer·최상위 전용 입력과 일반 Update 분리, 키·마우스·휠 동일 차단, Manager의 Layer/UI 의존 없음 및 008 수용 사례를 구현·검증한다.
- [x] INP-01~03/UI-02/04: 공급자 교체 전후 같은 OptionId, 공유 rect/clip hit-test, Up/Down 반복·Enter/숫자 최초 누름·동일 버튼 release 확정, 포커스/viewport 취소, Scene 전환 잔여 배치 폐기·release gate, Escape 즉시 종료.
- [ ] UI-01: 실제 conhost/Consolas 및 DPI/폰트 조합, 1240×940 일반 창과 실제 132×43 가용 셀, 크기 부족 현지화 안내와 탐색/확정 차단. Windows Terminal은 별도 best-effort 결과 기록.
- [ ] UI-05/09: ko-KR/en-US·CJK wrap/clip·슬롯 예산, 로그 8행/휠/추적·trim/resize clamp, 단일 합성·dirty 렌더/그림 교체·문구 축약 잔상 제거.

### UI-10 추가 수용 사례 — 기본 구조 fixture 검증, 콘텐츠/호스트 검증은 별도

아래는 §4 설계의 입력 → 기대 결과다. 기본 구조는 Tests/SceneUiRuntime에서 실행하며 실제 팝업 콘텐츠·수동 호스트 결과로 확대하지 않는다. 세부 제안 API와 코드의 차이는 아래 구현 현황을 따른다.

| 사례 | 기대 결과 |
|---|---|
| Base→팝업 A→팝업 B 열기, B 닫기, A 닫기 | 순서대로 겹치며 남은 A/Base 객체·선택·로그 상태 유지, 유효한 포커스 복원, 잔상 없음 |
| Modal 밖/빈 공간/disabled 버튼 클릭, 숫자·Enter·wheel 전달 | 아래 UI 확정/스크롤 없음. Unhandled도 관통하지 않음. 전역 Escape는 기존 Quit 유지 |
| 이전 NonModal/입력 투과 fixture | 기존 정책의 실행 역사로만 보존한다. 새 008 수용 기준은 최상위 Layer 밖 입력도 아래로 전달하지 않는 것이다. |
| 버튼 down 뒤 같은 버튼 up / 밖 up / 숨김·비활성 후 up | 첫 경우만 한 번 클릭. 나머지는 확정 없이 취소 |
| 드래그 지원 위젯 down→다른 셀→밖 up / 포커스 손실 | 캡처 대상에 Begin/Move/End 또는 Cancel. OnClick 중복 없음, 취소 시 드롭 없음 |
| 콜백에서 자기 팝업 닫기·다른 팝업 열기·중복 닫기 요청 | dispatch 끝까지 객체 생존, 뒤에 명령 적용, 중복 닫기 no-op, 같은 입력/repeat가 아래 UI를 확정하지 않음 |
| B가 열린 동안 아래 A 제거, B 닫기 / 복원 대상 disabled | 삭제된 ID를 역참조하지 않고 남은 입력 범위의 기본 포커스 또는 없음으로 복원 |
| vector 재할당·Base 늦은 등록·중간 삭제 | 기존 생존 UI ID 조회 유지, Base가 Popup 위에 오지 않음, iterator/index를 지속 참조하지 않음 |
| 팝업 준비 실패 / Scene 전환 실패·성공 / resize | 준비 실패는 기존 트리·포커스 유지. Scene 실패는 기존 UI/팝업 유지, 성공은 옛 참조·큐 해제. resize는 제스처 취소와 재합성 |

설계 문서의 정책 검증과 실제 런타임 검증은 구분한다. 구현 범위·실행 증거는 아래를 따르며 사용자 시안·workbook·원본 아트는 보존한다.

### 2026-11-14 구현 현황과 검증 경계

- 실제 API는 `UI/Common/IUI.h`, `FSceneUiRoot.h`, `FUiPanel.h`, `FUiButton.h`가 기준이다. 개념 `SUiEntry.Layer` 대신 Base/Popup 구분을 내부 bool로 표현하며 Modal/NonModal/PassThrough 정책을 둔다. 안정 ID와 지연 명령으로 팝업 수명을 관리한다.
- Panel은 부착 전 자식을 조립한다. 부착 후 AddChild는 거부하며 임의 자식 삭제/재배치 API는 아직 제공하지 않는다. 팝업 열기/닫기는 루트를 통해 처리한다. 일반화된 부모 이벤트 bubbling·상태별 테마/hover 표현·padding 설정 확장은 미완료이며 임의의 기존 API로 가정하지 않는다.
- 제목은 런타임 SetTitleKey로 현지화하고 공통 DrawBorder가 `┌─` 접두부와 모서리/셀 clip을 보존한다. 테스트용 SetTitle은 원시 문자열도 허용하지만 운영 콘텐츠 원본을 코드에 추가하는 용도가 아니다.
- FUiTextArt의 strict UTF-8/크기/경로 검증과 교체 실패 보존, MainMenu의 현재 로고 멤버 바인딩 및 카탈로그 없는 Scene 구성을 검사했다. Town/Battle의 캐릭터·몬스터·마을 아트/상태/로그는 빈 Panel 단계이며 완료로 간주하지 않는다.
- 직접 실행: `python Tools/Tests/test_scene_ui_runtime.py --scratch-root <소유 scratch> --label <고유 이름>` 및 `--without-textart`. 부모 재검증에서 runtime 17 checkpoint group과 카탈로그 부재 fixture가 종료 0이었다. 이는 개별 단위 테스트 개수나 전체 P3 완료 수가 아니다.
- Debug/Release × x64/x86 전체 솔루션 Rebuild와 게임 native `/analyze` 모두 경고/오류 0, 네 구성 PCH-off와 헤더 정책 검사를 통과했다. 증거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/iui-implementation/parent-verification/` 및 `parent-fixtures/`.
- 실제 Debug x64 실행 파일을 소유한 별도 최소화 콘솔에서 132×43 viewport로 실행하고 Win32 입력을 주입했다. 메인메뉴 네 선택지 출력, Enter의 Town 전환/네 선택지, Escape 종료 코드 0을 확인했다. 증거: `iui-implementation/console-smoke/result.json`, `main-menu.txt`, `town.txt`. 텍스트 버퍼 smoke이며 시각/DPI 인증은 아니다.
- 최초 독립 검토의 FAIL/다섯 반례는 `iui-implementation/review-results/`에 보존한다. 1차 보완에서 포커스 복원 체인 연결, 콜백 후 제스처 generation/조상 가용성/기하 재검사, clip 기반 포커스 필터, widened 좌표 계산과 clip 뒤 렌더 순회로 수정했다. 수정본의 부모 runtime 24 checkpoint group·카탈로그 부재 fixture·네 구성 솔루션/native 분석·PCH-off가 모두 성공했다. `parent-repair1-fixtures/`, `parent-verification-repair1/results.json`이 최신 증거다. 원래 다섯 probe도 수정 후 기대 결과를 확인했다. 변경 7파일의 독립 집중 재검토는 scoped PASS이며 구체 잔여 차단 결함을 발견하지 못했다. `review-repair1-results/verdict.json` 및 coverage/native 증거를 보존한다. frozen source 41개 해시를 검증했고 부모도 live source 일치를 재확인했다. 최초 전수 검토와 수정 7파일의 집중 재검토를 결합한 결과이지 프로젝트 전체/P3 콘텐츠 완료 인증은 아니다. 실제 conhost/Consolas/DPI, 1240×940 창/132×43 가용 viewport와 수동 마우스 시각 검증은 아직 수행하지 않았다. 기존 주기적 전체 합성 renderer를 유지하며 dirty 스케줄링 완료를 주장하지 않는다.

### 한글 행 테두리 정렬 보완 — 2026-11-14

- 사용자 Windows Terminal 화면에서 한글 버튼 행의 오른쪽 선이 밀리는 현상을 실제 ConPTY 출력으로 재현했다. UI 좌표 문제가 아니라 BMP 두 셀 문자를 glyph + 일반 공백으로 넘긴 출력 표현 문제였다.
- FFrameBuffer가 BMP 전각 문자를 같은 glyph의 선행/후행 CHAR_INFO 셀 속성으로 기록한다. 한쪽 셀 덮어쓰기 시 기존 쌍을 정리하고 색상은 보존하며 입력 색상에 섞인 선행/후행 플래그는 엔진에서 관리한다. surrogate pair 표현과 커서 정책은 이번 변경 범위가 아니다.
- 배치 좌표 변경 없이 네 Town 버튼 행의 오른쪽 끝이 모두 열 126, 패널 끝이 열 131로 맞는 것을 수정 전/후 ConPTY stream 검사로 확인했다. 모서리/한글·CJK/혼합 제목/clip·덮어쓰기·clear/resize를 실제 renderer + ReadConsoleOutputW로 검증했다.
- 최신 검증: native wide-cell fixture, UI runtime 24 checkpoint group, 네 구성 솔루션/native 분석 및 PCH-off/헤더 정책 성공. 증거: `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/ui-wide-cell-alignment/completion.json`. 실제 사용자 폰트/DPI 조합의 픽셀 인증은 아니며 논리 배치와 콘텐츠는 변경하지 않았다.

## 11. 근거 및 관련 문서

### 프로젝트 근거

- [최신 InputManager 상태/Scene UILayer 설계](008_InputManager및SceneUILayer설계안.md) — 충돌하는 입력 책임·Layer 정책에 우선.

- [프로젝트 지침](../../AGENTS.md), [C++ 컨벤션](../Conventions/cpp.md)
- [이전 UI 설계](004_UIManager및화면해상도설계안.md), [현지화 계약](003_다국어문자열및Excel관리원칙.md)
- [Excel·AssetPath 계약](005_Excel게임데이터및경로관리설계안.md), [로드맵](../ROADMAP.md)
- 현재 시안/소유권/입력 경계의 확인 소스: `Tests/BattleUiPreview/Main.cpp`, `TextRPG/Scene/IScene.h`, `TextRPG/Scene/FDataScene.*`, `TextRPG/UI/FUIManager.*`, `TextRPG/Rendering/FConsoleRenderer.*`, `TextRPG/Scene/FTextScene.cpp`.

### C++ 컨테이너·소유권 근거

- C1: Microsoft Learn, `vector class` — 재할당 및 erase 시 iterator/reference 무효화 계약. `https://learn.microsoft.com/en-us/cpp/standard-library/vector-class`
- C2: Microsoft Learn, `How to: Create and use unique_ptr instances` — 단독 소유권 이동과 컨테이너 사용. `https://learn.microsoft.com/en-us/cpp/cpp/how-to-create-and-use-unique-ptr-instances`
- 위 자료는 C++ 동작의 근거다. Layer/Modal/포커스·입력 정책은 자료가 강제하는 표준이 아니라 이 프로젝트에 대한 검토 제안이다.

### Windows API 참조 목록

- [W1: WriteConsoleOutput](https://learn.microsoft.com/en-us/windows/console/writeconsoleoutput) — 셀 배열의 사각형 출력과 좌표 인자.
- [W2: ReadConsoleInput](https://learn.microsoft.com/en-us/windows/console/readconsoleinput) — 입력 레코드 읽기/대기.
- [W3: INPUT_RECORD](https://learn.microsoft.com/en-us/windows/console/input-record-str) — 키·마우스·버퍼 크기 변경 이벤트.
- [W4: GetConsoleWindow](https://learn.microsoft.com/en-us/windows/console/getconsolewindow) — pseudoconsole의 표시 창과 다른 HWND 주의.
- [W5: SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos) — 실제 창 위치·픽셀 크기 변경.
