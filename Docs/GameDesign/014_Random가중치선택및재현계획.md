# Random 가중치 선택·콘텐츠별 재현 계획

> 상태: **Random 컴포넌트 로컬 구현·실행 검증·독립 소스 검토 수용 완료.** 아래 조사/제안은 구현 전 계획 이력이며 현재 API는 [Random 사용법](../../ConsoleGameCore/Math/Random/README.md)과 §9를 따른다. 사용자 작성 FSelector의 틀과 관련 PCH/프로젝트 변경을 기반으로 구현했고 커밋·푸시는 하지 않았다. 운영 Excel 가챠/강화 콘텐츠 연결은 별도 후속 범위다.
> 사용자 정정: 랜덤 콘텐츠가 FSelector를 멤버 컴포넌트로 보유하고 SaveFile Load 시 저장한 seed/randCount로 복원한다. 이번 범위는 난수 컴포넌트·가중치 표·기존 저장/로드 연결이다. 행동별 자동 저장·비용/보상 트랜잭션·결과 공개 순서 재설계 제안은 철회한다.

## 1. 요구사항과 현재 코드

### 사용자 요구

1. seed와 randCount로 상태를 복원한다. 콘텐츠마다 FSelector를 하나씩 사용하여, 다른 콘텐츠 행동이 해당 콘텐츠의 다음 결과를 바꾸지 않게 한다.
2. AddProc(key, weight)로 등록한 내부 표와 게임 데이터에서 준비한 외부 공유 표를 모두 지원한다. 운영 가중치는 Excel에서 정의하여 시작 시 로드한다.
3. std::mt19937 외에도 C++ 표준 난수 엔진을 옵션으로 선택한다.

### 구현 전 조사 이력

- [FSelector.h](../../ConsoleGameCore/Math/Random/FSelector.h), [FSelector.cpp](../../ConsoleGameCore/Math/Random/FSelector.cpp)는 존재하며 프로젝트와 필터에도 등록되어 있다. namespace TextRPG는 기존 Core와 동일하게 유지할 수 있다.
- 기본 생성자는 random_device로 mt19937을 초기화한다. 명시 seed/randCount 생성자는 비어 있고 AddProc는 false, SelectKey는 0을 반환하는 작성 중 코드다. 기능 완료나 빌드 성공으로 간주하지 않는다.
- 생성자 인수는 uint64지만 m_seed/m_randCount는 uint32이고, SSelectResult.nextRandCount도 uint32다. key/weight/API/저장 멤버의 폭도 서로 다르므로 구현 때 정리한다.
- 현재 SelectKey는 const다. 상태를 소비하는 최종 선택 API로 쓰려면 비const가 되어야 한다. 미리보기는 명시적인 후보 복사로 구분한다.
- 현재 random_device 및 할당 가능한 AddProc 등에 noexcept가 선언되어 있다. 실제 오류 경계에 맞게 수정하며, 할당/seed 생성 실패를 강제 종료로 바꾸지 않는다.
- [진행 스키마](../../GameData/InGameDataSchema/InGameData.yaml)에는 난수 상태가 없다. 콘텐츠가 멤버로 보유할 Selector의 seed/count를 기존 저장/로드 경로에 연결하는 것은 구현 단계의 작업이다. 현재 [FGameSession](../../TextRPG/InGameData/FGameSession.cpp)의 저장 주기를 이번 Random 작업에서 바꾸지 않는다.

## 2. 책임·소유권

| 구성 | 소유·책임 | 하지 않는 일 |
|---|---|---|
| Core FSelector | 특정 스트림의 엔진과 현재 위치, 내부 등록 표, 결정적 선택 | GameData 조회, Excel 읽기, 강화/가챠 의미 해석, 파일 저장 |
| Core FProcTable — 제안 | 정렬된 key/weight와 누적 가중치를 가진 불변 선택 표 | 콘텐츠별 엔진·randCount 소유 |
| GameData/게임 데이터 준비 계층 | Excel 검증·로드 후 도메인별 공유 FProcTable 준비·수명 유지 | 플레이별 난수 위치 보관 |
| TextRPG 콘텐츠 객체 | FSelector를 멤버 컴포넌트로 소유하고 적합한 표를 선택 | 전역 RNG 공유, UI에 게임 진행 원본 보관 |
| InGameData/기존 저장 경로 | 콘텐츠별 seed/count 저장 및 로드 후 해당 컴포넌트에 전달 | 엔진 전체·정적 표를 SaveFile에 직렬화 |
| Session/콘텐츠 실행 경계 | 기존 저장/로드 흐름에서 컴포넌트 상태 연결 | Random의 파일 I/O·자동 저장·보상 트랜잭션 책임 추가 |

