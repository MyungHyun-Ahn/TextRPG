#include "pch.h"
#include "Frame/FrameTypes.h"
#include "Frame/IFrameClock.h"
#include "Frame/FFrame.h"
#include "Rendering/FFrameBuffer.h"
#include "UI/Common/FUiElement.h"
#include "UI/Common/FUiPanel.h"
#include "UI/Common/FUiButton.h"
#include "UI/Common/FSceneUiRoot.h"
#include "Common/GameDataEnums.g.h"
#include "UI/FUIManager.h"
#include "UI/FGameUiBridge.h"
#include "Scene/FDataScene.h"
#include "Scene/TSceneManager.h"
#include "Scene/FTextRPGSceneManager.h"
#include "Input/FConsoleInput.h"
#include "Input/FInputManager.h"
#include "UI/Common/FUiTextArtView.h"
#include "UI/Common/FUiTextArt.h"
#include <cassert>
#include "GameData/FGameDataPackage.h"
#include "GameData/FGameDataStore.h"
#include "RuntimeFixture.h"
#include "Localization/Localization.h"
#include "Serialization/FSerializationArchive.h"
#include "InGameDataAll.h"
#include "../InGameData/LegacyProgressFixture.h"
#include "InGameData/InGameData.h"
#include "InGameData/FSaveDataStore.h"
#include "InGameData/FGameSession.h"
#include "GameData/FGameDataKey.h"
#include "SceneOption/SceneOptionData.g.h"
#include "NewGameProfile/NewGameProfileData.g.h"
#include <iterator>
#include <crtdbg.h>

using namespace TextRPG;

namespace
{
	constexpr auto kMainMenuSceneId = static_cast<std::uint32_t>(GameData::Common::ESceneId::MainMenu);
	constexpr auto kTownSceneId = static_cast<std::uint32_t>(GameData::Common::ESceneId::Town);
	constexpr auto kBattleFieldSceneId = static_cast<std::uint32_t>(GameData::Common::ESceneId::BattleField);
	constexpr auto kMainLogoArtId = static_cast<std::uint32_t>(GameData::Common::EArtId::MainLogo);
	constexpr auto kMonsterDragonArtId = static_cast<std::uint32_t>(GameData::Common::EArtId::MonsterDragon);
	constexpr auto kHpTextKey = static_cast<std::uint32_t>(GameData::Common::ETextKey::InGameData_Ui_Hp);
	constexpr auto kGoldTextKey = static_cast<std::uint32_t>(GameData::Common::ETextKey::InGameData_Ui_Gold);
	constexpr auto kNewGameTextKey = static_cast<std::uint32_t>(GameData::Common::ETextKey::MainMenu_NewGame);
	constexpr auto kMainMenuTitleTextKey = static_cast<std::uint32_t>(GameData::Common::ETextKey::Scene_MainMenu_Title);
	constexpr std::uint32_t kMissingDataId = 4294967295u;
	static_assert(kMainMenuSceneId == 1 && kTownSceneId == 2 && kBattleFieldSceneId == 3);
	static_assert(kMainLogoArtId == 1 && kMonsterDragonArtId == 3);
	static_assert(kHpTextKey == 22 && kGoldTextKey == 25 && kNewGameTextKey == 1 && kMainMenuTitleTextKey == 17);
}

// 옛 root 팝업 안전 회귀를 실제 Scene Layer 경로에서 재사용한다.
// 정책 이름을 유지하지 않는다: 모든 Layer는 입력을 독점한다.
class FLayerFixture final
{
public:
	explicit FLayerFixture(SCellRect = {}) : Scene(Store, Bridge)
	{
		Base = Scene.OpenUiLayer(nullptr, 1);
		Scene.FlushUiLayerCommands();
	}
	FUiId AddBase(std::unique_ptr<IUI> ui) { return Scene.AddUiToLayer(Base, std::move(ui)); }
	FUiId OpenPopup(std::unique_ptr<IUI> ui)
	{
		auto* pointer = ui.get();
		const auto layer = Scene.OpenUiLayer(std::move(ui));
		const auto id = pointer ? pointer->GetId() : 0;
		Popups.push_back({id, layer});
		return id;
	}
	void ClosePopup(FUiId id)
	{
		for (const auto& popup : Popups) if (popup.first == id) Scene.CloseUiLayer(popup.second);
	}
	void FlushCommands() { Scene.FlushUiLayerCommands(); }
	void DiscardCommands() { Scene.DiscardUiLayerCommands(); }
	IUI* Find(FUiId id) const { return Scene.FindUi(id); }
	FUiId GetFocusId() const
	{
		const auto* top = Scene.GetUiLayer(Scene.GetUiTopLayer());
		return top ? top->GetFocusId() : 0;
	}
	FUiId GetCaptureId() const
	{
		const auto* top = Scene.GetUiLayer(Scene.GetUiTopLayer());
		return top ? top->GetCaptureId() : 0;
	}
	void Dispatch(SInputEvent event)
	{
		event.ScreenWidth = 132;
		event.ScreenHeight = 43;
		(void)Scene.HandleInput(event);
	}
	void Render(FFrameBuffer& frame) const
	{
		FFrameBuffer logical(132, 43);
		Scene.Render(logical);
		frame.Clear();
		for (int y = 0; y < frame.GetHeight() && y < 43; ++y)
		{
			const auto width = std::min(frame.GetWidth(), 132);
			frame.DrawText(0, y, std::wstring(logical.GetCells().data() + y * 132, static_cast<std::size_t>(width)));
		}
	}
	GameData::FGameDataStore Store;
	FGameUiBridge Bridge;
	FDataScene Scene;
	std::uint64_t Base = 0;
	std::vector<std::pair<FUiId, std::uint64_t>> Popups;
};

class FDragFixture final : public FUiButton
{
public:
	FDragFixture(int& clicks, std::array<int, 4>& phases)
		: FUiButton({0, 0, 8, 3}, L"drag", [&clicks]() { ++clicks; })
		, m_phases(phases)
	{
	}
	bool SupportsDrag() const noexcept override
	{
		return true;
	}
	EUiReply OnDrag(EUiDragPhase phase, const SInputEvent&) override
	{
		++m_phases[static_cast<std::size_t>(phase)];
		return EUiReply::Handled;
	}

private:
	std::array<int, 4>& m_phases;
};

class FRenderMutationFixture final : public FUiPanel
{
public:
	FRenderMutationFixture(FLayerFixture& root, FUiId& id)
		: FUiPanel({0, 0, 8, 4})
		, m_root(root)
		, m_idRef(id)
	{
	}
	void Render(const SUiRenderContext& context) const override
	{
		m_root.ClosePopup(m_idRef);
		m_root.FlushCommands();
		assert(m_root.Find(m_idRef) == this);
		FUiPanel::Render(context);
	}

private:
	FLayerFixture& m_root;
	FUiId& m_idRef;
};

class FCancelFixture final : public FUiButton
{
public:
	FCancelFixture(FSceneUiRoot& root, int& cancels)
		: FUiButton({0, 0, 8, 3}, L"cancel", []() {})
		, m_root(root)
		, m_cancels(cancels)
	{
	}
	void OnPointerCancel() override
	{
		++m_cancels;
		if (m_cancels == 1)
		{
			m_root.CancelInput();
		}
	}

private:
	FSceneUiRoot& m_root;
	int& m_cancels;
};

class FCoordinateFixture final : public FUiButton
{
public:
	FCoordinateFixture(SCellRect rect, SInputEvent& received)
		: FUiButton(rect, L"coordinates", []() {})
		, m_received(received)
	{
	}
	EUiReply OnPointerDown(const SInputEvent& event) override
	{
		m_received = event;
		return EUiReply::Capture;
	}
	EUiReply OnPointerMove(const SInputEvent& event) override
	{
		m_received = event;
		return EUiReply::Handled;
	}

private:
	SInputEvent& m_received;
};

class FKeyboardFixture final : public FUiButton
{
public:
	FKeyboardFixture(int& activations, int& pointerClicks)
		: FUiButton({0, 0, 8, 3}, L"key", [&activations]() { ++activations; })
		, m_pointerClicks(pointerClicks)
	{
	}
	EUiReply OnClick(const SInputEvent& event) override
	{
		++m_pointerClicks;
		return FUiButton::OnClick(event);
	}

private:
	int& m_pointerClicks;
};

void TestRestoreChain()
{
	FLayerFixture root({0, 0, 20, 10});
	auto base = std::make_unique<FUiPanel>(SCellRect{0, 0, 20, 10});
	base->AddChild(std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"first", []() {}));
	auto second = std::make_unique<FUiButton>(SCellRect{0, 4, 8, 3}, L"second", []() {});
	auto* secondPtr = second.get();
	base->AddChild(std::move(second));
	root.AddBase(std::move(base));
	SInputEvent event;
	event.Type = EInputEventType::KeyDown;
	event.Key = EInputKey::Down;
	event.ScreenWidth = 20;
	event.ScreenHeight = 10;
	root.Dispatch(event);
	assert(root.GetFocusId() == secondPtr->GetId());
	const auto popup = []() { return std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"popup", []() {}); };
	const auto a = root.OpenPopup(popup());
	root.FlushCommands();
	const auto b = root.OpenPopup(popup());
	root.FlushCommands();
	root.ClosePopup(a);
	root.FlushCommands();
	root.ClosePopup(b);
	root.FlushCommands();
	assert(root.GetFocusId() == secondPtr->GetId());
	std::cout << "PASS R1 middle popup restores second base button\n";
}

class FReleaseMutationFixture final : public FUiButton
{
public:
	FReleaseMutationFixture(int& clicks, std::function<void()> mutation)
		: FUiButton({0, 0, 8, 3}, L"release", [&clicks]() { ++clicks; })
		, m_mutation(std::move(mutation))
	{
	}
	EUiReply OnPointerUp(const SInputEvent&) override
	{
		m_mutation();
		return EUiReply::Handled;
	}
private:
	std::function<void()> m_mutation;
};

void TestReleaseMutation()
{
	for (int action = 0; action < 4; ++action)
	{
		FSceneUiRoot root({0, 0, 20, 10});
		int clicks = 0;
		auto parent = std::make_unique<FUiPanel>(SCellRect{0, 0, 20, 10});
		auto* owner = parent.get();
		parent->AddChild(std::make_unique<FReleaseMutationFixture>(clicks, [&]()
		{
			if (action == 0) owner->SetVisible(false);
			if (action == 1) owner->SetEnabled(false);
			if (action == 2) owner->SetRect({10, 0, 20, 10});
			if (action == 3) root.CancelInput();
		}));
		root.AddBase(std::move(parent));
		SInputEvent event;
		event.ScreenWidth = 20;
		event.ScreenHeight = 10;
		event.MouseX = 2;
		event.MouseY = 2;
		event.Type = EInputEventType::PointerDown;
		root.Dispatch(event);
		event.Type = EInputEventType::PointerUp;
		root.Dispatch(event);
		assert(clicks == 0 && root.GetCaptureId() == 0);
	}
	std::cout << "PASS R2 release callback ancestor state geometry cancellation revalidated\n";
}

class FGestureCancellationFixture final : public FUiButton
{
public:
	FGestureCancellationFixture(FSceneUiRoot& root, int stage)
		: FUiButton({0, 0, 8, 3}, L"cancel", []() {})
		, m_root(root)
		, m_stage(stage)
	{
	}
	bool SupportsDrag() const noexcept override { return true; }
	EUiReply OnPointerDown(const SInputEvent&) override
	{
		if (m_stage == 1) m_root.CancelInput();
		return EUiReply::Capture;
	}
	EUiReply OnPointerMove(const SInputEvent&) override
	{
		if (m_cancelled) ++AfterCancel;
		if (m_stage == 2) m_root.CancelInput();
		return EUiReply::Handled;
	}
	EUiReply OnDrag(EUiDragPhase phase, const SInputEvent&) override
	{
		if (phase == EUiDragPhase::Cancel) m_cancelled = true;
		else if (m_cancelled) ++AfterCancel;
		if (phase == EUiDragPhase::Begin && m_stage == 0) m_root.CancelInput();
		return EUiReply::Handled;
	}
	int AfterCancel = 0;
private:
	FSceneUiRoot& m_root;
	int m_stage;
	bool m_cancelled = false;
};

void TestGestureCancellation()
{
	for (int stage = 0; stage < 3; ++stage)
	{
		FSceneUiRoot root;
		auto button = std::make_unique<FGestureCancellationFixture>(root, stage);
		auto* observed = button.get();
		root.AddBase(std::move(button));
		SInputEvent event;
		event.ScreenWidth = 20;
		event.ScreenHeight = 10;
		event.MouseX = 2;
		event.MouseY = 1;
		event.Type = EInputEventType::PointerDown;
		root.Dispatch(event);
		if (stage != 1)
		{
			event.MouseX = 3;
			event.Type = EInputEventType::PointerMove;
			root.Dispatch(event);
		}
		assert(observed->AfterCancel == 0 && root.GetCaptureId() == 0);
		event.Type = EInputEventType::PointerUp;
		root.Dispatch(event);
		assert(observed->AfterCancel == 0);
	}
	std::cout << "PASS R3 cancellation in down drag-begin pointer-move stops gesture delivery\n";
}

void TestClippedFocus()
{
	FSceneUiRoot root({0, 0, 20, 10});
	int clicks = 0;
	auto clipped = std::make_unique<FUiButton>(SCellRect{25, 0, 8, 3}, L"clipped", [&]() { ++clicks; }, 1);
	auto* clippedPtr = clipped.get();
	root.AddBase(std::move(clipped));
	assert(root.GetFocusId() == 0);
	SInputEvent event;
	event.ScreenWidth = 40;
	event.ScreenHeight = 20;
	event.Key = EInputKey::Enter;
	root.Dispatch(event);
	assert(clicks == 0 && root.GetFocusId() == 0);
	auto panel = std::make_unique<FUiPanel>(SCellRect{0, 0, 15, 9});
	auto first = std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"first", [&]() { ++clicks; }, 2);
	auto* firstPtr = first.get();
	panel->AddChild(std::move(first));
	panel->AddChild(std::make_unique<FUiButton>(SCellRect{16, 0, 8, 3}, L"outside-parent", [&]() { ++clicks; }, 3));
	auto last = std::make_unique<FUiButton>(SCellRect{0, 4, 8, 3}, L"last", [&]() { ++clicks; }, 4);
	auto* lastPtr = last.get();
	panel->AddChild(std::move(last));
	root.AddBase(std::move(panel));
	event.Key = EInputKey::Down;
	root.Dispatch(event);
	assert(root.GetFocusId() == lastPtr->GetId());
	event.Key = EInputKey::Digit1;
	root.Dispatch(event);
	event.Key = EInputKey::Digit3;
	root.Dispatch(event);
	assert(clicks == 0 && root.GetFocusId() == lastPtr->GetId());
	root.SuspendInput();
	lastPtr->SetRect({30, 4, 8, 3});
	root.ResumeInput();
	assert(root.GetFocusId() == firstPtr->GetId());
	lastPtr->SetRect({12, 4, 8, 3});
	event.Key = EInputKey::Digit4;
	event.Type = EInputEventType::KeyDown;
	root.Dispatch(event);
	assert(clicks == 1 && root.GetFocusId() == lastPtr->GetId());
	event.Type = EInputEventType::Resize;
	event.ScreenWidth = 10;
	root.Dispatch(event);
	assert(root.GetFocusId() == firstPtr->GetId());
	assert(clippedPtr->GetId() != root.GetFocusId());
	std::cout << "PASS R4 effective clip filters initial focus navigation shortcuts restoration viewport\n";
}

void TestExtremeGeometry()
{
	constexpr int minimum = (std::numeric_limits<int>::min)();
	constexpr int maximum = (std::numeric_limits<int>::max)();
	const SCellRect viewport{0, 0, 20, 10};
	const auto disjoint = SCellRect{maximum, 0, 1, 1}.Intersect(viewport);
	assert(disjoint.Width == 0 && disjoint.Height == 0);
	assert(!(SCellRect{minimum, 0, maximum, 1}.Contains(maximum, 0)));
	assert((SCellRect{maximum, 0, 1, 1}.Contains(maximum, 0)));
	for (int x : {minimum, minimum + 1, -20, -1, 0, 19, maximum - 1, maximum})
	{
		for (int width : {minimum, -1, 0, 1, 20, maximum})
		{
			const SCellRect rect{x, 0, width, 1};
			const auto clipped = rect.Intersect(viewport);
			const auto left = std::max<std::int64_t>(x, 0);
			const auto right = std::min<std::int64_t>(static_cast<std::int64_t>(x) + width, 20);
			const auto expected = width > 0 ? std::max<std::int64_t>(0, right - left) : 0;
			assert(clipped.Width == expected);
			for (int point : {minimum, -1, 0, 19, maximum})
			{
				assert(rect.Contains(point, 0) == (width > 0 && point >= x &&
					static_cast<std::int64_t>(point) < static_cast<std::int64_t>(x) + width));
			}
		}
	}
	FFrameBuffer frame(20, 10);
	FUiPanel overflow({maximum, 0, 8, 3});
	assert(overflow.GetOuterRect({frame, 1, 0, viewport}).Width == 0);
	FUiPanel negative({-4, -3, 8, 6});
	const auto translated = negative.GetOuterRect({frame, 2, 2, viewport});
	assert((translated == SCellRect{-2, -1, 8, 6}));
	assert((translated.Intersect(viewport) == SCellRect{0, 0, 6, 5}));
	FUiPanel invalid({0, 0, minimum, minimum});
	assert(invalid.GetContentRect({frame, 0, 0, viewport}).Width == 0);
	std::cout << "PASS R5a widened rectangle arithmetic checked origins negative coordinates\n";
}

void TestBoundedRendering(const GameData::FGameDataStore& store, const std::filesystem::path&)
{
	constexpr int maximum = (std::numeric_limits<int>::max)();
	constexpr int minimum = (std::numeric_limits<int>::min)();
	FFrameBuffer frame(20, 10);
	const SUiRenderContext context{frame, 0, 0, {0, 0, 20, 10}};
	FUiTextArt art({0, 0, 83, 41});
	std::wstring error;
	assert(art.LoadFromGameData(store, kMainLogoArtId, error));
	art.SetRect({0, -20, 83, 1});
	art.Render(context);
	assert(std::all_of(frame.GetCells().begin(), frame.GetCells().end(), [](wchar_t c) { return c == L' '; }));
	context.DrawBorder({0, 0, maximum, maximum}, L"title");
	assert(frame.GetCells()[0] == L'┌' && frame.GetCells()[19] == L'─' && frame.GetCells()[20] == L'│');
	frame.Clear();
	FUiPanel huge({-5, -5, maximum, maximum});
	huge.Render(context);
	assert(std::all_of(frame.GetCells().begin(), frame.GetCells().end(), [](wchar_t c) { return c == L' '; }));
	FUiPanel disjoint({maximum, maximum, maximum, maximum});
	disjoint.Render(context);
	FUiButton extreme({maximum, maximum, maximum, maximum}, L"extreme", []() {});
	extreme.Render(context);
	context.DrawText(maximum, 0, L"overflow");
	context.DrawText(minimum, 0, L"negative");
	assert(std::all_of(frame.GetCells().begin(), frame.GetCells().end(), [](wchar_t c) { return c == L' '; }));
	FUiPanel negative({-2, -1, 8, 6});
	negative.Render(context);
	assert(frame.GetCells()[5] == L'│' && frame.GetCells()[4 * 20 + 5] == L'┘');
	std::cout << "PASS R5b render extents clipped before allocations loops and art rows\n";
}

void TestExtremeLocalCoordinates()
{
	constexpr int minimum = (std::numeric_limits<int>::min)();
	constexpr int maximum = (std::numeric_limits<int>::max)();
	for (bool positive : {false, true})
	{
		FSceneUiRoot root;
		SInputEvent received;
		root.AddBase(std::make_unique<FCoordinateFixture>(
			positive ? SCellRect{-2, -2, maximum, maximum} : SCellRect{1, 1, 8, 3}, received));
		SInputEvent event;
		event.ScreenWidth = 20;
		event.ScreenHeight = 10;
		event.MouseX = 2;
		event.MouseY = 2;
		event.Type = EInputEventType::PointerDown;
		root.Dispatch(event);
		assert(root.GetCaptureId() != 0);
		event.MouseX = positive ? maximum : minimum;
		event.MouseY = event.MouseX;
		event.Type = EInputEventType::PointerMove;
		root.Dispatch(event);
		assert(received.LocalX == event.MouseX && received.LocalY == event.MouseY);
	}
	std::cout << "PASS R5c captured offscreen local subtraction remains representable without wrapping\n";
}

void TestNoLegacyTextLayout(const GameData::FGameDataStore& store, const std::filesystem::path& dataRoot, const std::filesystem::path& scratch)
{
	const auto fixture = scratch / L"without-legacy-layout";
	(void)dataRoot;
	assert(!std::filesystem::exists(fixture / L"Asset"));
	assert(!std::filesystem::exists(fixture / L"Asset/Scene"));
	for (const auto sceneId : {kMainMenuSceneId, kTownSceneId, kBattleFieldSceneId})
	{
		FGameUiBridge bridge;
		FDataScene scene(store, bridge);
		std::wstring error;
		assert(scene.LoadFromGameData(sceneId, error));
		FFrameBuffer frame(132, 43);
		scene.Render(frame);
		assert(frame.GetCells()[86] == L'┌');
	}
	std::cout << "PASS Scene UI loads and renders without retired ASCII layout files\n";
}

class FLayerMutationFixture final : public FUiButton
{
public:
	FLayerMutationFixture(int stage, std::array<int, 6>& calls, std::function<void()> mutate)
		: FUiButton({0, 0, 8, 3}, L"lifecycle", []() {})
		, Stage(stage), Calls(calls), Mutate(std::move(mutate)) {}
	bool SupportsDrag() const noexcept override { return true; }
	EUiReply OnPointerDown(const SInputEvent&) override
	{
		++Calls[0];
		if (Stage == 0) Mutate();
		return EUiReply::Capture;
	}
	EUiReply OnPointerMove(const SInputEvent&) override
	{
		++Calls[1];
		if (Stage == 2) Mutate();
		return EUiReply::Handled;
	}
	EUiReply OnPointerUp(const SInputEvent&) override
	{
		++Calls[2];
		if (Stage == 3) Mutate();
		return EUiReply::Handled;
	}
	EUiReply OnClick(const SInputEvent&) override { ++Calls[3]; return EUiReply::Handled; }
	EUiReply OnDrag(EUiDragPhase phase, const SInputEvent&) override
	{
		if (phase == EUiDragPhase::Begin && Stage == 1) Mutate();
		if (phase == EUiDragPhase::Move || phase == EUiDragPhase::End) ++Calls[4];
		return EUiReply::Handled;
	}
	void OnPointerCancel() override { ++Calls[5]; }
private:
	int Stage;
	std::array<int, 6>& Calls;
	std::function<void()> Mutate;
};

void TestSceneGestureMutation()
{
	for (int stage = 0; stage < 4; ++stage)
	{
		for (int mode = 0; mode < 4; ++mode)
		{
			FLayerFixture fixture;
			std::array<int, 6> calls{};
			std::uint64_t layer = 0;
			std::uint64_t hidden = 0;
			FUiId uiId = 0;
			auto ui = std::make_unique<FLayerMutationFixture>(stage, calls, [&]()
			{
				if (mode == 0) fixture.Scene.CloseUiLayer(layer);
				if (mode == 1) (void)fixture.Scene.OpenUiLayer();
				if (mode == 2) fixture.Scene.SetUiLayerVisible(layer, false);
				if (mode == 3) fixture.Scene.SetUiLayerVisible(hidden, true);
				fixture.Scene.FlushUiLayerCommands();
				assert(fixture.Scene.FindUi(uiId) != nullptr); // 콜백 안에서는 삭제 금지.
			});
			auto* pointer = ui.get();
			layer = fixture.Scene.OpenUiLayer(std::move(ui), 10);
			uiId = pointer->GetId();
			fixture.FlushCommands();
			if (mode == 3)
			{
				hidden = fixture.Scene.OpenUiLayer(nullptr, 100);
				fixture.Scene.SetUiLayerVisible(hidden, false);
				fixture.FlushCommands();
			}
			SInputEvent event;
			event.ScreenWidth = 132;
			event.ScreenHeight = 43;
			event.MouseX = 2;
			event.MouseY = 1;
			event.Type = EInputEventType::PointerDown;
			(void)fixture.Scene.HandleInput(event);
			if (stage == 1 || stage == 2)
			{
				event.Type = EInputEventType::PointerMove;
				event.MouseX = 3;
				(void)fixture.Scene.HandleInput(event);
			}
			event.Type = EInputEventType::PointerUp;
			(void)fixture.Scene.HandleInput(event);
			assert(calls[3] == 0 && calls[4] == 0);
			assert(stage != 1 || calls[1] == 0);
			if (const auto* surviving = fixture.Scene.GetUiLayer(layer)) assert(surviving->GetCaptureId() == 0);
		}
	}
	std::cout << "PASS INP04 callback close/open/hide/reveal in down begin move up stops later gesture callbacks\n";
}

void TestWithinLayerRouting()
{
	FLayerFixture fixture;
	std::array<int, 3> activations{};
	std::array<FUiId, 3> ids{};
	for (std::size_t index = 0; index < ids.size(); ++index)
	{
		ids[index] = fixture.AddBase(std::make_unique<FUiButton>(
			SCellRect{static_cast<int>(index) * 12, 0, 10, 3}, L"slot",
			[&, index]() { ++activations[index]; }, static_cast<int>(index) + 1));
	}
	SInputEvent event;
	event.Key = EInputKey::Down;
	fixture.Dispatch(event);
	assert(fixture.GetFocusId() == ids[1]);
	event.Key = EInputKey::Enter;
	fixture.Dispatch(event);
	assert((activations == std::array<int, 3>{0, 1, 0}));
	event.Key = EInputKey::Digit3;
	fixture.Dispatch(event);
	assert((activations == std::array<int, 3>{0, 1, 1}));
	assert(fixture.GetFocusId() == ids[2]);
	event.Key = EInputKey::None;
	event.Type = EInputEventType::PointerDown;
	event.MouseX = 2;
	event.MouseY = 1;
	fixture.Dispatch(event);
	event.Type = EInputEventType::PointerUp;
	fixture.Dispatch(event);
	assert((activations == std::array<int, 3>{1, 1, 1}));
	static_cast<FUiButton*>(fixture.Find(ids[0]))->SetEnabled(false);
	static_cast<FUiButton*>(fixture.Find(ids[2]))->SetEnabled(false);
	event.Type = EInputEventType::PointerMove;
	fixture.Dispatch(event);
	assert(fixture.GetFocusId() == ids[1]);
	std::cout << "PASS INP04 within-layer sibling entries share navigation numeric mouse focus\n";
}

