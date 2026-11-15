#include "pch.h"
#include "Rendering/FFrameBuffer.h"
#include "FSceneUiRoot.h"

namespace TextRPG
{
	namespace
	{
		// 사용자 virtual callback이 예외로 빠져도 framework 작업 상태는 원래 값으로 돌아간다.
		class SUiFlagScope final
		{
		public:
			explicit SUiFlagScope(bool& flag, bool value = true) noexcept
				: Flag(&flag), Previous(std::exchange(flag, value)) {}
			~SUiFlagScope() { Restore(); }
			void Restore() noexcept
			{
				if (auto* flag = std::exchange(Flag, nullptr)) *flag = Previous;
			}
			SUiFlagScope(const SUiFlagScope&) = delete;
			SUiFlagScope& operator=(const SUiFlagScope&) = delete;
		private:
			bool* Flag;
			bool Previous;
		};
	}
	FSceneUiRoot::FSceneUiRoot(SCellRect bounds, FUiId* nextId) noexcept
		: m_bounds(bounds)
		, m_sharedNextId(nextId)
	{
		m_geometryEvent.ScreenWidth = (std::numeric_limits<int>::max)();
		m_geometryEvent.ScreenHeight = (std::numeric_limits<int>::max)();
	}
	bool FSceneUiRoot::IsBusy() const noexcept
	{
		return m_operationDepth != 0 || m_isDispatching || m_isRendering;
	}
	SCellRect FSceneUiRoot::GetClip(int width, int height) const noexcept
	{
		const SCellRect viewport{0, 0, width, height};
		return m_bounds.Width > 0 && m_bounds.Height > 0 ? viewport.Intersect(m_bounds) : viewport;
	}
	FUiId FSceneUiRoot::AddBase(std::unique_ptr<IUI> incomingUi)
	{
		// 진입 전 busy를 판정하되, 거부/예외의 미채택 UI destructor도 작업 수명 안에서 실행한다.
		const bool wasBusy = IsBusy();
		const SOperationScope operation(m_operationDepth);
		auto ui = std::move(incomingUi); // 매개변수는 지역 guard보다 늦게 파괴되므로 소유권을 먼저 비운다.
		if (!ui || wasBusy || ui->GetId() != 0)
		{
			return 0;
		}
		ui->AssignIds(m_sharedNextId ? *m_sharedNextId : m_nextId);
		const FUiId id = ui->GetId();
		m_entries.push_back(std::move(ui));
		RepairFocus();
		return id;
	}
	void FSceneUiRoot::SuspendInput()
	{
		const SOperationScope operation(m_operationDepth);
		m_isSuspended = true;
		CancelInput();
		if (IUI* focused = Find(m_focusId))
		{
			focused->SetFocused(false);
		}
	}
	void FSceneUiRoot::ResumeInput()
	{
		const SOperationScope operation(m_operationDepth);
		m_isSuspended = false;
		RepairFocus();
	}
	void FSceneUiRoot::Render(FFrameBuffer& frame, bool clear) const
	{
		const SOperationScope operation(m_operationDepth);
		if (m_isRendering)
		{
			return;
		}
		SUiFlagScope rendering(m_isRendering);
		if (clear)
		{
			frame.Clear();
		}
		const SUiRenderContext context{frame, 0, 0, GetClip(frame.GetWidth(), frame.GetHeight())};
		for (const auto& entry : m_entries)
		{
			entry->Render(context);
		}
		rendering.Restore();
	}
	IUI* FSceneUiRoot::Find(FUiId id) const noexcept
	{
		const SOperationScope operation(m_operationDepth);
		if (id == 0)
		{
			return nullptr;
		}
		for (const auto& entry : m_entries)
		{
			if (IUI* found = entry->Find(id))
			{
				return found;
			}
		}
		return nullptr;
	}
	std::vector<IUI*> FSceneUiRoot::GetFocusable() const
	{
		std::vector<IUI*> result;
		for (const auto& entry : m_entries)
		{
			entry->CollectFocusable(result);
		}
		FilterFocusable(result);
		return result;
	}
	void FSceneUiRoot::FilterFocusable(std::vector<IUI*>& candidates) const
	{
		std::erase_if(candidates, [this](const IUI* ui)
		{
			SUiGeometry geometry;
			return !ResolveGeometry(ui->GetId(), m_geometryEvent, geometry) ||
				geometry.Clip.Width <= 0 || geometry.Clip.Height <= 0;
		});
	}
	void FSceneUiRoot::SetFocus(FUiId id)
	{
		if (IUI* previous = Find(m_focusId))
		{
			previous->SetFocused(false);
		}
		m_focusId = id;
		if (IUI* current = Find(id))
		{
			current->SetFocused(!m_isSuspended);
		}
	}
	void FSceneUiRoot::RepairFocus(FUiId preferred)
	{
		const auto focusable = GetFocusable();
		const FUiId requested = preferred == 0 ? m_focusId : preferred;
		for (IUI* ui : focusable)
		{
			if (ui->GetId() == requested)
			{
				SetFocus(requested);
				return;
			}
		}
		// entry는 다른 Layer가 아니라 같은 Layer 안의 형제 UI 집합이다.
		std::vector<IUI*> order;
		for (const auto& entry : m_entries)
		{
			entry->CollectFocusable(order, true);
		}
		const auto previous = std::find_if(order.begin(), order.end(), [requested](const IUI* ui) { return ui->GetId() == requested; });
		if (previous != order.end())
		{
			const auto offset = static_cast<std::size_t>(previous - order.begin());
			for (std::size_t step = 1; step <= order.size(); ++step)
			{
				IUI* candidate = order[(offset + step) % order.size()];
				if (std::find(focusable.begin(), focusable.end(), candidate) != focusable.end())
				{
					SetFocus(candidate->GetId());
					return;
				}
			}
		}
		SetFocus(focusable.empty() ? 0 : focusable.front()->GetId());
	}
	FUiId FSceneUiRoot::GetFocusId() const noexcept
	{
		return m_focusId;
	}
	FUiId FSceneUiRoot::GetCaptureId() const noexcept
	{
		return m_captureId;
	}
	void FSceneUiRoot::InvalidateInput() noexcept
	{
		++m_gestureGeneration;
		m_isSuspended = true;
	}
	void FSceneUiRoot::CancelInput()
	{
		const SOperationScope operation(m_operationDepth);
		++m_gestureGeneration;
		const FUiId captureId = std::exchange(m_captureId, 0);
		const bool wasDragging = std::exchange(m_isDragging, false);
		SUiFlagScope dispatching(m_isDispatching);
		IUI* captured = Find(captureId);
		if (captured)
		{
			if (wasDragging)
			{
				captured->OnDrag(EUiDragPhase::Cancel, m_lastPointerEvent);
			}
			captured->OnPointerCancel();
		}
		dispatching.Restore();
	}
	void FSceneUiRoot::Dispatch(const SInputEvent& event, bool allowInput)
	{
		const SOperationScope operation(m_operationDepth);
		if (m_isDispatching)
		{
			return;
		}
		if (event.ScreenWidth != 0 || event.ScreenHeight != 0 || event.Type == EInputEventType::Resize)
		{
			// 크기를 생략한 합성 키 이벤트는 마지막 viewport를 유지하고 실제 Resize의 0 크기는 반영한다.
			m_geometryEvent = event;
		}
		SUiFlagScope dispatching(m_isDispatching);
		if (m_captureId != 0)
		{
			bool available = false;
			for (std::size_t index = 0; index < m_entries.size(); ++index)
			{
				if (m_entries[index]->IsAvailable(m_captureId))
				{
					available = true;
					break;
				}
			}
			SUiGeometry geometry;
			if (!available || !ResolveGeometry(m_captureId, event, geometry) || geometry != m_pressGeometry)
			{
				CancelInput();
			}
		}
		const bool isSystem = event.Type == EInputEventType::FocusLost || event.Type == EInputEventType::Resize;
		const bool isConfirm = event.Key == EInputKey::Enter || (event.Key >= EInputKey::Digit1 && event.Key <= EInputKey::Digit4);
		if (isSystem || !allowInput)
		{
			CancelInput();
		}
		else if (!m_isSuspended && !(event.Type == EInputEventType::KeyDown && isConfirm && event.IsRepeat))
		{
			const auto generation = m_gestureGeneration;
			RepairFocus();
			if (generation == m_gestureGeneration && !m_isSuspended)
			{
				DispatchToTarget(event);
			}
		}
		RepairFocus();
		dispatching.Restore();
	}
	bool FSceneUiRoot::ResolveGeometry(FUiId id, const SInputEvent& event, SUiGeometry& geometry) const
	{
		FFrameBuffer dummy;
		const SUiRenderContext context{dummy, 0, 0, GetClip(event.ScreenWidth, event.ScreenHeight)};
		for (const auto& entry : m_entries)
		{
			if (entry->ResolveGeometry(id, context, geometry))
			{
				return true;
			}
		}
		return false;
	}
	IUI* FSceneUiRoot::FindInputTarget(FUiId id, std::uint64_t generation, const SInputEvent& event, bool requireFocus) const
	{
		if (generation != m_gestureGeneration || m_isSuspended || (requireFocus && m_focusId != id))
		{
			return nullptr;
		}
		IUI* ui = Find(id);
		if (!ui || (requireFocus && !ui->IsFocusable()))
		{
			return nullptr;
		}
		SUiGeometry geometry;
		if (!ResolveGeometry(id, event, geometry) || geometry.Clip.Width <= 0 || geometry.Clip.Height <= 0)
		{
			return nullptr;
		}
		for (const auto& entry : m_entries)
		{
			if (entry->IsAvailable(id))
			{
				return ui;
			}
		}
		return nullptr;
	}
	bool FSceneUiRoot::IsGestureValid(FUiId id, std::uint64_t generation, const SInputEvent& event) const
	{
		if (generation != m_gestureGeneration || m_captureId != id)
		{
			return false;
		}
		SUiGeometry geometry;
		if (!ResolveGeometry(id, event, geometry) || geometry != m_pressGeometry ||
			geometry.Clip.Width <= 0 || geometry.Clip.Height <= 0)
		{
			return false;
		}
		for (std::size_t index = 0; index < m_entries.size(); ++index)
		{
			const auto& entry = m_entries[index];
			if (entry->IsAvailable(id))
			{
				return true;
			}
		}
		return false;
	}
	SInputEvent FSceneUiRoot::MakeLocalEvent(FUiId id, const SInputEvent& event) const
	{
		SInputEvent local = event;
		SUiGeometry geometry;
		if (ResolveGeometry(id, event, geometry))
		{
			const auto localCoordinate = [](int position, int origin)
			{
				// 캡처 중 화면 밖 좌표도 전달하되 int 이벤트 필드의 한계에서만 제한한다. 기하는 보정하지 않는다.
				const auto difference = static_cast<std::int64_t>(position) - origin;
				return static_cast<int>(std::clamp(difference,
					static_cast<std::int64_t>((std::numeric_limits<int>::min)()),
					static_cast<std::int64_t>((std::numeric_limits<int>::max)())));
			};
			local.LocalX = localCoordinate(event.MouseX, geometry.Outer.X);
			local.LocalY = localCoordinate(event.MouseY, geometry.Outer.Y);
		}
		return local;
	}
	void FSceneUiRoot::DispatchToTarget(const SInputEvent& event)
	{
		FFrameBuffer dummy;
		const SUiRenderContext context{dummy, 0, 0, GetClip(event.ScreenWidth, event.ScreenHeight)};
		IUI* target = nullptr;
		for (std::size_t index = m_entries.size(); index > 0; --index)
		{
			const auto& entry = m_entries[index - 1];
			target = entry->HitTest(context, event.MouseX, event.MouseY);
			if (target)
			{
				break;
			}
		}
		if (event.Type == EInputEventType::PointerDown)
		{
			const FUiId targetId = target ? target->GetId() : 0;
			CancelInput();
			const auto generation = m_gestureGeneration;
			target = FindInputTarget(targetId, generation, event, false);
			if (target)
			{
				const bool requiresFocus = target->IsFocusable();
				if (requiresFocus)
				{
					SetFocus(targetId);
				}
				// 포커스 콜백 뒤에는 이전 포인터/조상 상태/기하를 재사용하지 않는다.
				target = FindInputTarget(targetId, generation, event, requiresFocus);
				if (!target || !ResolveGeometry(targetId, event, m_pressGeometry) ||
					!m_pressGeometry.Clip.Contains(event.MouseX, event.MouseY))
				{
					return;
				}
				if (target->OnPointerDown(MakeLocalEvent(targetId, event)) == EUiReply::Capture &&
					generation == m_gestureGeneration)
				{
					m_captureId = targetId;
					m_pressEvent = event;
					m_lastPointerEvent = MakeLocalEvent(m_captureId, event);
					if (!IsGestureValid(m_captureId, generation, event))
					{
						CancelInput();
					}
				}
			}
		}
		else if (event.Type == EInputEventType::PointerUp)
		{
			IUI* captured = Find(m_captureId);
			const auto generation = m_gestureGeneration;
			if (captured && captured->IsVisible() && captured->IsEnabled())
			{
				const auto local = MakeLocalEvent(captured->GetId(), event);
				captured->OnPointerUp(local);
				if (!IsGestureValid(captured->GetId(), generation, event))
				{
					CancelInput();
					return;
				}
				if (m_isDragging)
				{
					captured->OnDrag(EUiDragPhase::End, local);
				}
				else if (captured == target && m_pressGeometry.Clip.Contains(event.MouseX, event.MouseY))
				{
					captured->OnClick(local);
				}
			}
			m_captureId = 0;
			m_isDragging = false;
		}
		else if (event.Type == EInputEventType::PointerMove)
		{
			if (IUI* captured = Find(m_captureId))
			{
				const auto generation = m_gestureGeneration;
				m_lastPointerEvent = MakeLocalEvent(m_captureId, event);
				if (captured->SupportsDrag() && !m_isDragging &&
					(event.MouseX != m_pressEvent.MouseX || event.MouseY != m_pressEvent.MouseY))
				{
					m_isDragging = true;
					captured->OnDrag(EUiDragPhase::Begin, m_lastPointerEvent);
				}
				if (!IsGestureValid(captured->GetId(), generation, event))
				{
					CancelInput();
					return;
				}
				captured->OnPointerMove(m_lastPointerEvent);
				if (!IsGestureValid(captured->GetId(), generation, event))
				{
					CancelInput();
					return;
				}
				if (m_isDragging)
				{
					captured->OnDrag(EUiDragPhase::Move, m_lastPointerEvent);
				}
			}
			else if (target && target->IsEnabled())
			{
				target->OnPointerMove(MakeLocalEvent(target->GetId(), event));
			}
		}
		else if (event.Type == EInputEventType::Wheel)
		{
			if (target && target->IsEnabled())
			{
				target->OnWheel(MakeLocalEvent(target->GetId(), event));
			}
		}
		else if (event.Type == EInputEventType::KeyDown || event.Type == EInputEventType::KeyUp)
		{
			IUI* focused = FindInputTarget(m_focusId, m_gestureGeneration, m_geometryEvent, true);
			if (!focused)
			{
				return;
			}
			if (event.Type == EInputEventType::KeyUp)
			{
				focused->OnKeyUp(event);
				return;
			}
			const auto focusable = GetFocusable();
			if (event.Key == EInputKey::Up || event.Key == EInputKey::Down)
			{
				const auto current = std::find(focusable.begin(), focusable.end(), focused);
				if (current == focusable.end() || focusable.empty())
				{
					return;
				}
				const std::size_t count = focusable.size();
				const std::size_t offset = static_cast<std::size_t>(current - focusable.begin());
				const std::size_t steps = event.RepeatCount % count;
				const std::size_t next = event.Key == EInputKey::Down ? (offset + steps) % count : (offset + count - steps) % count;
				SetFocus(focusable[next]->GetId());
				return;
			}
			if (event.Key >= EInputKey::Digit1 && event.Key <= EInputKey::Digit4)
			{
				const int shortcut = static_cast<int>(event.Key) - static_cast<int>(EInputKey::Digit1) + 1;
				for (IUI* ui : focusable)
				{
					if (ui->GetShortcut() == shortcut)
					{
						const FUiId id = ui->GetId();
						const auto generation = m_gestureGeneration;
						SetFocus(id);
						if (IUI* current = FindInputTarget(id, generation, m_geometryEvent, true))
						{
							current->OnKeyDown(event);
						}
						return;
					}
				}
				return;
			}
			focused->OnKeyDown(event);
		}
	}
}