FSelector의 엔진 상태는 영속 상태에서 재구성하는 실행 캐시다. 저장할 상태를 별도 가변 원본 두 곳에서 독립 갱신하지 않는다. Scene 교체가 콘텐츠 스트림을 초기화하지 않게 한다. 기본 실행 모델은 게임 스레드의 직렬 실행이며, 복수 스레드 공유는 이번 범위가 아니다.

## 3. 재현 계약과 randCount의 단위

### 3.1 콘텐츠별 독립 스트림

- 강화와 가챠는 별도 FSelector 및 별도 저장 상태를 사용한다. 가챠·전투·이동이 강화 Selector의 엔진을 소비해서는 안 된다.
- 콘텐츠는 로드한 seed/randCount를 자신의 멤버 Selector에 전달한다. 로드 시 새 seed를 만들지 않는다. 엔진은 해당 컴포넌트 설정으로 동일하게 적용한다.
- 고정 엔진 설정이나 별도 알고리즘 버전을 세이브마다 중복 저장하도록 강제하지 않는다. 세이브별 엔진 선택을 지원할 때에만 엔진 식별자를 추가로 저장한다. 버전 이관 체계는 별도 요구가 있을 때 다룬다.
- 콘텐츠마다 하나라는 요구이므로 같은 강화 콘텐츠 안의 다른 아이템 강화는 같은 스트림을 소비한다. 아이템 인스턴스마다 결과를 고정하는 기능은 별도 스트림 설계이며 이번 최소 요구로 확대하지 않는다.
- 수용 기준은 같은 저장 상태를 복원한 뒤 다른 콘텐츠 행동을 사이에 넣어도 자신의 다음 결과가 같다는 것이다.

### 3.2 randCount는 선택 횟수가 아니라 엔진 원시 출력 호출 횟수

- seed: uint64, randCount: uint64. key와 개별 weight는 uint32, 누적 가중치는 checked uint64로 정한다. 데이터 PK/FK의 uint32 유지 결정은 바꾸지 않는다.
- randCount는 선택된 STL 엔진 객체의 operator()가 실제 반환한 횟수다. 분포 내부 rejection으로 버린 출력도 모두 센다. ranlux의 내부 base-engine 소비 횟수와는 구분한다.
- 복원은 고정 seed 초기화 규약 → engine.discard(randCount) 순서다. seed의 상·하위 비트를 고정 순서로 seed_seq에 전달하여 32비트 엔진에서 조용히 절삭하지 않는다. 초기화와 소비 규약을 구현·테스트에서 일치시킨다.
- SSelectResult.nextRandCount에는 성공 이후의 실제 위치를 반환한다. 실패는 엔진/count/기존 출력 값을 보존한다. count의 증가 overflow도 거부한다.
- 유효 선택은 양수 가중치 항목이 하나여도 최소 한 번의 엔진 출력을 소비하도록 고정한다. 실패한 선택은 소비하지 않는다.
- discard가 상수 시간이라고 가정하지 않는다. 큰 count의 복원 시간과 비정상 세이브의 자원 소모를 측정하여 복원 예산/명시 실패 경계를 정한다. 엔진 전체 상태 checkpoint는 필요성이 측정된 뒤의 최적화이며 첫 버전의 필수 저장 형식으로 넣지 않는다.

## 4. 가중치 표와 선택 방식

### 4.1 내부 등록·외부 공유를 함께 지원