class FFrameSource final : public IInputSource
{
public:
	void Queue(std::vector<SInputEvent> events) { Events = std::move(events); Offset = 0; }
	EInputReadResult ReadEvent(SInputEvent& event) noexcept override
	{
		if (Offset == Events.size()) return EInputReadResult::Timeout;
		event = Events[Offset++];
		return EInputReadResult::Event;
	}
private:
	std::vector<SInputEvent> Events;
	std::size_t Offset = 0;
};

void TestFocusResetGate()
{
	FLayerFixture fixture;
	int activations = 0;
	fixture.AddBase(std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"reset", [&]() { ++activations; }));
	FFrameSource source;
	auto manager = std::make_unique<FInputManager>(source);
	SInputEvent down;
	down.Key = EInputKey::Enter;
	down.PhysicalKey = VK_RETURN;
	down.ScreenWidth = 132;
	down.ScreenHeight = 43;
	source.Queue({down});
	assert(manager->Update() == EInputReadResult::Event);
	(void)fixture.Scene.HandleInputFrame(manager->GetEvents());
	assert(activations == 1 && manager->IsKeyDown(EInputKey::Enter));
	auto lost = down;
	lost.Key = EInputKey::None;
	lost.Type = EInputEventType::FocusLost;
	source.Queue({lost});
	assert(manager->Update() == EInputReadResult::Event);
	(void)fixture.Scene.HandleInputFrame(manager->GetEvents());
	assert(manager->WasKeyReleased(EInputKey::Enter));
	source.Queue({down});
	assert(manager->Update() == EInputReadResult::Event);
	(void)fixture.Scene.HandleInputFrame(manager->GetEvents());
	assert(activations == 2); // 포커스 reset 뒤 실제 새 누름을 오래된 UI gate가 막지 않는다.
	std::cout << "PASS INP04 real state manager focus reset re-arms Scene and layer confirm gate\n";
}

void TestSecondaryButtonDoesNotConfirm()
{
	FLayerFixture fixture;
	int activations = 0;
	fixture.AddBase(std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"left-only", [&]() { ++activations; }));
	SInputEvent event;
	event.ScreenWidth = 132;
	event.ScreenHeight = 43;
	event.MouseX = 2;
	event.MouseY = 1;
	event.MouseButton = EMouseButton::Right;
	event.Type = EInputEventType::PointerDown;
	(void)fixture.Scene.HandleInput(event);
	event.Type = EInputEventType::PointerUp;
	(void)fixture.Scene.HandleInput(event);
	assert(activations == 0 && fixture.GetCaptureId() == 0);
	event.MouseButton = EMouseButton::Left;
	event.Type = EInputEventType::PointerDown;
	(void)fixture.Scene.HandleInput(event);
	const auto capture = fixture.GetCaptureId();
	assert(capture != 0);
	event.MouseButton = EMouseButton::Right;
	event.Type = EInputEventType::PointerUp;
	(void)fixture.Scene.HandleInput(event);
	assert(activations == 0 && fixture.GetCaptureId() == capture);
	event.MouseButton = EMouseButton::Left;
	(void)fixture.Scene.HandleInput(event);
	assert(activations == 1 && fixture.GetCaptureId() == 0);
	std::cout << "PASS INP04 secondary mouse buttons neither confirm nor release left gesture\n";
}

class FInputProbe final : public FUiButton
{
public:
	FInputProbe(std::array<int, 6>& calls) : FUiButton({0, 0, 8, 3}, L"probe", []() {}, 1), Calls(calls) {}
	EUiReply OnKeyDown(const SInputEvent&) override { ++Calls[0]; return EUiReply::Unhandled; }
	EUiReply OnKeyUp(const SInputEvent&) override { ++Calls[1]; return EUiReply::Unhandled; }
	EUiReply OnPointerDown(const SInputEvent&) override { ++Calls[2]; return EUiReply::Unhandled; }
	EUiReply OnPointerMove(const SInputEvent&) override { ++Calls[3]; return EUiReply::Unhandled; }
	EUiReply OnWheel(const SInputEvent&) override { ++Calls[4]; return EUiReply::Unhandled; }
	void OnPointerCancel() override { ++Calls[5]; }
private:
	std::array<int, 6>& Calls;
};

void TestStrictTopMatrix()
{
	for (int mode = 0; mode < 3; ++mode)
	{
		FLayerFixture fixture;
		std::array<int, 6> lower{};
		std::array<int, 6> upper{};
		const auto lowerId = fixture.AddBase(std::make_unique<FInputProbe>(lower));
		const auto* retained = fixture.Find(lowerId);
		std::unique_ptr<IUI> top;
		if (mode != 0)
		{
			auto probe = std::make_unique<FInputProbe>(upper);
			if (mode == 1) probe->SetEnabled(false);
			top = std::move(probe);
		}
		const auto layer = fixture.Scene.OpenUiLayer(std::move(top), 900);
		fixture.FlushCommands();
		assert(fixture.Scene.GetUiTopLayer() == layer && fixture.Scene.GetUiTopLevel() == 900);
		SInputEvent event;
		event.ScreenWidth = 132;
		event.ScreenHeight = 43;
		event.MouseX = 2;
		event.MouseY = 1;
		for (auto key : {EInputKey::Enter, EInputKey::Digit1, EInputKey::Escape})
		{
			event.Key = key;
			event.Type = EInputEventType::KeyDown;
			(void)fixture.Scene.HandleInput(event);
			event.Type = EInputEventType::KeyUp;
			(void)fixture.Scene.HandleInput(event);
		}
		event.Key = EInputKey::None;
		for (auto type : {EInputEventType::PointerDown, EInputEventType::PointerMove, EInputEventType::PointerUp, EInputEventType::Wheel})
		{
			event.Type = type;
			event.WheelDelta = 120;
			(void)fixture.Scene.HandleInput(event);
		}
		assert((lower == std::array<int, 6>{}));
		if (mode < 2) assert((upper == std::array<int, 6>{}));
		else assert(upper[0] == 3 && upper[1] == 3 && upper[2] == 1 && upper[3] == 1 && upper[4] == 1);
		fixture.Scene.OnEnter();
		fixture.Bridge.HandleInput(event);
		assert((lower == std::array<int, 6>{}));
		fixture.Scene.Update(1.0f);
		assert(fixture.Find(lowerId) == retained);
		fixture.Scene.SetUiLayerVisible(layer, false);
		fixture.FlushCommands();
		assert(fixture.Scene.GetUiTopLayer() == fixture.Base && fixture.GetFocusId() == lowerId);
		fixture.Scene.SetUiLayerVisible(layer, true);
		fixture.FlushCommands();
		assert(fixture.Scene.GetUiTopLayer() == layer);
		fixture.Scene.CloseUiLayer(fixture.Base); // 중간/아래 제거가 위 UI 포커스·입력을 변경하지 않는다.
		fixture.FlushCommands();
		assert(fixture.Scene.GetUiTopLayer() == layer && fixture.Scene.FindUi(lowerId) == nullptr);
		fixture.Scene.CloseUiLayer(layer);
		fixture.Scene.CloseUiLayer(layer); // stale ID는 무해하다.
		fixture.Scene.SetUiLayerVisible(layer, true);
		fixture.FlushCommands();
		assert(fixture.Scene.GetUiTopLayer() == 0 && fixture.Scene.GetUiTopLevel() == 0 && fixture.Scene.GetUiLayerCount() == 0);
		assert(fixture.Scene.GetUiBaseRoot() == nullptr && fixture.Bridge.GetBoundRoot() == nullptr);
		(void)fixture.Scene.HandleInput(event);
		const auto fresh = fixture.Scene.OpenUiLayer(nullptr, 71);
		fixture.FlushCommands();
		assert(fresh != layer && fixture.Scene.GetUiTopLayer() == fresh && fixture.Scene.GetUiTopLevel() == 71);
	}
	std::cout << "PASS INP04 strict top keyboard numeric mouse wheel empty disabled Unhandled hidden sparse stale no-UI\n";
}

void TestPhysicalTrackingDuringTopChange()
{
	FLayerFixture fixture;
	int activations = 0;
	std::uint64_t opened = 0;
	fixture.AddBase(std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"open", [&]()
	{
		++activations;
		if (!opened) opened = fixture.Scene.OpenUiLayer();
	}, 1));
	FFrameSource source;
	auto manager = std::make_unique<FInputManager>(source);
	SInputEvent topRow;
	topRow.Key = EInputKey::Digit1;
	topRow.PhysicalKey = '1';
	topRow.ScreenWidth = 132;
	topRow.ScreenHeight = 43;
	auto numpad = topRow;
	numpad.PhysicalKey = VK_NUMPAD1;
	auto topUp = topRow;
	topUp.Type = EInputEventType::KeyUp;
	source.Queue({topRow, numpad, topUp});
	assert(manager->Update() == EInputReadResult::Event);
	const auto before = manager->GetKeyStatus(EInputKey::Digit1);
	(void)fixture.Scene.HandleInputFrame(manager->GetEvents());
	assert(activations == 1 && opened && manager->GetKeyStatus(EInputKey::Digit1) == before);
	assert(manager->IsKeyDown(EInputKey::Digit1) && !manager->WasKeyReleased(EInputKey::Digit1));
	fixture.Scene.CloseUiLayer(opened);
	fixture.FlushCommands();
	source.Queue({numpad});
	assert(manager->Update() == EInputReadResult::Event);
	(void)fixture.Scene.HandleInputFrame(manager->GetEvents());
	assert(activations == 1 && manager->GetKeyStatus(EInputKey::Digit1) == EKeyStatus::Held);
	auto numUp = numpad;
	numUp.Type = EInputEventType::KeyUp;
	source.Queue({numUp, topRow, topUp});
	assert(manager->Update() == EInputReadResult::Event);
	(void)fixture.Scene.HandleInputFrame(manager->GetEvents());
	assert(activations == 2 && manager->WasKeyReleased(EInputKey::Digit1) && manager->WasKeyPressed(EInputKey::Digit1));
	assert(!manager->IsKeyDown(EInputKey::Digit1));
	std::cout << "PASS INP04 real physical aliases tracked through top change frame gate without Manager mutation\n";
}

void TestLogicalAliasReleaseRouting()
{
	FLayerFixture fixture;
	std::array<int, 6> calls{};
	fixture.AddBase(std::make_unique<FInputProbe>(calls));
	FFrameSource source;
	auto manager = std::make_unique<FInputManager>(source);
	SInputEvent first;
	first.Key = EInputKey::Digit1;
	first.PhysicalKey = '1';
	first.ScreenWidth = 132;
	first.ScreenHeight = 43;
	auto second = first;
	second.PhysicalKey = VK_NUMPAD1;
	auto firstUp = first;
	firstUp.Type = EInputEventType::KeyUp;
	source.Queue({first, second, firstUp});
	assert(manager->Update() == EInputReadResult::Event);
	(void)fixture.Scene.HandleInputFrame(manager->GetEvents());
	assert(calls[0] == 1 && calls[1] == 0 && manager->IsKeyDown(EInputKey::Digit1));
	second.Type = EInputEventType::KeyUp;
	source.Queue({second});
	assert(manager->Update() == EInputReadResult::Event);
	(void)fixture.Scene.HandleInputFrame(manager->GetEvents());
	assert(calls[1] == 1 && !manager->IsKeyDown(EInputKey::Digit1));
	std::cout << "PASS INP04 physical alias release reaches UI only on logical release\n";
}

void TestPointerTopChangeGate()
{
	FLayerFixture fixture;
	std::array<int, 6> calls{};
	int clicks = 0;
	fixture.AddBase(std::make_unique<FLayerMutationFixture>(0, calls, [&]()
	{
		(void)fixture.Scene.OpenUiLayer(std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"top", [&]() { ++clicks; }));
	}));
	SInputEvent down;
	down.Type = EInputEventType::PointerDown;
	down.ScreenWidth = 132;
	down.ScreenHeight = 43;
	down.MouseX = 2;
	down.MouseY = 1;
	auto up = down;
	up.Type = EInputEventType::PointerUp;
	(void)fixture.Scene.HandleInputFrame({down, down, up, down, up});
	assert(clicks == 0 && fixture.Scene.GetUiLayerCount() == 2);
	(void)fixture.Scene.HandleInputFrame({down});
	assert(fixture.GetCaptureId() != 0 && clicks == 0);
	(void)fixture.Scene.HandleInputFrame({down, up});
	assert(clicks == 1); // 중복 down은 캡처를 다시 만들지 않는다.
	std::cout << "PASS INP04 pointer top change drops frame and requires release before fresh confirmation\n";
}

template <typename TScene>
SSceneAction DispatchSceneFrame(TScene& scene, const std::vector<SInputEvent>& events)
{
	if constexpr (requires { scene.HandleInputFrame(events); }) return scene.HandleInputFrame(events);
	else
	{
		SSceneAction result;
		for (const auto& event : events) result = scene.HandleInput(event);
		return result;
	}
}

void TestFrameBoundary()
{
	FLayerFixture fixture;
	int lower = 0;
	fixture.AddBase(std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"base", [&]() { ++lower; }, 1));
	FUiId self = 0;
	self = fixture.OpenPopup(std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"close", [&]()
	{
		fixture.ClosePopup(self);
	}));
	fixture.FlushCommands();
	SInputEvent down;
	down.Key = EInputKey::Enter;
	down.ScreenWidth = 132;
	down.ScreenHeight = 43;
	auto up = down;
	up.Type = EInputEventType::KeyUp;
	auto digit = down;
	digit.Key = EInputKey::Digit1;
	(void)DispatchSceneFrame(fixture.Scene, {down, up, digit});
	assert(lower == 0); // BatchId 0도 frame 경계를 지켜야 한다.
	assert(fixture.Scene.GetUiTopLayer() == fixture.Base);
	// 버린 잔여 이벤트도 release gate를 갱신한다. 누른 digit은 해제 전 재확정하지 않는다.
	(void)DispatchSceneFrame(fixture.Scene, {digit});
	assert(lower == 0);
	auto digitUp = digit;
	digitUp.Type = EInputEventType::KeyUp;
	(void)DispatchSceneFrame(fixture.Scene, {digitUp, digit});
	assert(lower == 1);
	std::cout << "PASS INP04 zero-batch frame remainder blocked and releases tracked\n";
}

class FFrameDispatchProbe final : public IScene
{
private:
	void OnEnterImpl() override {}
	void OnExitImpl() override {}
	void UpdateImpl(float) override {}
	void RenderImpl(FFrameBuffer&) const override {}
	SSceneAction HandleInputImpl(const SInputEvent&) override
	{
		++EventCalls;
		return {};
	}
	SSceneAction HandleInputFrameImpl(const std::vector<SInputEvent>&) override
	{
		++FrameCalls;
		return {ESceneAction::StartGame, kTownSceneId};
	}
public:
	int FrameCalls = 0;
	int EventCalls = 0;
};

template <typename TManager>
static void CheckSceneFrameRoute(TManager& manager, FFrameDispatchProbe& probe)
{
	const std::vector<SInputEvent> events{{.Key = EInputKey::Enter}, {.Type = EInputEventType::KeyUp}};
	if constexpr (requires { manager.HandleInputFrame(events); })
	{
		const auto action = manager.HandleInputFrame(events);
		assert(probe.FrameCalls == 1 && probe.EventCalls == 0);
		assert(action.Type == ESceneAction::StartGame && action.TargetSceneId == kTownSceneId);
	}
	else
	{
		(void)manager;
		(void)probe;
		assert(false && "INP05 SceneManager must dispatch one complete frame through IScene");
	}
}

// 모든 fixture도 제품과 같은 명시적 안전점에서만 게시한다.
template <typename TManager>
static void PublishTestFrame(TManager& scenes)
{
	std::wstring error;
	assert(scenes.EndFrame(error));
}

struct SSceneLifetime final
{
	int Entered = 0;
	int Exited = 0;
	int Destroyed = 0;
};

class FSceneLifetimeProbe final : public IScene
{
public:
	explicit FSceneLifetimeProbe(SSceneLifetime& state) : State(state) {}
	~FSceneLifetimeProbe() override { ++State.Destroyed; }
private:
	void OnEnterImpl() override { ++State.Entered; }
	void OnExitImpl() override { ++State.Exited; }
	void UpdateImpl(float) override {}
	SSceneAction HandleInputImpl(const SInputEvent&) override { return {}; }
	void RenderImpl(FFrameBuffer&) const override {}
private:
	SSceneLifetime& State;
};

class FSceneOwnedButton final : public FUiButton
{
public:
	FSceneOwnedButton(int& destroyed, std::function<void()> callback)
		: FUiButton({1, 1, 20, 3}, L"transition", std::move(callback)), Destroyed(destroyed) {}
	~FSceneOwnedButton() override { ++Destroyed; }
private:
	int& Destroyed;
};

static void TestEndFrameCallbackLifetime(GameData::FGameDataStore& store)
{
	for (const bool direct : {false, true})
	{	
		SSceneLifetime nextState;
		int destroyed = 0;
		int hits = 0;
		FGameUiBridge ui;
		FTextRPGSceneManager scenes(store);
		std::wstring error;
		assert(scenes.LoadStartupScene(ui, error));
		PublishTestFrame(scenes);
		auto* old = ui.GetBoundScene();
		assert(old);
		const auto layer = old->OpenUiLayer(std::make_unique<FSceneOwnedButton>(destroyed, [&]()
		{
			// 교체 뒤 lambda 저장소도 사라질 수 있다. 외부 관측 주소를 stack에 먼저 복사한다.
			auto* hitCounter = &hits;
			auto* destroyCounter = &destroyed;
			auto* state = &nextState;
			auto* bridge = &ui;
			auto* oldIdentity = old;
			auto* manager = &scenes;
			++*hitCounter;
			assert(manager->SetScene(std::make_unique<FSceneLifetimeProbe>(*state)));
			std::cout << "TRACE APP01 callback-hit=" << *hitCounter << " old-ui-destroyed=" << *destroyCounter
				<< " next-entered=" << state->Entered << '\n';
			assert(*destroyCounter == 0 && state->Entered == 0 && "APP01 old UI must survive callback until EndFrame");
			assert(bridge->GetBoundScene() == oldIdentity);
			std::wstring nestedError;
			assert(!manager->EndFrame(nestedError));
			assert(*destroyCounter == 0 && state->Entered == 0);
		}));
		old->FlushUiLayerCommands();
		SInputEvent enter;
		enter.ScreenWidth = 132;
		enter.ScreenHeight = 43;
		enter.Key = EInputKey::Enter;
		if (direct) (void)old->HandleInputFrame({enter});
		else (void)scenes.HandleInputFrame({enter});
		assert(hits == 1 && destroyed == 0 && old->GetUiLayer(layer));
		FFrameBuffer frame(132, 43);
		scenes.Update(0);
		scenes.Render(frame);
		assert(destroyed == 0 && nextState.Entered == 0);
		PublishTestFrame(scenes);
		assert(destroyed == 1 && nextState.Entered == 1 && ui.GetBoundScene() == nullptr);
	}
	std::cout << "PASS APP01 actual callback queues generic replacement old UI lives through Update Render until EndFrame\n";
}

struct SEndFrameProbeState final
{
	SSceneLifetime Lifetime;
	int InputHits = 0;
	int UpdateHits = 0;
	int RenderHits = 0;
	std::function<void()> Input;
	std::function<void()> Update;
	std::function<void()> Render;
	std::function<void()> Enter;
	std::function<void()> Exit;
	std::function<void()> Destroy;
};

class FEndFrameProbe final : public IScene
{
public:
	explicit FEndFrameProbe(SEndFrameProbeState& state) : State(state) {}
	~FEndFrameProbe() override { ++State.Lifetime.Destroyed; if (State.Destroy) State.Destroy(); }
private:
	void OnEnterImpl() override { ++State.Lifetime.Entered; if (State.Enter) State.Enter(); }
	void OnExitImpl() override { ++State.Lifetime.Exited; if (State.Exit) State.Exit(); }
	void UpdateImpl(float) override { ++State.UpdateHits; if (State.Update) State.Update(); }
	void RenderImpl(FFrameBuffer&) const override { ++State.RenderHits; if (State.Render) State.Render(); }
	SSceneAction HandleInputImpl(const SInputEvent&) override
	{
		++State.InputHits;
		if (State.Input) State.Input();
		return {};
	}
private:
	SEndFrameProbeState& State;
};

class FSceneFrameClock final : public IFrameClock
{
public:
	double Current = 0;
	double NowSeconds() override { return Current; }
	void WaitUntil(double deadlineSeconds) override { Current = (std::max)(Current, deadlineSeconds); }
};

class FSceneLoopProbe final : public FFrame
{
public:
	FSceneLoopProbe(const SFrameOptions& options, FSceneFrameClock& clock, FTextRPGSceneManager& scenes)
		: FFrame(options, clock)
		, Clock(clock)
		, Scenes(scenes)
	{
	}
	int Inputs = 0;
	int Ends = 0;
	int SkippedEnds = 0;
private:
	void OnInput() override { ++Inputs; }
	void OnUpdate(float deltaSeconds) override
	{
		if (!Scenes.HasPendingTransition()) Scenes.Update(deltaSeconds);
		if (Scenes.HasPendingTransition()) SkipRemainingUpdates();
	}
	void OnRender() override
	{
		Scenes.Render(Buffer);
		if (!Stalled)
		{
			Clock.Current += 1.0;
			Stalled = true;
		}
	}
	void OnEndFrame() override
	{
		const auto skipped = GetStatistics().SkippedRenders;
		if (skipped > SeenSkips) ++SkippedEnds;
		SeenSkips = skipped;
		std::wstring error;
		assert(Scenes.EndFrame(error));
		if (++Ends == 4) RequestExit();
	}
	FSceneFrameClock& Clock;
	FTextRPGSceneManager& Scenes;
	FFrameBuffer Buffer{132, 43};
	bool Stalled = false;
	std::uint64_t SeenSkips = 0;
};

static void TestInheritedSceneFrameModes()
{
	for (const auto mode : {EFrameMode::Basic, EFrameMode::FixedUpdate, EFrameMode::FrameSkip})
	{
		SEndFrameProbeState old, next;
		GameData::FGameDataStore store;
		FTextRPGSceneManager scenes(store);
		assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(old)));
		PublishTestFrame(scenes);
		SFrameOptions options;
		options.Mode = mode;
		options.TargetFps = 8;
		options.MaxAccumulatedElapsedSeconds = 2;
		options.MaxConsecutiveRenderSkips = 2;
		FSceneFrameClock clock;
		FSceneLoopProbe loop(options, clock, scenes);
		int requests = 0;
		int updatesAtRequest = 0;
		old.Update = [&]
		{
			if (loop.Ends < 2) return;
			++requests;
			updatesAtRequest = old.UpdateHits;
			assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(next)));
			assert(scenes.HasPendingTransition());
			std::wstring error;
			assert(!scenes.EndFrame(error));
			assert(old.Lifetime.Destroyed == 0 && next.Lifetime.Entered == 0);
		};
		assert(loop.Run() == 0);
		assert(loop.Inputs == 4 && loop.Ends == 4 && loop.GetStatistics().CompletedFrames == 4);
		assert(requests == 1 && old.UpdateHits == updatesAtRequest);
		assert(old.Lifetime.Destroyed == 1 && next.Lifetime.Entered == 1 && next.UpdateHits > 0);
		assert(!scenes.HasPendingTransition());
		if (mode == EFrameMode::FrameSkip) assert(loop.SkippedEnds > 0);
		std::cout << "PASS APP01 inherited frame mode=" << static_cast<int>(mode)
			<< " input once old-scene catchup stopped safe-point once skipped-end=" << loop.SkippedEnds << '\n';
	}
}

