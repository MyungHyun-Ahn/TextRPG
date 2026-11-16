#include "pch.h"
#include "Frame/FrameTypes.h"
#include "Frame/IFrameClock.h"
#include "Frame/FFrame.h"

namespace TextRPG
{
	namespace
	{
		class FSteadyFrameClock final : public IFrameClock
		{
		public:
			double NowSeconds() override
			{
				return std::chrono::duration<double>(std::chrono::steady_clock::now() - m_origin).count();
			}
			void WaitUntil(double deadlineSeconds) override
			{
				for (double remaining = deadlineSeconds - NowSeconds(); remaining > 0; remaining = deadlineSeconds - NowSeconds())
					std::this_thread::sleep_for(std::chrono::duration<double>(std::min(remaining, 1.0)));
			}
		private:
			const std::chrono::steady_clock::time_point m_origin = std::chrono::steady_clock::now();
		};

		IFrameClock& DefaultClock()
		{
			static FSteadyFrameClock clock;
			return clock;
		}
	}

	FFrame::FFrame(const SFrameOptions& options) : FFrame(options, DefaultClock()) {}
	FFrame::FFrame(const SFrameOptions& options, IFrameClock& clock) : m_options(options), m_clock(clock) {}
	FFrame::~FFrame() = default;
	const SFrameOptions& FFrame::GetOptions() const noexcept { return m_options; }
	EFrameFailureStage FFrame::GetFailureStage() const noexcept { return m_failureStage; }
	std::exception_ptr FFrame::GetException() const noexcept { return m_exception; }
	const SFrameStatistics& FFrame::GetStatistics() const noexcept { return m_statistics; }
	void FFrame::OnStart() {}
	void FFrame::OnStartFrame() {}
	void FFrame::OnInput() {}
	void FFrame::OnUpdate(float) {}
	void FFrame::OnRender() {}
	void FFrame::OnEndFrame() {}
	void FFrame::OnStop() {}
	void FFrame::SkipRemainingUpdates() noexcept { m_skipUpdates = true; }

	void FFrame::RequestExit(int exitCode) noexcept
	{
		m_exitRequested = true;
		if (m_exitCode == 0 && exitCode != 0)
		{
			m_exitCode = exitCode;
			m_failureStage = m_stage;
		}
	}

	void FFrame::CaptureException() noexcept
	{
		if (m_exitCode == 0) m_exception = std::current_exception();
		RequestExit(1);
	}

