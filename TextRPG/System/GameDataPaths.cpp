#include "pch.h"
#include "GameDataPaths.h"

namespace TextRPG
{
	bool ResolveGameDataPackagePath(std::filesystem::path& packagePath, std::wstring& errorMessage)
	{
		std::vector<wchar_t> executableBuffer(32768);
		const DWORD length = GetModuleFileNameW(nullptr, executableBuffer.data(), static_cast<DWORD>(executableBuffer.size()));
		if (length == 0 || length >= executableBuffer.size())
		{
			errorMessage = L"Cannot resolve executable-adjacent GameData.dat.";
			return false;
		}
		packagePath = std::filesystem::path(std::wstring(executableBuffer.data(), length)).parent_path() / L"GameData.dat";
		errorMessage.clear();
		return true;
	}

}
