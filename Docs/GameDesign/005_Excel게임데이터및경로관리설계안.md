# Excel 게임 데이터 및 파일 경로 관리 설계안

> **사용자 정정 — 모든 데이터 키는 숫자:** [Excel 작성 규칙](../../GameData/Excel/README.md)이 PK/FK의 최신 계약이다. 문자열 PK/FK는 금지하며 현재 원본·생성물·런타임은 uint32를 사용한다. SceneTypeConfig는 ConfigId, Enums는 EnumId를 숫자 PK로 추가하고 분류/타입 이름은 비키 필드로 구분한다. 과거 문자열 ID 보존·nullable 문자열 설명은 이 계약을 대체하지 않는다. 실제 검증과 최종 마감 상태는 로드맵을 따른다.

> **정적 정의·진행 데이터 소유권:** 게임 내 모든 정적 정의/고정 수치/초기값(레벨별 필요 경험치·Monster·ArtId와 경로 매칭 등)은 Excel 원본에서 관리한다. 플레이로 변하는 HP·Stats·인벤토리·업적 등은 단일 루트 `InGameData`와 직렬화/역직렬화 가능한 내부 객체가 소유하며, 화면에 표시돼도 Scene/UI의 게임 데이터가 아니다. Scene/UI는 비소유 조회·표현만 하고 선택·스크롤·포커스 등 UI 조작 상태만 소유한다. 최소 모델·FGameSession 앱 수명 소유·InGameData.xlsx 초기 정의·v1 저장/Continue와 EndFrame 게시의 현재 구현/실행 범위는 [009 §9](009_InGameData및저장복원설계안.md)를 따른다. 전체 콘텐츠/수동 호스트 인증 완료는 아니다.

> **현재 Asset/PCH 계약 — 2026-11-15:** `GameData.dat` TGDP v2는 `Data/Client/<YAML 경로>`와 `Asset/<자산 경로>`의 원시 바이트를 함께 인증 암호화한다. 런타임은 저장소 소유 메모리에서 아트를 읽으며 txt/YAML 재생성·loose Asset 루트 탐색/fallback은 하지 않는다. 원본 디렉터리는 유지하고 파일명은 `Category_Title_Words.ext`로 통일했다. 아트28개 바이트·ID·규격은 그대로다. 아래 과거의 Asset 별도 배포/미구현·v1 컨테이너·GameDataRoot 설명보다 이 계약과 [Packer 사용법](../../Tools/GameDataPacker/README.md)이 우선한다. 최종 수용/검토 상태는 [로드맵](../ROADMAP.md) 상단을 따른다.

> **최신 Scene 계약 변경:** 사용자 승인으로 `Asset/Scene`의 레거시 배치 txt와 `Scene.AssetPath`를 제거했다. 정적 Scene은 `SceneId`·`SceneType`·`IsStartupScene` 세 필드만 허용한다. TextArt.AssetPath는 유지하며 원본 아트 이름에 맞춘 경로만 수정한다. 아래 예전 네 필드/레거시 자산 유지 설명과 검증 기록은 당시 이력이며 현재 계약보다 우선하지 않는다. Asset 동봉의 현재 계약은 상단 TGDP v2 설명을 따른다. 이번 전환의 최종 검증을 완료했다. 현재 결과는 로드맵의 2026-11-15 Scene 레거시 제거 완료 기록을 따른다.

> 최신 데이터 경로: **GameData.dat 런타임 연결을 완료했다.** C++는 타입·Enum만 생성하며 실제 값은 Client YAML→암호화 dat→exe 기준 인증·복호화/메모리 파싱·검증→런타임 저장소로 공급한다. 원본 Excel·아트는 보존했다. 최종 네 구성 경고/오류 0, 각 EXE 18개 시나리오, x64/x86 store 각 521 checkpoint와 마지막 경로 focused 리뷰 PASS를 확인했다. 아래 행 배열/정적 조회 설명은 과거 기록이며 [006](006_YAML내보내기이식설계안.md)과 [로드맵 DATA-02~08](../ROADMAP.md)의 최종 상태가 우선한다.

> 상태: **Scene의 현재 아트·이름·지역 바인딩을 정적 행에 넣던 P1/P2 설계는 사용자 지적으로 폐기하고 런타임 UI 컴포넌트 소유로 정정했다.** 아래가 현재 계약이다. 이전 P1 검증은 거부된 스키마의 역사이며 정정본 검증 증거가 아니다. 정정 원본/생성물·AGENTS.md 게시와 네 구성 솔루션·실제 소비자 기술 검증을 완료했다. 실제 결과와 공식 리뷰 기록 미완료 상태는 [로드맵](../ROADMAP.md)의 정정 실행 기록을 따른다. 기존 공식 부정 판정은 기록 갱신 거부 상태 그대로 유지한다. Scene 소유 UI·Unicode TextArt 런타임 로더·MainMenu 로고는 **P3 일부 구현**이다. InputManager/Layer 통합은 008에, 최소 진행/저장/비소유 상태 조회는 009 §9에 구현·검증했으며 전체 엔티티 콘텐츠 바인딩·수동 호스트 검증은 미완료다.
> 현재 UI 정책은 [007](007_Scene별UI객체및입력분리설계안.md)이 우선한다. 과거의 5개 메뉴·최대 8개·Scroll·하단 패널은 현재 요구가 아니다.

