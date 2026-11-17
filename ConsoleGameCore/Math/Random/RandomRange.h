#pragma once

namespace TextRPG::RandomDetail
{
	// 규약 1: min 보정, 2의 거듭제곱 부분집합 rejection, 하위 비트부터 조립, 범위 밖 재추첨.
	// 호출자는 엔진/count 후보를 넘긴다. 실패한 후보는 게시하지 않는다.
	template<class TEngine>
	bool DrawBelow(TEngine& engine, std::uint64_t total, std::uint64_t& count,
		std::uint64_t& output, std::string& error)
	{
		if (total == 0)
		{
			error = "Random: zero range";
			return false;
		}
		constexpr auto kMaximum = std::numeric_limits<std::uint64_t>::max();
		constexpr auto kSpan = static_cast<std::uint64_t>(TEngine::max()) - static_cast<std::uint64_t>(TEngine::min());
		constexpr unsigned kChunkBits = kSpan == kMaximum ? 64U : static_cast<unsigned>(std::bit_width(kSpan + 1) - 1);
		static_assert(kChunkBits > 0);
		const unsigned needed = total == 1 ? 1U : static_cast<unsigned>(std::bit_width(total - 1));
		for (;;)
		{
			std::uint64_t candidate = 0;
			unsigned filled = 0;
			do
			{
				if (count == kMaximum)
				{
					error = "Random: randCount overflow";
					return false;
				}
				const auto raw = static_cast<std::uint64_t>(engine()) - static_cast<std::uint64_t>(TEngine::min());
				++count;
				if constexpr (kChunkBits < 64)
				{
					if (raw >= (std::uint64_t{1} << kChunkBits)) continue;
				}
				const unsigned take = std::min(kChunkBits, needed - filled);
				const auto mask = kMaximum >> (64U - take);
				candidate |= (raw & mask) << filled;
				filled += take;
			} while (filled < needed);
			if (total == 1 || candidate < total)
			{
				output = total == 1 ? 0 : candidate;
				return true;
			}
		}
	}
}
