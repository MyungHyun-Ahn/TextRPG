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

	bool ResolveGameDataRoot(std::filesystem::path& gameDataRoot, std::wstring& errorMessage)
	{
		std::vector<wchar_t> executableBuffer(32768);
		const DWORD executableLength = GetModuleFileNameW(
			nullptr,
			executableBuffer.data(),
			static_cast<DWORD>(executableBuffer.size()));
		if (executableLength == 0 || executableLength >= executableBuffer.size())
		{
			errorMessage = L"Unable to determine the executable directory.";
			return false;
		}

		const std::filesystem::path executablePath(std::wstring(executableBuffer.data(), executableLength));
		const std::filesystem::path executableDirectory = executablePath.parent_path();

		std::vector<wchar_t> overrideBuffer(32768);
		const DWORD overrideLength = GetEnvironmentVariableW(
			L"TEXTRPG_GAMEDATA_ROOT",
			overrideBuffer.data(),
			static_cast<DWORD>(overrideBuffer.size()));
		if (overrideLength != 0)
		{
			if (overrideLength >= overrideBuffer.size())
			{
				errorMessage = L"TEXTRPG_GAMEDATA_ROOT is too long.";
				return false;
			}
			std::filesystem::path requestedRoot(std::wstring(overrideBuffer.data(), overrideLength));
			if (requestedRoot.is_relative())
			{
				requestedRoot = executableDirectory / requestedRoot;
			}
			std::error_code error;
			if (!std::filesystem::is_directory(requestedRoot / L"Asset", error))
			{
				errorMessage = L"TEXTRPG_GAMEDATA_ROOT does not contain an Asset directory: ";
				errorMessage += requestedRoot.wstring();
				return false;
			}
			gameDataRoot = std::filesystem::weakly_canonical(requestedRoot, error);
			if (error)
			{
				errorMessage = L"Unable to resolve TEXTRPG_GAMEDATA_ROOT: ";
				errorMessage += requestedRoot.wstring();
				return false;
			}
			errorMessage.clear();
			return true;
		}

		for (std::filesystem::path directory = executableDirectory; !directory.empty(); directory = directory.parent_path())
		{
			const std::filesystem::path candidate = directory / L"GameData";
			std::error_code error;
			if (std::filesystem::is_directory(candidate / L"Asset", error))
			{
				gameDataRoot = std::filesystem::weakly_canonical(candidate, error);
				if (!error)
				{
					errorMessage.clear();
					return true;
				}
			}
			const std::filesystem::path parent = directory.parent_path();
			if (parent == directory)
			{
				break;
			}
		}

		errorMessage = L"Unable to locate GameData next to the executable or through its parent directories. Set TEXTRPG_GAMEDATA_ROOT to override.";
		return false;
	}
}