static void TestEndFrameGenericStages()
{
	// 거부된 인수의 destructor도 공개 Manager 작업의 일부다.
	{
		SEndFrameProbeState old, next, rejected;
		GameData::FGameDataStore store;
		FTextRPGSceneManager scenes(store);
		std::wstring error;
		assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(old)));
		PublishTestFrame(scenes);
		assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(next)));
		int cleanupHits = 0;
		rejected.Destroy = [&]
		{
			++cleanupHits;
			const bool nested = scenes.EndFrame(error);
			std::cout << "TRACE SCM incoming-cleanup hits=" << cleanupHits << " nested=" << nested
				<< " old-destroyed=" << old.Lifetime.Destroyed << " next-entered=" << next.Lifetime.Entered << '\n';
			assert(!nested && old.Lifetime.Destroyed == 0 && next.Lifetime.Entered == 0);
		};
		assert(!scenes.SetScene(std::make_unique<FEndFrameProbe>(rejected)));
		assert(cleanupHits == 1 && rejected.Lifetime.Destroyed == 1);
		PublishTestFrame(scenes);
		assert(old.Lifetime.Destroyed == 1 && next.Lifetime.Entered == 1);
		std::cout << "PASS APP01 rejected Scene argument cleanup stays inside Manager lifetime protection\n";
	}
	TestInheritedSceneFrameModes();
	for (int stage = 0; stage != 3; ++stage)
	{
		SEndFrameProbeState old, next, follow, final, rejected;
		GameData::FGameDataStore store;
		FTextRPGSceneManager scenes(store);
		std::wstring error;
		assert(!scenes.HasPendingTransition());
		assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(old)));
		assert(scenes.HasPendingTransition());
		assert(old.Lifetime.Entered == 0);
		PublishTestFrame(scenes);
		assert(!scenes.HasPendingTransition());
		assert(old.Lifetime.Entered == 1);
		int hits = 0;
		const auto request = [&]()
		{
			if (++hits != 1) return;
			assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(next)));
			assert(!scenes.SetScene(std::make_unique<FEndFrameProbe>(rejected)));
			assert(!scenes.EndFrame(error));
			assert(old.Lifetime.Exited == 0 && old.Lifetime.Destroyed == 0 && next.Lifetime.Entered == 0);
		};
		if (stage == 0) old.Input = request;
		if (stage == 1) old.Update = request;
		if (stage == 2) old.Render = request;
		old.Exit = [&]()
		{
			assert(old.Lifetime.Destroyed == 0);
			assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(follow)));
			assert(!scenes.EndFrame(error));
		};
		follow.Enter = [&]()
		{
			assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(final)));
			assert(!scenes.EndFrame(error));
		};
		FFrameSource source;
		FInputManager input(source);
		SInputEvent enter;
		enter.Key = EInputKey::Enter;
		source.Queue({enter});
		assert(input.Update() == EInputReadResult::Event);
		(void)scenes.HandleInputFrame(input.GetEvents());
		scenes.Update(0);
		FFrameBuffer frame(132, 43);
		scenes.Render(frame);
		assert(hits == 1 && old.Lifetime.Exited == 0 && old.Lifetime.Destroyed == 0);
		assert(next.InputHits == 0 && next.UpdateHits == 0 && next.RenderHits == 0);
		assert(rejected.Lifetime.Destroyed == 1 && rejected.Lifetime.Entered == 0);
		PublishTestFrame(scenes);
		assert(old.Lifetime.Exited == 1 && old.Lifetime.Destroyed == 1 && next.Lifetime.Entered == 1);
		assert(follow.Lifetime.Entered == 0 && final.Lifetime.Entered == 0);
		assert(input.GetKeyStatus(EInputKey::Enter) == EKeyStatus::Pressed);
		PublishTestFrame(scenes);
		assert(next.Lifetime.Exited == 1 && next.Lifetime.Destroyed == 1 && follow.Lifetime.Entered == 1);
		assert(final.Lifetime.Entered == 0);
		PublishTestFrame(scenes);
		assert(follow.Lifetime.Exited == 1 && follow.Lifetime.Destroyed == 1 && final.Lifetime.Entered == 1);
		// 물리 상태를 지우지 않고 새 Scene의 확정만 release gate로 차단한다.
		source.Queue({enter});
		assert(input.Update() == EInputReadResult::Event);
		(void)scenes.HandleInputFrame(input.GetEvents());
		assert(final.InputHits == 0 && input.GetKeyStatus(EInputKey::Enter) == EKeyStatus::Held);
		auto up = enter;
		up.Type = EInputEventType::KeyUp;
		source.Queue({up, enter});
		assert(input.Update() == EInputReadResult::Event);
		(void)scenes.HandleInputFrame(input.GetEvents());
		assert(final.InputHits == 2 && input.IsKeyDown(EInputKey::Enter));
		assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(rejected)));
		auto escape = enter;
		escape.Key = EInputKey::Escape;
		assert(scenes.HandleInputFrame({escape}).Type == ESceneAction::Quit);
		PublishTestFrame(scenes);
		assert(final.Lifetime.Exited == 0 && final.Lifetime.Destroyed == 0 && rejected.Lifetime.Entered == 0);
		assert(!scenes.SetScene(std::make_unique<FEndFrameProbe>(rejected)));
		std::cout << "PASS APP01 generic stage=" << stage
			<< " callback-hit=1 EndFrame-only exit/destroy=1 snapshot-next-frame first-request-wins reentry-reject release-gate Quit-cancel\n";
	}
}

static void TestGenericActivationFailure()
{
	for (const bool enterFailure : {true, false})
	{
		SEndFrameProbeState old, failed, follow;
		GameData::FGameDataStore store;
		FTextRPGSceneManager scenes(store);
		std::wstring error;
		assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(old)));
		PublishTestFrame(scenes);
		const auto fault = [&]()
		{
			assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(follow)));
			assert(!scenes.EndFrame(error));
			throw std::runtime_error("owned generic lifecycle fault");
		};
		if (enterFailure) failed.Enter = fault;
		else old.Exit = fault;
		assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(failed)));
		assert(!scenes.EndFrame(error) && error == L"owned generic lifecycle fault");
		assert(old.Lifetime.Destroyed == 0 && failed.Lifetime.Destroyed == 1 && follow.Lifetime.Entered == 0);
		assert(old.Lifetime.Exited == (enterFailure ? 0 : 1));
		old.Exit = {};
		PublishTestFrame(scenes);
		assert(old.Lifetime.Destroyed == 1 && follow.Lifetime.Entered == 1);
	}
	std::cout << "PASS APP01 generic OnEnter OnExit failure preserves active Scene and callback followup next EndFrame guard unwinds\n";
}

static void TestGenericDirectLifetime()
{
	bool allProtected = true;
	for (const bool direct : {true, false})
	{
		for (int stage = 0; stage != 4; ++stage)
		{
			SEndFrameProbeState old, next;
			GameData::FGameDataStore store;
			FTextRPGSceneManager scenes(store);
			std::wstring error;
			auto owned = std::make_unique<FEndFrameProbe>(old);
			IScene* borrowed = owned.get(); // 소유권 이동 전에 확보한 공개 직접 경로.
			assert(scenes.SetScene(std::move(owned)));
			PublishTestFrame(scenes);
			int hits = 0;
			bool nestedAccepted = false;
			bool aliveInside = false;
			const auto request = [&]()
			{
				if (hits != 0) return;
				++hits;
				assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(next)));
				nestedAccepted = scenes.EndFrame(error);
				aliveInside = old.Lifetime.Destroyed == 0 && old.Lifetime.Exited == 0;
				std::cout << "TRACE LIFE-01 direct=" << direct << " stage=" << stage << " hit=" << hits
					<< " nested=" << nestedAccepted << " destroyed-inside=" << old.Lifetime.Destroyed << '\n';
			};
			if (stage < 2) old.Input = request;
			if (stage == 2) old.Update = request;
			if (stage == 3) old.Render = request;
			SInputEvent enter;
			enter.Key = EInputKey::Enter;
			FFrameBuffer frame(132, 43);
			if (stage == 0) (void)(direct ? borrowed->HandleInput(enter) : scenes.HandleInput(enter));
			if (stage == 1) (void)(direct ? borrowed->HandleInputFrame({enter}) : scenes.HandleInputFrame({enter}));
			if (stage == 2) { if (direct) borrowed->Update(0); else scenes.Update(0); }
			if (stage == 3) { if (direct) borrowed->Render(frame); else scenes.Render(frame); }
			const bool aliveAfter = old.Lifetime.Destroyed == 0 && old.Lifetime.Exited == 0;
			scenes.Update(0);
			scenes.Render(frame);
			const bool aliveThroughRender = old.Lifetime.Destroyed == 0 && next.Lifetime.Entered == 0;
			PublishTestFrame(scenes);
			const bool complete = old.Lifetime.Destroyed == 1 && old.Lifetime.Exited == 1 && next.Lifetime.Entered == 1;
			allProtected = allProtected && hits == 1 && !nestedAccepted && aliveInside && aliveAfter && aliveThroughRender && complete;
			std::cout << "TRACE LIFE-01 direct=" << direct << " stage=" << stage << " alive-after=" << aliveAfter
				<< " alive-through-render=" << aliveThroughRender << " destroyed-EndFrame=" << old.Lifetime.Destroyed << '\n';
		}
	}
	if (!allProtected) throw std::runtime_error("LIFE-01 generic direct operation destroyed before real EndFrame");
	std::cout << "PASS APP01 LIFE-01 generic direct and manager Input Frame Update Render guarded callback return alive then EndFrame destroy exactly once\n";
}

static void TestGenericLifecycleAndCandidate()
{
	bool safe = true;
	for (const bool pending : {false, true})
	{
		for (int stage = pending ? 0 : 4; stage != 6; ++stage)
		{
			SEndFrameProbeState old, next;
			GameData::FGameDataStore store;
			FTextRPGSceneManager scenes(store);
			std::wstring error;
			auto owned = std::make_unique<FEndFrameProbe>(old);
			IScene* borrowed = owned.get();
			assert(scenes.SetScene(std::move(owned)));
			if (!pending) PublishTestFrame(scenes);
			int hits = 0;
			bool nested = true;
			int destroyedInside = -1;
			const auto request = [&]()
			{
				if (hits != 0) return;
				++hits;
				const bool accepted = scenes.SetScene(std::make_unique<FEndFrameProbe>(next));
				assert(accepted != pending);
				nested = scenes.EndFrame(error);
				destroyedInside = old.Lifetime.Destroyed;
				std::cout << "TRACE LIFE-01 lifecycle-candidate pending=" << pending << " stage=" << stage
					<< " hit=" << hits << " nested=" << nested << " destroyed-inside=" << destroyedInside << '\n';
			};
			if (stage < 2) old.Input = request;
			if (stage == 2) old.Update = request;
			if (stage == 3) old.Render = request;
			if (stage == 4) old.Enter = request;
			if (stage == 5) old.Exit = request;
			SInputEvent enter;
			enter.Key = EInputKey::Enter;
			FFrameBuffer frame(132, 43);
			if (stage == 0) (void)borrowed->HandleInput(enter);
			if (stage == 1) (void)borrowed->HandleInputFrame({enter});
			if (stage == 2) borrowed->Update(0);
			if (stage == 3) borrowed->Render(frame);
			if (stage == 4) borrowed->OnEnter();
			if (stage == 5) borrowed->OnExit();
			const bool alive = old.Lifetime.Destroyed == 0;
			PublishTestFrame(scenes);
			const bool final = pending ? old.Lifetime.Destroyed == 0 : old.Lifetime.Destroyed == 1;
			safe = safe && hits == 1 && !nested && destroyedInside == 0 && alive && final;
		}
	}
	if (!safe) throw std::runtime_error("LIFE-01 public lifecycle or pending candidate bypassed operation lifetime");
	std::cout << "PASS APP01 LIFE-01 public OnEnter OnExit and pending candidate six direct operations guarded until return\n";
}

static void TestGenericFrameInvalidation()
{
	bool allBlocked = true;
	for (const bool direct : {false, true})
	{
		for (const bool tailHeld : {false, true})
		{
			SEndFrameProbeState old, next;
			GameData::FGameDataStore store;
			FTextRPGSceneManager scenes(store);
			auto owned = std::make_unique<FEndFrameProbe>(old);
			IScene* borrowed = owned.get();
			assert(scenes.SetScene(std::move(owned)));
			PublishTestFrame(scenes);
			int requests = 0;
			old.Input = [&]()
			{
				if (requests++ == 0) assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(next)));
			};
			FFrameSource source;
			FInputManager input(source);
			SInputEvent enter;
			enter.Key = EInputKey::Enter;
			auto up = enter;
			up.Type = EInputEventType::KeyUp;
			auto digit = enter;
			digit.Key = EInputKey::Digit1;
			auto digitUp = digit;
			digitUp.Type = EInputEventType::KeyUp;
			SInputEvent pointer;
			pointer.Type = EInputEventType::PointerDown;
			pointer.MouseButton = EMouseButton::Left;
			auto pointerUp = pointer;
			pointerUp.Type = EInputEventType::PointerUp;
			std::vector<SInputEvent> events{enter, up, digit, pointer, digitUp, pointerUp};
			if (tailHeld) events.push_back(enter);
			source.Queue(events);
			assert(input.Update() == EInputReadResult::Event);
			(void)(direct ? borrowed->HandleInputFrame(input.GetEvents()) : scenes.HandleInputFrame(input.GetEvents()));
			const bool physical = input.WasKeyReleased(EInputKey::Enter) && input.WasKeyReleased(EInputKey::Digit1) &&
				input.IsKeyDown(EInputKey::Enter) == tailHeld && !input.IsMouseButtonDown(EMouseButton::Left);
			std::cout << "TRACE INPUT-01 direct=" << direct << " tail-held=" << tailHeld << " first-request=" << requests
				<< " old-deliveries=" << old.InputHits << " physical-release=" << physical << '\n';
			allBlocked = allBlocked && requests == 1 && old.InputHits == 1 && physical && next.InputHits == 0;
			PublishTestFrame(scenes);
			source.Queue({enter});
			assert(input.Update() == EInputReadResult::Event);
			(void)scenes.HandleInputFrame(input.GetEvents());
			const int beforeRelease = next.InputHits;
			allBlocked = allBlocked && beforeRelease == (tailHeld ? 0 : 1);
			source.Queue({up, enter});
			assert(input.Update() == EInputReadResult::Event);
			(void)scenes.HandleInputFrame(input.GetEvents());
			allBlocked = allBlocked && next.InputHits == beforeRelease + 2 && input.IsKeyDown(EInputKey::Enter);
		}
	}
	if (!allBlocked) throw std::runtime_error("INPUT-01 generic frame delivered remainder or lost logical release gate");
	std::cout << "PASS APP01 INPUT-01 generic default multi-event direct manager remainder actions zero physical releases and next-frame fresh press\n";
}

static void TestSceneFrameRoute()
{
	GameData::FGameDataStore store;
	FTextRPGSceneManager manager(store);
	auto scene = std::make_unique<FFrameDispatchProbe>();
	auto* probe = scene.get();
	assert(manager.SetScene(std::move(scene)));
	PublishTestFrame(manager);
	CheckSceneFrameRoute(manager, *probe);
	std::cout << "PASS INP05 SceneManager virtual frame route is called once without per-event bypass\n";
}

class FFocusLifetimeFixture final : public FUiButton
{
public:
	FFocusLifetimeFixture(FDataScene& scene, std::uint64_t layer, bool& destroyed, int& hits, bool& armed, bool gaining)
		: FUiButton({0, 0, 8, 3}, L"focus lifetime", []() {}), Scene(scene), Layer(layer),
		Destroyed(destroyed), Hits(hits), Armed(armed), Gaining(gaining) {}
	~FFocusLifetimeFixture() override { Destroyed = true; }
	void SetFocused(bool focused) noexcept override
	{
		FUiButton::SetFocused(focused);
		if (!Armed || focused != Gaining) return;
		Armed = false;
		// close/flush 뒤에는 this나 멤버를 읽지 않는다. 실패 자체가 UAF에 의존하지 않는다.
		auto* scene = &Scene;
		const auto layer = Layer;
		auto* destroyed = &Destroyed;
		auto* hits = &Hits;
		const auto* root = scene->GetUiLayer(layer);
		++*hits;
		scene->CloseUiLayer(layer);
		scene->FlushUiLayerCommands();
		assert(!*destroyed && "SCENE-01 focused UI destroyed before callback return");
		assert(scene->GetUiLayer(layer) == root);
		std::wstring error;
		assert(!scene->LoadFromGameData(kMainMenuSceneId, error));
		assert(error.find(L"during UI") != std::wstring::npos);
	}
private:
	FDataScene& Scene;
	std::uint64_t Layer;
	bool& Destroyed;
	int& Hits;
	bool& Armed;
	bool Gaining;
};

void TestFocusOwnerLifetime()
{
	for (int entry = 0; entry < 4; ++entry)
	{
		FLayerFixture fixture;
		bool destroyed = false;
		bool armed = false;
		int hits = 0;
		const auto id = fixture.AddBase(std::make_unique<FFocusLifetimeFixture>(
			fixture.Scene, fixture.Base, destroyed, hits, armed, entry == 3));
		auto* root = fixture.Scene.GetUiLayer(fixture.Base);
		assert(root && root->GetFocusId() == id);
		if (entry == 3) root->SuspendInput();
		armed = true;
		FUiId added = 0;
		if (entry < 2)
		{
			static_cast<FUiButton*>(fixture.Find(id))->SetEnabled(false); // A -> B의 실제 포커스 변경.
			auto next = std::make_unique<FUiButton>(SCellRect{10, 0, 8, 3}, L"next", []() {});
			added = entry == 0 ? fixture.Scene.AddUiToLayer(fixture.Base, std::move(next)) : root->AddBase(std::move(next));
			assert(added && root->GetFocusId() == added);
		}
		else if (entry == 2) root->SuspendInput();
		else root->ResumeInput();
		assert(hits == 1 && !armed && !destroyed);
		assert(fixture.Scene.GetUiLayer(fixture.Base) == root && fixture.Scene.GetUiLayerCount() == 1);
		fixture.Scene.FlushUiLayerCommands();
		assert(destroyed && fixture.Scene.GetUiLayer(fixture.Base) == nullptr);
		assert(fixture.Scene.GetUiLayerCount() == 0 && fixture.Scene.GetUiTopLayer() == 0);
		assert(fixture.Scene.FindUi(id) == nullptr && (!added || fixture.Scene.FindUi(added) == nullptr));
		std::cout << "PASS SCENE-01 entry=" << entry << " callback-hit=1 alive-until-root-return queued-close-committed\n";
	}
}

class FFocusMutationFixture final : public FUiButton
{
public:
	FFocusMutationFixture(SCellRect rect, int shortcut, int& activations, int& deliveries, std::function<void(bool)> focus)
		: FUiButton(rect, L"focus mutation", [&activations]() { ++activations; }, shortcut),
		Deliveries(deliveries), Focus(std::move(focus)) {}
	void SetFocused(bool focused) override
	{
		FUiButton::SetFocused(focused);
		Focus(focused);
	}
	EUiReply OnKeyDown(const SInputEvent& event) override
	{
		++Deliveries;
		return FUiButton::OnKeyDown(event);
	}
	EUiReply OnPointerDown(const SInputEvent& event) override
	{
		++Deliveries;
		return FUiButton::OnPointerDown(event);
	}
private:
	int& Deliveries;
	std::function<void(bool)> Focus;
};

void TestFocusMutationDelivery()
{
	for (bool pointer : {false, true})
	{
		for (bool gaining : {false, true})
		{
			for (int mutation = 0; mutation < 5; ++mutation)
			{
				FSceneUiRoot root({0, 0, 30, 15});
				auto panel = std::make_unique<FUiPanel>(SCellRect{0, 0, 25, 12});
				auto* ancestor = panel.get();
				int activations = 0;
				int deliveries = 0;
				int losses = 0;
				int hits = 0;
				bool armed = false;
				FUiId aId = 0;
				FUiId bId = 0;
				const auto mutate = [&]()
				{
					armed = false;
					++hits;
					assert(root.GetFocusId() == (gaining ? bId : aId));
					if (mutation == 0) ancestor->SetVisible(false);
					if (mutation == 1) ancestor->SetEnabled(false);
					if (mutation == 2) ancestor->SetRect({40, 0, 25, 12});
					if (mutation == 3) root.CancelInput();
					if (mutation == 4) root.SuspendInput();
				};
				auto a = std::make_unique<FFocusMutationFixture>(SCellRect{0, 0, 8, 3}, 1, activations, deliveries,
					[&](bool focused)
					{
						if (armed && !focused && ++losses == 2 && !gaining) mutate();
					});
				auto* aPtr = a.get();
				panel->AddChild(std::move(a));
				auto b = std::make_unique<FFocusMutationFixture>(SCellRect{0, 4, 8, 3}, 2, activations, deliveries,
					[&](bool focused) { if (armed && focused && gaining) mutate(); });
				auto* bPtr = b.get();
				panel->AddChild(std::move(b));
				root.AddBase(std::move(panel));
				aId = aPtr->GetId();
				bId = bPtr->GetId();
				assert(root.GetFocusId() == aId);
				armed = true;
				SInputEvent event;
				event.ScreenWidth = 30;
				event.ScreenHeight = 15;
				event.Type = pointer ? EInputEventType::PointerDown : EInputEventType::KeyDown;
				event.Key = pointer ? EInputKey::None : EInputKey::Digit2;
				event.MouseX = 3;
				event.MouseY = 6;
				root.Dispatch(event);
				assert(hits == 1 && !armed && losses == 2);
				assert(mutation != 0 || !ancestor->IsVisible());
				assert(mutation != 1 || !ancestor->IsEnabled());
				FFrameBuffer geometryFrame;
				assert(mutation != 2 || ancestor->GetOuterRect({geometryFrame, 0, 0, {0, 0, 30, 15}}).X == 40);
				assert(deliveries == 0 && "SCENE-02 stale target delivered after focus mutation");
				assert(activations == 0 && root.GetCaptureId() == 0);
				assert(root.GetFocusId() == (mutation < 3 ? 0 : bId));
				std::cout << "PASS SCENE-02 pointer=" << pointer << " gaining=" << gaining << " mutation=" << mutation
					<< " callback-hit=1 deliveries=0 activations=0 capture=0\n";
			}
		}
	}
}

// 실제 EXE 수용 검사만 사용하는 기존 native fixture 모드다. 암호문은 production 저장소만 쓴다.
static std::uint32_t FindNewGameScene(const GameData::FGameDataStore& store)
{
	for (const auto& option : store.GetSceneOptions())
	{
		if (option.sceneType == GameData::Common::ESceneType::MainMenu &&
			option.actionType == GameData::Common::ESceneActionType::StartGame) return option.targetSceneId;
	}
	throw std::runtime_error("new-game option missing");
}

static void PrintProgress(const InGameData& progress, const GameData::FGameDataStore& store)
{
	SCharacterStats stats;
	std::string error;
	if (!progress.GetStats(store, stats, error)) throw std::runtime_error(error);
	std::cout << "{\"hp\":" << progress.GetCharacter().CurrentHp << ",\"gold\":" << progress.GetGold()
		<< ",\"maxHp\":" << stats.MaxHp << ",\"attack\":" << stats.Attack << ",\"defense\":" << stats.Defense
		<< ",\"inventoryCount\":" << progress.GetInventory().size()
		<< ",\"achievementCount\":" << progress.GetAchievements().size() << "}\n";
}

static int ProgressFixtureMode(const std::string& mode, const std::filesystem::path& package,
	const std::filesystem::path& saveRoot)
{
	std::array<std::uint8_t, 32> key{};
	std::string error;
	GameData::FGameDataStore store;
	const bool loaded = GameData::ReadGameDataKey(key, error) && store.Load(package, key, error);
	SecureZeroMemory(key.data(), key.size());
	if (!loaded) throw std::runtime_error(error);
	FSaveDataStore saves(saveRoot);
	std::unique_ptr<InGameData> progress;
	std::vector<std::uint8_t> plain;
	if (mode == "--prepare-save")
	{
		if (!InGameData::CreateNew(store, FindNewGameScene(store), progress, error)) throw std::runtime_error(error);
		const auto initialHp = progress->GetCharacter().CurrentHp;
		const auto initialGold = progress->GetGold();
		const std::uint32_t hp = initialHp == 7 ? 6 : (std::min)(initialHp, 7u);
		const std::uint32_t gold = initialGold == 4321 ? 4322u : 4321u;
		if (hp == initialHp || !progress->SetCurrentHp(store, hp, error) || !progress->SetGold(gold, error) ||
			!progress->Serialize(plain, error) || !saves.Save(plain, error)) throw std::runtime_error("fixture mutation/save failed: " + error);
	}
	else
	{
		if (!saves.Load(plain, error) || !InGameData::Deserialize(plain, store, progress, error)) throw std::runtime_error(error);
	}
	if (!plain.empty()) SecureZeroMemory(plain.data(), plain.size());
	PrintProgress(*progress, store);
	return 0;
}

class SSaveRootScope final
{
public:
	explicit SSaveRootScope(const std::filesystem::path& root)
	{
		const DWORD count = GetEnvironmentVariableW(L"TEXT_RPG_SAVE_ROOT", nullptr, 0);
		m_hadValue = count != 0 || GetLastError() != ERROR_ENVVAR_NOT_FOUND;
		if (count)
		{
			std::vector<wchar_t> value(count);
			assert(GetEnvironmentVariableW(L"TEXT_RPG_SAVE_ROOT", value.data(), count) < count);
			m_previous = value.data();
		}
		assert(SetEnvironmentVariableW(L"TEXT_RPG_SAVE_ROOT", root.c_str()));
	}
	~SSaveRootScope()
	{
		(void)SetEnvironmentVariableW(L"TEXT_RPG_SAVE_ROOT", m_hadValue ? m_previous.c_str() : nullptr);
	}
private:
	bool m_hadValue = false;
	std::wstring m_previous;
};

static std::vector<std::uint8_t> ReadSaveBytes(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {(std::istreambuf_iterator<char>(stream)), {}};
}

static void TestCombatSession(GameData::FGameDataStore& store, const std::filesystem::path& work)
{
	SSaveRootScope environment(std::filesystem::absolute(work / "combat-saves"));
	FGameSession session(store);
	FGameUiBridge ui;
	FTextRPGSceneManager scenes(store);
	std::string error;
	std::wstring sceneError;
	assert(scenes.LoadStartupScene(ui, sceneError));
	PublishTestFrame(scenes);
	assert(session.NewGame(FindNewGameScene(store), scenes, ui, error) == EGameStartResult::Queued);
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	const auto* town = session.GetProgress();
	SInputEvent input{};
	input.ScreenWidth = 132;
	input.ScreenHeight = 43;
	input.Type = EInputEventType::KeyDown;
	input.Key = EInputKey::Digit4;
	input.BatchId = 910;
	const auto move = scenes.HandleInput(input);
	assert(move.Type == ESceneAction::Travel);
	assert(session.Travel(move.TargetSceneId, scenes, ui, error));
	assert(session.GetProgress() == town && session.GetProgress()->GetBattle().RuleId == 0);
	assert(!session.BasicAttack(scenes, error));
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	assert(session.GetProgress()->GetSceneId() == move.TargetSceneId);
	assert(session.GetProgress()->GetBattle().MonsterHp > 0 && session.GetProgress()->GetBattle().Round == 0);
	input.Type = EInputEventType::KeyUp;
	input.BatchId = 911;
	(void)scenes.HandleInput(input);
	input.Type = EInputEventType::KeyDown;
	input.Key = EInputKey::Digit1;
	input.BatchId = 912;
	const auto attack = scenes.HandleInput(input);
	assert(attack.Type == ESceneAction::BasicAttack && session.BasicAttack(scenes, error));
	const auto* stable = session.GetProgress();
	const auto savedBattle = stable->GetBattle();
	const auto savedHp = stable->GetCharacter().CurrentHp;
	assert(savedBattle.Round == 1 && savedBattle.RandCount > 0 && savedBattle.HitCount >= 1);
	FFrameBuffer frame(132, 43);
	scenes.Render(frame);
	const auto rendered = frame.GetCells();
	{
		std::ofstream snapshot(work / "combat-round1.utf16", std::ios::binary);
		snapshot.write(reinterpret_cast<const char*>(rendered.data()), static_cast<std::streamsize>(rendered.size() * sizeof(wchar_t)));
		assert(snapshot);
	}
	assert(session.Save(error) && session.CanContinue(scenes, ui, error));
	assert(session.Continue(scenes, ui, error));
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	assert(session.GetProgress()->GetBattle() == savedBattle && session.GetProgress()->GetCharacter().CurrentHp == savedHp);
	assert(session.BasicAttack(scenes, error));
	std::uint32_t attempts = 0;
	while (session.GetProgress()->GetBattle().Outcome == 0 && attempts++ < 100)
		assert(session.BasicAttack(scenes, error));
	assert(session.GetProgress()->GetBattle().Outcome != 0);
	const auto finished = session.GetProgress()->GetBattle();
	const auto finalHp = session.GetProgress()->GetCharacter().CurrentHp;
	assert(!session.BasicAttack(scenes, error));
	assert(session.GetProgress()->GetBattle() == finished && session.GetProgress()->GetCharacter().CurrentHp == finalHp);
	scenes.Render(frame);
	assert(frame.GetCells() != rendered);
	input.Key = EInputKey::Digit4;
	input.Type = EInputEventType::KeyUp;
	input.BatchId = 913;
	(void)scenes.HandleInput(input);
	input.Type = EInputEventType::KeyDown;
	input.BatchId = 914;
	const auto back = scenes.HandleInput(input);
	assert(back.Type == ESceneAction::Travel && session.Travel(back.TargetSceneId, scenes, ui, error));
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	assert(session.GetProgress()->GetBattle().RuleId == 0);
	assert(session.GetProgress()->GetCharacter().CurrentHp > 0);
	assert(session.Travel(move.TargetSceneId, scenes, ui, error));
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	assert(session.GetProgress()->GetBattle().Round == 0 && session.GetProgress()->GetBattle().RandCount == 0);
	std::cout << "PASS IGD combat producer Store UI travel EndFrame round archive Continue result return reentry\n";
}

