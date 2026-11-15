# GameData YAML 패키징·암복호화·C++ 로더

> **현재 Asset/PCH 계약 — 2026-11-15:** `GameData.dat` TGDP v2는 `Data/Client/<YAML 경로>`와 `Asset/<자산 경로>`의 원시 바이트를 함께 인증 암호화한다. 런타임은 저장소 소유 메모리에서 아트를 읽으며 txt/YAML 재생성·loose Asset 루트 탐색/fallback은 하지 않는다. 원본 디렉터리는 유지하고 파일명은 `Category_Title_Words.ext`로 통일했다. 아트28개 바이트·ID·규격은 그대로다. 아래 과거의 Asset 별도 배포/미구현·v1 컨테이너·GameDataRoot 설명보다 이 계약과 [Packer 사용법](../../Tools/GameDataPacker/README.md)이 우선한다. 최종 수용/검토 상태는 [로드맵](../ROADMAP.md) 상단을 따른다.

> **최신 Scene 계약 변경:** 사용자 승인으로 `Asset/Scene`의 레거시 배치 txt와 `Scene.AssetPath`를 제거했다. 정적 Scene은 `SceneId`·`SceneType`·`IsStartupScene` 세 필드만 허용한다. TextArt.AssetPath는 유지하며 원본 아트 이름에 맞춘 경로만 수정한다. 아래 예전 네 필드/레거시 자산 유지 설명과 검증 기록은 당시 이력이며 현재 계약보다 우선하지 않는다. Asset 동봉의 현재 계약은 상단 TGDP v2 설명을 따른다. 이번 전환의 최종 검증을 완료했다. 현재 결과는 로드맵의 2026-11-15 Scene 레거시 제거 완료 기록을 따른다.

> 최신 출력 계약: **schema YAML 제거, 타입 전용 C++ 및 exe 옆 GameData.dat 런타임 연결을 완료했다.** Excel/로더의 필드·타입·필수·Enum·참조 검증을 유지한다. 최종 검증과 scoped 리뷰는 아래 완료 상태를 따르며 예전 Schema/행 배열/원문 제공 전용 기록은 역사다.

## 다음 목표 정정 — 타입 전용 헤더 + exe 옆 GameData.dat

> **사용자 승인 범위의 DATA-02~08을 구현·검증 완료했다.** 기존 행 배열 유지/패키지 원문 제공 전용 범위는 최종 계약으로 대체했다. 과거 실패·보류 기록은 보존하며 최종 증거는 [로드맵](../ROADMAP.md)을 따른다.

### 목표 계약

1. Generator는 `GameData/Class`에 구조체·Enum 등 **타입 정의만** 생성한다. 실제 행 배열·값 리터럴과 그 배열 기반 Find 함수는 생성하지 않는다. 필드 기본 초기화와 Enum 정의는 타입 계약이므로 유지할 수 있다.
2. 실제 행 데이터는 `GameData/Data/Client/*.yaml`에만 생성하며 별도 Schema YAML은 생성하지 않는다. Excel 원본 필드·타입·필수/참조 검증은 유지한다. 생성기/소비자 변경을 함께 통합하며 생성 헤더를 수작업으로 지우는 방식으로 처리하지 않는다.
3. 기존 GameDataPacker와 AES-256-GCM 형식을 재사용하여 생성 YAML 묶음을 **게임 exe와 같은 디렉터리의 `GameData.dat`**로 게시한다. 빌드 대상은 `$(TargetDir)GameData.dat`이며 Debug/Release × x64/Win32별 exe 디렉터리를 따른다. 기존 파일 보호·고유 임시 파일·실패 시 이전 최종 출력 보존을 유지하고 패킹 실패는 빌드 실패다.
4. 게임은 cwd/원본 Excel/평문 YAML 탐색 대신 **실행 파일 디렉터리**에서 정확히 `GameData.dat`를 읽는다. 시작 Scene 생성보다 먼저 `FGameDataPackageReader`로 인증·복호화하고 엔트리별 메모리 unpack을 수행한다. 평문 YAML을 디스크에 풀 필요는 없다.
5. 복호화 성공은 데이터 로딩 완료가 아니다. 이어서 YAML 파싱→생성 타입 역직렬화→필수 테이블/필드·중복 ID·Enum·범위·교차 참조 검증→런타임 테이블 일괄 게시까지 수행한다. 파서와 지원 YAML/스키마 버전 계약은 DATA-03에서 확인·선택한다.
6. Scene·현지화·TextArt 등의 소비자는 런타임 저장소를 조회한다. 저장소가 반환 참조/포인터보다 오래 생존해야 하며, 최초 전체 로딩이 실패하면 게임 진입을 중단한다. 헤더 상수/평문 YAML/Excel로 fallback하여 실패를 감추지 않는다.
7. 누락·잘림·변조·키 오류·지원하지 않는 패키지/데이터 버전·파싱 실패는 부분 데이터 없이 명시적으로 거부한다. 인증 전 파싱이나 일부 테이블 먼저 게시를 하지 않는다. 기존 Scene 세 정적 필드·네 선택지·선택적 TextArt 카탈로그 정책은 변경하지 않는다.

