# Random 컴포넌트

`ConsoleGameCore.lib`의 콘텐츠 중립 가중치 선택 기능이다. 소비 CPP는 자신의 `pch.h` 뒤에 `Math/Random/FSelector.h`를 포함한다. 일반 헤더에는 include를 추가하지 않는다.

## 역할

- `FProcTable`: `(uint32 key, uint32 proc)`를 등록하고 `Prepare`하여 공유하는 선택용 표. 정렬·누적합 계산은 준비 단계에서 수행한다.
- `FSelector`: 각 콘텐츠가 멤버로 소유하는 독립 난수 엔진/seed/count와 선택 기능. 동적 내부 표 등록도 가능하다.
- `SRandomState`: `uint64 seed`, `uint64 randCount`만 보관하는 직렬화 가능 상태.
- `SSelectResult`: 선택한 `uint32 key`와 선택 후 `uint64 nextRandCount`.

가중치 원본은 게임의 Excel이다. 이 모듈은 workbook이나 GameData를 직접 읽지 않는다. 게임 데이터 로드 계층이 검증된 행을 등록하고, 콘텐츠는 적절한 공유 표를 전달하여 선택한다. 실제 가챠/강화 콘텐츠·운영 확률 테이블은 아직 없으므로 이 변경에서 임의로 만들지 않았다.

## 로드 시 표 준비, 콘텐츠에서 선택

다음은 콘텐츠 테이블의 `reward_id`, `proc` 행을 전달하는 사용 패턴이다. `gachaRows`는 호출자가 조회한 행 범위이며 새로운 제품 테이블/API 이름이 아니다.

```cpp
// 데이터 로드 단계: 한 번 준비하여 데이터 쪽에서 수명을 유지한다.
FProcTable gachaTable;
std::string error;
for (const auto& row : gachaRows)
{
	if (!gachaTable.AddProc(row.reward_id, row.proc, error)) return false;
}
if (!gachaTable.Prepare(error)) return false;

// 콘텐츠 초기 생성: 생성한 seed를 그대로 보관한다.
FSelector selector(GenerateSeed(), 0, ERandomEngine::Mt19937);
SSelectResult selected;
if (!selector.SelectKey(gachaTable, selected, error)) return false;
// selected.key가 reward_id다. 보상 처리는 콘텐츠가 수행한다.
```

- 반복 선택은 준비된 `const FProcTable&`를 사용한다. 선택 중 표 복사·표 재등록·할당을 하지 않는다. 표의 엔트리/누적합은 바꾸지 않는다.
- 표 변경은 다른 추첨 호출과 겹치지 않게 한다. 변경 뒤에는 다시 `Prepare`해야 한다. 멀티스레드 동시 수정/선택은 지원 범위가 아니다.
- 표 복사는 준비 상태를 보존한다. 이동은 목적지로 상태를 넘기고 원본을 빈 미준비 표로 초기화한다. 원본 선택은 상태를 소비하지 않고 실패하며, AddProc/Prepare로 재사용할 수 있다. 자기 이동 대입은 기존 상태를 유지한다.
- `Build(const unordered_map<uint32,uint32>&, error)`로 기존 map을 한 번에 준비할 수도 있다.
- `selector.SelectKey(map, result, error)` 편의 경로도 있지만 매번 표를 준비한다. 로드 완료한 정적 데이터에는 준비된 표 참조를 권장한다.

## 동적 내부 등록

```cpp
if (!selector.AddProc(rewardId, weight, error)) return false;
if (!selector.SelectKey(selected, error)) return false;
```

내부 표는 변경된 경우에만 다음 선택에서 준비한다. key-only `SelectKey()` 편의 함수는 실패 시 예약값 0을 반환한다. 진단/실패 구분이 필요한 경로는 bool/error overload를 사용한다.

## seed/count 저장·로드

- 신규 콘텐츠 생성 시 `GenerateSeed()` 결과를 seed로 사용하고 이후 seed는 고정한다.
- `GetState()`로 현재 seed/count를 얻고 기존 콘텐츠 진행 저장에 기록한다.
- Load에서는 저장한 값으로 `FSelector(savedSeed, savedCount, engineKind)`를 생성하거나 기존 멤버의 `Restore(state, error)`를 호출한다. Restore는 새 seed를 생성하지 않고 선택된 엔진과 내부 표를 유지한다.
- 엔진 종류는 해당 콘텐츠 생성/복원 설정에서 동일하게 사용한다. 세이브별로 엔진 선택을 허용할 때만 종류를 함께 기록한다.
- 저장 주기·보상·자동 저장 정책은 컴포넌트가 정하지 않는다. 공유 표·엔진 전체 상태는 저장하지 않는다.

