# TextRPG AI 작업 지침

이 문서는 `C:\Portfolio\devrocket\TextRPG` 및 모든 하위 디렉터리의 프로젝트 규칙이다. 작업 전에 이 문서와 [Agents/README.md](Agents/README.md)를 읽는다. 코드 작업은 [공통 컨벤션](Docs/Conventions/common.md), C++ 작업은 [C++ 컨벤션](Docs/Conventions/cpp.md)를 추가로 따른다.

## 작업 범위
- Windows C++ 콘솔 TextRPG와 기존 C# 데이터 생성 도구를 관리한다. 서버·네트워크 서버·DB 운영·Unity 지침은 적용하지 않는다.
- 이 프로젝트는 Windows 전용이므로 Win32/Windows SDK 등 Windows 종속 API와 코드를 직접 사용해도 된다. 콘솔·창 크기·입력·렌더링 등 Windows 기능을 활용할 수 있으며, 다른 OS 지원을 위한 이식성 계층은 필수가 아니다.
- 실제 구조, 프로젝트 참조, 기존 사용자 변경을 확인한 후 요청 범위만 수정한다. 소스·원본 데이터와 생성물을 구분한다.
- C++20, MSVC v145, 기존 Debug/Release 및 Win32/x64 구성을 유지한다. 기존 프로젝트 GUID와 의존성을 이유 없이 변경하지 않는다.
- 커밋·푸시·Git 초기화·파괴적 초기화는 별도 요청 없이 하지 않는다. `.gitignore` 등 추적 제외 규칙 변경은 사용자 승인 후 수행한다.
- 실제 빌드·테스트 결과와 미검증 항목을 구분해 보고한다. 비밀키·토큰·개인정보를 기록하지 않는다.

## 게임 내 문자열과 다국어
- 게임에서 플레이어에게 보이는 모든 문구는 `GameData/Excel`의 원본 데이터로 관리하고 한국어(`ko-KR`)와 영어(`en-US`) 번역을 제공한다. 메뉴, 대화, 선택지, 캐릭터·아이템 이름, 설명, 전투 메시지, 상태·오류 안내가 포함된다.
- 코드에는 표시 문자열을 하드코딩하지 않는다. 문자열 키 또는 Generator가 만든 타입 안전한 키를 사용하고, 데이터 테이블도 문구 대신 문자열 키를 참조한다.
- 사용자 승인으로 레거시 `GameData/Asset/Scene`과 `Scene.AssetPath`는 제거했다. Scene 데이터는 `SceneId`/`SceneType`/`IsStartupScene` 세 정적 필드만 둔다. Scene 전체 레이아웃 txt를 다시 도입하거나 AssetPath를 추가하지 않는다. 현재 캐릭터·몬스터·마을/지역의 아트·이름 키·상태와 로고 참조는 Scene을 이루는 런타임 UI 구성 객체의 멤버/현재 대상 바인딩으로 다룬다. 표시 필드를 Scene에 추가하거나 별도 정적 Scene 배정표로 옮기지 않는다. `TextArt`는 독립 선택적 카탈로그이며 존재하면 strict UTF-8·허용 단일 셀 문자·정확한 36×36/83×41을 검증한다. 실제 슬롯 적합성과 같은 Scene 내 몬스터/캐릭터 이미지·이름 교체는 P3 구현 대상이고, 83×41 로고는 메인메뉴 UI 슬롯/멤버 계약이다([007](Docs/GameDesign/007_Scene별UI객체및입력분리설계안.md)). 제목·레이블·버튼 문구는 Excel 현지화 데이터로 분리한다. 요청된 로고의 원본 브랜딩은 시각 자산으로 보존하되 UI 조작 문구의 원본으로 사용하지 않는다.
- 기본 언어는 한국어이며 설정에서 영어로 바꿀 수 있다. 번역 원본, 키 규칙, 누락 처리와 검증 계약은 `Docs/GameDesign/003_다국어문자열및Excel관리원칙.md`를 따른다. 개발자 전용 진단 로그는 게임 화면에 노출되지 않는 범위에서 별도 처리한다.
- Windows 전용 런타임의 문자열은 `std::wstring`/UTF-16 wide API로 처리하고 콘솔 프레임은 `WriteConsoleOutputW`로 출력한다. 코드·데이터 경계에서 게임 문구를 UTF-8 `std::string`으로 변환하지 않는다. C# `GameDataGenerator`는 Client C++ 타입·Enum 정의와 Client 실제 데이터 YAML을 생성하며 서버·JSON·C# 산출 모드를 제공하지 않는다. 별도 schema YAML은 생성·배포하지 않는다. 생성 헤더에는 실제 행 배열·값 리터럴·정적 배열 기반 Find 함수를 두지 않으며, 실제 값은 런타임 데이터 저장소에서 조회한다. 타입의 기본 초기화와 Enum 정의는 유지할 수 있다.

