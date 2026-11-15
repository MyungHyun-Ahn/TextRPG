# 작업별 AI 지침

루트 [AGENTS.md](../AGENTS.md)가 공통 규칙의 단일 원본이다. 요청과 관련된 문서만 읽고 기존 명령·도구를 우선 사용한다. 단순 작업을 위해 별도 역할 체계나 테스트 프레임워크를 만들지 않는다.

| 작업 | 추가 참조 |
|---|---|
| C++ 구현·코드 검토 | [공통 컨벤션](../Docs/Conventions/common.md), [C++ 컨벤션](../Docs/Conventions/cpp.md) |
| 게임 규칙·수치·콘텐츠 | [design.md](design.md) |
| 데이터 생성기·개발 보조도구 | [tools.md](tools.md) |
| 빌드·재현·회귀 검증 | [qa.md](qa.md) |

## 공통 실행 절차
1. 목표, 수정 경로, 현재 상태, 완료 기준을 확인한다.
2. 관련 인터페이스·기존 구현·검증 명령을 읽고 최소 변경을 수행한다.
3. 관련 검증을 실행하고 변경 파일, 결과, 미검증 사항을 보고한다.

## 원본 검토와 적용 범위
2026-11-13에 `C:\Portfolio\MMO_Portfolio`의 `AGENTS.md`, `Agents/README.md`, `Agents/director.md`, `Agents/design.md`, `Agents/tools.md`, `Agents/qa.md`, `Docs/Conventions/common.md`, `Docs/Conventions/cpp.md`를 검토하여 이 프로젝트용으로 정리했다.

- 유지: C++ 명명·포맷·소유권 규칙, 기존 작업 보존, 데이터 원본 보호, 정상/실패/경계 검증, 사실에 근거한 결과 보고.
- 제외: 서버/IOCP/RIO/DB 운영 계약, 네트워크·부하·배포 지침, Unity 전용 지침.
- 원본 저장소에 종속된 Director/Worker 상태 머신, TaskPreflight, 작업 기록·Worktree 체계와 아트 도구 스킬은 복사하지 않았다. 이 프로젝트에는 해당 실행 기반이 없다.
- 이식한 문서는 독립적으로 관리한다. 향후 작업 시 원본 저장소 지침을 자동 적용하거나 원본 파일을 수정하지 않는다.
