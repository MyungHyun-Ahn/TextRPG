#pragma once
namespace TextRPG
{
	class FFramePipeline final
	{
	public:
		// 실패한 입력/출력 프레임은 게시하지 않는다. 게임 행동/저장 정책은 각 hook 소유다.
		template<class TInput, class TUpdate, class TPresent, class TEndFrame>
		bool Run(TInput&& input, TUpdate&& update, TPresent&& present, TEndFrame&& endFrame)
		{
			if (m_isRunning) return false;
			struct SScope final
			{
				bool& Running;
				explicit SScope(bool& running) : Running(running) { Running = true; }
				~SScope() { Running = false; }
			} scope(m_isRunning);
			if (!input()) return false;
			update();
			if (!present()) return false;
			endFrame();
			return true;
		}
	private:
		bool m_isRunning = false;
	};
}
