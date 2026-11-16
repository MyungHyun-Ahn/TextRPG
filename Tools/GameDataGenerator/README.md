# GameDataGenerator

Excel을 유일한 **정적 콘텐츠 값/초기 규칙** 원본으로 사용하는 Client 전용 생성기다. 별도 InGameData YAML 모드는 진행 객체의 구조와 필드 codec을 생성한다. Windows, .NET 9와 프로젝트에 지정한 ClosedXML/YamlDotNet을 사용한다. 아래 명령은 **TextRPG.sln이 있는 솔루션 루트**에서 실행한다.

## Excel 키 작성 규칙

모든 테이블의 PK/FK는 숫자 정수다. 현재 원본은 양수 uint32 PK와 같은 타입의 FK를 사용하며 문자열 PK/참조키는 금지한다. Localization.TextKey와 TextArt.ArtId도 예외가 아니다. SceneTypeConfig.ConfigId 및 Enums.EnumId가 숫자 PK이고 SceneType/EnumName은 비키 분류/이름이다. 선택 TargetSceneId는 명시적 Default0으로 미지정을 표현한다. [Excel 작성 규칙](../../GameData/Excel/README.md)을 원본·템플릿·검증·생성·소비자 전체에 적용한다. 이번 숫자 이관의 실제 검증 상태는 로드맵을 따른다.

## 게임 빌드로 사용하기

Visual Studio 2026에서 TextRPG를 빌드하면 GameData가 생성의 단일 소유자다. Excel 검증/정적 생성과 InGameData 구조 생성 → 동적 CPP 항목 갱신/라이브러리 컴파일 → 소비자 링크/패킹 의존 순서를 사용한다. `GameData.dat`에는 정적 Client YAML과 Asset을 넣으며 InGameData 구조 입력/CPP는 패킹하지 않는다. 솔루션과 TextRPG 개별 프로젝트 빌드 모두 지원한다.

- C++ 플랫폼: `x64` 또는 `Win32` / 솔루션의 32비트 플랫폼 이름: `x86`
- 구성: `Debug`, `Release`
- 최종 데이터 예: `Out/TextRPG/Debug/x64/GameData.dat`
- 암호화·키 공급·직접 패킹 방법: [GameDataPacker README](../GameDataPacker/README.md)

Excel 편집 내용은 저장한 후 빌드한다. 생성기는 workbook을 편집하거나 열린 Excel을 강제로 닫지 않는다. 저장 중이거나 다른 프로세스가 파일을 독점하고 있으면 먼저 해당 작업을 끝낸다.

## 생성기를 직접 실행하기

```powershell
dotnet run --project .\Tools\GameDataGenerator\GameDataGenerator.csproj --configuration Release -- --input-root .\GameData\Excel --output-root .\GameData\Generated\GameData --yaml-output-root .\GameData\Data
```

이미 빌드된 도구로 검증/생성할 수도 있다.

```powershell
# 원본 검증만: 생성물 게시/정리하지 않음
dotnet .\Out\GameDataGenerator\Release\GameDataGenerator.dll --validate-only --input-root .\GameData\Excel

# 생성 결과가 현재 원본과 일치하는지 확인: 읽기 전용
dotnet .\Out\GameDataGenerator\Release\GameDataGenerator.dll --check --input-root .\GameData\Excel --output-root .\GameData\Generated\GameData --yaml-output-root .\GameData\Data

# 내장 정상/실패/경계 회귀 검사
dotnet .\Out\GameDataGenerator\Release\GameDataGenerator.dll --self-test
```

`dotnet run` 앞부분은 .NET 실행 옵션이고 `--` 뒤만 생성기 인자다. 서버/profile/JSON/C# 출력 모드는 제공하지 않는다.

| 인자 | 의미 / 기본값 |
|---|---|
| `--input-root <path>` | Excel 입력 루트. 기본 `GameData/Excel` |
| `--output-root <path>` | 생성 C++ 루트. 기본 `GameData/Generated/GameData` |
| `--yaml-output-root <path>` | YAML 생성 루트. 기본 `GameData/Data`; 도구가 그 안에 `Client/`를 만든다 |
| `--validate-only` | Excel/참조/자산 등 입력 검증만 수행 |
| `--check` | 예상 생성 결과와 실제 출력 전체의 drift 검사. 불일치 시 실패하며 파일은 바꾸지 않음 |
| `--self-test` | 내장 회귀 검사. 다른 인자와 조합하지 않음 |

