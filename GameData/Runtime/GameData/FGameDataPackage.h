#pragma once

namespace GameData
{
	struct SPackageEntry
	{
		std::string name;
		std::vector<std::uint8_t> contents;
	};

	class FGameDataPackageReader final
	{
	public:
		[[nodiscard]] bool Load(
			const std::filesystem::path& packagePath,
			const std::array<std::uint8_t, 32>& key,
			std::string& error);

		[[nodiscard]] const SPackageEntry* FindEntry(const std::string& relativeName) const noexcept;
		[[nodiscard]] const std::vector<SPackageEntry>& GetEntries() const noexcept;
		void Unload() noexcept;

	private:
		std::vector<SPackageEntry> m_entries;
	};

	[[nodiscard]] bool BuildEncryptedPackage(
		const std::filesystem::path& yamlRoot,
		const std::filesystem::path& outputPath,
		const std::array<std::uint8_t, 32>& key,
		std::string& error,
		const std::filesystem::path& assetRoot = {});
}
