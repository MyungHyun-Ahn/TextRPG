# 콘텐츠 독립 필드 직렬화

`ConsoleGameCorePch.h`가 안정 STL과 `ISerializable.h`를 제공한다. 소비 CPP는 그 다음
`Serialization/FSerializationArchive.h`와 구체/생성 타입을 포함한다. 일반 헤더에는 include가 없다.
게임 타입, 생성 헤더, 저장 경로, 암호화, 전역 가변 레지스트리에 의존하지 않는다.

## 호출과 실패 경계

- `FSerializationArchive::Serialize(root, output, error, schemaVersion, registry)`는 성공 후에만 출력 벡터를 swap한다.
- `Deserialize(bytes, candidate, error, supportedSchemaVersion, registry)`는 별도 후보만 수정한다.
  후보는 실패 시 부분 변경될 수 있으므로 호출자가 버리고, 검증 성공 후에만 활성 상태로 게시한다.
- wire schema version은 양수이며 지원 버전보다 새 값은 최초 타입 조회/팩토리/훅 전에 거부한다.
- NVI는 before hook → 고정 필드 열거, 고정 필드 복원 → after hook 순서다. 예외는 bool 실패로 변환한다.
  after hook은 객체의 모든 필드가 성공한 경우에만 호출한다.
- 같은 스레드의 중첩 archive 호출, 같은 reader/writer의 NVI/Field 재진입은 거부한다.
  중첩 Serialize/Deserialize는 별도 error 인자를 써도 활성 archive를 실패 상태로 만든다.
  최초 가상 조회 전부터 문맥/임시 객체의 가상 소멸까지 보호하고, 정리가 끝난 뒤 실패 상태를
  확인하여 Serialize 출력 swap과 Deserialize 성공 반환을 금지한다. 후보의 부분 변경은 rollback하지 않는다.
  archive guard와 레지스트리 사용 pin은 지역 후보/팩토리 결과/공유 문맥 정리의 가상 소멸까지 살아 있다.
- 입력/출력 버퍼, 원본 객체, 레지스트리의 수명을 호출 전체 동안 보장한다. 콜백은 이들 저장소를
  다른 별칭을 통해 변경하거나 파괴하지 않아야 한다. 사용자 콜백 자체의 외부 상태 변경은 rollback하지 않는다.
- 서로 독립된 registry 없는 archive 호출은 스레드별 guard를 사용한다. **동일 레지스트리 접근은
  호출자가 직렬화한다.** 레지스트리는 동시 수정/접근용 컨테이너가 아니며, 사용 중 Register/Replace는 거부한다.

## 지원 타입

bool, int32/uint32/int64/uint64, float/double, UTF-16 `std::wstring`, 등록 enum,
기본 allocator/comparator/hash의 vector/map/unordered_map과 그 재귀 조합을 지원한다.
직렬화 객체 값은 `ISerializable`과 `static constexpr std::string_view kTypeName`을 제공하고
기본 생성 및 base 값의 move/copy assignment가 가능해야 한다. unique_ptr는 지원 값 타입을,
shared_ptr는 직렬화 객체를 가리킨다. shared null/반복 참조/서로 값이 같은 별도 인스턴스는 구분한다.

Map key는 정수 네 종류, wstring, 명시적 enum만 지원한다. unordered_map은 key의 `<` 순서로
방문하므로 버킷 배치/삽입 순서와 무관하다. raw/weak pointer, 임의 string 인코딩,
커스텀 allocator/comparator/hash, 일반 STL 임의 타입은 지원하지 않고 bool 실패한다.

enum은 archive 헤더 다음, 최초 Field 인스턴스화 전에 다음 특수화를 제공한다.

```cpp
namespace TextRPG
{
    template<> struct TSerializationEnumTraits<MyEnum>
    {
        static constexpr std::string_view kTypeName = "MyEnum";
        static bool IsValid(MyEnum value) noexcept;
    };
}
```

enum underlying type은 signed/unsigned 8/16/32/64-bit 정수다. 숫자 구조/필드 ID가 아니라
타입 이름과 underlying signature를 저장한다. 예: uint8_t는 `enum<ESceneType,u8>`,
uint16_t는 `enum<EMode,u16>`, int8_t/int16_t는 `i8`/`i16`이다. 기존 i32/u32/i64/u64
signature와 little-endian payload 바이트는 바뀌지 않는다. 타입 이름은 traits의 bare schema name이며
C++ namespace의 `::`를 넣지 않는다. 현재 스키마가 소비하는 enum 값은 IsValid로 검사한다.
제거된 enum 필드는 현재 enum 정의 자체가 없으므로 underlying 폭/framing만 검사하여 건너뛴다.

