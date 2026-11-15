#pragma once

namespace TextRPG
{
	[[nodiscard]] bool ResolveGameDataPackagePath(std::filesystem::path& packagePath, std::wstring& errorMessage);
	[[nodiscard]] bool ResolveGameDataRoot(std::filesystem::path& gameDataRoot, std::wstring& errorMessage);
}