## 1. 목표

- 게임에서 사용하는 밸런스·콘텐츠·표시 문구·자산 참조를 Excel 원본으로 관리한다.
- Scene, 몬스터 등 게임 파일을 가리키는 상대 경로를 코드에 직접 쓰지 않고 Excel 데이터에서 지정한다.
- Excel을 검증된 Client 전용 C++ 데이터로 생성해 런타임에서 타입 안전하게 조회한다.
- 원본 데이터, 생성물, 시각 자산의 책임을 분리하고 데이터 키와 경로 참조를 검증한다.

## 2. 현재 확인된 기반

- 이전 P1에서 TextArt 카탈로그·네 옵션·추출 파일을 추가했다. 그때 함께 넣었던 Scene 표시 바인딩은 실제 원본과 생성물에서 제거하고 검증했다. 독립 TextArt는 선택적 카탈로그로 유지할 수 있지만 Scene이 그 존재를 요구하지 않는다. Monster 전용 workbook 추가는 범위 밖이다.
- `Tools/GameDataGenerator`는 Excel workbook을 검증하고 Client 전용 C++ `.g.h`와 실제 데이터 YAML을 만든다. 스키마 YAML은 출력하지 않는다. JSON은 생성하지 않는다. 기본 출력은 C++ 헤더 `GameData/Class`, Client YAML `GameData/Data/Client`로 분리한다.
- 빌드 전 Generator가 Scene workbook을 읽으며, 템플릿 하위 폴더는 입력 검색에서 제외한다. 런타임은 생성된 C++ 테이블만 조회하고 workbook을 직접 읽지 않는다.
- `FSceneManager`는 Scene ID로 생성 데이터를 조회하고 Excel 상대 경로로 자산을 연다. 기본 Scene ID는 코드에 고정하지 않고 `IsStartupScene` 열에서 찾는다.
- Scene.AssetPath와 레거시 Scene 배치 txt는 폐기했다. TextArt.AssetPath는 strict Unicode 아트 전용으로 유지하며 플레이어 조작 문구는 같은 workbook의 Localization 키로 조회한다.

## 3. 데이터 원본과 산출 흐름

**확정된 방향:** `GameData/Excel/*.xlsx`를 원본으로 유지하고, 빌드 과정에서 Generator가 workbook을 검증해 Client 전용 C++ 데이터와 실제 데이터 YAML을 생성한다. 스키마 YAML 출력 제거와 원본 스키마 검증 유지는 구분한다. 게임 런타임은 workbook을 직접 열거나 읽지 않는다. YAML 패키지의 암복호화 및 로더 계약은 `006_YAML내보내기이식설계안.md`를 따른다. C++ 헤더는 `GameData/Class`, YAML 데이터는 `GameData/Data/Client`에 생성한다.

```text
Excel 원본 workbook
  → Generator 검증
  → Client 전용 C++ 데이터 헤더 생성
  → 런타임이 안정적인 키로 조회
  → 생성 데이터의 상대 자산 경로를 프로젝트 데이터 루트에 결합
```

Generator 기본 입력은 `GameData/Excel`이다. `--output-root` 기본값은 `GameData/Class`이며 그 아래 Common 및 테이블별 폴더에 헤더를 생성한다. `--yaml-output-root` 기본값은 `GameData/Data`이며 그 아래 Client에만 YAML을 생성하며 Schema는 출력하지 않는다. 런타임 `GameDataRoot`는 실행 파일 위치에서 상위 디렉터리를 따라 `GameData/Asset`을 찾는다. 이를 찾지 못하거나 배포 환경에서 다른 위치를 쓰는 경우 `TEXTRPG_GAMEDATA_ROOT` 환경 변수로 루트를 지정할 수 있다. 상대 자산 경로는 이 루트와 결합하며 현재 작업 디렉터리에 의존하지 않는다.

Workbook parser는 입력 루트 아래의 `.xlsx`를 재귀 검색하되 경로 요소가 `Templates`인 디렉터리를 제외한다.

모든 생성 YAML은 `GameDataPacker`가 단일 인증 암호화 패키지로 묶는다. 파일 형식, 키 입력, 제한, C++ 로더 API는 `006_YAML내보내기이식설계안.md`를 기준으로 한다. 런타임 로더는 YAML 원문 엔트리를 제공하며 게임 객체로 변환하는 YAML 파서는 아직 연결되지 않았다.

## 4. Workbook 구조

### 4.1 `GameData/Excel/Scene.xlsx`

