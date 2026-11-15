#pragma once
namespace TextRPG
{
	class FUiTextArtView : public FUiElement
	{
	public:
		explicit FUiTextArtView(SCellRect rect);
		void SetLines(std::vector<std::wstring> lines);
		void Render(const SUiRenderContext& context) const override;
	private:
		std::vector<std::wstring> m_lines;
	};
}