static void TestProgressSession(GameData::FGameDataStore& store, const std::filesystem::path& work)
{
	const auto saveRoot = std::filesystem::absolute(work / "progress-saves");
	SSaveRootScope environment(saveRoot);
	FGameSession session(store);
	FGameUiBridge ui;
	FTextRPGSceneManager scenes(store);
	std::string error;
	std::wstring sceneError;
	assert(session.Save(error));
	assert(!std::filesystem::exists(saveRoot));
	assert(!session.CanContinue(scenes, ui, error));
	assert(scenes.LoadStartupScene(ui, sceneError));
	PublishTestFrame(scenes);
	const auto target = FindNewGameScene(store);
	assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
	assert(session.GetProgress() == nullptr && ui.GetBoundScene()->GetProgress() == nullptr);
	assert(!std::filesystem::exists(saveRoot));
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Queued);
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::None);
	const auto* initial = session.GetProgress();
	const auto* initialScene = ui.GetBoundScene();
	assert(initial && initialScene && initialScene->GetProgress() == initial);
	assert(initial->GetSceneId() == target && std::filesystem::is_regular_file(saveRoot / "InGameData.sav"));
	const auto originalBytes = ReadSaveBytes(saveRoot / "InGameData.sav");
	const auto originalKey = ReadSaveBytes(saveRoot / "SaveData.key");
	assert(!originalKey.empty());
	assert(!originalBytes.empty());
	assert(session.NewGame(kMissingDataId, scenes, ui, error) == EGameStartResult::Failed);
	assert(session.GetProgress() == initial && ui.GetBoundScene() == initialScene);
	assert(ReadSaveBytes(saveRoot / "InGameData.sav") == originalBytes);
	assert(ReadSaveBytes(saveRoot / "SaveData.key") == originalKey);
	assert(scenes.LoadSceneById(target, ui, sceneError));
	assert(ui.GetBoundScene() == initialScene);
	PublishTestFrame(scenes);
	assert(session.GetProgress() == initial && ui.GetBoundScene()->GetProgress() == initial);
	assert(session.CanContinue(scenes, ui, error));
	SInputEvent moveInput{};
	moveInput.Type = EInputEventType::KeyDown;
	moveInput.Key = EInputKey::Digit4;
	moveInput.BatchId = 901;
	moveInput.ScreenWidth = 132;
	moveInput.ScreenHeight = 43;
	const auto moveAction = scenes.HandleInput(moveInput);
	assert(moveAction && "Town Move must produce a playable travel action");
	moveInput.Type = EInputEventType::KeyUp;
	(void)scenes.HandleInput(moveInput);

	std::unique_ptr<InGameData> changed;
	assert(InGameData::CreateNew(store, target, changed, error));
	assert(changed->SetCurrentHp(store, 7, error) && changed->SetGold(4321, error));
	std::vector<std::uint8_t> plain;
	assert(changed->Serialize(plain, error) && FSaveDataStore(saveRoot).Save(plain, error));
	SecureZeroMemory(plain.data(), plain.size());
	assert(session.Continue(scenes, ui, error));
	assert(session.GetProgress() == initial);
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	assert(session.GetProgress() != initial && session.GetProgress()->GetGold() == 4321);
	assert(session.GetProgress()->GetCharacter().CurrentHp == 7);
	assert(ui.GetBoundScene()->GetProgress() == session.GetProgress());
	FFrameBuffer frame(132, 43);
	scenes.Render(frame);
	const auto cellText = [](const std::wstring& text)
	{
		FFrameBuffer expected(38, 1);
		expected.DrawText(0, 0, text, 7, 38);
		return expected.GetCells();
	};
	const auto matches = [&frame, &cellText](int x, int y, const std::wstring& text)
	{
		const auto expected = cellText(text);
		const auto offset = frame.GetCells().begin() + y * 132 + x;
		assert(std::equal(expected.begin(), expected.end(), offset));
	};
	SCharacterStats stats;
	assert(session.GetProgress()->GetStats(store, stats, error));
	matches(2, 2, GetLocalizedText(store, kHpTextKey) + L": 7/" + std::to_wstring(stats.MaxHp));
	matches(45, 1, GetLocalizedText(store, kGoldTextKey) + L": 4321");
	const auto* restored = session.GetProgress();
	const auto* restoredScene = ui.GetBoundScene();
	auto corrupt = ReadSaveBytes(saveRoot / "InGameData.sav");
	corrupt.back() ^= 1;
	{
		std::ofstream output(saveRoot / "InGameData.sav", std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<const char*>(corrupt.data()), static_cast<std::streamsize>(corrupt.size()));
	}
	assert(!session.Continue(scenes, ui, error));
	assert(session.GetProgress() == restored && ui.GetBoundScene() == restoredScene);
	assert(ReadSaveBytes(saveRoot / "InGameData.sav") == corrupt);
	assert(ReadSaveBytes(saveRoot / "SaveData.key") == originalKey);
	scenes.Render(frame);
	matches(45, 1, GetLocalizedText(store, kGoldTextKey) + L": 4321");
	const auto blocked = std::filesystem::absolute(work / "blocked-save-root");
	std::ofstream(blocked) << "owned failure fixture";
	{
		SSaveRootScope invalidLocation(blocked);
		assert(!session.Save(error));
		assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
		assert(session.GetProgress() == restored && ui.GetBoundScene() == restoredScene);
		PublishTestFrame(scenes);
		assert(session.TakeStartResult(scenes, error) == EGameStartResult::StartedSaveFailed);
		assert(session.GetProgress() != restored && ui.GetBoundScene()->GetProgress() == session.GetProgress());
		assert(ReadSaveBytes(saveRoot / "InGameData.sav") == corrupt);
	assert(ReadSaveBytes(saveRoot / "SaveData.key") == originalKey);
	}
	std::cout << "PASS IGD app root new-game autosave transactional restore/recreate failure preservation and localized live readout\n";
}

static std::vector<std::uint8_t> MakeLegacySessionPayload(bool framed, bool unknown, std::uint32_t contentVersion)
{
	LegacyProgressFixture::SRootInGameData legacy;
	legacy.ContentVersion = contentVersion;
	legacy.SceneId = unknown ? L"UnknownScene" : L"Town";
	std::vector<std::uint8_t> bytes;
	if (framed)
	{
		std::string error;
		assert(FSerializationArchive::Serialize(legacy, bytes, error, 1));
		return bytes;
	}
	// 독립 구형 IGD1 wire를 실제 암호화 저장에 전달한다. 새 제품 codec으로 기대값을 만들지 않는다.
	const auto number = [&bytes](std::uint32_t value)
	{
		for (std::size_t i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(value >> (8 * i)));
	};
	const auto id = [&bytes, &number](const std::wstring& value)
	{
		number(static_cast<std::uint32_t>(value.size()));
		for (const auto unit : value)
		{
			bytes.push_back(static_cast<std::uint8_t>(unit));
			bytes.push_back(static_cast<std::uint8_t>(static_cast<std::uint16_t>(unit) >> 8));
		}
	};
	number(0x31444749); number(1); number(contentVersion);
	id(legacy.SceneId); id(legacy.Character->CharacterId);
	number(1); number(0); number(7); number(0); number(0); number(0);
	number(54321); number(0); number(0);
	return bytes;
}

static void TestLegacyProgressSession(GameData::FGameDataStore& gameData, const std::filesystem::path& work)
{
	const auto* profile = gameData.GetDefaultNewGameProfile();
	assert(profile);
	for (const bool framed : {false, true})
	{
		const auto root = std::filesystem::absolute(work / (framed ? "legacy-cgsa-session" : "legacy-igd1-session"));
		SSaveRootScope environment(root);
		FGameSession session(gameData);
		FGameUiBridge ui;
		FTextRPGSceneManager scenes(gameData);
		std::string error;
		const auto target = FindNewGameScene(gameData);
		assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
		PublishTestFrame(scenes);
		assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
		FSaveDataStore slot(root);
		const auto key = ReadSaveBytes(root / "SaveData.key");
		assert(!key.empty());
		const auto legacy = MakeLegacySessionPayload(framed, false, profile->contentVersion);
		assert(slot.Save(legacy, error));
		const auto cipher = ReadSaveBytes(root / "InGameData.sav");
		const auto* previous = session.GetProgress();
		assert(session.CanContinue(scenes, ui, error));
		assert(session.GetProgress() == previous && ReadSaveBytes(root / "InGameData.sav") == cipher);
		assert(session.Continue(scenes, ui, error) && session.GetProgress() == previous);
		PublishTestFrame(scenes);
		assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
		assert(session.GetProgress()->GetSceneId() == kTownSceneId && session.GetProgress()->GetGold() == 54321);
		assert(session.GetProgress()->GetCharacter().CurrentHp == 7);
		assert(ui.GetBoundScene()->GetProgress() == session.GetProgress());
		assert(ReadSaveBytes(root / "InGameData.sav") == cipher && ReadSaveBytes(root / "SaveData.key") == key);
		assert(session.Save(error));
		std::vector<std::uint8_t> current;
		assert(slot.Load(current, error) && current.size() >= 12);
		assert(current[0] == 'C' && current[1] == 'G' && current[2] == 'S' && current[3] == 'A');
		assert(current[8] == 3 && current[9] == 0 && current[10] == 0 && current[11] == 0);
		std::unique_ptr<InGameData> restored;
		assert(InGameData::Deserialize(current, gameData, restored, error));
		assert(restored->GetGold() == 54321 && restored->GetCharacter().CurrentHp == 7);
		assert(ReadSaveBytes(root / "SaveData.key") == key);
		const auto unknown = MakeLegacySessionPayload(framed, true, profile->contentVersion);
		assert(slot.Save(unknown, error));
		const auto protectedCipher = ReadSaveBytes(root / "InGameData.sav");
		const auto* active = session.GetProgress();
		const auto* scene = ui.GetBoundScene();
		assert(!session.CanContinue(scenes, ui, error) && error == "SceneId definition is missing");
		assert(!session.Continue(scenes, ui, error) && error == "SceneId definition is missing");
		assert(session.GetProgress() == active && ui.GetBoundScene() == scene);
		assert(!session.Save(error) && error == "SceneId definition is missing");
		assert(ReadSaveBytes(root / "InGameData.sav") == protectedCipher && ReadSaveBytes(root / "SaveData.key") == key);
		assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
		assert(session.GetProgress() == active && ui.GetBoundScene() == scene);
		PublishTestFrame(scenes);
		assert(session.TakeStartResult(scenes, error) == EGameStartResult::StartedSaveFailed);
		assert(error == "SceneId definition is missing");
		assert(ui.GetBoundScene()->GetProgress() == session.GetProgress());
		assert(ReadSaveBytes(root / "InGameData.sav") == protectedCipher && ReadSaveBytes(root / "SaveData.key") == key);
		SecureZeroMemory(current.data(), current.size());
	}
	std::cout << "PASS IGD1 and CGSA schema1 authenticated Continue stays read-only, numeric Save migrates, "
		"unknown legacy IDs protect slot key and failed Continue state including NewGame autosave\n";
}

static void TestNewerProgressSlotProtection(GameData::FGameDataStore& gameData, const std::filesystem::path& work)
{
	const auto root = std::filesystem::absolute(work / "newer-progress-slots");
	SSaveRootScope environment(root);
	FGameSession session(gameData);
	FGameUiBridge ui;
	FTextRPGSceneManager scenes(gameData);
	std::string error;
	const auto target = FindNewGameScene(gameData);
	assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	FSaveDataStore store(root);
	std::vector<std::uint8_t> seed;
	assert(store.Load(seed, error) && seed.size() >= 12);
	assert(seed[0] == 'C' && seed[1] == 'G' && seed[2] == 'S' && seed[3] == 'A');
	const auto key = ReadSaveBytes(root / "SaveData.key");
	const auto put = [](std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value)
	{
		assert(at <= bytes.size() && bytes.size() - at >= 4);
		for (std::size_t i = 0; i < 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
	};
	auto newerSchema = seed;
	put(newerSchema, 8, GameData::InGameData::kSchemaVersion + 1);
	auto newerFormat = seed;
	put(newerFormat, 4, 2);
	struct SNewerSlot
	{
		std::vector<std::uint8_t> Bytes;
		const char* WriteError;
		const char* ReadError;
	};
	const std::array<SNewerSlot, 3> cases{{
		{std::move(newerSchema), "Refusing to overwrite a newer progress schema version.",
			"unsupported archive or newer schema version"},
		{std::move(newerFormat), "Refusing to overwrite a newer progress format version.",
			"unsupported archive or newer schema version"},
		{{'I', 'G', 'D', '1', 2, 0, 0, 0}, "Refusing to overwrite a newer progress format version.",
			"Unsupported progress format version"}
	}};
	for (const auto& test : cases)
	{
		// 미래 버전 평문을 실제 인증 저장 경로로 게시한다. 암호문/키는 테스트 전용 루트에만 있다.
		assert(store.Save(test.Bytes, error));
		const auto cipher = ReadSaveBytes(root / "InGameData.sav");
		const auto* active = session.GetProgress();
		const auto* activeScene = ui.GetBoundScene();
		assert(!session.CanContinue(scenes, ui, error) && error == test.ReadError);
		assert(!session.Continue(scenes, ui, error) && error == test.ReadError);
		assert(session.GetProgress() == active && ui.GetBoundScene() == activeScene);
		assert(!session.Save(error) && error == test.WriteError);
		assert(ReadSaveBytes(root / "InGameData.sav") == cipher && ReadSaveBytes(root / "SaveData.key") == key);
		// NewGame의 안전점 게시 정책은 유지하지만 미래 슬롯을 덮는 자동 저장만 실패한다.
		assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
		PublishTestFrame(scenes);
		assert(session.TakeStartResult(scenes, error) == EGameStartResult::StartedSaveFailed);
		assert(error == test.WriteError && session.GetProgress() != active);
		assert(ui.GetBoundScene()->GetProgress() == session.GetProgress());
		assert(ReadSaveBytes(root / "InGameData.sav") == cipher && ReadSaveBytes(root / "SaveData.key") == key);
		assert(store.Save(seed, error) && session.Save(error));
	}
	SecureZeroMemory(seed.data(), seed.size());
	std::cout << "PASS IGD newer schema and framed or legacy format veto Save and NewGame autosave "
		"without replacing slot key or failed Continue active state\n";
}

class FRootLifetimeButton final : public FUiButton
{
public:
	FRootLifetimeButton(const InGameData& root, int& hits)
		: FUiButton({1, 1, 20, 3}, L"old root view", []() {}), Root(root), Hits(hits) {}
	~FRootLifetimeButton() override
	{
		// old root는 자신의 비소유 UI destructor가 끝날 때까지 유효해야 한다.
		assert(Root.GetGold() == 9876);
		++Hits;
	}
private:
	const InGameData& Root;
	int& Hits;
};

static void TestProgressPublicationFailure(GameData::FGameDataStore& store, const std::filesystem::path& work)
{
	const auto root = std::filesystem::absolute(work / "publication-failure-saves");
	SSaveRootScope environment(root);
	SEndFrameProbeState old;
	FGameSession session(store);
	FGameUiBridge ui;
	FTextRPGSceneManager scenes(store);
	std::string error;
	std::wstring sceneError;
	const auto target = FindNewGameScene(store);
	assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	const auto* identity = session.GetProgress();
	const auto cipher = ReadSaveBytes(root / "InGameData.sav");
	const auto key = ReadSaveBytes(root / "SaveData.key");
	assert(!cipher.empty() && !key.empty());
	assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(old)));
	PublishTestFrame(scenes);
	assert(ui.GetBoundScene() == nullptr && session.GetProgress() == identity);
	old.Exit = []() { throw std::runtime_error("owned EndFrame activation fault"); };
	assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
	assert(!scenes.EndFrame(sceneError) && sceneError == L"owned EndFrame activation fault");
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Failed && error == "owned EndFrame activation fault");
	assert(session.GetProgress() == identity && old.Lifetime.Exited == 1 && old.Lifetime.Destroyed == 0);
	assert(ReadSaveBytes(root / "InGameData.sav") == cipher && ReadSaveBytes(root / "SaveData.key") == key);
	old.Exit = {};
	assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
	const auto* recovered = session.GetProgress();
	auto* recoveredScene = ui.GetBoundScene();
	assert(recovered && recovered != identity && recoveredScene->GetProgress() == recovered);
	const auto recoveredCipher = ReadSaveBytes(root / "InGameData.sav");
	assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
	SInputEvent escape;
	escape.Key = EInputKey::Escape;
	assert(scenes.HandleInputFrame({escape}).Type == ESceneAction::Quit);
	PublishTestFrame(scenes);
	assert(session.TakeStartResult(scenes, error) == EGameStartResult::Failed);
	assert(session.GetProgress() == recovered && ui.GetBoundScene() == recoveredScene);
	assert(ReadSaveBytes(root / "InGameData.sav") == recoveredCipher && ReadSaveBytes(root / "SaveData.key") == key);
	std::cout << "PASS IGD EndFrame activation fault preserves active root cipher key then recovers Quit cancels unpublished autosave\n";
}

struct SDestructorProbeState final
{
	int Hits = 0;
	int Completed = 0;
	std::function<void()> Callback;
};

class FDestructorProbeButton final : public FUiButton
{
public:
	explicit FDestructorProbeButton(SDestructorProbeState& state)
		: FUiButton({1, 1, 20, 3}, L"teardown", []() {}), State(state) {}
	~FDestructorProbeButton() override
	{
		auto* state = &State;
		++state->Hits;
		if (state->Hits != 1)
		{
			std::cout << "FAIL CANCEL-01 recursive candidate teardown hits=" << state->Hits << '\n';
			std::exit(86); // 재귀 delete/UAF 전에 외부 카운터와 고정 원인으로 실패한다.
		}
		if (state->Callback) state->Callback();
		++state->Completed;
	}
private:
	SDestructorProbeState& State;
};

static void InvokeCancelAction(FTextRPGSceneManager& scenes, bool escape)
{
	if (escape)
	{
		SInputEvent event;
		event.Key = EInputKey::Escape;
		assert(scenes.HandleInput(event).Type == ESceneAction::Quit);
	}
	else scenes.CancelPendingTransitions();
}

static void InvokeBoundaryOperation(IScene& scene, int stage)
{
	SInputEvent event;
	event.Key = EInputKey::Down;
	FFrameBuffer frame(132, 43);
	if (stage == 0) (void)scene.HandleInput(event);
	if (stage == 1) (void)scene.HandleInputFrame({event});
	if (stage == 2) scene.Update(0);
	if (stage == 3) scene.Render(frame);
	if (stage == 4) scene.OnEnter();
	if (stage == 5) scene.OnExit();
}

static void TestBusySceneBoundary()
{
	for (int target = 0; target != 3; ++target) // pending, active, active -> pending 중첩
	{
		for (int action = 0; action != 3; ++action) // Cancel, Escape, EndFrame
		{
			for (int stage = 0; stage != 6; ++stage)
			{
				SEndFrameProbeState active, pending;
				GameData::FGameDataStore store;
				FTextRPGSceneManager scenes(store);
				auto old = std::make_unique<FEndFrameProbe>(active);
				auto* oldView = old.get();
				assert(scenes.SetScene(std::move(old)));
				PublishTestFrame(scenes);
				auto next = std::make_unique<FEndFrameProbe>(pending);
				auto* nextView = next.get();
				int hits = 0;
				const auto boundary = [&]()
				{
					++hits;
					if (target == 1) assert(scenes.SetScene(std::move(next)));
					std::wstring error;
					if (action == 0) scenes.CancelPendingTransitions();
					if (action == 1) InvokeCancelAction(scenes, true);
					if (action == 2) assert(!scenes.EndFrame(error) && !error.empty());
					std::cout << "TRACE BOUNDARY-01 target=" << target << " stage=" << stage << " action=" << action
						<< " hit=" << hits << " pending-destroyed-inside=" << pending.Lifetime.Destroyed << '\n';
					assert(hits == 1 && pending.Lifetime.Destroyed == 0 && active.Lifetime.Destroyed == 0);
					assert(scenes.IsQuitting() == (action != 2));
					assert(!scenes.EndFrame(error) && !error.empty()); // 취소도 공통 eligibility를 우회하지 않는다.
				};
				auto& state = target == 1 ? active : pending;
				if (stage < 2) state.Input = boundary;
				if (stage == 2) state.Update = boundary;
				if (stage == 3) state.Render = boundary;
				if (stage == 4) state.Enter = boundary;
				if (stage == 5) state.Exit = boundary;
				if (target != 1) assert(scenes.SetScene(std::move(next)));
				if (target == 2)
				{
					active.Update = [&]() { InvokeBoundaryOperation(*nextView, stage); };
					oldView->Update(0);
					active.Update = {};
				}
				else InvokeBoundaryOperation(target == 0 ? *nextView : *oldView, stage);
				assert(hits == 1 && pending.Lifetime.Destroyed == 0 && active.Lifetime.Destroyed == 0);
				if (action != 2)
				{
					InvokeCancelAction(scenes, action == 0); // unwind 뒤 idle 재취소도 EndFrame 이월을 우회하지 않는다.
					std::cout << "TRACE BOUNDARY-01 repeat-after-unwind pending-destroyed=" << pending.Lifetime.Destroyed << '\n';
					assert(pending.Lifetime.Destroyed == 0);
				}
				state.Input = state.Update = state.Render = state.Enter = state.Exit = {};
				PublishTestFrame(scenes);
				assert(pending.Lifetime.Destroyed == (action != 2 ? 1 : 0));
				assert(active.Lifetime.Destroyed == (action == 2 ? 1 : 0));
				assert(pending.Lifetime.Entered == (action == 2 ? (stage == 4 && target != 1 ? 2 : 1) : (stage == 4 && target != 1 ? 1 : 0)));
				PublishTestFrame(scenes);
				assert(pending.Lifetime.Destroyed == (action != 2 ? 1 : 0));
			}
		}
	}
	std::cout << "PASS APP01 BOUNDARY-01 busy pending active nested six NVI Cancel Escape EndFrame lifetime eligibility destroy once\n";
}

struct SBoundaryUiState final
{
	int Hits = 0;
	int Destroyed = 0;
	int RootReads = 0;
	int Deliveries = 0;
	bool Armed = false;
	const InGameData* Progress = nullptr;
	std::function<void()> Callback;
};

class FBoundaryUi final : public FUiButton
{
public:
	explicit FBoundaryUi(SBoundaryUiState& state) : FUiButton({0, 0, 12, 3}, L"boundary", []() {}), State(state) {}
	~FBoundaryUi() override
	{
		++State.Destroyed;
		if (State.Progress) { assert(State.Progress->GetGold() == 7654); ++State.RootReads; }
	}
	void Hit() const
	{
		if (!State.Armed) return;
		auto* state = &State;
		state->Armed = false;
		++state->Hits;
		const auto callback = state->Callback; // early-delete RED에서도 멤버 lambda의 해제에 의존하지 않는다.
		callback();
	}
	void SetFocused(bool focused) override { FUiButton::SetFocused(focused); Hit(); }
	EUiReply OnKeyDown(const SInputEvent&) override { ++State.Deliveries; Hit(); return EUiReply::Handled; }
	void Render(const SUiRenderContext& context) const override { FUiButton::Render(context); Hit(); }
	void OnPointerCancel() override { FUiButton::OnPointerCancel(); Hit(); }
private:
	SBoundaryUiState& State;
};

struct SRootEntryState final
{
	int Hook = -1;
	int Hits = 0;
	int Destroyed = 0;
	int RootReads = 0;
	int PointerCancels = 0;
	int DragBegins = 0;
	int DragCancels = 0;
	int KeyDeliveries = 0;
	int AssignThrows = 0;
	bool ThrowAssign = false;
	int GetterCalls = 0;
	bool Armed = false;
	const InGameData* Progress = nullptr;
	std::function<void()> Callback;
	std::function<void()> Destroy;
};

class FRootEntryUi final : public FUiButton
{
public:
	explicit FRootEntryUi(SRootEntryState& state)
		: FUiButton({0, 0, 12, 3}, L"entry", []() {}), State(state) {}
	~FRootEntryUi() override
	{
		++State.Destroyed;
		Hit(2);
		if (State.Progress) { assert(State.Progress->GetGold() == 7654); ++State.RootReads; }
		if (State.Destroy) State.Destroy();
	}
	void AssignIds(FUiId& nextId) override
	{
		FUiButton::AssignIds(nextId);
		Hit(3);
		if (State.ThrowAssign)
		{
			++State.AssignThrows;
			throw std::runtime_error("owned root entry AssignIds fault");
		}
	}
	FUiId GetId() const noexcept override
	{
		++State.GetterCalls;
		const auto id = FUiButton::GetId();
		Hit(0);
		return id;
	}
	IUI* Find(FUiId id) noexcept override
	{
		Hit(1);
		return FUiButton::Find(id);
	}
	EUiReply OnKeyDown(const SInputEvent&) override { ++State.KeyDeliveries; return EUiReply::Handled; }
	void Hit(int hook) const
	{
		auto* state = &State;
		if (!state->Armed || state->Hook != hook) return;
		state->Armed = false;
		++state->Hits;
		const auto callback = state->Callback;
		callback();
	}
	bool SupportsDrag() const noexcept override { return true; }
	EUiReply OnDrag(EUiDragPhase phase, const SInputEvent&) override
	{
		if (phase == EUiDragPhase::Begin) ++State.DragBegins;
		if (phase == EUiDragPhase::Cancel) ++State.DragCancels;
		return EUiReply::Handled;
	}
	void OnPointerCancel() override { ++State.PointerCancels; FUiButton::OnPointerCancel(); }
private:
	SRootEntryState& State;
};

