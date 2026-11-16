#include "pch.h"
#include "Frame/FrameTypes.h"
#include "Frame/IFrameClock.h"
#include "Frame/FFrame.h"
#include "Frame/FTextRpgFrame.h"

int main(int argc, char* argv[])
{
	TextRPG::SFrameOptions options;
	if (!TextRPG::FTextRpgFrame::ReadOptions(argc, argv, options))
	{
		std::cerr << "Usage: TextRPG.exe [--frame-mode basic|fixed-update|frame-skip]\n";
		return 2;
	}
	TextRPG::FTextRpgFrame game(options);
	const int exitCode = game.Run();
	if (exitCode != 0)
	{
		std::cerr << "Frame stopped with an error (stage " << static_cast<int>(game.GetFailureStage()) << ").\n";
	}
	return exitCode;
}