```text
Excel → Generator ┬→ 타입 전용 C++ 헤더
                  └→ Client 데이터 YAML → 기존 packer → exe 옆 GameData.dat
게임 시작 → dat 인증·복호화 → 메모리 unpack → YAML 파싱·검증
          → 런타임 데이터 저장소 → Scene/Localization/TextArt/UI
```

### 범위와 미결정 사항

- 패키지 입력은 실제 Client YAML과 명시적 `GameData/Asset`의 모든 파일이다. YAML은 `Data/Client/`, 자산은 `Asset/` 전체 상대 경로로 저장한다. 원본 Excel·타입 헤더·schema·소스·키는 제외한다. 자산은 원시 바이트 그대로이며 별도 디렉터리 개수 헤더는 없다. 공유 키 공급을 전제로 exe+dat만으로 아트까지 읽고 디스크 추출/fallback은 하지 않는다.
- 개발 키 공급 계약: 패커/게임 공통 공급자가 `TEXT_RPG_GAMEDATA_KEY`의 64자리 hex를 우선한다. 변수가 정의돼 있으나 빈 값/잘못된 형식이면 실패하고 개발 키로 fallback하지 않는다. 변수 자체가 없을 때만 `%LOCALAPPDATA%/TextRPG/Development/GameData.key`의 현재 사용자 DPAPI 보호 키를 읽는다. 빌드의 명시적 `prepare-development-key`가 최초 CNG 난수 키를 생성/보호·저장하고 이후 재사용한다. 게임은 키를 만들지 않으며 손상된 기존 파일을 덮어쓰지 않는다. 키 원문은 코드·문서·로그·명령행·dat에 기록하지 않는다. 이는 동일 Windows 사용자 개발 편의일 뿐이며 **제품 배포 키 전달 방식은 여전히 미결정**이다. 예전 환경변수 전용 기록은 과거 범위다.
- YAML 데이터 프로토콜 v1은 각 파일 첫 줄의 `# TextRPG.GameData 1`로 구분한다. 별도 schema/manifest YAML을 되살리지 않는다. 패키지 바이너리 버전과 구분해 미지원 데이터 버전을 거부하며, 구조 호환성은 컴파일된 테이블/필드/타입/Enum 및 의미 검증으로 검사한다. 지원 범위는 생성기가 내보내는 double-quoted mapping/scalar·정수·bool·null이며 일반 YAML의 alias/tag/flow collection/임의 중첩은 지원하지 않는다. 파싱 실패를 추정 복구하지 않는다.
- packer 빌드와 게임 빌드 간 의존성을 순서화하고 같은 output/obj 경합을 피한다. 현재 패키지 출력의 nonce는 매번 달라질 수 있으므로 바이트가 매번 같다는 기준 대신 복호화된 엔트리/값 일치와 실제 게임 반영을 검사한다.
- 원본 스키마 확장, 엔티티/콘텐츠 추가, 이미지/아트 내장, hot reload, 키 보관의 완전한 비밀 보장은 이번 목표가 아니다.

### 완료 기준 — 연결 구현 및 잔여 검증

