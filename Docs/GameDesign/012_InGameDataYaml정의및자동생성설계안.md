# InGameData YAML 정의·ISerializable 코드 자동 생성 설계안

> **현재 정적 데이터 참조 타입:** [Excel 작성 규칙](../../GameData/Excel/README.md)에 따라 제품 InGameData의 CharacterId/ItemId/AchievementId/m_sceneId는 uint32이며 schema_version은 2다. 이전 IGD1/CGSA schema1 문자열 ID 읽기는 콘텐츠 계층에서 이관하고 새 저장에는 숫자만 쓴다. 일반 Core 아카이브의 string·string-map 지원과 구조/필드 이름 메타데이터는 유지한다. 아래 범용 codec 예제의 문자열을 Excel 데이터 PK/FK 허용으로 해석하지 않는다. 현재 실제 검증과 최종 소스 검토는 로드맵을 따른다.

> 상태 — 2026-11-16: **생성기·Core·생성 모델 직접 사용·저장/Continue 통합을 구현했고, 네 구성 실제 실행과 독립 소스 검토의 조합을 확인했다.** 타입과 멤버의 원본은 InGameData YAML이며, 여기서 C++ 구조체와 직렬화·역직렬화 코드를 생성한다. 현재 실행/검토 근거와 범위는 §11을 따른다. 로컬 작업만 완료했으며 스테이징·커밋·푸시·원격 승인은 없다.
> YAML은 개발용 **구조 정의 입력**이다. 실제 세이브는 byte payload와 기존 인증 암호화·파일 저장 경로를 사용한다. 기존 진행 소유권·Continue/EndFrame 게시 계약과 현재 실행 상태는 [009](009_InGameData및저장복원설계안.md)를 따른다.

## 1. 확정 요구와 변경 규칙

- InGameData YAML에서 구조체/클래스와 Serialize/Deserialize 기능을 자동 생성한다. C++ 헤더를 역으로 분석하거나 별도 수동 필드 목록을 작성하는 방식이 아니다.
- 생성 InGameData 타입은 모두 public ISerializable을 상속한다. Custom 타입은 생성 타입을 상속하여 같은 계약을 따른다. 기존 UI/실행 객체나 Excel 정적 타입까지 일괄 상속시키지 않는다.
- 생성 타입은 다른 생성 타입을 값·포인터·지원 컨테이너의 원소로 가질 수 있다. **멤버 선언 타입은 개발자가 선택**하고 생성기는 지원하는 선언을 그대로 처리한다.
- 모든 객체를 한 포인터 종류로 바꾸는 전역 규칙이나 별도 ownership 설정 항목은 두지 않는다. shared_ptr를 이번 지원에 포함하고 컨테이너의 원소·값까지 재귀 조합한다. 실제 캐릭터 진행은 shared_ptr로 선언한다.
- STL 직렬화에 vector, map, unordered_map의 재귀 조합을 구현했다. Excel 정적 모드의 필드 문법(§2.2)과 InGameData 구조 모드의 지원(§7)은 별개다.
- 생성 InGameData 구현은 **GameData 프로젝트에서 반드시 컴파일**한다. 수작업 특수 처리는 TextRPG/CustomInGameData에 둔다.
- 출력 경로는 **GameData/Generated/GameData**, **GameData/Generated/InGameData**다. 기존 Class 출력 전체 13파일(소유 마커 포함)은 바이트 변경 없이 전자로 이관했다.
- **구조체는 추가·삭제 가능하지만 이름 변경은 금지**한다. 기존 타입을 참조 중인 선언을 남긴 채 대상을 삭제하면 생성 오류다.
- **멤버는 추가·삭제 가능하지만 기존 멤버의 이름·타입 변경은 금지**한다. 컨테이너 원소/키/값 타입이나 포인터 종류 교체도 타입 변경이다. 게임 진행에 따른 저장 값 변경을 금지하는 것은 아니다.
- 타입과 멤버는 YAML에 정의한 **이름으로 식별**한다. 숫자 타입/필드 식별자와 별도 호환 lock 파일은 두지 않는다.

