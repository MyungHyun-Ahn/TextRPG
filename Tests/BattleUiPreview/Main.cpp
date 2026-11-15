#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <conio.h>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// FDataScene.h 주석의 화면 시안을 복사한 일회성 표시 테스트다.
// 실제 게임 데이터, 입력 액션 또는 게임 렌더러와 연결하지 않는다.
static const wchar_t kLayout[] = LR"LAYOUT(┌─ PLAYER STATUS ────────────────────────┐ ┌─ MONSTER STATUS ───────────────────────┐ ┌─ BATTLE LOG / 8 LINES ─────────────────────┐
│ Lv.18   루나 / 여검사                  │ │ Lv.22   고대 드래곤 [BOSS]             │ │ [12:31] 전투 시작: 고대 드래곤 등장       ░│
│HP  ████████████████░░░░  178/220       │ │HP  █████████████░░░░░░░  512/800       │ │ [12:32] 루나가 검을 휘둘렀다.             ░│
│ 상태: 정상    직업: Swordswoman        │ │ 상태: 화상    타입: Dragon             │ │ [12:32] 드래곤에게 48의 피해!             ░│
└────────────────────────────────────────┘ └────────────────────────────────────────┘ │ [12:32] 드래곤의 화염 숨결 준비...        ░│
┌─ PLAYER ART / 36 x 36 ─────────────────┐ ┌─ MONSTER ART / 36 x 36 ────────────────┐ │ [12:33] 루나가 방어 태세를 취함.          ░│
│                                        │ │                                        │ │ [12:33] 피해량 34 → 11 감소               █│
│                                        │ │                                        │ │ [12:34] 드래곤: 화상 지속 피해 9          █│
│                                        │ │             █            █             │ │ [12:34] 다음 행동을 선택하세요.           █│
│                 ███████          █     │ │              ▓          ▓              │ │ [휠 스크롤 ↑↓]    16~23 / 23    ▼          │
│               ██▓▓▓▓▓▓▓█         █     │ │       █      ▓▓        ▓▓      █       │ └────────────────────────────────────────────┘
│              █▓▓▓██████▓█▓      █░█    │ │       ▓█      ▓▓      ▓▓      █▓       │ ┌─ ACTIONS / 4 OPTIONS ──────────────────────┐
│              ▓▓██░░░▓▓░█▓█      █░█    │ │       ▓▒█     ▓▓▓    ▓▓▓     █▒▓       │ │                                            │
│             ▓▓█▓▓▓▓▓░░▓░██▓     █░█    │ │       ▓▓▒█      █▓ █▓ █     █▒▓▓       │ │                                            │
│             ▓▓█▒░░░░░░░▓░█▓     █░█    │ │    █  ▓▒▓▓█    █████████   █▓▓▒▓  █    │ │  ┌────────────────────────────────────┐    │
│             ▓▓█▒░░░░░░░▒█▓█▓    █░█    │ │    ██ ▓▒▒▓▓    █████████   ▓▓▒▒▓ ██    │ │  │▶ [1] 공격   · 기본 공격            │    │
│             ▓▓█▒░█░░░█░▒█▓█▓    █░█    │ │    █▒█▓▒▒▒▓▓   █▒▒▒▒▒▒███ ▓▓▒▒▒▓█▒█    │ │  └────────────────────────────────────┘    │
│            ▓█▓▒█░░░░░░░▒█▓█▓    █░█    │ │    █▒▒▓▒▒▒▒▓▓  ▒▓▓▒▒▓▓▒██▓▓▒▒▒▒▓▒▒█    │ │                                            │
│            ▓█▓▒█░░░▒░░░▒█▓█▓    █░█    │ │   █▒▒▒▒▒▒▒▒▓▒▓▓▓█▒▒▒▒█▓▓█▒▓▒▒▒▒▒▒▒▒█   │ │                                            │
│            ▓█▓▒█▒░▒▒▒░▒▓█▓█▓    █░█    │ │   █▓▓▒▒▒▒▒▒▒▓▓▒▒▒▒▒▒▒▒▒▒▓▓▒▒▒▒▒▒▒▓▓█   │ │                                            │
│            ▓█▓▒█ ▒░░░▒ ▓█▓█▓    █░█    │ │   █▒▒▓▓▒▒▒▒▒▒▓█▒▒▒▒▒▒▒▒█▓▒▒▒▒▒▒▓▓▒▒█   │ │  ┌────────────────────────────────────┐    │
│            ▓█▓█▓███████▓█▓██    █░█    │ │   ████▒▓▓▓▒▒▒▒█▒▓▓▓▓▓▓▒█▒▒▒▒▓▓▓▒████   │ │  │  [2] 스킬   · 스킬 사용            │    │
│           ███████████████████   █░█    │ │      █▒▒▒▒▓▓▓▒▒█▓░▓▓░▓█▒▒▓▓▓▒▒▒▒█      │ │  └────────────────────────────────────┘    │
│          ███████▒▒▒░▒▒▒████████ █░█    │ │      █▒▒▒▒▒▒▒▓██▓▓▓▓▓▓▓█▓▒▒▒▒▒▒▒█      │ │                                            │
│         █▒▒▒▒▒██▒▒▒░▒▒▒███▒▒▒▒█ █░█    │ │     █▒▒▒▒█▒▒▒▒██▓████▓▓██▒▒▒█▒▒▒▒█     │ │                                            │
│          ███████▒▒▒░▒▒▒████████ █░█    │ │     █▓▓▓▓▓▓▓▓█████░▓░█████▓▓▓▓▓▓▓█     │ │                                            │
│           ▓▓▓▓▓█▓▓▓▓▓▓▓▒██▓▓▓▓▓ █░█    │ │     ███   █▒████▓▓▓▓▓▓▓███▒█   ███     │ │  ┌────────────────────────────────────┐    │
│           ▓▓▓▓▓██▒▒░▒▒▒▒██▓▓▓▓▓▓███▓   │ │     █     █▒███▒▒▒▒▒▒▒▒███▒█     █     │ │  │  [3] 아이템   · 소지품 사용        │    │
│          ▓▒▓▓▓▓██▒▒░▒▒▒▒██▓▓▓████████  │ │           █▒███▒▒▒▒▒▒▒▒███▒█           │ │  └────────────────────────────────────┘    │
│          ▓▒▓▓▓████▒░▒▒▒██░▓▓▓▒▓▓ ▓     │ │           █▒████▓▓▓▓▓▓████▒█           │ │                                            │
│         ▓▓▒▓▓▓█▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒▓▓ ▓     │ │           █▒████▒▒▒▒▒▒████▒█           │ │                                            │
│         █████▓██████▒████▓░░████ ▓     │ │           █▒████▒▒▒▒▒▒████▒█           │ │                                            │
│         █▒▒▒█▓▓▓▓▒░▒▒▒░▓▓▓▓░████ ▓     │ │           █▒███▒▓▓▓▓▓▓▓███▒█▓▓▓        │ │  ┌────────────────────────────────────┐    │
│         █████▓▓▓▒▒░▒▒▒░▓▓▓▓░████ ▓     │ │           █▒███▒▒▒▒▒▒▒▒▒██▒█▒▓▓▓       │ │  │  [4] 방어   · 피해 감소            │    │
│         ▒░░▓████▒▒░▒▒▒░████▓░░▒  ▓     │ │          ███████▒▒▒▒▒▒▒██████▒▓▓▓▓     │ │  └────────────────────────────────────┘    │
│         ▒░░░▓▓▓▒▒▒░▒▒▒░▒▓▓░░░░░ ███    │ │         ████████▓▓▓▓▓▓▓███████▒▓▓▓     │ │                                            │
│           ░░░▓▓▓▓▓▓▓▓▓▓▓▓▓░░░   ███    │ │         ▓ ▓█▓███████▒████▓█▓█▓ ▒▓▓     │ │                                            │
│            ░░▓▓▓▓▒▓  ▓▒▓▓▓░░           │ │           █▒▒▒▒████████▒▒▒▒▒█   ▒▓▓    │ │                                            │
│              ▓▓▓▓▒▓  ▓▒▓▓▓▓            │ │          ████████    █████████▒▒▒▓▓    │ │                                            │
│             ▓▓▓▓▓▒   ▓▒▒▒▒▒            │ │          ▓▓█▓▓█▓▓     ▓▓█▓▓█▓▓▓▓▓▓     │ │  마우스 클릭 / 숫자키 1~4                  │
│             ██████   ████████          │ │          ▓  ▓  ▓      ▓  ▓ █▓  ▓       │ │  ↑↓ 선택   Enter 확인                      │
│                                        │ │          ▓  ▓  ▓      ▓  ▓  ▓          │ │                                            │
└────────────────────────────────────────┘ └────────────────────────────────────────┘ └────────────────────────────────────────────┘)LAYOUT";