- [x] DATA-02 생성 헤더에서 실제 데이터 행/상수 배열 제거, YAML의 값과 타입 계약 보존.
- [x] DATA-03 YAML 파싱·역직렬화·교차 테이블 검증·저장소 조회 테스트.
- [x] DATA-04 인증 먼저/메모리 unpack/전체 성공 후 게시/첫 Scene 전 로드.
- [x] DATA-05 게임의 기존 정적 배열/Find 참조를 런타임 저장소로 이관.
- [x] DATA-06 네 구성의 exe 옆 GameData.dat 자동 생성과 실패 출력 보존.
- [x] DATA-07 개발 키 공급 계약 고정 및 배포 키 미결정 범위 명시.
- [x] DATA-08 다른 cwd 실행·재컴파일 없이 새 dat 값 반영·누락/키/변조/버전/YAML/중복/참조 실패 거부·네 구성 경고/오류 0.

### 실제 연결 상태와 미완료 범위

타입 전용 생성물을 실제 게시하고 공통 키 공급·빌드 패킹·Main 시작 로더·실제 소비자 조회를 연결했다. 부모가 네 구성 솔루션/네이티브 분석 경고·오류 0, 각 EXE 18개 통합 시나리오 및 다른 cwd·재컴파일 없는 dat 값 변경을 확인했다. 직접 프로젝트 Build와 pack target 실패 시 이전 dat 보존도 확인했다. 사용법은 [Generator](../../Tools/GameDataGenerator/README.md), [Packer](../../Tools/GameDataPacker/README.md)에 기록했다.

**이번 GameData.dat 런타임 연결 수용 기준을 완료했다.** 추가 승인 후 RUNTIME-001의 콘솔 별칭·superscript COM/LPT 숫자·확장자 앞 Unicode 공백 분류를 보완하고 원래 AssetPath 값은 보존했다. x64/x86 전체 store 각 521 checkpoint, 최종 네 구성 빌드/분석 경고·오류 0 및 실제 EXE 각 18개 시나리오를 확인했다. 예외/정리·회귀 보완 여섯 파일과 마지막 경로 두 파일의 focused 정적 리뷰가 각각 PASS이며 canonical 기록을 읽어 확인했다. 전체 프로젝트 재인증은 아니며 초기 실패·incomplete 기록은 보존한다. P3 콘텐츠·수동 호스트 인증·제품 키 배포·아트 내장은 별도 범위다. [로드맵 최종 기록](../ROADMAP.md)을 따른다.

## 목적

기존 `GameDataGenerator`에 YAML 내보내기를 추가하고, 생성된 YAML 전체를 하나의 패키지로 묶고 암호화하는 독립 C++ 도구와 게임 런타임에서 패키지를 복호화해 읽는 C++ 클래스를 마련한다. 현재 구현 상태와 미완료 항목을 기록한다.

## 확인한 현재 상태와 참고 코드

- TextRPG의 Generator는 Client 전용 C++ 헤더를 생성하며, 이번 변경에서 실제 Client 데이터 YAML을 출력한다. 별도 스키마 출력은 제거 대상으로 확정했다.
- 원본 Generator(`C:\Portfolio\GameServer_Portfolio\Tools\GameDataGenerator`)는 테이블 YAML과 Enum·테이블 스키마 YAML 생성 방식을 보유한다. 기존 제안에서는 Client 데이터와 스키마를 `Out/GameData/Generated/Data/Client/`, `Schema/`에 생성하도록 했다.
- 기존 암호화 참고 경로는 `C:\Portfolio\GameServer_Portfolio\Libraries\NetworkLib\Crypto`다. 확인한 `FDefaultPacketCipher`는 패킷 바이트를 사용자 정의 상태 변환으로 인코딩하고, 체크섬은 바이트 합의 하위 8비트로 계산한다. 이 구현은 형식 호환성 참고 대상으로 검토할 수 있지만, 강한 기밀성이나 위·변조 검증을 제공하는 암호화로 간주하지 않는다.
- 게임 런타임은 현재 생성된 C++ 데이터 헤더를 사용하는 구조이며 YAML 패키지 로더는 없다.

## 구성 요소와 데이터 흐름

```text
GameData/Excel/*.xlsx
        │
        ▼
GameDataGenerator (기존 C# 도구)
  검증 + Client YAML 생성
        │
        ▼
GameData/Data/Client/*.yaml
        │
        ▼
GameDataPacker (신규 C++ 콘솔 프로젝트)
  YAML 수집 → 단일 패키지 구성 → 인증 암호화
        │
        ▼
GameData package
        │
        ▼
게임의 C++ GameDataPackageReader/Loader
  패키지 읽기 → 인증 확인 및 복호화 → YAML 엔트리 로드/조회
```

