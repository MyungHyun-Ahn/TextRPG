#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	class FGameUiBridge;
	class FDataScene;
	class InGameData;

	class FTextRPGSceneManager final : public TSceneManager<SSceneAction>, private ISceneInputPolicy
	{
	public:
		explicit FTextRPGSceneManager(const GameData::FGameDataStore& store);
		~FTextRPGSceneManager() override;
		[[nodiscard]] bool LoadStartupScene(FGameUiBridge& uiManager, std::wstring& errorMessage);
		[[nodiscard]] bool LoadSceneById(std::uint32_t sceneId, FGameUiBridge& uiManager, std::wstring& errorMessage);
		[[nodiscard]] bool PrepareSceneById(std::uint32_t sceneId, FGameUiBridge& uiManager,
			const InGameData* progress, std::unique_ptr<FDataScene>& output, std::wstring& errorMessage) const;
		// active는 Manager보다 오래 사는 Session의 안정된 owner다. bool은 수락이며 실제 완료는 별도로 소비한다.
		[[nodiscard]] bool CommitProgress(std::unique_ptr<FDataScene>& nextScene,
			std::unique_ptr<InGameData>& active, std::unique_ptr<InGameData>& candidate, std::wstring& errorMessage);
		[[nodiscard]] bool TakeProgressResult(std::unique_ptr<InGameData>& active, bool& published, std::wstring& errorMessage);
		void SetContinueAvailable(bool available) noexcept;
		void ShowMessage(std::uint32_t textKey);
		void SetSaveRetryAvailable(bool available) noexcept;

	private:
		bool ValidateCandidate(const FScene& scene) const override;
		bool ValidatePublication(const FScene& scene) const override;
		bool ValidateDataScene(const FScene& scene, const InGameData* progress) const;
		void EnterPreparedScene(FScene& scene) override;
		void OnSceneLeaving(FScene& scene) noexcept override;
		void RetireScene(FScene& scene) noexcept override;
		void RestoreSceneAfterFailedPublication(FScene& scene) noexcept override;
		void OnTransitionRequested(FScene* scene, bool requested) noexcept override;
		void InvalidatePendingScene(FScene* scene) noexcept override;
		void OnTransitionSnapshot() noexcept override;
		void PublishScene(SPublication& publication) noexcept override;
		void OnScenePublished(FScene& scene) noexcept override;
		void OnTransitionComplete(ESceneTransitionResult result, std::wstring& error) override;
		bool AllowManagerInput(const SInputEvent& event, SSceneAction& response) override;
		bool AllowManagerInputFrame(const std::vector<SInputEvent>& events, SSceneAction& response) override;
		void RenderOverlay(FFrameBuffer& frame) const override;
		bool FilterSceneInput(const SInputEvent& event, bool dataScene, SSceneAction& response) override;
		bool FilterSceneInputFrame(const std::vector<SInputEvent>& events, bool dataScene, bool transitionRequested,
			std::vector<SInputEvent>& allowed, const std::vector<SInputEvent>*& selected, SSceneAction& response) override;
		[[nodiscard]] bool ObserveInput(const SInputEvent& event) noexcept;

		const GameData::FGameDataStore& m_store;
		const InGameData* m_progress = nullptr;
		// typed 협력 문맥일 뿐 Scene owner/별도 전환 큐는 없다. Scene은 Core의 한 snapshot만 사용한다.
		struct SProgressTransition final
		{
			std::unique_ptr<InGameData> Progress;
			std::unique_ptr<InGameData>* Owner = nullptr;
		};
		SProgressTransition m_pendingProgress;
		SProgressTransition m_publicationProgress;
		const InGameData* m_acceptProgress = nullptr;
		std::unique_ptr<InGameData>* m_completedOwner = nullptr;
		bool m_progressPublished = false;
		bool m_restorePreviousBinding = false;
		std::wstring m_publicationError;
		std::array<bool, static_cast<std::size_t>(EInputKey::Digit4) + 1> m_confirmHeld{};
		std::array<bool, static_cast<std::size_t>(EInputKey::Digit4) + 1> m_confirmGate{};
		bool m_primaryHeld = false;
		bool m_primaryGate = false;
		std::uint64_t m_currentBatch = 0;
		bool m_canContinue = false;
		std::uint32_t m_messageKey{};
	};
}
