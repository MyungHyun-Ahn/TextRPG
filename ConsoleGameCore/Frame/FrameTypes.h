#pragma once
namespace TextRPG
{
	enum class EFrameMode { Basic, FixedUpdate, FrameSkip };

	struct SFrameOptions final
	{
		EFrameMode Mode = EFrameMode::Basic;
		double TargetFps = 60.0;
		int MaxCatchUpUpdates = 5;
		int MaxConsecutiveRenderSkips = 5;
		double MaxAccumulatedElapsedSeconds = 0.25;
	};

	enum class EFrameFailureStage { None, Options, Clock, Start, StartFrame, Input, Update, Render, EndFrame, Stop };

	struct SFrameStatistics final
	{
		std::uint64_t CompletedFrames = 0;
		std::uint64_t UpdateCalls = 0;
		std::uint64_t RenderCalls = 0;
		std::uint64_t SkippedRenders = 0;
		double DroppedSimulationSeconds = 0;
	};
}