// 마을 시안: 기존 배틀 시안과 같은 132셀 x 43행, 가운데 아트는 36 x 36.
static const wchar_t kLayout2[] = LR"TOWN(┌─ PLAYER STATUS ────────────────────────┐ ┌─ REGION / TOWN ────────────────────────┐ ┌─ TOWN LOG / 8 LINES ───────────────────────┐
│ Lv.18   루나 / 여검사                  │ │ 지역: 에르덴 숲 / 동부                 │ │ [13:20] 바람숲 마을에 도착했습니다.       ░│
│HP  ████████████████░░░░  178/220       │ │ 마을: 바람숲 마을                      │ │ [13:20] 나무 사이로 종소리가 들립니다.    ░│
│ 상태: 정상    직업: Swordswoman        │ │ 시간: 오후   날씨: 맑음   안전 지역    │ │ [13:21] 광장에서 상인들이 손짓합니다.     ░│
└────────────────────────────────────────┘ └────────────────────────────────────────┘ │ [13:21] 경비병이 고개를 끄덕입니다.       ░│
┌─ PLAYER ART / 36 x 36 ─────────────────┐ ┌─ TOWN VIEW / 36 x 36 ──────────────────┐ │ [13:22] 여관에서 따뜻한 불빛이 샙니다.    ░│
│                                        │ │                            ░░░░        │ │ [13:22] 게시판에 새 의뢰가 붙었습니다.    █│
│                                        │ │    ░░░                        ░░░      │ │ [13:23] 분수 옆에 여행자가 서 있습니다.   █│
│                                        │ │                                        │ │ [13:23] 다음 행동을 선택하세요.           █│
│                 ███████          █     │ │         ░░░               ░░░░░░░      │ │ [휠 스크롤 ↑↓]       1~8 / 8     ▼         │
│               ██▓▓▓▓▓▓▓█         █     │ │       ░░░░░░░      ▓    ░░░░░░░░░░░    │ └────────────────────────────────────────────┘
│              █▓▓▓██████▓█▓      █░█    │ │     ░░░░░░░░░░░   ▓▓█ ░░░░░░▒▒▒░░░░░░  │ ┌─ TOWN ACTIONS / PAGE 1 ────────────────────┐
│              ▓▓██░░░▓▓░█▓█      █░█    │ │   ░░░░░░▒▒▒░░░░░░▓▓█▓▓░░░░▒▒▒▒▒▒▒░░░░  │ │                                            │
│             ▓▓█▓▓▓▓▓░░▓░██▓     █░█    │ │  ░░░░░░▒▒▒▒▒▒░░░▓▓█▓▓█▓░▒▒▒▒▒▒▒▒▒▒▒░░  │ │                                            │
│             ▓▓█▒░░░░░░░▓░█▓     █░█    │ │  ░░░▒▒██▒▒▒▒▒▒▒▓▓█▓▓█▓▓█▒▒▒▒▒▒▒▒▒▒▒▒▒  │ │  ┌────────────────────────────────────┐    │
│             ▓▓█▒░░░░░░░▒█▓█▓    █░█    │ │  ░▒▒▒▒██▓▓▒▒▒▒▒▒▓▒▒▒▒▒█▒▒▒▒▒▒▒▒▒▒▒▒▒▒  │ │  │▶ [1] 대화   · 주민과 이야기        │    │
│             ▓▓█▒░█░░░█░▒█▓█▓    █░█    │ │  ▒▒▒▒▒██▓▒▓▓▒▒▒▒▓┌───┐█▒▒▒▒▒▒▓▒▓▒▒▒▒▒  │ │  └────────────────────────────────────┘    │
│            ▓█▓▒█░░░░░░░▒█▓█▓    █░█    │ │  ▒▒▒▒▒██▒▓▓▒▓▒▒▒▓│○╱ │█▒▒▒▒▒▓▒▓▓▒▒▒▒▒  │ │                                            │
│            ▓█▓▒█░░░▒░░░▒█▓█▓    █░█    │ │  ▒▒▒▒▓▓▒▓▓▒▓▓▒▒▒▓└───┘█▒▒▒▒▓▒▓▓▒▓▓▒▒▒  │ │                                            │
│            ▓█▓▒█▒░▒▒▒░▒▓█▓█▓    █░█    │ │  ▒▒▒▓▓▒▓▓▒▓▓▒▓▓▒▓▒▒▒▒▒█▒▒▒▓▒▓▓▒▓▓▒▓▒▒  │ │                                            │
│            ▓█▓▒█ ▒░░░▒ ▓█▓█▓    █░█    │ │     ▓▒▒▒▒▒▒▒▒█  ▓▒▒▒▒▒█  ▓▒▓▓▒▓▓▒▓▓▒   │ │  ┌────────────────────────────────────┐    │
│            ▓█▓█▓███████▓█▓██    █░█    │ │     ▓▒▒▒▒▒▒▒▒█  ▓▒███▒█   ▓▒▒▒▒▒▒▒█    │ │  │  [2] 상점   · 물건 사고팔기        │    │
│           ███████████████████   █░█    │ │     ▓▒██▒▒██▒█  ▓▒███▒█   ▓▒▒▒▒▒▒▒█    │ │  └────────────────────────────────────┘    │
│          ███████▒▒▒░▒▒▒████████ █░█    │ │     ▓▒╬█▒▒╬█▒█  ▓▒▒▒▒▒█   ▓▒██▒██▒█    │ │                                            │
│         █▒▒▒▒▒██▒▒▒░▒▒▒███▒▒▒▒█ █░█    │ │     ▓▒▒▒▒▒▒▒▒█  ▓▒▒▒▒▒█   ▓▒╬█▒╬█▒█    │ │                                            │
│          ███████▒▒▒░▒▒▒████████ █░█    │ │     ▓▒░▒▒▒░▒▒█  ▓▒▓▓▓▒█   ▓▒▒▒░▒▒▒█    │ │                                            │
│           ▓▓▓▓▓█▓▓▓▓▓▓▓▒██▓▓▓▓▓ █░█    │ │     ▓░▒▒▒░▒▒▒█  ▓▒▓▓▓▒█   ▓▒▒░▒▒▒░█    │ │  ┌────────────────────────────────────┐    │
│           ▓▓▓▓▓██▒▒░▒▒▒▒██▓▓▓▓▓▓███▓   │ │   ░▓▓░░▓▓░░▓▓░░ ▓▒▒▒▒▒█   ▓▒░██▒░▒█    │ │  │  [3] 여관   · 휴식과 회복          │    │
│          ▓▒▓▓▓▓██▒▒░▒▒▒▒██▓▓▓████████  │ │   ░▓▓░░▓▓░░▓▓░░ ▓▒▒▒▒▒█   ▓░▒██░▒▒█    │ │  └────────────────────────────────────┘    │
│          ▓▒▓▓▓████▒░▒▒▒██░▓▓▓▒▓▓ ▓     │ │   ░▓▓░░▓▓░░▓▓░░ ▓▒▒▒▒▒█  █▓▒████▒▒█    │ │                                            │
│         ▓▓▒▓▓▓█▓▓▓▓▓▓▓▓▓▓▓▓▓▓▒▓▓ ▓     │ │   █           █▓▓      ▓▓▒▓▓▒▓▓▒▓▓▒    │ │                                            │
│         █████▓██████▒████▓░░████ ▓     │ │   █ ▒▒ ▓▓ ▒▒  █▒▓      ▓▒▓▓▒▓▓▒▓▓▒▓    │ │                                            │
│         █▒▒▒█▓▓▓▓▒░▒▒▒░▓▓▓▓░████ ▓     │ │   █▄▄▄▄▄▄▄▄▄▄▄█ ▓░░▒░░▓      ▄▓▓▓▄     │ │  ┌────────────────────────────────────┐    │
│         █████▓▓▓▒▒░▒▒▒░▓▓▓▓░████ ▓     │ │   █           █ ▓░▒░░░▓     █     █    │ │  │  [4] 이동   · 다른 장소로          │    │
│         ▒░░▓████▒▒░▒▒▒░████▓░░▒  ▓     │ │     ▒▓▓        ▓░▒░░░▒░▓    █ ▒░▒ █    │ │  └────────────────────────────────────┘    │
│         ▒░░░▓▓▓▒▒▒░▒▒▒░▒▓▓░░░░░ ███    │ │    ▒▓▓▒▓       ▓▒░░░▒░░▓     ▀▓▓▓▀     │ │                                            │
│           ░░░▓▓▓▓▓▓▓▓▓▓▓▓▓░░░   ███    │ │   ▒▓▓▒▓▓▒     ▓▒░░░▒░░░▒▓      ▒▓▓     │ │                                            │
│            ░░▓▓▓▓▒▓  ▓▒▓▓▓░░           │ │    ▓▒▓▓▒      ▓░░░▒░░░▒░▓     ▒▓▓▒▓    │ │                                            │
│              ▓▓▓▓▒▓  ▓▒▓▓▓▓            │ │     ██   ░▒░ ▓░░░▒░░░▒░░░▓   ▒▓▓▒▓▓▒   │ │       ◀ 이전      1 / 3      다음 ▶        │
│             ▓▓▓▓▓▒   ▓▒▒▒▒▒            │ │     ██       ▓░░▒░░░▒░░░▒▓    ▓▒▓▓▒    │ │                                            │
│             ██████   ████████          │ │             ▓░░▒░░░▒░░░▒░░▓░▒░ ██      │ │ 마우스 클릭 / 숫자키 1~4                   │
│                                        │ │             ▓░▒░░░▒░░░▒░░░▓    ██      │ │ ↑↓ 선택   Enter 확인   ←→ 페이지           │
└────────────────────────────────────────┘ └────────────────────────────────────────┘ └────────────────────────────────────────────┘)TOWN";

