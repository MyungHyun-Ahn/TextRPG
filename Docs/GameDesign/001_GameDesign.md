# TextRPG 게임 기획 및 기반 설계

> 상태: 기반 구현 진행 중. 구현된 런타임·Generator 결정과 남은 현지화 범위는 §8을 참조한다.

## 1. 목표

텍스트 기반 RPG를 위한 확장 가능한 Windows 콘솔 클라이언트 기반을 마련한다. 게임 내 문구를 Excel 원본으로 관리하고 한국어·영어를 지원하며, 장면·렌더·객체 기반을 분리해 콘텐츠와 기능을 확장하기 쉽게 한다.

## 2. 1차 범위

1. **현지화 문자열**: 플레이어에게 보이는 모든 게임 문구를 `GameData/Excel`에서 관리하고 한국어(`ko-KR`)와 영어(`en-US`) 값을 제공한다. 코드와 게임 데이터는 문자열 키를 사용한다.
2. **장면 시각 자산**: `GameData/Asset/Scene`의 txt는 ASCII 아트와 비언어적 화면 배치만 담는다. 장면 제목, 메뉴, 상태 레이블, 대화, 선택지 등 읽을 수 있는 문구는 Excel 문자열 데이터에서 가져온다.
3. **SceneManager**: 시각 자산 로드·검증과 장면 생명 주기·전환을 관리한다. Excel의 Scene/문구 데이터는 생성된 게임 데이터 접근 계층을 통해 소비한다.
4. **게임 데이터 생성**: `Tools/GameDataGenerator`는 Client 전용 C++ 헤더와 YAML 데이터·스키마를 생성한다. 서버, JSON, C# 산출은 제공하지 않는다. YAML 묶음은 별도 C++ 패커가 암호화하고 런타임 패키지 로더가 메모리에서 읽는다.
5. **화면 출력**: Windows wide API `WriteConsoleOutputW`와 더블 버퍼링으로 프레임을 출력한다. 런타임 문자열과 프레임 버퍼 문자는 `std::wstring`/`wchar_t`로 표현하고, 문자열 렌더링은 현재 선택 언어의 생성 데이터에서 가져온다.
6. **확장 가능한 객체 구조**: `IObject` 등 인터페이스를 활용하되, 객체 공통 역할과 공격·이동 등 선택 기능을 구분한다.

## 3. 책임과 데이터 흐름

### 문자열·현지화 데이터

`GameData/Excel`이 플레이어 노출 문자열의 원본이다. 각 문구는 안정적인 문자열 키를 가지며 `ko-KR`, `en-US` 열을 모두 채운다. 게임 데이터의 이름·설명·대사·메뉴 텍스트는 문장을 직접 복제하지 않고 문자열 키로 참조한다. 런타임은 사용자 선택 언어로 문구를 조회하고, 구체적인 시트 형식·키 규칙·누락 정책은 `003_다국어문자열및Excel관리원칙.md`를 따른다.

### 장면

`GameData/Asset/Scene/*.txt`는 ASCII 아트 같은 비언어적 시각 표현의 원본이다. 파일에 의미 있는 단어를 장면 문구로 넣지 않는다. Scene 데이터의 제목·레이블·선택지 등은 Excel에서 키로 정의하며, SceneManager와 렌더러가 키를 통해 현재 언어의 문구를 표시한다. 잘못된 파일은 파일명과 줄 등 원인을 추적할 수 있는 진단을 제공해야 한다.

### 게임 데이터 생성

`GameData/Excel`의 현지화와 게임 규칙 표를 검증한 뒤 두 언어를 포함하는 C++ 산출물을 만든다. 생성물은 직접 수정하지 않는다. 키 중복, 필수 번역 누락, 참조 오류는 입력 위치를 표시하고 생성 실패로 처리한다. Generator는 클라이언트 전용으로 고정한다.

### 화면과 객체

Scene과 게임 객체는 문자열을 직접 소유하거나 출력하지 않고, 렌더 요청에 문자열 키와 시각 상태를 전달한다. 렌더러가 현재 언어의 생성 문자열 데이터를 조회하고 백 버퍼를 구성한 뒤 표시 버퍼와 비교·교체해 콘솔에 출력한다. Scene이나 개별 객체가 직접 화면을 지우거나 문구를 하드코딩하지 않도록 한다.

## 4. 기존 프로젝트 재사용 원칙

`C:\PROCADEMEY\course1\과제\OOP_TextShootingGame`의 `BaseObject`, `ConsoleManager`, `Core`, `FileManager`, `GameScene` 등을 재사용 후보로 본다. 파일을 그대로 복사하기 전에 책임, 소유권, 업데이트·렌더 순서, 버퍼 처리, Scene 전환 결합도를 확인한다. RPG에 맞지 않는 전역 상태나 게임 전용 동작은 분리하거나 재설계한다.