이름·타입 변경 금지는 개발 규칙이다. 현재 정의만 읽는 생성기가 과거 rename/타입 변경까지 자동 검출한다고 주장하지 않는다. 자동 대조가 필요해지면 Git 이전 정의/리뷰로 확인할 수 있지만 별도 이력 파일을 필수 산출물로 추가하지 않는다.

## 2. 진행 데이터의 실행 모델과 기존 Excel 모드

### 2.1 생성 타입을 실행 원본으로 직접 사용

- 생성 InGameData 객체가 **게임 중 변경되는 진행 값의 단일 원본**이다. 저장 직전 별도 DTO에 복사하거나 별도 수작업 진행 필드와 양방향 동기화하지 않는다.
- 기존 TextRPG::InGameData는 하나의 생성 SRootInGameData를 비공개로 소유하고, 기존 세션 API·게임 검증·변경 동작을 연결한다. ISerializable의 필드 처리는 그 생성 루트에 위임하며 필드를 수작업으로 다시 열거하지 않는다.
- 캐릭터·스탯·인벤토리·업적은 생성 타입을 직접 사용한다. 기존 공개 타입명은 생성 타입 alias로 유지할 수 있다. CustomInGameData는 필요할 때만 생성 타입을 상속한다.
- FGameSession이 활성 루트를 소유하고 인게임 로직이 같은 객체를 수정한다. Scene/UI는 const·비소유 조회만 하며 별도 HP·재화 원본이나 진행 객체의 소유권을 갖지 않는다. 루트의 shared_ptr를 UI에 노출해 const 경계를 우회하지 않는다.
- 메모리 상태 변경과 파일 게시를 구분한다. 파일에는 기존 저장 시점에 현재 생성 객체 전체를 Serialize한 결과를 반영하며 매 멤버 변경마다 자동 I/O하지 않는다.

### 2.2 기존 Excel 모드의 지원 범위

아래는 **Excel 정적 생성 모드**의 필드 지원 범위다. YAML 진행 모드(§7)의 포인터·컨테이너 지원을 정적 콘텐츠 스키마로 확대하지 않는다.

| 원본 #Type | 현재 생성 C++ 필드 |
|---|---|
| bool | bool |
| int32 / uint32 | ::std::int32_t / ::std::uint32_t |
| int64 / uint64 | ::std::int64_t / ::std::uint64_t |
| float / double | float / double |
| string | ::std::wstring |
| enum<Name> | ::GameData::Common::EName |

- 일반 STL 클래스 필드로 직접 생성하는 것은 현재 std::wstring이다. vector/map/unordered_map을 포함한 컨테이너, 스마트 포인터, 임의 생성 구조체의 값/포인터 멤버는 아직 지원하지 않는다.
- enum 기반 정수의 uint8/uint16 지원은 별도다. 일반 필드의 uint8/uint16 문법까지 지원한다는 뜻은 아니다.
- 집계 헤더의 `<array>` include, 생성기 구현의 C# List/Dictionary, 런타임 Store의 vector 사용은 입력 필드 타입 지원 증거가 아니다.
- Excel 모드는 타입 헤더와 정적 값 YAML을 출력한다. 기존 ClosedXML과 별도로 InGameData 구조 입력에는 YamlDotNet 16.3.0 event parser를 사용한다. 두 모드는 입력 모델·출력 소유 마커를 분리한다.

근거: [WorkbookParser.cs](../../Tools/GameDataGenerator/WorkbookParser.cs)의 ParseType, [GameDataGenerators.cs](../../Tools/GameDataGenerator/GameDataGenerators.cs)의 GenerateCppTable/RenderCppType, [GameDataModel.cs](../../Tools/GameDataGenerator/GameDataModel.cs), [생성기 프로젝트](../../Tools/GameDataGenerator/GameDataGenerator.csproj), [SelfTest.cs](../../Tools/GameDataGenerator/SelfTest.cs).

## 3. 원본·생성물·수작업 코드의 배치

아래는 실제 배치다. 스키마/생성 출력/Custom 코드를 별도로 관리한다.