현재 정정 계약은 `Scene`, `SceneOption`, `SceneTypeConfig`, `Enums`, `Localization`과 **독립적인 선택적 `TextArt` 카탈로그**다. 기존 TextArt 시트·네 추출 자산은 보존하지만 Scene이 TextArt 존재를 요구하지 않는다. TextArt 없는 유효 Scene workbook도 허용한다. 실제 원본 정정 게시·생성·검증 결과는 로드맵의 정정 P1 실행 기록에 남긴다. `Enums`는 Shared 정의를 유지한다. Monster workbook/새 도메인 스키마 추가는 이번 범위가 아니다.

#### `Scene` 테이블 필드

| 필드 | 템플릿 형식 | 용도와 규칙 |
|---|---|---|
| `SceneId` | `uint32`, Primary key, Required, Min `1` | 배정 후 고정하는 숫자 Scene 키 |
| `SceneType` | `enum<SceneType>`, Required | 해당 종류의 정확히 네 선택지 집합을 지정 |
| `IsStartupScene` | `bool`, Required | 전체에서 정확히 하나만 true |

Scene 데이터 필드는 위 **`SceneId`/`SceneType`/`IsStartupScene` 세 정적 필드만** 둔다. `SceneType` Enum은 `MainMenu=0|Town=1|BattleField=2`, 기반 타입 `uint8`을 유지하고 세 필드의 Client 생성 계약을 보존한다. `PlayerArtId`, `ContentArtId`, `TitleTextKey`, `RegionTextKey`는 잘못 추가했던 Scene 컬럼이므로 제거하며 재도입 시 명확한 metadata 진단으로 거부한다. nullable로 남기거나 `#Reference`만 지우는 정정은 허용하지 않는다.

현재 캐릭터·몬스터·마을/지역의 게임상 대상·HP·Stats 등 진행 원본은 `InGameData`가 소유한다. 제안 `FCharacterUi`/`FMonsterUi`/`FTownUi`의 멤버는 해당 대상의 비소유 바인딩과 재생성 가능한 표시 모델이며, 아트/이름 정의는 정적 저장소에서 조회한다. `FMainMenuUi`는 로고 표시 모델을 소유할 수 있으나 로고 ArtId/경로 정의는 Excel 원본이다. **IUI/Panel/Button/Scene UI 루트·FMainMenuUi 로고와 최소 InGameData/FInGameStatusUi 비소유 상태 조회는 구현했다. FCharacterUi/FMonsterUi/FTownUi의 전체 엔티티 아트·조우/스킨/지역 진행 바인딩은 미구현**이다([009 §9](009_InGameData및저장복원설계안.md), [007 §3](007_Scene별UI객체및입력분리설계안.md)). 같은 BattleField Scene에서 조우/스킨 진행 상태가 바뀌면 UI 표시를 함께 갱신하는 후속 계약을 유지하며 Scene 행/SceneId를 콘텐츠 원본으로 사용하지 않는다.

아트·이름·현지화 정의는 각자 데이터 도메인에 둘 수 있다. 그러나 `SceneId`/`SceneType`에서 현재 엔티티를 고정 선택하거나 별도 `SceneUiBinding` 표로 옮겨 같은 정적 매핑을 되살리지 않는다. SceneType별 옵션 집합 공유는 이 금지와 별개의 유지 계약이다.

#### `TextArt` 테이블 필드

| 필드 | 템플릿 형식 | 용도와 규칙 |
|---|---|---|
| `ArtId` | `uint32`, Primary key, Required, Min `1` | 아트의 고정 숫자 식별자 |
| `AssetPath` | `string`, Required | GameData 기준 상대 txt 경로; Scene에는 자산 경로를 두지 않음 |
| `Width` | `uint32`, Required | 정확한 셀 너비 |
| `Height` | `uint32`, Required | 정확한 행 수 |

카탈로그가 있으면 Scene 유무와 무관하게 네 필드·경로·파일을 독립 검증한다. `#Target=Client`, `ArtId #Scope=Shared`, 나머지 필드는 `#Scope=Client`다. 허용 크기 쌍은 `(36,36)`, `(83,41)`뿐이며 36×41/83×36은 허용하지 않는다. TextArt는 재사용 가능한 자산 metadata이지 Scene 배정표가 아니므로 유효 ArtId/파일 추가·변경에 Scene 행 편집이 필요하지 않다. **83×41은 제안 FMainMenuUi의 로고 슬롯/멤버 계약**이며 카탈로그를 SceneType 역할에 연결해 검증하지 않는다. 실제 표시 시 현재 UI 슬롯의 크기 적합성을 확인하는 것은 P3다. 경로 리터럴 하드코딩·자동 크기 맞춤은 하지 않는다.

**현재 카탈로그 경로와 과거 추출 좌표:** 아래 literal/rect는 최초 추출의 출처 기록이며 rect는 0 기반 `(x,y,width,height)` 셀 좌표다. 현재 사용자 아트를 시안 crop과 다시 일치시키거나 재추출하지 않는다. 이번 변경은 실제 파일명에 맞춘 경로 정정뿐이며 아트 바이트·ID·규격과 `Tests/BattleUiPreview/Main.cpp`는 보존한다.

