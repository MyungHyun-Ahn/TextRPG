#pragma once

namespace TextRPG
{
	enum class ERandomEngine
	{
		Mt19937,
		Mt19937_64,
		MinStdRand,
		Ranlux48
	};

	struct SRandomState final : ISerializable
	{
		static constexpr std::string_view kTypeName = "SRandomState";
		std::uint64_t seed = 0;
		std::uint64_t randCount = 0;

		SRandomState() = default;
		SRandomState(std::uint64_t initialSeed, std::uint64_t initialCount) noexcept;
		std::string_view GetSerializableTypeName() const noexcept override;
		bool SerializeFields(FBinaryWriter& writer, std::string& error) const override;
		bool DeserializeFields(FBinaryReader& reader, std::string& error) override;
	};

	struct SSelectResult
	{
		std::uint32_t key = 0;
		std::uint64_t nextRandCount = 0;
	};

	class FProcTable final
	{
	public:
		FProcTable() = default;
		FProcTable(const FProcTable&) = default;
		FProcTable& operator=(const FProcTable&) = default;
		FProcTable(FProcTable&& other) noexcept;
		// 자기 이동 대입은 현재 표를 보존한다. 이동된 원본은 빈 미준비 표로 재사용할 수 있다.
		FProcTable& operator=(FProcTable&& other) noexcept;
		bool AddProc(std::uint32_t key, std::uint32_t proc, std::string& error);
		bool Prepare(std::string& error);
		bool Build(const std::unordered_map<std::uint32_t, std::uint32_t>& entries, std::string& error);
		bool IsPrepared() const noexcept
		{
			return m_isPrepared;
		}
		std::uint64_t GetTotalWeight() const noexcept
		{
			return m_totalWeight;
		}
		std::size_t GetEntryCount() const noexcept
		{
			return m_entries.size();
		}

	private:
		friend class FSelector;
		struct SEntry
		{
			std::uint32_t key;
			std::uint32_t proc;
			std::uint64_t prefix;
		};
		std::vector<SEntry> m_entries;
		std::uint64_t m_totalWeight = 0;
		bool m_isPrepared = false;
	};

	std::uint64_t GenerateSeed();

	class FSelector final
	{
	public:
		// 복원 비용은 엔진과 count에 따른다. 정상 선택으로 만든 위치를 임의 상한으로 거부하지 않는다.
		FSelector();
		explicit FSelector(std::uint64_t seed, std::uint64_t randCount = 0,
			ERandomEngine engineKind = ERandomEngine::Mt19937);
		SRandomState GetState() const noexcept;
		ERandomEngine GetEngineKind() const noexcept
		{
			return m_engineKind;
		}
		bool Restore(const SRandomState& state, std::string& error);
		bool AddProc(std::uint32_t key, std::uint32_t proc);
		bool AddProc(std::uint32_t key, std::uint32_t proc, std::string& error);
		bool SelectKey(SSelectResult& result, std::string& error);
		bool SelectKey(const FProcTable& table, SSelectResult& result, std::string& error);
		// map 편의 경로는 호출마다 준비한다. 반복 선택에는 준비된 const FProcTable을 사용한다.
		bool SelectKey(const std::unordered_map<std::uint32_t, std::uint32_t>& entries,
			SSelectResult& result, std::string& error);
		std::uint32_t SelectKey();

	private:
		using TEngine = std::variant<std::mt19937, std::mt19937_64, std::minstd_rand, std::ranlux48>;
		TEngine m_engine;
		ERandomEngine m_engineKind = ERandomEngine::Mt19937;
		std::uint64_t m_seed = 0;
		std::uint64_t m_randCount = 0;
		FProcTable m_table;
	};
}