- YAML은 검증된 Excel에서 파생되는 배포 형식이다. 기존 Client 전용 C++ 헤더 생성은 유지한다.
- 패커는 지정된 생성 디렉터리의 YAML만 대상으로 하며, 하위 디렉터리 포함 여부와 정렬 순서를 고정한다. 입력 폴더의 임의 파일은 패키지에 넣지 않는다.
- 런타임 로더는 패키지를 메모리에서 읽어 엔트리별 YAML 원문을 제공한다. YAML 텍스트를 게임 데이터 객체로 변환하는 파서는 기존 의존성을 확인한 뒤 정하며, 파서가 없다면 별도 선택·승인 항목으로 남긴다.

## YAML 생성 계약

기존 제안의 Client 전용 범위를 유지한다.

```text
GameData/
├─ Excel/...                         # 원본
├─ Class/                            # 생성 C++ 헤더 (--output-root)
│  ├─ Common/GameDataEnums.g.h
│  └─ <Table>/<Table>Data.g.h
└─ Data/                             # 생성 YAML (--yaml-output-root)
   └─ Client/<Table>.yaml
```

- Client 대상 테이블과 필드만 기록한다. 원본의 Server 출력 모드는 이식하지 않는다.
- 각 행은 원본 Generator와 같은 `<TableName><PrimaryKey>:` 맵 키를 사용하고 선언된 필드 순으로 출력한다.
- YAML 타입 표현, 문자열 인용, null, 숫자 문화권, UTF-8/LF 및 결정적 출력 순서는 기존 설계안의 계약을 따른다.
- `--output-root` 기본값은 솔루션의 `GameData/Class`, `--yaml-output-root` 기본값은 `GameData/Data`다. C++ include 루트는 Class 자체이며 Cpp 하위 폴더를 추가하지 않는다. 컴파일 결과와 중간 파일은 기존처럼 Out에 둔다.
- `--validate-only`는 파일을 생성하지 않고, `--check`는 두 출력 루트를 읽기 전용으로 비교한다. 입력/출력 및 두 출력 루트의 중첩·소유권을 먼저 검증한다. 생성 실패 시 기존 출력을 보존하도록 두 루트의 staging/publish/rollback을 하나의 작업으로 처리한다.
- GameData/Class와 GameData/Data는 생성기 소유 디렉터리다. 원본이나 수동 관리 파일을 섞지 않는다. 기존 Out/GameData/Generated 산출물은 새 빌드의 입력/출력 경로에서 제외하며, 별도 확인 없이 삭제하지 않는다.

## 패키지 파일 형식

현재 컨테이너 버전은 **2**다. v1 dat는 명시적으로 거부하며 새 패커로 다시 생성한다. YAML 데이터 프로토콜 표식은 계속 1이다. 상대 경로와 본문을 한 암호문에 담고 nonce는 매 패킹 시 난수 생성하므로 엔트리 정렬이 결정적이어도 암호화 dat 바이트 자체는 같지 않을 수 있다.

사용자가 지정한 엔트리 순서는 `[파일명][파일사이즈]` 뒤에 해당 YAML 본문이 오는 형식으로 한다. 가변 길이 파일명과 본문을 안전하게 구분할 수 있도록 다음 길이 정보를 직렬화한다. 대괄호 표기는 필드 순서를 나타내며 리터럴 바이트로 쓰지 않는다.

```text
패키지 외부 헤더: magic("TGDP") | formatVersion(u32 LE) | nonce(12 bytes) | ciphertextSize(u64 LE) | ciphertext | authTag(16 bytes)
암호문 안의 평문 인덱스:
    entryCount(u32 LE)
    반복 {
        filenameByteLength(u32 LE) | filename(UTF-8) | fileSize(u64 LE) | fileBody(YAML 또는 Asset raw bytes)
    }
```

