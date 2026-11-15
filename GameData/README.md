# GameData — TextRPG 전용 정적 데이터 라이브러리

`GameData.lib`는 TextRPG의 생성 타입·Enum·타입별 Store와 인증 패키지/key/generated-YAML 런타임을 제공한다. 범용 게임 데이터 프레임워크나 진행 상태 저장소가 아니다. ConsoleGameCore의 UI/Scene/프레임에는 의존하지 않는다.

## 디렉터리와 소비자

- `Excel/`: 정적 정의·고정 수치·초기값·문구/아트 매핑의 원본. 직접 코드 상수로 대체하지 않는다.
- `Class/`: 생성 C++ 타입/Enum과 자동 생성 `GameDataAll.h`. 개별 `.g.h`는 include-free이며 실제 행 배열 없음. `#include "GameDataAll.h"` 하나로 모든 생성 타입/Enum을 사용할 수 있다. 집계 헤더는 PCH에 넣지 않는다.
- `Data/Client/`: 생성 실제 데이터 YAML.
- `Asset/`: 기존 txt 아트 원시 바이트.
- `Runtime/GameData/`: package/key/generated-YAML/typed Store. 안정 SDK/표준 전제는 `GameDataPch.h`, 라이브러리의 실제 PCH 진입은 `pch.h`다. 소비 프로젝트의 PCH는 `GameData/GameDataPch.h`를 직접 포함한다.
- `GameData.vcxproj`: C++20/v145 StaticLibrary. `Runtime`와 `Class`를 소비 include 경로로 사용하므로 기존 `GameData/FGameDataStore.h` 등의 spelling을 유지한다.

일반 TextRPG와 GameDataPacker는 이 프로젝트에 ProjectReference로 연결한다. 타깃별 obj/PCH를 공유하지 않으며 Debug/Release·x64/Win32·CRT·분석 설정을 맞춘다. 소비자 전용 OutDir/IntDir/TargetName은 참조 라이브러리에 전파하지 않는다. 매크로 치환·생성 헤더 부재를 검증하는 특수 테스트는 원본 CPP variant를 유지한다.

## 빌드와 패킹

Visual Studio 개발자 명령 프롬프트에서 솔루션 루트 기준 예시:

```text
MSBuild.exe GameData\GameData.vcxproj -t:Build -p:Configuration=Debug -p:Platform=x64
MSBuild.exe TextRPG.sln -t:Build -m -p:Configuration=Release -p:Platform=x86
```

솔루션의 x86은 native 프로젝트의 Win32에 대응한다. Clean/Rebuild 대신 기존 PCH·obj·증분 Build를 유지한다. 산출물은 `Out/GameData/<Configuration>/<Platform>/GameData.lib`, 중간 파일은 해당 프로젝트의 Out/obj 아래이며 분석 캐시는 분리한다.

GameData 프로젝트가 기존 Generator 실행을 단독 소유한다. `--check` 성공 시 생성물 재게시를 생략하고, 변경 또는 오류가 있으면 기존 생성 명령을 실행한다. 최종 생성/검증 실패는 빌드 실패로 전달한다. Excel·생성 헤더/데이터·아트의 기존 경로를 유지하며 생성기를 수동 복제하지 않는다.

`GameData.lib`는 코드 링크 산출물이고 `GameData.dat`는 배포 콘텐츠다. 게임 빌드가 Packer 준비 후 Client YAML과 Asset을 각 EXE 옆 dat로 패킹한다. 인증·복호화·메모리 조회와 실패 시 기존 저장소 보존 정책은 유지한다. 상세 CLI/개발 키/실패 게시 계약은 [Packer README](../Tools/GameDataPacker/README.md)를 따른다. 키 값은 소스/문서/로그에 쓰지 않는다.

InGameData·게임 세션·구체 Scene/UI·게임 행동과 NewGame/Continue/정상 Quit 저장 시점은 TextRPG가 담당한다. 데이터 라이브러리를 링크했다는 이유로 게임 진행 루트를 이 라이브러리가 소유하지 않는다.

## 실제 확인 범위

- 네 구성 솔루션 Build 및 실제 native 분석, 각 게임 EXE의 기본/진행 저장·복원 검사를 수행했다. library 파일 존재만으로 게임 기능이나 코어 독립성을 판정하지 않는다.
- 기존 GameData 런타임 10파일은 바이트 동일하게 이동했으며 Excel/아트/생성 원본은 보존했다. 일반 링크 경로와 특수 원본 주입 테스트를 구분한다.
- 현재 세부 실행 수치·동결 소스 검토·남은 gate와 최종 수용 상태는 [010 설계/구현 기록](../Docs/GameDesign/010_게임코어및콘텐츠라이브러리분리설계안.md) 및 [로드맵](../Docs/ROADMAP.md) 상단을 따른다. 진행·도구 게시 검증과 source-only 판정을 제품 실행으로 합산하지 않는다.
