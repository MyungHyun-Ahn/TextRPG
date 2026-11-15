# GameDataGenerator

Excel을 유일한 게임 데이터 편집 원본으로 사용하는 Client 전용 생성기다. Windows, .NET 9와 프로젝트에 지정한 ClosedXML을 사용한다. 아래 명령은 **TextRPG.sln이 있는 솔루션 루트**에서 실행한다.

## 게임 빌드로 사용하기

Visual Studio 2026에서 TextRPG를 빌드하면 생성기가 컴파일 전에 실행된다. 저장한 Excel 검증 → 타입 헤더/Client YAML 생성 → 게임 컴파일 → Packer 준비 → exe 옆 `GameData.dat` 생성 순서다. 솔루션과 TextRPG 개별 프로젝트 빌드 모두 지원한다.

- C++ 플랫폼: `x64` 또는 `Win32` / 솔루션의 32비트 플랫폼 이름: `x86`
- 구성: `Debug`, `Release`
- 최종 데이터 예: `Out/TextRPG/Debug/x64/GameData.dat`
- 암호화·키 공급·직접 패킹 방법: [GameDataPacker README](../GameDataPacker/README.md)

Excel 편집 내용은 저장한 후 빌드한다. 생성기는 workbook을 편집하거나 열린 Excel을 강제로 닫지 않는다. 저장 중이거나 다른 프로세스가 파일을 독점하고 있으면 먼저 해당 작업을 끝낸다.

## 생성기를 직접 실행하기

```powershell
dotnet run --project .\Tools\GameDataGenerator\GameDataGenerator.csproj --configuration Release -- --input-root .\GameData\Excel --output-root .\GameData\Class --yaml-output-root .\GameData\Data
```

이미 빌드된 도구로 검증/생성할 수도 있다.

```powershell
# 원본 검증만: 생성물 게시/정리하지 않음
dotnet .\Out\GameDataGenerator\Release\GameDataGenerator.dll --validate-only --input-root .\GameData\Excel

# 생성 결과가 현재 원본과 일치하는지 확인: 읽기 전용
dotnet .\Out\GameDataGenerator\Release\GameDataGenerator.dll --check --input-root .\GameData\Excel --output-root .\GameData\Class --yaml-output-root .\GameData\Data

# 내장 정상/실패/경계 회귀 검사
dotnet .\Out\GameDataGenerator\Release\GameDataGenerator.dll --self-test
```

`dotnet run` 앞부분은 .NET 실행 옵션이고 `--` 뒤만 생성기 인자다. 서버/profile/JSON/C# 출력 모드는 제공하지 않는다.

| 인자 | 의미 / 기본값 |
|---|---|
| `--input-root <path>` | Excel 입력 루트. 기본 `GameData/Excel` |
| `--output-root <path>` | 생성 C++ 루트. 기본 `GameData/Class` |
| `--yaml-output-root <path>` | YAML 생성 루트. 기본 `GameData/Data`; 도구가 그 안에 `Client/`를 만든다 |
| `--validate-only` | Excel/참조/자산 등 입력 검증만 수행 |
| `--check` | 예상 생성 결과와 실제 출력 전체의 drift 검사. 불일치 시 실패하며 파일은 바꾸지 않음 |
| `--self-test` | 내장 회귀 검사. 다른 인자와 조합하지 않음 |

명시한 상대 경로는 실행 cwd 기준이다. 기본값은 도구 위치 또는 cwd의 상위에서 찾은 솔루션 루트를 기준으로 한다. `--check`와 `--validate-only`는 함께 사용할 수 없다. 종료 코드 `0`은 성공, `1`은 인자/입력/검사/게시 실패다.

개발자 프롬프트의 잘못된 `LIB` 검색 경로가 .NET 빌드에 경고를 유발하면 생성기 프로세스에만 `LIB`를 비워 실행한다. 게임 프로젝트의 생성 target은 이미 이를 처리한다. SDK/참조 어셈블리가 누락됐다는 진단은 경고 억제나 생성물 수작업 수정으로 숨기지 않는다.

## 원본과 생성물

```text
GameData/Excel/*.xlsx 및 하위 폴더
  ├─ GameData/Class/GameDataAll.h
  ├─ GameData/Class/Common/GameDataEnums.g.h
  ├─ GameData/Class/<Table>/<Table>Data.g.h
  └─ GameData/Data/Client/<Table>.yaml
```

