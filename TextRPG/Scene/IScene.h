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
	// 게임 입력 정책만 연결한다. Scene은 Manager의 private 소유권/전환 상태를 직접 보지 않는다.
	class ISceneInputPolicy
	{
	public:
		virtual ~ISceneInputPolicy() = default;
		virtual bool FilterSceneInput(const SInputEvent& event, bool dataScene, SSceneAction& response) = 0;
		virtual bool FilterSceneInputFrame(const std::vector<SInputEvent>& events, bool dataScene, bool transitionRequested,
			std::vector<SInputEvent>& allowed, const std::vector<SInputEvent>*& selected, SSceneAction& response) = 0;
	};
	class IScene : public TScene<SSceneAction>
	{
	public:
		void SetInputPolicy(ISceneInputPolicy& policy) noexcept { m_inputPolicy = &policy; }
	private:
		bool AllowInput(const SInputEvent& event, SSceneAction& response) override;
		bool AllowInputFrame(const std::vector<SInputEvent>& events, std::vector<SInputEvent>& allowed,
			const std::vector<SInputEvent>*& selected, SSceneAction& response) override;
		ISceneInputPolicy* m_inputPolicy = nullptr;
	};
}
