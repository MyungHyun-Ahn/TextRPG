#include "pch.h"
#ifdef CORE_DEPENDENCY_NEGATIVE
#include "Common/GameDataEnums.g.h"
#endif
#include "Rendering/FFrameBuffer.h"
#include "Input/FInputManager.h"
#include "UI/Common/FUiElement.h"
#include "UI/Common/FUiPanel.h"
#include "UI/Common/FUiButton.h"
#include "UI/Common/FSceneUiRoot.h"
#include "UI/FUIManager.h"
#include "Scene/TScene.h"
#include "Scene/FSceneHost.h"
#include "Frame/FrameTypes.h"
#include "Frame/IFrameClock.h"
#include "Frame/FFrame.h"
#include <cassert>
#include <cstdlib>
#include <crtdbg.h>

using namespace TextRPG;
namespace
{
	class FFakeFrameClock final : public IFrameClock
	{
	public:
		double Time = 0;
		int Samples = 0;
		std::vector<double> Waits;
		std::function<void()> Sample;
		std::function<void(double)> Wait;
		double NowSeconds() override { ++Samples; if (Sample) Sample(); return Time; }
		void WaitUntil(double deadlineSeconds) override
		{
			Waits.push_back(deadlineSeconds);
			if (Wait) Wait(deadlineSeconds); else Time = deadlineSeconds;
		}
	};

	class FCallbackFrame final : public FFrame
	{
	public:
		FCallbackFrame(const SFrameOptions& options, IFrameClock& clock) : FFrame(options, clock) {}
		std::function<void()> Start, StartFrame, Input, Render, EndFrame, Stop;
		std::function<void(float)> Update;
		void Exit(int code = 0) { RequestExit(code); }
		void SkipUpdates() { SkipRemainingUpdates(); }
	private:
		void OnStart() override { if (Start) Start(); }
		void OnStartFrame() override { if (StartFrame) StartFrame(); }
		void OnInput() override { if (Input) Input(); }
		void OnUpdate(float seconds) override { if (Update) Update(seconds); }
		void OnRender() override { if (Render) Render(); }
		void OnEndFrame() override { if (EndFrame) EndFrame(); }
		void OnStop() override { if (Stop) Stop(); }
	};

	// 필요한 종료 훅 하나만 구현하는 독립 소비자다.
	class FOptionalFrame final : public FFrame
	{
	public:
		using FFrame::FFrame;
	private:
		void OnEndFrame() override { RequestExit(); }
	};

	void TestFrameInheritance()
	{
		FFakeFrameClock clock;
		SFrameOptions options;
		FOptionalFrame optional(options, clock);
		options.TargetFps = 30;
		assert(optional.GetOptions().TargetFps == 60);
		assert(optional.Run() == 0);
		assert(optional.GetFailureStage() == EFrameFailureStage::None && !optional.GetException());
		assert(optional.GetStatistics().CompletedFrames == 1 && optional.GetStatistics().UpdateCalls == 1);
		assert(optional.GetStatistics().RenderCalls == 1);
		const int samples = clock.Samples;
		assert(optional.Run() != 0 && clock.Samples == samples);
		assert(optional.GetStatistics().CompletedFrames == 1 && optional.GetFailureStage() == EFrameFailureStage::None);
		std::cout << "PASS FRAME-01 lib Run optional inheritance copied options one-shot statistics\n";
	}