- AddProc(key, weight)는 Selector 내부 m_mapProc를 갱신한다. 같은 key의 중복 등록은 거부한다. 가중치 수정/제거 API는 실제 수요가 있을 때 별도 추가하고 AddProc에 암묵적 합산·덮어쓰기를 섞지 않는다.
- 정적 외부 표는 게임 시작 시 한 번 검증·정렬·누적합 계산하여 FProcTable로 준비한다. 여러 강화 Selector/대상이 같은 표를 const 참조로 재사용해도 엔진 상태는 섞이지 않는다.
- SelectKey(const FProcTable&, ...)는 표를 호출 중에만 참조한다. 표를 Selector에 복사하거나 비소유 멤버 포인터로 영구 저장하지 않는다. 호출자는 호출 동안 표의 수명과 불변성을 보장한다.
- 이미 있는 const mapProc를 직접 인자로 받는 편의 overload도 제공할 수 있다. 이 경로는 호출마다 정규화/검증 비용이 있음을 명시하고, 반복 선택의 기본 경로는 준비된 FProcTable로 둔다. 주소만으로 외부 가변 map을 캐싱하지 않는다.
- 내부 AddProc는 표를 dirty로 만들고 다음 선택 때 준비한다. 성공적으로 준비한 누적 표만 캐시하며, 실패 시 이전 등록 내용과 엔진을 보존한다.
- 일회성 동적 표가 재시작 뒤에도 필요하면 해당 입력을 진행 상태에 저장하거나 동일하게 재구성하는 규칙이 있어야 한다. seed/count만 저장하고 동적 표를 잃어버린 상태로 재현을 주장하지 않는다.

### 4.2 검증·결정성

- key 0, 중복 key, 빈 표, 양수 가중치가 없는 표, 잘못된 정수/범위와 누적합 overflow를 거부한다. weight 0은 등록 가능하지만 선택 대상에서 제외한다. 이 정책은 Excel/런타임에서 일치시킨다.
- unordered_map 순회 순서나 Excel 행 순서를 추첨 순서로 사용하지 않는다. 항상 key 오름차순으로 정규화한다.
- 표 전체 합계에 대한 균등 정수 하나를 뽑고 누적 구간을 이진 탐색하여 key를 반환한다. 같은 표의 반복 추첨은 준비 비용을 반복하지 않는다.
- 실수 확률 대신 정수 weight를 사용한다. 단순 난수 % totalWeight는 사용하지 않는다. rejection 방식의 편향 없는 정수 범위 변환을 자체 고정 규약으로 정의한다.
- 각 엔진의 min/max와 유효 출력 범위를 사용한다. 모든 엔진을 0부터 시작하는 32비트 출력이라고 가정하지 않는다. 필요한 비트 조립 순서·초과 출력 처리·재시도와 소비 count를 버전화하고 엔진별 고정 기대값으로 검사한다.
- STL 분포의 생성 알고리즘은 구현 정의이므로, 저장 호환 경로에는 discrete_distribution/uniform_int_distribution의 결과 매핑을 그대로 의존하지 않는다. 엔진은 STL을 사용하되 가중치 변환은 프로젝트 규약으로 고정한다.

## 5. 엔진 옵션과 제안 API

최소 제공 옵션은 Mt19937(std::mt19937), Mt19937_64(std::mt19937_64), MinStdRand(std::minstd_rand), Ranlux48(std::ranlux48)이다. 기본값은 기존 Mt19937을 유지한다. 런타임 enum과 variant 등 내부 dispatch로 선택하여 콘텐츠 코드는 엔진 템플릿에 결합하지 않게 한다. random_device는 신규 seed 생성용으로만 사용하며 복원 가능한 추첨 엔진 옵션으로 제공하지 않는다. default_random_engine처럼 구현에 따라 실제 타입이 달라지는 별칭도 영속 엔진 식별자로 사용하지 않는다.

아래는 **제안 API**이며 현재 헤더에 구현되어 있다는 뜻이 아니다. 실제 선언의 오류/예외 명세는 지원 엔진과 할당 경계를 확인하여 확정한다.

```cpp
struct SSelectResult
{
    std::uint32_t key = 0;
    std::uint64_t nextRandCount = 0;
};

// SRandomState: seed, randCount. 엔진은 컴포넌트 설정을 유지한다.
FSelector(std::uint64_t seed, std::uint64_t randCount,
    ERandomEngine engineKind = ERandomEngine::Mt19937);

bool AddProc(std::uint32_t key, std::uint32_t weight);
bool SelectKey(SSelectResult& result, std::string& error);
bool SelectKey(const FProcTable& table, SSelectResult& result, std::string& error);
SRandomState GetState() const noexcept;
bool Restore(const SRandomState& state, std::string& error);
```

신규 생성 시 seed를 정하고 저장에는 그 seed와 현재 count를 기록한다. 일반 헤더 include 금지, PCH의 안정 STL 전제, 구체 Random 헤더의 CPP include, 탭4/Allman/140열, m_ 명명과 경고0 규칙을 따른다. 현재 사용자 코드/PCH 중복 정리는 구현 승인 후 관련 범위에서만 다룬다.

## 6. Excel·진행·강제 종료 연결

