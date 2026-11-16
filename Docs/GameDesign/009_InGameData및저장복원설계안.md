# InGameData 소유권·직렬화·암호화 저장/복원 설계

> 현재 생성 모델 — 2026-11-16: [012 InGameData YAML 정의·자동 생성](012_InGameDataYaml정의및자동생성설계안.md)의 **생성 단일 가변 루트 직접 사용·shared_ptr/재귀 컨테이너·Custom hook/factory·이름 기반 CGSA 저장을 구현·실행 검증했다.** 기존 IGD1 v1은 읽기 전용으로 이관하며, 아래 인증 저장·후보 검증·EndFrame 게시·종료 정책은 유지한다. 현재 실행/검토는 §9.6과 012 §11, 이전 후보 기록은 §9.5로 구분한다. 로컬만 반영했고 커밋·푸시·publication은 없다.

> 상태 — 2026-11-15: **최소 진행 모델·앱 수명 소유·인증 저장/Continue·EndFrame 결합 게시를 구현했고, 실제 실행 게이트와 독립 focused source review를 확인했다.** 현재 구현·검증 범위는 §9가 기준이다. UI-08/09의 전투·장비·몬스터 콘텐츠·업적 게임플레이/전용 UI 및 전체 호스트 시각 인증을 완료한 것은 아니다. 초기 문서 전용 작업의 이력은 §8에 보존한다.
> 우선순위: 진행 데이터 소유권은 이 문서가 [005](005_Excel게임데이터및경로관리설계안.md)·[007](007_Scene별UI객체및입력분리설계안.md)의 과거 표시 상태 설명보다 우선한다. 입력 상태와 Scene Layer 책임은 [008](008_InputManager및SceneUILayer설계안.md)을 유지한다.

## 1. 확정한 경계

| 구분 | 단일 원본과 소유자 | 예시 | 저장 대상 |
|---|---|---|---|
| 게임 내 모든 정적 정의 | `GameData/Excel` 원본 → 생성·검증 → GameData.dat → FGameDataStore 조회 | 레벨별 필요 경험치, 기본/성장 능력치, Monster 정의, 아이템·업적 정의, 이름 키, ArtId와 경로 매칭 | 세이브에 정의 전체를 복사하지 않음 |
| 게임을 진행·재개하는 데 필요한 상태 | **InGameData 단일 루트 클래스와 그 소유 하위 데이터 객체** | 캐릭터 레벨·누적 경험치·현재 HP·획득/할당 스탯·장비, 인벤토리, 업적 진행/달성, 게임상 현재 위치·조우 상태 | 모두 직렬화·역직렬화 가능해야 함 |
| UI 조작·표현 상태 | 해당 Scene/UI | 포커스, 선택된 UI OptionId, hover/눌림, 입력 캡처, 로그 스크롤, 표시용 문자열·HP바 셀 | InGameData 및 세이브에서 제외 |
| 입력 기기 상태 | 기존 FInputManager | Pressed/Held/Released·입력 전이 | 제외 |

**화면에 표시된다는 이유로 HP·Stats가 UI 상태가 되지 않는다.** HP·Stats·소지품·업적 등 실제 게임 상태는 InGameData에 속한다. Scene/UI는 이를 소유하거나 별도의 변경 가능한 원본으로 복제하지 않고 조회·표현한다. 선택·스크롤·포커스 같은 UI 조작 상태와 게임 진행 상태를 구분한다.

- 게임 콘텐츠의 고정 수치·목록·참조·초기값은 예외 없이 Excel 원본으로 관리한다. 코드·별도 JSON/CSV/INI를 두 번째 콘텐츠 원본으로 만들지 않는다. 새 게임의 시작 수치도 해당 Excel 정의에서 생성한다.
- 텍스트 아트 원시 바이트는 기존 Asset 계약으로 유지하고 ArtId↔상대 경로·규격 등 정의는 Excel에서 관리한다. 그림 바이트 자체를 workbook 셀이나 세이브에 복제하지 않는다.
- 엔진 알고리즘·자료형·저장 프로토콜 버전/기술적 제한과 게임 콘텐츠 수치를 구분한다. 이 계약은 게임 규칙·수치의 별도 하드코딩을 허용하지 않는다.
- 기본/성장 스탯은 Excel 정의, 플레이로 획득한 보정·할당·현재 HP 등은 진행 상태다. 최종 스탯처럼 계산 가능한 값은 정의와 진행 상태에서 계산한다. 어떤 값을 영속화하고 어떤 값을 재계산할지는 해당 도메인 구현 시 명시하며 UI에 독립적인 능력치 원본을 두지 않는다.
- 표의 도메인은 전체 요구 범위와 예시다. 현재 구현한 최소 값 모델·정의는 §9.1로 한정하며 장비·조우 등 모든 예시가 구현됐다는 뜻은 아니다.