	void TestFrameLifecycle()
	{
		const std::array<EFrameFailureStage, 7> stages = {EFrameFailureStage::Start, EFrameFailureStage::StartFrame,
			EFrameFailureStage::Input, EFrameFailureStage::Update, EFrameFailureStage::Render,
			EFrameFailureStage::EndFrame, EFrameFailureStage::Stop};
		for (std::size_t target = 0; target != stages.size(); ++target)
		{
			for (int action = 0; action != 3; ++action)
			{
				FFakeFrameClock clock;
				FCallbackFrame frame({}, clock);
				std::vector<std::size_t> entries;
				int nested = 0;
				auto hook = [&](std::size_t stage)
				{
					entries.push_back(stage);
					assert(frame.Run() != 0); // 정리 중에도 바깥 실행 상태/원인을 건드리지 않는다.
					++nested;
					if (stage == target)
					{
						if (action == 2) throw 71;
						frame.Exit(action == 1 ? 9 : 0);
					}
					if (stage == 5 || stage == 6) frame.Exit();
				};
				frame.Start = [&] { hook(0); };
				frame.StartFrame = [&] { hook(1); };
				frame.Input = [&] { hook(2); };
				frame.Update = [&](float) { hook(3); };
				frame.Render = [&] { hook(4); };
				frame.EndFrame = [&] { hook(5); };
				frame.Stop = [&] { hook(6); };
				bool escaped = false;
				int result = 0;
				try { result = frame.Run(); }
				catch (...) { escaped = true; }
				std::cout << "TRACE FRAME-02 target=" << target << " action=" << action << " escaped=" << escaped << '\n';
				assert(!escaped);
				assert(action == 0 ? result == 0 : action == 1 ? result == 9 : result != 0);
				assert(frame.GetFailureStage() == (action == 0 ? EFrameFailureStage::None : stages[target]));
				assert(static_cast<bool>(frame.GetException()) == (action == 2));
				if (action == 2)
				{
					int cause = 0;
					try { std::rethrow_exception(frame.GetException()); } catch (int value) { cause = value; }
					assert(cause == 71);
				}
				const auto last = std::min(target, std::size_t{5});
				std::vector<std::size_t> expected;
				for (std::size_t stage = 0; stage <= last; ++stage) expected.push_back(stage);
				expected.push_back(6);
				assert(entries == expected && nested == static_cast<int>(entries.size()));
				assert(frame.GetStatistics().CompletedFrames == ((target == 5 && action == 0) || target == 6 ? 1u : 0u));
				assert(frame.GetStatistics().UpdateCalls == (target >= 3 ? 1u : 0u));
				assert(frame.GetStatistics().RenderCalls == (target >= 4 ? 1u : 0u));
				assert(frame.Run() != 0 && entries == expected);
			}
		}
		FFakeFrameClock clock;
		FCallbackFrame firstFailure({}, clock);
		firstFailure.Start = [&] { firstFailure.Exit(23); throw 41; };
		firstFailure.Stop = [&] { firstFailure.Exit(); firstFailure.Exit(24); throw 42; };
		assert(firstFailure.Run() == 23);
		assert(firstFailure.GetFailureStage() == EFrameFailureStage::Start);
		assert(!firstFailure.GetException()); // 명시된 첫 원인을 후속 throw가 교체하지 않는다.
		FOptionalFrame production(SFrameOptions{});
		assert(production.Run() == 0);
		std::cout << "PASS FRAME-02 all hook exits throws cleanup reentry first cause default clock\n";
	}