- 각 엔트리에서 파일명과 파일 크기 정보가 YAML 본문 바로 앞에 온다. `fileSize`는 본문 바이트 수이며 문자 수가 아니다.
- 정수 바이트 순서, 폭, 필드 크기 제한, 최대 엔트리 수·총 패키지 크기, 이름 정규화, 포맷 버전 호환 정책은 구현 전에 명세하고 문서화한다.
- 파일명은 상대 경로만 허용하고 절대 경로, `..`, 중복 이름, 잘못된 UTF-8, 크기 초과, 잘린/추가 데이터는 거부한다. 엔트리는 정규화된 경로 순으로 기록해 같은 입력에서 결과가 결정적이게 한다.
- 모든 YAML의 평문 묶음을 통째로 암호화한다. 따라서 파일명과 크기도 암호문 안에 포함되어 메타데이터 노출을 줄인다. 외부 헤더에는 복호화에 필요한 비밀이 아닌 포맷 식별 정보와 nonce, 암호문 크기, 인증 태그만 둔다.
- 파싱은 인증 검증 성공 후 진행한다. 인증 실패 또는 형식 오류가 나면 일부 엔트리도 로더에 노출하지 않는다.

## 암호화 및 키 관리

- 패키지 보호 목적을 먼저 명시한다. 게임 실행 파일에 복호화 키를 포함하면 숙련된 사용자가 키를 추출할 수 있으므로, 클라이언트 패키지 암호화는 데이터 추출·수정의 장벽을 높일 뿐 게임 배포물 안에서 비밀을 완전히 지키지는 못한다.
- 기존 `FDefaultPacketCipher`의 사용자 정의 변환과 약한 체크섬은 그대로 새 패키지 보호에 사용하지 않는다. 원본 코드의 의존성·라이선스·테스트를 추가 조사하고, 필요하면 패킷 호환성용으로만 분리한다.
- 패키지는 Windows CNG의 AES-256-GCM으로 암호화한다. nonce는 시스템 CNG 난수 생성기로 만들고, magic·버전·암호문 크기를 추가 인증 데이터로 인증하며 16바이트 태그를 검증한다.
- 32바이트 키는 패커가 `TEXT_RPG_GAMEDATA_KEY` 환경 변수에서 64자리 16진수로 읽는다. 런타임 로더는 호출자로부터 32바이트 키를 받는다. 실제 배포 키 제공 방식은 아직 정해지지 않았다.
- 키를 소스, 생성 YAML, 패키지 헤더, 로그, 명령행 인자에 기록하지 않는다. 실행 파일에 배포 키를 포함하면 추출될 수 있어 클라이언트 패키지 암호화만으로 비밀을 완전히 지킬 수 없다.

## C++ 프로젝트와 런타임 API

- 솔루션에 독립 콘솔 프로젝트 `GameDataPacker`를 추가했다. 명령은 `pack <yaml-directory> <output-package>`와 `verify <package>`이며 `verify`는 메모리 내 인증·복호화 후 목록만 출력하고 평문 파일을 추출하지 않는다.
- 런타임 `GameData::FGameDataPackageReader`는 `Load(packagePath, key, error)`, `FindEntry(relativeName)`, `GetEntries()`, `Unload()`를 제공한다.
- 엔트리 본문은 UTF-8 YAML 바이트를 보존한다. 인증·형식·경로·중복 파일명·크기 제한 실패 시 엔트리를 노출하지 않는다. YAML 원문을 게임 객체로 변환하는 파서는 구현하지 않았다.
- 추출형 도구 동작이 필요하지 않으면 로더는 파일 시스템에 패키지 엔트리를 기록하지 않는다. 메모리 내 읽기 전용 접근을 기본으로 한다.
- 프로젝트 추가 시 기존 GUID·구성·출력 경로 규칙을 보존하고 Debug/Release 및 Win32/x64 설정을 정합시킨다. C++20과 MSVC v145를 따른다.

## 구현 후 검증 계획

- Generator: 문자열·숫자·불리언·Enum·null과 특수 문자열 YAML 직렬화, 결정적 출력, `--check`, 검증 실패 시 기존 출력 보호.
- 패커/로더: 빈 입력, 단일·다중 파일, UTF-8 파일명, 빈 본문, 크기 경계, 잘못된/중복 경로, 잘린 패키지, 인증 태그 변조, 잘못된 키, 버전 미지원.
- 성공 시 왕복 결과의 파일명·크기·본문이 원본 YAML과 바이트 단위로 일치해야 한다. 실패한 패키지는 어떤 엔트리도 부분 반환하지 않아야 한다.
- 키가 로그·명령 기록·패키지 평문 메타데이터에 노출되지 않는지 확인한다. 실제 배포 키 관리가 확정되지 않으면 보호 수준을 그에 맞게 제한해 보고한다.
- 프로젝트 빌드와 기존 Generator 검증을 실행하고 Debug/Release 및 Win32/x64 결과와 미검증 항목을 보고한다.