### 6.1 정적 데이터와 저장 호환

- 가중치 원본은 Excel이며 기존 Generator → Client YAML → GameData.dat → Store 로드 경로를 재사용한다. Core가 workbook이나 게임 Store를 직접 알지 않게 한다. GameData.lib는 정의 행을 제공하고 TextRPG의 게임 데이터 준비 계층이 Core FProcTable을 구성·소유하는 방향을 기본안으로 삼아, 새 GameData→Core 라이브러리 의존을 요구하지 않는다.
- 표 정의/행의 PK와 PoolId/ResultKey 같은 참조는 uint32다. 실제 테이블 설계에서는 결과 key의 대상 도메인(아이템/강화 결과 등)을 명확히 하여 FK를 검증한다. 어떤 테이블이든 가리키는 무검증 범용 key 열로 대신하지 않는다.
- 각 콘텐츠의 멤버 Selector에 대응하는 seed/randCount를 기존 진행 저장 구조에 둔다. 불필요한 전역 StreamId 레지스트리나 별도 RNG 저장 시스템을 만들지 않는다.
- 현재 세이브에는 RNG 필드가 없으므로 추가 필드의 이전 세이브 호환을 기존 저장 체계에서 처리한다. Continue 가능 여부의 읽기 전용 검사에서 seed 생성이나 파일 저장을 하지 않는다.
- 엔진 설정은 콘텐츠 생성/복원에서 동일하게 적용한다. 세이브별 엔진 선택이 요구될 때만 엔진 식별자를 함께 저장한다.
- 구체 강화/가챠의 운영 확률·보상·비용·천장 규칙은 아직 정하지 않았다. 이번 계획에서 임의 운영 workbook/확률값을 추가하지 않는다. 구현 검증은 통제된 fixture로 먼저 연결한다.

### 6.2 기존 SaveFile 로드에서 컴포넌트 복원

```text
강화 콘텐츠 → 멤버 FSelector → 강화 전용 seed / randCount
가챠 콘텐츠 → 멤버 FSelector → 가챠 전용 seed / randCount

SaveFile 저장 → 각 콘텐츠의 seed / randCount 기록
SaveFile Load → 저장 상태 읽기 → 해당 멤버 FSelector 복원 → 계속 사용
```

복원 시 같은 엔진을 seed로 초기화하고 randCount 위치로 진행시킨다. 전체 엔진 상태나 공유 가중치 표를 저장할 필요는 없다. 비용/보상·저장 시점은 기존 콘텐츠 책임이며, 행동별 자동 저장·결과 공개 순서·저장 트랜잭션 재설계는 이번 Random의 필수 범위에서 제외한다.

## 7. 구현 순서와 완료 기준

| ID | 작업 | 선행·완료 기준 |
|---|---|---|
| RNG-D01 | 설계와 로드맵 동기화 | 문서 완료. 요구·현재 stub·제안·미검증을 분리 |
| RNG-01 | 상태/엔진/count 계약과 작은 복원 회귀 | RNG-D01. 엔진별 연속 실행과 seed/count 복원 결과 및 다음 count 일치, 64비트 seed 절삭 없음 |
| RNG-02 | AddProc/불변 공유 표/결정적 가중치 선택 | RNG-01. 내부·외부 표 결과 동등, 순서 독립, weight/합계/경계·오류 시 상태 보존 |
| RNG-03 | 기존 Core 테스트·프로젝트/PCH 연결 | RNG-02. GameData 없는 독립 소비, 실제 lib 사용, 기존 NativeTests/runner·PCH/obj 재사용 |
| RNG-04 | Excel 유래 표와 콘텐츠별 진행 스트림 | RNG-02의 작은 연결부터 가능. 정적 정의→로드→표 공유, 다른 콘텐츠 소비 독립, 이전 세이브 읽기 호환 |
| RNG-05 | 기존 SaveFile seed/count 저장·로드 연결 | RNG-04. 콘텐츠 멤버 복원 후 동일한 다음 key/count, 로드 시 재시드 없음 |
| RNG-06 | 최종 검증·독립 소스 검토·문서 마감 | RNG-03/05. 필요한 네 구성 Build/분석 경고0·기존 영향 회귀·원본/캐시 보호, 실행/소스 범위 명시 |

검증에는 다음을 포함한다.