## 2. InGameData는 단일 소유 루트다

```text
앱/게임 실행 수명 소유자
├─ FGameDataStore                 정적 정의·자산 조회
├─ FGameSession → InGameData      현재 게임 진행의 유일한 루트 (unique_ptr)
│  ├─ 캐릭터 진행 데이터          각 하위 데이터도 직렬화/역직렬화 가능
│  ├─ 인벤토리·장비 데이터
│  ├─ 업적 진행 데이터
│  └─ 재개에 필요한 기타 게임 상태
├─ FSaveDataStore                인증 암호화·파일 I/O (진행 루트와 분리)
└─ FSceneManager
   └─ 현재 Scene → 소유 UI        진행 데이터에는 비소유로 접근
```

- `InGameData`와 `FGameSession`은 현재 C++ 타입이다. 위 하위 항목은 전체 도메인의 개념 경계이며 구현된 값 타입/API는 §9.1을 따른다. 장비·기타 게임 상태까지 이미 존재한다는 뜻은 아니다.
- 활성 게임 진행의 루트는 하나다. 값 멤버 또는 단독 소유 하위 객체로 구성할 수 있으며 모든 필드를 한 거대 클래스에 평탄하게 넣으라는 뜻은 아니다. 검증 중인 불러오기 후보는 활성 루트가 아니며 성공 전까지 게시하지 않는다.
- Scene/FUIManager/개별 UI가 InGameData를 생성·단독 소유·삭제하거나 Scene 전환 때 초기화하지 않는다. 앱/게임 실행 수명의 소유자가 새 게임·불러오기·종료에 맞춰 활성 루트를 관리한다. 전역 가변 singleton을 요구하지 않는다.
- 게임 로직이 InGameData의 변경 경계를 통해 상태를 갱신한다. UI 입력은 행동 요청으로 전달하고, HP 변경·재화 차감·아이템 추가를 UI 내부 변수에 직접 반영하지 않는다.
- InGameData에는 UI 객체·포커스·스크롤·입력 상태·Win32 핸들·renderer·저장 서비스·정적 저장소를 소유 멤버로 넣지 않는다. 진행 상태를 정의 ID로 연결하고 서비스는 외부에서 사용한다.
- Scene이 파괴돼도 진행 상태는 유지된다. 새 게임은 검증된 Excel 초기 정의로 새 루트를 준비하며, 불러오기는 저장된 상태로 새 후보 루트를 복원한다. 현재 초기화 규칙은 §9.1의 `NewGameProfile`과 `LevelDefinition`을 따른다.

## 3. 직렬화·역직렬화 계약

1. **루트에서 소유하는 모든 내부 진행 데이터 객체**는 자신에게 필요한 데이터를 누락 없이 직렬화·역직렬화할 수 있어야 한다. 루트만 저장하고 캐릭터/인벤토리/업적 내부 값이 빠지는 구현은 불합격이다. 루트의 round-trip과 각 하위 데이터의 round-trip을 확인한다.
2. 직렬화는 객체 메모리 덤프가 아니다. 포인터 주소·vtable·컨테이너 내부 메모리·정적 테이블 행 주소를 저장하지 않는다. 정의 참조에는 안정적인 콘텐츠 ID, 런타임 개체 간 참조에는 필요한 인스턴스 ID를 사용하고 복원 시 유효성과 중복을 검증한다.
3. UI의 표시용 문자열·HP바·아트 디코드 캐시 등 재생성 가능한 표현 객체를 InGameData에 포함하지 않는다. 모든 소유 진행 객체의 직렬화 요구를 피해 가기 위한 숨은 비직렬화 상태를 만들지 않는다.
4. 필드 타입·길이·개수·범위·중복·관계·필수값 및 정적 정의 참조를 검증한 후보만 수용한다. 합계 크기·컬렉션 개수·문자열 길이 등을 제한하고 할당/산술 오버플로를 거부한다. 파일의 임의 타입명을 따라 객체를 생성하지 않는다.
5. 저장 형식 버전과 콘텐츠 호환성 식별을 구분한다. 지원하지 않는 버전·삭제된 필수 정의·복원할 수 없는 관계는 명확히 실패하며 임의 기본값으로 정상 저장처럼 복구하지 않는다. 마이그레이션은 지원하는 버전 조합에 대해 명시적으로 구현·검증한다.
6. 저장은 게임 행동 처리의 안전 지점에서 일관된 상태를 읽는다. 역직렬화는 활성 루트를 직접 덮어쓰지 않는다. 성공 시 논리적으로 동등한 진행 상태를 복원하되 객체 주소나 UI 상태의 일치는 요구하지 않는다.
7. 현재 포맷은 §9.2의 bounded CGSA binary archive이며 `Serialize`/`Deserialize`를 구현했다. 생성 루트/하위 타입은 public ISerializable을 상속하고 생성 필드 codec을 사용한다. 기존 UI·정적 Excel 타입까지 상속을 강제하지 않는다.

