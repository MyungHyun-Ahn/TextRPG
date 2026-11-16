#pragma once
/*
TextRPG — BATTLE UI (가로 3열 구성 / 콘솔 전용)

왼쪽: 플레이어 Lv·이름·HP바 + 36×36 여검사 아트
가운데: 몬스터 Lv·이름·HP바 + 36×36 드래곤 아트
오른쪽: 8줄 배틀로그(휠 스크롤 표시) + 세로 4개 선택지

전체 레이아웃: 132 문자 셀 × 43줄 (설명 제외)
※ 본 문서는 화면 배치 시안이며, 휠/버튼은 실제 입력 이벤트를 처리하지 않습니다.
※ 고정폭 폰트 및 유니코드 폭 지원 콘솔에서 열어 주세요.

┌─ PLAYER STATUS ────────────────────────┐ ┌─ MONSTER STATUS ───────────────────────┐ ┌─ BATTLE LOG / 8 LINES ─────────────────────┐
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
└────────────────────────────────────────┘ └────────────────────────────────────────┘ └────────────────────────────────────────────┘

*/

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	class FFrameBuffer;
	class InGameData;
	class FSceneManager;
	class FSceneUiRoot;
	class FGameUiBridge;
	struct SUIOption final
	{
		std::uint32_t OptionId = 0;
		std::uint32_t OptionOrder = 0;
		std::wstring Text;
		::GameData::Common::ESceneActionType ActionType = ::GameData::Common::ESceneActionType::StartGame;
		std::wstring TargetSceneId;
		bool IsEnabled = true;
	};

	class FDataScene final : public IScene, public IUiInputTarget
	{
	public:
		FDataScene(const GameData::FGameDataStore& store, FGameUiBridge& uiManager,
			const InGameData* progress = nullptr, bool canContinue = false);
		~FDataScene() override;

		// 직접 API도 NVI와 같은 Scene 작업 수명을 쓴다. callback/인수 정리 반환 전 EndFrame 게시를 허용하지 않는다.
		[[nodiscard]] bool LoadFromGameData(const std::wstring& sceneId, std::wstring& errorMessage);

		[[nodiscard]] const InGameData* GetProgress() const noexcept { const SOperation operation(*this); return m_progress; }
		// 기존 참조 API는 로드된 기본 Layer가 있을 때만 사용한다. 동적 Layer/빈 Scene은 nullable API를 쓴다.
		[[nodiscard]] FSceneUiRoot& GetUiRoot() noexcept;
		[[nodiscard]] FSceneUiRoot* GetUiBaseRoot() const noexcept override;
		// 반환 포인터는 비소유 뷰이며 다음 Layer commit/Scene 재로드까지만 유효하다.
		// open/close/visible은 요청이며 Flush 또는 입력 dispatch 뒤 commit한다. 0 ID는 없음/실패다.
		// level 0은 현재/대기 Layer의 최고 level + 1, 같은 level은 나중에 열린 Layer가 앞이다.
		[[nodiscard]] std::uint64_t OpenUiLayer(std::unique_ptr<IUI> ui = nullptr, std::uint32_t level = 0);
		[[nodiscard]] FUiId AddUiToLayer(std::uint64_t layerId, std::unique_ptr<IUI> ui);
		void CloseUiLayer(std::uint64_t layerId);
		void SetUiLayerVisible(std::uint64_t layerId, bool visible);
		void FlushUiLayerCommands();
		void DiscardUiLayerCommands();
		void CancelUiInput() override;
		void DispatchUiInput(const SInputEvent& event) override { (void)HandleInput(event); }
		[[nodiscard]] FSceneUiRoot* GetUiLayer(std::uint64_t layerId) const noexcept;
		[[nodiscard]] IUI* FindUi(FUiId id) const noexcept;
		[[nodiscard]] std::uint64_t GetUiTopLayer() const noexcept;
		[[nodiscard]] std::uint32_t GetUiTopLevel() const noexcept;
		[[nodiscard]] std::size_t GetUiLayerCount() const noexcept;


	private:
		bool IsOwnedUiBusy() const noexcept override;
		void OnEnterImpl() override;
		void OnExitImpl() override;
		void UpdateImpl(float deltaSeconds) override;
		[[nodiscard]] SSceneAction HandleInputImpl(const SInputEvent& inputEvent) override;
		[[nodiscard]] SSceneAction HandleInputFrameImpl(const std::vector<SInputEvent>& events) override;
		void RenderImpl(FFrameBuffer& frameBuffer) const override;
		friend class FSceneManager;
		// 안전점에서만 호출한다. 가상 UI destructor는 재진입할 수 있으며 root보다 먼저 반환한다.
		void RetireForReplacement() noexcept;
		void InvalidateUiInputForShutdown() noexcept;
		std::wstring m_sceneId;
		const InGameData* m_progress = nullptr;
		bool m_canContinue = false;
		bool m_isRetired = false;
		bool m_replacementRequested = false;
		const GameData::FGameDataStore& m_store;
		FGameUiBridge& m_uiManager;
		struct SUiLayer final
		{
			std::uint64_t Id = 0;
			std::uint32_t Level = 0;
			bool Visible = true;
			std::unique_ptr<FSceneUiRoot> Root;
		};
		struct SUiLayerCommand final
		{
			SUiLayer Layer;
			std::uint64_t Target = 0;
			bool IsVisibility = false;
			bool Visible = true;
		};
		// 실제 소유권·겹침 순서의 유일한 원본이다. root에는 팝업/다른 Layer 목록이 없다.
		std::vector<SUiLayer> m_uiLayers;
		std::vector<SUiLayerCommand> m_uiLayerCommands;
		FUiId m_nextUiId = 1;
		std::uint64_t m_baseLayer = 0;
		std::uint64_t m_uiTopLayer = 0;
		std::uint64_t m_currentBatch = 0;
		std::uint64_t m_dropBatch = 0;
		// UI 경계의 재확정 방지 latch다. 기기 상태 서비스/물리 키 저장소가 아니다.
		std::array<bool, static_cast<std::size_t>(EInputKey::Digit4) + 1> m_confirmHeld{};
		bool m_primaryConfirmHeld = false;
		bool m_isHandlingFrame = false;
		bool m_dropFrame = false;
		std::uint64_t m_layerGeneration = 0;
		bool m_isDispatching = false;
		bool m_isCommitting = false;
		mutable bool m_isRendering = false;
		bool m_discardLayerCommands = false;
		void RefreshUiTopLayer();
		[[nodiscard]] bool IsUiBusy() const noexcept;
		SSceneAction m_pendingAction;

	};
}