기존 archive의 콘텐츠 `SerializeFields`/`DeserializeFields`에 연결하는 패턴은 다음과 같다.

```cpp
// SerializeFields: 엔진/표를 저장하지 않고 두 값만 기록한다.
const auto state = m_selector.GetState();
return writer.Field("seed", state.seed, error)
	&& writer.Field("randCount", state.randCount, error);

// DeserializeFields: 로드한 값으로 멤버 컴포넌트를 복원한다.
SRandomState state;
return reader.Field("seed", state.seed, error)
	&& reader.Field("randCount", state.randCount, error)
	&& m_selector.Restore(state, error);
```

`SRandomState` 자체를 독립 객체/지속 수명의 멤버로 archive 직렬화하는 것도 지원한다. 여러 콘텐츠를 한 archive에 저장할 때는 위처럼 snapshot의 두 primitive를 기록한다. 호출마다 생성/파괴되는 임시 ISerializable snapshot을 중첩 Field로 전달하면 주소가 재사용되어 archive의 객체 소유권 검사와 충돌할 수 있다.

## 엔진과 결정성

`ERandomEngine::Mt19937`(기본), `Mt19937_64`, `MinStdRand`, `Ranlux48`을 선택할 수 있다. `GenerateSeed`는 신규 seed용 `random_device` 출력으로 uint64 seed를 만들며 오류를 noexcept로 숨기지 않는다.

randCount는 **선택된 엔진의 operator() 원시 출력 횟수**다. rejection으로 버린 출력도 포함한다. 성공한 추첨의 증가량이 반드시 1인 것은 아니다. ranlux는 내부 base 호출이 아니라 선택한 wrapper 엔진의 출력 횟수다.

초기화는 seed의 하위/상위 32비트를 고정 순서로 seed_seq에 전달한다. 복원은 그 초기화 뒤 `discard(randCount)`를 수행한다. 선택은 key 정렬, 엔진 min/max 정규화, 정수 비트 조립/rejection 규약으로 처리하며 unordered_map 순회나 STL 분포 구현의 매핑에 의존하지 않는다.

정상 선택으로 생성한 저장 위치를 거부하는 임의 복원 상한은 없다. 다만 discard 비용은 엔진과 count에 따라 증가할 수 있다. 신뢰할 수 없는 count를 무제한 실행하는 외부 서비스로 사용하지 않으며, 큰 스트림의 빠른 복원/checkpoint는 별도 최적화 범위다.

## 오류 정책

- key 0·중복 key·빈 표·전체 weight 0을 거부한다. weight 0인 행은 선택되지 않는다.
- key/개별 proc는 uint32, 누적합은 checked uint64다. 합계가 uint32 범위를 넘는 표도 지원한다.
- AddProc/Build/Prepare/Restore/Select 실패 시 적용 전 엔진·count·기존 출력/표를 보존한다. 선택 count overflow도 거부한다.
- bool/error API라도 오류 문자열 할당 실패나 런타임 종료까지 복구한다고 보장하지 않는다. GenerateSeed 및 유효하지 않은 명시 생성자 인수는 예외를 전달할 수 있다.

## 검증 경로

기존 runner와 원본 소스·구성별 PCH/obj 캐시를 재사용한다.

```text
python Tools/Tests/test_scene_ui_runtime.py --core-only --analyze --architecture x64 --configuration Debug --scratch-root <owned-root> --label core
python Tools/Tests/test_gamedata_store_runtime.py --suite SaveData --architecture x64 --configuration Debug --scratch-root <owned-root> --label save
```

Core 검사는 엔진별 참조/고정 시퀀스, rejection 소비 횟수, 표 순서·오류 보존, 컴포넌트 독립성, 상태 archive와 큰 위치 복원을 포함한다. SaveData 검사는 콘텐츠 멤버 Selector의 seed/count를 실제 archive→인증 암호화 파일에 저장하고 별도 프로세스에서 Load하여 공유 표를 다시 준비한 뒤 다음 key/count를 확인한다. 기존 세이브·키 대신 소유 fixture 디렉터리만 사용한다.

실행 범위와 독립 소스 검토 상태는 [설계/기록](../../../Docs/GameDesign/014_Random가중치선택및재현계획.md)과 [로드맵](../../../Docs/ROADMAP.md)을 따른다. 강제 종료 도중 파일 복구·운영 가챠 콘텐츠 구현까지 검증한 뜻은 아니다.