## 게임 데이터 원본 및 경로
- 모든 게임 플레이 데이터와 콘텐츠 설정은 `GameData/Excel`의 원본 workbook에서 관리한다. 코드, 임의 JSON/CSV/INI를 게임 규칙·수치·콘텐츠의 별도 원본으로 사용하지 않는다. Unicode TextArt는 독립 자산 계약으로 관리하고 폐기한 Scene 레이아웃 txt를 참조하지 않는다. txt에 게임 규칙이나 UI 조작 문구를 넣지 않는다.
- TextArt, 맵, 몬스터 및 기타 실제 게임 자산의 구체 경로는 해당 도메인의 Excel 필드에 상대 경로로 기록한다. 정적 Scene에는 자산 경로를 두지 않는다. 런타임 코드와 Scene 전환 로직에 자산 경로 또는 데이터 값의 리터럴을 하드코딩하지 않는다. `AssetPath`는 `GameData/` 기준으로 해석한다.
- Excel 원본은 빌드 시 GameDataGenerator가 검증하고 Client 전용 C++ 타입 헤더와 실제 데이터 YAML로 생성한다. 생성 C++ 헤더는 기본 `GameData/Class`, Client YAML은 `GameData/Data/Client`에 둔다. `GameData/Data/Schema`와 `.schema.yaml`은 생성하지 않는다. `--output-root`는 C++ 생성 루트(`Common`·테이블별 하위 폴더 포함), `--yaml-output-root`는 `Client` 하위가 있는 YAML 생성 루트를 지정한다. 두 출력 루트는 Excel 입력 및 서로와 물리적으로 겹치지 않아야 하며 생성기 소유 디렉터리로 관리한다. Generator는 로컬 일반 경로만 지원하고 UNC·device·extended 네임스페이스는 거부한다. 미존재 폴더를 포함한 각 경로 구성요소의 잘못된 문자와 Windows 예약 장치 이름도 생성 전에 거부한다. 공유 트리/드라이브 매핑 변경을 포함한 동시 생성은 지원하지 않으므로 호출자가 직렬화한다. 두 출력 게시 전 실패는 rollback하며, 둘 다 게시된 뒤 백업 정리 실패는 committed 상태와 잔여 경로를 경고로 보고하고 성공 종료한다.
- 게임 데이터 런타임 전환 계약: 빌드는 `GameData/Data/Client`만 패킹해 각 게임 exe와 같은 디렉터리에 정확히 `GameData.dat`를 생성한다. 게임은 cwd와 무관하게 exe 옆 dat를 읽고, 첫 Scene 생성 전에 인증·복호화 → 메모리 unpack → YAML 파싱·타입/필수/Enum/범위/중복/참조 검증 → 테이블 전체 게시를 마친다. `FGameDataPackageReader`의 원문 엔트리 제공과 게임 데이터 저장소의 파싱·조회 책임을 구분한다. 누락·변조·잘못된 키·지원하지 않는 버전·데이터 검증 실패 시 진입을 중단하고 Excel·평문 YAML·헤더 상수로 fallback하지 않는다. txt 아트 배포는 데이터 패키지와 별도 자산 계약이다.
- Generator는 중복 ID, 미등록 참조/문자열 키, 잘못된 Enum·수치, 필수 파일 누락과 데이터 루트 이탈 경로를 진단과 실패 종료 코드로 알려야 한다. 오류가 있으면 불완전한 생성 출력을 적용하지 않는다.
- YAML 패키지 형식과 AES-GCM 암복호화 계약은 `Docs/GameDesign/006_YAML내보내기이식설계안.md`를 따른다. 키는 코드·문서·로그에 기록하지 않는다. 실행 파일 안에 배포 키를 포함하면 추출될 수 있으므로 배포 보안의 한계를 설명한다. 엔진 알고리즘, 입력·렌더 절차와 도구 설정은 Excel 게임 콘텐츠와 구분한다.