// 메인 메뉴 시안: 132셀 x 43행, 좌측 대형 게임 로고와 우측 선택지.
static const wchar_t kLayout3[] = LR"MENU(┌─ TextRPG ─────────────────────────────────────────────────────────────────────────┐ ┌─ MAIN MENU / 4 OPTIONS ────────────────────┐
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │                 메인 메뉴                  │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │  ┌────────────────────────────────────┐    │
│                                                                                   │ │  │▶ 1. 새 게임                        │    │
│                                                                                   │ │  └────────────────────────────────────┘    │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│         ─────────────────────────────────────────────────────────────────         │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │  ┌────────────────────────────────────┐    │
│                                                                                   │ │  │  2. 불러오기                       │    │
│  ██████████                            ██    ████████    ████████      ██████     │ │  └────────────────────────────────────┘    │
│      ██                                ██    ██      ██  ██      ██  ██      ██   │ │                                            │
│      ██        ██████    ██      ██  ██████  ██      ██  ██      ██  ██           │ │                                            │
│      ██      ██      ██    ██  ██      ██    ████████    ████████    ██  ██████   │ │                                            │
│      ██      ██████████      ██        ██    ██  ██      ██          ██      ██   │ │                                            │
│      ██      ██            ██  ██      ████  ██    ██    ██          ██      ██   │ │  ┌────────────────────────────────────┐    │
│      ██        ████████  ██      ██      ██  ██      ██  ██            ██████     │ │  │  3. 옵션                           │    │
│                                                                                   │ │  └────────────────────────────────────┘    │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                      TextRPG                                      │ │                                            │
│                                                                                   │ │                                            │
│         ─────────────────────────────────────────────────────────────────         │ │  ┌────────────────────────────────────┐    │
│                                                                                   │ │  │  4. 나가기                         │    │
│                                                                                   │ │  └────────────────────────────────────┘    │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │                                            │
│                                                                                   │ │ 마우스 클릭 / 숫자키 1~4                   │
│                                                                                   │ │                                            │
│                                                                                   │ │ ↑↓ 선택   Enter 확인                       │
│                                                                                   │ │                                            │
└───────────────────────────────────────────────────────────────────────────────────┘ └────────────────────────────────────────────┘)MENU";

