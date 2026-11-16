#include "pch.h"
#include "Serialization/FSerializationArchive.h"

namespace TextRPG
{
	namespace
	{
		struct SArchiveGuard;
		thread_local SArchiveGuard* g_archive = nullptr;
		struct SArchiveGuard
		{
			bool Failed = false;
			SArchiveGuard() { g_archive = this; }
			~SArchiveGuard() { g_archive = nullptr; }
		};
		struct SForwardGuard
		{
			std::set<const ISerializable*>& Active;
			const ISerializable* Data;
			~SForwardGuard() { Active.erase(Data); }
		};
		struct SDepthGuard
		{
			SerializationDetail::SContext& Context;
			explicit SDepthGuard(SerializationDetail::SContext& context) : Context(context) { ++Context.Depth; }
			~SDepthGuard() { --Context.Depth; }
		};
	}
	namespace SerializationDetail
	{
		bool Fail(std::string& error, std::string_view reason)
		{
			try { if (error.empty()) error.assign(reason); }
			catch (...) { /* 할당 실패도 bool 실패를 보존한다. */ }
			return false;
		}
		bool Name(std::string_view value)
		{
			if (value.empty() || value.size() > 128) return false;
			for (const char c : value)
				if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return false;
			return !(value.front() >= '0' && value.front() <= '9');
		}
		bool Utf16(std::wstring_view value)
		{
			for (std::size_t i = 0; i < value.size(); ++i)
			{
				const auto unit = static_cast<std::uint32_t>(value[i]);
				if (unit > 0xffff || (unit >= 0xdc00 && unit <= 0xdfff)) return false;
				if (unit >= 0xd800 && unit <= 0xdbff)
				{
					if (++i == value.size() || value[i] < 0xdc00 || value[i] > 0xdfff) return false;
				}
			}
			return true;
		}
		bool SCursor::Read(std::size_t size, std::span<const std::uint8_t>& value, std::string& error)
		{
			if (size > Bytes.size() - Position) return Fail(error, "truncated framing");
			value = Bytes.subspan(Position, size); Position += size; return true;
		}
		bool SCursor::Unsigned(std::size_t width, std::uint64_t& value, std::string& error)
		{
			std::span<const std::uint8_t> bytes;
			if (width > 8 || !Read(width, bytes, error)) return false;
			value = 0;
			for (std::size_t i = 0; i < width; ++i) value |= std::uint64_t(bytes[i]) << (i * 8);
			return true;
		}
		bool SCursor::Blob(std::span<const std::uint8_t>& value, std::string& error)
		{
			std::uint64_t size = 0;
			if (!Unsigned(4, size, error)) return false;
			if (size > kMaxBytes) return Fail(error, "blob byte limit");
			return Read(static_cast<std::size_t>(size), value, error);
		}
		bool SCursor::Text(std::string& value, std::string& error)
		{
			std::span<const std::uint8_t> bytes;
			if (!Blob(bytes, error) || bytes.size() > 4096) return Fail(error, "text length limit");
			value.assign(bytes.begin(), bytes.end()); return true;
		}
		bool SCursor::End(std::string& error) const { return Position == Bytes.size() || Fail(error, "trailing bytes"); }
		bool Put(std::vector<std::uint8_t>& out, std::uint64_t value, std::size_t width, std::string& error)
		{
			if (width > kMaxBytes - out.size()) return Fail(error, "byte limit");
			for (std::size_t i = 0; i < width; ++i) out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
			return true;
		}
		bool Blob(std::vector<std::uint8_t>& out, std::span<const std::uint8_t> value, std::string& error)
		{
			if (value.size() > kMaxBytes || !Put(out, value.size(), 4, error) || value.size() > kMaxBytes - out.size())
				return Fail(error, "byte limit");
			out.insert(out.end(), value.begin(), value.end()); return true;
		}
		bool Text(std::vector<std::uint8_t>& out, std::string_view value, std::string& error)
		{
			if (value.size() > 4096) return Fail(error, "text length limit");
			if (!Put(out, value.size(), 4, error) || value.size() > kMaxBytes - out.size()) return Fail(error, "byte limit");
			out.insert(out.end(), value.begin(), value.end()); return true;
		}
		// 전체 패킷의 framing을 사용자 코드 실행 전에 검사한다. 은퇴 타입은 생성하지 않는다.
		struct SType
		{
			std::string Atom;
			std::vector<SType> Args;
		};
		bool ParseType(std::string_view text, std::size_t& position, SType& type, std::size_t depth, std::string& error)
		{
			if (depth > kMaxDepth) return Fail(error, "signature depth limit");
			const auto begin = position;
			while (position < text.size() && ((text[position] >= 'a' && text[position] <= 'z') ||
				(text[position] >= 'A' && text[position] <= 'Z') || (text[position] >= '0' && text[position] <= '9') ||
				text[position] == '_')) ++position;
			type.Atom = text.substr(begin, position - begin);
			if (!Name(type.Atom)) return Fail(error, "invalid type signature");
			if (position < text.size() && text[position] == '<')
			{
				++position;
				for (;;)
				{
					if (type.Args.size() == 2) return Fail(error, "invalid signature arity");
					SType argument;
					if (!ParseType(text, position, argument, depth + 1, error)) return false;
					type.Args.push_back(std::move(argument));
					if (position >= text.size()) return Fail(error, "truncated signature");
					if (text[position++] == '>') break;
					if (text[position - 1] != ',') return Fail(error, "invalid signature separator");
				}
			}
			return true;
		}
		bool IsInteger(const SType& type)
		{ return type.Args.empty() && (type.Atom == "i8" || type.Atom == "u8" || type.Atom == "i16" || type.Atom == "u16" ||
			type.Atom == "i32" || type.Atom == "u32" || type.Atom == "i64" || type.Atom == "u64"); }
		bool IsKey(const SType& type)
		{ return IsInteger(type) || (type.Atom == "utf16" && type.Args.empty()) || (type.Atom == "enum" && type.Args.size() == 2); }
		bool CheckType(const SType& type, std::string& error)
		{
			const auto& kind = type.Atom;
			if (kind == "bool" || kind == "i8" || kind == "u8" || kind == "i16" || kind == "u16" ||
				kind == "i32" || kind == "u32" || kind == "i64" || kind == "u64" ||
				kind == "f32" || kind == "f64" || kind == "utf16") return type.Args.empty() || Fail(error, "invalid primitive signature");
			if (kind == "object") return (type.Args.size() == 1 && type.Args[0].Args.empty()) || Fail(error, "invalid object signature");
			if (kind == "enum") return (type.Args.size() == 2 && type.Args[0].Args.empty() && IsInteger(type.Args[1])) ||
				Fail(error, "invalid enum signature");
			if (kind == "unique" || kind == "shared" || kind == "vector")
			{
				if (type.Args.size() != 1 || !CheckType(type.Args[0], error)) return Fail(error, "invalid container signature");
				return kind != "shared" || type.Args[0].Atom == "object" || Fail(error, "shared pointer requires object");
			}
			if (kind == "map" || kind == "unordered_map")
				return (type.Args.size() == 2 && CheckType(type.Args[0], error) && CheckType(type.Args[1], error) && IsKey(type.Args[0])) ||
					Fail(error, "invalid map signature or key");
			return Fail(error, "unsupported field signature");
		}
		class FPacketValidator
		{
		public:
			explicit FPacketValidator(const std::vector<std::span<const std::uint8_t>>& objects)
				: m_objects(objects), m_names(objects.size() + 1), m_edges(objects.size() + 1),
				m_inlineDepth(objects.size() + 1, 1), m_state(objects.size() + 1), m_longest(objects.size() + 1) {}
			bool Run(std::span<const std::uint8_t> root, std::string& error)
			{
				for (std::size_t i = 0; i < m_objects.size(); ++i)
				{
					SCursor in{m_objects[i]};
					if (!in.Text(m_names[i + 1], error) || !Name(m_names[i + 1])) return Fail(error, "invalid shared object type");
				}
				if (!Object(root, {}, 0, 1, error)) return false;
				for (std::size_t i = 0; i < m_objects.size(); ++i)
					if (!Object(m_objects[i], m_names[i + 1], i + 1, 1, error)) return false;
				for (std::size_t i = 0; i < m_edges.size(); ++i) if (!Graph(i, 0, error)) return false;
				return true;
			}
		private:
			bool Graph(std::size_t index, std::size_t level, std::string& error)
			{
				if (level > kMaxDepth || m_state[index] == 1) return Fail(error, "owning shared cycle or graph depth limit");
				if (m_state[index] == 2) return true;
				m_state[index] = 1; std::size_t longest = m_inlineDepth[index];
				for (const auto& [target, weight] : m_edges[index])
				{
					if (!Graph(target, level + 1, error)) return false;
					longest = (std::max)(longest, weight + m_longest[target]);
				}
				if (longest > kMaxDepth) return Fail(error, "graph depth limit");
				m_longest[index] = longest; m_state[index] = 2; return true;
			}
			bool Object(std::span<const std::uint8_t> bytes, std::string_view expected, std::size_t owner,
				std::size_t depth, std::string& error)
			{
				if (depth > kMaxDepth || ++m_nodes > kMaxNodes || ++m_objectCount > kMaxObjects)
					return Fail(error, "object depth or count limit");
				m_inlineDepth[owner] = (std::max)(m_inlineDepth[owner], depth);
				SCursor in{bytes}; std::string name; std::uint64_t count = 0;
				if (!in.Text(name, error) || !Name(name) || (!expected.empty() && expected != name))
					return Fail(error, "object type mismatch");
				if (!in.Unsigned(4, count, error) || count > kMaxCount || count > (bytes.size() - in.Position) / 12)
					return Fail(error, "field count limit");
				std::set<std::string> names;
				for (std::uint64_t i = 0; i < count; ++i)
				{
					std::string field, signature; std::span<const std::uint8_t> payload;
					if (!in.Text(field, error) || !Name(field) || !names.emplace(field).second ||
						!in.Text(signature, error) || !in.Blob(payload, error))
						return Fail(error, "invalid or duplicate field framing");
					SType type; std::size_t position = 0;
					if (!ParseType(signature, position, type, 1, error) || position != signature.size() || !CheckType(type, error))
						return Fail(error, "invalid full field signature");
					SCursor value{payload};
					if (!Value(type, value, owner, depth, error) || !value.End(error)) return false;
				}
				return in.End(error);
			}
			bool Value(const SType& type, SCursor& in, std::size_t owner, std::size_t depth, std::string& error)
			{
				if (depth > kMaxDepth || ++m_nodes > kMaxNodes) return Fail(error, "value depth or count limit");
				m_inlineDepth[owner] = (std::max)(m_inlineDepth[owner], depth);
				const auto& kind = type.Atom;
				SContext dummy;
				if (kind == "utf16") { std::wstring text; return TCodec<std::wstring>::Read(dummy, in, text, error); }
				if (kind == "bool") { bool value = false; return TCodec<bool>::Read(dummy, in, value, error); }
				if (kind == "f32") { float value = 0; return TCodec<float>::Read(dummy, in, value, error); }
				if (kind == "f64") { double value = 0; return TCodec<double>::Read(dummy, in, value, error); }
				if (IsInteger(type))
				{
					const std::size_t width = kind.ends_with("8") ? 1 : kind.ends_with("16") ? 2 : kind.ends_with("32") ? 4 : 8;
					std::uint64_t value = 0; return in.Unsigned(width, value, error);
				}
				if (kind == "enum") return Value(type.Args[1], in, owner, depth, error);
				if (kind == "object")
				{
					const auto bytes = in.Bytes.subspan(in.Position); in.Position = in.Bytes.size();
					return Object(bytes, type.Args[0].Atom, owner, depth + 1, error);
				}
				if (kind == "shared")
				{
					std::uint64_t id = 0;
					if (!in.Unsigned(4, id, error) || id > m_objects.size()) return Fail(error, "invalid shared reference");
					if (id)
					{
						if (m_names[static_cast<std::size_t>(id)] != type.Args[0].Args[0].Atom)
							return Fail(error, "shared reference type mismatch");
						m_edges[owner].emplace_back(static_cast<std::size_t>(id), depth + 1);
					}
					return true;
				}
				if (kind == "unique")
				{
					std::uint64_t tag = 0;
					if (!in.Unsigned(1, tag, error) || tag > 1) return Fail(error, "invalid unique pointer tag");
					return !tag || Value(type.Args[0], in, owner, depth + 1, error);
				}
				std::uint64_t count = 0;
				const bool map = kind == "map" || kind == "unordered_map";
				if (!in.Unsigned(4, count, error) || count > kMaxCount || count > (in.Bytes.size() - in.Position) / (map ? 8 : 4))
					return Fail(error, "container limit or truncation");
				std::set<std::vector<std::uint8_t>> keys;
				for (std::uint64_t i = 0; i < count; ++i)
				{
					for (std::size_t part = 0; part < (map ? 2U : 1U); ++part)
					{
						std::span<const std::uint8_t> bytes;
						if (!in.Blob(bytes, error)) return false;
						SCursor element{bytes};
						if (!Value(type.Args[part], element, owner, depth + 1, error) || !element.End(error)) return false;
						if (map && part == 0 && !keys.emplace(bytes.begin(), bytes.end()).second) return Fail(error, "duplicate map key");
					}
				}
				return true;
			}
			const std::vector<std::span<const std::uint8_t>>& m_objects;
			std::vector<std::string> m_names;
			std::vector<std::vector<std::pair<std::size_t, std::size_t>>> m_edges;
			std::vector<std::size_t> m_inlineDepth;
			std::vector<int> m_state;
			std::vector<std::size_t> m_longest;
			std::size_t m_nodes = 0, m_objectCount = 0;
		};
		bool WriteObject(SContext& context, const ISerializable& value, std::vector<std::uint8_t>& out, std::string& error)
		{
			if (context.Depth >= kMaxDepth || ++context.Objects > kMaxObjects || ++context.Nodes > kMaxNodes)
				return Fail(error, "object depth or count limit");
			SDepthGuard guard(context);
			const auto name = value.GetSerializableTypeName();
			if (!Name(name)) return Fail(error, "invalid object type name");
			FBinaryWriter writer(context);
			if (!value.Serialize(writer, error)) return false;
			if (!Text(out, name, error) || !Put(out, writer.m_names.size(), 4, error)) return false;
			if (writer.m_bytes.size() > kMaxBytes - out.size()) return Fail(error, "byte limit");
			out.insert(out.end(), writer.m_bytes.begin(), writer.m_bytes.end()); return true;
		}
		bool ReadObject(SContext& context, std::span<const std::uint8_t> bytes, ISerializable& value, std::string& error)
		{
			if (context.Depth >= kMaxDepth || ++context.Objects > kMaxObjects || ++context.Nodes > kMaxNodes)
				return Fail(error, "object depth or count limit");
			SDepthGuard guard(context);
			SCursor in{bytes}; std::string name; std::uint64_t count = 0;
			if (!in.Text(name, error) || !Name(name) || name != value.GetSerializableTypeName()) return Fail(error, "object type mismatch");
			if (!in.Unsigned(4, count, error) || count > kMaxCount || count > (bytes.size() - in.Position) / 12)
				return Fail(error, "field count limit");
			FBinaryReader reader(context);
			for (std::uint64_t i = 0; i < count; ++i)
			{
				std::string fieldName, signature; std::span<const std::uint8_t> payload;
				if (!in.Text(fieldName, error) || !Name(fieldName) || !in.Text(signature, error) || !in.Blob(payload, error))
					return Fail(error, "invalid field framing");
				if (!reader.m_fields.emplace(fieldName, FBinaryReader::SField{signature, payload}).second)
					return Fail(error, "duplicate field name");
			}
			return in.End(error) && value.Deserialize(reader, error);
		}
	}
	bool FBinaryWriter::Reject(std::string& error, std::string_view reason)
	{ m_failed = true; return SerializationDetail::Fail(error, reason); }
	bool FBinaryReader::Reject(std::string& error, std::string_view reason)
	{ m_failed = true; return SerializationDetail::Fail(error, reason); }
	bool ISerializable::ForwardSerializeFields(const ISerializable& data, FBinaryWriter& writer, std::string& error)
	{
		if (!writer.m_running || !writer.m_fieldsOpen || writer.m_fieldRunning || writer.m_failed || writer.m_owner == &data)
			return writer.Reject(error, "field forwarding outside dispatch or reentrant owner");
		try
		{
			if (writer.m_forwarding.size() >= SerializationDetail::kMaxDepth || !writer.m_forwarding.emplace(&data).second)
				return writer.Reject(error, "field forwarding cycle or depth limit");
			SForwardGuard guard{writer.m_forwarding, &data};
			const bool result = data.SerializeFields(writer, error);
			return (result && !writer.m_failed) || writer.Reject(error, "forwarded field serialization rejected");
		}
		catch (...) { return writer.Reject(error, "forwarded field serialization exception"); }
	}
	bool ISerializable::ForwardDeserializeFields(ISerializable& data, FBinaryReader& reader, std::string& error)
	{
		if (!reader.m_running || !reader.m_fieldsOpen || reader.m_fieldRunning || reader.m_failed || reader.m_owner == &data)
			return reader.Reject(error, "field forwarding outside dispatch or reentrant owner");
		try
		{
			if (reader.m_forwarding.size() >= SerializationDetail::kMaxDepth || !reader.m_forwarding.emplace(&data).second)
				return reader.Reject(error, "field forwarding cycle or depth limit");
			SForwardGuard guard{reader.m_forwarding, &data};
			const bool result = data.DeserializeFields(reader, error);
			return (result && !reader.m_failed) || reader.Reject(error, "forwarded field deserialization rejected");
		}
		catch (...) { return reader.Reject(error, "forwarded field deserialization exception"); }
	}
	bool ISerializable::Serialize(FBinaryWriter& writer, std::string& error) const
	{
		if (writer.m_running || writer.m_failed) return writer.Reject(error, "reentrant object serialization");
		writer.m_running = true;
		writer.m_owner = this;
		try
		{
			if (!OnBeforeSerialize(error)) return writer.Reject(error, "before hook rejected");
			writer.m_fieldsOpen = true;
			const bool result = SerializeFields(writer, error);
			writer.m_fieldsOpen = false;
			return (result && !writer.m_failed) || writer.Reject(error, "field serialization rejected");
		}
		catch (...) { writer.m_fieldsOpen = false; return writer.Reject(error, "serialization callback exception"); }
	}
	bool ISerializable::Deserialize(FBinaryReader& reader, std::string& error)
	{
		if (reader.m_running || reader.m_failed) return reader.Reject(error, "reentrant object deserialization");
		reader.m_running = true;
		reader.m_owner = this;
		try
		{
			reader.m_fieldsOpen = true;
			const bool result = DeserializeFields(reader, error);
			reader.m_fieldsOpen = false;
			if (!result || reader.m_failed) return reader.Reject(error, "field deserialization rejected");
			return (OnAfterDeserialize(error) && !reader.m_failed) || reader.Reject(error, "after hook rejected");
		}
		catch (...) { reader.m_fieldsOpen = false; return reader.Reject(error, "deserialization callback exception"); }
	}
	bool FSerializationArchive::Serialize(const ISerializable& source, std::vector<std::uint8_t>& output, std::string& error,
		std::uint32_t version, const FSerializableTypeRegistry* registry)
	{
		using namespace SerializationDetail;
		if (g_archive)
		{
			g_archive->Failed = true;
			return Fail(error, "reentrant archive call");
		}
		SArchiveGuard guard;
		error.clear();
		try
		{
			FSerializableTypeRegistry::SPin pin(registry);
			std::vector<std::uint8_t> bytes{'C', 'G', 'S', 'A'};
			const bool result = [&]
			{
				if (version == 0) return Fail(error, "invalid schema version");
				SContext context; context.Version = version; context.Registry = registry;
				context.Ownership.emplace(&source, false);
				std::vector<std::uint8_t> payload;
				if (!WriteObject(context, source, payload, error) || !Put(bytes, 1, 4, error) || !Put(bytes, version, 4, error) ||
					!Blob(bytes, payload, error) || !Put(bytes, context.SharedWrite.size(), 4, error)) return false;
				for (const auto& entry : context.SharedWrite) if (!Blob(bytes, entry.Bytes, error)) return false;
				std::vector<std::span<const std::uint8_t>> objects;
				for (const auto& entry : context.SharedWrite) objects.emplace_back(entry.Bytes);
				FPacketValidator validator(objects);
				return validator.Run(payload, error);
			}();
			// 문맥과 임시 객체의 가상 소멸까지 끝난 뒤에만 성공을 게시한다.
			if (guard.Failed) return Fail(error, "reentrant archive call");
			if (!result) return false;
			output.swap(bytes); return true;
		}
		catch (...) { return Fail(error, "archive serialization exception"); }
	}
	bool FSerializationArchive::Deserialize(const std::vector<std::uint8_t>& bytes, ISerializable& candidate, std::string& error,
		std::uint32_t version, const FSerializableTypeRegistry* registry)
	{
		using namespace SerializationDetail;
		if (g_archive)
		{
			g_archive->Failed = true;
			return Fail(error, "reentrant archive call");
		}
		SArchiveGuard guard;
		error.clear();
		try
		{
			FSerializableTypeRegistry::SPin pin(registry);
			const bool result = [&]
			{
				if (bytes.size() > kMaxBytes || bytes.size() < 16 || bytes[0] != 'C' || bytes[1] != 'G' || bytes[2] != 'S' || bytes[3] != 'A')
					return Fail(error, "invalid archive magic or byte limit");
				SCursor in{bytes, 4}; std::uint64_t format = 0, actualVersion = 0; std::span<const std::uint8_t> payload;
				if (!in.Unsigned(4, format, error) || format != 1 || !in.Unsigned(4, actualVersion, error) || actualVersion == 0 ||
					actualVersion > version) return Fail(error, "unsupported archive or newer schema version");
				if (!in.Blob(payload, error)) return false;
				SContext context; context.Version = static_cast<std::uint32_t>(actualVersion); context.Registry = registry;
				std::uint64_t sharedCount = 0;
				if (!in.Unsigned(4, sharedCount, error) || sharedCount > kMaxCount || sharedCount > (bytes.size() - in.Position) / 4)
					return Fail(error, "shared object table limit");
				context.SharedRead.reserve(static_cast<std::size_t>(sharedCount));
				for (std::uint64_t i = 0; i < sharedCount; ++i)
				{
					std::span<const std::uint8_t> object;
					if (!in.Blob(object, error)) return false;
					context.SharedRead.push_back({object, {}, 0});
				}
				if (!in.End(error)) return false;
				std::vector<std::span<const std::uint8_t>> objects;
				for (const auto& entry : context.SharedRead) objects.push_back(entry.Bytes);
				FPacketValidator validator(objects);
				if (!validator.Run(payload, error)) return false;
				return ReadObject(context, payload, candidate, error);
			}();
			if (guard.Failed) return Fail(error, "reentrant archive call");
			return result;
		}
		catch (...) { return Fail(error, "archive deserialization exception"); }
	}
}
