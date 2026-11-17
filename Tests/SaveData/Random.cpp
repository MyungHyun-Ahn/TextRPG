#include "pch.h"
#include "Serialization/FSerializationArchive.h"
#include "Math/Random/FSelector.h"

#include "InGameData/FSaveDataStore.h"

namespace SaveTest
{
	void Require(bool condition, const char* message);
	void Pass(const char* name);

	namespace
	{
		using namespace TextRPG;

		// 콘텐츠가 컴포넌트를 소유하며 저장에는 seed/count만 전달한다.
		class FRandomContent final : public ISerializable
		{
		public:
			static constexpr std::string_view kTypeName = "RandomContentFixture";

			FRandomContent()
				: m_selector(0, 0)
			{
			}
			explicit FRandomContent(ERandomEngine engine)
				: m_selector(0, 0, engine)
			{
			}

			std::string_view GetSerializableTypeName() const noexcept override
			{
				return kTypeName;
			}
			FSelector& Selector() noexcept
			{
				return m_selector;
			}
			const FSelector& Selector() const noexcept
			{
				return m_selector;
			}

		protected:
			bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
			{
				const auto state = m_selector.GetState();
				return writer.Field("seed", state.seed, error) && writer.Field("randCount", state.randCount, error);
			}

			bool DeserializeFields(FBinaryReader& reader, std::string& error) override
			{
				SRandomState state;
				return reader.Field("seed", state.seed, error) && reader.Field("randCount", state.randCount, error)
					&& m_selector.Restore(state, error);
			}

		private:
			FSelector m_selector;
		};

		class FRandomProgress final : public ISerializable
		{
		public:
			static constexpr std::string_view kTypeName = "RandomProgressFixture";
			FRandomContent Enhance;
			FRandomContent Gacha;

			std::string_view GetSerializableTypeName() const noexcept override
			{
				return kTypeName;
			}

		protected:
			bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
			{
				return writer.Field("Enhance", Enhance, error) && writer.Field("Gacha", Gacha, error);
			}

			bool DeserializeFields(FBinaryReader& reader, std::string& error) override
			{
				return reader.Field("Enhance", Enhance, error) && reader.Field("Gacha", Gacha, error);
			}
		};

		FProcTable PrepareRandomTable()
		{
			// 실제 운영 확률이 아닌, 데이터 로드 완료를 모사한 통제된 입력 표다.
			FProcTable table;
			std::string error;
			Require(table.AddProc(17, 5, error) && table.AddProc(42, 11, error) && table.AddProc(99, 2, error),
				"random table registration failed");
			Require(table.Prepare(error), "random table preparation failed");
			return table;
		}

		SSelectResult SelectRandom(FSelector& selector, const FProcTable& table)
		{
			SSelectResult selected;
			std::string error;
			Require(selector.SelectKey(table, selected, error), "random fixture selection failed");
			return selected;
		}

		void SetRandomSeed(FSelector& selector)
		{
			SRandomState state;
			state.seed = GenerateSeed();
			state.randCount = 0;
			std::string error;
			Require(selector.Restore(state, error), "random fixture seed setup failed");
		}

		std::vector<std::uint8_t> ReadRandomFile(const std::filesystem::path& path)
		{
			std::ifstream file(path, std::ios::binary);
			Require(file.good(), "random fixture file missing");
			return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
		}

		struct FRandomChild final
		{
			PROCESS_INFORMATION Process{};
			~FRandomChild()
			{
				if (Process.hThread != nullptr) CloseHandle(Process.hThread);
				if (Process.hProcess != nullptr)
				{
					if (WaitForSingleObject(Process.hProcess, 1000) == WAIT_TIMEOUT)
					{
						TerminateProcess(Process.hProcess, 99);
						WaitForSingleObject(Process.hProcess, 1000);
					}
					CloseHandle(Process.hProcess);
				}
			}
		};
	}

	int RandomContentChild(int argc, wchar_t* argv[])
	{
		Require(argc == 7, "random child argument count mismatch");
		const std::filesystem::path root(argv[2]);
		TextRPG::FSaveDataStore store(root);
		std::vector<std::uint8_t> bytes;
		std::string error;
		Require(store.Load(bytes, error), "random child authenticated Load failed");
		auto restoredOwner = std::make_unique<FRandomProgress>();
		auto& restored = *restoredOwner;
		Require(TextRPG::FSerializationArchive::Deserialize(bytes, restored, error), "random child Deserialize failed");
		const auto table = PrepareRandomTable();
		for (unsigned i = 0; i < 17; ++i) SelectRandom(restored.Gacha.Selector(), table);
		const auto enhance = SelectRandom(restored.Enhance.Selector(), table);
		const auto gacha = SelectRandom(restored.Gacha.Selector(), table);
		Require(enhance.key == std::stoull(argv[3]) && enhance.nextRandCount == std::stoull(argv[4]),
			"random child unrelated content changed enhancement result or position");
		Require(gacha.key == std::stoull(argv[5]) && gacha.nextRandCount == std::stoull(argv[6]),
			"random child gacha did not resume saved engine position");
		std::ofstream report(root / L"child-verified.txt");
		report << "PASS authenticated Load restored content-owned seed/count without reseeding\n";
		report.close();
		Require(!report.fail(), "random child verification report failed");
		return 0;
	}