현재 `MainMenu.txt`의 브랜드 ASCII 타이틀은 한국어·영어 선택과 관계없이 `TextRPG`로 통일한다. TextRPG 런타임과 Generator의 실제 변경 상태는 §8에 기록한다.

## 5. 프로젝트 배치 기준

- 현지화 및 게임 규칙의 Excel 원본은 `GameData/Excel` 아래에 둔다.
- `GameData/Asset/Scene`에는 ASCII 아트와 비언어적 화면 시각 자산만 둔다.
- 소스는 `TextRPG` 프로젝트에 두고, 생성기는 `Tools/GameDataGenerator`에서 관리한다.
- C++ 게임 데이터 헤더는 기본 `GameData/Class`, YAML 데이터·스키마는 `GameData/Data/Client` 및 `GameData/Data/Schema`에 둔다. 원본은 `GameData/Excel`이며 생성물은 직접 편집하지 않는다. 컴파일 결과와 중간 파일은 기존처럼 `Out`에 둔다.
- C++ 실제 디렉터리와 Visual Studio 필터를 일치시킨다.

## 6. 구현 전 결정할 항목

- 현지화 Excel 시트·열, 문자열 키 명명·안정성, 포맷 인자와 줄바꿈 규칙
- 기본 언어는 한국어로 하고 설정에서 영어로 변경한다. 설정 보존 방식과 누락 시 한국어 fallback 진단 세부 동작을 구현 설계에서 정한다.
- C++ 현지화 문자열 테이블 접근 방식, Unicode 코드 포인트와 한글·전각 문자 화면 폭 처리
- ASCII Scene 파일 문법·인코딩·검증, Scene 데이터와 문자열 키의 연결 방법
- Scene 식별자, 로드 실패 시 동작, 전환 방식과 생명 주기
- 더블 버퍼의 콘솔 API, 화면 크기 변경, 출력 최적화 기준
- `IObject` 최소 계약과 소유권, Scene별 객체 관리 방식
- Generator의 클라이언트 전용 C++ 산출물(파일, 네임스페이스, ID 및 참조 규칙)
- 기존 슈팅 게임에서 가져올 코드와 RPG용으로 다시 설계할 경계

## 7. 진행 순서

1. 현지화 데이터 스키마와 키·번역 검증 규칙을 확정한다.
2. 기존 `GameDataGenerator`의 CLI, 입력 스키마, 생성물, SelfTest를 대조한다.
3. Scene ASCII 아트와 플레이어 노출 문구를 분리하고, 슈팅 게임의 객체·Scene·파일·콘솔 관리 코드를 검토한다.
4. Scene/문구 데이터 계약, 렌더링, 객체 수명 주기를 설계한다.
5. 합의된 기반 구조를 구현하고 Generator 및 프로젝트 검증 절차로 확인한다.

## 8. 구현 현황 및 남은 작업

- 프레임 버퍼·Text Scene은 `std::wstring`을 사용하고 콘솔 출력은 `WriteConsoleOutputW`로 처리한다. Scene txt는 ASCII 아트·비언어 레이아웃만 허용한다.
- MainMenu ASCII 타이틀 표기는 모든 언어에서 `TextRPG`로 고정한다.
- 메인 메뉴는 위·아래 화살표로 항목을 선택하고 Enter로 실행한다. `New Game` 선택은 `GameData/Asset/Scene/Town.txt`를 불러오며, `Exit` 또는 Esc로 종료한다. Continue/Settings/Credits 동작은 아직 구현하지 않았다.
- Generator는 Client 대상만 받아 C++ `.g.h`, Client YAML, Enum·테이블 스키마 YAML을 생성하고 C++ 문자열 필드는 `std::wstring`으로 만든다. Server 대상 데이터는 검증 오류로 거부하며 서버/JSON/C# 생성 모드는 없다. 패키징, 형식, 로더 API는 `006_YAML내보내기이식설계안.md`를 따른다.
- `GameData/Excel/Templates/GameDataTableTemplate.xlsx` 템플릿은 추가됐다. 현지화 데이터를 담은 원본 workbook, 두 언어 필수값 및 문자열 키 검증, Scene 문구 이관, 런타임 언어 선택·키 조회는 아직 완료되지 않았다. 한글·전각 문자의 화면 칸 폭 처리도 확정할 필요가 있다.

## 9. UI 및 화면 해상도 설계 제안

UI Manager, MainMenu/Town/Battle 공통 하단 선택 패널, 1920×1080 화면 요구를 묶은 설계 초안은 [004_UIManager및화면해상도설계안.md](004_UIManager및화면해상도설계안.md)를 참조한다. 제안은 사용자 검토 대기 중이며 런타임에 반영되지 않았다.