- 엔진별 고정 seed의 원시 출력/선택 key/nextRandCount 고정 기대값과 유효 엔진 설정.
- count 0/중간/큰 값, seed 상위 비트, count overflow와 rejection으로 여러 원시 출력을 쓴 선택의 정확한 복원.
- 서로 다른 엔진의 min/max 및 32비트·64비트 출력 차이. Debug/Release × Win32/x64에서 같은 계약의 결과 일치.
- 표의 등록/행/해시 버킷 순서만 바꾼 입력, key 최대값·0, duplicate, weight 0, 단일 양수 항목, 전부0·빈 표, 누적합 overflow와 구간 끝값.
- 분포 품질은 경계/알고리즘 검사와 구분한다. 작은 전수 가능한 가짜 원시 엔진으로 rejection/count를 검사하고 실제 지원 STL 엔진의 고정 시퀀스도 실행한다. 흔들리는 통계 허용치만으로 수용하지 않는다.
- 강화만 실행한 기준과 다른 콘텐츠를 사이에 실행한 뒤의 강화 key/count 일치. 같은 강화 스트림 소비는 다음 위치로 진행하는 대조군도 유지한다.
- seed/count 저장·로드 반복, old-save 추가 필드 호환, 동적 표 재구성과 컴포넌트 복원. 기존 저장·전용 키는 건드리지 않고 소유 fixture에서만 수행.
- 같은 저장 fixture를 별도 프로세스에서 로드한 뒤 다른 콘텐츠 행동을 섞어도 대상 콘텐츠의 다음 결과/count 일치. 파일 교체 중 강제 종료 fault matrix나 보상 트랜잭션은 이번 게이트에 추가하지 않는다.

작업 중에는 최소 RED/GREEN과 실제 producer→consumer 연결을 먼저 수행한다. 필수 전체 검증·독립 검토는 최종 후보에 모으고, 결함 보완은 영향 검사만 재실행한다. 처음 계획 작성 단계에서는 제품 빌드/테스트를 실행하지 않았으며, 후속 구현의 실제 실행은 §9로 구분한다.

## 8. 참조