| ArtId | 원본 literal / crop rect | GameData 기준 AssetPath | 크기 |
|---|---|---|---|
| `1` (기호 `MainLogo`) | `kLayout3` / `(1,1,83,41)` | `Asset/Logo/Logo_Text_Rpg.txt` | 83×41 |
| `2` (기호 `PlayerSwordswoman`) | `kLayout` / `(3,6,36,36)` | `Asset/Character/Character_Swordswoman.txt` | 36×36 |
| `3` (기호 `MonsterDragon`) | `kLayout` / `(46,6,36,36)` | `Asset/Monster/Monster_Dragon.txt` | 36×36 |
| `4` (기호 `TownWindwood`) | `kLayout2` / `(46,6,36,36)` | `Asset/Town/Town_Windwood.txt` | 36×36 |

로고는 원본의 브랜딩을 포함해 충실히 추출한다. 그 브랜딩을 메뉴·버튼·오류 안내 등 UI 조작 문구의 데이터 원본으로 전용하지 않는다. 운영 문구는 모두 Localization의 `ko-KR`/`en-US`에서 관리한다.

**파일 계약:** strict UTF-8, 선두 BOM 선택, LF/CRLF, 마지막 줄바꿈 하나 허용. 후행 공백을 보존하고 정확한 행 수·행별 셀 폭을 검사한다. 자동 패딩·잘라내기·확대는 하지 않는다. 허용 문자는 printable ASCII `U+0020..U+007E`, box drawing `U+2500..U+257F`, block elements `U+2580..U+259F`, arrows `U+2190..U+2193`, triangles `U+25B2/U+25B6/U+25BC/U+25C0`, town clock `○ U+25CB`이며 이 계약에서는 각 1셀이다. 탭·제어 문자·CJK·결합 문자·이모지·그 밖의 문자는 거부한다. 이 제한은 **아트 파일**에 적용하며 한국어 UI 문구 금지가 아니다. 호스트/글꼴의 실제 표현 검증은 P3다.

#### `SceneOption` 테이블 필드

Scene은 UI 루트를 인스턴스별로 소유하지만 선택지 데이터는 `SceneType`별로 공유한다. **비활성을 포함한 전체 행 수가 SceneType마다 정확히 4개**, `OptionOrder` 집합은 정확히 `{1,2,3,4}`다. 활성 항목 수만 세지 않는다. 네 슬롯을 항상 표시하고 숨김·다섯째 항목·선택지 페이지·초과분 스크롤을 허용하지 않는다.

| 필드 | 템플릿 형식 | 용도와 규칙 |
|---|---|---|
| `OptionId` | `uint32`, Primary key, Required | 재정렬해도 보존하는 고유 키 |
| `SceneType` | `enum<SceneType>`, Required | 같은 종류의 Scene이 공유하는 선택지 집합 |
| `OptionOrder` | `uint32`, Required, Min `1`, Max `4` | 같은 SceneType 내 중복 불가, 반드시 1~4 모두 존재 |
| `OptionTextKey` | `uint32`, Required | 숫자 `Localization.TextKey` 필수 참조 |
| `ActionType` | `enum<SceneActionType>`, Required | 허용 행동 종류. 구현되지 않은 행동은 비활성 |
| `TargetSceneId` | `uint32`, Optional, `#Default=0`, Min `0` | 숫자 `Scene.SceneId` 참조. 0은 미지정이며 0이 아닌 값은 비활성이어도 유효해야 한다. 활성 StartGame은 0이 아닌 목적지가 필수 |
| `IsEnabled` | `bool`, Required | false도 보이는 네 행에 포함. 회색 표시하며 선택/확정 불가 |

| SceneType | Order 1 | Order 2 | Order 3 | Order 4 |
|---|---|---|---|---|
| MainMenu | `1 / StartGame` 새 게임(New Game) | `2 / Continue` 불러오기(Load Game) | `3 / Settings` 옵션(Options) | `5 / Quit` 나가기(Exit) |
| Town | `105 / Talk` 대화(Talk) | `103 / Shop` 상점(Shop) | `104 / Rest` 여관(Inn) | `101 / Move` 이동(Move) |
| BattleField | `201 / Attack` 공격(Attack) | `202 / Skill` 스킬(Skill) | `203 / Item` 아이템(Item) | `204 / Defend` 방어(Defend) |

표의 숫자는 OptionId다. MainMenu의 기존 `1,2,3,5`를 유지하고 Quit을 order 4로 이동한다. Credits 선택지 행은 제거하지만 Enum과 기존 현지화 키는 임의 삭제하지 않는다. Town은 Shop103/Rest104/Move101을 보존하고 Talk105를 추가하며 Inventory는 현행 네 슬롯에서 제외한다. 기존 Enum 값 `StartGame=0|Continue=1|Settings=2|Credits=3|Quit=4|Move=5|Inventory=6|Shop=7|Rest=8`은 유지하고 `Talk=9|Attack=10|Skill=11|Item=12|Defend=13`을 뒤에 추가한다.

