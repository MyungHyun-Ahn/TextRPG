#pragma once

namespace TextRPG
{
	class FTextRpgFrame final : public FFrame
	{
	public:
		explicit FTextRpgFrame(const SFrameOptions& options);
		~FTextRpgFrame() override;

		// 실행 전에만 읽는다. 잘못된 인수는 options를 바꾸지 않는다.
		[[nodiscard]] static bool ReadOptions(int argc, char* const argv[], SFrameOptions& options) noexcept;

	protected:
		void OnStart() override;
		void OnStartFrame() override;
		void OnInput() override;
		void OnUpdate(float deltaSeconds) override;
		void OnRender() override;
		void OnEndFrame() override;
		void OnStop() override;

	private:
		struct SState;
		std::unique_ptr<SState> m_state;
	};
}