static int CellWidth(wchar_t value)
{
    const unsigned c = static_cast<unsigned>(value);
    return (c >= 0x1100 && c <= 0x11FF) || (c >= 0x2E80 && c <= 0xA4CF) ||
        (c >= 0xAC00 && c <= 0xD7A3) || (c >= 0xF900 && c <= 0xFAFF) ||
        (c >= 0xFF01 && c <= 0xFF60) ? 2 : 1;
}

struct SPreviewWindowSearch
{
    const wchar_t* Title;
    HWND Window = nullptr;
};

static BOOL CALLBACK FindPreviewWindow(HWND window, LPARAM parameter)
{
    auto& search = *reinterpret_cast<SPreviewWindowSearch*>(parameter);
    wchar_t title[512]{};
    if (IsWindowVisible(window) && GetWindowTextW(window, title, 512) > 0 && wcsstr(title, search.Title))
    {
        search.Window = window;
        return FALSE;
    }
    return TRUE;
}

static HWND EnlargePreviewWindow(const std::wstring& title)
{
    SPreviewWindowSearch search{title.c_str()};
    // Windows Terminal의 표시 HWND는 의사 콘솔의 GetConsoleWindow()와 별개다.
    // PID를 포함한 고유 제목으로 이 미리보기가 열린 창만 찾는다.
    for (int attempt = 0; attempt < 40 && !search.Window; ++attempt)
    {
        EnumWindows(FindPreviewWindow, reinterpret_cast<LPARAM>(&search));
        if (!search.Window) Sleep(50);
    }
    if (!search.Window) return nullptr;

    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfoW(MonitorFromWindow(search.Window, MONITOR_DEFAULTTONEAREST), &monitor))
        return nullptr;

    const RECT area = monitor.rcWork;
    constexpr LONG kPreferredWidth = 1240;
    constexpr LONG kPreferredHeight = 940;
    const LONG windowWidth = std::min(kPreferredWidth, std::max<LONG>(1, area.right - area.left - 32));
    const LONG windowHeight = std::min(kPreferredHeight, std::max<LONG>(1, area.bottom - area.top - 32));
    const LONG x = area.left + (area.right - area.left - windowWidth) / 2;
    const LONG y = area.top + (area.bottom - area.top - windowHeight) / 2;
    if (IsZoomed(search.Window) || IsIconic(search.Window))
        ShowWindow(search.Window, SW_RESTORE);
    if (!SetWindowPos(search.Window, nullptr, x, y, windowWidth, windowHeight, SWP_NOZORDER | SWP_NOACTIVATE))
        return nullptr;
    Sleep(200);
    return search.Window;
}