MainMenu의 StartGame은 `TargetSceneId=Town`을 사용한다. 현재 Continue는 데이터 `IsEnabled=true`와 실제 저장/정의/Scene 후보 검증 성공의 AND로 활성화한다. Settings와 아직 구현하지 않은 Town 행동은 `IsEnabled=false`이며 BattleField 네 행동도 구현 전 모두 false다. 런타임 유효 활성은 데이터 허용과 실제 구현/게임 상태의 AND이며, 미구현을 true로 둔 채 무응답으로 방치하지 않는다. 전부 비활성이어도 네 행은 보이고 선택 OptionId는 없다. 위 순서/문구는 현지화 원본으로 반영하며 다른 미사용 번역 키를 지우지 않는다.

#### `SceneTypeConfig` 테이블 필드

| 필드 | 템플릿 형식 | 현재 계약 |
|---|---|---|
| `ConfigId` | `uint32`, Primary key, Required, Min `1` | 설정 행의 고정 숫자 ID |
| `SceneType` | `enum<SceneType>`, Required | 비키 분류 필드. 모든 정의된 SceneType에 정확히 한 행 |
| `MaxVisibleOptions` | `uint32`, Required, Min `4`, Max `4` | 반드시 4 |
| `OptionOverflowPolicy` | `enum<OptionOverflowPolicy>`, Required | 반드시 `NoScroll` |

기존 Enum `NoScroll=0|Scroll=1`은 호환성을 위해 보존하지만 **현재 데이터에서 Scroll 사용은 거부**한다. NoScroll은 단순 상한 검사가 아니라 정확히 네 행 검사와 함께 적용한다. 이전 시안의 Town `1/3`은 정적 샘플일 뿐이며 운영 UI에는 페이지 상태·페이지 버튼·페이지 입력이 없다.

#### 작성 예: 독립 카탈로그와 현재 표시 대상 변경의 구분

1. `Scene`은 세 정적 필드만 유지한다. 예를 들어 아래 행은 마을의 현재 그림/이름/지역을 지정하지 않는다.

| SceneId | SceneType | IsStartupScene |
|---|---|---|
| Town | Town | false |

2. 기존 `TextArt`를 유지한다면 템플릿 형식의 독립 카탈로그로 관리한다. 예: `TownWindwood | Asset/Town/Town_Windwood.txt | 36 | 36`. 다른 유효 ArtId와 파일을 추가해도 Scene 행을 바꾸지 않는다. 카탈로그를 새로 만드는 것은 Scene의 필수 준비 절차가 아니다.
3. 이름/지역 번역은 `Localization`에 두고 현재 마을 객체가 제공하는 키를 제안 `FTownUi`의 현재 대상 멤버에 바인딩한다. 기존 키를 임의 삭제하거나 중복 정의하지 않는다. 어떤 마을/몬스터를 보여 줄지는 런타임 상태/조우 결과가 결정하며 `SceneId`/`SceneType` 매핑으로 결정하지 않는다. 이 바인딩 동작은 P3 미구현이다.
4. Town SceneOption의 `105,103,104,101` 네 행·order 1~4·미구현 false와 Config `4 / NoScroll`은 유지한다. `SceneOption.TargetSceneId`/`OptionTextKey` 참조 메타데이터는 유지한다.
5. 같은 Scene에서 몬스터 A→B 또는 캐릭터 스킨을 바꿀 때 해당 UI 컴포넌트의 현재 대상·아트·이름/상태를 함께 준비하고 성공 후 교체한다. 로고도 FMainMenuUi의 로고 멤버를 갱신한다. 새 workbook/엔티티 스키마나 정적 Scene 바인딩표를 추가하지 않는다. 폐기한 `Scene.AssetPath`를 다시 만들지 않는다.

다음 명령은 정정본의 실행 절차이지 이 문서가 실행 성공을 보장한다는 뜻이 아니다. 원본 정정 게시 뒤 실제 명령/종료 코드/증거는 로드맵에 별도로 기록한다.

```text
dotnet run --project Tools/GameDataGenerator/GameDataGenerator.csproj -- --self-test
dotnet run --project Tools/GameDataGenerator/GameDataGenerator.csproj -- --validate-only
dotnet run --project Tools/GameDataGenerator/GameDataGenerator.csproj --
dotnet run --project Tools/GameDataGenerator/GameDataGenerator.csproj -- --check
```

**선택적 숫자 참조:** `SceneOption.TargetSceneId`는 uint32 Optional과 명시적 `#Default=0`으로 미지정을 표현한다. 빈 원본 셀은 기본값 0이며 YAML/C++도 숫자 0이다. 값이 있으면 비활성이어도 유효해야 하고 활성 StartGame 목적지는 필수다. 일반 optional/default 규칙은 완화하지 않는다. 제거한 Scene 컬럼에 nullable 예외를 남기지 않는다.

