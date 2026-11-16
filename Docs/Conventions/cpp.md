# Windows C++ 컨벤션

[공통 규칙](common.md)을 따른다. 현재 프로젝트의 C++20, MSVC v145, Windows SDK 및 런타임 설정을 확인하고 호환성을 유지한다.

## 명명
| 대상 | 규칙 | 예시 |
|---|---|---|
| 런타임 객체·서비스·공개 계약 | F + PascalCase | FPlayer, FGame |
| 내부 상태·옵션·스냅샷 | S + PascalCase | SCharacterStats |
| 인터페이스 | I + PascalCase | ICharacter |
| 열거형 | E + PascalCase | ECharacterClass |
| 템플릿 타입 | T + PascalCase | TObjectPool |
| 함수·열거 값 | PascalCase | StartGame(), Warrior |
| 지역 변수·매개변수 | camelCase | playerId |
| 멤버 | m_ + camelCase | m_isRunning |
| 상수 | k + PascalCase | kMaxInventorySlots |

- F/S는 class/struct 문법이 아닌 역할 기준이다. I/E/T는 해당 타입에 우선 적용한다.
- 약어는 단어처럼 쓴다: `FUiState`, `GetNpcName()`.
- 주요 타입 하나를 중심으로 동명 `.h`/`.cpp`를 구성한다. `Main.cpp`는 시작과 종료 연결에 집중한다. 단순 테스트·더미 도구는 예외다.
- 기존 코드의 명명은 별도 요청 없이 전면 변경하지 않는다. 새 코드와 실제 수정 범위부터 적용한다.

## 포맷·헤더
- **탭 폭 4, Allman, 140열**을 사용한다. 생성자 초기화는 선행 쉼표를 사용한다.
```cpp
FPlayer::FPlayer(const SCharacterStats& stats, int level)
	: m_stats(stats)
	, m_level(level)
{
}
```
- **일반 헤더 파일(`.h`/`.hpp`/`.hxx`)에는 `#include`를 작성하지 않는다.** 프로젝트 소유 헤더와 생성 `.g.h` 모두 적용한다. 포함을 모으는 `pch.h`, 안정 전제 공유용 `GameDataPch.h`/`ConsoleGameCorePch.h`, 생성 집계 전용 `GameData/Generated/GameData/GameDataAll.h`·`GameData/Generated/InGameData/InGameDataAll.h`(테스트 스키마도 동일 집계 규칙) 및 수정 대상이 아닌 외부 라이브러리/SDK 헤더는 예외다. 개별 생성 `.g.h`에는 기존 include 금지를 유지한다. 생성 헤더는 직접 수정하지 않고 Generator 출력 규칙을 변경한다.
- include를 자동 정렬하지 않는다. `.cpp`가 필요한 정의를 의존 순서대로 제공한다. PCH 사용 시 `pch.h`를 첫 include로 둔 다음 필요한 구체 타입/생성 Enum/데이터 헤더를 명시적으로 포함한다. 일반 헤더에서 `pch.h`를 포함하거나 강제 포함 설정으로 규칙 위반을 감추지 않는다.
- Windows 전용 게임 런타임의 표시 문자열은 `std::wstring`으로 보관·전달하고 콘솔에는 `WriteConsoleOutputW`를 사용한다. `std::wstring`은 Windows에서 UTF-16 `wchar_t` 코드 단위를 담는다. 게임 문구를 좁은 `std::string`으로 변환하지 않는다. Scene.AssetPath/FTextScene 및 Asset/Scene 레이아웃은 폐기했으며, Scene은 SceneId/SceneType/IsStartupScene 세 정적 필드만 유지한다. 새 TextArt는 별도 strict UTF-8 디코딩·허용 단일 셀 문자·정확한 36×36/83×41 검증 후 UTF-16 셀로 전달하는 [007](../GameDesign/007_Scene별UI객체및입력분리설계안.md) 계약을 따른다. 런타임 로더 통합은 P3이며 기존 ASCII 제한을 일괄 완화하지 않는다. 소스 파일 자체의 문자 인코딩과 런타임 문자열 타입은 별개의 설정이다.
- **표준 라이브러리 및 자주 바뀌지 않는 파일(예: 공통 인터페이스)은 `pch.h`에 모은다.** Windows SDK와 안정된 공통 의존성을 먼저, `IObject`/`IScene` 같은 인터페이스를 뒤에 포함한다. 구체 렌더러/UI/Scene 클래스와 Excel에서 생성되는 데이터·Enum 헤더는 변경 파급을 피하도록 PCH에 넣지 않고 필요한 `.cpp`에서 포함한다.
- 공통 Windows SDK의 고정 링크 의존성을 `#pragma comment(lib, ...)`로 선언할 때도 SDK 헤더를 모으는 PCH에 집약한다. 현재 `bcrypt.lib`/`crypt32.lib`는 `GameData/GameDataPch.h`에서 선언하며 CPP에 중복 작성하지 않는다. 타깃별 프로젝트의 명시적 링크 설정은 별도 빌드 계약으로 유지한다.
- `pch.h`가 있는 프로젝트 루트를 공통 `AdditionalIncludeDirectories`에 명시하여 모든 구성에서 빌드와 VS의 헤더 검색이 같은 경로를 사용하게 한다. `/Yu` 빌드 성공만으로 텍스트 헤더 검색이 정상이라고 판단하지 말고 PCH를 끈 실제 소스 해석도 검사한다.
- TextRPG 소유 CPP는 진행·Custom·저장 구현까지 `#include "pch.h"`를 첫 include로 두고 게임의 루트 PCH 하나를 사용한다. TextRPG에 보조 `GameDataPch.cpp`나 파일별 `GameData.pch` 설정을 두지 않는다. GameData 라이브러리·Packer·Store/Yaml 및 reader 예외 테스트는 `GameData/Runtime/GameData/GameDataPch.h`의 안정 전제를 `GameData/pch.h`로 포함하는 타깃별 PCH를 유지한다. NativeTests에서 TextRPG 구현을 컴파일하는 Scene/InGameData/SaveData suite는 해당 원본과 일치하는 루트 PCH를 자체 생성하며, GameData 구현·생성 fixture의 PCH와 구분한다. compiled PCH는 타깃·구성·아키텍처·분석/ABI별로 격리한다. 치환 reader·매크로·직접 포함 구현은 PCH 뒤에 두고 생성/구체 타입은 PCH에 넣지 않는다. 의도적인 PCH-off 진단·보조 소비자는 안정 헤더를 일반 include로 읽을 수 있지만 기존 미사용 상태를 모든 공유 타깃의 영구 규칙으로 강제하지 않는다.
- 각 라이브러리의 `pch.h`는 해당 이름 있는 전제 공유 헤더를 포함한다. TextRPG의 `pch.h`는 `GameData/GameDataPch.h`와 `ConsoleGameCorePch.h`를 포함하며 다른 프로젝트의 `pch.h`를 직접 포함하지 않는다. `GameDataAll.h`와 `InGameDataAll.h`는 필요한 `.cpp`에서만 포함하고 모든 PCH·전제 공유 헤더에서 제외한다. 이름 있는 공유 헤더는 compiled PCH 파일 공유를 뜻하지 않는다.
- 포인터/참조는 가능한 경우 전방 선언을 사용한다. 값 멤버·상속·Enum 열거 값·템플릿에 완전한 정의가 필요하면 소비 `.cpp`에서 그 정의 헤더를 먼저 포함한다. 이 프로젝트의 일반 헤더는 단독 include 가능성을 보장하지 않으며 명시적 소비 순서와 실제 빌드로 검증한다. 표준 라이브러리 타입을 임의 전방 선언하거나 컴파일 오류를 피하려고 값 멤버를 포인터로 바꾸지 않는다.
- 파일과 필터는 확장자별 분류 대신 실제 기능 디렉터리를 함께 사용한다.