명시한 상대 경로는 실행 cwd 기준이다. 기본값은 도구 위치 또는 cwd의 상위에서 찾은 솔루션 루트를 기준으로 한다. `--check`와 `--validate-only`는 함께 사용할 수 없다. 종료 코드 `0`은 성공, `1`은 인자/입력/검사/게시 실패다.

개발자 프롬프트의 잘못된 `LIB` 검색 경로가 .NET 빌드에 경고를 유발하면 생성기 프로세스에만 `LIB`를 비워 실행한다. 게임 프로젝트의 생성 target은 이미 이를 처리한다. SDK/참조 어셈블리가 누락됐다는 진단은 경고 억제나 생성물 수작업 수정으로 숨기지 않는다.

## 원본과 생성물

```text
GameData/Excel/*.xlsx 및 하위 폴더
  ├─ GameData/Generated/GameData/GameDataAll.h
  ├─ GameData/Generated/GameData/Common/GameDataEnums.g.h
  ├─ GameData/Generated/GameData/<Table>/<Table>Data.g.h
  └─ GameData/Data/Client/<Table>.yaml
```

- C++ 헤더에는 구조체/KeyType/필드 기본 초기화와 Enum 정의만 둔다. 실제 행 배열, Excel 행 값 리터럴, 그 배열 기반 `Find<Table>` 함수는 없다.
- 일반 헤더와 leaf 생성 `.g.h`는 include-free다. 개별 leaf를 사용하는 `.cpp`는 필요한 표준 타입과 생성 Enum을 데이터 헤더보다 먼저 포함한다.
- 생성 `GameDataAll.h`는 예외인 **타입 전용 umbrella**다. `<array>`, `<cstdint>`, `<string>` 다음에 실제 내보낸 `Common/GameDataEnums.g.h`(있을 때), 모든 Client 테이블 `.g.h`를 테이블 이름의 Ordinal 순서로 포함한다. 경로는 Generated/GameData 루트 기준이며 기존 출력 디렉터리를 검색해 폐기된 파일을 포함하지 않는다. 테이블 추가/삭제는 정상 재생성/`--check`에 함께 반영된다.
- umbrella는 `#pragma once`로 중복 포함에 안전하며 표준/PCH 사전 포함 없이 단독 소비할 수 있다. `GameData/Generated/GameData`를 include 검색 경로에 추가하고 `.cpp`에서 `#include "GameDataAll.h"`를 사용한다. 런타임 Store/로더/고정 콘텐츠 헤더, 행 값, PCH를 포함하지 않으며 별도 `GameData.h` alias는 만들지 않는다. 변경이 잦은 생성 umbrella와 Enum/테이블은 PCH에 넣지 않는다.
- 발행기 단위 경계에서 Enum이 없으면 Enum include를 생략하고, 빈 emitted set이면 표준 전제만 있는 umbrella를 만든다. 비 Client 테이블과 Ignore 필드는 내보낸 집합에 들어가지 않는다. 이는 CLI의 필수 Enums·비어 있지 않은 행·Client 전용 입력 검증을 완화하지 않는다.
- 실제 행은 Client YAML에만 있다. 별도 Schema 폴더, `.schema.yaml`, Enum 데이터 YAML은 생성하지 않는다.
- YAML 첫 줄은 정확히 `# TextRPG.GameData 1`이다. 이후에는 double-quoted root/field mapping과 문자열·숫자·bool·null을 출력한다. 이 데이터 프로토콜과 암호화 패키지 파일형식 버전은 별개다.
- Excel의 필드/타입/필수/Enum/중복/참조 검증은 그대로 유지한다. Scene의 정적 필드는 `SceneId`, `SceneType`, `IsStartupScene` 세 개다. 폐기한 `Scene.AssetPath`와 `GameData/Asset/Scene` 배치 txt는 사용하지 않는다. `TextArt.AssetPath`와 자산 경로·내용 검증은 유지한다. 현재 대상의 아트/이름/상태를 Scene 컬럼으로 추가하지 않는다.
- 런타임은 현재 Scene/SceneOption/SceneTypeConfig/Localization 및 선택적 TextArt의 명시적인 C++ decoder/validator를 사용한다. 새 테이블·필드·Enum 이름 등 구조를 변경하면 런타임 decoder/검증과 데이터 버전 호환 정책도 함께 변경해야 한다. 생성기 성공만으로 새 구조의 런타임 지원을 보장하지 않는다.

## 출력 보호

두 출력 루트는 **생성기 소유 디렉터리**이며 수동 파일 보관 장소가 아니다. 입력과 출력, 두 출력 서로가 물리적으로 같거나 포함 관계이면 거부한다. 로컬 일반 경로만 지원하며 UNC/device/extended 경로와 잘못된 구성요소를 거부한다. 같은 출력 트리의 동시 생성은 지원하지 않는다.