**Enum 단일 원본:****Enum 단일 원본:** `OptionOverflowPolicy`의 정의는 Enums 시트에만 두고 필드 `#Allowed`는 비운다. Config 값 `NoScroll`과 `MaxVisibleOptions=4`는 생성기 도메인 검증으로 강제한다.

메타데이터 제거로 통과시키거나 생성 헤더/YAML을 손으로 수정하지 않는다. 크기 오류/누락 참조는 원본 workbook 또는 txt에서 수정한다. 생성기 성공은 새 화면 표시 성공이 아니다. **현재 FTextScene은 제거했으며 Panel/Button/TextArt UI와 MainMenu 로고를 사용한다. Town/Battle의 현재 엔티티 아트·이름·상태 바인딩은 별도 P3 미완료 범위다.**

과거 검증 한계(레거시 폐기 전의 기록): Scene.AssetPath의 ASCII 내용 검증은 당시 런타임 FTextScene에 남아 있었다. 생성기가 경로를 허용했다고 Unicode 파일을 이 레거시 필드에 넣어도 된다는 뜻은 아니다. 현재 세 ASCII 파일은 보존/검증했고 새 아트는 TextArt 참조만 사용한다. 이 경계의 생성기 보강 또는 레거시 폐기는 P3 이관 때 별도 처리한다.

### 4.2 `GameData/Excel/Monster.xlsx`

Monster 테이블은 밸런스 값과 시각 자산 참조를 같은 몬스터 키로 관리하는 후속 범위다. 이번 첫 구현에서는 `Monster.xlsx`를 만들거나 연결하지 않는다.

| 필드 | 용도 | 제약 |
|---|---|---|
| `MonsterId` | 안정적인 고유 키 | 중복 금지 |
| `NameTextKey`, `DescriptionTextKey` | 이름·설명 현지화 키 | Localization 참조 검증 |
| `MaxHP`, `Attack`, `Defense`, `Speed` | 기본 전투 능력치 | 정수, 설계된 양수/0 허용 범위 검증 |
| `ExpReward`, `GoldReward` | 처치 보상 | 0 이상 |
| `PortraitAssetPath` | 초상/표시 자산 | 선택값, 데이터 루트 기준 상대 경로 |
| `BattleAssetPath` | 전투 표현 자산 | 선택값, 데이터 루트 기준 상대 경로 |

수치의 실제 상한, 속도 순서와 자산 필수 여부는 전투/렌더 설계 시 확정한다. 제안 단계에서 임의의 밸런스 수치를 고정하지 않는다.

### 4.3 다른 게임 데이터

요청 범위를 넘겨 즉시 workbook을 대량 생성하지는 않는다. 이후 직업, 아이템, 맵, 전투 조우, 드롭, 퀘스트, 설정과 선택지 등 게임 규칙·콘텐츠 값도 도메인별 Excel 테이블에서 관리한다. 번역 문구는 기존 다국어 원칙에 따라 `Localization` 원본으로 관리하고, 다른 workbook은 문구 본문 대신 문자열 키를 참조한다.

엔진/알고리즘, 입력 처리, 렌더링 절차와 화면 배치용 비언어 아트 파일은 게임 데이터가 아니다. 이들은 코드 또는 자산에 두되 자산의 위치 참조는 Excel에서 읽는다. 빌드 산출 경로와 개발 도구 설정은 게임 콘텐츠 테이블에 넣지 않는다.

## 5. 경로 규칙

- **데이터 루트는 프로젝트의 `GameData/` 디렉터리다.** TextArt 등 도메인별 Excel의 `AssetPath`에는 이 루트를 제외한 상대 경로만 기록한다. 예: `Asset/Town/Town_Windwood.txt`; 런타임은 이 전체 상대 경로를 `FGameDataStore::FindAsset` 메모리 조회 키로 사용한다. Scene에는 자산 경로가 없다.
- 게임은 실행 파일 옆 `GameData.dat`만 읽는다. 자산을 위해 부모 디렉터리/`TEXTRPG_GAMEDATA_ROOT`를 탐색하거나 txt를 디스크에 풀어 열지 않는다. 원본 `GameData/Asset` 검증은 생성기·패커 단계의 책임이다.
- Excel 경로 필드는 절대 경로가 아닌 프로젝트 데이터 루트 기준 상대 경로로 기록한다.
- 경로 구분자를 정규화하고 `..`를 통한 데이터 루트 이탈, 빈 필수 경로, 존재하지 않는 파일을 Generator에서 오류로 보고한다. 자산 종류별 확장자 강제는 별도 범위이며 폐기한 Scene 배치 파일의 ASCII 내용 검증은 더 이상 요구하지 않는다. 현재 제공한 TextArt는 모두 `.txt`다. 새 TextArt 경로는 GameData 내부의 비어 있지 않은 상대 파일 경로만 허용하며 잘못된 Windows 경로, colon/ADS, reparse/symlink 구성요소를 거부한다. 절대/rooted 경로는 명확히 진단하며 조용히 상대 경로로 고치지 않는다. 생성기 기준 GameData는 parser 입력 루트의 부모를 일관되게 사용하므로 Excel/Sub workbook도 같은 루트를 사용한다.
- 코드와 Scene 간 전환에 구체 자산 경로 리터럴을 두지 않는다. 런타임은 `SceneId`로 레코드를 찾고 현재 아트는 UI 구성 객체의 바인딩과 독립 TextArt 카탈로그로 선택한다.
- 에셋 이동/이름 변경은 Excel 원본 참조를 함께 바꾸고 검증한다.
- Scene 로딩은 SceneId로 런타임 저장소를 조회하며 레거시 배치 파일을 요구하지 않는다.