## 소유권·오류
- 단독 소유는 `std::unique_ptr`, 실제 공유 수명만 `std::shared_ptr`를 사용한다. 일반 raw pointer는 비소유 nullable 참조이며 non-null 인자는 참조를 우선한다.
- 자원은 RAII로 관리한다. 필요한 경우 복사/이동을 제한하고 const 정확성을 지킨다. 변환 생성자에는 `explicit`, 중요 결과에는 `[[nodiscard]]`, 보장 가능한 경우에는 `noexcept`를 사용한다.
- 생성 `GameData::Scene`의 세 정적 필드(SceneId/SceneType/IsStartupScene)와 런타임 Scene/UI 객체를 구분한다. 현재 표시용 PlayerArtId/ContentArtId/TitleTextKey/RegionTextKey를 Scene 데이터에 되살리지 않는다. 제안 FCharacterUi/FMonsterUi/FTownUi가 현재 대상·아트/이름·지역 멤버를 가지며 같은 Scene의 랜덤 몬스터/캐릭터 스킨 변경에 함께 갱신하는 설계다. IUI/Panel/Button/Scene UI 루트와 FMainMenuUi 로고는 구현했으며 엔티티 컴포넌트/API 바인딩은 P3 미완료다.
- 정의 데이터의 자산/현지화 키는 해당 도메인에서 공급하되 SceneId/SceneType→현재 엔티티 고정 매핑이나 SceneUiBinding 대체표를 만들지 않는다. 선택적 TextArt는 재사용 metadata로 독립 검증하며 Scene의 필수 의존성이 아니다. 36×36/83×41 카탈로그 크기 검증과 실제 컴포넌트 슬롯 적합성은 구분하고 83×41 로고는 FMainMenuUi 슬롯/멤버 계약으로 둔다. 현재 대상 교체 시 수명이 유효한 참조/스냅샷과 아트·이름/상태를 준비·검증 후 함께 적용하고 실패 시 기존 UI 상태를 보존하는 실제 구현은 P3에서 검증한다.
- 예상 실패는 bool 또는 도메인 결과 코드로 구분한다. 반환값은 모든 실행 경로에서 명확히 하고 출력 인자의 유효 조건을 명시한다.
- 예외는 명시된 경계에서 처리하고 무시하지 않는다. 뷰·참조·포인터가 원본 객체보다 오래 살아남지 않도록 한다.
- 병렬 처리가 실제로 필요할 때만 공유 상태 보호와 락 순서를 정의한다. mutex는 `lock_guard`/`unique_lock`, atomic은 기본 seq_cst를 우선하며 완화된 memory order는 근거와 검증을 남긴다.
- 프로젝트에 경고는 단 1건도 허용하지 않는다. 일반 컴파일 경고뿐 아니라 MSVC 코드 분석(`/analyze`, C6387 등) 경고도 포함한다. 일반 빌드의 경고 0만으로 코드 분석까지 통과했다고 판단하지 않는다.
- 네이티브 변경은 필요한 구성에서 코드 분석을 실제 실행하고 일반 빌드 결과와 구분해 확인한다. 경고 숨김, 규칙 비활성화, 근거 없는 분석기 가정으로 통과시키지 않고 API의 포인터·길이·수명 계약을 수정한다.