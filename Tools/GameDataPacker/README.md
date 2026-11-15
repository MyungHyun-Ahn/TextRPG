# GameDataPacker

Client YAML과 Asset 원시 바이트를 하나의 AES-256-GCM 패키지로 암호화하는 Windows C++20 콘솔 도구다. 아래 명령은 **솔루션 루트**에서 실행한다. 키 값을 명령행 인자로 받지 않으며 평문 추출 기능도 제공하지 않는다.

## 게임 빌드에서 자동 사용

Visual Studio 2026에서 TextRPG를 빌드하면 Packer 프로젝트 의존성이 먼저 준비된다. 게임 빌드 완료 뒤 `prepare-development-key`와 `pack`을 순서대로 호출하며, 입력은 `GameData/Data/Client`와 명시적 `--asset-root`의 `GameData/Asset`이다.

| 구성 / 플랫폼 | 게임 데이터 출력 |
|---|---|
| Debug / x64 | `Out/TextRPG/Debug/x64/GameData.dat` |
| Release / x64 | `Out/TextRPG/Release/x64/GameData.dat` |
| Debug / Win32 | `Out/TextRPG/Debug/Win32/GameData.dat` |
| Release / Win32 | `Out/TextRPG/Release/Win32/GameData.dat` |

Packer 실행 파일은 `Out/GameDataPacker/<Configuration>/<Platform>/GameDataPacker.exe`에 있다. 솔루션의 32비트 플랫폼 이름은 `x86`, 네이티브 프로젝트의 이름은 `Win32`다. 생성/키 준비/패킹 실패는 빌드 실패로 전파된다. 오래된 dat가 남아 있다는 이유로 성공 처리하지 않는다.

## 직접 실행

아래는 Release/x64 예다. 다른 구성에서는 해당 경로를 바꾼다.

```powershell
# 로컬 개발용 키를 준비한다. 기존 환경 변수 키/보호 파일이 유효하면 재사용한다.
& .\Out\GameDataPacker\Release\x64\GameDataPacker.exe prepare-development-key

# 생성된 Client YAML과 모든 Asset 파일을 원시 바이트 그대로 패킹한다.
& .\Out\GameDataPacker\Release\x64\GameDataPacker.exe pack .\GameData\Data\Client .\Out\TextRPG\Release\x64\GameData.dat --asset-root .\GameData\Asset

# 인증·복호화 후 엔트리 이름과 크기를 확인한다. 평문을 디스크에 풀지 않는다.
& .\Out\GameDataPacker\Release\x64\GameDataPacker.exe verify .\Out\TextRPG\Release\x64\GameData.dat
```

| 명령 | 동작 |
|---|---|
| `prepare-development-key` | 유효한 환경 변수 키 또는 보호된 기존 개발 키 확인. 둘 다 없으면 개발 키 최초 생성 |
| `pack <yaml-directory> <output-package> [--asset-root <asset-directory>]` | YAML은 `Data/Client/`, 명시적 자산은 `Asset/` 전체 상대 경로로 수집·암호화·안전 게시. 옵션 생략 시 generic YAML-only 패킹도 지원 |
| `verify <package>` | 패키지 인증·복호화·엔트리 목록 확인 |

종료 코드 `0`: 성공. `2`: 사용법/키 입력·키 준비 실패. `1`: 패키지 I/O/인증/형식 또는 예외 실패. `verify` 성공은 패키지의 인증/컨테이너 검증이지 YAML의 게임 객체·참조 검증 성공을 뜻하지 않는다. 게임의 `FGameDataStore`가 추가 검증한다.

## 키 공급과 개발 편의

패커와 게임은 같은 `FGameDataKey`를 사용한다.

1. `TEXT_RPG_GAMEDATA_KEY`가 **정의돼 있으면** 정확히 64자리 16진수(32바이트)만 허용한다. 대소문자 모두 가능하다. 빈 값·길이 오류·비-hex 값은 실패하며 개발 키로 fallback하지 않는다.
2. 변수가 **없을 때만** `%LOCALAPPDATA%/TextRPG/Development/GameData.key`를 읽는다. 파일에는 현재 사용자 DPAPI로 보호한 blob만 저장한다.
3. `prepare-development-key`는 최초 CNG 난수 키를 DPAPI로 보호한 뒤 고유 임시 파일과 덮어쓰기 없는 게시로 저장한다. 기존 유효한 키는 재사용하며 손상된 기존 파일은 덮어쓰지 않는다.
4. 게임 런타임은 키를 생성하거나 복구하지 않는다. 준비된 키가 없으면 명시적으로 시작 실패한다.

이 기본 개발 키는 **동일 Windows 사용자용 개발 편의**다. Release 로컬 빌드에도 사용할 수 있지만 제품 배포 키 전달 전략을 대신하지 않는다. CI/배포에서는 별도의 보안 경로에서 pack과 실행 환경 양쪽에 같은 키를 공급해야 한다. 배포 키 전달 정책은 아직 별도 설계 대상이다.

키 값·원문을 소스/문서/로그/명령행/패키지에 넣지 않는다. 기존 보호 키를 잃거나 교체하면 그 키로 만든 dat를 읽지 못하므로, 키 복구 또는 의도적인 재패킹을 구분한다. 클라이언트가 복호화할 수 있는 암호화를 완전한 비밀 보호 수단으로 설명하지 않는다.

## Asset 파일명과 카탈로그