	void TestFrameOptions()
	{
		std::vector<SFrameOptions> invalid;
		auto add = [&](auto change) { SFrameOptions options; change(options); invalid.push_back(options); };
		add([](auto& o) { o.Mode = static_cast<EFrameMode>(77); });
		for (double value : {0.0, -1.0, 0.5, 1000.01, std::numeric_limits<double>::infinity(),
			std::numeric_limits<double>::quiet_NaN(), (std::numeric_limits<double>::max)()})
			add([&](auto& o) { o.TargetFps = value; });
		for (int value : {0, -1, 1001, (std::numeric_limits<int>::max)()})
		{
			add([&](auto& o) { o.MaxCatchUpUpdates = value; });
			add([&](auto& o) { o.MaxConsecutiveRenderSkips = value; });
		}
		for (double value : {0.0, -1.0, 60.01, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
			add([&](auto& o) { o.MaxAccumulatedElapsedSeconds = value; });
		for (const auto& options : invalid)
		{
			FFakeFrameClock clock;
			FCallbackFrame frame(options, clock);
			int hooks = 0;
			frame.Start = [&] { ++hooks; frame.Exit(); };
			frame.Stop = [&] { ++hooks; };
			const int result = frame.Run();
			std::cout << "TRACE FRAME-03 invalid result=" << result << " hooks=" << hooks << " samples=" << clock.Samples << '\n';
			assert(result != 0 && frame.GetFailureStage() == EFrameFailureStage::Options);
			assert(hooks == 0 && clock.Samples == 0 && clock.Waits.empty() && !frame.GetException());
			assert(frame.Run() != 0 && hooks == 0 && clock.Samples == 0);
		}
		for (double fps : {1.0, 1000.0})
		{
			SFrameOptions options;
			options.TargetFps = fps;
			options.MaxCatchUpUpdates = fps == 1 ? 1 : 1000;
			options.MaxConsecutiveRenderSkips = options.MaxCatchUpUpdates;
			options.MaxAccumulatedElapsedSeconds = fps == 1 ? (std::numeric_limits<double>::min)() : 60;
			FFakeFrameClock clock;
			FOptionalFrame frame(options, clock);
			assert(frame.Run() == 0);
		}
		std::cout << "PASS FRAME-03 invalid options no hooks clock and valid inclusive boundaries\n";
	}

	void TestBasicFrameTiming()
	{
		SFrameOptions options;
		options.TargetFps = 8;
		FFakeFrameClock clock;
		clock.Time = 10;
		FCallbackFrame frame(options, clock);
		std::vector<float> deltas;
		int inputs = 0;
		int renders = 0;
		int ends = 0;
		frame.Start = [&] { clock.Time += 2; };
		frame.Input = [&] { ++inputs; };
		frame.Update = [&](float seconds)
		{
			deltas.push_back(seconds);
			if (ends == 0) clock.Time += 0.03125;
			if (ends == 1) clock.Time += 0.25;
		};
		frame.Render = [&] { ++renders; if (ends < 2) clock.Time += 0.03125; };
		frame.EndFrame = [&] { if (++ends == 3) frame.Exit(); };
		assert(frame.Run() == 0);
		std::cout << "TRACE FRAME-04 deltas=" << deltas[0] << ',' << deltas[1] << ',' << deltas[2]
			<< " waits=" << clock.Waits.size() << '\n';
		assert(deltas == std::vector<float>({0, 0.125f, 0.28125f}));
		assert(clock.Waits == std::vector<double>({12.125}));
		assert(inputs == 3 && renders == 3 && ends == 3);
		assert(frame.GetStatistics().CompletedFrames == 3 && frame.GetStatistics().DroppedSimulationSeconds == 0);
		FFakeFrameClock skipClock;
		FCallbackFrame skipped(options, skipClock);
		int outer = 0;
		int updates = 0;
		skipped.StartFrame = [&] { if (outer == 0) skipped.SkipUpdates(); };
		skipped.Input = [&] { if (outer == 1) skipped.SkipUpdates(); };
		skipped.Update = [&](float) { ++updates; if (outer == 2) skipped.SkipUpdates(); };
		skipped.EndFrame = [&] { if (++outer == 4) skipped.Exit(); };
		assert(skipped.Run() == 0 && updates == 2);
		assert(skipped.GetStatistics().CompletedFrames == 4 && skipped.GetStatistics().RenderCalls == 4);
		std::cout << "PASS FRAME-04 Basic actual elapsed remaining-budget wait overrun optional update skip reset\n";
	}

	void TestFrameClockFailures()
	{
		for (const auto mode : {EFrameMode::Basic, EFrameMode::FixedUpdate, EFrameMode::FrameSkip})
		{
			for (int fault = 0; fault != 10; ++fault)
			{
				SFrameOptions options;
				options.Mode = mode;
				options.TargetFps = 8;
				FFakeFrameClock clock;
				clock.Time = 1;
				FCallbackFrame frame(options, clock);
				int starts = 0;
				int stops = 0;
				int ends = 0;
				frame.Start = [&] { ++starts; };
				frame.EndFrame = [&] { if (++ends == 3) frame.Exit(); };
				frame.Stop = [&] { ++stops; frame.Exit(); throw 99; };
				clock.Sample = [&]
				{
					if (clock.Samples == 1)
					{
						if (fault == 0) clock.Time = std::numeric_limits<double>::quiet_NaN();
						if (fault == 1) clock.Time = std::numeric_limits<double>::infinity();
						if (fault == 2) clock.Time = -std::numeric_limits<double>::infinity();
						if (fault == 3) clock.Time = (std::numeric_limits<double>::max)();
						if (fault == 4) clock.Time = -(std::numeric_limits<double>::max)();
						if (fault == 7) throw 71;
					}
					if (ends == 1 && fault == 5) clock.Time = 0.5;
					if (ends == 1 && fault == 6) clock.Time = 1e40;
				};
				if (fault == 8) clock.Wait = [](double) { throw 72; };
				if (fault == 9) clock.Wait = [&](double) { clock.Time = -1; };
				const int result = frame.Run();
				std::cout << "TRACE FRAME-05 mode=" << static_cast<int>(mode) << " fault=" << fault
					<< " stage=" << static_cast<int>(frame.GetFailureStage()) << " ends=" << ends << '\n';
				assert(result != 0 && frame.GetFailureStage() == EFrameFailureStage::Clock);
				assert(starts == 1 && stops == 1);
				assert(frame.GetStatistics().CompletedFrames == (fault == 5 || fault == 6 || fault == 8 || fault == 9 ? 1u : 0u));
				assert(static_cast<bool>(frame.GetException()) == (fault == 7 || fault == 8));
				if (frame.GetException())
				{
					int cause = 0;
					try { std::rethrow_exception(frame.GetException()); } catch (int value) { cause = value; }
					assert(cause == (fault == 7 ? 71 : 72));
				}
			}
		}
		std::cout << "PASS FRAME-05 invalid backward unrepresentable throwing clocks wait cleanup preserves completed frame\n";
	}

	void TestFixedFrameTicks()
	{
		SFrameOptions options;
		options.Mode = EFrameMode::FixedUpdate;
		options.TargetFps = 8;
		options.MaxAccumulatedElapsedSeconds = 1;
		FFakeFrameClock clock;
		FCallbackFrame frame(options, clock);
		std::vector<int> counts;
		std::vector<float> deltas;
		int updates = 0;
		int inputs = 0;
		int ends = 0;
		frame.Input = [&] { ++inputs; };
		frame.Update = [&](float seconds) { ++updates; deltas.push_back(seconds); };
		frame.Render = [&]
		{
			counts.push_back(updates);
			updates = 0;
			if (ends == 0) clock.Time += 0.125;
			if (ends == 1) clock.Time += 0.375;
		};
		frame.EndFrame = [&] { if (++ends == 3) frame.Exit(); };
		assert(frame.Run() == 0);
		std::cout << "TRACE FRAME-06 ticks=" << counts[0] << ',' << counts[1] << ',' << counts[2] << '\n';
		assert(counts == std::vector<int>({0, 1, 3}));
		assert(deltas == std::vector<float>(4, 0.125f));
		assert(inputs == 3 && ends == 3 && clock.Waits.empty());
		assert(frame.GetStatistics().CompletedFrames == 3 && frame.GetStatistics().UpdateCalls == 4);
		assert(frame.GetStatistics().RenderCalls == 3 && frame.GetStatistics().DroppedSimulationSeconds == 0);
		std::cout << "PASS FRAME-06 Fixed initial zero exact one multiple ticks constant delta single input render EndFrame\n";
	}

	void TestFixedFrameBudget()
	{
		SFrameOptions options;
		options.Mode = EFrameMode::FixedUpdate;
		options.TargetFps = 8;
		options.MaxCatchUpUpdates = 2;
		options.MaxAccumulatedElapsedSeconds = 0.5625;
		FFakeFrameClock clock;
		FCallbackFrame frame(options, clock);
		std::vector<int> counts;
		int updates = 0;
		int outer = 0;
		frame.Update = [&](float seconds) { assert(seconds == 0.125f); ++updates; };
		frame.Render = [&]
		{
			counts.push_back(updates);
			updates = 0;
			if (outer == 0) clock.Time += 1.0625;
			if (outer == 1) clock.Time += 0.1875;
		};
		frame.EndFrame = [&] { if (++outer == 3) frame.Exit(); };
		assert(frame.Run() == 0);
		std::cout << "TRACE FRAME-07 ticks=" << counts[0] << ',' << counts[1] << ',' << counts[2]
			<< " dropped=" << frame.GetStatistics().DroppedSimulationSeconds << '\n';
		assert(counts == std::vector<int>({0, 2, 2}));
		assert(frame.GetStatistics().DroppedSimulationSeconds == 0.75);
		assert(frame.GetStatistics().CompletedFrames == 3 && clock.Waits.empty());
		for (int action = 0; action != 3; ++action)
		{
			FFakeFrameClock exitClock;
			FCallbackFrame exitFrame(options, exitClock);
			int ticks = 0;
			int ends = 0;
			int renders = 0;
			int stops = 0;
			exitFrame.Update = [&](float) { ++ticks; if (action == 2) throw 85; exitFrame.Exit(action); };
			exitFrame.Render = [&] { ++renders; exitClock.Time += 0.5; };
			exitFrame.EndFrame = [&] { if (++ends == 3) exitFrame.Exit(); };
			exitFrame.Stop = [&] { ++stops; };
			const int result = exitFrame.Run();
			assert(action == 0 ? result == 0 : result != 0);
			assert(ticks == 1 && ends == 1 && renders == 1 && stops == 1);
			assert(exitFrame.GetFailureStage() == (action == 0 ? EFrameFailureStage::None : EFrameFailureStage::Update));
			assert(exitFrame.GetStatistics().CompletedFrames == 1 && exitFrame.GetStatistics().UpdateCalls == 1);
		}
		std::cout << "PASS FRAME-07 Fixed clamp budget discard whole debt preserve fraction exit between ticks\n";
	}

	void TestFixedFrameTransitionDebt()
	{
		for (int skipStage = 0; skipStage != 3; ++skipStage)
		{
			SFrameOptions options;
			options.Mode = EFrameMode::FixedUpdate;
			options.TargetFps = 8;
			options.MaxAccumulatedElapsedSeconds = 1;
			FFakeFrameClock clock;
			FCallbackFrame frame(options, clock);
			int outer = 0;
			int updates = 0;
			int inputs = 0;
			int skipHits = 0;
			std::vector<int> counts;
			auto skip = [&](int stage)
			{
				if (outer == 1 && stage == skipStage) { ++skipHits; frame.SkipUpdates(); }
			};
			frame.StartFrame = [&] { skip(0); };
			frame.Input = [&] { ++inputs; skip(1); };
			frame.Update = [&](float seconds) { assert(seconds == 0.125f); ++updates; skip(2); };
			frame.Render = [&]
			{
				counts.push_back(updates);
				updates = 0;
				if (outer == 0) clock.Time += 0.4375;
				if (outer == 1) clock.Time += 0.1875;
			};
			frame.EndFrame = [&] { if (++outer == 3) frame.Exit(); };
			assert(frame.Run() == 0);
			std::cout << "TRACE FRAME-08 stage=" << skipStage << " ticks=" << counts[0] << ',' << counts[1] << ',' << counts[2]
				<< " dropped=" << frame.GetStatistics().DroppedSimulationSeconds << '\n';
			assert(counts == std::vector<int>({0, skipStage == 2 ? 1 : 0, 1}));
			assert(skipHits == 1 && inputs == 3 && frame.GetStatistics().CompletedFrames == 3);
			assert(frame.GetStatistics().DroppedSimulationSeconds == (skipStage == 2 ? 0.3125 : 0.4375));
		}
		std::cout << "PASS FRAME-08 transition skips remaining fixed ticks drops all old fractional debt resets next frame\n";
	}

	void TestFrameSkipTiming()
	{
		SFrameOptions options;
		options.Mode = EFrameMode::FrameSkip;
		options.TargetFps = 8;
		FFakeFrameClock clock;
		FCallbackFrame frame(options, clock);
		std::vector<int> renderFrames;
		std::vector<float> deltas;
		int inputs = 0;
		int outer = 0;
		frame.Input = [&] { ++inputs; };
		frame.Update = [&](float seconds) { deltas.push_back(seconds); };
		frame.Render = [&] { renderFrames.push_back(outer); if (outer == 0) clock.Time += 0.5; };
		frame.EndFrame = [&] { if (++outer == 5) frame.Exit(); };
		assert(frame.Run() == 0);
		std::cout << "TRACE FRAME-09 renders=" << renderFrames.size() << " skips=" << frame.GetStatistics().SkippedRenders << '\n';
		assert(renderFrames == std::vector<int>({0, 3, 4}));
		assert(deltas == std::vector<float>(5, 0.125f));
		assert(inputs == 5 && outer == 5 && frame.GetStatistics().CompletedFrames == 5);
		assert(frame.GetStatistics().UpdateCalls == 5 && frame.GetStatistics().RenderCalls == 3);
		assert(frame.GetStatistics().SkippedRenders == 2 && frame.GetStatistics().DroppedSimulationSeconds == 0);
		assert(clock.Waits.empty());
		FFakeFrameClock aheadClock;
		FCallbackFrame ahead(options, aheadClock);
		int ends = 0;
		ahead.Input = [&] { aheadClock.Time += 0.03125; };
		ahead.Render = [&] { aheadClock.Time += 0.03125; };
		ahead.EndFrame = [&] { if (++ends == 3) ahead.Exit(); };
		assert(ahead.Run() == 0 && ahead.GetStatistics().SkippedRenders == 0);
		assert(aheadClock.Waits == std::vector<double>({0.125, 0.25}));
		std::cout << "PASS FRAME-09 Skip fixed update input EndFrame retained logical deadlines catch-up ahead wait\n";
	}

	void TestFrameSkipBudget()
	{
		for (const bool slowForcedRender : {false, true})
		{
			SFrameOptions options;
			options.Mode = EFrameMode::FrameSkip;
			options.TargetFps = 8;
			options.MaxConsecutiveRenderSkips = 2;
			FFakeFrameClock clock;
			FCallbackFrame frame(options, clock);
			std::vector<int> renderFrames;
			int outer = 0;
			int inputs = 0;
			frame.Input = [&] { ++inputs; };
			frame.Update = [&](float seconds) { assert(seconds == 0.125f); };
			frame.Render = [&]
			{
				renderFrames.push_back(outer);
				if (outer == 0) clock.Time += 0.875;
				if (outer == 3 && slowForcedRender) clock.Time += 0.375;
			};
			frame.EndFrame = [&] { if (++outer == (slowForcedRender ? 6 : 5)) frame.Exit(); };
			assert(frame.Run() == 0);
			std::cout << "TRACE FRAME-10 slow-forced=" << slowForcedRender << " renders=" << renderFrames.size()
				<< " skipped=" << frame.GetStatistics().SkippedRenders << " dropped="
				<< frame.GetStatistics().DroppedSimulationSeconds << '\n';
			assert(renderFrames == std::vector<int>({0, 3, slowForcedRender ? 5 : 4}));
			assert(frame.GetStatistics().DroppedSimulationSeconds == 0.375);
			assert(frame.GetStatistics().SkippedRenders == (slowForcedRender ? 3u : 2u));
			assert(frame.GetStatistics().CompletedFrames == static_cast<std::uint64_t>(outer));
			assert(frame.GetStatistics().UpdateCalls == static_cast<std::uint64_t>(inputs));
			assert(clock.Waits == (slowForcedRender ? std::vector<double>{} : std::vector<double>{1.0}));
		}
		std::cout << "PASS FRAME-10 Skip bounded forced render rebase dropped lag fresh budget slow render keeps next skips\n";
	}

	void TestFixedFrameExactBoundaries()
	{
		for (double fps : {3.0, 7.0, 10.0, 30.0, 50.0, 60.0, 144.0, 1000.0})
		{
			for (int ticks : {1, 5, 10, 100})
			{
				SFrameOptions options;
				options.Mode = EFrameMode::FixedUpdate;
				options.TargetFps = fps;
				options.MaxCatchUpUpdates = 1000;
				options.MaxAccumulatedElapsedSeconds = 60;
				FFakeFrameClock clock;
				FCallbackFrame frame(options, clock);
				int outer = 0;
				int updates = 0;
				frame.Update = [&](float seconds) { assert(seconds == static_cast<float>(1.0 / fps)); ++updates; };
				frame.Render = [&] { if (outer == 0) clock.Time += ticks * (1.0 / fps); };
				frame.EndFrame = [&] { if (++outer == 2) frame.Exit(); };
				assert(frame.Run() == 0);
				std::cout << "TRACE FRAME-11 fps=" << fps << " expected=" << ticks << " actual=" << updates << '\n';
				assert(updates == ticks && frame.GetStatistics().DroppedSimulationSeconds == 0);
			}
			SFrameOptions options;
			options.Mode = EFrameMode::FixedUpdate;
			options.TargetFps = fps;
			options.MaxAccumulatedElapsedSeconds = 60;
			FFakeFrameClock clock;
			FCallbackFrame below(options, clock);
			int outer = 0;
			int updates = 0;
			below.Update = [&](float) { ++updates; };
			below.Render = [&] { if (outer == 0) clock.Time = std::nextafter(3 * (1.0 / fps), 0.0); };
			below.EndFrame = [&] { if (++outer == 2) below.Exit(); };
			assert(below.Run() == 0 && updates == 2);
		}
		std::cout << "PASS FRAME-11 nonbinary exact tick multiples no repeated subtraction loss strict below-boundary control\n";
	}

	class FQueuedInput final : public IInputSource
	{
	public:
		std::vector<SInputEvent> Events;
		std::size_t Next = 0;
		EInputReadResult ReadEvent(SInputEvent& event) noexcept override
		{
			if (Next == Events.size()) return EInputReadResult::Timeout;
			event = Events[Next++];
			return EInputReadResult::Event;
		}
	};
	struct SResult { bool Done = false; explicit operator bool() const noexcept { return Done; } };
	class FConsumerUi final : public FUiPanel
	{
	public:
		explicit FConsumerUi(int& activated) : FUiPanel({0, 0, 20, 5})
		{
			SetTitle(L"neutral");
			assert(AddChild(std::make_unique<FUiButton>(SCellRect{0, 0, 12, 3}, L"activate", [&activated] { ++activated; })));
		}
	};
	class FConsumerScene final : public TScene<SResult>, public IUiInputTarget
	{
	public:
		FConsumerScene() : Root(SCellRect{0, 0, 24, 8}) {}
		void DispatchUiInput(const SInputEvent& event) override { (void)HandleInput(event); }
		void CancelUiInput() override { Root.CancelInput(); }
		FSceneUiRoot* GetUiBaseRoot() const noexcept override { return const_cast<FSceneUiRoot*>(&Root); }
		FSceneUiRoot Root;
		std::function<void()> Callback;
		std::function<void()> BusyQuery;
	private:
		void OnEnterImpl() override {}
		void OnExitImpl() override {}
		void UpdateImpl(float) override { if (Callback) Callback(); }
		SResult HandleInputImpl(const SInputEvent& event) override { Root.Dispatch(event, true); return {}; }
		void RenderImpl(FFrameBuffer& frame) const override { Root.Render(frame, false); }
		bool IsOwnedUiBusy() const noexcept override { if (BusyQuery) BusyQuery(); return Root.IsBusy(); }
	};
#ifdef _DEBUG
	struct SResizeAllocationFault final
	{
		bool Armed = false;
		int Ordinal = 0;
		int Seen = 0;
		int Hits = 0;
		std::size_t Bytes = 0;
	};
	SResizeAllocationFault g_resizeFault;

	int __cdecl ResizeAllocationHook(int type, void*, std::size_t size, int block, long, const unsigned char*, int)
	{
		if (type == _HOOK_ALLOC && block == _NORMAL_BLOCK && g_resizeFault.Armed && size == g_resizeFault.Bytes)
		{
			if (++g_resizeFault.Seen == g_resizeFault.Ordinal)
			{
				g_resizeFault.Armed = false;
				++g_resizeFault.Hits;
				return 0;
			}
		}
		return 1;
	}
#endif

	void TestFrameResizeTransaction()
	{
#ifdef _DEBUG
		bool preservedAll = true;
		for (const int ordinal : {1, 2})
		{
			FFrameBuffer frame(3, 2);
			frame.DrawText(0, 0, L"가A", 0x0003);
			const auto cells = frame.GetCells();
			const auto attributes = frame.GetAttributes();
			g_resizeFault = {true, ordinal, 0, 0, 17 * 11 * sizeof(wchar_t)};
			const auto previous = _CrtSetAllocHook(ResizeAllocationHook);
			bool caught = false;
			try { frame.Resize(17, 11); }
			catch (const std::bad_alloc&) { caught = true; }
			(void)_CrtSetAllocHook(previous);
			g_resizeFault.Armed = false;
			const bool preserved = frame.GetWidth() == 3 && frame.GetHeight() == 2 &&
				frame.GetCells() == cells && frame.GetAttributes() == attributes;
			std::cout << "TRACE CORE-03 allocation=" << ordinal << " seen=" << g_resizeFault.Seen
				<< " fault-hit=" << g_resizeFault.Hits << " bad-alloc=" << caught << " preserved=" << preserved << '\n';
			assert(caught && g_resizeFault.Hits == 1 && g_resizeFault.Seen == ordinal);
			preservedAll = preservedAll && preserved;
			if (!preserved) continue; // RED에서 불변식이 깨진 객체에 DrawText를 호출하여 실제 OOB를 만들지 않는다.
			frame.DrawText(2, 1, L"Z", 0x0005);
			assert(frame.GetCells()[5] == L'Z' && frame.GetAttributes()[5] == 0x0005);
			frame.Resize(17, 11);
			assert(frame.GetWidth() == 17 && frame.GetHeight() == 11);
			assert(frame.GetCells() == std::vector<wchar_t>(17 * 11, L' '));
			assert(frame.GetAttributes() == std::vector<std::uint16_t>(17 * 11, 0x0007));
			frame.DrawText(16, 10, L"R", 0x0002);
			assert(frame.GetCells().back() == L'R' && frame.GetAttributes().back() == 0x0002);
		}
		if (!preservedAll)
		{
			std::cout << "FAIL CORE-03 throwing Resize published inconsistent dimensions/storage\n";
			std::exit(90);
		}
		g_resizeFault = {false, 1, 0, 0, 17 * 11 * sizeof(wchar_t)};
		const auto previous = _CrtSetAllocHook(ResizeAllocationHook);
		FFrameBuffer control(17, 11);
		(void)_CrtSetAllocHook(previous);
		assert(g_resizeFault.Hits == 0 && g_resizeFault.Seen == 0 && control.GetCells().size() == 17 * 11);
#else
		std::cout << "SCOPE CORE-03 CRT allocation fault injection is Debug-only\n";
#endif
		FFrameBuffer frame(3, 2);
#ifndef _WIN64
		bool lengthCaught = false;
		try { frame.Resize((std::numeric_limits<int>::max)(), (std::numeric_limits<int>::max)()); }
		catch (const std::length_error&) { lengthCaught = true; }
		assert(lengthCaught && frame.GetWidth() == 3 && frame.GetHeight() == 2 && frame.GetCells().size() == 6);
		std::cout << "TRACE CORE-03 Win32 checked-product length-error=1 previous-storage=1\n";
#endif
		frame.Resize(-1, 2);
		assert(frame.GetWidth() == 0 && frame.GetHeight() == 2 && frame.GetCells().empty() && frame.GetAttributes().empty());
		frame.DrawText(0, 1, L"ignored");
		frame.Resize(2, -1);
		assert(frame.GetWidth() == 2 && frame.GetHeight() == 0 && frame.GetCells().empty() && frame.GetAttributes().empty());
		frame.Clear();
		frame.Resize(2, 1);
		frame.DrawText(0, 0, L"ok");
		assert(frame.GetCells() == std::vector<wchar_t>({L'o', L'k'}));
		frame.Resize(2, 1);
		assert(frame.GetCells() == std::vector<wchar_t>(2, L' ') && frame.GetAttributes() == std::vector<std::uint16_t>(2, 7));
		std::cout << "PASS CORE-03 first second allocation strong rollback old drawable content recovery clamp empty reset controls\n";
	}
}
int main()
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	_set_abort_behavior(0, _CALL_REPORTFAULT | _WRITE_ABORT_MSG);
#ifdef _DEBUG
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
	std::cout << std::unitbuf;
	TestFrameInheritance();
	TestFrameLifecycle();
	TestFrameOptions();
	TestBasicFrameTiming();
	TestFrameClockFailures();
	TestFixedFrameTicks();
	TestFixedFrameBudget();
	TestFixedFrameTransitionDebt();
	TestFrameSkipTiming();
	TestFrameSkipBudget();
	TestFixedFrameExactBoundaries();
	TestFrameResizeTransaction();
	// 외부 소유 핸들은 실행 중인 Scene과 직접 root callback에서 해제되지 않는다.
	FSceneHost lifetimeHost;
	auto active = std::make_unique<FConsumerScene>();
	auto pending = std::make_unique<FConsumerScene>();
	int firstQueryHits = 0;
	int forbiddenPublications = 0;
	active->BusyQuery = [&]
	{
		if (++firstQueryHits != 1) return;
		const bool nested = lifetimeHost.TryEndFrame(active.get(), pending.get(), [&] { ++forbiddenPublications; });
		assert(!nested && forbiddenPublications == 0);
	};
	assert(lifetimeHost.IsSafePointEligible(active.get(), pending.get()));
	assert(firstQueryHits == 1 && forbiddenPublications == 0);
	active->BusyQuery = {};
	int published = 0;
	auto publish = [&] { ++published; active = std::move(pending); };
	active->Callback = [&]
	{
		assert(!lifetimeHost.TryEndFrame(active.get(), pending.get(), publish));
		assert(active && pending && published == 0);
	};
	active->Update(0);
	assert(published == 0 && active && pending);
	assert(lifetimeHost.TryEndFrame(active.get(), pending.get(), [&]
	{
		assert(!lifetimeHost.TryEndFrame(active.get(), pending.get(), publish));
		publish();
	}));
	assert(published == 1 && active && !pending);
	try { (void)lifetimeHost.TryEndFrame(active.get(), nullptr, [] { throw 7; }); }
	catch (int) {}
	assert(lifetimeHost.IsSafePointEligible(active.get(), nullptr));
	std::cout << "PASS generic safe-point blocks active callbacks/reentry and recovers throw before lifetime publication\n";

	FConsumerScene scene;
	int activated = 0;
	assert(scene.Root.AddBase(std::make_unique<FConsumerUi>(activated)) != 0);
	FFrameBuffer frame;
	frame.Resize(24, 8);
	FSceneHost host;
	int callbacks = 0;
	scene.Callback = [&]
	{
		++callbacks;
		assert(scene.IsOperating());
		assert(!host.IsSafePointEligible(&scene, nullptr));
	};
	std::vector<int> order;
	FFakeFrameClock frameClock;
	FCallbackFrame pipeline({}, frameClock);
	FQueuedInput source;
	SInputEvent key;
	key.Type = EInputEventType::KeyDown;
	key.Key = EInputKey::Enter;
	key.ScreenWidth = 24;
	key.ScreenHeight = 8;
	source.Events.push_back(key);
	FInputManager input(source);
	FUIManager bridge;
	bridge.Bind(scene);
	assert(bridge.GetBoundTarget() == &scene && bridge.GetBoundRoot() == &scene.Root);
	pipeline.Input = [&] { order.push_back(1); assert(input.Update() == EInputReadResult::Event);
		assert(input.WasKeyPressed(EInputKey::Enter));
		for (const auto& event : input.GetEvents()) bridge.HandleInput(event); };
	pipeline.Update = [&](float) { order.push_back(2); scene.Update(0); };
	pipeline.Render = [&] { order.push_back(3); scene.Render(frame); };
	pipeline.EndFrame = [&] { order.push_back(4); assert(host.IsSafePointEligible(&scene, nullptr)); pipeline.Exit(); };
	const bool success = pipeline.Run() == 0;
	assert(frame.GetCells()[0] == L'┌' && frame.GetCells()[2] == L'n');
	assert(success && callbacks == 1 && activated == 1 && order == std::vector<int>({1, 2, 3, 4}));
	std::cout << "PASS standalone derived UI input/render and generic Scene safe-point/frame order\n";
	order.clear();
	FCallbackFrame failedPresent({}, frameClock);
	failedPresent.Input = [&] { order.push_back(1); };
	failedPresent.Update = [&](float) { order.push_back(2); };
	failedPresent.Render = [&] { order.push_back(3); failedPresent.Exit(1); };
	failedPresent.EndFrame = [&] { order.push_back(4); };
	assert(failedPresent.Run() != 0);
	assert(failedPresent.GetFailureStage() == EFrameFailureStage::Render);
	assert(failedPresent.GetStatistics().CompletedFrames == 0);
	assert(order == std::vector<int>({1, 2, 3}));
	std::cout << "PASS present failure does not publish EndFrame\n";
	bridge.Clear();
	assert(bridge.GetBoundTarget() == nullptr && bridge.GetBoundRoot() == nullptr);
	return 0;
}
