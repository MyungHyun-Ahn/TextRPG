#include "pch.h"
#include "Serialization/FSerializationArchive.h"
#include "FSelector.h"
#include "RandomRange.h"

namespace TextRPG
{
	SRandomState::SRandomState(std::uint64_t initialSeed, std::uint64_t initialCount) noexcept
		: seed(initialSeed)
		, randCount(initialCount)
	{
	}

	std::string_view SRandomState::GetSerializableTypeName() const noexcept
	{
		return kTypeName;
	}

	bool SRandomState::SerializeFields(FBinaryWriter& writer, std::string& error) const
	{
		return writer.Field("seed", seed, error) && writer.Field("randCount", randCount, error);
	}

	bool SRandomState::DeserializeFields(FBinaryReader& reader, std::string& error)
	{
		SRandomState candidate;
		if (!reader.Field("seed", candidate.seed, error) || !reader.Field("randCount", candidate.randCount, error)) return false;
		seed = candidate.seed;
		randCount = candidate.randCount;
		return true;
	}

	FProcTable::FProcTable(FProcTable&& other) noexcept
		: m_entries(std::move(other.m_entries))
		, m_totalWeight(other.m_totalWeight)
		, m_isPrepared(other.m_isPrepared)
	{
		other.m_entries.clear();
		other.m_totalWeight = 0;
		other.m_isPrepared = false;
	}

	FProcTable& FProcTable::operator=(FProcTable&& other) noexcept
	{
		if (this != &other)
		{
			m_entries = std::move(other.m_entries);
			m_totalWeight = other.m_totalWeight;
			m_isPrepared = other.m_isPrepared;
			other.m_entries.clear();
			other.m_totalWeight = 0;
			other.m_isPrepared = false;
		}
		return *this;
	}

	bool FProcTable::AddProc(std::uint32_t key, std::uint32_t proc, std::string& error)
	{
		if (key == 0)
		{
			error = "Random: key zero";
			return false;
		}
		if (std::any_of(m_entries.begin(), m_entries.end(), [key](const SEntry& entry) { return entry.key == key; }))
		{
			error = "Random: duplicate key";
			return false;
		}
		try
		{
			m_entries.push_back({key, proc, 0});
		}
		catch (const std::exception&)
		{
			error = "Random: table allocation failed";
			return false;
		}
		m_isPrepared = false;
		m_totalWeight = 0;
		error.clear();
		return true;
	}

	bool FProcTable::Prepare(std::string& error)
	{
		if (m_isPrepared)
		{
			error.clear();
			return true;
		}
		try
		{
			auto candidate = m_entries;
			std::sort(candidate.begin(), candidate.end(), [](const SEntry& left, const SEntry& right) { return left.key < right.key; });
			std::uint64_t total = 0;
			for (auto& entry : candidate)
			{
				if (entry.proc > std::numeric_limits<std::uint64_t>::max() - total)
				{
					error = "Random: weight total overflow";
					return false;
				}
				total += entry.proc;
				entry.prefix = total;
			}
			if (total == 0)
			{
				error = "Random: empty or all-zero table";
				return false;
			}
			m_entries.swap(candidate);
			m_totalWeight = total;
			m_isPrepared = true;
			error.clear();
			return true;
		}
		catch (const std::exception&)
		{
			error = "Random: table preparation failed";
			return false;
		}
	}

	bool FProcTable::Build(const std::unordered_map<std::uint32_t, std::uint32_t>& entries, std::string& error)
	{
		FProcTable candidate;
		for (const auto& [key, proc] : entries)
		{
			if (!candidate.AddProc(key, proc, error)) return false;
		}
		if (!candidate.Prepare(error)) return false;
		m_entries.swap(candidate.m_entries);
		m_totalWeight = candidate.m_totalWeight;
		m_isPrepared = true;
		return true;
	}

	std::uint64_t GenerateSeed()
	{
		std::random_device source;
		const auto low = static_cast<std::uint64_t>(source());
		const auto high = static_cast<std::uint64_t>(source());
		return (high << 32) | (low & 0xffffffffULL);
	}

	FSelector::FSelector()
		: FSelector(GenerateSeed(), 0)
	{
	}