## 4. 암호화 저장 경계

```text
일관된 InGameData
  → 전체 하위 진행 상태 직렬화
  → 저장 버전·호환성 정보와 함께 인증 암호화
  → 암호문을 소유 임시 파일에 기록·완료 확인
  → 성공한 파일만 기존 세이브와 안전하게 교체
```

- 평문을 숨기는 암호화뿐 아니라 변조를 검출하는 인증 암호화를 사용한다. 현재 Windows CNG AES-256-GCM과 세이브 전용 랜덤 키를 구현했다(§9.2~3). 키·nonce·인증 tag·AAD의 요구를 생략하는 별도 자체 암호를 만들지 않는다.
- 형식/버전·호환성 등 중요한 메타데이터도 인증 범위에 포함한다. 인증 전에는 파일 크기·고정 헤더 등 최소한의 제한된 framing만 확인하고, 진행 객체를 역직렬화하거나 활성 상태에 적용하지 않는다.
- 평문 직렬화 결과는 메모리에서 처리한다. 평문 세이브·디버그 덤프를 디스크에 남기지 않는다. 인증 실패 시 복호화 출력은 소비하지 않으며 불필요해진 민감 버퍼와 키의 수명을 제한한다.
- 저장용 키의 보안 용도는 콘텐츠 패키지 키와 구분한다. GameData.dat 키/개발 키를 재사용하지 않으며 평문 키를 Excel·세이브·로그에 넣지 않는다. 전용 `SaveData.key`에는 현재 Windows 사용자 DPAPI로 보호한 blob만 보관한다(§9.3).
- 쓰기·flush·close·교체 실패 시 기존 정상 세이브를 보존하고 실패를 보고한다. 정리 대상은 해당 시도에서 독점 생성한 임시 파일뿐이다. 강제 종료/전원 중단의 복구 보장은 구현·파일시스템 검증 없이 주장하지 않는다.
- 목표는 로컬 세이브의 기밀성·무결성 보호다. 클라이언트가 복호화 키와 게임 상태를 사용하는 오프라인 구조를 완전한 치팅 방지로 설명하지 않는다. 정상적인 과거 세이브로 되돌리는 rollback 방지도 별도 계약 없이는 보장하지 않는다.
- v1은 **같은 PC·같은 Windows 사용자** 범위로 확정했다. 다른 PC/사용자로의 이동·키 복구/회전은 지원 보장이 없으며 콘텐츠 개발 키 정책을 자동 적용하지 않는다.

## 5. 메인메뉴 불러오기와 안전한 게시

```text
MainMenu의 기존 Continue 선택
  → 세이브 읽기·framing/크기 검사
  → 인증 검증·복호화
  → 후보 InGameData와 모든 내부 객체 역직렬화
  → 현재 FGameDataStore 정의·저장 버전·진행 상태 관계 검증
  → 후보 데이터로 재개 Scene/UI 준비
  → Render/Present 뒤 EndFrame에서 데이터와 Scene 바인딩 함께 교체
  → 이전 Scene/참조를 정리한 뒤 이전 진행 객체 해제
```

- 준비·검증 실패 시 메인메뉴 또는 기존 활성 게임 상태를 보존한다. 반쯤 복원된 InGameData를 노출하거나 먼저 Scene만 바꾸지 않는다.
- 교체 시 이전 UI의 캡처·입력 콜백·비소유 진행 참조를 안전하게 해제/재바인딩한다. 예전 객체의 포인터를 남겨 다음 프레임에 사용하지 않는다. 기존 입력 배치 폐기·release gate 정책은 유지한다.
- 새 게임과 불러오기 모두 검증된 후보 루트/Scene을 준비한 뒤 게시한다. 불러오기에서는 새 게임 기본값으로 진행 상태를 덮어쓰지 않는다.
- 재개에 필요한 논리 위치·상태를 저장하며 UI의 포커스·스크롤이나 Scene 객체 자체를 역직렬화하지 않는다. 전투 중 저장/정확한 재개 범위는 별도 결정한다.
- MainMenu의 New/Continue/Settings/Exit 네 슬롯·기존 OptionId를 유지한다. 현재 Continue는 저장/정의/Scene 후보 검증이 가능한 경우에만 활성화하며 실제 재시작 복원·실패 보존 검증은 §9.5에 기록한다. 손상·누락을 새 게임 기본값으로 대체하지 않는다.