## 6. Generator 검증과 생성 계약

아래는 새 P1의 **검증 요구**다. 구현/회귀 통과 체크는 [로드맵 P1](../ROADMAP.md)의 부모 최종 결과를 따른다.

1. Templates 하위 폴더를 제외하고 스키마·필수 Client 필드·Enum·중복 키·범위를 검사한다. Scene의 세 정적 필드는 Client 생성에 포함하며 폐기한 AssetPath와 임의 추가 필드는 Scope와 무관하게 거부한다. 제거한 네 표시 컬럼을 재도입하면 명시적으로 거부한다. TextArt가 있으면 네 필드의 Client 포함 계약도 독립 검증하되 Scene 때문에 카탈로그를 요구하지 않는다.
2. 모든 SceneType에 설정 한 행, 비활성을 포함한 SceneOption 정확히 네 행과 order 1~4, `MaxVisibleOptions=4`/`NoScroll`을 검사한다. 0/1/3/5/8개는 실패이며 네 행 전부 비활성은 정상이다.
3. `SceneOption.TargetSceneId → Scene.SceneId`, `SceneOption.OptionTextKey → Localization.TextKey`의 참조 계약은 `#Reference`를 제거해도 우회할 수 없다. TargetSceneId의 제한적 빈 값 예외와 활성 StartGame의 유효 목적지를 검사한다. Scene→TextArt/제목·지역 Localization 참조 및 SceneType별 아트 역할/지역 키 필수 검사는 제거한다.
4. `Scene.AssetPath`는 폐기하고 `TextArt.AssetPath`의 strict Unicode 계약은 유지한다. TextArt가 있으면 파일·루트 경계·인코딩·문자 집합·정확한 허용 크기 쌍을 독립 검증하며 SceneType별 용도는 검사하지 않는다. 중첩 workbook에서도 parser 입력 루트의 부모를 자산 루트로 사용한다.
5. 새 Enum은 기존 값 0~8을 보존하고 뒤에 추가한다. 생성 C++에서 `std` 같은 이름으로 네임스페이스를 가리는 입력은 거부하거나 컴파일 가능한 `::std` 한정을 사용한다(GEN-05).
6. 진단에는 workbook·시트·행/키·열과 파일/예상·실제 크기를 포함한다. 실패 시 기존 생성 출력의 경로·내용을 보존하고 불완전한 산출물을 게시하지 않는다.
7. C++은 GameData/Class, YAML은 GameData/Data의 생성물이다. 새 테이블 때문에 과거 파일/테이블 수를 현재 결과로 재사용하지 않는다. 생성기 self-test/validate-only/check와 실제 소비자 컴파일 결과는 부모가 실행해 기록한다.

## 7. `AGENTS.md` 반영

루트 지침에 다음 규칙을 추가했다.

- 모든 게임 플레이 데이터와 콘텐츠 설정은 `GameData/Excel`의 원본 workbook에서 관리한다. 코드, txt, 임의 JSON/CSV/INI를 게임 데이터의 대체 원본으로 사용하지 않는다.
- Scene, 몬스터, 맵, 아이템 등 게임 파일 참조 경로는 Excel의 데이터 필드로 관리하며 게임 로직·Scene 전환 코드에 구체 경로를 하드코딩하지 않는다. 상대 경로는 정해진 데이터 루트를 기준으로 해석한다.
- 플레이어 표시 문구는 Localization 키를 사용하고, 데이터 테이블에 현지화 문구를 복사하지 않는다.
- Generator에서 스키마·참조·경로를 검증하고 Client 전용 C++ 타입으로 생성한다. C++ 생성물은 `GameData/Class`, YAML 생성물은 `GameData/Data`에 두며 수정 원본은 Excel이다.
- Excel 원본 경로를 찾는 bootstrap 설정, 엔진/도구 설정 및 코드 알고리즘은 게임 콘텐츠 데이터와 구분한다.

## 8. 구현 기록과 전환 상태

### 기존 구현 기록 — 새 UI 검증 증거가 아님