모든 검증과 staging 뒤 두 루트를 게시한다. 게시 전 실패 시 이전 출력을 복원하며, 두 루트 게시 후 백업 정리 실패는 committed 경고와 잔여 경로를 보고한다. 프로세스/전원 중단까지 보장하는 전역 원자적 트랜잭션은 아니다. 생성물과 소유권 마커를 임의로 편집·삭제하지 않는다. 폐기된 소유 파일은 정상 재생성으로 정리한다.

## InGameData 구조 YAML 모드

정적 콘텐츠 값은 계속 Excel 원본이다. 이 모드는 **진행 객체의 구조만** 입력으로 받으며 Client YAML/Excel/아트/Custom 소스를 변경하지 않는다. 실제 세이브는 Core 바이너리 아카이브가 담당하고 스키마 YAML은 패키지에 넣지 않는다.

```powershell
# 별도 구조 입력 → 별도 소유 출력
# 제품 정의/출력의 현재 경로다. 실험에는 별도 소유 scratch 출력을 사용한다.
dotnet .\Out\GameDataGenerator\Release\GameDataGenerator.dll --ingame-schema .\GameData\InGameDataSchema\InGameData.yaml --ingame-output-root .\GameData\Generated\InGameData
# 위 명령 뒤에 --check 또는 --validate-only를 붙일 수 있다.
# 독립 컴파일 fixture: --ingame-namespace TextRPGTest::InGameData
```

| 인자 | 의미 |
|---|---|
| `--ingame-schema <file>` | 단일 UTF-8 YAML 구조 입력. 이 인자가 없으면 기존 Excel 모드 |
| `--ingame-output-root <path>` | 기본 `GameData/Generated/InGameData`; 정적 출력과 분리 |
| `--ingame-namespace <qualified>` | 기본 `GameData::InGameData`; `::`로 구분한 검증된 C++ 식별자만 허용. 키워드/예약 이름/빈 segment/임의 C++ 거부 |

구조 모드의 `--input-root`는 기존 의미 그대로 Excel 입력 루트이며 enum 참조가 있을 때 `Enums` catalog를 검증·조회한다(기본 `GameData/Excel`). enum 없는 스키마는 Excel 없이도 독립 실행된다. 정적 출력 인자 `--output-root`, `--yaml-output-root`는 혼합하지 않는다. 구조 모드의 `--validate-only`는 출력 부모도 만들지 않는다. `--check`는 예측 파일 전체와 원시 바이트를 비교하고 실패 시 기존 출력을 보존한다.

```yaml
schema_version: 2
types:
  - name: SCharacter
    fields:
      - {name: m_label, type: string, missing: reject, since: 1}
  - name: SRoot
    fields:
      - name: m_characters
        type: 'vector<map<string, shared_ptr<SCharacter>>>'
        missing: default
        since: 2
        default: []
```

