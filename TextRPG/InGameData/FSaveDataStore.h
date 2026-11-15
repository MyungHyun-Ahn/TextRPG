#pragma once

namespace TextRPG
{
	// 실패 시 byte 출력은 그대로 보존한다. error는 개발자 진단 전용이다.
	// 평문은 최대 16 MiB이며 호출자가 소유한다. 성공한 Load/Decrypt 출력의 민감 메모리 수명도 호출자가 관리한다.
	[[nodiscard]] bool ResolveSaveDataDirectory(std::filesystem::path& directory, std::string& error);
	[[nodiscard]] bool EncryptSaveData(const std::vector<std::uint8_t>& plain, const std::array<std::uint8_t, 32>& key,
		std::vector<std::uint8_t>& encrypted, std::string& error);
	[[nodiscard]] bool DecryptSaveData(const std::vector<std::uint8_t>& encrypted, const std::array<std::uint8_t, 32>& key,
		std::vector<std::uint8_t>& plain, std::string& error);

	class FSaveDataStore final
	{
	public:
		explicit FSaveDataStore(std::filesystem::path directory);
		[[nodiscard]] bool Save(const std::vector<std::uint8_t>& serialized, std::string& error) const;
		[[nodiscard]] bool Load(std::vector<std::uint8_t>& serialized, std::string& error) const;

	private:
		std::filesystem::path m_directory;
	};
}