class FNeutralBridgeTarget final : public IUiInputTarget
{
public:
	void DispatchUiInput(const SInputEvent&) override { ++Deliveries; }
	void CancelUiInput() override { ++Cancels; }
	FSceneUiRoot* GetUiBaseRoot() const noexcept override { return const_cast<FSceneUiRoot*>(&Root); }
	FSceneUiRoot Root;
	int Deliveries = 0;
	int Cancels = 0;
};

static void TestGameUiBridgeTarget(GameData::FGameDataStore& store)
{
	FGameUiBridge bridge;
	FUIManager& base = bridge;
	FNeutralBridgeTarget neutral;
	assert(bridge.GetBoundScene() == nullptr && base.GetBoundTarget() == nullptr);
	base.Bind(neutral);
	base.HandleInput({});
	const bool typedNull = bridge.GetBoundScene() == nullptr;
	std::cout << "TRACE CORE-02 generic-base-bind typed-null=" << typedNull << " deliveries=" << neutral.Deliveries << '\n';
	if (!typedNull)
	{
		std::cout << "FAIL CORE-02 neutral public target produced unchecked typed Scene pointer\n";
		std::exit(89);
	}
	assert(base.GetBoundTarget() == &neutral && base.GetBoundRoot() == &neutral.Root && neutral.Deliveries == 1);
	base.Clear();
	assert(neutral.Cancels == 1 && bridge.GetBoundScene() == nullptr);
	FDataScene real(store, bridge);
	std::wstring error;
	assert(real.LoadFromGameData(kMainMenuSceneId, error));
	base.Bind(real);
	assert(bridge.GetBoundScene() == &real && base.GetBoundRoot() == real.GetUiBaseRoot());
	bridge.Bind(real);
	assert(bridge.GetBoundScene() == &real);
	base.Clear();
	assert(bridge.GetBoundScene() == nullptr && base.GetBoundTarget() == nullptr);
	std::cout << "PASS APP01 CORE-02 neutral base Bind input Clear typed-null real Scene positive typed/base binding\n";
}

static void TestSceneOperationBoundary(GameData::FGameDataStore& store)
{
	for (int entry = 0; entry != 8; ++entry)
	{
		SRootEntryState sentinel, incoming;
		SSceneLifetime next;
		FGameUiBridge bridge;
		FTextRPGSceneManager scenes(store);
		std::wstring error;
		assert(scenes.LoadStartupScene(bridge, error));
		PublishTestFrame(scenes);
		auto* old = bridge.GetBoundScene();
		const auto marker = old->OpenUiLayer(std::make_unique<FRootEntryUi>(sentinel));
		old->FlushUiLayerCommands();
		assert(marker && sentinel.Destroyed == 0);
		incoming.Hook = entry == 0 ? 0 : entry == 1 ? 3 : 2;
		incoming.Armed = true;
		incoming.ThrowAssign = entry == 5;
		incoming.Callback = [&]()
		{
			const bool operating = old->IsOperating();
			assert(incoming.Hits == 1);
			assert(scenes.SetScene(std::make_unique<FSceneLifetimeProbe>(next)));
			const bool ended = scenes.EndFrame(error);
			std::cout << "TRACE CORE-01 entry=" << entry << " hit=" << incoming.Hits << " operating=" << operating
				<< " ended=" << ended << " sentinel-destroyed=" << sentinel.Destroyed << " next-entered=" << next.Entered << '\n';
			if (!operating || ended || sentinel.Destroyed || next.Entered)
			{
				std::cout << "FAIL CORE-01 direct Scene operation escaped lifetime boundary\n";
				std::exit(88); // 해제된 Scene/공유 ID 포인터로 복귀하지 않고 정확한 RED 원인을 보존한다.
			}
			assert(bridge.GetBoundScene() == old && (entry == 2 || old->GetUiLayer(marker)));
			assert(old->GetProgress() == nullptr && old->IsOperating());
		};
		auto value = std::make_unique<FRootEntryUi>(incoming);
		bool caught = false;
		if (entry == 2 || entry == 6 || entry == 7)
		{
			const auto pendingLayer = old->OpenUiLayer(std::move(value));
			assert(pendingLayer);
			assert(incoming.Hits == 0 && incoming.Destroyed == 0);
			if (entry == 2) assert(old->LoadFromGameData(kMainMenuSceneId, error));
			else if (entry == 6) old->DiscardUiLayerCommands();
			else
			{
				old->FlushUiLayerCommands();
				old->CloseUiLayer(pendingLayer);
				old->FlushUiLayerCommands(); // 로컬 command snapshot의 마지막 정리까지 Scene이 살아 있다.
			}
		}
		else if (entry == 3) assert(old->AddUiToLayer(0, std::move(value)) == 0);
		else
		{
			if (entry == 4) assert(old->OpenUiLayer(nullptr, (std::numeric_limits<std::uint32_t>::max)()));
			try
			{
				const auto id = old->OpenUiLayer(std::move(value));
				assert((entry == 4 && id == 0) || (entry < 2 && id));
			}
			catch (const std::runtime_error& fault)
			{
				caught = true;
				assert(entry == 5 && incoming.AssignThrows == 1 &&
					std::string(fault.what()) == "owned root entry AssignIds fault");
			}
		}
		assert(incoming.Hits == 1 && caught == (entry == 5));
		assert(!old->IsOperating() && bridge.GetBoundScene() == old && next.Entered == 0);
		assert(incoming.Destroyed == (entry < 2 ? 0 : 1));
		assert(sentinel.Destroyed == (entry == 2 ? 1 : 0));
		PublishTestFrame(scenes);
		assert(incoming.Destroyed == 1 && sentinel.Destroyed == 1 && next.Entered == 1 && bridge.GetBoundScene() == nullptr);
		PublishTestFrame(scenes);
		assert(incoming.Hits == 1 && incoming.Destroyed == 1 && sentinel.Destroyed == 1 && next.Entered == 1);
	}
	std::cout << "PASS APP01 CORE-01 unpublished GetId AssignIds reload pending destructor refusal throw snapshot cleanup EndFrame defer destroy once\n";
}

static void TestRootEntryBoundary(GameData::FGameDataStore& store, const std::filesystem::path& work, int entry)
{
	for (int target = 0; target != 3; ++target) // pending, active, active NVI -> pending root
	{
		for (const bool progressRequest : {true, false})
		{
			for (const int action : (entry == 1 ? std::array{0, 1, 2} : std::array{2, 1, 0}))
			{
				SRootEntryState oldState, nextState, incoming;
				SBoundaryUiState nestedState;
				FGameUiBridge ui;
				std::unique_ptr<InGameData> active, candidate;
				FTextRPGSceneManager scenes(store);
				std::unique_ptr<FDataScene> prepared;
				std::string error;
				std::wstring diagnostic;
				const auto sceneId = FindNewGameScene(store);
				assert(InGameData::CreateNew(store, sceneId, candidate, error) && candidate->SetGold(7654, error));
				assert(scenes.PrepareSceneById(sceneId, ui, candidate.get(), prepared, diagnostic));
				assert(scenes.CommitProgress(prepared, active, candidate, diagnostic));
				PublishTestFrame(scenes);
				bool published = false;
				assert(scenes.TakeProgressResult(active, published, diagnostic) && published);
				const auto* identity = active.get();
				std::vector<std::uint8_t> before;
				assert(identity->Serialize(before, error));
				const auto saveRoot = work / ("root-entry-" + std::to_string(entry) + std::to_string(target) +
					std::to_string(progressRequest) + std::to_string(action));
				assert(FSaveDataStore(saveRoot).Save(before, error));
				const auto cipher = ReadSaveBytes(saveRoot / "InGameData.sav");
				const auto key = ReadSaveBytes(saveRoot / "SaveData.key");
				assert(!cipher.empty() && !key.empty());
				auto* old = ui.GetBoundScene();
				oldState.Progress = identity;
				const auto oldLayer = old->OpenUiLayer(std::make_unique<FRootEntryUi>(oldState));
				old->FlushUiLayerCommands();
				if (progressRequest)
				{
					assert(InGameData::CreateNew(store, sceneId, candidate, error) && candidate->SetGold(7654, error));
					assert(scenes.PrepareSceneById(sceneId, ui, candidate.get(), prepared, diagnostic));
					nextState.Progress = candidate.get();
				}
				else
				{
					assert(scenes.PrepareSceneById(sceneId, ui, identity, prepared, diagnostic));
					nextState.Progress = identity;
				}
				auto* next = prepared.get();
				const auto nextLayer = next->OpenUiLayer(std::make_unique<FRootEntryUi>(nextState));
				next->FlushUiLayerCommands();
				auto* borrowed = target == 1 ? old : next;
				const auto layer = target == 1 ? oldLayer : nextLayer;
				auto* root = borrowed->GetUiLayer(layer);
				const auto focus = root->GetFocusId();
				assert(focus && !root->IsBusy());
				const auto* registered = std::as_const(*root).Find(focus);
				assert(registered && !root->IsBusy()); // 무장 전 실제 등록/조회 양성
				auto& probe = entry == 0 || entry >= 3 ? incoming : (target == 1 ? oldState : nextState);
				if (entry == 1)
				{
					SInputEvent pointer;
					pointer.ScreenWidth = 132; pointer.ScreenHeight = 43; pointer.MouseButton = EMouseButton::Left;
					pointer.MouseX = 2; pointer.MouseY = 1; pointer.Type = EInputEventType::PointerDown;
					root->Dispatch(pointer);
					assert(root->GetCaptureId() == focus);
					pointer.Type = EInputEventType::PointerMove; pointer.MouseX = 3;
					root->Dispatch(pointer);
					assert(probe.DragBegins == 1 && root->GetCaptureId() == focus && probe.Hits == 0);
				}
				const auto queue = [&]()
				{
					if (progressRequest) assert(scenes.CommitProgress(prepared, active, candidate, diagnostic));
					else assert(scenes.SetScene(std::move(prepared)));
				};
				if (target != 1) queue();
				probe.Hook = entry == 0 ? 0 : entry >= 3 ? 2 : 1;
				incoming.Progress = target == 1 ? identity : nextState.Progress;
				incoming.ThrowAssign = entry == 4;
				probe.Callback = [&]()
				{
					const bool busy = root->IsBusy();
					if (target == 1) queue();
					if (action < 2) InvokeCancelAction(scenes, action == 1);
					const bool ended = scenes.EndFrame(diagnostic);
					std::cout << "TRACE ROOT-ENTRY entry=" << entry << " target=" << target << " progress=" << progressRequest
						<< " action=" << action << " hit=" << probe.Hits << " busy=" << busy << " ended=" << ended
						<< " old-destroyed=" << oldState.Destroyed << " next-destroyed=" << nextState.Destroyed << '\n';
					// RED는 정확한 callback/count를 남기고 해제된 root로 복귀하기 전에 고정 원인으로 끝낸다.
					if (!busy || ended || oldState.Destroyed || nextState.Destroyed)
					{
						std::cout << "FAIL CORE-ROOT-ENTRY-01 first virtual callback escaped root lifetime\n";
						std::exit(88);
					}
					assert(probe.Hits == 1 && root->IsBusy() && borrowed->GetUiLayer(layer) == root);
					assert(borrowed->GetProgress() == (target == 1 ? identity : nextState.Progress));
					assert(old->GetUiLayer(oldLayer) && old->GetProgress() == identity && active.get() == identity);
					assert(old->IsOperating() == (target == 2));
					assert(!scenes.TakeProgressResult(active, published, diagnostic));
					assert(nextState.Progress->GetGold() == 7654 && identity->GetGold() == 7654);
					if (entry == 1)
					{
						assert(root->GetCaptureId() == 0);
						root->CancelInput();
						SInputEvent keyEvent; keyEvent.Key = EInputKey::Enter;
						root->Dispatch(keyEvent);
						assert(probe.DragCancels == 0 && probe.PointerCancels == 0 && probe.KeyDeliveries == 0);
					}
				};
				probe.Armed = true;
				const auto operation = [&]()
				{
					if (entry == 0) assert(root->AddBase(std::make_unique<FRootEntryUi>(incoming)));
					else if (entry == 1) root->CancelInput();
					else if (entry == 2) assert(std::as_const(*root).Find(focus) == registered);
					else
					{
						auto value = std::make_unique<FRootEntryUi>(incoming);
						if (entry == 3) { auto id = focus; value->AssignIds(id); }
						try
						{
							assert(root->AddBase(std::move(value)) == 0 && entry == 3);
						}
						catch (const std::runtime_error& fault)
						{
							assert(entry == 4 && incoming.AssignThrows == 1 &&
								std::string(fault.what()) == "owned root entry AssignIds fault");
						}
					}
				};
				if (target == 2)
				{
					const auto trigger = old->OpenUiLayer(std::make_unique<FBoundaryUi>(nestedState));
					old->FlushUiLayerCommands();
					assert(trigger);
					nestedState.Callback = operation;
					nestedState.Armed = true;
					FFrameBuffer frame(132, 43);
					old->Render(frame);
					assert(nestedState.Hits == 1);
				}
				else operation();
				assert(probe.Hits == 1 && incoming.Destroyed == (entry >= 3 ? 1 : 0) && oldState.Destroyed == 0 && nextState.Destroyed == 0);
				assert(incoming.AssignThrows == (entry == 4 ? 1 : 0));
				if (entry == 1) assert(probe.DragCancels == 1 && probe.PointerCancels == 1 && root->GetCaptureId() == 0);
				assert(!root->IsBusy() && !old->IsOperating() && active.get() == identity);
				assert(!scenes.TakeProgressResult(active, published, diagnostic));
				if (action < 2)
				{
					InvokeCancelAction(scenes, action == 0); // 취소 latch를 idle 재취소로 앞당기지 않는다.
					assert(nextState.Destroyed == 0 && !scenes.TakeProgressResult(active, published, diagnostic));
				}
				assert(!root->IsBusy());
				assert(root->AddBase(std::make_unique<FUiButton>(SCellRect{15, 0, 12, 3}, L"after-query", []() {})));
				int destructorChecks = 0;
				const auto beforeCompletion = [&]()
				{
					++destructorChecks;
					assert(!scenes.TakeProgressResult(active, published, diagnostic));
				};
				oldState.Destroy = nextState.Destroy = incoming.Destroy = beforeCompletion;
				PublishTestFrame(scenes);
				assert(oldState.Destroyed == (action == 2 ? 1 : 0));
				assert(nextState.Destroyed == (action < 2 ? 1 : 0));
				assert(incoming.Destroyed == ((entry >= 3 || (entry == 0 && (target == 1 ? action == 2 : action < 2))) ? 1 : 0));
				if (progressRequest)
				{
					assert(scenes.TakeProgressResult(active, published, diagnostic) && published == (action == 2));
					assert(!scenes.TakeProgressResult(active, published, diagnostic));
				}
				if (action < 2 || !progressRequest)
				{
					std::vector<std::uint8_t> after;
					assert(active.get() == identity && identity->Serialize(after, error) && after == before);
				}
				assert(oldState.RootReads == oldState.Destroyed && nextState.RootReads == nextState.Destroyed);
				assert(incoming.RootReads == incoming.Destroyed);
				assert(destructorChecks == 1 + (entry == 0 && (target == 1 ? action == 2 : action < 2) ? 1 : 0));
				assert(ReadSaveBytes(saveRoot / "InGameData.sav") == cipher && ReadSaveBytes(saveRoot / "SaveData.key") == key);
				PublishTestFrame(scenes);
				assert(oldState.Destroyed == (action == 2 ? 1 : 0) && nextState.Destroyed == (action < 2 ? 1 : 0));
				assert(!scenes.TakeProgressResult(active, published, diagnostic));
				oldState.Destroy = nextState.Destroy = incoming.Destroy = {}; // 지역 관찰자가 종료 teardown까지 캡처되지 않는다.
			}
		}
	}
	std::cout << "PASS APP01 ROOT-ENTRY " << entry << " first virtual active pending nested EndFrame Cancel Escape lifetime result once root cipher key\n";
}

#ifdef _DEBUG
struct SRootEntryAllocationState final
{
	bool Armed = false;
	int Hits = 0;
};

static SRootEntryAllocationState& RootEntryAllocationState()
{
	static SRootEntryAllocationState state;
	return state;
}

static int __cdecl RootEntryAllocationHook(int type, void*, std::size_t size, int block, long, const unsigned char*, int)
{
	auto& state = RootEntryAllocationState();
	if (type == _HOOK_ALLOC && block == _NORMAL_BLOCK && size == sizeof(std::unique_ptr<IUI>) && state.Armed)
	{
		state.Armed = false;
		++state.Hits;
		return 0; // 빈 root의 첫 vector entry 저장소 할당만 결정적으로 실패시킨다.
	}
	return 1;
}
#endif

static void TestRootEntryControls()
{
	SRootEntryState registered, rejected, failed;
	FSceneUiRoot root({0, 0, 132, 43});
	assert(root.AddBase(nullptr) == 0 && !root.IsBusy());
	assert(std::as_const(root).Find(0) == nullptr && std::as_const(root).Find(999) == nullptr && !root.IsBusy());
	const auto id = root.AddBase(std::make_unique<FRootEntryUi>(registered));
	assert(id && root.GetFocusId() == id && !root.IsBusy());
	rejected.Hook = 2;
	rejected.Armed = true;
	rejected.Callback = [&]() { assert(root.IsBusy() && rejected.Hits == 1 && rejected.Destroyed == 1); };
	registered.Hook = 1;
	registered.Armed = true;
	registered.Callback = [&]()
	{
		assert(root.IsBusy());
		assert(root.AddBase(std::make_unique<FRootEntryUi>(rejected)) == 0);
		assert(rejected.Hits == 1 && rejected.GetterCalls == 0 && rejected.Destroyed == 1 && root.IsBusy());
	};
	assert(std::as_const(root).Find(id) && registered.Hits == 1 && !root.IsBusy());
	assert(root.AddBase(std::make_unique<FUiButton>(SCellRect{15, 0, 12, 3}, L"query-control", []() {})));
	assert(!root.IsBusy());
#ifdef _DEBUG
	FSceneUiRoot empty;
	failed.Hook = 2;
	failed.Armed = true;
	failed.Callback = [&]() { assert(empty.IsBusy() && failed.Hits == 1 && failed.Destroyed == 1); };
	auto value = std::make_unique<FRootEntryUi>(failed);
	auto& fault = RootEntryAllocationState();
	fault = {};
	fault.Armed = true;
	const auto previous = _CrtSetAllocHook(RootEntryAllocationHook);
	bool caught = false;
	try { (void)empty.AddBase(std::move(value)); }
	catch (const std::bad_alloc&) { caught = true; }
	(void)_CrtSetAllocHook(previous);
	fault.Armed = false;
	std::cout << "TRACE ROOT-ENTRY vector-registration-failure allocation-hit=" << fault.Hits
		<< " bad-alloc=" << caught << " destructor-hit=" << failed.Hits << " destroyed=" << failed.Destroyed << '\n';
	assert(caught && fault.Hits == 1 && failed.Hits == 1 && failed.Destroyed == 1 && !empty.IsBusy());
	assert(std::as_const(empty).Find(1) == nullptr);
	assert(empty.AddBase(std::make_unique<FUiButton>(SCellRect{0, 0, 12, 3}, L"allocation-recovery", []() {})));
#else
	std::cout << "SCOPE ROOT-ENTRY CRT allocation injection requires Debug; rejection and query controls still run\n";
#endif
	std::cout << "PASS APP01 ROOT-ENTRY null missing const query reentrant busy refusal no GetId destructor guard registration failure recovery\n";
}

static void TestBusyDataBoundary(GameData::FGameDataStore& store, bool sceneNvi)
{
	for (const bool pending : {true, false})
	{
		for (const bool progressRequest : {true, false})
		{
			for (int action = 2; action >= 0; --action) // pending direct-root EndFrame 반례를 먼저 실행한다.
			{
				for (const int stage : (sceneNvi ? std::vector<int>{3, 4, 5, 6, 7} : std::vector<int>{8, 0, 1, 2, 9, 10}))
				{
					std::unique_ptr<InGameData> active, candidate;
					SBoundaryUiState state;
					FGameUiBridge ui;
					FTextRPGSceneManager scenes(store);
					std::string error;
					std::wstring diagnostic;
					assert(scenes.LoadStartupScene(ui, diagnostic));
					PublishTestFrame(scenes);
					auto* old = ui.GetBoundScene();
					std::unique_ptr<FDataScene> prepared;
					if (progressRequest)
					{
						assert(InGameData::CreateNew(store, FindNewGameScene(store), candidate, error));
						assert(candidate->SetGold(7654, error));
						assert(scenes.PrepareSceneById(candidate->GetSceneId(), ui, candidate.get(), prepared, diagnostic));
						if (pending) state.Progress = candidate.get();
					}
					else assert(scenes.PrepareSceneById(kMainMenuSceneId, ui, nullptr, prepared, diagnostic));
					auto* borrowed = pending ? prepared.get() : old;
					const auto layer = borrowed->OpenUiLayer(std::make_unique<FBoundaryUi>(state));
					borrowed->FlushUiLayerCommands();
					auto* root = borrowed->GetUiLayer(layer);
					const auto focus = root->GetFocusId();
					assert(focus != 0 && !root->IsBusy());
					SInputEvent event;
					event.ScreenWidth = 132; event.ScreenHeight = 43;
					event.MouseButton = EMouseButton::Left;
					event.MouseX = 2; event.MouseY = 1;
					event.Key = EInputKey::Down;
					if (stage == 0 || stage == 6) root->SuspendInput();
					if (stage == 7 || stage == 10)
					{
						event.Type = EInputEventType::PointerDown;
						(void)borrowed->HandleInput(event);
						assert(root->GetCaptureId() == focus);
					}
					const auto queue = [&]()
					{
						if (progressRequest) assert(scenes.CommitProgress(prepared, active, candidate, diagnostic));
						else assert(scenes.SetScene(std::move(prepared)));
					};
					if (pending) queue();
					state.Callback = [&]()
					{
						if (!pending) queue();
						if (action < 2) InvokeCancelAction(scenes, action == 1);
						const bool nested = scenes.EndFrame(diagnostic);
						std::cout << "TRACE BOUNDARY-02 pending=" << pending << " progress=" << progressRequest
							<< " stage=" << stage << " action=" << action << " hit=" << state.Hits
							<< " nested=" << nested << " destroyed-inside=" << state.Destroyed << '\n';
						assert(!nested && diagnostic == L"EndFrame requires all Scene and UI operations to have returned.");
						assert(state.Hits == 1 && state.Destroyed == 0 && root->IsBusy());
						assert(borrowed->GetUiLayer(layer) == root && scenes.IsQuitting() == (action < 2));
						bool published = true;
						assert(!scenes.TakeProgressResult(active, published, diagnostic));
						if (state.Progress) assert(state.Progress->GetGold() == 7654);
					};
					state.Armed = true;
					if (stage == 0) root->ResumeInput();
					if (stage == 1) root->SuspendInput();
					if (stage == 2) assert(root->AddBase(std::make_unique<FUiButton>(SCellRect{15, 0, 12, 3}, L"added", []() {})));
					event.Type = EInputEventType::KeyDown;
					if (stage == 3) (void)borrowed->HandleInput(event);
					if (stage == 4) (void)borrowed->HandleInputFrame({event});
					if (stage == 5) { FFrameBuffer frame(132, 43); borrowed->Render(frame); }
					if (stage == 6) borrowed->OnEnter();
					if (stage == 7) borrowed->OnExit();
					if (stage == 8) { event.Key = EInputKey::Enter; root->Dispatch(event); }
					if (stage == 9) { FFrameBuffer frame(132, 43); root->Render(frame); }
					if (stage == 10) root->CancelInput();
					if (stage == 8)
					{
						std::cout << "TRACE BOUNDARY-01 direct-root remaining-deliveries=" << state.Deliveries << " action=" << action << '\n';
						assert(state.Deliveries == (action == 2 ? 1 : 0));
					}
					assert(state.Hits == 1 && state.Destroyed == 0 && !root->IsBusy() && !borrowed->IsOperating());
					// 직접 lifecycle 호출의 명시적 Bind/Clear 효과는 호출자가 되돌린다. 수명 검사와 구분한다.
					ui.Bind(*old);
					PublishTestFrame(scenes);
					assert(state.Destroyed == (pending ? (action < 2 ? 1 : 0) : (action == 2 ? 1 : 0)));
					if (progressRequest)
					{
						bool published = false;
						assert(scenes.TakeProgressResult(active, published, diagnostic) && published == (action == 2));
						assert(!scenes.TakeProgressResult(active, published, diagnostic));
						assert(static_cast<bool>(active) == (action == 2));
					}
					if (state.Progress && state.Destroyed) assert(state.RootReads == 1);
					PublishTestFrame(scenes);
				}
			}
		}
	}
	std::cout << "PASS APP01 BOUNDARY-02 " << (sceneNvi ? "data NVI Input Frame Render Enter Exit" : "direct root Resume Suspend AddBase Dispatch Render Cancel")
		<< " active pending SetScene CommitProgress Cancel Escape EndFrame root before views result once\n";
}

class FBoundaryCancelFault final : public std::runtime_error
{
public:
	explicit FBoundaryCancelFault(bool drag) : std::runtime_error(drag ? "owned drag cancel boundary fault" : "owned pointer cancel boundary fault") {}
};

struct SBoundaryCancelState final
{
	bool Armed = false;
	bool Drag = false;
	int PointerCancels = 0;
	int DragBegins = 0;
	int DragCancels = 0;
	int ThrowHits = 0;
	int ThrowOnCancel = 1;
	int Destroyed = 0;
	int Activations = 0;
	std::function<void()> PointerCanceled;
	FSceneUiRoot* Root = nullptr;
	FDataScene* Scene = nullptr;
	std::string Stage;
};

