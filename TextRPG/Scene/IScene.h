#pragma once
namespace TextRPG
{
	enum class ESceneAction { None, StartGame, Continue, Quit };
	struct SSceneAction final
	{
		ESceneAction Type = ESceneAction::None;
		std::wstring TargetSceneId;
		explicit operator bool() const noexcept { return Type != ESceneAction::None; }
	};
	class FSceneManager;
	class IScene : public TScene<SSceneAction>
	{
	private:
		friend class FSceneManager;
		bool AllowInput(const SInputEvent& event, SSceneAction& response) override;
		bool AllowInputFrame(const std::vector<SInputEvent>& events, std::vector<SInputEvent>& allowed,
			const std::vector<SInputEvent>*& selected, SSceneAction& response) override;
		FSceneManager* m_owner = nullptr;
		bool m_transitionRequested = false;
	};
}