```text
GameData/
  Excel/                       기존 정적 콘텐츠 원본, 유지
  InGameDataSchema/             진행 구조 정의 원본
    InGameData.yaml
  Generated/
    GameData/                   기존 Class 전체 13파일의 이관 위치
      Common/ ... / GameDataAll.h
    InGameData/                   타입·구현·기본 팩토리·집계 생성 전용
      ... / InGameDataAll.h
  Runtime/                     기존 정적 데이터 소스, 유지

TextRPG/
  CustomInGameData/               수작업 상속 확장·게임 측 팩토리 바인딩
```

- 정적 콘텐츠 값과 초기 규칙은 계속 Excel 원본이다. InGameData YAML에 초기 HP·아이템 정의 같은 콘텐츠 수치를 복제하지 않는다.
- 스키마 입력과 두 생성 출력은 물리적으로 분리한다. 입력 스키마/도구 소스는 GameData/Data/Client 패킹 입력이나 사용자 세이브에 섞지 않는다.
- 각 생성 출력은 별도 소유 마커와 실제 출력 목록으로 관리한다. 생성기가 CustomInGameData·스키마·다른 생성 출력을 정리 대상으로 삼지 않는다.
- 경로 이관은 출력 기본값·include 검색 경로·프로젝트/filters·생성/테스트·문서·집계 계약을 함께 변경한다. 기존 값 YAML/Excel/아트와 Runtime 경로까지 불필요하게 바꾸지 않는다.

## 4. 생성 구조와 컴파일 소유자

```text
Excel → 기존 정적 콘텐츠 생성 → Generated/GameData + 기존 값 YAML
InGameData.yaml → 스키마/참조/선언 타입 검증
             → Generated/InGameData 타입·Serialize/Deserialize·기본 팩토리
             → GameData 프로젝트의 C++ 컴파일 → GameData.lib
             → TextRPG/CustomInGameData의 선택적 상속·팩토리 연결
             → InGameData 루트의 진행 소유

저장: 전체 객체 인코딩 → byte payload → 기존 FSaveDataStore 인증 저장
복원: 인증/복호화 → 후보 생성·복원·검증 → 기존 EndFrame 게시
```

- ISerializable과 byte 처리의 게임 독립 구현은 `ConsoleGameCore/Serialization`에 있다. 제품 생성 CPP는 **GameData.lib만** 컴파일·소유하며 일반 TextRPG/Packer/native 소비자는 링크한다. altered-generated/optional fixture만 원본을 별도 variant로 컴파일한다. Core는 GameData/생성 타입/CustomInGameData를 참조하지 않는다.
- GameData 프로젝트에는 생성 헤더의 ClInclude 등록뿐 아니라 **생성 CPP의 ClCompile 포함**을 보장한다. 생성이 먼저 실행되고, 새 구조체를 추가한 첫 빌드와 빈 생성 출력 상태에서도 새 구현을 실제로 컴파일해야 한다.
- GameData가 Core 구현 함수를 사용하면 필요한 단방향 빌드/링크 관계를 명시한다. 순수 인터페이스의 헤더 의존성과 구현 라이브러리 의존성을 구분하고, GameData를 참조하는 기존 Packer/테스트의 전이 영향도 확인한다.
- 일반 헤더 include 금지, 생성 타입/집계의 PCH 제외와 [C++ 컨벤션](../Conventions/cpp.md)을 유지한다. 필요한 정의는 소비 CPP에서 의존 순서대로 포함한다.
- 값 멤버의 정의 순서, 포인터 대상 전방 선언, 스마트 포인터 소멸자의 완전한 타입 요구까지 생성한다. 값 타입의 무한 포함은 오류로 하며 재귀 객체 인스턴스는 깊이/개수 한도로 제한한다.
- Custom 구현은 TextRPG에서 컴파일한다. GameData에 TextRPG/CustomInGameData 역의존성을 추가하지 않는다.

## 5. YAML 정의 예시

다음은 지원하는 문법의 축약 예시다. 실제 제품 정의는 [InGameData.yaml](../../GameData/InGameDataSchema/InGameData.yaml)이며, 예시 필드는 새 게임 콘텐츠 정의가 아니다. `since` 이전 파일의 누락 기본값과 현재 버전 필수 검사를 구분한다.

