#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	class FFrameBuffer;
	class FGameUiBridge;
	class FDataScene;
	class InGameData;
	class FSceneManager : private FSceneHost
	{
	public:
		explicit FSceneManager(const GameData::FGameDataStore& store);
		~FSceneManager();
		[[nodiscard]] bool LoadStartupScene(
			FGameUiBridge& uiManager,
			std::wstring& errorMessage);
		[[nodiscard]] bool LoadSceneById(
			const std::wstring& sceneId,
			FGameUiBridge& uiManager,
			std::wstring& errorMessage);
		[[nodiscard]] bool SetScene(std::unique_ptr<IScene> nextScene);
		[[nodiscard]] bool PrepareSceneById(const std::wstring& sceneId, FGameUiBridge& uiManager,
			const InGameData* progress,
			std::unique_ptr<FDataScene>& output, std::wstring& errorMessage) const;
		// 후보 Scene과 진행을 함께 게시한다. 진입/퇴장 실패는 활성 핸들을 보존한다.
		// old destructor의 종료는 이미 해제한 Scene을 복원하지 않으며 활성 root/저장 파일을 보존한다.
		[[nodiscard]] bool CommitProgress(std::unique_ptr<FDataScene>& nextScene,
			std::unique_ptr<InGameData>& active, std::unique_ptr<InGameData>& candidate, std::wstring& errorMessage);
		// bool은 요청 수락이다. 첫 요청 우선, 실제 게시/OnExit/파괴는 EndFrame만 실행한다.
		// CommitProgress의 active는 앱 세션의 안정된 핸들이며 Manager보다 오래 생존해야 한다.
		// 후보는 수락 시 Manager 소유로 이동한다. TakeProgressResult로 실제 게시 결과를 받는다.
		[[nodiscard]] bool EndFrame(std::wstring& errorMessage);
		[[nodiscard]] bool TakeProgressResult(std::unique_ptr<InGameData>& active, bool& published, std::wstring& errorMessage);
		// 종료 우선: 준비 후보만 취소하고 활성 진행은 보존한다. 이후 요청도 거부한다.
		// 저장 여부는 앱이 결정한다. 파괴 중 종료는 추가 쓰기 없이 루프를 종료한다.
		void CancelPendingTransitions() noexcept;
		[[nodiscard]] bool IsQuitting() const noexcept { return m_isQuitting; }
		// 읽기 전용: Fixed catch-up을 중단하되 실제 소유권 교체는 EndFrame에 남긴다.
		[[nodiscard]] bool HasPendingTransition() const noexcept { return m_pendingScene != nullptr; }
		void SetContinueAvailable(bool available) noexcept;
		void ShowMessage(std::wstring textKey);
		[[nodiscard]] SSceneAction HandleInput(const SInputEvent& inputEvent);
		[[nodiscard]] SSceneAction HandleInputFrame(const std::vector<SInputEvent>& events);
		void Update(float deltaSeconds);
		void Render(FFrameBuffer& frameBuffer) const;

	private:
		friend class IScene;
		const GameData::FGameDataStore& m_store;
		std::unique_ptr<IScene> m_currentScene;
		const InGameData* m_progress = nullptr;
		// 역순 파괴: 후보 Scene/UI가 후보 진행보다 먼저 해제된다.
		std::unique_ptr<InGameData> m_pendingProgress;
		std::unique_ptr<IScene> m_pendingScene;
		std::unique_ptr<InGameData>* m_pendingOwner = nullptr;
		std::unique_ptr<InGameData>* m_completedOwner = nullptr;
		bool m_progressPublished = false;
		std::wstring m_publicationError;
		bool m_isPublishingProgress = false;
		bool m_isQuitting = false;
		bool m_isCancellationDeferred = false;
		std::array<bool, static_cast<std::size_t>(EInputKey::Digit4) + 1> m_confirmHeld{};
		std::array<bool, static_cast<std::size_t>(EInputKey::Digit4) + 1> m_confirmGate{};
		bool m_primaryHeld = false;
		bool m_primaryGate = false;
		std::uint64_t m_currentBatch = 0;
		void MarkTransitionRequested() noexcept;
		[[nodiscard]] bool ObserveInput(const SInputEvent& event) noexcept;
		static void DiscardPreparedScene(std::unique_ptr<IScene>& scene) noexcept;
		[[nodiscard]] bool IsSafePointEligible() const noexcept;
		void DiscardPendingTransitions() noexcept;
		bool m_canContinue = false;
		std::wstring m_messageKey;
	};
}