- C++ 헤더에는 구조체/KeyType/필드 기본 초기화와 Enum 정의만 둔다. 실제 행 배열, Excel 행 값 리터럴, 그 배열 기반 `Find<Table>` 함수는 없다.
- 일반 헤더와 leaf 생성 `.g.h`는 include-free다. 개별 leaf를 사용하는 `.cpp`는 필요한 표준 타입과 생성 Enum을 데이터 헤더보다 먼저 포함한다.
- 생성 `GameDataAll.h`는 예외인 **타입 전용 umbrella**다. `<array>`, `<cstdint>`, `<string>` 다음에 실제 내보낸 `Common/GameDataEnums.g.h`(있을 때), 모든 Client 테이블 `.g.h`를 테이블 이름의 Ordinal 순서로 포함한다. 경로는 Class 루트 기준이며 기존 출력 디렉터리를 검색해 폐기된 파일을 포함하지 않는다. 테이블 추가/삭제는 정상 재생성/`--check`에 함께 반영된다.
- umbrella는 `#pragma once`로 중복 포함에 안전하며 표준/PCH 사전 포함 없이 단독 소비할 수 있다. Class를 include 검색 경로에 추가하고 `.cpp`에서 `#include "GameDataAll.h"`를 사용한다. 런타임 Store/로더/고정 콘텐츠 헤더, 행 값, PCH를 포함하지 않으며 별도 `GameData.h` alias는 만들지 않는다. 변경이 잦은 생성 umbrella와 Enum/테이블은 PCH에 넣지 않는다.
- 발행기 단위 경계에서 Enum이 없으면 Enum include를 생략하고, 빈 emitted set이면 표준 전제만 있는 umbrella를 만든다. 비 Client 테이블과 Ignore 필드는 내보낸 집합에 들어가지 않는다. 이는 CLI의 필수 Enums·비어 있지 않은 행·Client 전용 입력 검증을 완화하지 않는다.
- 실제 행은 Client YAML에만 있다. 별도 Schema 폴더, `.schema.yaml`, Enum 데이터 YAML은 생성하지 않는다.
- YAML 첫 줄은 정확히 `# TextRPG.GameData 1`이다. 이후에는 double-quoted root/field mapping과 문자열·숫자·bool·null을 출력한다. 이 데이터 프로토콜과 암호화 패키지 파일형식 버전은 별개다.
- Excel의 필드/타입/필수/Enum/중복/참조 검증은 그대로 유지한다. Scene의 정적 필드는 `SceneId`, `SceneType`, `IsStartupScene` 세 개다. 폐기한 `Scene.AssetPath`와 `GameData/Asset/Scene` 배치 txt는 사용하지 않는다. `TextArt.AssetPath`와 자산 경로·내용 검증은 유지한다. 현재 대상의 아트/이름/상태를 Scene 컬럼으로 추가하지 않는다.
- 런타임은 현재 Scene/SceneOption/SceneTypeConfig/Localization 및 선택적 TextArt의 명시적인 C++ decoder/validator를 사용한다. 새 테이블·필드·Enum 이름 등 구조를 변경하면 런타임 decoder/검증과 데이터 버전 호환 정책도 함께 변경해야 한다. 생성기 성공만으로 새 구조의 런타임 지원을 보장하지 않는다.

## 출력 보호

두 출력 루트는 **생성기 소유 디렉터리**이며 수동 파일 보관 장소가 아니다. 입력과 출력, 두 출력 서로가 물리적으로 같거나 포함 관계이면 거부한다. 로컬 일반 경로만 지원하며 UNC/device/extended 경로와 잘못된 구성요소를 거부한다. 같은 출력 트리의 동시 생성은 지원하지 않는다.

모든 검증과 staging 뒤 두 루트를 게시한다. 게시 전 실패 시 이전 출력을 복원하며, 두 루트 게시 후 백업 정리 실패는 committed 경고와 잔여 경로를 보고한다. 프로세스/전원 중단까지 보장하는 전역 원자적 트랜잭션은 아니다. 생성물과 소유권 마커를 임의로 편집·삭제하지 않는다. 폐기된 소유 파일은 정상 재생성으로 정리한다.

## 회귀 검사

```powershell
python .\Tools\Tests\test_generated_header_consumer.py --generator <절대경로-GameDataGenerator.dll> --scratch-root <절대경로-소유-scratch>
python .\Tools\Tests\test_scene_generator_contract.py --generator <절대경로-GameDataGenerator.dll> --scratch-root <절대경로-소유-scratch>
```

첫 번째 스크립트는 기존 Enum 전용/Enum 없는 테이블/std 이름 가림 개별 leaf 소비자 3개 번역 단위를 유지하고, `GameDataAll.h`만 두 번 포함하는 독립 번역 단위를 추가로 MSVC C++20 `/W4 /WX /analyze` 컴파일·실행한다. 이 direct-cl `/Y-` 경로는 standalone include 경계 검사이며 제품/일반 테스트의 PCH·증분 빌드를 대체하지 않는다. 로그 디렉터리의 `commands.json`에 실제 명령·종료 코드·로그 SHA256을 남기며 모든 검사가 통과한 뒤 `results.json`을 기록한다. `--vcvarsall <절대경로>`로 툴체인을 명시할 수 있다.

두 번째 스크립트에는 openpyxl이 설치된 Python이 필요하다. 현재 수용 상태와 검증 범위는 [로드맵](../../Docs/ROADMAP.md)과 [데이터 런타임 설계](../../Docs/GameDesign/006_YAML내보내기이식설계안.md)를 따른다.
