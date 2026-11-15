#pragma once

namespace GameData { class FGameDataStore; }

namespace TextRPG
{
	class InGameData;
	class FSceneManager;
	class FGameUiBridge;

	enum class EGameStartResult
	{
		None,
		Queued,
		Failed,
		Started,
		StartedSaveFailed,
	};

	// 앱 수명에서 SceneManager보다 먼저 생성/나중 파괴한다. Scene/UI는 이 루트를 소유하지 않는다.
	class FGameSession final
	{
	public:
		explicit FGameSession(const GameData::FGameDataStore& store);
		~FGameSession();
		[[nodiscard]] const InGameData* GetProgress() const noexcept;
		[[nodiscard]] bool CanContinue(FSceneManager& scenes, FGameUiBridge& ui, std::string& error) const;
		[[nodiscard]] EGameStartResult NewGame(const std::wstring& sceneId, FSceneManager& scenes, FGameUiBridge& ui,
			std::string& error);
		[[nodiscard]] bool Continue(FSceneManager& scenes, FGameUiBridge& ui, std::string& error);
		// NewGame/Continue는 요청만 수락한다. EndFrame 후 실제 게시/저장 결과를 한 번 받는다.
		[[nodiscard]] EGameStartResult TakeStartResult(FSceneManager& scenes, std::string& error);
		[[nodiscard]] bool Save(std::string& error) const;

	private:
		const GameData::FGameDataStore& m_store;
		std::unique_ptr<InGameData> m_progress;
		bool m_hasPendingStart = false;
		bool m_pendingAutosave = false;
		[[nodiscard]] bool ReadCandidate(std::unique_ptr<InGameData>& candidate, std::string& error) const;
	};
}