## 하나의 생성 루트를 합성하는 wrapper

`ISerializable`의 protected static `ForwardSerializeFields(data, writer, error)`와
`ForwardDeserializeFields(data, reader, error)`는 wrapper 자신의 fields override에서만 사용한다.
이 helper는 동일한 열린 프레임에 delegate의 protected fields만 직접 열거하며,
delegate의 NVI나 before/after hook은 다시 실행하지 않는다. 바깥 wrapper NVI가 훅을 소유한다.
wrapper의 타입 이름은 생성 루트의 kTypeName이며, private 생성 루트 하나만 실제 상태로 보관한다.
wrapper와 생성 루트를 직접 archive한 바이트는 같다. delegate에 Custom-only hook이 있다면
이 forwarding 경로는 그것을 실행하지 않으므로 wrapper 훅에서 그 정책을 명시적으로 소유해야 한다.

helper는 writer/reader phase를 열거나 running/failed guard를 초기화하지 않는다.
fields dispatch 밖, 실행 중인 Field의 callback 안, 바깥 NVI owner 자신으로의 전달,
전달 순환/깊이 초과는 거부한다. delegate의 false/throw와 무시한 중첩 실패도 프레임을 실패 상태로 유지한다.
일반 `data.Serialize(writer, error)`/`data.Deserialize(reader, error)`를 같은 활성 프레임으로 호출하는 것은
여전히 NVI 재진입이므로 거부한다.

## 레지스트리와 Custom

`Register<T>(T::kTypeName, std::function<std::unique_ptr<T>()>, error)`로 기본 타입을 등록한다.
중복 등록은 거부한다. `Replace<T>`는 기존 선언 타입과 스키마 이름이 같은 binding만 교체한다.
Custom factory는 unique_ptr<T>로 파생 타입을 반환한다. factory의 null/예외/잘못된 타입 이름은
실패하며 기본 T로 fallback하지 않는다. 제공된 루트 후보 자체를 레지스트리로 교체하지 않는다.

중첩 shared/unique/value 모두 레지스트리를 조회한다. 공유 인스턴스 하나당 factory와 훅은 한 번이다.
**값 멤버는 등록된 파생 임시 후보에서 복원/검증 훅을 실행한 뒤 생성 base T 필드를 이동한다.**
따라서 값 저장소는 여전히 선언 T이며 Custom-only 상태/동적 타입은 유지하지 않는다.
그 상태를 유지하려면 포인터 멤버를 사용한다. 빈 registry나 해당 이름의 binding 부재는 기본 T를 사용한다.

## 소유권 지원 범위와 제한

지원 그래프는 **완전 객체를 가리키는 비순환 소유 그래프와 비순환 공유 DAG**다.
사이클(사용하지 않는 wire 객체 표 안의 사이클 포함), 제어 블록 없는 nonnull shared_ptr,
관측된 같은 제어 블록의 서로 다른 주소, 같은 주소의 서로 다른 제어 블록,
관측된 shared/exclusive/value 혼합 소유는 거부한다. 실패한 복원 중 공유 사이클을 게시하지 않는다.

**단일 shared_ptr만 관측하면 표준 C++ API로 그 포인터가 aliasing constructor로 만들어진
멤버 포인터인지, 동일 완전 객체의 정당한 owning pointer인지 판별할 수 없다.**
따라서 aliasing/subobject/shared-unique 혼합 소유는 호출자 계약상 미지원이며,
모든 미관측 별칭을 런타임이 검출한다고 주장하지 않는다. 여러 주소/소유 경로가 관측되는
음성 사례는 검사하지만, 유일하게 도달 가능한 숨은 subobject alias의 완전 검출은 남은 제한이다.

## wire format 1

모든 정수/길이는 little-endian이며 메모리 전체나 포인터 주소를 복사하지 않는다.

1. 4-byte ASCII `CGSA`, u32 wire version `1`, u32 schema version.
2. u32 root byte length + root object bytes.
3. u32 shared object count + 각 객체의 u32 byte length와 object bytes. 표 index+1이 wire 인스턴스 참조 번호다.
4. object: u32 name length + ASCII schema type name, u32 field count.
5. field: 길이 기반 ASCII member name, 길이 기반 full type signature, u32 payload length + payload.
   kind는 signature의 최상위 atom이다. 예: `vector<shared<object<SCharacter>>>`, `enum<EMode,i32>`.
