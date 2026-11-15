# 콘텐츠 기획

루트 [AGENTS.md](../AGENTS.md)를 먼저 읽는다.

- 직업·맵 이동·전투·아이템 등 요청한 콘텐츠 범위의 상태, 수치, 실패·경계 규칙을 정의한다.
- 수치의 단위, 반올림, 상한/하한을 필요한 항목에 명시한다.
- 입력 → 기대 결과 예시를 작성하여 구현과 검증이 같은 기준을 사용하게 한다.
- 현재 소스와 데이터 형식을 확인하고 모호한 제품 판단은 사용자에게 확인한다. 합의되지 않은 기능을 미리 구현하지 않는다.
- 결과에는 규칙, 변경 대상, 확인할 사례, 미결정 사항을 남긴다.
- 모든 게임 정적 정의/고정 수치/초기값은 Excel 원본이다. 레벨별 필요 경험치·Monster·ArtId와 경로 매칭 등도 코드/별도 설정에 하드코딩하지 않는다. HP·Stats·인벤토리·업적 등 진행 상태는 Scene 밖의 `InGameData` 단일 루트와 그 내부 직렬화/역직렬화 가능한 진행 객체가 소유한다. Scene/UI는 진행 상태를 소유/복제하지 않고 비소유 조회·표현만 한다. 선택·스크롤·포커스는 UI 조작 상태로 구분한다([009](../Docs/GameDesign/009_InGameData및저장복원설계안.md)).
- Scene 정의는 `SceneId`/`SceneType`/`IsStartupScene` 세 정적 필드만 둔다. 폐기한 Scene.AssetPath와 PlayerArtId/ContentArtId/TitleTextKey/RegionTextKey를 재도입하지 않는다. 현재 아트·이름·지역·진행 수치를 Scene 행/SceneUiBinding 배정표로 옮기지 않는다.
- 제안 `FCharacterUi`/`FMonsterUi`/`FTownUi`의 현재 대상 멤버는 InGameData의 캐릭터/스킨·조우·마을/지역에 대한 비소유 바인딩/표시 모델이다. HP·Stats 원본을 UI 멤버에 두지 않는다. 같은 Scene에서 진행 대상이 바뀌면 정의·진행 조회로 표시를 준비하고 성공 시 안전하게 바꾸며 실패 시 이전 진행과 표시를 보존한다. 현재 최소 InGameData/FGameSession·FInGameStatusUi의 비소유 조회·EndFrame 앱/UI 루트 일관성은 구현·검증했고 위 전체 엔티티/전투/장비·업적 전용 UI/게임플레이와 수동 호스트 인증은 미완료다([007 §3](../Docs/GameDesign/007_Scene별UI객체및입력분리설계안.md), [009 §9](../Docs/GameDesign/009_InGameData및저장복원설계안.md)).
- 저장은 전체 InGameData 직렬화→인증 암호화→안전한 파일 교체, Continue는 인증/복호화→전체 후보 역직렬화·검증→재개 Scene 준비→EndFrame 진행/바인딩 결합 게시다. 현재 최소 필드·v1 포맷·같은 PC/Windows 사용자 DPAPI 전용 키·단일 Default 슬롯·게시 후 NewGame autosave/정상 Quit 저장은 009 §9에 확정·구현했다. Continue는 read-only/no rekey/no NewGame 기본값 대체이며 수락과 완료를 구분한다. 실패 시 정상 세이브·현재 메뉴/진행을 보존하되 이미 정리된 old Scene을 destructor Quit 뒤 복원한다고 주장하지 않는다. 다른 PC 이동·키 복구/회전·마이그레이션·전투 재개는 미보장/후속이며 문서 완료를 기능 완료로 대신하지 않는다.
- 자산·이름·현지화 정의는 각 도메인에 둘 수 있으나 SceneId/SceneType→현재 엔티티 매핑·SceneUiBinding 대체표나 요청하지 않은 새 엔티티/workbook 스키마를 만들지 않는다. TextArt는 선택적 재사용 metadata로 존재 시 36×36/83×41을 독립 검증하고 Scene 자체는 카탈로그를 요구하지 않는다. 83×41은 MainMenu UI 로고 슬롯/멤버 계약이지 카탈로그의 Scene 배정이 아니다.
- SceneOption의 현지화 레이블/TargetSceneId 참조·활성 StartGame 목적지와 비활성 포함 네 옵션/order 1~4/4·NoScroll은 유지한다. optional 문자열 참조의 값/기본값 동시 빈 값 예외는 TargetSceneId만 허용한다. 정책 정정 승인과 실제 데이터/런타임 검증 완료를 구분하고 이전 거부된 스키마의 P1 결과를 정정본 통과 증거로 쓰지 않는다.

- 플레이어에게 보이는 문구는 `GameData/Excel`의 한국어·영어 문자열 데이터로 정의한다. 기획·코드에서 문구를 직접 박아 넣지 않고 안정적인 문자열 키를 지정한다. 폐기된 Scene txt/Scene.AssetPath는 재도입하지 않으며 TextArt는 [007](../Docs/GameDesign/007_Scene별UI객체및입력분리설계안.md)의 strict UTF-8·허용 단일 셀 문자·36×36/83×41 계약으로 관리한다. 요청된 로고의 원본 브랜딩은 보존하지만 조작 문구는 현지화 키로 관리한다.