```yaml
schema_version: 2
types:
  - name: SCharacterProgress
    fields:
      - name: CharacterId
        type: string
        missing: reject
        since: 1

  - name: SRootInGameData
    fields:
      - name: Character
        type: 'shared_ptr<SCharacterProgress>'
        missing: default
        since: 2
        default: null
      - name: Characters
        type: 'vector<shared_ptr<SCharacterProgress>>'
        missing: default
        since: 2
        default: []
      - name: Counters
        type: 'map<string, uint32>'
        missing: default
        since: 2
        default: {}
      - name: Flags
        type: 'unordered_map<string, bool>'
        missing: default
        since: 2
        default: {}
```

- 타입은 예시의 생성 대상 이름·기본형·지원하는 템플릿 문법으로 제한한다.
- 타입 선언은 검증된 타입 트리로 해석한다. 임의 C++ 문자열을 그대로 소스에 삽입하지 않는다. 미등록 대상·중복 이름·잘못된 인수 개수·미지원 조합·중복 YAML 키를 위치와 함께 진단한다.
- 같은 스키마에서 구조체 이름은 유일하고, 한 구조체 안의 멤버 이름도 유일하다. 파일 식별자는 YAML에 정의한 이름을 사용하며 C++ RTTI 이름이나 실행별 해시를 쓰지 않는다.
- missing/default는 이전 파일에 새 멤버가 없을 때의 복원 정책이다. 콘텐츠 초기값은 Excel 정의와 게임 검증에 연결한다. 모든 누락을 0/빈 값으로 정상 처리하지 않는다.
- shared_ptr 멤버와 컨테이너의 여러 원소가 같은 객체를 가리키면 복원 후에도 동일 객체를 공유한다. 선언만 같다고 서로 다른 원본 객체를 하나로 합치지 않는다. 기본형·값·unique_ptr의 의미도 각각 유지한다.

## 6. 이름 기반 추가·삭제 호환과 파일 형식

### 6.1 개발 규칙과 검증 범위

- 구조체/멤버의 이름이 영속 식별자다. 이름을 바꾸면 과거 파일에서는 다른 타입/필드가 되므로 변경을 허용하지 않는다. 기존 멤버의 선언 타입도 유지한다.
- 구조체 추가·삭제와 멤버 추가·삭제는 가능하다. 다른 선언이 참조하는 타입을 삭제하고 해당 참조를 남기면 현재 스키마 검증에서 거부한다.
- 이름을 삭제 후 다른 의미로 재사용하면 과거 파일을 잘못 해석할 수 있다. 기존 멤버의 정체성을 바꾸지 않는다는 규칙으로 관리한다.
- 생성기는 현재 입력의 문법·중복·참조·지원 타입을 검사한다. 과거 정의 없는 자동 변경 감지나 이력 승인을 제공하는 것으로 표현하지 않는다. 별도 호환 lock 파일은 만들지 않는다.

### 6.2 구현된 바이너리 framing

```text
내부 payload: CGSA / wireVersion(1) / schemaVersion / rootByteLength / rootObject
              / sharedObjectCount / 길이 기반 sharedObject 표
객체:        typeName / fieldCount / 길이 기반 field 목록
필드:        fieldName / fullTypeSignature / fieldByteLength / fieldPayload
```

