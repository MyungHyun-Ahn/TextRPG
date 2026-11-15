#pragma once
namespace TextRPG
{
	class FSceneLifetime
	{
	public:
		virtual ~FSceneLifetime() = default;
		[[nodiscard]] bool IsOperating() const noexcept { return m_operationDepth != 0; }
		[[nodiscard]] bool IsBusy() const noexcept;
	protected:
		virtual bool IsOwnedUiBusy() const noexcept { return false; }
		struct SOperation final
		{
			explicit SOperation(const FSceneLifetime& scene) noexcept;
			~SOperation();
			SOperation(const SOperation&) = delete;
			SOperation& operator=(const SOperation&) = delete;
			const FSceneLifetime& Scene;
		};
	private:
		mutable unsigned m_operationDepth = 0;
	};
}
