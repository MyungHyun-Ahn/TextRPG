# TextRPG

> **게임 콘텐츠 개발 연습을 위한 다양한 장르의 콘텐츠를 TextRPG에 붙여보자!**

Windows 콘솔에서 게임 콘텐츠를 구현하고 실험하는 C++20 프로젝트입니다.
텍스트 RPG를 출발점으로 다양한 장르의 규칙과 시스템을 붙여 보면서, 콘텐츠 코드와 공통 게임 기반을 분리하고 데이터 중심으로 개발하는 과정을 연습합니다.

현재는 콘솔 UI·입력·Scene 기반, Excel 데이터 생성·로딩, 기본 진행 상태와 저장·복원을 갖추고 콘텐츠를 확장하는 단계입니다. 다양한 장르의 콘텐츠가 모두 구현된 완성 게임은 아닙니다.

## 바로가기

| 항목 | 링크 |
|---|---|
| 전체 문서 | [Docs 폴더](Docs/) |
| 작업 현황·로드맵 | [ROADMAP](Docs/ROADMAP.md) |
| 게임 기획 | [게임 설계](Docs/GameDesign/001_GameDesign.md) · [설계 문서 모음](Docs/GameDesign/) |
| 데이터 생성 도구 | [GameDataGenerator 사용법](Tools/GameDataGenerator/README.md) |
| 데이터 패킹 도구 | [GameDataPacker 사용법](Tools/GameDataPacker/README.md) |
| 공통 게임 코어 | [ConsoleGameCore 설명](ConsoleGameCore/README.md) |
| 게임 데이터 라이브러리 | [GameData 설명](GameData/README.md) |
| 테스트 | [검증 도구·실행 방법](Tools/Tests/README.md) |
| 코딩 규칙 | [공통 컨벤션](Docs/Conventions/common.md) · [C++ 컨벤션](Docs/Conventions/cpp.md) · [커밋 메시지](Docs/Conventions/common.md#커밋-메시지) |

## 개발 환경

- **플랫폼:** Windows / Win32 콘솔
- **게임·라이브러리:** C++20
- **IDE·컴파일러:** Visual Studio 2026 / MSVC v145
- **필요 구성 요소:** C++를 사용한 데스크톱 개발, Windows SDK
- **데이터 생성기:** C# / .NET 9 대상 프로젝트를 빌드·실행할 수 있는 .NET SDK 및 런타임
- **빌드 구성:** Debug / Release, x64 / Win32
  - 솔루션에서 32비트 플랫폼 이름은 `x86`, 개별 C++ 프로젝트에서는 `Win32`입니다.

## 프로젝트 구성

| 경로 | 역할 |
|---|---|
| [TextRPG/](TextRPG/) | 게임 실행 파일. 구체 Scene·UI, 콘텐츠 행동, 진행 상태·세션·저장 정책 |
| [ConsoleGameCore/](ConsoleGameCore/) | 콘텐츠 독립 공통 UI·입력·렌더링·Frame/Scene 기반 정적 라이브러리 |
| [GameData/Runtime/](GameData/Runtime/) | TextRPG 전용 데이터 조회·검증·패키지 로딩 정적 라이브러리 |
| [GameData/Excel/](GameData/Excel/) | 정적 게임 데이터·현지화의 편집 원본 |
| [GameData/Class/](GameData/Class/) | 생성 C++ 타입·Enum과 집계 헤더 `GameDataAll.h` |
| [GameData/Data/Client/](GameData/Data/Client/) | 생성된 실제 데이터 YAML |
| [GameData/Asset/](GameData/Asset/) | 텍스트 아트 원본 |
| [Tools/](Tools/) | 데이터 생성기·패커·검증 스크립트 |
| [Tests/](Tests/) | 네이티브 회귀 테스트·UI 시안 |
| [Docs/](Docs/) | 기획·설계·개발 규칙·로드맵 |

## 빌드·실행

1. 개발 환경을 준비하고 [TextRPG.sln](TextRPG.sln)을 Visual Studio 2026에서 엽니다.
2. `TextRPG`를 시작 프로젝트로 지정하고 구성을 선택합니다. 처음에는 `Debug / x64`를 권장합니다.
3. 솔루션을 빌드한 뒤 실행합니다.

빌드 시 Excel 검증·C++ 타입/Enum 및 YAML 생성, 라이브러리 연결, `GameData.dat` 패킹을 수행합니다.
기본 `Debug / x64` 실행 파일은 `Out/TextRPG/Debug/x64/TextRPG.exe`에 생성되며, 같은 폴더의 `GameData.dat`를 사용합니다.

```text
Excel 원본
  → GameDataGenerator: C++ 타입·Enum / Client YAML
  → GameDataPacker: YAML·텍스트 아트를 GameData.dat로 패킹
  → TextRPG: GameData 런타임으로 로딩·검증 후 콘텐츠에서 사용
```

정적 데이터는 Excel에서 편집하고 생성 파일은 직접 수정하지 않습니다. 생성 타입·Enum은 필요한 `.cpp`에서 `GameDataAll.h`로 함께 포함할 수 있으며, 이 집계 헤더는 PCH에 넣지 않습니다.

개발 키는 로컬에서 준비하며 저장소에 올리지 않습니다. 키 공급과 직접 도구 실행 방법은 [Generator](Tools/GameDataGenerator/README.md)와 [Packer](Tools/GameDataPacker/README.md) 문서를 참고하세요.

## 개발 방향

- 공통 기반은 라이브러리에, 게임별 규칙과 화면은 콘텐츠 프로젝트에 둡니다.
- 정적 정의는 Excel로, 플레이 중 변화하는 상태는 `InGameData`로 관리합니다.
- 작은 콘텐츠를 실제로 붙여 보며 필요한 시스템을 확장합니다.

세부 구현 현황과 다음 작업은 [로드맵](Docs/ROADMAP.md)에서 확인할 수 있습니다.