- 리더는 멤버 이름으로 대응시키고 길이로 모르는/삭제된 필드를 건너뛴다. 문자열 이름을 쓰더라도 이름 길이·문법·전체 크기·객체 경계를 검증한다. 타입명은 생성/게임 등록 목록 조회에만 사용하고 임의 클래스/모듈을 로드하지 않는다.
- 멤버 추가: `since`보다 이전 파일에서만 `missing: default`의 중립 기본값을 허용한다. `missing: reject`는 항상 필수이며, default 필드도 같은/이후 버전에서 누락되면 손상으로 거부한다. 도입 버전은 YAML `since`로 관리한다.
- 멤버 삭제: 과거 파일의 해당 필드는 길이를 검사하고 건너뛴다. 현재 코드가 반드시 필요로 하는 타입/필드를 몰라 복원할 수 없는 경우에는 실패하고 기존 파일을 보존한다.
- unknown-field를 재저장용으로 보존하지 않는다. 지원보다 새 schemaVersion/wireVersion은 거부한다. 세션도 인증된 기존 슬롯의 미래 CGSA schema/wire 또는 IGD1 format을 확인하면 **Save와 NewGame 저장/덮어쓰기를 거부**하고 슬롯/키를 보존한다. Continue 실패 시 활성 진행도 보존한다. 게임 스레드의 직렬 저장 경계이며 프로세스 간 CAS를 보장하지 않는다.
- shared 표는 먼저 전체 framing/참조/사이클/예산을 검증한 뒤 현재 필드가 처음 소비할 때 생성한다. 삭제 필드에서 처음 정의된 객체를 유지 필드가 참조해도 identity를 복원하며, 삭제 전용 타입은 현재 registry에 없어도 생성하지 않고 건너뛴다. 실제 교차 버전 fixture로 확인했다.
- 동일 객체의 중복 필드, 잘못된 kind/길이, 필수 누락, 후행 데이터, 잘못된 문자, 총량/count/깊이 초과는 오류로 한다. 할당·덧셈·곱셈 전에 한도를 검사한다.

### 6.3 현재 세이브와의 관계

현재 writer는 이름/길이 기반 **CGSA wire 1**이다. 고정 순서 **IGD1 v1은 읽기 전용 이관 경로**로 남겼다. 두 포맷의 바이트가 같다는 뜻이 아니며 TRSV 인증 컨테이너 재사용과 내부 모델 호환은 별개다.

IGD1 v1은 기존 검증을 유지하여 새 생성 루트 후보로 읽는다. Continue는 읽기만 하며 다음 명시 저장에서 새 포맷으로 기록한다. 기존 세이브를 임의 삭제·초기화하거나 새 게임 기본값으로 대체하지 않는다. 기존 모델/byte 서비스의 크기 제한, 인증 후 복원, 실패 출력 보존은 초기 이관에서 유지한다.

## 7. 선언 타입과 STL 지원

다음은 현재 InGameData 생성/직렬화 지원 범위다. 실제 생성 C++ 컴파일·round-trip과 실패 보존 근거는 §11을 따른다.

| 선언 종류 | 생성/직렬화 계약 | 범위 |
|---|---|---|
| 기본형·문자열·enum | 명시적 폭/인코딩·값 검증. enum은 기존 Excel Enums에 바인딩 | 구현·검증 |
| 생성 InGameData 값 멤버 | 해당 구조체의 Serialize/Deserialize로 재귀 처리 | 구현·검증 |
| vector<T> | 개수 + 원소 처리, 빈/다수/최대·할당 전 검사, 지원 T 조합 검증 | 구현·검증 |
| map<K,V> | 개수 + 키/값 처리. 지원 키와 복원 중복 키 거부 | 구현·검증 |
| unordered_map<K,V> | 개수 + 키/값 처리. 키 정렬로 결정적 출력, 복원 중복 키 거부 | 구현·검증 |
| unique_ptr<T> | null/존재와 단독 소유 객체 처리, 후보 RAII 복원 | 개발자가 해당 타입을 선언했을 때 적용 |
| shared_ptr<T> | null·공유 객체 식별과 실제 공유 관계 복원. vector/map/unordered_map 원소·값의 재귀 조합도 포함 | 구현·검증, 실제 캐릭터도 shared_ptr |
| weak_ptr/비소유 포인터·참조 | 별도 참조 복원 계약이 없으면 위치가 있는 생성 오류 | 이번 지원에 포함하지 않으며 다른 소유 타입으로 대체 금지 |
| 그 외 컨테이너/선언 | 구현된 handler가 없으면 위치가 있는 생성 오류 | 임의 지원/묵시적 생략 금지 |