## 6. UI 바인딩의 해석

- 캐릭터/몬스터 UI의 현재 대상은 비소유 ID/유효 수명 뷰 또는 표시용 스냅샷이다. `HP=10`처럼 게임에 영향을 주는 독립 상태를 UI가 보관하지 않는다. 표시용 스냅샷은 원본이 아니며 거꾸로 진행 데이터로 게시하지 않는다.
- 게임상 현재 조우/대상은 진행 데이터에서 결정한다. UI의 선택 강조·마우스 hover 등 단순 표시 선택과 구분한다. 이름·ArtId·경로는 진행 상태의 정의 ID 및 FGameDataStore를 통해 조회한다.
- HP·이름·아트 등을 한 표시 갱신 단위로 준비해 보여 줄 수 있지만 그 갱신은 게임 상태 소유권의 이전이 아니다. 다른 Scene이나 UI 재생성 뒤에도 같은 진행 원본을 읽는다.
- 정적 Scene 필드는 기존 `SceneId`/`SceneType`/`IsStartupScene` 세 개다. 현재 캐릭터/몬스터/스탯을 Scene 행이나 SceneUiBinding 배정표로 옮기지 않는다.
- UI-08/09는 이 소유권 계약에 맞춰 구현한다. 기존 입력·Layer 소유권을 InGameData로 이관하지 않는다.

## 7. 구현 작업 ID·수용 기준

| ID | 후속 작업 | 완료 조건 |
|---|---|---|
| IGD-01 | InGameData 루트·하위 진행 모델과 수명 경계 | 앱/게임 수명 소유, Scene 전환/재생성 후 동일 진행 유지, 새 게임 초기 정의는 Excel에서 공급 |
| IGD-02 | 전체 객체 직렬화·역직렬화 | 루트/하위 객체 round-trip, 빈/다수 객체·경계값·관계·정의 참조 검사, 오류 시 활성 루트 불변 |
| IGD-03 | 인증 암호화와 안전한 저장 I/O | 저장/복호화 성공, 잘못된 키·변조·잘림 거부, 쓰기/교체 실패 시 이전 세이브 보존, 평문 파일 없음 |
| IGD-04 | Continue의 전체 복원 경로 | 같은 실행의 새 객체 및 프로그램 재시작 뒤 진행 복원, 실패 시 MainMenu/기존 상태 보존, 안전한 데이터/Scene 교체 |
| IGD-05 | UI-08/09의 비소유 진행 바인딩 | HP·Stats·소지품·업적 원본이 UI에 없음, 상태 변경 표시 및 진행 교체 뒤 오래된 참조 없음 |
| IGD-06 | 기존 증분 테스트 경로에 최소 회귀 통합 | 원본 직접 참조·기존 PCH/obj 재사용, 관련 RED→GREEN과 최종 필요한 회귀, 정상/실패/경계 결과 구분 |

IGD-01~06의 **현재 최소 모델 범위 구현·실행 확인**은 §9와 [로드맵](../ROADMAP.md)에 연결한다. 표의 ID와 원래 정책 계약은 유지한다. 구체 값 모델·CGSA writer/IGD1 reader·TRSV 컨테이너·기본 슬롯·DPAPI 키 보관·NewGame 게시 후 autosave·정상 Quit 저장은 더 이상 미정이 아니다. 명시된 IGD1 이관과 이름 기반 추가/삭제 호환 외의 임의 버전 마이그레이션, 다른 PC/사용자 이동·키 복구/회전, 전투 중 저장/정확한 재개 및 확장 콘텐츠는 남은 범위다. 구현하지 않은 내용을 기본값·새 Manager로 채웠다고 해석하지 않는다.

## 8. 초기 문서 전용 작업의 검증 이력

- 005/007과 로드맵·기획 지침에 진행 원본과 UI 표시 바인딩의 경계를 동기화한다.
- 문서 내부 링크·후속 ID·기존 완료 이력 보존을 확인한다.
- 당시에는 C++·원본 Excel/아트·생성물·프로젝트·runner를 수정하지 않았고 문서만으로 기능 완료를 선언하지 않았다. 이는 이후 승인된 구현을 금지하거나 현재 런타임이 미구현이라는 뜻이 아니다. 이번 최종 동기화도 문서만 수정하며 이미 동결된 구현의 빌드·런타임 회귀를 반복하지 않는다.

## 9. 현재 구현·실행 확인 범위 — 2026-11-16

### 9.1 실제 소유자·최소 값 모델과 Excel 정의