## 빌드 산출물 경로
- 모든 컴파일 결과와 중간 파일은 솔루션 루트의 `Out` 아래에 둔다. 프로젝트 소스 폴더 안에 `bin`, `obj`, `Debug`, `Release`, `x64` 산출물 폴더를 만들지 않는다.
- 최종 출력의 기본 구조는 `Out\프로젝트명\Debug` 또는 `Out\프로젝트명\Release`다.
- C++은 서로 다른 아키텍처의 실행 파일이 덮어써지지 않도록 구성 아래에 플랫폼을 추가한다: `Out\TextRPG\Debug\x64\TextRPG.exe`, `Out\TextRPG\Debug\Win32\TextRPG.exe`. Release도 같은 구조다.
- C++ 중간 파일은 `Out\TextRPG\obj\플랫폼\구성\`에 둔다.
- C# 도구 실행 파일은 `Out\GameDataGenerator\Debug\` 또는 `Release\`에 둔다. 중간 파일과 NuGet 복원 정보는 `Out\GameDataGenerator\obj\`에 둔다.
- 패킹 도구 실행 파일은 `Out\GameDataPacker\Debug\x64\GameDataPacker.exe`, `Out\GameDataPacker\Debug\Win32\GameDataPacker.exe`에 두며 Release도 같은 구조를 사용한다. 중간 파일은 `Out\GameDataPacker\obj\플랫폼\구성\`에 둔다.
- C++ 경로는 `TextRPG/TextRPG.vcxproj`, C# 경로는 루트 `Directory.Build.props`에서 관리한다. 솔루션 빌드와 개별 프로젝트 빌드 모두 같은 경로를 사용해야 한다.
- `Out`은 빌드 도구가 관리하는 산출물 전용 폴더다. 소스·기획 문서·원본 게임 데이터를 넣지 않는다. 데이터 생성기의 입력/출력 계약은 컴파일 출력 경로 변경과 별개로 관리한다.
- 산출물 정리는 실제 절대경로가 프로젝트 내부인지 확인하고 소스·원본 데이터·링크 대상이 섞이지 않았는지 점검한 뒤 수행한다. `.vs`의 사용자 설정과 편집 상태는 빌드 산출물 정리 대상으로 취급하지 않는다.

## 실제 디렉터리와 Visual Studio 필터 일치
- **실제 디렉터리 구조와 Visual Studio의 프로젝트 필터 구조를 반드시 일치시킨다.** 물리 디렉터리를 기준으로 파일을 배치한다.
- `.vcxproj`와 `.vcxproj.filters`의 Include는 프로젝트 루트 기준의 동일한 상대경로를 사용한다.
- 프로젝트 루트의 `Main.cpp`는 필터 없이 루트에 표시한다. `Characters\FPlayer.cpp`와 `.h`는 모두 실제 폴더와 같은 `Characters` 필터에 표시한다.
- 중첩 폴더는 `Characters`, `Characters\Jobs`처럼 부모와 하위 필터를 모두 등록한다. 실제 폴더가 없는 `소스 파일`·`헤더 파일` 같은 확장자별 가상 분류는 사용하지 않는다.
- 파일 추가·이동·이름 변경·삭제 시 실제 파일, `.vcxproj`, `.vcxproj.filters`, include와 관련 `.sln` 참조를 같은 작업에서 갱신한다.
- 솔루션 폴더 역시 실제 위치와 맞춘다. `Tools/GameDataGenerator` 프로젝트는 `Tools` 솔루션 폴더에 배치한다.
- `Out`, 빌드 캐시, 중간 파일은 소스 항목이나 필터에 등록하지 않는다. 빈 데이터 폴더를 억지로 소스 항목에 추가하지 않는다.

## 검증
- **검증 주기는 작업 중 최소 확인 + 최종 후보 전체 검증 1회로 한다.** 파일 수정·작업자 인계마다 전체 빌드, 네 구성 matrix, 전체 코드 분석, 독립 검토를 반복하지 않는다.
- 작업 중에는 변경한 기능의 작은 RED/GREEN과 필요한 구조 검사만 수행한다. 공용 API·생성기·빌드 경로가 바뀌면 다른 작업의 실패를 늦게 발견하지 않도록 가장 작은 실제 연결 경로를 먼저 확인한다. 모든 검증을 마지막까지 미루는 규칙은 아니다.
- 전체 구현을 마친 후보에서 필수 빌드·분석·회귀·독립 검토를 한 묶음으로 수행한다. 결함을 보완하면 영향받은 검사만 다시 실행하고, 소스·설정·산출물 정체성이 같은 통과 기록은 승계한다. 문서·주석·집계만 바뀌면 내용·링크·정체성만 확인하며 제품 빌드는 반복하지 않는다. 자세한 절차는 `Agents/qa.md`를 따른다.
- 경로·빌드 설정 변경은 C++ Debug/Release × Win32/x64 및 전체 솔루션 빌드로 확인한다. 이전 경로에 산출물이 다시 생성되지 않는지도 점검한다.
- 기존 데이터 생성 도구의 검증은 솔루션 루트에서 `dotnet run --project .\Tools\GameDataGenerator\GameDataGenerator.csproj -- --self-test`로 실행한다.
- 기획·도구·검증 작업은 `Agents/design.md`, `Agents/tools.md`, `Agents/qa.md` 중 필요한 지침을 함께 읽는다. 별도 에이전트 생성을 필수로 하지 않는다.
