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
#include "Frame/FFramePipeline.h"
#include <cassert>
#include <cstdlib>
#include <crtdbg.h>

using namespace TextRPG;
namespace
{
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
	FFramePipeline pipeline;
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
	const bool success = pipeline.Run(
		[&] { order.push_back(1); assert(input.Update() == EInputReadResult::Event);
			assert(input.WasKeyPressed(EInputKey::Enter));
			for (const auto& event : input.GetEvents()) bridge.HandleInput(event); return true; },
		[&] { order.push_back(2); scene.Update(0); },
		[&] { order.push_back(3); scene.Render(frame); return true; },
		[&] { order.push_back(4); assert(host.IsSafePointEligible(&scene, nullptr)); });
	assert(frame.GetCells()[0] == L'┌' && frame.GetCells()[2] == L'n');
	assert(success && callbacks == 1 && activated == 1 && order == std::vector<int>({1, 2, 3, 4}));
	std::cout << "PASS standalone derived UI input/render and generic Scene safe-point/frame order\n";
	order.clear();
	assert(!pipeline.Run([&] { order.push_back(1); return true; }, [&] { order.push_back(2); },
		[&] { order.push_back(3); return false; }, [&] { order.push_back(4); }));
	assert(order == std::vector<int>({1, 2, 3}));
	std::cout << "PASS present failure does not publish EndFrame\n";
	bridge.Clear();
	assert(bridge.GetBoundTarget() == nullptr && bridge.GetBoundRoot() == nullptr);
	return 0;
}