class FBoundaryCancelButton final : public FUiButton
{
public:
	explicit FBoundaryCancelButton(SBoundaryCancelState& state)
		: FUiButton({0, 0, 18, 3}, L"recovered", [&state]() { ++state.Activations; }), State(state) {}
	~FBoundaryCancelButton() override { ++State.Destroyed; }
	bool SupportsDrag() const noexcept override { return true; }
	EUiReply OnDrag(EUiDragPhase phase, const SInputEvent&) override
	{
		if (phase == EUiDragPhase::Begin) ++State.DragBegins;
		if (phase == EUiDragPhase::Cancel)
		{
			++State.DragCancels;
			if (State.Drag) Fault("OnDrag(Cancel)");
		}
		return EUiReply::Handled;
	}
	void OnPointerCancel() override
	{
		FUiButton::OnPointerCancel();
		++State.PointerCancels;
		const auto callback = State.PointerCanceled;
		if (callback) callback();
		if (!State.Drag && State.PointerCancels == State.ThrowOnCancel) Fault("OnPointerCancel");
	}
private:
	void Fault(const char* stage)
	{
		if (!State.Armed) return;
		State.Armed = false;
		++State.ThrowHits;
		State.Stage = stage;
		assert(State.Root->GetCaptureId() == 0 && State.Root->IsBusy() && State.Scene->IsOperating());
		throw FBoundaryCancelFault(State.Drag); // 고유 타입/문구/단계를 실제 사용자 virtual callback에서 던진다.
	}
	SBoundaryCancelState& State;
};

static void TestUiCancelFailureRecovery(GameData::FGameDataStore& store, const std::filesystem::path& work)
{
	for (const bool drag : {false, true})
	{
		for (const bool newGame : {true, false})
		{
			const auto saveRoot = std::filesystem::absolute(work / ("cancel-unwind-" + std::to_string(drag) + std::to_string(newGame)));
			SSaveRootScope environment(saveRoot);
			SBoundaryCancelState state;
			FGameSession session(store);
			FGameUiBridge ui;
			FTextRPGSceneManager scenes(store);
			std::string error;
			std::wstring diagnostic;
			const auto target = FindNewGameScene(store);
			assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
			PublishTestFrame(scenes);
			assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
			const auto* identity = session.GetProgress();
			auto* old = ui.GetBoundScene();
			std::vector<std::uint8_t> before;
			assert(identity && identity->Serialize(before, error));
			const auto cipher = ReadSaveBytes(saveRoot / "InGameData.sav");
			const auto key = ReadSaveBytes(saveRoot / "SaveData.key");
			assert(!cipher.empty() && !key.empty());
			state.Drag = drag;
			state.Scene = old;
			const auto layer = old->OpenUiLayer(std::make_unique<FBoundaryCancelButton>(state));
			old->FlushUiLayerCommands();
			auto* root = old->GetUiLayer(layer);
			state.Root = root;
			const auto focus = root->GetFocusId();
			assert(focus != 0);
			FFrameBuffer frame(132, 43);
			scenes.Render(frame);
			const auto cells = frame.GetCells();
			SInputEvent event;
			event.ScreenWidth = 132; event.ScreenHeight = 43;
			event.MouseButton = EMouseButton::Left;
			event.MouseX = 2; event.MouseY = 1;
			event.Type = EInputEventType::PointerDown;
			ui.HandleInput(event);
			assert(root->GetCaptureId() == focus);
			if (drag)
			{
				event.Type = EInputEventType::PointerMove;
				event.MouseX = 3;
				ui.HandleInput(event);
				assert(state.DragBegins == 1 && root->GetCaptureId() == focus);
			}
			assert(state.ThrowHits == 0 && state.Activations == 0);
			state.Armed = true;
			if (newGame) assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
			else assert(session.Continue(scenes, ui, error));
			const std::string expected = drag ? "owned drag cancel boundary fault" : "owned pointer cancel boundary fault";
			assert(!scenes.EndFrame(diagnostic) && diagnostic == std::wstring(expected.begin(), expected.end()));
			assert(session.TakeStartResult(scenes, error) == EGameStartResult::Failed && error == expected);
			assert(session.TakeStartResult(scenes, error) == EGameStartResult::None);
			std::cout << "TRACE BOUNDARY-03 drag=" << drag << " new-game=" << newGame << " throw-hit=" << state.ThrowHits
				<< " type=FBoundaryCancelFault stage=" << state.Stage << " bridge-old=" << (ui.GetBoundScene() == old)
				<< " root-busy=" << root->IsBusy() << " scene-operating=" << old->IsOperating() << '\n';
			assert(state.ThrowHits == 1 && state.Stage == (drag ? "OnDrag(Cancel)" : "OnPointerCancel"));
			assert(state.DragCancels == (drag ? 1 : 0) && state.PointerCancels == (drag ? 0 : 1));
			assert(session.GetProgress() == identity && ui.GetBoundScene() == old && ui.GetBoundRoot() == old->GetUiBaseRoot());
			assert(!root->IsBusy() && !old->IsOperating() && root->GetCaptureId() == 0 && root->GetFocusId() == focus);
			std::vector<std::uint8_t> after;
			assert(identity->Serialize(after, error) && after == before);
			assert(ReadSaveBytes(saveRoot / "InGameData.sav") == cipher && ReadSaveBytes(saveRoot / "SaveData.key") == key);
			// 실패한 취소는 성공으로 숨기지 않는다. 이미 비운 capture에는 중복 cancel을 보내지 않는다.
			event.Type = EInputEventType::PointerUp;
			ui.HandleInput(event);
			event.Type = EInputEventType::PointerDown;
			ui.HandleInput(event);
			assert(root->GetCaptureId() == focus);
			event.Type = EInputEventType::PointerUp;
			ui.HandleInput(event);
			assert(state.Activations == 1 && root->GetCaptureId() == 0);
			event.Type = EInputEventType::KeyUp; event.Key = EInputKey::Enter;
			ui.HandleInput(event);
			event.Type = EInputEventType::KeyDown;
			ui.HandleInput(event);
			assert(state.Activations == 2);
			scenes.Render(frame);
			assert(frame.GetCells() == cells && !root->IsBusy());
			assert(ReadSaveBytes(saveRoot / "InGameData.sav") == cipher && ReadSaveBytes(saveRoot / "SaveData.key") == key);
			assert(scenes.EndFrame(diagnostic));
			if (newGame) assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
			else assert(session.Continue(scenes, ui, error));
			PublishTestFrame(scenes);
			assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
			assert(session.GetProgress() != identity && ui.GetBoundScene() != old && state.ThrowHits == 1);
			assert(ReadSaveBytes(saveRoot / "SaveData.key") == key);
			if (!newGame) assert(ReadSaveBytes(saveRoot / "InGameData.sav") == cipher);
		}
	}
	std::cout << "PASS IGD BOUNDARY-03 actual capture drag cancellation typed throw old bridge busy unwind input render retry root cipher key preservation\n";
}

static void TestPreparedPublicationBoundary(GameData::FGameDataStore& store, const std::filesystem::path& work)
{
	bool allPreserved = true;
	for (int stage = 0; stage != 8; ++stage)
	{
		std::unique_ptr<InGameData> active, candidate;
		SBoundaryCancelState cancel;
		SRootEntryState retired;
		SEndFrameProbeState destroyed;
		FGameUiBridge bridge;
		FTextRPGSceneManager scenes(store);
		std::unique_ptr<FDataScene> prepared;
		std::string error;
		std::wstring diagnostic;
		const auto target = FindNewGameScene(store);
		assert(target != kMainMenuSceneId && store.FindScene(kMainMenuSceneId));
		assert(InGameData::CreateNew(store, target, candidate, error) && candidate->SetGold(7654, error));
		assert(scenes.PrepareSceneById(target, bridge, candidate.get(), prepared, diagnostic));
		assert(scenes.CommitProgress(prepared, active, candidate, diagnostic));
		PublishTestFrame(scenes);
		bool published = false;
		assert(scenes.TakeProgressResult(active, published, diagnostic) && published);
		const auto* identity = active.get();
		std::vector<std::uint8_t> before;
		assert(identity->Serialize(before, error));
		const auto saveRoot = work / ("prepared-publication-" + std::to_string(stage));
		assert(FSaveDataStore(saveRoot).Save(before, error));
		const auto cipher = ReadSaveBytes(saveRoot / "InGameData.sav");
		const auto key = ReadSaveBytes(saveRoot / "SaveData.key");
		assert(!cipher.empty() && !key.empty());
		const bool genericOld = stage == 2 || stage == 7;
		if (genericOld)
		{
			assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(destroyed)));
			PublishTestFrame(scenes);
		}
		auto* old = bridge.GetBoundScene();
		if (!genericOld)
		{
			cancel.Scene = old;
			const auto layer = old->OpenUiLayer(std::make_unique<FBoundaryCancelButton>(cancel));
			old->FlushUiLayerCommands();
			cancel.Root = old->GetUiLayer(layer);
			const auto sentinel = old->OpenUiLayer(std::make_unique<FRootEntryUi>(retired));
			old->FlushUiLayerCommands();
			assert(sentinel && retired.Destroyed == 0);
			// OnExit는 모든 실제 root를 취소한다. 하위 root의 직접 capture도 유효한 진입이다.
			SInputEvent pointer;
			pointer.ScreenWidth = 132; pointer.ScreenHeight = 43;
			pointer.MouseButton = EMouseButton::Left; pointer.MouseX = 2; pointer.MouseY = 1;
			pointer.Type = EInputEventType::PointerDown;
			cancel.Root->ResumeInput();
			cancel.Root->Dispatch(pointer);
			assert(cancel.Root->GetCaptureId());
		}
		FFrameBuffer oldFrame(132, 43);
		scenes.Render(oldFrame);
		assert(InGameData::CreateNew(store, target, candidate, error) && candidate->SetGold(4321, error));
		auto* nextProgress = candidate.get();
		assert(scenes.PrepareSceneById(target, bridge, nextProgress, prepared, diagnostic));
		auto* next = prepared.get();
		auto* base = next->GetUiBaseRoot();
		const auto layer = next->GetUiTopLayer();
		assert(base && next->GetUiLayerCount() == 1);
		FFrameBuffer expected(132, 43);
		next->Render(expected);
		int attempts = 0;
		bool reloaded = false;
		std::wstring reloadError;
		const auto mutate = [&]()
		{
			++attempts;
			if (stage == 3) reloaded = next->LoadFromGameData(kMainMenuSceneId, reloadError);
			else if (stage >= 4) assert(nextProgress->SetSceneId(store, kMainMenuSceneId, error));
			else
			{
				next->CloseUiLayer(next->GetUiTopLayer());
				next->FlushUiLayerCommands();
			}
			std::cout << "TRACE R1 mutation stage=" << stage << " attempt=" << attempts
				<< " layers=" << next->GetUiLayerCount() << " reload=" << reloaded
				<< " old-ui-destroyed=" << retired.Destroyed << " old-scene-destroyed=" << destroyed.Lifetime.Destroyed << '\n';
		};
		if (stage == 0 || stage == 5) cancel.PointerCanceled = mutate;
		if (stage == 1 || stage == 6) retired.Destroy = mutate;
		if (genericOld) destroyed.Destroy = mutate;
		assert(scenes.CommitProgress(prepared, active, candidate, diagnostic));
		if (stage == 3 || stage == 4) mutate();
		const bool ended = scenes.EndFrame(diagnostic);
		const auto publicationError = diagnostic;
		assert(scenes.TakeProgressResult(active, published, diagnostic));
		assert(!scenes.TakeProgressResult(active, published, diagnostic));
		assert(attempts == 1 && !scenes.HasPendingTransition() && !scenes.IsExecutingTransition());
		const bool invalidProgress = stage >= 4;
		bool preserved = ended == !invalidProgress && published == !invalidProgress;
		if (!invalidProgress)
		{
			preserved = preserved && active.get() == nextProgress && bridge.GetBoundScene() == next &&
				next->GetProgress() == active.get() && active->GetSceneId() == target &&
				next->GetUiLayerCount() == 1 && next->GetUiBaseRoot() == base && next->GetUiTopLayer() == layer;
			FFrameBuffer actual(132, 43);
			scenes.Render(actual);
			preserved = preserved && actual.GetCells() == expected.GetCells();
			if (stage == 3) preserved = preserved && !reloaded && !reloadError.empty();
			if (stage == 0) assert(cancel.PointerCancels == 1);
			if (stage == 1) assert(retired.Destroyed == 1);
			if (stage == 2) assert(destroyed.Lifetime.Destroyed == 1);
			if (preserved)
			{
				// 게시 뒤에만 준비 보호가 풀려 정상적인 직접 Layer 편집이 다시 동작한다.
				next->CloseUiLayer(layer);
				next->FlushUiLayerCommands();
				assert(next->GetUiLayerCount() == 0);
			}
		}
		else
		{
			std::vector<std::uint8_t> after;
			preserved = preserved && !publicationError.empty() && active.get() == identity &&
				identity->Serialize(after, error) && after == before;
			if (stage <= 5)
			{
				preserved = preserved && bridge.GetBoundScene() == old && retired.Destroyed == 0;
				if (bridge.GetBoundScene() == old)
				{
					assert(!old->IsBusy() && old->GetUiBaseRoot() == bridge.GetBoundRoot());
					FFrameBuffer actual(132, 43);
					scenes.Render(actual);
					preserved = preserved && actual.GetCells() == oldFrame.GetCells();
				}
			}
			else
			{
				// 이미 실행된 destructor는 복구했다고 주장하지 않는다. root/파일만 보존한다.
				preserved = preserved && bridge.GetBoundScene() == nullptr &&
					(genericOld ? destroyed.Lifetime.Destroyed == 1 : retired.Destroyed == 1);
			}
		}
		assert(ReadSaveBytes(saveRoot / "InGameData.sav") == cipher && ReadSaveBytes(saveRoot / "SaveData.key") == key);
		std::cout << "TRACE R1 outcome stage=" << stage << " ended=" << ended << " published=" << published
			<< " active-old=" << (active.get() == identity) << " bridge-old=" << (bridge.GetBoundScene() == old)
			<< " invariant=" << preserved << '\n';
		allPreserved = allPreserved && preserved;
		cancel.PointerCanceled = {};
		retired.Destroy = {};
		destroyed.Destroy = {};
	}
	assert(allPreserved && "R1 accepted candidate publication invariant failed");
	std::cout << "PASS IGD R1 accepted candidate close reload freeze OnExit retire destructor "
		"visible typed SceneId final validation old recovery cipher key\n";
}

static void TestSecondPassExitRecovery(GameData::FGameDataStore& store, const std::filesystem::path& work)
{
	for (const bool newerBinding : {false, true})
	{
		for (const bool newGame : {true, false})
		{
			const auto saveRoot = std::filesystem::absolute(work / ("second-pass-" +
				std::to_string(newerBinding) + std::to_string(newGame)));
			SSaveRootScope environment(saveRoot);
			SBoundaryCancelState first, second;
			FNeutralBridgeTarget newer;
			FGameSession session(store);
			FGameUiBridge bridge;
			FTextRPGSceneManager scenes(store);
			std::string error;
			std::wstring diagnostic;
			const auto target = FindNewGameScene(store);
			assert(session.NewGame(target, scenes, bridge, error) == EGameStartResult::Queued);
			PublishTestFrame(scenes);
			assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
			const auto* identity = session.GetProgress();
			auto* old = bridge.GetBoundScene();
			std::vector<std::uint8_t> before;
			assert(identity && identity->Serialize(before, error));
			const auto cipher = ReadSaveBytes(saveRoot / "InGameData.sav");
			const auto key = ReadSaveBytes(saveRoot / "SaveData.key");
			assert(!cipher.empty() && !key.empty());
			const auto a = old->OpenUiLayer(std::make_unique<FBoundaryCancelButton>(first));
			const auto b = old->OpenUiLayer(std::make_unique<FBoundaryCancelButton>(second));
			old->FlushUiLayerCommands();
			first.Scene = second.Scene = old;
			first.Root = old->GetUiLayer(a);
			second.Root = old->GetUiLayer(b);
			first.ThrowOnCancel = 2;
			FFrameBuffer frame(132, 43);
			scenes.Render(frame);
			const auto cells = frame.GetCells();
			SInputEvent pointer;
			pointer.ScreenWidth = 132; pointer.ScreenHeight = 43;
			pointer.MouseButton = EMouseButton::Left; pointer.MouseX = 2; pointer.MouseY = 1;
			pointer.Type = EInputEventType::PointerDown;
			first.Root->ResumeInput();
			first.Root->Dispatch(pointer);
			second.Root->Dispatch(pointer);
			assert(first.Root->GetCaptureId() && second.Root->GetCaptureId());
			int recaptures = 0;
			second.PointerCanceled = [&]()
			{
				assert(first.PointerCancels == 1 && first.ThrowHits == 0 && first.Root->GetCaptureId() == 0);
				first.Root->ResumeInput();
				first.Root->Dispatch(pointer);
				assert(first.Root->GetCaptureId() == first.Root->GetFocusId());
				++recaptures;
				if (newerBinding) static_cast<FUIManager&>(bridge).Bind(newer);
			};
			first.Armed = true;
			if (newGame) assert(session.NewGame(target, scenes, bridge, error) == EGameStartResult::Queued);
			else assert(session.Continue(scenes, bridge, error));
			assert(!scenes.EndFrame(diagnostic) && diagnostic == L"owned pointer cancel boundary fault");
			assert(session.TakeStartResult(scenes, error) == EGameStartResult::Failed && error == "owned pointer cancel boundary fault");
			assert(session.TakeStartResult(scenes, error) == EGameStartResult::None);
			std::cout << "TRACE R2 second-pass new-game=" << newGame << " newer-binding=" << newerBinding
				<< " A-cancels=" << first.PointerCancels << " B-cancels=" << second.PointerCancels
				<< " recaptures=" << recaptures << " throw-hit=" << first.ThrowHits
				<< " bridge-old=" << (bridge.GetBoundScene() == old) << " A-busy=" << first.Root->IsBusy()
				<< " scene-operating=" << old->IsOperating() << '\n';
			assert(first.PointerCancels == 2 && second.PointerCancels == 1 && recaptures == 1);
			assert(first.ThrowHits == 1 && first.Stage == "OnPointerCancel" && second.ThrowHits == 0);
			assert(session.GetProgress() == identity && first.Destroyed == 0 && second.Destroyed == 0);
			assert(!old->IsBusy() && !first.Root->IsBusy() && !second.Root->IsBusy());
			assert(first.Root->GetCaptureId() == 0 && second.Root->GetCaptureId() == 0);
			assert(!scenes.HasPendingTransition() && !scenes.IsExecutingTransition() && !scenes.IsQuitting());
			if (newerBinding)
			{
				assert(bridge.GetBoundTarget() == &newer && bridge.GetBoundRoot() == &newer.Root);
				bridge.Bind(*old); // 새 연결 보존을 관찰한 뒤에만 명시적으로 정상 입력을 복구한다.
			}
			else assert(bridge.GetBoundScene() == old && bridge.GetBoundRoot() == old->GetUiBaseRoot());
			std::vector<std::uint8_t> after;
			assert(identity->Serialize(after, error) && after == before);
			assert(ReadSaveBytes(saveRoot / "InGameData.sav") == cipher && ReadSaveBytes(saveRoot / "SaveData.key") == key);
			second.PointerCanceled = {};
			// 직접 root, 직접 Scene, bridge, Manager의 정상 입력을 모두 실제 버튼으로 재검사한다.
			first.Root->Dispatch(pointer);
			pointer.Type = EInputEventType::PointerUp;
			first.Root->Dispatch(pointer);
			assert(first.Activations == 1);
			pointer.Type = EInputEventType::PointerDown;
			(void)old->HandleInput(pointer);
			pointer.Type = EInputEventType::PointerUp;
			(void)old->HandleInput(pointer);
			assert(second.Activations == 1);
			pointer.Type = EInputEventType::PointerDown;
			bridge.HandleInput(pointer);
			pointer.Type = EInputEventType::PointerUp;
			bridge.HandleInput(pointer);
			assert(second.Activations == 2);
			SInputEvent enter;
			enter.ScreenWidth = 132; enter.ScreenHeight = 43; enter.Key = EInputKey::Enter;
			enter.Type = EInputEventType::KeyUp;
			(void)scenes.HandleInputFrame({enter});
			enter.Type = EInputEventType::KeyDown;
			(void)scenes.HandleInput(enter);
			assert(second.Activations == 3 && old->FindUi(second.Root->GetFocusId()));
			scenes.Update(0);
			scenes.Render(frame);
			assert(frame.GetCells() == cells && !old->IsBusy());
			assert(ReadSaveBytes(saveRoot / "InGameData.sav") == cipher && ReadSaveBytes(saveRoot / "SaveData.key") == key);
			assert(scenes.EndFrame(diagnostic));
			if (newGame) assert(session.NewGame(target, scenes, bridge, error) == EGameStartResult::Queued);
			else assert(session.Continue(scenes, bridge, error));
			PublishTestFrame(scenes);
			assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
			assert(first.Destroyed == 1 && second.Destroyed == 1 && session.GetProgress() != identity);
			assert(bridge.GetBoundScene() && bridge.GetBoundScene()->GetProgress() == session.GetProgress());
			assert(ReadSaveBytes(saveRoot / "SaveData.key") == key);
			if (!newGame) assert(ReadSaveBytes(saveRoot / "InGameData.sav") == cipher);
		}
	}
	std::cout << "PASS IGD R2 second-pass exact A recapture fault old bridge flags "
		"direct managed input render retry cipher key newer binding\n";
}

static void TestDirectReloadFailureBoundary(GameData::FGameDataStore& store)
{
	for (const bool drag : {false, true})
	{
		SBoundaryCancelState state;
		SRootEntryState pending;
		FGameUiBridge bridge;
		FDataScene scene(store, bridge);
		std::wstring error;
		assert(scene.LoadFromGameData(kMainMenuSceneId, error));
		scene.OnEnter();
		auto* baseRoot = scene.GetUiBaseRoot();
		const auto layer = scene.OpenUiLayer(std::make_unique<FBoundaryCancelButton>(state));
		scene.FlushUiLayerCommands();
		state.Scene = &scene;
		state.Root = scene.GetUiLayer(layer);
		state.Drag = drag;
		FFrameBuffer frame(132, 43);
		scene.Render(frame);
		const auto before = frame.GetCells();
		SInputEvent pointer;
		pointer.ScreenWidth = 132; pointer.ScreenHeight = 43;
		pointer.MouseX = 2; pointer.MouseY = 1;
		pointer.MouseButton = EMouseButton::Left; pointer.Type = EInputEventType::PointerDown;
		bridge.HandleInput(pointer);
		assert(state.Root->GetCaptureId() != 0);
		if (drag)
		{
			pointer.Type = EInputEventType::PointerMove; pointer.MouseX = 3;
			bridge.HandleInput(pointer);
			assert(state.DragBegins == 1);
		}
		const auto queued = scene.OpenUiLayer(std::make_unique<FRootEntryUi>(pending));
		assert(queued && !scene.GetUiLayer(queued) && pending.Destroyed == 0);
		state.Armed = true;
		bool caught = false;
		try { (void)scene.LoadFromGameData(kMainMenuSceneId, error); }
		catch (const FBoundaryCancelFault& fault)
		{
			caught = true;
			assert(std::string(fault.what()) == (drag ? "owned drag cancel boundary fault" : "owned pointer cancel boundary fault"));
		}
		assert(caught && state.ThrowHits == 1 && state.Destroyed == 0 && pending.Destroyed == 0);
		assert(!scene.IsOperating() && !state.Root->IsBusy() && state.Root->GetCaptureId() == 0);
		assert(bridge.GetBoundScene() == &scene && bridge.GetBoundRoot() == baseRoot && scene.GetUiLayer(layer) == state.Root);
		scene.Render(frame);
		assert(frame.GetCells() == before && scene.GetUiLayerCount() == 2);
		scene.FlushUiLayerCommands();
		assert(scene.GetUiLayer(queued) && scene.GetUiLayerCount() == 3 && pending.Destroyed == 0);
		assert(scene.LoadFromGameData(kMainMenuSceneId, error));
		assert(state.Destroyed == 1 && pending.Destroyed == 1 && scene.GetUiLayerCount() == 1);
		assert(bridge.GetBoundScene() == &scene && bridge.GetBoundRoot() == scene.GetUiBaseRoot());
		FNeutralBridgeTarget neutral;
		static_cast<FUIManager&>(bridge).Bind(neutral);
		assert(scene.LoadFromGameData(kMainMenuSceneId, error));
		assert(bridge.GetBoundTarget() == &neutral && bridge.GetBoundScene() == nullptr);
		bridge.Clear();
		std::cout << "TRACE CORE-01 direct-reload drag=" << drag << " throw-hit=" << state.ThrowHits
			<< " preserved-old=1 preserved-pending=1 recovered=1 other-target-preserved=1\n";
	}
	std::cout << "PASS APP01 CORE-01 direct reload typed cancel throw rollback existing bridge old UI pending command render retry controls\n";
}