- YamlDotNet **16.3.0**(MIT)을 고정 참조한다. 기존 NuGet 캐시에 있던 버전을 NuGet 공식 버전 등록 API로 확인했다. Core event parser로 실제 block/flow YAML·주석·인용 scalar를 읽는다. JSON 파서 대용이 아니며 자동 객체 생성/태그 타입 로딩은 사용하지 않는다.
- 허용 metadata는 루트 `schema_version/types`, 타입 `name/fields`, 필드 `name/type/missing/since/default`뿐이다. 필수 항목·중복 YAML 키·타입명/필드명·Windows 대소문자 파일 충돌·미등록 참조·키워드/예약 이름을 위치 있는 `IGD1001` 진단으로 거부한다. YAML alias/anchor/명시 tag/다중 문서는 지원하지 않고 거부한다.
- 기본형 `bool/int32/uint32/int64/uint64/float/double/string`을 지원한다(`string` → `std::wstring`). 타입을 AST로 파싱하며 `vector<T>`, `map<K,V>`, `unordered_map<K,V>`의 원소/값을 재귀 지원한다. 생성 객체는 값·`unique_ptr<T>`·`shared_ptr<T>`로 선언할 수 있다. 스마트 포인터 대상은 생성 객체다. Map 키는 정수, string 또는 검증된 enum을 지원한다.
- `enum<SceneType>`처럼 기존 Excel `Enums.EnumName`을 명시한다. 이름은 대소문자를 정확히 비교하며 Client/Shared 대상만 `::GameData::Common::ESceneType`에 바인딩한다. enum 값 정의·게임 초기값을 진행 YAML에 추가하지 않는다. 기존 workbook parser/catalog가 이름·명시 수치·중복·underlying 범위를 검증하며 알려지지 않은 참조는 YAML 파일/행/열, 잘못된 catalog 값은 workbook/시트/cell의 정확 진단으로 거부한다. `uint8/uint16/int32/uint32`의 기존 정적 enum 정의/출력은 변경하지 않는다.
- enum은 scalar, 재귀 컨테이너 원소/값, map/unordered_map 키에서 사용할 수 있다. 집계만 `Common/GameDataEnums.g.h`를 포함하고 `TextRPG::TSerializationEnumTraits<E>`의 `kTypeName = "EName"` 및 `IsValid(E) noexcept`를 생성한다. identity는 Core가 허용하는 이름 토큰이며 숫자 ID가 아니다. 타입별 guard는 스키마 namespace와 무관해 같은 enum을 여러 집계에서 사용해도 specialization이 중복되지 않는다. include-free 타입 헤더와 PCH 외부 구체 의존성을 유지한다. 실제 binary codec/잘못된 enum 수치 거부는 Core API가 담당한다.
- Raw/weak/reference/aliasing 선언 및 임의 STL/C++ 타입은 거부한다. shared 객체의 null/반복 참조 identity 복원과 실제 owning cycle/관측된 소유 충돌 검사는 Core 아카이브 책임이다. 지원 범위는 완전 객체의 비순환 소유 그래프/DAG이며, 단일 포인터에 숨은 aliasing provenance까지 보편적으로 검출하지는 못한다. 포인터를 통과하지 않는 값/컨테이너 의존 순환은 생성 단계에서 거부한다.
- `since`는 양수이고 현재 `schema_version`을 넘을 수 없다. `missing: reject`는 항상 필수다. `missing: default`도 같은/이후 버전에서는 필수이며 실제 아카이브 버전이 `since`보다 작을 때만 기본값을 허용한다. 선택 default는 타입에 맞는 중립값 `0`, 실수의 `0.0`, `false`, 빈 문자열, `null`, `[]`, `{}`만 받는다. 생략하면 타입 중립값이다. `missing: reject`와 default를 조합하지 않는다. 새 게임 HP/재화 등 콘텐츠 초기값은 Excel에서 가져온다.
- 입력은 최대 1 MiB, YAML 깊이 64/노드 20,000/scalar 4,096자, 타입 AST 깊이 32, 타입 256개/타입별 필드 256개로 제한한다. 파일·행·열과 타입 토큰 offset을 진단에 포함한다.
- 타입별 include-free `.g.h`, `GameData/pch.h`가 첫 include인 `.g.cpp`, `InGameDataRegistry.g.cpp`, 위상 정렬한 `InGameDataAll.h`를 생성한다. 집계에 `kSchemaVersion`과 `RegisterInGameDataTypes`를 선언한다. 생성 타입은 non-final이고 final 타입명/필드 codec 및 상속 가능한 hook 계약을 유지한다. 소멸자/이동 연산은 out-of-line이며 `unique_ptr` 조합은 move-only다. 필드순 편의 생성자와 기반 클래스를 비교하지 않는 멤버 equality를 제공한다(스마트 포인터 equality는 포인터 정체성).
- enum scalar의 중립 default는 `0`이며 `missing: default`를 쓰려면 catalog에 실제 0 값이 있어야 한다(명시 default 생략 시도 동일). enum 이름/값을 YAML default로 정의하지 않는다.
- `FInGameSchemaGenerator.Parse(file, inputRoot = null)`/`Generate(schema, namespaceName = "GameData::InGameData")`가 내부 생성 API다. enum 사용 시 Parse에 Excel 루트를 공급해야 하며 바인딩되지 않은 enum AST는 생성하지 않는다. namespace는 CLI/API 양쪽에서 검증한다. 변경 이력/호환 lock은 만들지 않으므로 과거 이름·타입 변경을 자동 탐지한다고 보장하지 않는다.
- 구조 출력은 `.ingamedata-output`, 정적 출력은 `.gamedata-output` 소유 마커를 사용하며 서로 덮어쓰지 못한다. 기존 `FOutputManager`의 물리 경로/재분석 지점 검증·staging·rollback·committed cleanup 정책을 재사용한다. 같은 출력은 이동하지 않고 변경된 루트를 게시할 때도 내용이 같은 파일의 mtime을 보존한다. 같은 물리 트리의 동시 변경은 지원하지 않는다.

