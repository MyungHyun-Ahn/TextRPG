#pragma once

namespace GameData
{
	enum class EYamlScalar { String, UInt32, Bool, Null };
	struct SYamlScalar
	{
		EYamlScalar kind{};
		std::wstring text;
		std::uint32_t number{};
		bool boolean{};
	};
	struct SYamlRow
	{
		std::wstring name;
		std::map<std::wstring, SYamlScalar> fields;
		std::size_t line{};
	};
	// 일반 YAML이 아니다: marker + 인용된 root mapping + 2칸 들여쓴 scalar mapping만 허용한다.
	// 파일 8MiB, 행 65536개, 필드 64개, 물리 줄/문자열 64Ki UTF-16 단위로 제한한다.
	[[nodiscard]] bool ParseGeneratedYaml(const std::vector<std::uint8_t>& bytes, std::vector<SYamlRow>& rows, std::string& error);
}