6. vector: u32 count + 길이 기반 요소. map: u32 count + 길이 기반 key/value 쌍.
   unique: 0/1 byte + 존재 시 값. shared: u32 표 참조(0은 null). wstring: u32 UTF-16 unit count + u16 units.

객체 표는 먼저 전체 indexing/framing/type signature/참조/중복/사이클/제한 검사를 마친다.
객체를 실제로 생성하는 시점은 현재 필드가 그 참조를 처음 소비할 때다. 따라서 필드 삭제와
읽기 순서 변경 뒤에도 공유 identity가 보존되며, 제거된 타입을 registry에 등록하거나 생성할 필요가 없다.
알 수 없는 필드도 framing 전체를 재귀 검사한다. 중복 이름/중복 key/잘못된 signature/잘린 값/
잘못된 UTF-16/nonfinite float/trailing bytes는 거부한다. 현재 enum 의미 검사는 typed dispatch가 담당한다.

## 제한

| 제한 | 기본 상한 / 단위 |
|---|---|
| 전체 archive bytes | 1 MiB |
| depth | 소유 참조를 포함한 깊이64 |
| container | 컨테이너 하나당 원소4096개 |
| shared-table | 공유 객체 표4096개 |
| object | 전체 객체8192개 |
| structural-value visits | 전체 구조/값 방문65536개 |

유지하는 공유 객체 payload 합도 최대 1 MiB다. 문자열 최대32768 UTF-16 units와
객체 하나의 필드 최대4096개는 별도 한도다.
객체 예산과 컨테이너/값 예산은 독립적이다. 루트/접두 객체와 3개 scalar 필드를 가진
값 객체 4096개도 1 MiB 이내면 지원하고, 필드의 이름 기반 읽기 순서는 수용 여부를 바꾸지 않는다.
전체 패킷 validator는 제거된 필드까지 이 예산을 검사하고 실제 read/write도 같은 상한을 사용한다.
이 제한은 기본 안전 상한이며 가변 설정/자동 상향은 제공하지 않는다.
생성기는 `reader.GetSchemaVersion()`과 `required`를 결합하여 since 이전 missing default만 허용한다.
일반 Field(required=false)는 누락 시 현재 값을 보존하므로 생성 코드가 먼저 스키마 기본값을 설정한다.

## 현재 통합·검증 — 2026-11-16

- 기존 `test_scene_ui_runtime.py --core-only`와 원본 소스·고정 obj/PCH 캐시를 사용했다.
  네 구성(Debug/Release × x64/x86, 프로젝트 Win32)의 실제 Core **28 checkpoint groups**와
  solution Build/native 분석 경고0을 확인했다. model137/Scene94·실제 앱 일반23/진행5는
  각각 다른 소비 범위이며 Core 테스트 수와 합산하지 않는다.
- 제품 `GameData/Generated/InGameData` CPP는 GameData.lib가 소유하고 게임은 private 생성 루트
  하나에 직접 변경/필드 위임한다. Core에는 생성 타입·Custom·Excel·저장 서비스 의존성이 없다.
  게임의 IGD1 읽기→CGSA 쓰기 이관/인증된 미래 슬롯 저장 거부는 게임 통합 정책이며 Core의
  archive API만으로 파일 암호화·교체를 제공한다는 뜻이 아니다. 모델 ID256/모델1 MiB와
  byte 저장 서비스16 MiB도 위 Core 예산과 구분한다.
- 근거는 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/ingamedata-41751db1/`의
  `build/current-final-grid.json`과 `current-evidence-index.json`이다. source coverage는
  core-v1 원본 검토의 불변4파일 + core-v2 변경3파일 집중 재검토의 조합이며,
  전체 신규 전수 재검토/원격 승인이 아니다. 초기 FAIL은 보존한다.
- warm model 기록(`native/model-warm-final/results.json`)의 compiledObjects0/PCH 재생성0은
  캐시 재사용 증거다. runtime은 동일 EXE 결과 승계이며 새 `/analyze` 실행이 아니다.
  전체 통합 범위는 [012 §11](../../Docs/GameDesign/012_InGameDataYaml정의및자동생성설계안.md#11-구현검증-상태-관리)을 따른다.
  이번 README 갱신에서는 빌드/제품 실행/추가 코드 검토를 반복하지 않았고 커밋·푸시하지 않았다.