- [FGameSession](../../TextRPG/InGameData/FGameSession.h)이 앱 수명에서 `std::unique_ptr<InGameData>`를 소유한다. Main의 생성/역순 파괴는 Scene/UI가 먼저 정리되고 진행 루트가 나중에 해제되게 한다. 정적 Store와 저장 서비스는 루트의 소유 멤버가 아니다.
- [InGameData](../../TextRPG/InGameData/InGameData.h)는 private 생성 `GameData::InGameData::SRootInGameData m_data` **하나를 실제 가변 원본**으로 소유한다. 아래 하위 공개 타입명은 생성 타입 alias이며 별도 수작업 필드/저장 DTO를 중복 보관하지 않는다. UI는 const 참조와 파생 표시만 조회하며 내부 가변 shared_ptr를 노출하지 않는다. 변경은 기존 모델 검증 API가 같은 생성 객체에 직접 반영한다.

| 값 타입/루트 | 실제 필드 |
|---|---|
| `SCharacterProgress` | `CharacterId`, `Level`, `Experience`, `CurrentHp`, `StatBonuses` |
| `SCharacterStats` (보정 및 파생 결과의 값 타입) | `MaxHp`, `Attack`, `Defense` |
| 생성 `SRootInGameData` | `m_contentVersion`, `m_sceneId`, `m_character` (`shared_ptr<SCharacterProgress>`), `m_gold`, `m_inventory`, `m_achievements` |
| `SInventoryItemProgress` | `InstanceId`, `ItemId`, `Count` |
| `SAchievementProgress` | `AchievementId`, `Progress`, `IsCompleted` |

- [InGameData.xlsx](../../GameData/Excel/InGameData.xlsx)의 `CharacterDefinition`(characterId/nameTextKey/artId), `LevelDefinition`(level/requiredExperience/maxHp/attack/defense), `ItemDefinition`(itemId/nameTextKey/maxStack), `AchievementDefinition`(achievementId/nameTextKey/targetCount), `NewGameProfile`(profileId/characterId/initialLevel/initialExperience/initialGold/initialMaxHpBonus/initialAttackBonus/initialDefenseBonus/contentVersion/isDefault) 다섯 정의가 실제 존재한다. **최소 초기값이며 최종 밸런스가 아니다.** 다섯 cohort 전체 부재는 레거시 입력, 일부만 존재하면 오류다.
- `CreateNew`는 기본 profile과 정의에서 초기값을 만들고 `GetStats`는 Level 정의와 저장된 보정을 합쳐 최종 Stats를 재계산한다. 파생 Stats·HP바·이름 문자열/아트 캐시는 저장하지 않는다. 경험치·HP·컬렉션 중복/범위·정의 참조·contentVersion을 검증하며 UI는 별도 HP/Stats 원본을 만들지 않는다.
- [FInGameStatusUi](../../TextRPG/UI/InGameData/FInGameStatusUi.cpp)는 캐릭터/HP·Stats·Gold·인벤토리/업적 개수를 비소유 조회한다. 전용 장비·소지품 조작·업적 달성 게임플레이나 FCharacterUi/FMonsterUi/FTownUi 전체 콘텐츠 바인딩 완료가 아니다.

### 9.2 codec과 인증 컨테이너를 구분한다