int main()
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    const std::wstring title = L"TextRPG - Main Menu UI Preview [" + std::to_wstring(GetCurrentProcessId()) + L"] (ESC to close)";
    SetConsoleTitleW(title.c_str());
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX font{};
    font.cbSize = sizeof(font);
    if (GetCurrentConsoleFontEx(output, FALSE, &font))
    {
        wcscpy_s(font.FaceName, L"Consolas");
        font.dwFontSize = { 0, 16 };
        font.FontWeight = FW_NORMAL;
        SetCurrentConsoleFontEx(output, FALSE, &font);
    }
    std::wistringstream input(kLayout);
    std::vector<std::wstring> rows;
    std::wstring line;
    int width = 1;
    while (std::getline(input, line))
    {
        int cells = 0;
        for (wchar_t c : line) cells += CellWidth(c);
        width = std::max(width, cells);
        rows.push_back(line);
    }
    rows.emplace_back();
    rows.emplace_back(L"STATIC PREVIEW ONLY - Press ESC to close.");
    width = std::max(width + 4, 160);
    rows.resize(std::max<std::size_t>(rows.size(), 52));
    const int height = static_cast<int>(rows.size());
    SMALL_RECT tiny{0, 0, 1, 1};
    SetConsoleWindowInfo(output, TRUE, &tiny);
    COORD bufferSize{static_cast<SHORT>(width), static_cast<SHORT>(height + 1)};
    SetConsoleScreenBufferSize(output, bufferSize);
    COORD largest = GetLargestConsoleWindowSize(output);
    SMALL_RECT window{0, 0, static_cast<SHORT>(std::min(width, static_cast<int>(largest.X)) - 1),
        static_cast<SHORT>(std::min(height + 1, static_cast<int>(largest.Y)) - 1)};
    SetConsoleWindowInfo(output, TRUE, &window);
    if (!EnlargePreviewWindow(title))
        std::wcerr << L"Unable to locate the visible preview window for resizing.\n";
    CONSOLE_CURSOR_INFO cursor{25, FALSE};
    SetConsoleCursorInfo(output, &cursor);
    std::vector<CHAR_INFO> cells(static_cast<std::size_t>(width) * height);
    for (auto& cell : cells) { cell.Char.UnicodeChar = L' '; cell.Attributes = 0x0007; }
    for (int y = 0; y < height; ++y)
    {
        int x = 0;
        for (wchar_t c : rows[static_cast<std::size_t>(y)])
        {
            const int span = CellWidth(c);
            if (x + span > width) break;
            auto& cell = cells[static_cast<std::size_t>(y) * width + x];
            cell.Char.UnicodeChar = c;
            if (span == 2)
            {
                cell.Attributes |= COMMON_LVB_LEADING_BYTE;
                auto& trailing = cells[static_cast<std::size_t>(y) * width + x + 1];
                trailing.Char.UnicodeChar = c;
                trailing.Attributes |= COMMON_LVB_TRAILING_BYTE;
            }
            x += span;
        }
    }
    SMALL_RECT region{0, 0, static_cast<SHORT>(width - 1), static_cast<SHORT>(height - 1)};
    if (!WriteConsoleOutputW(output, cells.data(), {static_cast<SHORT>(width), static_cast<SHORT>(height)}, {0, 0}, &region))
    {
        std::wcerr << L"Unable to display preview. Press any key to close.\n";
        _getwch();
        return 1;
    }
    while (_getwch() != 27) {}
    return 0;
}