- [Excel·정적 정의 계약](005_Excel게임데이터및경로관리설계안.md)
- [진행·저장·단일 원본](009_InGameData및저장복원설계안.md)
- [Core/콘텐츠 분리](010_게임코어및콘텐츠라이브러리분리설계안.md)
- [진행 모델 생성/직렬화](012_InGameDataYaml정의및자동생성설계안.md)
- [로드맵](../ROADMAP.md), [C++ 컨벤션](../Conventions/cpp.md)
- [C++ 표준 작업초안 rand.req.eng](https://eel.is/c++draft/rand.req.eng): 엔진 상태 복사와 discard의 의미/복잡도.
- [C++ 표준 작업초안 rand.dist.general](https://eel.is/c++draft/rand.dist.general): 분포 생성 알고리즘은 구현 정의다. 실제 구현은 프로젝트 C++20 지원 범위로 제한한다.
- [Microsoft Learn의 C++ Standard Library random](https://learn.microsoft.com/en-us/cpp/standard-library/random?view=msvc-170): 엔진/분포 분리, 사전 정의 엔진, random_device 및 seed_seq 참고. 문헌 확인은 설계 근거이며 현재 코드의 실행 검증이 아니다.

## 9. 실제 로컬 구현과 검증

사용자 후속 구현 승인에 따라 기존 `FSelector.*`를 구현하고 `RandomRange.h`를 추가했다. 현재 사용법은 [Random README](../../ConsoleGameCore/Math/Random/README.md)를 따른다. 클래스/테스트를 구현한 것과 운영 가챠/강화 콘텐츠를 새로 만든 것은 구분한다.

- FProcTable은 로드 단계 AddProc/Prepare 및 map Build로 준비하고, FSelector는 const 표를 선택 호출 동안 참조한다. 선택 중 표 복사/등록/할당 없이 독립 엔진 상태만 진행한다. 동적 내부 AddProc와 map 편의 선택도 제공한다.
- 네 STL 엔진, uint64 seed/raw count, 정수 rejection·정렬된 누적 표, 오류 시 상태/출력 보존을 구현했다. SRandomState는 기존 archive에서 seed/count 두 필드만 직렬화한다. GenerateSeed는 신규 생성에만 사용하고 Restore는 재시드하지 않는다.
- 통합 중 발견한 임시 복원 상한은 제거했다. 백만 회 초과 상태를 복원하고 다시 선택하여 저장한 위치도 복원되는 것을 네 엔진의 실제 참조 출력과 검사한다. 큰 discard의 시간 비용은 남으며, 무제한 외부 비신뢰 입력 서비스나 빠른 checkpoint 기능을 구현한 것은 아니다.
- 실제 SaveData fixture는 콘텐츠별 FSelector 멤버의 seed/count를 기존 archive와 인증 암호화 파일에 저장한다. 별도 프로세스가 Load하고 같은 표를 다시 준비하며, 다른 콘텐츠 소비를 사이에 넣어도 기대 next key/count가 같고 로드가 ciphertext/key를 쓰지 않음을 확인한다.

### 최종 실행 범위 — v2 영향 검사와 불변 경로의 기존 증거

| 항목 | 실제 결과 |
|---|---|
| Debug/Release × x64/x86 solution Build | 네 구성 성공, 진단0. Clean/Rebuild 없이 기존 캐시 사용 |
| TextRPG 제품 코드 분석 | v1 네 구성 성공, 경고0. 제품 TU/PCH/옵션 불변으로 해당 분석 증거 재사용. 변경 Random 코드는 아래 네 구성 Core 분석으로 갱신 |
| Core lib 및 독립 native 소비자 분석/실행 | v2 네 구성 각각39 checkpoint group, 경고0. 기존31과 RNG8 포함 |
| SaveData 분석/실행 | 네 구성 각각31 checkpoint group, 경고0. 기존28과 RNG 저장/로드3 포함 |
| Debug/x64 기존 Scene 회귀 | 불변 Scene 소스/의존 계약의 v1 분석·실행99 checkpoint group, 경고0 증거 재사용. 새 제품 EXE 실행 주장 아님 |
| Core 역의존 음성 | 불변 프로젝트/PCH/include 경계의 v1 정상 독립 소비자 양성 뒤 미허용 생성 헤더 include가 정확히 C1083으로 거부된 증거 재사용. 이 예상 컴파일 실패를 제품 오류로 집계하지 않음 |
| 구조/등록·헤더 정책 | 기존 build-contract33 및 header-policy8 통과 |
| 독립 소스 검토 | v1 전체 지정12경로 검사에서 발견한 두 지적을 repair1로 보완. v2 변경4경로의 별도 독립 집중 재검토 PASS와 불변8경로의 기존 검사 범위를 조합하여 수용. 전체65파일 의미 재검토 주장 아님 |

Core 첫 행동 RED, 통합의 상한 거부 RED, 분석/fixture 초기 실패와 v1 실행 통과를 별도 보존한다. 증거는 소유 scratch `random-selector-83bd9a20`의 `core-worker/`, `integrated-core-green2/`, `integrated-save-green3/`, `final/matrix.json`, `review/v1/`에 있고 최초 독립 결과는 `evidence/review/v1/result/`다.

repair1에서 준비 표 이동 후 원본의 vector/합계/준비 상태를 함께 초기화하고, 복사 지원과 자기 이동 보존 및 선택 게시 전 empty/zero/end 방어를 추가했다. 이동 원본 미준비 assertion의 실제 RED 뒤 RNG-08로 원본 거부·상태/출력 보존·재사용·복사/목적지 수열을 확인했다. 신규 Allman 위반도 보완했다. 최초 FAIL은 바꾸지 않고 `repair1/affected/matrix.json`의 현재 네 구성 실행, `review/v2/result/`의 새 독립 집중 검토, `review/composition.json`의 부모 범위 조합을 사용한다. 원본 소스·기존 PCH/obj 캐시를 재사용했고 보완 뒤 불변 제품 분석/Scene 회귀/경계 검사를 반복하지 않았다.

### 범위와 남은 연결

- RNG-01/02/03과 재사용 컴포넌트의 기존 archive/SaveFile 연결 패턴은 구현·실행했다. RNG-04의 **운영 Excel 가챠/강화 정의·실제 콘텐츠 진행 필드**는 해당 콘텐츠가 정의될 때 연결한다. 현재 없는 테이블/밸런스를 임의 추가하지 않았다.
- 파일 교체 도중 강제 종료·전원 차단 보장, 행동별 autosave·보상 트랜잭션, 엔진/표 변경의 별도 버전 이관은 이번 변경에 추가하지 않았다.
- 사용자 원본 Excel·아트·기존 세이브/전용 키와 무관한 구현·과거 실패 이력은 보존한다. 실행은 소유 fixture만 사용하며 커밋·푸시는 하지 않았다.
