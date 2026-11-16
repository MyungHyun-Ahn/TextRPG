#pragma once
namespace TextRPG
{
	enum class ESceneTransitionResult
	{
		None,
		Published,
		Failed,
		Canceled
	};

	// Scene 소유/실행/안전점은 이 클래스만 통제한다. 파생 클래스는 typed 정책만 협력한다.
	template<class TAction>
	class TSceneManager : private FSceneHost
	{
	public:
		using FScene = TScene<TAction>;
		TSceneManager() = default;
		TSceneManager(const TSceneManager&) = delete;
		TSceneManager& operator=(const TSceneManager&) = delete;
		virtual ~TSceneManager()
		{
			// 파생 멤버가 사라진 뒤에는 base hook만 사용한다. typed 소비자는 destructor에서 먼저 Shutdown한다.
			Shutdown();
		}

		[[nodiscard]] virtual bool SetScene(std::unique_ptr<FScene> incoming) final
		{
			SHostOperation operation(*this);
			// 매개변수는 local guard보다 늦게 파괴된다. 실제 owner를 guard 안으로 옮긴다.
			auto scene = std::move(incoming);
			if (QueueScene(scene, []() noexcept
			{
			})) return true;
			DiscardScene(scene);
			return false;
		}

		[[nodiscard]] virtual bool EndFrame(std::wstring& error) final
		{
			if (!IsSafePointEligible())
			{
				error = L"EndFrame requires all Scene and UI operations to have returned.";
				return false;
			}
			error.clear();
			if (m_isQuitting)
			{
				DiscardPendingTransitions();
				return true;
			}
			if (!m_pendingScene) return true;
			SEndFrameScope ending(*this);
			auto scene = std::move(m_pendingScene);
			OnTransitionSnapshot();
			bool published = false;
			try
			{
				if (!ValidatePublication(*scene)) error = L"Prepared Scene no longer matches its publication context.";
				else
				{
					EnterPreparedScene(*scene);
					if (!m_isQuitting && m_currentScene) m_currentScene->OnExit();
					// 아직 살아 있는 이전 Scene을 버리기 전에 callback의 typed 문맥 변경도 거부한다.
					if (!m_isQuitting && !ValidatePublication(*scene))
						error = L"Prepared Scene no longer matches its publication context.";
				}
				if (m_isQuitting) error = L"Scene publication was canceled by shutdown.";
				if (error.empty())
				{
					if (m_currentScene)
					{
						OnSceneLeaving(*m_currentScene);
						RetireScene(*m_currentScene);
					}
					m_currentScene.reset();
					// 준비 보호는 retire와 실제 Scene destructor가 모두 반환할 때까지 유지한다.
					// 이 시점의 실패는 진행을 보존하지만 이미 파괴한 Scene을 복원하지 않는다.
					if (m_isQuitting) error = L"Scene publication was canceled by shutdown.";
					else if (!ValidatePublication(*scene) || m_isQuitting)
						error = L"Prepared Scene no longer matches its publication context.";
					else
					{
						SPublication publication(*this, scene);
						PublishScene(publication);
						published = m_currentScene != nullptr;
						if (published) OnScenePublished(*m_currentScene);
					}
				}
			}
			catch (const std::exception& exception)
			{
				const std::string message = exception.what();
				error.assign(message.begin(), message.end());
			}
			catch (...)
			{
				error = L"Scene activation failed with an unknown exception.";
			}
			if (m_isQuitting) error = L"Scene publication was canceled by shutdown.";
			DiscardScene(scene);
			if (!published && m_currentScene) RestoreSceneAfterFailedPublication(*m_currentScene);
			if (m_currentScene) MarkTransitionRequested(m_pendingScene != nullptr || m_isQuitting);
			const auto result = published ? ESceneTransitionResult::Published :
				m_isQuitting ? ESceneTransitionResult::Canceled : ESceneTransitionResult::Failed;
			OnTransitionComplete(result, error);
			m_transitionResult = result;
			return published;
		}