	int FFrame::Run()
	{
		if (m_hasRun) return 1;
		m_hasRun = true;
		const double periodSeconds = 1.0 / m_options.TargetFps;
		const bool knownMode = m_options.Mode == EFrameMode::Basic || m_options.Mode == EFrameMode::FixedUpdate ||
			m_options.Mode == EFrameMode::FrameSkip;
		if (!knownMode || !std::isfinite(m_options.TargetFps) || m_options.TargetFps < 1 || m_options.TargetFps > 1000 ||
			m_options.MaxCatchUpUpdates < 1 || m_options.MaxCatchUpUpdates > 1000 ||
			m_options.MaxConsecutiveRenderSkips < 1 || m_options.MaxConsecutiveRenderSkips > 1000 ||
			!std::isfinite(m_options.MaxAccumulatedElapsedSeconds) || m_options.MaxAccumulatedElapsedSeconds <= 0 ||
			m_options.MaxAccumulatedElapsedSeconds > 60 || !std::isfinite(periodSeconds) || periodSeconds <= 0)
		{
			m_stage = EFrameFailureStage::Options;
			RequestExit(1);
			return m_exitCode;
		}
		try
		{
			m_stage = EFrameFailureStage::Start;
			OnStart();
			double previousStartSeconds = 0;
			double accumulatedSeconds = 0;
			double scheduledDeadlineSeconds = 0;
			int consecutiveSkips = 0;
			double lastSampleSeconds = 0;
			bool hasSample = false;
			bool firstFrame = true;
			auto readClock = [&](double& seconds)
			{
				m_stage = EFrameFailureStage::Clock;
				seconds = m_clock.NowSeconds();
				if (!std::isfinite(seconds) || (hasSample && seconds < lastSampleSeconds))
				{
					RequestExit(1);
					return false;
				}
				lastSampleSeconds = seconds;
				hasSample = true;
				return !m_exitRequested;
			};
			while (!m_exitRequested)
			{
				double startSeconds = 0;
				if (!readClock(startSeconds)) break;
				const double elapsedSeconds = firstFrame ? 0 : startSeconds - previousStartSeconds;
				const double deadlineBaseSeconds = !firstFrame && m_options.Mode == EFrameMode::FrameSkip ?
					scheduledDeadlineSeconds : startSeconds;
				double deadlineSeconds = deadlineBaseSeconds + periodSeconds;
				if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0 ||
					(m_options.Mode == EFrameMode::Basic && elapsedSeconds > (std::numeric_limits<float>::max)()) ||
					!std::isfinite(deadlineSeconds) || deadlineSeconds <= deadlineBaseSeconds ||
					!std::isfinite(startSeconds + periodSeconds) || startSeconds + periodSeconds <= startSeconds)
				{
					RequestExit(1);
					break;
				}
				firstFrame = false;
				scheduledDeadlineSeconds = deadlineSeconds;
				previousStartSeconds = startSeconds;
				m_skipUpdates = false;
				m_stage = EFrameFailureStage::StartFrame;
				OnStartFrame();
				if (m_exitRequested) break;
				m_stage = EFrameFailureStage::Input;
				OnInput();
				if (m_exitRequested) break;
				m_stage = EFrameFailureStage::Update;
				if (m_options.Mode == EFrameMode::FixedUpdate)
				{
					const double acceptedSeconds = std::min(elapsedSeconds, m_options.MaxAccumulatedElapsedSeconds);
					m_statistics.DroppedSimulationSeconds += elapsedSeconds - acceptedSeconds;
					accumulatedSeconds += acceptedSeconds;
					const double availableSeconds = accumulatedSeconds;
					// 옵션 검증으로 누적량은 61초 미만, 전체 tick은 61000 미만이다.
					int wholeTicks = static_cast<int>(availableSeconds / periodSeconds);
					if (wholeTicks * periodSeconds > availableSeconds) --wholeTicks;
					else if ((wholeTicks + 1) * periodSeconds <= availableSeconds) ++wholeTicks;
					int updates = 0;
					while (!m_skipUpdates && !m_exitRequested && updates < wholeTicks && updates < m_options.MaxCatchUpUpdates)
					{
						++updates;
						// 반복 뺄셈의 오차로 정확한 N주기에서 N-1회가 되는 것을 막는다.
						accumulatedSeconds = availableSeconds - updates * periodSeconds;
						++m_statistics.UpdateCalls;
						OnUpdate(static_cast<float>(periodSeconds));
					}
					if (m_skipUpdates)
					{
						// 전환 이전 Scene의 소수 tick도 새 Scene으로 전달하지 않는다.
						m_statistics.DroppedSimulationSeconds += accumulatedSeconds;
						accumulatedSeconds = 0;
					}
					else if (!m_exitRequested && wholeTicks > updates)
					{
						const double remainderSeconds = availableSeconds - wholeTicks * periodSeconds;
						m_statistics.DroppedSimulationSeconds += accumulatedSeconds - remainderSeconds;
						accumulatedSeconds = remainderSeconds;
					}
				}
				else if (!m_skipUpdates)
				{
					++m_statistics.UpdateCalls;
					OnUpdate(static_cast<float>(m_options.Mode == EFrameMode::FrameSkip ? periodSeconds : elapsedSeconds));
				}
				if (m_exitRequested) break;
				bool skipRender = false;
				if (m_options.Mode == EFrameMode::FrameSkip)
				{
					double renderSeconds = 0;
					if (!readClock(renderSeconds)) break;
					const bool behind = renderSeconds > deadlineSeconds;
					skipRender = behind && consecutiveSkips < m_options.MaxConsecutiveRenderSkips;
					if (behind && !skipRender)
					{
						const double rebasedDeadlineSeconds = renderSeconds + periodSeconds;
						if (!std::isfinite(rebasedDeadlineSeconds) || rebasedDeadlineSeconds <= renderSeconds)
						{
							RequestExit(1);
							break;
						}
						m_statistics.DroppedSimulationSeconds += renderSeconds - deadlineSeconds;
						// 강제 출력 직전 재기준화: 이 Render의 긴 작업은 다음 회차의 지연으로 남긴다.
						scheduledDeadlineSeconds = deadlineSeconds = rebasedDeadlineSeconds;
					}
				}
				if (skipRender)
				{
					++consecutiveSkips;
					++m_statistics.SkippedRenders;
				}
				else
				{
					consecutiveSkips = 0;
					m_stage = EFrameFailureStage::Render;
					++m_statistics.RenderCalls;
					OnRender();
				}
				if (m_exitRequested) break;
				m_stage = EFrameFailureStage::EndFrame;
				OnEndFrame();
				if (m_exitCode == 0) ++m_statistics.CompletedFrames;
				if (m_exitRequested) break;
				double endSeconds = 0;
				if (!readClock(endSeconds)) break;
				if (endSeconds < deadlineSeconds) m_clock.WaitUntil(deadlineSeconds);
			}
		}
		catch (...) { CaptureException(); }
		m_stage = EFrameFailureStage::Stop;
		try { OnStop(); }
		catch (...) { CaptureException(); }
		m_stage = EFrameFailureStage::None;
		return m_exitCode;
	}
}