- 키는 int32/uint32/int64/uint64·문자열·enum을 지원하며 임의 객체 키/사용자 comparer·hash는 지원하지 않는다. 값은 지원하는 기본형/생성 타입/포인터/중첩 컨테이너 조합으로 검증한다.
- unordered_map은 키의 `<` 순서로 직렬화한다. 버킷/삽입 순서와 독립적인 평문 byte 재현성과 매번 nonce가 달라지는 암호문을 구분한다.
- 저장 가능한 타입이라고 모든 선언을 자동 지원하는 것은 아니다. 미지원 타입을 건너뛰거나 다른 포인터 종류로 바꾸지 않는다. 생성 자료형의 복사/이동 가능성도 멤버 선언을 따르고 move-only 값을 복사하도록 강제하지 않는다.
- 공유 인스턴스는 주소가 아닌 payload 내부 표 참조로 복원한다. 완전 객체의 비순환 소유 그래프/DAG를 지원하고 순환은 거부한다. aliasing/서브객체/혼합 소유는 미지원이며, 단일 포인터에 숨은 aliasing provenance까지 보편적으로 검출할 수는 없다. 관측된 충돌 거부와 미관측 별칭의 완전 검출을 혼동하지 않는다. 타입/필드 숫자 ID와 payload 내부 인스턴스 참조 번호는 별개다.

## 8. CustomInGameData 상속과 복원

```cpp
// 실제 상속 경계의 축약 예시. 생성 타입은 ISerializable을 상속한다.
class FCharacterInGameData : public GameData::InGameData::SCharacterProgress
{
    // 생성 필드 처리를 재사용하고 문서화된 확장 훅에서 특수 동작을 구현한다.
};
```

- ISerializable에는 기반 포인터로 파괴할 수 있는 가상 소멸자를 둔다. 생성 타입은 final로 막지 않고 문서화한 가상 확장 훅을 제공한다. 공개 Serialize/Deserialize는 비가상 진입점으로 두고 생성 SerializeFields/DeserializeFields를 final로 고정한다. Custom은 OnBeforeSerialize/OnAfterDeserialize 훅으로 특수 동작·검증을 확장하며 스키마 필드 열거를 생략할 수 없다.
- 기본 생성 코드가 모든 스키마 필드를 처리한다. Custom 확장이 필드 누락이나 인증/버전/한도/후보 게시 우회를 만들지 않도록 기본 처리와 전후 훅을 구분한다.
- 게임은 **스키마 구조체명 → 생성/Custom 타입 팩토리**를 명시적으로 연결한다. 중첩 객체 복원에도 같은 바인딩을 적용한다. Custom C++ 클래스명을 파일의 별도 스키마 타입으로 취급하지 않는다.
- 바인딩된 허용 타입만 생성한다. Core/GameData가 CustomInGameData 구현을 직접 참조하지 않으며 TextRPG가 등록한다. 기본 팩토리는 생성 타입을 만든다.
- 실제 `TextRPG/CustomInGameData/FCharacterInGameData`를 캐릭터 스키마명에 연결한다. shared 객체는 반복 참조 전체에 factory/hook 한 번, unique/value 중첩도 같은 registry를 사용한다. 값 멤버는 Custom 후보 검증 후 생성 base 필드를 이동하므로 Custom-only 동적 상태는 유지하지 않는다. null/throw/잘못된 타입 factory는 실패하며 기본 타입으로 fallback하지 않는다.
- Custom의 추가 영속 멤버도 YAML에 정의한다. YAML 밖에 byte 필드를 몰래 덧붙여 멤버 규칙을 우회하지 않는다. 캐시/서비스 참조 같은 비영속 상태는 구분한다.
- 복원은 새 후보에 수행한다. 인증·필드·참조·현재 정적 정의/게임 관계 검증 후에만 기존 Scene 준비/EndFrame 게시로 연결하며 실패 시 현재 진행·메뉴·세이브를 보존한다.

## 9. 생성·빌드 통합과 보호

