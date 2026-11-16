#pragma once
namespace TextRPG
{
	class FFrameBuffer;
	// 결과의 의미는 소비자만 정의한다. bool 변환은 중립적인 처리 완료 여부다.
	template<class TAction>
	class TScene : public FSceneLifetime
	{
	public:
		virtual void OnEnter() final { SOperation operation(*this); OnEnterImpl(); }
		virtual void OnExit() final { SOperation operation(*this); OnExitImpl(); }
		virtual void Update(float deltaSeconds) final { SOperation operation(*this); UpdateImpl(deltaSeconds); }
		virtual void Render(FFrameBuffer& frame) const final { SOperation operation(*this); RenderImpl(frame); }
		[[nodiscard]] virtual TAction HandleInput(const SInputEvent& event) final
		{
			SOperation operation(*this);
			TAction response{};
			if (!AllowInput(event, response)) return response;
			return HandleInputImpl(event);
		}
		[[nodiscard]] virtual TAction HandleInputFrame(const std::vector<SInputEvent>& events) final
		{
			SOperation operation(*this);
			if (m_isHandlingInputFrame) return {};
			struct SFrameScope final
			{
				bool& Handling;
				explicit SFrameScope(bool& handling) : Handling(handling) { Handling = true; }
				~SFrameScope() { Handling = false; }
			} frame(m_isHandlingInputFrame);
			TAction response{};
			std::vector<SInputEvent> allowed;
			const std::vector<SInputEvent>* selected = &events;
			if (!AllowInputFrame(events, allowed, selected, response)) return response;
			return HandleInputFrameImpl(*selected);
		}
	protected:
		virtual void OnEnterImpl() = 0;
		virtual void OnExitImpl() = 0;
		virtual void UpdateImpl(float deltaSeconds) = 0;
		virtual void RenderImpl(FFrameBuffer& frame) const = 0;
		virtual TAction HandleInputImpl(const SInputEvent& event) = 0;
		virtual TAction HandleInputFrameImpl(const std::vector<SInputEvent>& events)
		{
			for (const auto& event : events)
			{
				auto action = HandleInput(event);
				if (static_cast<bool>(action)) return action;
			}
			return {};
		}
		virtual bool AllowInput(const SInputEvent&, TAction&) { return !m_transitionRequested; }
		virtual bool AllowInputFrame(const std::vector<SInputEvent>&, std::vector<SInputEvent>&,
			const std::vector<SInputEvent>*&, TAction&) { return !m_transitionRequested; }
		[[nodiscard]] bool IsTransitionRequested() const noexcept { return m_transitionRequested; }
		// Manager가 수락한 준비 상태는 실제 게시 또는 물리 폐기까지 유지한다.
		// 직접 실행/조회는 허용하며, 콘텐츠는 게시 불변식을 깨는 재준비/소유 목록 변경만 거부한다.
		[[nodiscard]] bool IsPublicationPrepared() const noexcept
		{
			return m_isPublicationPrepared;
		}
		bool m_isHandlingInputFrame = false;
	private:
		template<class> friend class TSceneManager;
		bool m_transitionRequested = false;
		bool m_isPublicationPrepared = false;
	};
}
