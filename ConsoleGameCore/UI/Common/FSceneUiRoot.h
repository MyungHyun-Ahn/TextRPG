#pragma once

namespace TextRPG
{
	class FSceneUiRoot final
	{
	public:
		// nextId는 선택적인 Scene 소유 ID allocator이며 root보다 오래 살아야 한다.
		explicit FSceneUiRoot(SCellRect bounds = {}, FUiId* nextId = nullptr) noexcept;
		FUiId AddBase(std::unique_ptr<IUI> ui);
		void Render(FFrameBuffer& frame, bool clear = true) const;
		void SuspendInput();
		void ResumeInput();
		void Dispatch(const SInputEvent& event, bool allowInput = true);
		[[nodiscard]] FUiId GetFocusId() const noexcept;
		[[nodiscard]] FUiId GetCaptureId() const noexcept;
		[[nodiscard]] IUI* Find(FUiId id) const noexcept;
		void CancelInput();
		// 모든 공개 작업의 최초 virtual 조회부터 인수/지역 객체 정리까지 소유자는 파괴/재로드를 미룬다.
		// const/noexcept 조회도 사용자 virtual callback의 재진입을 허용하는 같은 작업 경계다.
		[[nodiscard]] bool IsBusy() const noexcept;
		// 취소 callback 없이 제스처만 즉시 무효화한다. 물리 정리는 소유 Scene의 안전점에서 수행한다.
		void InvalidateInput() noexcept;

	private:
		struct SOperationScope final
		{
			explicit SOperationScope(std::size_t& depth) noexcept : Depth(depth) { ++Depth; }
			~SOperationScope() { --Depth; }
			std::size_t& Depth;
		};
		[[nodiscard]] SCellRect GetClip(int width, int height) const noexcept;
		void SetFocus(FUiId id);
		void RepairFocus(FUiId preferred = 0);
		[[nodiscard]] std::vector<IUI*> GetFocusable() const;
		void FilterFocusable(std::vector<IUI*>& candidates) const;
		void DispatchToTarget(const SInputEvent& event);
		[[nodiscard]] IUI* FindInputTarget(FUiId id, std::uint64_t generation, const SInputEvent& event, bool requireFocus) const;
		[[nodiscard]] bool IsGestureValid(FUiId id, std::uint64_t generation, const SInputEvent& event) const;
		[[nodiscard]] bool ResolveGeometry(FUiId id, const SInputEvent& event, SUiGeometry& geometry) const;
		[[nodiscard]] SInputEvent MakeLocalEvent(FUiId id, const SInputEvent& event) const;
		std::vector<std::unique_ptr<IUI>> m_entries;
		SCellRect m_bounds;
		SInputEvent m_geometryEvent;
		FUiId m_nextId = 1;
		FUiId* m_sharedNextId = nullptr;
		bool m_isSuspended = false;
		FUiId m_focusId = 0;
		FUiId m_captureId = 0;
		std::uint64_t m_gestureGeneration = 0;
		bool m_isDragging = false;
		SInputEvent m_pressEvent;
		SUiGeometry m_pressGeometry;
		SInputEvent m_lastPointerEvent;
		bool m_isDispatching = false;
		mutable bool m_isRendering = false;
		mutable std::size_t m_operationDepth = 0;
	};
}