## P0 및 생성 경로 변경 검증

> P0 및 생성 경로 변경 완료. 독립 리뷰 지적을 보완한 최종 후보가 전체 구성 빌드·테스트와 변경된 두 C# 파일의 범위 한정 독립 재리뷰를 통과했다. 이전 다른 파일 검토를 결합한 판정이며 세부 완료 상태는 ROADMAP을 따른다.

- 현재 C++ 헤더 기본 출력은 `GameData/Class`, YAML 기본 출력은 `GameData/Data`다. 아래 초기 구현 기록의 Out 통합 경로는 과거 결과다.
- 전체 솔루션 Debug/Release × x64/x86(네이티브 Win32) 빌드와 구성별 패커 안전성 10개 테스트, 생성기 self-test/check/validate-only가 통과했다. 최종 빌드는 경고·오류 진단 0건이다.
- 실제 생성 YAML 9개 패킹·verify 성공. 기존 임시 파일·입력 별칭·게시 실패 보존 검사를 수행했다. 키는 기록하지 않았다.
- 출력 경로가 입력 YAML과 같은 파일이면 실패하며, 임시 파일은 난수 이름과 CREATE_NEW로 독점 생성한다. 기존 .tmp 파일을 소유한다고 가정하지 않는다. 전체 쓰기·flush·close 뒤에만 최종 교체한다.
- 동일 출력 동시 패킹은 마지막 성공 게시를 채택한다. 악의적인 동시 디렉터리/입력 교체와 프로세스 강제 종료 복구는 보장 범위 밖이다.
- 두 생성 루트의 게시 전 실패는 rollback하며 복원 자체가 실패하면 백업을 남기고 실패를 보고한다. 둘 다 게시되면 커밋이며 이후 백업 정리 실패는 성공 종료와 구조화된 committed 경고/남은 경로로 보고한다. 남은 백업 내용은 부분 삭제됐을 수 있다. 프로세스/전원 중단까지 포함하는 전역 원자적 트랜잭션은 아니다.
- Generator는 로컬 일반 경로를 지원한다. UNC·device·extended 경로는 거부하고 Windows 핸들로 기존 조상의 물리 경로를 확인해 짧은 이름/드라이브 매핑 별칭의 동일·중첩을 검사한다. 미존재 조상의 접미사를 포함한 각 폴더 이름의 잘못된 문자·예약 장치 이름도 게시 전에 거부한다. 트리·매핑이 안정적이어야 하며 동일 트리 동시 생성은 호출자가 직렬화한다.

## 초기 구현 상태 및 남은 일

- 구현됨: Client YAML 데이터·Enum/테이블 스키마 생성, AES-256-GCM 패킹·검증 C++ 도구, 메모리 기반 C++ 로더, 솔루션 연결, 관련 지침·설계 문서 갱신.
- 빌드 확인: 전체 솔루션 Debug/Release × x64/Win32 빌드 성공. 생성기는 5개 workbook 테이블을 검증해 C++ 헤더·YAML 등 15개 파일을 `Out/GameData/Generated`에 생성했다. Release 빌드에 기존 MSVC `LIB` 환경 경로 경고(CS1668)가 있었지만 빌드는 성공했다.
- 확인: Generator Release self-test 통과. 테스트 workbook에 필수 공유 `Enums` 테이블을 포함하고, 기존 한글 wide-literal 기대값을 실제 문자열 길이에 맞게 고쳤다.
- 확인: 임시 키로 생성 YAML 9개를 `GameDataPacker pack`으로 암호화하고 `verify`로 인증·복호화 및 엔트리 로드를 마쳤다. 잘못된 키/변조 패키지 거부 사례와 원본 본문 바이트 대조는 아직 실행하지 않았다.
- 남음: 런타임 YAML 파싱 및 실제 게임 데이터 경로에서 로더 호출, 배포 환경의 키 제공 방식을 정해야 한다. 실행 파일 안에 배포 키를 넣지 않는다.
