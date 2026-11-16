#pragma once
namespace TextRPG
{
	class FFrame
	{
	public:
		explicit FFrame(const SFrameOptions& options);
		FFrame(const SFrameOptions& options, IFrameClock& clock);
		virtual ~FFrame();
		FFrame(const FFrame&) = delete;
		FFrame& operator=(const FFrame&) = delete;
		FFrame(FFrame&&) = delete;
		FFrame& operator=(FFrame&&) = delete;

		int Run();
		const SFrameOptions& GetOptions() const noexcept;
		EFrameFailureStage GetFailureStage() const noexcept;
		std::exception_ptr GetException() const noexcept;
		const SFrameStatistics& GetStatistics() const noexcept;

	protected:
		virtual void OnStart();
		virtual void OnStartFrame();
		virtual void OnInput();
		virtual void OnUpdate(float deltaSeconds);
		virtual void OnRender();
		virtual void OnEndFrame();
		virtual void OnStop();
		void RequestExit(int exitCode = 0) noexcept;
		void SkipRemainingUpdates() noexcept;

	private:
		void CaptureException() noexcept;
		const SFrameOptions m_options;
		IFrameClock& m_clock; // 비소유: 시계는 Frame보다 오래 살아야 한다.
		bool m_hasRun = false;
		bool m_exitRequested = false;
		bool m_skipUpdates = false;
		int m_exitCode = 0;
		EFrameFailureStage m_stage = EFrameFailureStage::None;
		EFrameFailureStage m_failureStage = EFrameFailureStage::None;
		std::exception_ptr m_exception;
		SFrameStatistics m_statistics;
	};
}
