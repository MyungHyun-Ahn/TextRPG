#include "GameData/pch.h"
#include "FGeneratedYaml.h"

namespace
{
	bool Quoted(std::wstring_view line, std::size_t& at, std::wstring& value)
	{
		if (at >= line.size() || line[at++] != L'"') return false;
		while (at < line.size())
		{
			wchar_t c = line[at++];
			if (c == L'"') return true;
			if (c < 0x20) return false;
			if (c != L'\\') { value += c; continue; }
			if (at == line.size()) return false;
			c = line[at++];
			switch (c)
			{
			case L'"': case L'\\': case L'/': value += c; break;
			case L'n': value += L'\n'; break;
			case L'r': value += L'\r'; break;
			case L't': value += L'\t'; break;
			case L'b': value += L'\b'; break;
			case L'f': value += L'\f'; break;
			case L'u':
			{
				if (line.size() - at < 4) return false;
				unsigned number = 0;
				for (unsigned digit = 0; digit < 4; ++digit)
				{
					const wchar_t hex = line[at++];
					number *= 16;
					if (hex >= L'0' && hex <= L'9') number += hex - L'0';
					else if (hex >= L'a' && hex <= L'f') number += hex - L'a' + 10;
					else if (hex >= L'A' && hex <= L'F') number += hex - L'A' + 10;
					else return false;
				}
				if (number == 0) return false;
				value += static_cast<wchar_t>(number);
				break;
			}
			default: return false;
			}
		}
		return false;
	}
	bool ValidUtf16(const std::wstring& value)
	{
		for (std::size_t i = 0; i < value.size(); ++i)
		{
			const auto c = static_cast<unsigned>(value[i]);
			if (c >= 0xD800 && c <= 0xDBFF)
			{
				if (++i == value.size() || value[i] < 0xDC00 || value[i] > 0xDFFF) return false;
			}
			else if (c >= 0xDC00 && c <= 0xDFFF) return false;
		}
		return true;
	}
}

namespace GameData
{
	bool ParseGeneratedYaml(const std::vector<std::uint8_t>& bytes, std::vector<SYamlRow>& rows, std::string& error)
	{
		const auto fail = [&error](std::size_t line, const char* reason)
		{
			error = "YAML line " + std::to_string(line) + ": " + reason;
			return false;
		};
		if (bytes.empty() || bytes.size() > 8 * 1024 * 1024) return fail(1, "size limit");
		const std::string_view encoded(reinterpret_cast<const char*>(bytes.data()), bytes.size());
		constexpr std::string_view marker = "# TextRPG.GameData 1\n";
		if (!encoded.starts_with(marker)) return fail(1, "missing or unsupported GameData format version");
		const int size = static_cast<int>(encoded.size());
		const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, encoded.data(), size, nullptr, 0);
		if (count == 0) return fail(1, "invalid UTF-8");
		std::wstring text(static_cast<std::size_t>(count), L' ');
		if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, encoded.data(), size, text.data(), count) != count)
			return fail(1, "UTF-8 conversion failed");
		std::vector<SYamlRow> candidate;
		std::set<std::wstring> rootKeys;
		std::size_t start = marker.size();
		std::size_t lineNumber = 1;
		while (start < text.size())
		{
			++lineNumber;
			const auto end = text.find(L'\n', start);
			const auto stop = end == std::wstring::npos ? text.size() : end;
			std::wstring_view line(text.data() + start, stop - start);
			start = stop + 1;
			if (!line.empty() && line.back() == L'\r') line.remove_suffix(1);
			if (line.empty()) return fail(lineNumber, "blank lines are unsupported");
			if (line.size() > 65536) return fail(lineNumber, "line limit");
			const bool field = line.starts_with(L"  \"");
			std::size_t at = field ? 2 : 0;
			std::wstring key;
			if (!Quoted(line, at, key) || key.empty() || !ValidUtf16(key) || at == line.size() || line[at++] != L':')
				return fail(lineNumber, "expected quoted mapping key");
			if (!field)
			{
				if (at != line.size() || !rootKeys.insert(key).second) return fail(lineNumber, "duplicate or malformed row key");
				if (candidate.size() >= 65536) return fail(lineNumber, "row limit");
				candidate.push_back({std::move(key), {}, lineNumber});
				continue;
			}
			if (candidate.empty() || at == line.size() || line[at++] != L' ') return fail(lineNumber, "expected row scalar");
			SYamlScalar scalar;
			const auto tail = line.substr(at);
			if (tail.starts_with(L"\""))
			{
				scalar.kind = EYamlScalar::String;
				if (!Quoted(line, at, scalar.text) || at != line.size() || !ValidUtf16(scalar.text))
					return fail(lineNumber, "invalid quoted string or escape");
			}
			else if (tail == L"null") scalar.kind = EYamlScalar::Null;
			else if (tail == L"true" || tail == L"false")
			{
				scalar.kind = EYamlScalar::Bool;
				scalar.boolean = tail == L"true";
			}
			else
			{
				scalar.kind = EYamlScalar::UInt32;
				if (tail.empty() || (tail.size() > 1 && tail.front() == L'0')) return fail(lineNumber, "invalid uint32 scalar");
				std::uint64_t number = 0;
				for (const wchar_t c : tail)
				{
					if (c < L'0' || c > L'9') return fail(lineNumber, "unsupported scalar kind");
					number = number * 10 + static_cast<unsigned>(c - L'0');
					if (number > UINT32_MAX) return fail(lineNumber, "uint32 range");
				}
				scalar.number = static_cast<std::uint32_t>(number);
			}
			auto& fields = candidate.back().fields;
			if (fields.size() >= 64 || !fields.emplace(std::move(key), std::move(scalar)).second)
				return fail(lineNumber, "duplicate field or field limit");
		}
		rows.swap(candidate);
		error.clear();
		return true;
	}
}
