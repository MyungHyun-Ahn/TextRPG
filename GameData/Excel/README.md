# Excel 게임 데이터 작성 규칙

> 사용자 정정: **모든 데이터 기본키(PK)와 참조키(FK)는 숫자 정수다. 문자열 키는 금지한다.** 원본·템플릿·생성기·런타임·테스트에 적용했고 실제 실행 검증을 마쳤다. 독립 소스 보완 검토를 포함한 최종 상태는 [로드맵](../../Docs/ROADMAP.md)을 따른다.

## 1. 숫자 키 — 필수

- `item_id`, `skill_id`를 비롯한 모든 테이블의 ID·기본키·참조키는 정수로 정의한다. 현재 프로젝트의 `SceneId`, `OptionId`, `TextKey`, `ArtId`, `CharacterId`, `ProfileId`, `ItemId`, `AchievementId`, `ConfigId`, `EnumId`도 예외가 아니다. 기존 필드 명명은 유지한다.
- 현재 정적 테이블은 `#Type=uint32`를 사용한다. PK는 `#Key=Primary`, `#Scope=Shared`, `#Required=true`, `#Min=1`이며 테이블마다 정확히 하나다. 숫자 셀로 작성하고 텍스트·실수·bool·음수·중복·범위 초과 값은 사용하지 않는다.
- 영문 이름, 번역 문구, 경로, Enum 이름을 PK로 쓰지 않는다. `Token`, `Swordswoman`, `MainMenu`, `MainMenu.NewGame` 같은 문자열을 그대로 ID에 넣거나 해시해 ID를 만들지 않는다.
- 숫자는 한 번 배정한 뒤 고정한다. 행 정렬·삽입·삭제에 맞춰 다시 번호를 매기지 않으며, 삭제한 ID를 다른 의미로 재사용하지 않는다. 이번 이관에서 이미 숫자였던 OptionId와 Level 값은 유지한다.
- `SceneTypeConfig`는 숫자 `ConfigId`를 PK로 갖고 `SceneType`은 분류용 Enum 필드로 둔다. SceneType마다 설정은 정확히 한 행이다. `Enums`도 숫자 `EnumId`가 PK이며 `EnumName`은 타입 정의 이름이지 PK가 아니다.

## 2. 참조키와 미지정값

- FK는 대상 테이블 PK와 같은 정수 타입을 사용하고 `#Reference=테이블명.기본키필드`를 지정한다. 예: `ItemDefinition.NameTextKey → Localization.TextKey`, `CharacterDefinition.ArtId → TextArt.ArtId`.
- 필수 FK는 양수이며 존재하는 대상 행을 가리켜야 한다. 표시 비활성 상태라도 잘못된 참조를 허용하지 않는다. `#Reference`를 삭제해 도메인 필수 참조 검증을 우회하지 않는다.
- 선택적 숫자 FK의 미지정값은 명시된 계약에서만 `0`으로 표현한다. 현재 `SceneOption.TargetSceneId`는 `uint32`, Optional, `#Default=0`, `#Min=0`이다. 빈 셀은 기본값 0으로 생성하며 YAML/C++에서도 숫자 0이다. 활성 StartGame은 유효한 0이 아닌 목적지가 필수다.
- PK로 0을 쓰거나 선택적 FK를 가짜 양수 ID로 채우지 않는다. 빈 문자열·`""`·문자열 `"0"`·YAML null로 숫자 FK를 대신하지 않는다. 다른 Optional 필드는 기존 명시적 기본값 규칙을 따른다.

## 3. 사람이 읽는 이름과 실제 ID를 구분한다

- 이름·설명·한국어/영어 문구·파일 경로는 계속 문자열이다. 이 규칙은 문자열 데이터 자체를 금지하는 것이 아니다.
- 현지화 `Localization.TextKey`도 숫자다. 다른 테이블의 `NameTextKey`, `OptionTextKey` 등은 그 숫자를 참조하고, 실제 문구는 `KoKR`/`EnUS`에 둔다.
- 코드에서 숫자 ID를 임의 하드코딩하지 않는다. Excel `Enums`에 정의한 `SceneId`, `TextKey`, `ArtId`, `CharacterId`, `ProfileId`, `ItemId`, `AchievementId`의 숫자 상수와 생성 C++ Enum 이름을 사용한다. 해당 값은 실제 데이터 행의 숫자 ID와 일치해야 한다. Enum의 기호 이름·값 정의 문법은 메타데이터이며 문자열 데이터 키가 아니다.
- YAML 필드명·타입명·테이블 이름·파일 경로와 직렬화 메타데이터 이름은 식별 구조다. 이들을 숫자로 바꾸거나 표시 문구와 혼동하지 않는다. 데이터 PK/FK 값은 생성물과 런타임에서도 정수로 유지한다.

## 4. Workbook 형식과 생성

- 1행 `#Table`, 2행 `#Target`, 4~12행 메타데이터, 13행 `#Field`, 14행부터 실제 데이터다. 템플릿은 [GameDataTableTemplate.xlsx](Templates/GameDataTableTemplate.xlsx)를 사용한다.
- `#Type`, `#Key`, `#Scope`, `#Required`, `#Min/#Max`, `#Default`, `#Reference`를 원본에서 수정한다. 생성 `.g.h`/`.g.cpp`/YAML을 손으로 수정하지 않는다.
- Enum의 이름·기반 타입·명시 숫자는 `Enums` 시트가 단일 원본이다. 필드의 `#Allowed`로 Enum 값을 중복 정의하지 않는다. 정적 Scene은 기존 세 필드(SceneId/SceneType/IsStartupScene)만 유지한다.
- 모든 고정 게임 수치·콘텐츠 초기값은 Excel 원본이다. 진행 상태는 Session 소유 InGameData에 있고 UI/Scene이 원본을 복제하지 않는다.
- workbook을 저장한 뒤 [Generator의 실제 검증·생성 명령](../../Tools/GameDataGenerator/README.md)을 실행한다. 생성기 성공과 실제 C++ 소비자·앱·저장 호환 검증을 구분한다.
- 열린 Excel을 강제로 닫거나 저장하지 않은 원본을 덮어쓰지 않는다. 입력 폴더 안에 백업 workbook을 두지 않고 `~$` 잠금 파일을 데이터로 취급하지 않는다.

## 5. 기존 데이터·저장 호환

- 문자열 ID에서 숫자 ID로 바꿀 때 모든 PK/FK·초기 정의·생성 타입·실제 YAML·런타임 조회·테스트를 같은 고정 매핑으로 바꾼다. 일부 테이블만 숫자로 바꾸거나 문자열 조회를 운영 경로에 남기지 않는다.
- 기존 인증 세이브와 전용 키는 덮어쓰거나 삭제하지 않는다. 과거 문자열 ID를 해석하는 코드는 격리된 읽기 전용 마이그레이션 경계로 한정하고 새 저장은 숫자 ID를 쓴다. 모르는 과거 ID는 추정하지 않고 안전하게 거부한다.
- 규칙 위반은 workbook·시트·행·열을 포함한 진단으로 거부한다. 실패 시 원본과 기존 생성 출력·활성 상태를 보존한다. 실제 데이터 셀 타입과 참조 일치도 검증한다.
