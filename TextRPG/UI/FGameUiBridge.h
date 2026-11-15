#pragma once
namespace TextRPG
{
	class FDataScene;
	class FGameUiBridge final : public FUIManager
	{
	public:
		void Bind(FDataScene& scene) noexcept;
		[[nodiscard]] FDataScene* GetBoundScene() const noexcept;
	};
}