- 현재 writer는 **CGSA wire 1 / 제품 schema_version 1**의 이름·타입 signature·길이 기반 archive다. wrapper는 `ForwardSerializeFields`/`ForwardDeserializeFields`로 생성 루트에 위임하며 필드를 수작업으로 재열거하지 않는다. Core는 shared null/반복 참조/동일 값의 별도 인스턴스와 vector/map/unordered_map의 재귀 조합을 구분한다. `TextRPG/CustomInGameData/FCharacterInGameData`의 hook/factory를 캐릭터 스키마명에 연결한다. 포인터·객체 메모리 덤프·임의 타입 로딩은 없다.
- **기존 IGD1 format 1은 읽기 전용**이다. 기존 콘텐츠 버전·SceneId·캐릭터/Gold/컬렉션의 명시적 little-endian/UTF-16 순서를 보존해 생성 후보로 복원한다. Continue는 read-only이며 이후 정상 Quit 등 저장 시 CGSA로 쓴다. 구형 입력→신형 출력은 값/신형 framing으로, 신형 round-trip은 정확 byte 동등성으로 검증하며 서로 다른 포맷의 byte equality를 요구하지 않는다.
- 모델 전체 최대 **1 MiB**, ID 최대 **256 UTF-16 code unit**, 각 컬렉션 최대 **4096 entries**다. Core 기본 한도는 별도로 **bytes1 MiB / depth64 / container4096 / shared-table4096 / object8192 / structural-value visits65536**다. 객체 예산에 모든 컨테이너 노드를 합산하지 않는다. 문자열32768 UTF-16 units·필드4096도 Core 한도이며 모델의 ID256과 다르다([Core README](../../ConsoleGameCore/Serialization/README.md#제한)). 잘림·비정규 bool·후행 바이트·중복/참조/관계 오류는 거부하고 전체 후보 검증 뒤만 출력 루트를 교체한다.
- 이름 기반 필드 추가/삭제·since 이전 missing default를 지원하되 현재 버전 필수 누락은 거부한다. **미래 schema/wire/IGD1 format을 인식한 인증 슬롯은 Save/NewGame 덮어쓰기를 거부**하고 키·암호문·실패 Continue의 활성 상태를 보존한다. contentVersion도 임의 보정하지 않는다. unknown-field 재저장 보존·프로세스 간 CAS·임의 버전 마이그레이션은 제공하지 않는다. 소유 순환·weak_ptr·aliasing/서브객체는 지원 범위가 아니며, 숨은 aliasing provenance까지 완전 검출한다고 주장하지 않는다.
- [FSaveDataStore](../../TextRPG/InGameData/FSaveDataStore.cpp)의 범용 **byte 서비스 평문 최대 16 MiB**는 모델의 1 MiB와 별개다. 컨테이너는 `TRSV` version 1, 28-byte header(magic/version/length/reserved/12-byte nonce), ciphertext, 16-byte tag다. header 전체를 AAD로 인증하고 매 저장마다 CNG 랜덤 nonce를 생성하는 AES-256-GCM이다. 인증 전에는 bounded framing만 읽고 진행 객체를 게시하지 않는다.
- 평문은 메모리에서만 처리한다. 완성된 암호문만 독점 생성한 같은 디렉터리 임시 파일에 write→flush→close 후 rename 교체한다. 실패 시 이전 canonical bytes와 호출자 출력을 보존하고 해당 시도 소유 임시 파일만 정리한다. 키와 세이브 두 파일의 동시 원자성이나 전원 차단 durability를 보장한다는 뜻은 아니다.

### 9.3 기본 슬롯·키·저장 시점

```text
%LOCALAPPDATA%/TextRPG/Saves/Default/
  SaveData.key   현재 Windows 사용자 DPAPI 보호 blob (전용 랜덤 32-byte 키)
  InGameData.sav 인증 암호문
```

- `ResolveSaveDataDirectory`는 `TEXT_RPG_SAVE_ROOT`가 없을 때 위 **단일 Default 슬롯**을 사용한다. 테스트 override는 저장 디렉터리 전체를 지정하며 일반 로컬 절대경로를 검증한다. 빈/잘못된 override에서 기본 경로로 fallback하지 않는다. 키 값 자체를 읽거나 문서·로그·명령행에 기록하지 않는다.
- 콘텐츠 패키지 키·`TEXT_RPG_GAMEDATA_KEY`와 별개다. 최초 Save에서만 필요 시 새 키를 만들고, 기존 손상 키나 키 없는 기존 세이브를 조용히 rekey하지 않는다. 경쟁에서 진 경우 승자의 기존 보호 키를 검증해 사용한다. Continue/CanContinue/Load는 read-only이며 키 생성·파일 갱신·NewGame 기본값 재생성을 하지 않는다.
- NewGame은 실제 EndFrame 게시 성공을 `TakeStartResult`로 확인한 뒤 autosave한다. 저장 실패는 **`StartedSaveFailed` = 진행은 시작했으나 저장 실패**다. 정상 Quit는 현재 진행을 저장하며 실패 시 현지화 안내와 nonzero 종료를 사용한다. 강제 종료·비정상 입력/렌더 종료까지 모두 저장한다고 약속하지 않는다.

### 9.4 요청 수락과 프레임 끝 완료·예외/재진입 경계

- NewGame의 `Queued`와 Continue의 `true`는 **요청 수락**이다. 완료는 `Input → Update → Render/Present → EndFrame → TakeStartResult` 뒤 `Started`/`StartedSaveFailed`/`Failed`로 한 번 소비한다. 준비 실패는 기존 메뉴/진행/정상 파일을 보존한다. Continue에는 autosave가 없다.
- [FSceneManager](../../TextRPG/Scene/FSceneManager.cpp)는 typed request와 후보 소유권을 보관한다. first-wins·프레임 snapshot·recursive EndFrame guard로 처리 중 새 요청을 다음 프레임으로 넘기며 무제한 임의 lambda queue를 도입하지 않는다. 이전 비소유 UI/Scene을 먼저 정리하고 새 Scene/진행을 결합 게시하며, 이전 루트는 snapshot 수명 종료 때 늦게 해제한다.
- 공통 Scene NVI는 직접 공개 작업과 lifecycle을 포함한다. 활성/후보 Scene 및 실제 UI root의 busy를 검사한다. root는 **첫 사용자 virtual GetId/Find 호출 전부터 incoming 인수의 거부·예외 정리 종료까지** busy guard를 유지한다. const/noexcept 조회도 재진입 가능 경계다. CancelInput은 capture/drag를 먼저 비우고 중첩 flag를 복원한다.
- Quit/취소의 논리 shutdown·입력 무효화는 즉시지만 busy 후보의 물리 파괴/완료는 EndFrame까지 보류한다. callback이 반환해 idle이 된 뒤 재취소해도 이미 이월된 파괴를 앞당기지 않는다. destructor가 모두 반환한 뒤 완료를 한 번만 공개한다.
- 실제 OnExit→UI bridge→root 취소 callback throw는 RAII로 framework flag/bridge를 복구하고 최종 파괴 때 detach한다. old UI/Scene destructor의 Quit도 보존해 후보 게시·NewGame autosave를 취소하며 Main의 late shutdown에서는 추가 저장/해제된 Scene 재렌더를 하지 않는다. 임의 callback이 바꾼 앱 전역 상태의 rollback, callback 안 Manager 자기 삭제, 기존 모든 Layer flush의 EndFrame 이관은 범위 밖이다.

### 9.5 이전 후보의 실제 증거와 당시 수용 범위 — 2026-11-15 이력

이전 후보의 증거 루트는 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/ingamedata-implementation-20261115/`다. 아래 기록은 당시 범위 그대로 보존하며 현재 생성 모델 후보의 결과로 승격하지 않는다. 최신 실행은 §9.6을 따른다.

| 구분 | 실제 확인 범위 | 기록 |
|---|---|---|
| Scene/UI native | Debug/Release × x64/x86(프로젝트 Win32), 각 full **86 group**(UI62/EndFrame18/progress6), legacy **73**(62/11/0), progress **24**(0/18/6); 서로 겹치므로 합산 금지 | `entry-publication-repair/execution-gates.json`, `core/completion.json` |
| 선택 카탈로그·입력·게임 분석 | TextArt 생성 헤더 없는 소비자 각 1 group/4구성, Input 각168 checks/4구성, 게임 실제 분석 4구성 경고0 | 같은 execution gate |
| 실제 제품 EXE | 각 구성 default **23 case**, 명시 InGameData **5 case**. HP7·Gold4321의 재시작 Continue와 Quit 재역직렬화, 손상 보존·저장 실패 포함 | 같은 execution gate의 `actualGame` |
| 기존 서비스 실행 승계 | 네 구성 각각 SaveData **28**, InGameData **127**, Store **330 checkpoint occurrences**. 모델127은 고유 라벨54의 반복 보존 검사를 포함하므로 고유 테스트127개가 아님 | `entry-publication-repair/inherited-service-evidence.json`의 실제 `cells` 경로/해시 |
| Store reader 예외 실행 승계 | 네 구성 각각 **10 command record** = compile2 + fault execution8 | 같은 승계 기록; 신규 다섯 정의의 typed lookup/전체 필드·행/자산 보존·실제 reader 복구 포함 |
| runner 제어·화면 오라클 | meta **86 record**, cleanup **5**, 순수 물리 셀 screen **21**; preflight 4건 normal/-O 모두 exit2·child0 | `entry-publication-repair/report/completion.json` (제품 case와 합산 금지) |
| 독립 source review | core-v5-entry의 canonical coverage/verdict, report-v5-atomic coverage/verdict PASS. Save-v2·model-v2·exception-cohort-v1도 지정 동결 소스 범위 PASS | 각 `review-*/verdict.json`; 실행 리뷰가 아닌 source 판정 |

- 실제 EXE 감시는 소유 app/cwd/TMP를 자식 시작 전부터 회수 뒤까지 유지하고 save-root를 별도로 분리한다. 지정 루트 관찰이며 전역 sandbox/모든 쓰기 attribution이 아니다. Debug CRT allocation 주입은 Debug에서만 실행하며 Release의 다른 수명 회귀와 구분한다.
- 초기 `save-validated-matrix.json`/`model-validated-matrix.json`은 과거 후보 기록으로 그대로 보존한다. 최신 승계 기록의16개 suite/config cell을 기준으로 하며 서비스를 재실행하지 않았다. Save Debug/x64의 전체 XML은 다르지만 제외된 standalone CPP의 PCH metadata·IDE header 등록만 달라 **활성 compilation projection이 동일**하다. 이를 전체 프로젝트 파일 바이트 동일이라고 쓰지 않는다.
- 모델 private 데이터50파일 중49파일은 live와 바이트 동일하다. `Localization.yaml`은 기존 bytes가 정확한 prefix인 채 앱 전용 `InGameData.Ui.NewGameFailed`/`InGameData.Ui.NewGameSaveFailed` 두 행만 추가됐다. 정의/생성 헤더는 동일하고 현재 live 문구는 새 UI/실제 앱 matrix에서 검증했다. **private/live 전체 데이터 바이트 동일을 주장하지 않는다.** 보호 아트28개+Preview+AGENTS 총30파일 불변도 승계 기록에 명시한다.
- **최종 수용 기록 경로:** `entry-publication-repair/final-acceptance.json`. 이는 조정자 최종 집계의 목적 경로이며 이 문서만으로 해당 파일의 작성/전체 수용 완료를 선언하지 않는다. 과거 FAIL·NOT_RUN·보호 AGENTS 쓰기 차단 기록을 변경하지 않는다. 선택적 review suggestions는 새 필수 완료 gate가 아니다.
- UI-08/09의 모든 전투/장비/몬스터 콘텐츠·업적 UI 게임플레이, dirty 출력 스케줄링, 전체 DPI/글꼴/수동 호스트 시각 인증, 배포 콘텐츠 키 전달 정책은 남아 있다. 같은 PC/Windows 사용자 v1 범위를 넘어선 이동·rollback 방지·완전 치팅 방지·전원 차단 복구는 미보장이다.

### 9.6 생성 모델 통합의 현재 실행·검토 — 2026-11-16

현재 근거 루트는 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/ingamedata-41751db1/`이며 `current-evidence-index.json`의 현재 gate와 `build/current-final-grid.json`을 따른다. 문서 작업은 기록을 읽어 반영했고 빌드/코드 감사/추가 재검토를 수행하지 않았다.

| 범위 | 현재 기록 |
|---|---|
| 네 구성 solution Build + native 분석 | Debug/Release × x64/x86(프로젝트 Win32) 각각 성공·경고0 |
| native checkpoint groups | 네 구성 각각 Core28 / model137 / Scene94. model137은 고유64라벨의 반복 raw checkpoint occurrences이며 고유 단위 테스트137개가 아님 |
| 실제 앱 | 네 구성 각각 일반23 / 명시 진행5 case. missing-package가 반복되므로 고유28개로 합산 금지 |
| 추가 모드/화면 | Debug/x64 fixed-update·frame-skip 진행 각5 case; screen-oracle21은 게임 미실행 control로 별도 범위 |
| 생성/캐시 | optional 최초 정적 출력 누락 실제빌드1 group, 별도 schema missing→add→remove 실제 CPP 컴파일/삭제·안정 PCH 보존. warm model compiledObjects0/PCH 재생성0·생성물 bytes/mtime/EXE 불변이며 runtime 승계(새 분석/재실행 아님) |
| 독립 소스 검토 | 원본 coverage + core-v2 변경3/불변4, generator-v2 변경2/불변8, integration-v2 변경6/불변23의 집중 재검토 조합. 최초 FAIL은 보존하며 전체 신규 전수 리뷰/원격 승인 아님 |

- optional·동적 타입·warm의 정확한 기록 경로와 검토 coverage/verdict는 [012 §11](012_InGameDataYaml정의및자동생성설계안.md#11-구현검증-상태-관리)에 모았다. 전체 정적 13파일은 `GameData/Generated/GameData`로 바이트 그대로 이동했고 진행 생성물은 `GameData/Generated/InGameData`에 둔다. 제품 생성 CPP 소유자는 GameData.lib뿐이며 Custom CPP는 TextRPG, 콘텐츠 독립 archive는 ConsoleGameCore가 소유한다.
- 현재 source/실행 gate와 조정자의 최종 보호 입력·소스/산출물 정체성·문서/전체 수용 집계는 구분한다. 문서만으로 최종 acceptance를 게시하지 않는다. 과거 Frame·FAIL/BLOCKED·승인 차단은 그대로이며 UI-08/09 미구현 콘텐츠·수동 호스트 인증·배포 키 정책은 완료 범위 밖이다. 스테이징·커밋·푸시하지 않았다.

## 10. 구현 시 참고한 1차 자료

Windows CNG의 인증 tag 검증과 nonce/AAD 계약, DPAPI의 일반적인 계정/컴퓨터 종속성을 확인한 기존 참고 목록이다. 실제 정책/구현과 검증은 §9의 로컬 source·실행 기록을 기준으로 하며 이번 문서 동기화에서는 네트워크 조회를 하지 않았다.

```text
Microsoft Learn — BCryptDecrypt (STATUS_AUTH_TAG_MISMATCH)
https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptdecrypt
Microsoft Learn — BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO
https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/ns-bcrypt-bcrypt_authenticated_cipher_mode_info
Microsoft Learn — CryptProtectData
https://learn.microsoft.com/en-us/windows/win32/api/dpapi/nf-dpapi-cryptprotectdata
```