원본 디렉터리는 유지하고 파일명은 `Category_Title_Words.ext`를 사용한다. 예: `Asset/Monster/Monster_Stone_Golem.txt`, `Asset/Logo/Logo_Text_Rpg.txt`. 단어는 `_`로 나누고 첫 글자 대문자·확장자 소문자를 사용하며 `36x36` 규격 토큰은 유지한다. 기존 28개 자산은 내용·ID·규격을 바꾸지 않고 이름만 전환했다. Excel 참조·생성 YAML·VS 항목/필터·회귀 기대 경로를 함께 맞춘다. `test_scene_ui_data.py`는 미참조 자산의 이름도 검사하며 기본 네 아트의 원시 SHA256 보호를 유지한다.

## 게임이 읽는 방식

```text
실행 파일의 디렉터리/GameData.dat
  → 키 읽기
  → 패키지 인증·복호화
  → 메모리 엔트리 unpack
  → 버전 표식/엄격한 YAML subset 파싱
  → 타입·필수·중복·Enum·범위·교차 참조 검증
  → 참조 아트의 UTF-8/문자/규격 검증
  → 런타임 테이블과 소유 자산 바이트 동시 게시
  → 첫 Scene과 UI 생성
```

- dat 경로는 **cwd가 아닌 exe 기준**이다. 파일명은 정확히 `GameData.dat`다.
- 데이터가 누락/변조되거나 키/버전/YAML/참조가 잘못되면 Scene과 ConsoleInput 진입 전에 실패한다. Excel/평문 YAML/컴파일된 행 데이터로 fallback하지 않는다.
- 현재 데이터 프로토콜 표식은 `# TextRPG.GameData 1`이며 바이너리 컨테이너 TGDP v2와 별개다. v1 dat는 새 패커로 재생성해야 한다. 일반 YAML 전체를 지원하지 않는다.
- 필수 테이블은 Scene, SceneOption, SceneTypeConfig, Localization이다. TextArt 카탈로그는 선택적이다. 구조 확장에는 runtime decoder/validator 변경이 함께 필요하다.
- 원본 Excel, 타입 헤더, schema 파일, 키 파일은 게임 패키지 입력에 넣지 않는다.
- 모든 Asset 파일은 재인코딩/공백·개행 정규화 없이 내장한다. 메모리 엔트리 키는 `Data/Client/Scene.yaml`, `Asset/Monster/Monster_Dragon.txt`처럼 GameData 기준 전체 상대 경로다. 별도 디렉터리 개수 헤더는 없다. 게임 배포 콘텐츠는 exe+dat로 동작하며 공유 키 공급은 별도다. txt/YAML 추출·임시 자산 생성·loose Asset 루트 탐색/fallback은 하지 않으며 `TEXTRPG_GAMEDATA_ROOT`를 자산 로딩에 사용하지 않는다.
- `FGameDataStore::FindAsset(wideAssetPath)`는 저장소가 소유한 const 바이트 vector를 반환한다. 행·자산 객체/버퍼 포인터는 다음 성공한 Load 또는 소멸까지 유효하며 실패한 Load는 값과 주소를 보존한다. UI는 디코딩한 셀을 별도 소유하고 임시 reader view를 보관하지 않는다. 현재 게임은 시작 시 한 번 로드하며 실행 중 hot reload를 제공하지 않는다.

## 입력·출력 보호

입력 YAML/Asset을 출력 패키지로 지정하는 동일/별칭 충돌과 루트 이탈·reparse 경로·대소문자 모호 중복을 거부한다. 패커는 고유 임시 파일을 배타적으로 생성하고 전체 쓰기/flush/close 후 최종 파일을 교체한다. 패킹 실패 시 이전 최종 출력과 자신이 소유하지 않은 임시 파일을 보존한다. 동일 출력에 대한 동시 성공은 마지막 게시가 적용되며 악의적 트리 교체나 프로세스/전원 중단 복구는 보장 범위 밖이다.

## 회귀 검사

```powershell
python .\Tools\Tests\test_package_safety.py --packer <절대경로-GameDataPacker.exe> --scratch <절대경로-소유-scratch>
python .\Tools\Tests\test_game_data_key.py --scratch-root <절대경로-소유-scratch>
python .\Tools\Tests\test_game_data_application.py --game <절대경로-TextRPG.exe> --packer <절대경로-GameDataPacker.exe> --scratch-root <새로운-절대경로-소유-scratch>
```

마지막 검사는 원본을 바꾸지 않고 exe+dat 배포본과 소유 최소화 콘솔로 다른 cwd 실행, 정확한 로고 셀·무추출, 같은 EXE로 데이터만/아트만 재패킹한 뒤 재시작 시 반영, 참조 아트 누락/손상·키/데이터 실패 차단과 복구를 확인한다. 독립 예외/cleanup 회귀도 제공한다. 아래 검사는 예약장치 경로 테스트와 별개다.

```powershell
python .\Tools\Tests\test_gamedata_store_exceptions.py --scratch-root <절대경로-소유-scratch> --architecture x64 --label exception-x64
python .\Tools\Tests\test_gamedata_store_exceptions.py --scratch-root <절대경로-소유-scratch> --architecture x86 --label exception-x86
python .\Tools\Tests\test_game_data_application_cleanup.py --scratch-root <절대경로-소유-scratch> --label cleanup
python .\Tools\Tests\test_game_data_application_cleanup_meta.py --scratch-root <절대경로-소유-scratch> --label cleanup-meta
```

세부 수용 상태·미검증 항목은 [로드맵](../../Docs/ROADMAP.md)과 [설계 문서](../../Docs/GameDesign/006_YAML내보내기이식설계안.md)를 따른다.