## 현재 제품 연결과 실행 근거 — 2026-11-16

- 기존 `GameData/Class` 전체13파일(소유 마커 포함)을 **바이트 그대로** `GameData/Generated/GameData`로 이동했다. 진행 생성물은 `GameData/Generated/InGameData`, 구조 원본은 `GameData/InGameDataSchema/InGameData.yaml`이다. 정적 Excel 값/Client YAML과 진행 구조 YAML을 혼동하지 않는다.
- 제품 `.g.cpp`/registry CPP는 **GameData.lib만 컴파일·소유**한다. 생성 뒤 동적 ClCompile 항목을 갱신해 누락 출력/새 타입 첫 빌드에도 실제 컴파일한다. 일반 게임/Packer/native는 lib를 링크하고 optional/altered-generated fixture의 별도 원본 variant는 유지한다. Custom CPP는 TextRPG가 소유한다. 생성 타입/umbrella는 안정 PCH에 넣지 않는다.
- 게임은 private 생성 `SRootInGameData` 하나를 가변 진행 원본으로 직접 사용한다. 생성 하위 alias·실제 shared 캐릭터·Custom hook/factory를 연결하며 별도 저장 DTO는 없다. IGD1 v1 read-only→CGSA wire1 쓰기, since 기반 추가/삭제 호환, 인증된 미래 버전 슬롯 Save/NewGame 거부를 구현·검증했다.
- 근거 루트 `C:/Users/ahn02/AppData/Local/hermes/cache/scratch/ingamedata-41751db1/`의 `build/current-final-grid.json`은 네 구성 solution Build/native 분석 경고0과 각 Core28/model137/Scene94 checkpoint groups·실제 앱 일반23/진행5를 구분한다. model137은 고유64라벨의 반복 checkpoint occurrences다. 모드 간 missing-package가 겹치므로 앱 case를 고유28개로 합산하지 않는다. Debug/x64 fixed-update/frame-skip 각5와 순수 screen control21은 `app/Debug-x64-final-modes.json`의 별도 범위다.
- 최초 정적 출력 누락 optional 실제빌드는 `native/optional-first-missing-preservation.json`, 별도 스키마 missing→add→remove 실제 컴파일/삭제와 안정 PCH bytes/mtime 보존은 `build/dynamic-types-first-add-remove.json`을 따른다. `native/model-warm-final/results.json`은 compiledObjects0/PCH 재생성0·생성물 bytes/mtime/EXE 불변이며 runtime 승계/새 분석 미실행이다.
- 독립 coverage는 generator-v1 원본 검토 불변8파일 + generator-v2 변경2파일 집중 재검토의 조합이다. Core/통합의 별도 coverage와 최초 FAIL/BLOCKED도 보존한다. 전체 신규 전수 재검토/원격 승인이 아니다. 상세 범위·제한은 [012 §11](../../Docs/GameDesign/012_InGameDataYaml정의및자동생성설계안.md#11-구현검증-상태-관리)을 따른다. 문서 갱신에서는 빌드/코드 감사/추가 재검토를 반복하지 않았으며 publication·커밋·푸시는 없다.

## 회귀 검사

```powershell
python .\Tools\Tests\test_generated_header_consumer.py --generator <절대경로-GameDataGenerator.dll> --scratch-root <절대경로-소유-scratch>
python .\Tools\Tests\test_scene_generator_contract.py --generator <절대경로-GameDataGenerator.dll> --scratch-root <절대경로-소유-scratch>
```

첫 번째 스크립트는 기존 Enum 전용/Enum 없는 테이블/std 이름 가림 개별 leaf 소비자 3개 번역 단위를 유지하고, `GameDataAll.h`만 두 번 포함하는 독립 번역 단위를 추가로 MSVC C++20 `/W4 /WX /analyze` 컴파일·실행한다. 이 direct-cl `/Y-` 경로는 standalone include 경계 검사이며 제품/일반 테스트의 PCH·증분 빌드를 대체하지 않는다. 로그 디렉터리의 `commands.json`에 실제 명령·종료 코드·로그 SHA256을 남기며 모든 검사가 통과한 뒤 `results.json`을 기록한다. `--vcvarsall <절대경로>`로 툴체인을 명시할 수 있다.

두 번째 스크립트에는 openpyxl이 설치된 Python이 필요하다. 현재 수용 상태와 검증 범위는 [로드맵](../../Docs/ROADMAP.md)과 [데이터 런타임 설계](../../Docs/GameDesign/006_YAML내보내기이식설계안.md)를 따른다.