기존 첫 단계에서 Scene.xlsx 다섯 테이블, Templates 제외, 생성 테이블/SceneId 조회, GameDataRoot 탐색·환경 변수 override, 공통 하단 선택 패널을 구현했다. 당시 문서에는 다섯 테이블·생성 파일 여섯 개 및 Debug/Release × Win32/x64 C++ 빌드 통과를 기록했다. 이는 과거 시점의 기록이며 현재 출력 구조/개수나 새 TextArt·네 슬롯 UI 통과 결과로 재사용하지 않는다. 최신 완료된 P0/DATA-01 증거는 [로드맵](../ROADMAP.md)의 해당 원문 기록을 따른다.

### 현재 단계 구분

| 단계 | 범위 | 상태/완료 판정 |
|---|---|---|
| P1 | GEN-01~05 정정, 네 정적 Scene 필드, 독립 선택적 TextArt, 기존 네 옵션/crop/출력 보호 | 이전 결과는 폐기된 스키마의 역사. 정정 원본·생성물 게시와 네 구성 빌드/소비자 기술 검증 완료. 공식 리뷰 기록은 미완료로 보존 |
| P2 | 본 문서와 007의 런타임 컴포넌트 소유·데이터/UX 정책 정정 | 사용자 정정 반영. 정책 승인과 실제 컴포넌트 구현/호스트 통과는 별개 |
| P3 | Scene 소유 UI·TextArt 로더·입력 어댑터/Manager·새 데이터 바인딩 | **일부 구현**. IUI/Panel/Button/루트·팝업 API·입력 어댑터·Unicode 로더·MainMenu 로고 구현. 엔티티 상태/아트/로그 바인딩·완성 InputManager·수동 호스트 검증은 미완료. 최신 결과는 007 및 로드맵 §6 참조 |
| 후속 | Monster workbook·게임 행동·설정 저장·YAML 게임 객체 파싱 | 별도 기획/구현 |

사용자 추가 승인으로 `Scene.AssetPath`와 `Asset/Scene`의 세 배치 txt를 제거했다. 이전 네 필드/ASCII 자산 보존 결정과 당시 검증 수치는 역사로 남기며 현재 세 필드 계약에 적용하지 않는다. TextArt 원본 아트 바이트·ID·규격은 보존한다.

## 9. 후속 검증 사례와 부모 최종 갱신

- 부모는 실제 workbook 시트/필드·OptionId/순서/IsEnabled·ko-KR/en-US 키, 추출 파일 크기·문자·원본 crop 일치를 확인하고 최종 경로/명령/종료 결과를 기록한다. 부모 담당 `Tools/Tests/test_scene_ui_data.py`는 openpyxl이 필요한 데이터 검증 helper이며 실제 실행 결과는 부모가 추가한다. 현재 문서의 목표 파일명은 실행 증거가 아니다.
- 정정 P1은 네 필드 Scene 정상·TextArt 없이 정상·Scene 없이 TextArt 독립 검증·유효 ArtId/파일 변경 시 Scene 수정 불필요·금지 Scene 컬럼별 명시 진단을 확인한다. 기존 0/1/3/5/8행, 중복/누락 order, Scroll, 누락 설정, SceneOption 필수 참조, 존재하는 카탈로그의 파일/UTF-8/크기/문자·경로 이탈/링크, 중첩 Excel 입력과 실패 출력 보존 검사는 유지한다. 네 행 전부 비활성은 유효 데이터다.
- P3는 네 슬롯의 표시·회색 비활성·선택 없음, 안정적 숫자키/방향키, FMainMenuUi 로고 슬롯 83×41, 캐릭터/몬스터/마을 아트 슬롯 36×36, 부모 좌표·클리핑·한국어/영어 긴 문구를 확인한다. 같은 Scene에서 랜덤 몬스터 A→B 및 캐릭터 스킨 변경 시 컴포넌트의 아트·이름/상태 동시 갱신과 실패 보존도 검증한다.
- P3는 입력 전환 관통·반복 확정·포커스/viewport 취소, 작은 화면 차단, 로그 여덟 행·휠/추적·resize clamp, dirty 렌더와 실제 conhost/Consolas 표현을 확인한다. Windows Terminal은 best effort이며 픽셀 동일성 보장이 아니다.
- 새 UI에서 네 슬롯을 숨기거나 선택지 페이지로 바꾸는 것으로 실패를 피하지 않는다. 원본 수정 → 검증/생성 → 소비자 확인 순서를 지킨다.

## 10. 관련 계약

- [Scene 소유 UI·입력·TextArt 정책](007_Scene별UI객체및입력분리설계안.md)
- [현지화 키·한국어/영어 관리](003_다국어문자열및Excel관리원칙.md)
- [YAML·패키지 계약](006_YAML내보내기이식설계안.md)
- [진행 상태·실행 증거](../ROADMAP.md), [프로젝트 지침](../../AGENTS.md)
