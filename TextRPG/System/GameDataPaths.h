#pragma once

namespace TextRPG
{
	[[nodiscard]] bool ResolveGameDataPackagePath(std::filesystem::path& packagePath, std::wstring& errorMessage);
}
