#pragma once

namespace GameData
{
	[[nodiscard]] bool ResolveGameDataPackagePath(std::filesystem::path& packagePath, std::wstring& errorMessage);
}