	FSelector::FSelector(std::uint64_t seed, std::uint64_t randCount, ERandomEngine engineKind)
		: m_engineKind(engineKind)
	{
		std::string error;
		if (!Restore(SRandomState(seed, randCount), error)) throw std::invalid_argument(error);
	}

	SRandomState FSelector::GetState() const noexcept
	{
		return SRandomState(m_seed, m_randCount);
	}

	bool FSelector::Restore(const SRandomState& state, std::string& error)
	{
		try
		{
			TEngine candidate;
			switch (m_engineKind)
			{
			case ERandomEngine::Mt19937: candidate.emplace<std::mt19937>(); break;
			case ERandomEngine::Mt19937_64: candidate.emplace<std::mt19937_64>(); break;
			case ERandomEngine::MinStdRand: candidate.emplace<std::minstd_rand>(); break;
			case ERandomEngine::Ranlux48: candidate.emplace<std::ranlux48>(); break;
			default: error = "Random: unsupported engine"; return false;
			}
			std::seed_seq sequence{static_cast<std::uint32_t>(state.seed), static_cast<std::uint32_t>(state.seed >> 32)};
			std::visit([&](auto& engine) { engine.seed(sequence); engine.discard(state.randCount); }, candidate);
			m_engine = candidate;
			m_seed = state.seed;
			m_randCount = state.randCount;
			error.clear();
			return true;
		}
		catch (const std::exception&)
		{
			error = "Random: engine initialization failed";
			return false;
		}
	}

	bool FSelector::SelectRange(std::uint32_t minimum, std::uint32_t maximum, std::uint32_t& output, std::string& error)
	{
		if (minimum > maximum) { error = "Random: inverted range"; return false; }
		if (minimum == maximum) { output = minimum; error.clear(); return true; }
		auto candidate = m_engine;
		auto count = m_randCount;
		std::uint64_t value = 0;
		const auto width = static_cast<std::uint64_t>(maximum) - minimum + 1;
		if (!std::visit([&](auto& engine)
		{
			return RandomDetail::DrawBelow(engine, width, count, value, error);
		}, candidate)) return false;
		m_engine = candidate;
		m_randCount = count;
		output = static_cast<std::uint32_t>(minimum + value);
		error.clear();
		return true;
	}

	bool FSelector::AddProc(std::uint32_t key, std::uint32_t proc)
	{
		std::string error;
		return AddProc(key, proc, error);
	}

	bool FSelector::AddProc(std::uint32_t key, std::uint32_t proc, std::string& error)
	{
		return m_table.AddProc(key, proc, error);
	}

	bool FSelector::SelectKey(SSelectResult& result, std::string& error)
	{
		if (!m_table.Prepare(error)) return false;
		return SelectKey(m_table, result, error);
	}

	bool FSelector::SelectKey(const FProcTable& table, SSelectResult& result, std::string& error)
	{
		if (!table.IsPrepared() || table.m_entries.empty() || table.m_totalWeight == 0)
		{
			error = "Random: table not prepared";
			return false;
		}
		auto candidate = m_engine;
		auto nextCount = m_randCount;
		std::uint64_t value = 0;
		if (!std::visit([&](auto& engine) { return RandomDetail::DrawBelow(engine, table.m_totalWeight, nextCount, value, error); },
			candidate)) return false;
		const auto found = std::upper_bound(table.m_entries.begin(), table.m_entries.end(), value,
			[](std::uint64_t target, const FProcTable::SEntry& entry) { return target < entry.prefix; });
		if (found == table.m_entries.end())
		{
			error = "Random: invalid prepared table";
			return false;
		}
		m_engine = candidate;
		m_randCount = nextCount;
		result = {found->key, nextCount};
		error.clear();
		return true;
	}

	bool FSelector::SelectKey(const std::unordered_map<std::uint32_t, std::uint32_t>& entries,
		SSelectResult& result, std::string& error)
	{
		FProcTable table;
		return table.Build(entries, error) && SelectKey(table, result, error);
	}

	std::uint32_t FSelector::SelectKey()
	{
		SSelectResult result;
		std::string error;
		return SelectKey(result, error) ? result.key : 0;
	}
}