		virtual void CancelPendingTransitions() noexcept final
		{
			const bool idle = !m_isEndingFrame && m_operationDepth == 0;
			{
				SHostOperation operation(*this);
				m_isQuitting = true;
				MarkTransitionRequested(true);
				if (m_pendingScene) m_pendingScene->m_transitionRequested = true;
				InvalidatePendingScene(m_pendingScene.get());
			}
			// 한 번 이월한 물리 취소는 callback 반환 뒤 idle 재취소로도 앞당기지 않는다.
			if (!idle || !IsSafePointEligible()) m_isCancellationDeferred = true;
			else if (!m_isCancellationDeferred) DiscardPendingTransitions();
		}

		// 정상 idle 수명 끝에서 호출한다. 자기 callback 안에서 Manager를 파괴하는 사용은 지원하지 않는다.
		virtual void Shutdown() noexcept final
		{
			if (m_isShutdown) return;
			CancelPendingTransitions();
			if (!IsSafePointEligible()) return;
			SEndFrameScope ending(*this);
			DiscardPendingTransitions();
			try
			{
				if (m_currentScene) m_currentScene->OnExit();
			}
			catch (...)
			{
				std::cerr << "Scene shutdown failed.\n";
			}
			DiscardScene(m_currentScene);
			m_isShutdown = true;
		}

		[[nodiscard]] bool IsQuitting() const noexcept
		{
			return m_isQuitting;
		}
		[[nodiscard]] bool HasPendingTransition() const noexcept
		{
			return m_pendingScene != nullptr;
		}
		[[nodiscard]] bool IsExecutingTransition() const noexcept
		{
			return m_isEndingFrame;
		}
		[[nodiscard]] bool TakeTransitionResult(ESceneTransitionResult& result) noexcept
		{
			if (m_isEndingFrame || m_operationDepth != 0 || m_transitionResult == ESceneTransitionResult::None) return false;
			result = std::exchange(m_transitionResult, ESceneTransitionResult::None);
			return true;
		}

		[[nodiscard]] virtual TAction HandleInput(const SInputEvent& event) final
		{
			SHostOperation operation(*this);
			TAction response{};
			if (!AllowManagerInput(event, response)) return response;
			if (!m_isEndingFrame && !m_isQuitting && m_currentScene) return m_currentScene->HandleInput(event);
			return response;
		}
		[[nodiscard]] virtual TAction HandleInputFrame(const std::vector<SInputEvent>& events) final
		{
			SHostOperation operation(*this);
			TAction response{};
			if (!AllowManagerInputFrame(events, response)) return response;
			if (!m_isEndingFrame && !m_isQuitting && m_currentScene) return m_currentScene->HandleInputFrame(events);
			return response;
		}
		virtual void Update(float deltaSeconds) final
		{
			SHostOperation operation(*this);
			if (!m_isEndingFrame && !m_isQuitting && m_currentScene) m_currentScene->Update(deltaSeconds);
		}
		virtual void Render(FFrameBuffer& frame) const final
		{
			SHostOperation operation(*this);
			if (!m_isEndingFrame && m_currentScene)
			{
				m_currentScene->Render(frame);
				RenderOverlay(frame);
			}
		}

