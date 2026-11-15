#pragma once

namespace GameData
{
	// 실패 시 key는 0으로 지운다. 읽기는 개발 키를 생성하지 않는다.
	[[nodiscard]] bool ReadGameDataKey(std::array<std::uint8_t, 32>& key, std::string& error);
	[[nodiscard]] bool EnsureDevelopmentGameDataKey(std::string& error);
}