static void TestScenePublicCleanupControls(GameData::FGameDataStore& store)
{
	SRootEntryState removed, survivor, rejected;
	FGameUiBridge bridge;
	std::wstring error;
	auto scene = std::make_unique<FDataScene>(store, bridge);
	assert(scene->LoadFromGameData(kMainMenuSceneId, error));
	scene->OnEnter();
	auto* identity = scene.get();
	const auto lower = scene->OpenUiLayer(std::make_unique<FRootEntryUi>(removed));
	const auto upper = scene->OpenUiLayer(std::make_unique<FRootEntryUi>(survivor));
	scene->FlushUiLayerCommands();
	assert(lower && upper && scene->GetUiLayerCount() == 3);
	removed.Destroy = [&]()
	{
		const auto count = identity->GetUiLayerCount();
		std::cout << "TRACE CORE-01 close-destructor hit=" << removed.Destroyed << " layer-count=" << count
			<< " operating=" << identity->IsOperating() << '\n';
		if (count != 2)
		{
			std::cout << "FAIL CORE-01 close destructor observes erase-in-progress ownership\n";
			std::exit(91); // 손상된 순회 중 FindUi(null root)로 진행하지 않는다.
		}
		assert(identity->IsOperating() && identity->GetUiLayer(upper) && !identity->GetUiLayer(lower));
		assert(identity->FindUi((std::numeric_limits<FUiId>::max)()) == nullptr);
		assert(survivor.Destroyed == 0);
	};
	scene->CloseUiLayer(lower);
	scene->FlushUiLayerCommands();
	assert(removed.Destroyed == 1 && survivor.Destroyed == 0 && !scene->IsOperating());
	survivor.Destroy = [&]()
	{
		assert(identity->IsOperating() && identity->GetUiLayerCount() == 0 && identity->GetUiBaseRoot() == nullptr);
		assert(identity->FindUi((std::numeric_limits<FUiId>::max)()) == nullptr);
		bridge.Bind(*identity);
		assert(!identity->LoadFromGameData(kMainMenuSceneId, error));
		rejected.Hook = 2; rejected.Armed = true;
		rejected.Callback = [&]() { assert(identity->IsOperating() && rejected.Destroyed == 1 && rejected.Hits == 1); };
		assert(identity->OpenUiLayer(std::make_unique<FRootEntryUi>(rejected)) == 0);
		assert(rejected.GetterCalls == 0 && rejected.Destroyed == 1 && rejected.Hits == 1);
		identity->CloseUiLayer(upper);
		identity->SetUiLayerVisible(upper, false);
		identity->DiscardUiLayerCommands();
		identity->FlushUiLayerCommands();
		identity->CancelUiInput();
	};
	scene.reset();
	assert(survivor.Destroyed == 1 && rejected.Destroyed == 1 && rejected.Hits == 1);
	assert(bridge.GetBoundTarget() == nullptr && bridge.GetBoundScene() == nullptr);
	std::cout << "PASS APP01 CORE-01 close destructor stable query retired refusal incoming cleanup final detach teardown controls\n";
}

static void TestUiCancelShutdown(GameData::FGameDataStore& store)
{
	for (const bool managerOwned : {true, false})
	{
		SBoundaryCancelState state;
		FGameUiBridge ui;
		auto scenes = std::make_unique<FTextRPGSceneManager>(store);
		std::unique_ptr<FDataScene> standalone;
		std::wstring error;
		if (managerOwned)
		{
			assert(scenes->LoadStartupScene(ui, error));
			PublishTestFrame(*scenes);
		}
		else
		{
			assert(scenes->PrepareSceneById(kMainMenuSceneId, ui, nullptr, standalone, error));
			standalone->OnEnter();
		}
		auto* old = ui.GetBoundScene();
		const auto layer = old->OpenUiLayer(std::make_unique<FBoundaryCancelButton>(state));
		old->FlushUiLayerCommands();
		state.Scene = old;
		state.Root = old->GetUiLayer(layer);
		SInputEvent event;
		event.ScreenWidth = 132; event.ScreenHeight = 43;
		event.Type = EInputEventType::PointerDown; event.MouseButton = EMouseButton::Left;
		event.MouseX = 2; event.MouseY = 1;
		ui.HandleInput(event);
		assert(state.Root->GetCaptureId() != 0);
		state.Armed = true;
		if (managerOwned) { scenes->CancelPendingTransitions(); scenes.reset(); }
		else standalone.reset();
		std::cout << "TRACE BOUNDARY-03 shutdown manager=" << managerOwned << " throw-hit=" << state.ThrowHits
			<< " destroyed=" << state.Destroyed << " bridge-null=" << (ui.GetBoundScene() == nullptr) << '\n';
		assert(state.ThrowHits == 1 && state.Destroyed == 1 && ui.GetBoundScene() == nullptr && ui.GetBoundRoot() == nullptr);
	}
	std::cout << "PASS APP01 BOUNDARY-03 shutdown callback exception reported forced teardown unbinds old bridge once\n";
}

static void GuardCancelReentry(FTextRPGSceneManager& scenes, bool escape)
{
	__try { InvokeCancelAction(scenes, escape); }
	__except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		std::cout << "FAIL CANCEL-01 access violation inside exact candidate destructor cancel reentry\n";
		std::exit(87);
	}
}

static void TestCandidateCancelReentry(GameData::FGameDataStore& store)
{
	for (const bool escape : {false, true})
	{
		for (const bool queuedUi : {false, true})
		{
			std::unique_ptr<InGameData> active, candidate;
			SDestructorProbeState state;
			FGameUiBridge ui;
			FTextRPGSceneManager scenes(store);
			std::string error;
			std::wstring sceneError;
			assert(scenes.LoadStartupScene(ui, sceneError));
			PublishTestFrame(scenes);
			const auto* menu = ui.GetBoundScene();
			const auto target = FindNewGameScene(store);
			assert(InGameData::CreateNew(store, target, candidate, error));
			assert(candidate->SetGold(7654, error));
			const auto* view = candidate.get();
			std::unique_ptr<FDataScene> prepared;
			assert(scenes.PrepareSceneById(target, ui, view, prepared, sceneError));
			state.Callback = [&]()
			{
				std::cout << "TRACE CANCEL-01 escape=" << escape << " queued-ui=" << queuedUi << " destructor-hit=" << state.Hits << '\n';
				assert(view->GetGold() == 7654 && !active);
				GuardCancelReentry(scenes, escape);
				bool published = true;
				const bool early = scenes.TakeProgressResult(active, published, sceneError);
				std::cout << "TRACE CANCEL-01 after-reentry root-gold=" << view->GetGold() << " early-result=" << early << '\n';
				assert(view->GetGold() == 7654 && !early && state.Hits == 1 && state.Completed == 0);
			};
			(void)prepared->OpenUiLayer(std::make_unique<FDestructorProbeButton>(state));
			if (!queuedUi) prepared->FlushUiLayerCommands();
			assert(scenes.CommitProgress(prepared, active, candidate, sceneError));
			scenes.CancelPendingTransitions();
			bool published = true;
			assert(state.Hits == 1 && state.Completed == 1 && !active && ui.GetBoundScene() == menu);
			assert(scenes.TakeProgressResult(active, published, sceneError) && !published);
			assert(!scenes.TakeProgressResult(active, published, sceneError));
			scenes.CancelPendingTransitions();
			PublishTestFrame(scenes);
			assert(state.Hits == 1 && state.Completed == 1);
		}
	}
	// 파생 typed 멤버보다 base 소유 현재/후보 Scene을 먼저 drain하는 실제 게임 경로다.
	{
		std::unique_ptr<InGameData> active, candidate;
		SDestructorProbeState currentUi, pendingUi;
		FGameUiBridge ui;
		std::string error;
		std::wstring sceneError;
		const auto target = FindNewGameScene(store);
		const InGameData* retained = nullptr;
		bool published = false;
		{
			FTextRPGSceneManager scenes(store);
			std::unique_ptr<FDataScene> prepared;
			assert(InGameData::CreateNew(store, target, candidate, error));
			assert(candidate->SetGold(2468, error));
			assert(scenes.PrepareSceneById(target, ui, candidate.get(), prepared, sceneError));
			assert(scenes.CommitProgress(prepared, active, candidate, sceneError));
			PublishTestFrame(scenes);
			assert(scenes.TakeProgressResult(active, published, sceneError) && published);
			retained = active.get();
			currentUi.Callback = [&]
			{
				assert(retained->GetGold() == 2468 && scenes.IsQuitting() && pendingUi.Completed == 1);
				assert(!scenes.EndFrame(sceneError));
			};
			(void)ui.GetBoundScene()->OpenUiLayer(std::make_unique<FDestructorProbeButton>(currentUi));
			ui.GetBoundScene()->FlushUiLayerCommands();
			assert(InGameData::CreateNew(store, target, candidate, error));
			assert(candidate->SetGold(8642, error));
			const auto* view = candidate.get();
			assert(scenes.PrepareSceneById(target, ui, view, prepared, sceneError));
			pendingUi.Callback = [&, view]
			{
				assert(view->GetGold() == 8642 && retained->GetGold() == 2468 && scenes.IsQuitting());
				assert(!scenes.TakeProgressResult(active, published, sceneError));
				scenes.CancelPendingTransitions();
			};
			(void)prepared->OpenUiLayer(std::make_unique<FDestructorProbeButton>(pendingUi));
			prepared->FlushUiLayerCommands();
			assert(scenes.CommitProgress(prepared, active, candidate, sceneError));
		}
		assert(currentUi.Hits == 1 && currentUi.Completed == 1 && pendingUi.Hits == 1 && pendingUi.Completed == 1);
		assert(active.get() == retained && active->GetGold() == 2468 && !ui.GetBoundScene());
	}
	std::cout << "PASS IGD CANCEL-01 candidate UI cancel Escape reentry exact teardown root alive result once after destructor\n";
	std::cout << "PASS IGD SCM-04 derived Manager teardown drains candidate UI before typed root and current UI before active root\n";
}

template <typename TManager>
static bool ReadShutdown(const TManager& scenes)
{
	if constexpr (requires { scenes.IsQuitting(); }) return scenes.IsQuitting();
	else return false;
}

static void TestDestructorQuit(GameData::FGameDataStore& store, const std::filesystem::path& work)
{
	bool allPreserved = true;
	for (const bool generic : {false, true})
	{
		for (const bool escape : {false, true})
		{
			for (const bool newGame : {true, false})
			{
				const auto root = std::filesystem::absolute(work / ("destructor-quit-" + std::to_string(generic) +
					std::to_string(escape) + std::to_string(newGame)));
				SSaveRootScope environment(root);
				SEndFrameProbeState old;
				SDestructorProbeState uiState;
				FGameSession session(store);
				FGameUiBridge ui;
				FTextRPGSceneManager scenes(store);
				std::string error;
				std::wstring sceneError;
				const auto target = FindNewGameScene(store);
				assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
				PublishTestFrame(scenes);
				assert(session.TakeStartResult(scenes, error) == EGameStartResult::Started);
				const auto* identity = session.GetProgress();
				const auto gold = identity->GetGold();
				std::vector<std::uint8_t> before;
				assert(identity->Serialize(before, error));
				const auto cipher = ReadSaveBytes(root / "InGameData.sav");
				const auto key = ReadSaveBytes(root / "SaveData.key");
				assert(!cipher.empty() && !key.empty());
				int quitHits = 0;
				const auto quit = [&]()
				{
					++quitHits;
					assert(identity == session.GetProgress() && identity->GetGold() == gold);
					std::cout << "TRACE QUIT-01 generic=" << generic << " escape=" << escape << " new-game=" << newGame
						<< " destructor-quit-hit=" << quitHits << '\n';
					InvokeCancelAction(scenes, escape);
					assert(identity == session.GetProgress() && identity->GetGold() == gold);
				};
				if (generic)
				{
					assert(scenes.SetScene(std::make_unique<FEndFrameProbe>(old)));
					PublishTestFrame(scenes);
					old.Destroy = quit;
				}
				else
				{
					uiState.Callback = quit;
					auto* bound = ui.GetBoundScene();
					(void)bound->OpenUiLayer(std::make_unique<FDestructorProbeButton>(uiState));
					bound->FlushUiLayerCommands();
				}
				if (newGame) assert(session.NewGame(target, scenes, ui, error) == EGameStartResult::Queued);
				else assert(session.Continue(scenes, ui, error));
				const bool ended = scenes.EndFrame(sceneError);
				const auto result = session.TakeStartResult(scenes, error);
				std::vector<std::uint8_t> after;
				assert(session.GetProgress() && session.GetProgress()->Serialize(after, error));
				const bool rootPreserved = session.GetProgress() == identity && before == after;
				const bool cipherPreserved = ReadSaveBytes(root / "InGameData.sav") == cipher;
				const bool keyPreserved = ReadSaveBytes(root / "SaveData.key") == key;
				std::cout << "TRACE QUIT-01 ended=" << ended << " start-result=" << static_cast<int>(result)
					<< " root-preserved=" << rootPreserved << " cipher-preserved=" << cipherPreserved
					<< " key-preserved=" << keyPreserved << " shutdown=" << ReadShutdown(scenes) << '\n';
				allPreserved = allPreserved && quitHits == 1 && !ended && result == EGameStartResult::Failed &&
					rootPreserved && cipherPreserved && keyPreserved && ReadShutdown(scenes) && ui.GetBoundScene() == nullptr &&
					(generic ? old.Lifetime.Destroyed == 1 : uiState.Hits == 1 && uiState.Completed == 1);
				assert(session.TakeStartResult(scenes, error) == EGameStartResult::None);
			}
		}
	}
	if (!allPreserved) throw std::runtime_error("QUIT-01 destructor Quit published root or started/autosaved canceled progress");
	std::cout << "PASS IGD QUIT-01 old UI generic destructor Quit NewGame Continue root cipher key preserved no Started autosave shutdown exposed\n";
}

static void TestProgressCommitBoundary(GameData::FGameDataStore& store)
{
	std::unique_ptr<InGameData> active;
	int oldViewDestroyed = 0;
	FGameUiBridge ui;
	FTextRPGSceneManager scenes(store);
	std::string error;
	std::wstring sceneError;
	bool published = false;
	const auto target = FindNewGameScene(store);
	assert(scenes.LoadStartupScene(ui, sceneError));
	PublishTestFrame(scenes);
	std::unique_ptr<InGameData> candidate;
	assert(InGameData::CreateNew(store, target, candidate, error));
	std::unique_ptr<FDataScene> prepared;
	const auto* menu = ui.GetBoundScene();
	assert(scenes.PrepareSceneById(target, ui, candidate.get(), prepared, sceneError));
	assert(ui.GetBoundScene() == menu && active == nullptr);
	assert(scenes.CommitProgress(prepared, active, candidate, sceneError));
	assert(!active && !prepared && !candidate && ui.GetBoundScene() == menu);
	PublishTestFrame(scenes);
	assert(scenes.TakeProgressResult(active, published, sceneError) && published);
	assert(active && ui.GetBoundScene()->GetProgress() == active.get());
	const auto* identity = active.get();
	assert(active->SetGold(9876, error) && active->SetCurrentHp(store, 5, error));
	FFrameBuffer frame(132, 43);
	scenes.Render(frame);
	FFrameBuffer expected(38, 1);
	expected.DrawText(0, 0, GetLocalizedText(store, kGoldTextKey) + L": 9876");
	assert(std::equal(expected.GetCells().begin(), expected.GetCells().end(), frame.GetCells().begin() + 132 + 45));
	assert(scenes.LoadSceneById(target, ui, sceneError));
	PublishTestFrame(scenes);
	assert(active.get() == identity && ui.GetBoundScene()->GetProgress() == identity);
	assert(InGameData::CreateNew(store, target, candidate, error));
	assert(scenes.PrepareSceneById(target, ui, candidate.get(), prepared, sceneError));
	const auto* preparedIdentity = prepared.get();
	assert(!scenes.PrepareSceneById(kMissingDataId, ui, candidate.get(), prepared, sceneError));
	assert(prepared.get() == preparedIdentity && active.get() == identity);
	auto* bound = ui.GetBoundScene();
	(void)bound->OpenUiLayer(std::make_unique<FRootLifetimeButton>(*identity, oldViewDestroyed));
	bound->FlushUiLayerCommands();
	int acceptedInsideCallback = 0;
	(void)bound->OpenUiLayer(std::make_unique<FUiButton>(SCellRect{1, 1, 20, 3}, L"commit", [&]()
	{
		assert(scenes.CommitProgress(prepared, active, candidate, sceneError));
		assert(active.get() == identity && ui.GetBoundScene() == bound && !prepared && !candidate);
		assert(!scenes.EndFrame(sceneError));
		assert(active.get() == identity && oldViewDestroyed == 0);
		++acceptedInsideCallback;
	}));
	bound->FlushUiLayerCommands();
	SInputEvent enter;
	enter.ScreenWidth = 132;
	enter.ScreenHeight = 43;
	enter.Key = EInputKey::Enter;
	enter.Type = EInputEventType::KeyDown;
	enter.BatchId = 931;
	(void)scenes.HandleInputFrame({enter});
	assert(acceptedInsideCallback == 1 && oldViewDestroyed == 0);
	assert(active.get() == identity && ui.GetBoundScene() == bound);
	scenes.Update(0);
	scenes.Render(frame);
	assert(active.get() == identity && oldViewDestroyed == 0);
	PublishTestFrame(scenes);
	assert(scenes.TakeProgressResult(active, published, sceneError) && published);
	assert(active.get() != identity && ui.GetBoundScene()->GetProgress() == active.get());
	assert(oldViewDestroyed == 1);
	std::cout << "PASS IGD candidate prepare unpublished callback queues atomic root Scene commit EndFrame only old root outlives view teardown\n";
}