- 기존 GameDataGenerator의 진단·명명·결정적 출력·파일 게시 보호를 재사용하되 Excel 정적 입력과 YAML InGameData의 모델·검증·출력 소유권은 분리한다. 제한된 YAML event parser·타입 AST·정확 위치 진단을 구현했다. 실제 옵션/지원 문법은 [Generator README](../../Tools/GameDataGenerator/README.md#ingamedata-구조-yaml-모드)를 따른다.
- GameDataAll과 InGameDataAll을 분리한다. 정적 데이터만 쓰는 소비자가 InGameData 전체 헤더를 무조건 포함하지 않게 한다.
- 생성 헤더/CPP·필요한 기본 팩토리/집계와 실제 폴더 구조를 GameData 프로젝트/filters에 등록한다. 생성 결과의 신규/삭제 타입이 실제 ClCompile 항목에 반영되는 시점을 검증한다.
- 생성 실패 시 입력·Custom 파일·기존 생성 출력을 보존하고 성공한 전체 후보만 게시한다. 검사 실패를 오래된 생성물 사용 성공으로 처리하지 않는다.
- 변경 없는 생성은 내용과 mtime을 보존하며 원본 CPP, 기존 native_build/NativeTests, 타깃별 고정 PCH/obj와 증분 Build를 사용한다. 소스 복사·Clean/Rebuild·중복 runner를 기본 절차로 삼지 않는다.
- NewGame 게시 뒤 autosave, 정상 Quit 저장, Continue read-only/no autosave, late-shutdown의 추가 저장 금지 및 기존 키/슬롯/파일 교체 정책을 바꾸지 않는다.

## 10. 작업 ID와 수용 기준

SDG-D01은 설계 문서 작성만 뜻한다. SDG-01~06은 아래 범위의 구현·실제 실행/검토 근거(§11)를 확보했으며, 문서만으로 체크를 닫은 것이 아니다. 확장 게임 콘텐츠나 미지원 소유 그래프까지 완료한 뜻은 아니다.

| ID | 작업 | 완료 기준 |
|---|---|---|
| SDG-01 | YAML 문법·타입·포맷 계약 | 확정 경로, 선언 타입 선택, 이름 기반 식별·변경 금지, 누락/버전 및 기존 파일 이관 범위 확정 |
| SDG-02 | YAML 파싱·현재 정의 검증 | 문법/위치·중복 이름·미등록 참조·미지원 타입/조합·값 포함 순환 거부, 입력/기존 출력 보존 |
| SDG-03 | ISerializable·타입·양방향 코드 생성 | 기본형/생성 값/선언 포인터와 vector/map/unordered_map 실제 컴파일·round-trip·미지원 오류·한도 검사 |
| SDG-04 | CustomInGameData 확장·중첩 팩토리 | 사용자 파일 재생성 불변, 구조체명 바인딩과 실제 Custom 생성/확장, 후보 실패 정리 |
| SDG-05 | GameData 컴파일·출력 경로 이관 | Generated/GameData·Generated/InGameData 등록, 새 타입 첫 빌드/빈 출력에서 생성 CPP 컴파일, Core 단방향 관계와 Packer 영향·PCH/캐시 재사용 |
| SDG-06 | 저장·호환·실제 통합 검증 | 이름 기반 추가/삭제·누락·잘못된 이름/타입/길이·새→구 덮어쓰기 방어, 기존 파일 보존/명시 이관, 실제 Continue/정상 Quit·필요 구성 Build/분석 경고0·독립 검토 |

순서: SDG-01 → SDG-02/공통 인터페이스 계약 → SDG-03 → SDG-04/05 → SDG-06. 모든 단계는 기존 테스트 진입점을 확장하고 관련 정상·실패·경계와 source/config/artifact 조건을 구분한다. 타입 추가/삭제 호환을 약속하려면 실제 교차 버전 fixture를 실행해야 한다.

## 11. 구현·검증 상태 관리

현재 근거 루트는 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/ingamedata-41751db1/`다. `current-evidence-index.json`의 과거 중간 NOT_RUN/FAIL은 보존하고, 현재 범위는 `currentFinalExecutionGrid`/`sourceAndExecutionGates`와 아래 개별 기록을 따른다. 문서 작업은 이 기록을 읽어 반영했으며 빌드·코드 감사·새 독립 검토를 반복하지 않았다.

| 구분 | 실제 확인 범위 | 근거(증거 루트 기준) |
|---|---|---|
| 솔루션/분석 | Debug/Release × x64/x86(프로젝트 Win32) **네 구성 각각 solution Build + native 분석, 경고0** | `build/current-final-grid.json`의 구성별 buildEvidence/analysisEvidence |
| Core/model/Scene native | 네 구성 각각 **Core28 / model137 / Scene94 checkpoint groups**. model137은 반복 라벨 포함 raw checkpoint occurrences이며 고유 라벨64다. Scene94는 UI62/EndFrame25/진행7 | 같은 grid의 coreResult/modelResult/sceneResult |
| 실제 제품 앱 | 네 구성 각각 일반23 case와 명시 진행5 case PASS. missing-package가 모드 사이 반복되므로 고유28개로 합산하지 않음 | 같은 grid의 fullAppResult/progressAppResult |
| 추가 Frame 모드/화면 오라클 | Debug/x64 실제 앱 fixed-update·frame-skip 진행 **각5 case**. screen-oracle **21 control**은 게임 미실행 별도 범위 | `app/Debug-x64-final-modes.json` |
| optional 첫 빌드 | 기본 정적 생성 출력이 없는 상태에서 실제 생성→optional 소비자 컴파일/실행 1 group, 경고0. 기존 생성물 bytes/mtime 복구 | `native/optional-first-missing-preservation.json` |
| 신규/삭제 타입 첫 컴파일 | 별도 스키마·출력의 missing→add→remove 실제 GameData Build 각각 성공/경고0, 추가 CPP 컴파일·삭제 타입 lib 잔존 없음, 안정 PCH bytes/mtime 보존 | `build/dynamic-types-first-add-remove.json` |
| warm 재사용 | Debug/x64 model Build **compiledObjects 0 / PCH 재생성0**, 생성물 bytes/mtime·EXE 불변. runtime은 기존 동일 EXE 결과 승계, 새 분석 실행 아님 | `native/model-warm-final/results.json` |
| 독립 source coverage | core-v1 + core-v2 변경3/불변4, generator-v1 + generator-v2 변경2/불변8, integration-v1 + integration-v2 변경6/불변23의 조합. 현재 차단 지적 해결 | index의 coreRepairRound1/generatorRepairRound1/integrationRepairRound1 및 `review/*-v2-independent/{coverage,verdict}.json` |

- 위 수치는 suite/구성/모드별 범위다. 제품 case·native checkpoint·screen control을 합쳐 고유 테스트 수로 만들지 않는다. 독립 coverage는 원본 검토와 변경분 집중 재검토의 조합이지 전체를 새로 전수 재검토하거나 원격 승인받은 기록이 아니다. 최초 부정 판정·설정 실패/승인 차단은 그대로 남는다.
- Core 기본 한도는 **전체 bytes 1 MiB / 소유 참조 포함 depth64 / 개별 container4096 / shared-table4096 / object8192 / structural-value visits65536**다. object 예산에 모든 컨테이너 노드를 합산하지 않는다. 문자열32768 UTF-16 units·개별 필드4096 및 shared payload 합1 MiB도 별도 제한이다. 모델 ID256·모델1 MiB·byte 저장 서비스16 MiB·생성기 YAML/AST 입력 한도를 구분한다([Core README](../../ConsoleGameCore/Serialization/README.md#제한)).
- 실제 모델은 `InGameData` wrapper의 private 생성 `SRootInGameData` 하나를 변경 원본으로 사용한다. 생성 alias·shared 캐릭터·Custom factory를 연결하고 wrapper는 `ForwardSerializeFields`/`ForwardDeserializeFields`로 위임한다. 수동 필드 사본/저장 DTO는 없다. 새 CGSA 쓰기·IGD1 읽기 이관·미래 버전 Save/NewGame 거부를 기존 인증 저장/EndFrame 정책에 통합했다.
- SDG-01~06의 로컬 구현/실행 근거를 확보했다. 조정자의 최종 보호 입력·소스/산출물 정체성·문서 집계와 전체 수용 기록은 별도이며 이 문서가 대신 게시하지 않는다. 원본 Excel/아트·실제 사용자 세이브/키와 과거 Frame/FAIL/BLOCKED 이력은 보존한다. UI-08/09 확장 콘텐츠·수동 호스트 인증 및 배포 키 정책은 미완료다. 스테이징·커밋·푸시·publication은 없다.