	protected:
		// 최종 typed owner swap과 이 비가상/비예외 Publish 사이에는 callback/할당/파괴를 두지 않는다.
		class SPublication final
		{
		public:
			void Publish() noexcept
			{
				if (Scene && !Manager.m_currentScene)
				{
					Scene->m_isPublicationPrepared = false;
					Manager.m_currentScene = std::move(Scene);
				}
			}
		private:
			friend class TSceneManager;
			SPublication(TSceneManager& manager, std::unique_ptr<FScene>& scene) noexcept
				: Manager(manager)
				, Scene(scene)
			{
			}
			TSceneManager& Manager;
			std::unique_ptr<FScene>& Scene;
		};
		template<class TPreparedScene, class TAccept>
		[[nodiscard]] bool QueueScene(std::unique_ptr<TPreparedScene>& scene, TAccept&& accept)
		{
			static_assert(noexcept(accept()));
			SHostOperation operation(*this);
			if (!scene || m_isQuitting || m_pendingScene || m_isAccepting) return false;
			struct SAcceptScope final
			{
				bool& Active;
				explicit SAcceptScope(bool& active) noexcept
					: Active(active)
				{
					Active = true;
				}
				~SAcceptScope()
				{
					Active = false;
				}
			} accepting(m_isAccepting);
			// 첫 virtual busy 조회와 검증 중 재진입은 pending owner를 바꿀 수 없다.
			if (scene->IsBusy() || !ValidateCandidate(*scene) || m_isQuitting || m_pendingScene) return false;
			scene->m_isPublicationPrepared = true;
			accept();
			m_pendingScene = std::move(scene);
			MarkTransitionRequested(true);
			return true;
		}
		[[nodiscard]] FScene* GetCurrentScene() const noexcept
		{
			return m_currentScene.get();
		}
		[[nodiscard]] bool IsHostOperating() const noexcept
		{
			return m_operationDepth != 0;
		}
		struct SHostOperation final
		{
			explicit SHostOperation(const TSceneManager& manager) noexcept
				: Operation(manager)
			{
			}
		private:
			FSceneHost::SHostOperation Operation;
		};
		virtual bool ValidateCandidate(const FScene&) const
		{
			return true;
		}
		virtual bool ValidatePublication(const FScene&) const
		{
			return true;
		}
		virtual void EnterPreparedScene(FScene& scene)
		{
			scene.OnEnter();
		}
		virtual void OnSceneLeaving(FScene&) noexcept
		{
		}
		virtual void RetireScene(FScene&) noexcept
		{
		}
		virtual void RestoreSceneAfterFailedPublication(FScene&) noexcept
		{
		}
		virtual void OnTransitionRequested(FScene*, bool) noexcept
		{
		}
		virtual void InvalidatePendingScene(FScene*) noexcept
		{
		}
		virtual void OnTransitionSnapshot() noexcept
		{
		}
		virtual void PublishScene(SPublication& publication) noexcept
		{
			publication.Publish();
		}
		virtual void OnScenePublished(FScene&) noexcept
		{
		}
		virtual void OnTransitionComplete(ESceneTransitionResult, std::wstring&)
		{
		}
		virtual bool AllowManagerInput(const SInputEvent&, TAction&)
		{
			return true;
		}
		virtual bool AllowManagerInputFrame(const std::vector<SInputEvent>&, TAction&)
		{
			return true;
		}
		virtual void RenderOverlay(FFrameBuffer&) const
		{
		}

	private:
		[[nodiscard]] bool IsSafePointEligible() const noexcept
		{
			if (m_isEndingFrame || m_operationDepth != 0) return false;
			SHostOperation operation(*this);
			return (!m_currentScene || !m_currentScene->IsBusy()) && (!m_pendingScene || !m_pendingScene->IsBusy());
		}
		void MarkTransitionRequested(bool requested) noexcept
		{
			if (m_currentScene) m_currentScene->m_transitionRequested = requested;
			OnTransitionRequested(m_currentScene.get(), requested);
		}
		void DiscardScene(std::unique_ptr<FScene>& scene) noexcept
		{
			if (scene) RetireScene(*scene);
			scene.reset();
		}
		void DiscardPendingTransitions() noexcept
		{
			SEndFrameScope ending(*this);
			if (m_pendingScene)
			{
				auto scene = std::move(m_pendingScene);
				OnTransitionSnapshot();
				DiscardScene(scene);
				std::wstring error;
				OnTransitionComplete(ESceneTransitionResult::Canceled, error);
				m_transitionResult = ESceneTransitionResult::Canceled;
			}
			m_isCancellationDeferred = false;
		}
		std::unique_ptr<FScene> m_currentScene;
		std::unique_ptr<FScene> m_pendingScene;
		ESceneTransitionResult m_transitionResult = ESceneTransitionResult::None;
		bool m_isQuitting = false;
		bool m_isCancellationDeferred = false;
		bool m_isAccepting = false;
		bool m_isShutdown = false;
	};
}