int main(int argc, char** argv)
try
{
	_set_error_mode(_OUT_TO_STDERR);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_set_abort_behavior(0, _CALL_REPORTFAULT | _WRITE_ABORT_MSG);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	std::cout << std::unitbuf;
	if (argc == 4 && (std::string(argv[1]) == "--prepare-save" || std::string(argv[1]) == "--inspect-save"))
	{
		return ProgressFixtureMode(argv[1], std::filesystem::path(argv[2]), std::filesystem::path(argv[3]));
	}
	if (argc == 5 && std::string(argv[1]) == "--root-entry-tests")
	{
		auto store = std::make_unique<GameData::FGameDataStore>();
		LoadRuntimeFixture(*store, std::filesystem::path(argv[3]), std::filesystem::path(argv[4]));
		const std::string entry(argv[2]);
		assert(entry == "getid" || entry == "cancel-find" || entry == "find" || entry == "incoming");
		if (entry == "incoming")
		{
			TestSceneOperationBoundary(*store);
			TestGameUiBridgeTarget(*store);
			TestDirectReloadFailureBoundary(*store);
			TestScenePublicCleanupControls(*store);
			TestRootEntryControls();
			TestRootEntryBoundary(*store, std::filesystem::path(argv[4]), 3);
			TestRootEntryBoundary(*store, std::filesystem::path(argv[4]), 4);
		}
		else TestRootEntryBoundary(*store, std::filesystem::path(argv[4]), entry == "getid" ? 0 : entry == "cancel-find" ? 1 : 2);
		return 0;
	}
	if (argc == 5 && std::string(argv[1]) == "--boundary-tests")
	{
		if (std::string(argv[2]) == "01") TestBusySceneBoundary();
		else
		{
			auto store = std::make_unique<GameData::FGameDataStore>();
			LoadRuntimeFixture(*store, std::filesystem::path(argv[3]), std::filesystem::path(argv[4]));
			if (std::string(argv[2]) == "02")
			{
				TestPreparedPublicationBoundary(*store, std::filesystem::path(argv[4]));
				TestSceneOperationBoundary(*store);
				TestGameUiBridgeTarget(*store);
				TestDirectReloadFailureBoundary(*store);
				TestScenePublicCleanupControls(*store);
				TestRootEntryControls();
				for (int entry = 0; entry != 5; ++entry) TestRootEntryBoundary(*store, std::filesystem::path(argv[4]), entry);
				TestBusyDataBoundary(*store, false);
				TestBusyDataBoundary(*store, true);
			}
			else
			{
				assert(std::string(argv[2]) == "03");
				TestSecondPassExitRecovery(*store, std::filesystem::path(argv[4]));
				TestUiCancelFailureRecovery(*store, std::filesystem::path(argv[4]));
				TestUiCancelShutdown(*store);
			}
		}
		return 0;
	}
	if (argc == 4 && std::string(argv[1]) == "--progress-tests")
	{
		const std::filesystem::path work(argv[3]);
		std::filesystem::create_directories(work);
		auto ownedStore = std::make_unique<GameData::FGameDataStore>();
		auto& store = *ownedStore;
		LoadRuntimeFixture(store, std::filesystem::path(argv[2]), work);
		std::cout << "MODE progress-only\n";
		TestSceneOperationBoundary(store);
		TestGameUiBridgeTarget(store);
		TestDirectReloadFailureBoundary(store);
		TestScenePublicCleanupControls(store);
		TestRootEntryControls();
		for (int entry = 0; entry != 5; ++entry) TestRootEntryBoundary(store, work, entry);
		TestBusySceneBoundary();
		TestBusyDataBoundary(store, false);
		TestBusyDataBoundary(store, true);
		TestPreparedPublicationBoundary(store, work);
		TestSecondPassExitRecovery(store, work);
		TestUiCancelFailureRecovery(store, work);
		TestUiCancelShutdown(store);
		TestGenericActivationFailure();
		TestGenericDirectLifetime();
		TestGenericLifecycleAndCandidate();
		TestGenericFrameInvalidation();
		TestEndFrameGenericStages();
		TestEndFrameCallbackLifetime(store);
		TestCombatSession(store, work);
		TestProgressSession(store, work);
		TestLegacyProgressSession(store, work);
		TestNewerProgressSlotProtection(store, work);
		TestCandidateCancelReentry(store);
		TestDestructorQuit(store, work);
		TestProgressCommitBoundary(store);
		TestProgressPublicationFailure(store, work);
		return 0;
	}
	TestFocusOwnerLifetime();
	TestRootEntryControls();
	TestFocusMutationDelivery();
	TestSceneFrameRoute();
	TestPointerTopChangeGate();
	TestLogicalAliasReleaseRouting();
	TestStrictTopMatrix();
	TestPhysicalTrackingDuringTopChange();
	TestSecondaryButtonDoesNotConfirm();
	TestFocusResetGate();
	TestWithinLayerRouting();
	TestSceneGestureMutation();
	TestFrameBoundary();
	TestRestoreChain();
	TestReleaseMutation();
	TestGestureCancellation();
	TestClippedFocus();
	TestExtremeGeometry();
	assert(argc == 3 || argc == 4);
	// C6262: fixture 진입점의 긴 UI 검사는 Store를 힙에서 단독 소유한다.
	auto ownedStore = std::make_unique<GameData::FGameDataStore>();
	auto& store = *ownedStore;
	LoadRuntimeFixture(store, std::filesystem::path(argv[1]), std::filesystem::path(argv[2]), true, argc == 4 ? std::filesystem::path(argv[3]) : std::filesystem::path{});
	TestSceneOperationBoundary(store);
	TestGameUiBridgeTarget(store);
	TestDirectReloadFailureBoundary(store);
	TestScenePublicCleanupControls(store);
	TestBusySceneBoundary();
	TestUiCancelShutdown(store);
	TestGenericActivationFailure();
	TestGenericDirectLifetime();
	TestGenericLifecycleAndCandidate();
	TestGenericFrameInvalidation();
	TestEndFrameGenericStages();
	TestEndFrameCallbackLifetime(store);
	// 일반 legacy --yaml-root는 신규 cohort가 없어도 기존 Scene/UI 경로를 모두 실행한다.
	// 기본 full catalog와 명시 --progress-tests는 진행 검사를 생략하지 않는다.
	const bool hasProgressCatalog = argc == 3 || store.GetDefaultNewGameProfile() != nullptr;
	std::cout << (hasProgressCatalog ? "MODE full-catalog-scene-ui\n" : "MODE legacy-scene-ui\n");
	if (hasProgressCatalog)
	{
		for (int entry = 0; entry != 5; ++entry) TestRootEntryBoundary(store, std::filesystem::path(argv[2]), entry);
		TestBusyDataBoundary(store, false);
		TestBusyDataBoundary(store, true);
		TestPreparedPublicationBoundary(store, std::filesystem::path(argv[2]));
		TestSecondPassExitRecovery(store, std::filesystem::path(argv[2]));
		TestUiCancelFailureRecovery(store, std::filesystem::path(argv[2]));
		TestProgressSession(store, std::filesystem::path(argv[2]));
		TestLegacyProgressSession(store, std::filesystem::path(argv[2]));
		TestNewerProgressSlotProtection(store, std::filesystem::path(argv[2]));
		TestCandidateCancelReentry(store);
		TestDestructorQuit(store, std::filesystem::path(argv[2]));
		TestProgressCommitBoundary(store);
		TestProgressPublicationFailure(store, std::filesystem::path(argv[2]));
	}
	// INP-04: 옛 PassThrough 정책도 Scene 최상위 전용 정책으로 대체한다.
	{
		FGameUiBridge bridge;
		FDataScene strict(store, bridge);
		std::wstring error;
		assert(strict.LoadFromGameData(kMainMenuSceneId, error));
		(void)strict.OpenUiLayer(std::make_unique<FUiPanel>(SCellRect{0, 0, 1, 1}));
		strict.FlushUiLayerCommands();
		SInputEvent event;
		event.ScreenWidth = 132;
		event.ScreenHeight = 43;
		event.Key = EInputKey::Enter;
		assert(strict.HandleInput(event).Type == ESceneAction::None);
		std::cout << "PASS INP04 Scene empty top blocks old PassThrough Enter\n";
	}
	{
		FGameUiBridge bridge;
		FDataScene retained(store, bridge);
		std::wstring error;
		assert(retained.LoadFromGameData(kMainMenuSceneId, error));
		SInputEvent event;
		event.ScreenWidth = 132;
		event.ScreenHeight = 43;
		event.Key = EInputKey::Enter;
		event.BatchId = 101;
		assert(retained.HandleInputFrame({event}).Type == ESceneAction::StartGame);
		event.Type = EInputEventType::KeyUp;
		assert(retained.HandleInputFrame({event}).Type == ESceneAction::None);
		event.Type = EInputEventType::KeyDown;
		event.BatchId = 102;
		assert(retained.HandleInputFrame({event}).Type == ESceneAction::StartGame);
		std::cout << "PASS INP04 retained Scene after failed transition re-arms on blocked-batch release\n";
	}
	{
		FGameUiBridge bridge;
		FDataScene retained(store, bridge);
		std::wstring error;
		assert(retained.LoadFromGameData(kMainMenuSceneId, error));
		std::uint64_t layer = 0;
		int callbacks = 0;
		layer = retained.OpenUiLayer(std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"reentrant", [&]()
		{
			++callbacks;
			assert(!retained.LoadFromGameData(kTownSceneId, error));
			assert(retained.GetUiLayer(layer) != nullptr && !error.empty());
		}));
		retained.FlushUiLayerCommands();
		SInputEvent event;
		event.Key = EInputKey::Enter;
		event.ScreenWidth = 132;
		event.ScreenHeight = 43;
		(void)retained.HandleInput(event);
		assert(callbacks == 1 && retained.GetUiTopLayer() == layer);
		std::cout << "PASS INP04 reentrant Scene preparation cannot destroy dispatch owner\n";
	}
	TestNoLegacyTextLayout(store, std::filesystem::path(argv[1]), std::filesystem::path(argv[2]));
	TestBoundedRendering(store, std::filesystem::path(argv[1]));
	TestExtremeLocalCoordinates();
	FFrameBuffer frame(20, 10);
	FUiPanel panel({1, 1, 10, 6});
	panel.SetTitle(L"전투필드");
	panel.Render({frame, 0, 0, {0, 0, 20, 10}});
	const auto cell = [&frame](int x, int y) { return frame.GetCells()[static_cast<std::size_t>(y * 20 + x)]; };
	assert(cell(1, 1) == L'┌' && cell(2, 1) == L'─' && cell(10, 1) == L'┐');
	assert(cell(3, 1) == L'전' && cell(7, 1) == L'필' && cell(9, 1) == L'─');
	panel.SetTitle(L"A");
	panel.Render({frame, 0, 0, {0, 0, 20, 10}});
	assert(cell(3, 1) == L'A' && cell(4, 1) == L'─' && cell(7, 1) == L'─');
	panel.SetTitle(L"");
	panel.Render({frame, 0, 0, {0, 0, 20, 10}});
	assert(cell(3, 1) == L'─');
	assert(GetLocalizedText(store, kNewGameTextKey, true) == L"New Game");
	panel.SetTitle(GetLocalizedText(store, kMainMenuTitleTextKey));
	panel.Render({frame, 0, 0, {0, 0, 20, 10}});
	assert(cell(3, 1) == L'메' && cell(5, 1) == L'인');
	std::cout << "PASS titled border CJK clipping prefix shrink empty\n";
	FLayerFixture root;
	auto parent = std::make_unique<FUiPanel>(SCellRect{2, 2, 14, 7});
	int clicks = 0;
	auto button = std::make_unique<FUiButton>(SCellRect{1, 1, 8, 3}, L"fixture", [&clicks]() { ++clicks; });
	FUiButton* buttonPtr = button.get();
	parent->AddChild(std::move(button));
	root.AddBase(std::move(parent));
	frame.Clear();
	root.Render(frame);
	assert(cell(4, 4) == L'┌');
	SInputEvent event;
	event.ScreenWidth = 20;
	event.ScreenHeight = 10;
	event.MouseX = 5;
	event.MouseY = 5;
	event.Type = EInputEventType::PointerDown;
	root.Dispatch(event);
	assert(clicks == 0 && root.GetCaptureId() == buttonPtr->GetId());
	event.Type = EInputEventType::PointerUp;
	root.Dispatch(event);
	assert(clicks == 1 && root.GetCaptureId() == 0);
	buttonPtr->SetEnabled(false);
	event.Type = EInputEventType::PointerDown;
	root.Dispatch(event);
	event.Type = EInputEventType::PointerUp;
	root.Dispatch(event);
	assert(clicks == 1);
	std::cout << "PASS child relative coordinates release click disabled\n";
	buttonPtr->SetEnabled(true);
	const FUiId baseFocus = buttonPtr->GetId();
	const auto makePopup = []()
	{
		auto popup = std::make_unique<FUiPanel>(SCellRect{0, 0, 12, 8});
		popup->AddChild(std::make_unique<FUiButton>(SCellRect{1, 1, 8, 3}, L"popup", []() {}));
		return popup;
	};
	const FUiId popupA = root.OpenPopup(makePopup());
	root.FlushCommands();
	const FUiId focusA = root.GetFocusId();
	assert(focusA != baseFocus && root.Find(popupA));
	const FUiId popupB = root.OpenPopup(makePopup());
	root.FlushCommands();
	const FUiId focusB = root.GetFocusId();
	root.ClosePopup(popupA);
	root.FlushCommands();
	assert(root.GetFocusId() == focusB && root.Find(popupA) == nullptr);
	root.ClosePopup(popupB);
	root.FlushCommands();
	assert(root.GetFocusId() == baseFocus);
	const FUiId tooltip = root.OpenPopup(makePopup());
	root.FlushCommands();
	assert(root.GetFocusId() != baseFocus);
	std::vector<FUiId> stacked;
	for (int index = 0; index < 80; ++index)
	{
		stacked.push_back(root.OpenPopup(makePopup()));
	}
	root.FlushCommands();
	assert(root.Find(baseFocus) == buttonPtr);
	for (auto id : stacked) root.ClosePopup(id);
	root.ClosePopup(tooltip);
	root.FlushCommands();
	// Empty modal surface and Unhandled never fall through to Base.
	const FUiId modal = root.OpenPopup(std::make_unique<FUiPanel>(SCellRect{12, 0, 7, 7}));
	root.FlushCommands();
	event.Type = EInputEventType::PointerDown;
	root.Dispatch(event);
	event.Type = EInputEventType::PointerUp;
	root.Dispatch(event);
	assert(clicks == 1);
	event.Type = EInputEventType::KeyDown;
	event.Key = EInputKey::Enter;
	root.Dispatch(event);
	assert(clicks == 1);
	root.ClosePopup(modal);
	root.FlushCommands();
	// Key held while modal closes cannot activate its newly exposed Base.
	root.Dispatch(event);
	assert(clicks == 1);
	event.Type = EInputEventType::KeyUp;
	root.Dispatch(event);
	event.Type = EInputEventType::KeyDown;
	root.Dispatch(event);
	assert(clicks == 2);
	root.Dispatch(event);
	assert(clicks == 2);
	event.Type = EInputEventType::KeyUp;
	root.Dispatch(event);
	// A callback requests its own deletion; destruction must follow callback return.
	FUiId selfId = 0;
	bool callbackReturned = false;
	auto self = std::make_unique<FUiButton>(SCellRect{0, 0, 10, 3}, L"self",
											[&]()
											{
												root.ClosePopup(selfId);
												assert(root.Find(selfId) != nullptr);
												callbackReturned = true;
											});
	selfId = root.OpenPopup(std::move(self));
	root.FlushCommands();
	event.Type = EInputEventType::KeyDown;
	event.BatchId = 11;
	root.Dispatch(event);
	assert(callbackReturned && root.Find(selfId) == nullptr);
	event.Type = EInputEventType::KeyUp;
	root.Dispatch(event);
	event.Type = EInputEventType::KeyDown;
	root.Dispatch(event);
	assert(clicks == 2);
	event.BatchId = 12;
	event.Type = EInputEventType::KeyUp;
	root.Dispatch(event);
	event.Type = EInputEventType::KeyDown;
	root.Dispatch(event);
	assert(clicks == 3);
	std::cout << "PASS nested popup middle close restore strict-top (old tooltip policy superseded) stable IDs deferred close batch release gates\n";
	FSceneUiRoot gestures;
	int dragClicks = 0;
	std::array<int, 4> phases{};
	auto drag = std::make_unique<FDragFixture>(dragClicks, phases);
	FDragFixture* dragPtr = drag.get();
	gestures.AddBase(std::move(drag));
	event = {};
	event.ScreenWidth = 20;
	event.ScreenHeight = 10;
	event.MouseX = 2;
	event.MouseY = 1;
	event.Type = EInputEventType::PointerDown;
	gestures.Dispatch(event);
	event.MouseX = 3;
	event.Type = EInputEventType::PointerMove;
	gestures.Dispatch(event);
	event.Type = EInputEventType::PointerUp;
	gestures.Dispatch(event);
	assert(dragClicks == 0 && phases[0] == 1 && phases[2] == 1);
	event.Type = EInputEventType::PointerDown;
	gestures.Dispatch(event);
	event.MouseX = 4;
	event.Type = EInputEventType::PointerMove;
	gestures.Dispatch(event);
	event.Type = EInputEventType::Resize;
	gestures.Dispatch(event);
	assert(phases[3] == 1 && gestures.GetCaptureId() == 0);
	event.Type = EInputEventType::PointerUp;
	gestures.Dispatch(event);
	event.Type = EInputEventType::PointerDown;
	gestures.Dispatch(event);
	dragPtr->SetEnabled(false);
	event.Type = EInputEventType::PointerMove;
	gestures.Dispatch(event);
	assert(gestures.GetCaptureId() == 0 && dragClicks == 0);
	std::cout << "PASS drag click exclusion resize disabled cancellation\n";
	assert(argc == 3 || argc == 4);

	FGameUiBridge bridge;
	FDataScene scene(store, bridge);
	std::wstring error;
	assert(scene.LoadFromGameData(kMainMenuSceneId, error));
	scene.OnEnter();
	assert(bridge.GetBoundRoot() == &scene.GetUiRoot());
	FFrameBuffer production(132, 43);
	scene.Render(production);
	const auto productionCell = [&production](int x, int y) { return production.GetCells()[static_cast<std::size_t>(y * 132 + x)]; };
	assert(productionCell(86, 0) == L'┌' && productionCell(131, 42) == L'┘');
	for (int index = 0; index < 4; ++index)
	{
		assert(productionCell(89, 4 + index * 9) == L'┌');
	}
	event = {};
	event.ScreenWidth = 132;
	event.ScreenHeight = 43;
	event.Type = EInputEventType::KeyDown;
	event.Key = EInputKey::Digit2;
	assert(scene.HandleInput(event).Type == ESceneAction::None);
	event.Key = EInputKey::Digit4;
	assert(scene.HandleInput(event).Type == ESceneAction::Quit);
	assert(scene.HandleInput(event).Type == ESceneAction::None);
	event.Type = EInputEventType::KeyUp;
	(void)scene.HandleInput(event);
	event.Type = EInputEventType::KeyDown;
	event.Key = EInputKey::Digit1;
	assert(scene.HandleInput(event).Type == ESceneAction::StartGame);
	scene.OnExit();
	assert(bridge.GetBoundRoot() == nullptr);
	std::cout << "PASS production Scene tree four buttons fixed numbers disabled bridge ownership\n";
	FConsoleInput adapter;
	INPUT_RECORD record{};
	record.EventType = KEY_EVENT;
	record.Event.KeyEvent.bKeyDown = TRUE;
	record.Event.KeyEvent.wVirtualKeyCode = '3';
	record.Event.KeyEvent.wRepeatCount = 5;
	assert(adapter.TranslateRecord(record, {10, 20}, 132, 43, event));
	assert(event.Type == EInputEventType::KeyDown && event.Key == EInputKey::Digit3 && event.RepeatCount == 5 && !event.IsRepeat);
	assert(adapter.TranslateRecord(record, {10, 20}, 132, 43, event) && event.IsRepeat);
	record.Event.KeyEvent.bKeyDown = FALSE;
	assert(adapter.TranslateRecord(record, {}, 132, 43, event) && event.Type == EInputEventType::KeyUp);
	record = {};
	record.EventType = MOUSE_EVENT;
	record.Event.MouseEvent.dwMousePosition = {12, 24};
	record.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
	assert(adapter.TranslateRecord(record, {10, 20}, 132, 43, event));
	assert(event.Type == EInputEventType::PointerDown && event.MouseX == 2 && event.MouseY == 4);
	record.Event.MouseEvent.dwButtonState = 0;
	assert(adapter.TranslateRecord(record, {10, 20}, 132, 43, event) && event.Type == EInputEventType::PointerUp);
	record.Event.MouseEvent.dwEventFlags = MOUSE_WHEELED;
	record.Event.MouseEvent.dwButtonState = static_cast<DWORD>(WHEEL_DELTA) << 16;
	assert(adapter.TranslateRecord(record, {}, 132, 43, event) && event.Type == EInputEventType::Wheel && event.WheelDelta == WHEEL_DELTA);
	record.EventType = FOCUS_EVENT;
	record.Event.FocusEvent.bSetFocus = FALSE;
	assert(adapter.TranslateRecord(record, {}, 132, 43, event) && event.Type == EInputEventType::FocusLost);
	std::cout << "PASS actual Win32 adapter key releases repeats digits pointer wheel focus normalization\n";
	// Prepare must not publish a bridge; failed preparation preserves the live tree.
	FGameUiBridge transitionBridge;
	FDataScene prepared(store, transitionBridge);
	assert(prepared.LoadFromGameData(kMainMenuSceneId, error));
	assert(transitionBridge.GetBoundRoot() == nullptr);
	prepared.OnEnter();
	FSceneUiRoot* retainedRoot = transitionBridge.GetBoundRoot();
	const auto retainedFocus = retainedRoot->GetFocusId();
	assert(!prepared.LoadFromGameData(kMissingDataId, error));
	assert(error == L"Scene ID is not registered in GameData: " + std::to_wstring(kMissingDataId));
	assert(transitionBridge.GetBoundRoot() == retainedRoot && retainedRoot->GetFocusId() == retainedFocus);
	FTextRPGSceneManager manager(store);
	assert(manager.LoadStartupScene(transitionBridge, error));
	PublishTestFrame(manager);
	FSceneUiRoot* oldRoot = transitionBridge.GetBoundRoot();
	assert(!manager.LoadSceneById(kMissingDataId, transitionBridge, error));
	assert(transitionBridge.GetBoundRoot() == oldRoot);
	assert(manager.LoadSceneById(kTownSceneId, transitionBridge, error));
	PublishTestFrame(manager);
	assert(transitionBridge.GetBoundRoot() != oldRoot);
	manager.Render(production);
	assert(productionCell(0, 5) == L'┌' && productionCell(86, 11) == L'┌');
	event = {};
	event.Key = EInputKey::Escape;
	assert(manager.HandleInput(event).Type == ESceneAction::Quit);
	// Insufficient viewport blocks both digits and pointer confirmation.
	event.Key = EInputKey::Digit4;
	event.ScreenWidth = 80;
	event.ScreenHeight = 25;
	assert(scene.HandleInput(event).Type == ESceneAction::None);
	std::cout << "PASS preparation preservation transition bridge lifetime Escape viewport block\n";
	FUiTextArt logo({0, 0, 83, 41});
	assert(logo.LoadFromGameData(store, kMainLogoArtId, error));
	FFrameBuffer logoFrame(83, 41);
	logo.Render({logoFrame, 0, 0, {0, 0, 83, 41}});
	const auto logoCells = logoFrame.GetCells();
	assert(std::any_of(logoCells.begin(), logoCells.end(), [](wchar_t glyph) { return glyph != L' '; }));
	assert(!logo.LoadFromGameData(store, kMonsterDragonArtId, error));
	logoFrame.Clear();
	logo.Render({logoFrame, 0, 0, {0, 0, 83, 41}});
	assert(logoFrame.GetCells() == logoCells);
	scene.Render(production);
	for (int y = 0; y < 41; ++y)
	{
		for (int x = 0; x < 83; ++x)
		{
			assert(productionCell(x + 1, y + 1) == logoCells[static_cast<std::size_t>(y * 83 + x)]);
		}
	}
	std::cout << "PASS real main-menu logo runtime member strict slot safe replacement\n";
	FLayerFixture opaque;
	int underClicks = 0;
	auto under = std::make_unique<FUiButton>(SCellRect{0, 0, 12, 5}, L"under", [&underClicks]() { ++underClicks; });
	opaque.AddBase(std::move(under));
	const auto opaqueId = opaque.OpenPopup(std::make_unique<FUiPanel>(SCellRect{0, 0, 12, 5}));
	opaque.FlushCommands();
	opaque.Render(frame);
	assert(cell(1, 1) == L' '); // Panel background, not old Base label.
	event = {};
	event.ScreenWidth = 20;
	event.ScreenHeight = 10;
	event.MouseX = 2;
	event.MouseY = 1;
	event.Type = EInputEventType::PointerDown;
	opaque.Dispatch(event);
	event.Type = EInputEventType::PointerUp;
	opaque.Dispatch(event);
	assert(underClicks == 0); // Unhandled cannot fall through.
	opaque.ClosePopup(opaqueId);
	opaque.FlushCommands();
	opaque.Render(frame);
	assert(cell(1, 1) == L'u');
	FSceneUiRoot ancestors;
	auto owner = std::make_unique<FUiPanel>(SCellRect{0, 0, 12, 6});
	FUiPanel* ownerPtr = owner.get();
	owner->AddChild(std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"child", []() {}));
	ancestors.AddBase(std::move(owner));
	event.Type = EInputEventType::PointerDown;
	ancestors.Dispatch(event);
	assert(ancestors.GetCaptureId() != 0);
	ownerPtr->SetVisible(false);
	event.Type = EInputEventType::PointerMove;
	ancestors.Dispatch(event);
	assert(ancestors.GetCaptureId() == 0 && ancestors.GetFocusId() == 0);
	std::cout << "PASS opaque panel Unhandled occlusion recomposition ancestor cancellation\n";
	FLayerFixture transitionCommands;
	FUiId queuedPopup = 0;
	auto transitionButton = std::make_unique<FUiButton>(SCellRect{0, 0, 8, 3}, L"transition",
														[&]()
														{
															queuedPopup = transitionCommands.OpenPopup(makePopup());
															transitionCommands.DiscardCommands();
														});
	transitionCommands.AddBase(std::move(transitionButton));
	event = {};
	event.Type = EInputEventType::KeyDown;
	event.Key = EInputKey::Enter;
	transitionCommands.Dispatch(event);
	assert(queuedPopup != 0 && transitionCommands.Find(queuedPopup) == nullptr);
	std::cout << "PASS Scene transition priority discards pending old-tree popup commands\n";
	FConsoleInput viewportAdapter;
	viewportAdapter.BeginBatch({4, 8}, 132, 43);
	assert(viewportAdapter.ReadEvent(event) == EInputReadResult::Event && event.Type == EInputEventType::Resize);
	viewportAdapter.BeginBatch({5, 8}, 132, 43);
	assert(viewportAdapter.ReadEvent(event) == EInputReadResult::Event && event.Type == EInputEventType::Resize);
	assert(viewportAdapter.ReadEvent(event) == EInputReadResult::Closed);
	std::cout << "PASS viewport origin changes normalize cancellation before physical events\n";
	// Additional framebuffer boundary checks use the production cell-width path.
	FUiPanel narrow({0, 0, 4, 3});
	narrow.SetTitle(L"전");
	frame.Clear();
	narrow.Render({frame, 0, 0, {0, 0, 20, 10}});
	assert(cell(0, 0) == L'┌' && cell(1, 0) == L'─' && cell(2, 0) == L'─' && cell(3, 0) == L'┐');
	panel.SetTitle(L"A\nB");
	frame.Clear();
	panel.Render({frame, 0, 0, {4, 0, 7, 10}});
	assert(cell(3, 1) == L' ' && cell(4, 1) == L' ' && cell(5, 1) == L'B' && cell(10, 1) == L'┐');
	// The actual adapter routes press/release to the real Scene button.
	scene.OnEnter();
	record = {};
	record.EventType = MOUSE_EVENT;
	record.Event.MouseEvent.dwMousePosition = {90, 32};
	record.Event.MouseEvent.dwButtonState = FROM_LEFT_1ST_BUTTON_PRESSED;
	assert(adapter.TranslateRecord(record, {}, 132, 43, event));
	assert(scene.HandleInput(event).Type == ESceneAction::None);
	record.Event.MouseEvent.dwButtonState = 0;
	assert(adapter.TranslateRecord(record, {}, 132, 43, event));
	assert(scene.HandleInput(event).Type == ESceneAction::Quit);
	scene.OnExit();
	// 후보 패키지의 손상 자산은 게시 전 거부하며 기존 Store/UI를 함께 보존한다.
	const auto fixtureRoot = std::filesystem::path(argv[2]) / L"asset-fixture";
	std::filesystem::create_directories(fixtureRoot);
	std::filesystem::copy(std::filesystem::path(argv[1]) / L"Asset", fixtureRoot / L"Asset",
		std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing);
	const auto fixtureAsset = fixtureRoot / L"Asset/Logo/Logo_Text_Rpg.txt";
	std::ifstream originalAsset(fixtureAsset, std::ios::binary);
	const std::string originalBytes{std::istreambuf_iterator<char>(originalAsset), std::istreambuf_iterator<char>()};
	originalAsset.close();
	const auto* priorAsset = store.FindAsset(L"Asset/Logo/Logo_Text_Rpg.txt");
	assert(priorAsset && std::string(priorAsset->begin(), priorAsset->end()) == originalBytes);
	const auto replaceFixture = [&fixtureAsset](const std::string& bytes)
	{
		std::ofstream output(fixtureAsset, std::ios::binary | std::ios::trunc);
		output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	};
	const auto fixturePackage = fixtureRoot / L"fixture.dat";
	const auto fixtureYaml = std::filesystem::path(argv[2]) / L"runtime-yaml";
	const std::array<std::uint8_t, 32> fixtureKey{};
	std::string fixtureError;
	for (const std::string& invalid : {std::string("\xff"), originalBytes + "\n", std::string("wrong-size\n")})
	{
		replaceFixture(invalid);
		assert(GameData::BuildEncryptedPackage(fixtureYaml, fixturePackage, fixtureKey, fixtureError, fixtureRoot / L"Asset"));
		assert(!store.Load(fixturePackage, fixtureKey, fixtureError));
		assert(fixtureError.find("TextArt") != std::string::npos);
		assert(store.FindAsset(L"Asset/Logo/Logo_Text_Rpg.txt") == priorAsset);
		assert(std::string(priorAsset->begin(), priorAsset->end()) == originalBytes);
		assert(logo.LoadFromGameData(store, kMainLogoArtId, error));
		logoFrame.Clear();
		logo.Render({logoFrame, 0, 0, {0, 0, 83, 41}});
		assert(logoFrame.GetCells() == logoCells);
	}
	replaceFixture(std::string("\xef\xbb\xbf") + originalBytes);
	assert(GameData::BuildEncryptedPackage(fixtureYaml, fixturePackage, fixtureKey, fixtureError, fixtureRoot / L"Asset"));
	assert(store.Load(fixturePackage, fixtureKey, fixtureError));
	assert(logo.LoadFromGameData(store, kMainLogoArtId, error));
	std::cout << "PASS external clip newline narrow title real adapter click malformed art safe preservation\n";
	FLayerFixture readonlyRender;
	FUiId renderPopup = 0;
	renderPopup = readonlyRender.OpenPopup(std::make_unique<FRenderMutationFixture>(readonlyRender, renderPopup));
	readonlyRender.FlushCommands();
	readonlyRender.Render(frame);
	readonlyRender.FlushCommands();
	assert(readonlyRender.Find(renderPopup) != nullptr);
	assert(!ownerPtr->AddChild(std::make_unique<FUiPanel>(SCellRect{0, 0, 2, 2})));
	std::cout << "PASS rendering cannot mutate root and attached child ownership is sealed\n";
	FSceneUiRoot cancellation;
	int cancels = 0;
	cancellation.AddBase(std::make_unique<FCancelFixture>(cancellation, cancels));
	event = {};
	event.ScreenWidth = 20;
	event.ScreenHeight = 10;
	event.MouseX = 2;
	event.MouseY = 1;
	event.Type = EInputEventType::PointerDown;
	cancellation.Dispatch(event);
	cancellation.CancelInput();
	assert(cancels == 1 && cancellation.GetCaptureId() == 0);
	std::cout << "PASS pointer cancellation clears ownership before optional callback reentry\n";
	FSceneUiRoot coordinates;
	SInputEvent received;
	auto coordinatePanel = std::make_unique<FUiPanel>(SCellRect{2, 2, 14, 7});
	auto coordinateButton = std::make_unique<FCoordinateFixture>(SCellRect{1, 1, 8, 3}, received);
	FCoordinateFixture* coordinatePtr = coordinateButton.get();
	coordinatePanel->AddChild(std::move(coordinateButton));
	coordinates.AddBase(std::move(coordinatePanel));
	event = {};
	event.ScreenWidth = 20;
	event.ScreenHeight = 10;
	event.MouseX = 5;
	event.MouseY = 5;
	event.Type = EInputEventType::PointerDown;
	coordinates.Dispatch(event);
	assert(received.MouseX == 5 && received.MouseY == 5 && received.LocalX == 1 && received.LocalY == 1);
	assert(coordinates.GetCaptureId() != 0);
	coordinatePtr->SetRect({2, 1, 8, 3});
	event.Type = EInputEventType::PointerMove;
	coordinates.Dispatch(event);
	assert(coordinates.GetCaptureId() == 0);
	std::cout << "PASS shared geometry supplies normalized local cells and layout change cancels capture\n";
	FSceneUiRoot selection;
	std::array<FUiButton*, 4> slots{};
	auto slotsPanel = std::make_unique<FUiPanel>(SCellRect{0, 0, 20, 20});
	for (int index = 0; index < 4; ++index)
	{
		auto slot = std::make_unique<FUiButton>(SCellRect{0, index * 4, 10, 3}, L"slot", []() {}, index + 1);
		slots[static_cast<std::size_t>(index)] = slot.get();
		slotsPanel->AddChild(std::move(slot));
	}
	selection.AddBase(std::move(slotsPanel));
	event = {};
	event.Key = EInputKey::Down;
	event.RepeatCount = 2;
	selection.Dispatch(event);
	assert(selection.GetFocusId() == slots[2]->GetId());
	slots[2]->SetEnabled(false);
	event.Key = EInputKey::None;
	event.Type = EInputEventType::PointerMove;
	selection.Dispatch(event);
	assert(selection.GetFocusId() == slots[3]->GetId());
	for (auto* slot : slots)
	{
		slot->SetEnabled(false);
	}
	selection.Dispatch(event);
	assert(selection.GetFocusId() == 0);
	FSceneUiRoot keyboard;
	int activations = 0;
	int pointerClicks = 0;
	keyboard.AddBase(std::make_unique<FKeyboardFixture>(activations, pointerClicks));
	event = {};
	event.Key = EInputKey::Enter;
	keyboard.Dispatch(event);
	assert(activations == 1 && pointerClicks == 0);
	std::cout << "PASS disabled focus advances in fixed order all-disabled none keyboard semantic activation\n";
	int outsideClicks = 0;
	const FUiId outsidePopup = scene.OpenUiLayer(
		std::make_unique<FUiButton>(SCellRect{140, 2, 12, 3}, L"outside", [&outsideClicks]() { ++outsideClicks; }));
	scene.FlushUiLayerCommands();
	FFrameBuffer oversized(160, 50);
	scene.Render(oversized);
	assert(oversized.GetCells()[static_cast<std::size_t>(2 * 160 + 140)] == L' ');
	event = {};
	event.ScreenWidth = 160;
	event.ScreenHeight = 50;
	event.MouseX = 141;
	event.MouseY = 3;
	event.Type = EInputEventType::PointerDown;
	(void)scene.HandleInput(event);
	event.Type = EInputEventType::PointerUp;
	(void)scene.HandleInput(event);
	assert(outsideClicks == 0);
	scene.CloseUiLayer(outsidePopup);
	scene.FlushUiLayerCommands();
	std::cout << "PASS production Scene clips popup rendering and hit testing to logical 132x43\n";
	return 0;
}
catch (const std::exception& exception)
{
	std::cerr << "FAIL Scene fixture: " << exception.what() << '\n';
	return 1;
}