	void RandomContentRoundTrip(const std::filesystem::path& directory, const wchar_t* executable)
	{
		const auto root = directory.parent_path() / L"random-components";
		const auto table = PrepareRandomTable();
		auto sourceOwner = std::make_unique<FRandomProgress>();
		auto& source = *sourceOwner;
		SetRandomSeed(source.Enhance.Selector());
		SetRandomSeed(source.Gacha.Selector());
		for (unsigned i = 0; i < 9; ++i) SelectRandom(source.Enhance.Selector(), table);
		for (unsigned i = 0; i < 5; ++i) SelectRandom(source.Gacha.Selector(), table);
		const auto enhanceState = source.Enhance.Selector().GetState();
		const auto gachaState = source.Gacha.Selector().GetState();
		std::vector<std::uint8_t> bytes;
		std::string error;
		Require(TextRPG::FSerializationArchive::Serialize(source, bytes, error), "random content Serialize failed");
		TextRPG::FSaveDataStore store(root);
		Require(store.Save(bytes, error), "random content authenticated Save failed");
		const auto ciphertext = ReadRandomFile(root / L"InGameData.sav");
		const auto key = ReadRandomFile(root / L"SaveData.key");
		Require(ciphertext != bytes, "random content was saved as plaintext");
		Pass("random-content-member-seed-count-archive-encrypted-save");

		auto restoredOwner = std::make_unique<FRandomProgress>();
		auto& restored = *restoredOwner;
		std::vector<std::uint8_t> loaded;
		Require(store.Load(loaded, error) && loaded == bytes, "random content authenticated Load failed");
		Require(TextRPG::FSerializationArchive::Deserialize(loaded, restored, error), "random content Deserialize failed");
		const auto restoredEnhance = restored.Enhance.Selector().GetState();
		const auto restoredGacha = restored.Gacha.Selector().GetState();
		Require(restoredEnhance.seed == enhanceState.seed && restoredEnhance.randCount == enhanceState.randCount
			&& restoredGacha.seed == gachaState.seed && restoredGacha.randCount == gachaState.randCount,
			"random content Load regenerated seed or lost count");
		Pass("random-content-load-preserves-generated-seeds-and-counts");

		const auto expectedEnhance = SelectRandom(source.Enhance.Selector(), table);
		for (unsigned i = 0; i < 17; ++i) SelectRandom(source.Gacha.Selector(), table);
		const auto expectedGacha = SelectRandom(source.Gacha.Selector(), table);
		std::wstring command = L"\"" + std::wstring(executable) + L"\" --random-state-check \"" + root.wstring() + L"\" "
			+ std::to_wstring(expectedEnhance.key) + L" " + std::to_wstring(expectedEnhance.nextRandCount) + L" "
			+ std::to_wstring(expectedGacha.key) + L" " + std::to_wstring(expectedGacha.nextRandCount);
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		FRandomChild child;
		Require(CreateProcessW(executable, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
			&startup, &child.Process) != FALSE, "random child CreateProcess failed");
		const bool threadClosed = CloseHandle(child.Process.hThread) != FALSE;
		if (threadClosed) child.Process.hThread = nullptr;
		Require(threadClosed, "random child thread handle close failed");
		DWORD exitCode = 99;
		Require(WaitForSingleObject(child.Process.hProcess, 20000) == WAIT_OBJECT_0
			&& GetExitCodeProcess(child.Process.hProcess, &exitCode) && exitCode == 0,
			"random child restart seed/count verification failed");
		Require(!ReadRandomFile(root / L"child-verified.txt").empty(), "random child verification was not reached");
		Require(ReadRandomFile(root / L"InGameData.sav") == ciphertext && ReadRandomFile(root / L"SaveData.key") == key,
			"random content Load changed original ciphertext or dedicated key");
		Pass("random-content-fresh-process-load-shared-table-independent-streams-no-save-writes");
	}
}
