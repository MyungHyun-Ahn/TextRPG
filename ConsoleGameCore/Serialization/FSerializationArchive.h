#pragma once

namespace TextRPG
{
	template<class E> struct TSerializationEnumTraits;
	class FSerializableTypeRegistry;
	namespace SerializationDetail
	{
		inline constexpr std::size_t kMaxBytes = 1024 * 1024, kMaxCount = 4096, kMaxString = 32768, kMaxDepth = 64;
		inline constexpr std::size_t kMaxObjects = 8192, kMaxNodes = 65536;
		bool Fail(std::string&, std::string_view);
		template<class T> concept TRegisteredEnum = std::is_enum_v<T> && requires(T value)
		{
			TSerializationEnumTraits<T>::kTypeName;
			TSerializationEnumTraits<T>::IsValid(value);
		};
		struct SContext
		{
			std::uint32_t Version = 1;
			const FSerializableTypeRegistry* Registry = nullptr;
			std::size_t Depth = 0, Nodes = 0, Objects = 0, SharedBytes = 0;
			struct SSharedWrite
			{
				std::shared_ptr<const ISerializable> Owner;
				std::vector<std::uint8_t> Bytes;
				bool Complete = false;
			};
			struct SSharedRead
			{
				std::span<const std::uint8_t> Bytes;
				std::shared_ptr<ISerializable> Object;
				int State = 0;
			};
			std::vector<SSharedWrite> SharedWrite;
			std::vector<SSharedRead> SharedRead;
			std::map<const ISerializable*, bool> Ownership;
		};
		struct SFieldGuard
		{
			bool& Busy;
			explicit SFieldGuard(bool& busy) : Busy(busy) { Busy = true; }
			~SFieldGuard() { Busy = false; }
		};
		struct SValueGuard
		{
			SContext& Context;
			explicit SValueGuard(SContext& context) : Context(context) { ++Context.Depth; }
			~SValueGuard() { --Context.Depth; }
			bool Check(std::string& error)
			{
				return (Context.Depth <= kMaxDepth && ++Context.Nodes <= kMaxNodes) || Fail(error, "value depth or count limit");
			}
		};
		template<class T> std::unique_ptr<T> CreateConcrete(SContext&, std::string&);
		inline bool Own(SContext& context, const ISerializable* pointer, bool shared, std::string& error)
		{
			const auto [entry, added] = context.Ownership.emplace(pointer, shared);
			return added || (shared && entry->second) || Fail(error, "mixed or repeated exclusive ownership");
		};
		struct SCursor
		{
			std::span<const std::uint8_t> Bytes;
			std::size_t Position = 0;
			bool Read(std::size_t, std::span<const std::uint8_t>&, std::string&);
			bool Unsigned(std::size_t, std::uint64_t&, std::string&);
			bool Text(std::string&, std::string&);
			bool Blob(std::span<const std::uint8_t>&, std::string&);
			bool End(std::string&) const;
		};
		bool Put(std::vector<std::uint8_t>&, std::uint64_t, std::size_t, std::string&);
		bool Text(std::vector<std::uint8_t>&, std::string_view, std::string&);
		bool Blob(std::vector<std::uint8_t>&, std::span<const std::uint8_t>, std::string&);
		bool Name(std::string_view);
		bool Utf16(std::wstring_view);
		bool WriteObject(SContext&, const ISerializable&, std::vector<std::uint8_t>&, std::string&);
		bool ReadObject(SContext&, std::span<const std::uint8_t>, ISerializable&, std::string&);
		template<class T> struct TCodec
		{
			static std::string Signature()
			{
				if constexpr (std::is_same_v<T, bool>) return "bool";
				else if constexpr (std::is_same_v<T, std::int8_t>) return "i8";
				else if constexpr (std::is_same_v<T, std::uint8_t>) return "u8";
				else if constexpr (std::is_same_v<T, std::int16_t>) return "i16";
				else if constexpr (std::is_same_v<T, std::uint16_t>) return "u16";
				else if constexpr (std::is_same_v<T, std::int32_t>) return "i32";
				else if constexpr (std::is_same_v<T, std::uint32_t>) return "u32";
				else if constexpr (std::is_same_v<T, std::int64_t>) return "i64";
				else if constexpr (std::is_same_v<T, std::uint64_t>) return "u64";
				else if constexpr (std::is_same_v<T, float>) return "f32";
				else if constexpr (std::is_same_v<T, double>) return "f64";
				else if constexpr (std::is_same_v<T, std::wstring>) return "utf16";
				else if constexpr (TRegisteredEnum<T>)
					return "enum<" + std::string(TSerializationEnumTraits<T>::kTypeName) + "," +
						TCodec<std::underlying_type_t<T>>::Signature() + ">";
				else if constexpr (std::is_base_of_v<ISerializable, T>) return "object<" + std::string(T::kTypeName) + ">";
				else return "unsupported";
			}
			static bool Write(SContext& context, const T& value, std::vector<std::uint8_t>& out, std::string& error)
			{
				if constexpr (std::is_same_v<T, bool>) return Put(out, value ? 1 : 0, 1, error);
				else if constexpr (std::is_integral_v<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8))
					return Put(out, static_cast<std::make_unsigned_t<T>>(value), sizeof(T), error);
				else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
				{
					if (!std::isfinite(value)) return Fail(error, "non-finite float");
					if constexpr (sizeof(T) == 4) return Put(out, std::bit_cast<std::uint32_t>(value), 4, error);
					else return Put(out, std::bit_cast<std::uint64_t>(value), 8, error);
				}
				else if constexpr (std::is_same_v<T, std::wstring>)
				{
					if (value.size() > kMaxString || !Utf16(value)) return Fail(error, "invalid UTF16 or string limit");
					if (!Put(out, value.size(), 4, error)) return false;
					for (const auto unit : value) if (!Put(out, static_cast<std::uint16_t>(unit), 2, error)) return false;
					return true;
				}
				else if constexpr (std::is_base_of_v<ISerializable, T>)
				{
					if (value.GetSerializableTypeName() != T::kTypeName) return Fail(error, "declared object type mismatch");
					return Own(context, &value, false, error) && WriteObject(context, value, out, error);
				}
				else if constexpr (TRegisteredEnum<T>)
				{
					if (!Name(TSerializationEnumTraits<T>::kTypeName) || !TSerializationEnumTraits<T>::IsValid(value))
						return Fail(error, "invalid enum value");
					return TCodec<std::underlying_type_t<T>>::Write(context, static_cast<std::underlying_type_t<T>>(value), out, error);
				}
				else return Fail(error, "unsupported value type");
			}
			static bool Read(SContext& context, SCursor& in, T& value, std::string& error)
			{
				if constexpr (std::is_same_v<T, bool>)
				{
					std::uint64_t bits = 0;
					if (!in.Unsigned(1, bits, error)) return false;
					if (bits > 1) return Fail(error, "invalid bool");
					value = bits != 0; return true;
				}
				else if constexpr (std::is_integral_v<T> && (sizeof(T) == 1 || sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8))
				{
					std::uint64_t bits = 0;
					if (!in.Unsigned(sizeof(T), bits, error)) return false;
					value = std::bit_cast<T>(static_cast<std::make_unsigned_t<T>>(bits)); return true;
				}
				else if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
				{
					std::uint64_t bits = 0;
					if (!in.Unsigned(sizeof(T), bits, error)) return false;
					if constexpr (sizeof(T) == 4) value = std::bit_cast<float>(static_cast<std::uint32_t>(bits));
					else value = std::bit_cast<double>(bits);
					return std::isfinite(value) || Fail(error, "non-finite float");
				}
				else if constexpr (std::is_same_v<T, std::wstring>)
				{
					std::uint64_t size = 0;
					if (!in.Unsigned(4, size, error)) return false;
					if (size > kMaxString || size > (in.Bytes.size() - in.Position) / 2) return Fail(error, "string limit or truncation");
					std::wstring next; next.reserve(static_cast<std::size_t>(size));
					for (std::size_t i = 0; i < size; ++i)
					{
						std::uint64_t unit = 0;
						if (!in.Unsigned(2, unit, error)) return false;
						next.push_back(static_cast<wchar_t>(unit));
					}
					if (!Utf16(next)) return Fail(error, "invalid UTF16");
					value = std::move(next); return true;
				}
				else if constexpr (std::is_base_of_v<ISerializable, T>)
				{
					const auto remaining = in.Bytes.subspan(in.Position); in.Position = in.Bytes.size();
					auto next = CreateConcrete<T>(context, error);
					if (!next || !ReadObject(context, remaining, *next, error)) return false;
					value = std::move(*next); return true;
				}
				else if constexpr (TRegisteredEnum<T>)
				{
					std::underlying_type_t<T> bits{};
					if (!TCodec<std::underlying_type_t<T>>::Read(context, in, bits, error)) return false;
					const auto next = static_cast<T>(bits);
					if (!TSerializationEnumTraits<T>::IsValid(next)) return Fail(error, "invalid enum value");
					value = next; return true;
				}
				else return Fail(error, "unsupported value type");
			}
		};

		template<class T> bool WriteElement(SContext& context, const T& value, std::vector<std::uint8_t>& out, std::string& error)
		{
			std::vector<std::uint8_t> bytes;
			return TCodec<T>::Write(context, value, bytes, error) && Blob(out, bytes, error);
		}
		template<class T> bool ReadElement(SContext& context, SCursor& in, T& value, std::string& error)
		{
			std::span<const std::uint8_t> bytes;
			if (!in.Blob(bytes, error)) return false;
			SCursor element{bytes};
			return TCodec<T>::Read(context, element, value, error) && element.End(error);
		}
		template<class T> struct TCodec<std::vector<T>>
		{
			static std::string Signature() { return "vector<" + TCodec<T>::Signature() + ">"; }
			static bool Write(SContext& context, const std::vector<T>& value, std::vector<std::uint8_t>& out, std::string& error)
			{
				SValueGuard guard(context);
				if (!guard.Check(error) || value.size() > kMaxCount) return Fail(error, "container limit");
				if (!Put(out, value.size(), 4, error)) return false;
				for (std::size_t i = 0; i < value.size(); ++i)
				{
					if constexpr (std::is_same_v<T, bool>) { if (!WriteElement(context, bool(value[i]), out, error)) return false; }
					else if (!WriteElement(context, value[i], out, error)) return false;
				}
				return true;
			}
			static bool Read(SContext& context, SCursor& in, std::vector<T>& value, std::string& error)
			{
				SValueGuard guard(context); std::uint64_t count = 0;
				if (!guard.Check(error) || !in.Unsigned(4, count, error) || count > kMaxCount ||
					count > (in.Bytes.size() - in.Position) / 4)
					return Fail(error, "container limit or truncation");
				std::vector<T> next; next.reserve(static_cast<std::size_t>(count));
				for (std::uint64_t i = 0; i < count; ++i)
				{
					T element{};
					if (!ReadElement(context, in, element, error)) return false;
					next.push_back(std::move(element));
				}
				value = std::move(next); return true;
			}
		};
		template<class K> constexpr bool kKey = std::is_same_v<K, std::wstring> ||
			std::is_same_v<K, std::int32_t> || std::is_same_v<K, std::uint32_t> ||
			std::is_same_v<K, std::int64_t> || std::is_same_v<K, std::uint64_t> || std::is_enum_v<K>;
		template<class M> struct TMapCodec
		{
			using K = typename M::key_type;
			using V = typename M::mapped_type;
			static std::string Signature() { return "map<" + TCodec<K>::Signature() + "," + TCodec<V>::Signature() + ">"; }
			static bool Write(SContext& context, const M& value, std::vector<std::uint8_t>& out, std::string& error)
			{
				if constexpr (!kKey<K>) return Fail(error, "unsupported map key");
				else
				{
					SValueGuard guard(context);
					if (!guard.Check(error) || value.size() > kMaxCount) return Fail(error, "container limit");
					std::vector<const typename M::value_type*> ordered;
					for (const auto& entry : value) ordered.push_back(&entry);
					std::sort(ordered.begin(), ordered.end(), [](const auto* a, const auto* b) { return a->first < b->first; });
					if (!Put(out, value.size(), 4, error)) return false;
					for (const auto* entry : ordered)
						if (!WriteElement(context, entry->first, out, error) ||
							!WriteElement(context, entry->second, out, error)) return false;
					return true;
				}
			}
			static bool Read(SContext& context, SCursor& in, M& value, std::string& error)
			{
				if constexpr (!kKey<K>) return Fail(error, "unsupported map key");
				else
				{
					SValueGuard guard(context); std::uint64_t count = 0;
					if (!guard.Check(error) || !in.Unsigned(4, count, error) || count > kMaxCount ||
						count > (in.Bytes.size() - in.Position) / 8)
						return Fail(error, "container limit or truncation");
					M next;
					for (std::uint64_t i = 0; i < count; ++i)
					{
						K key{}; V element{};
						if (!ReadElement(context, in, key, error) || !ReadElement(context, in, element, error)) return false;
						if (!next.emplace(std::move(key), std::move(element)).second) return Fail(error, "duplicate map key");
					}
					value = std::move(next); return true;
				}
			}
		};
		template<class K, class V> struct TCodec<std::map<K, V>> : TMapCodec<std::map<K, V>> {};
		template<class K, class V> struct TCodec<std::unordered_map<K, V>> : TMapCodec<std::unordered_map<K, V>>
		{
			static std::string Signature() { return "unordered_map<" + TCodec<K>::Signature() + "," + TCodec<V>::Signature() + ">"; }
		};
		template<class T> struct TCodec<std::unique_ptr<T>>
		{
			static std::string Signature() { return "unique<" + TCodec<T>::Signature() + ">"; }
			static bool Write(SContext& context, const std::unique_ptr<T>& value, std::vector<std::uint8_t>& out, std::string& error)
			{
				SValueGuard guard(context);
				return guard.Check(error) && Put(out, value ? 1 : 0, 1, error) && (!value || TCodec<T>::Write(context, *value, out, error));
			}
			static bool Read(SContext& context, SCursor& in, std::unique_ptr<T>& value, std::string& error)
			{
				SValueGuard guard(context); std::uint64_t present = 0;
				if (!guard.Check(error) || !in.Unsigned(1, present, error) || present > 1) return Fail(error, "invalid unique pointer tag");
				if (!present) { value.reset(); return true; }
				auto next = CreateConcrete<T>(context, error);
				if (!next) return false;
				if constexpr (std::is_base_of_v<ISerializable, T>)
				{
					const auto remaining = in.Bytes.subspan(in.Position); in.Position = in.Bytes.size();
					if (!ReadObject(context, remaining, *next, error)) return false;
				}
				else if (!TCodec<T>::Read(context, in, *next, error)) return false;
				value = std::move(next); return true;
			}
		};
		template<class T> struct TCodec<std::shared_ptr<T>>
		{
			static std::string Signature() { return "shared<" + TCodec<T>::Signature() + ">"; }
			static bool Write(SContext& context, const std::shared_ptr<T>& value, std::vector<std::uint8_t>& out, std::string& error)
			{
				if constexpr (!std::is_base_of_v<ISerializable, T>) return Fail(error, "shared pointer requires serializable object");
				else
				{
					SValueGuard guard(context);
					if (!guard.Check(error)) return false;
					if (!value) return Put(out, 0, 4, error);
					if (value.use_count() == 0 || !Own(context, value.get(), true, error))
						return Fail(error, "shared pointer without ownership");
					std::shared_ptr<const ISerializable> owner = value;
					for (std::size_t i = 0; i < context.SharedWrite.size(); ++i)
					{
						const auto& entry = context.SharedWrite[i];
						const bool sameOwner = !entry.Owner.owner_before(owner) && !owner.owner_before(entry.Owner);
						if (sameOwner != (entry.Owner.get() == owner.get())) return Fail(error, "aliased or conflicting shared ownership");
						if (sameOwner)
						{
							if (!entry.Complete) return Fail(error, "owning shared cycle unsupported");
							return Put(out, i + 1, 4, error);
						}
					}
					if (context.SharedWrite.size() >= kMaxCount || value->GetSerializableTypeName() != T::kTypeName)
						return Fail(error, "shared object count or type mismatch");
					const auto index = context.SharedWrite.size();
					context.SharedWrite.push_back({owner, {}, false});
					std::vector<std::uint8_t> bytes;
					if (!WriteObject(context, *value, bytes, error)) return false;
					if (bytes.size() > kMaxBytes - context.SharedBytes) return Fail(error, "aggregate shared byte limit");
					context.SharedBytes += bytes.size();
					context.SharedWrite[index].Bytes = std::move(bytes); context.SharedWrite[index].Complete = true;
					return Put(out, index + 1, 4, error);
				}
			}
			static bool Read(SContext& context, SCursor& in, std::shared_ptr<T>& value, std::string& error)
			{
				if constexpr (!std::is_base_of_v<ISerializable, T>) return Fail(error, "shared pointer requires serializable object");
				else
				{
					SValueGuard guard(context); std::uint64_t id = 0;
					if (!guard.Check(error) || !in.Unsigned(4, id, error) || id > context.SharedRead.size())
						return Fail(error, "invalid shared reference");
					if (!id) { value.reset(); return true; }
					auto& entry = context.SharedRead[static_cast<std::size_t>(id - 1)];
					if (entry.State == 1) return Fail(error, "owning shared cycle unsupported");
					if (entry.State == 0)
					{
						entry.State = 1;
						auto next = CreateConcrete<T>(context, error);
						if (!next || !ReadObject(context, entry.Bytes, *next, error)) return false;
						entry.Object = std::move(next); entry.State = 2;
					}
					auto next = std::dynamic_pointer_cast<T>(entry.Object);
					if (!next || next->GetSerializableTypeName() != T::kTypeName) return Fail(error, "shared reference type mismatch");
					value = std::move(next); return true;
				}
			}
		};
	}
	class FSerializableTypeRegistry
	{
	public:
		FSerializableTypeRegistry() = default;
		FSerializableTypeRegistry(const FSerializableTypeRegistry&) = delete;
		FSerializableTypeRegistry& operator=(const FSerializableTypeRegistry&) = delete;
		template<class T> bool Register(std::string_view name, std::function<std::unique_ptr<T>()> factory, std::string& error)
		{ return Bind<T>(name, std::move(factory), error, false); }
		template<class T> bool Replace(std::string_view name, std::function<std::unique_ptr<T>()> factory, std::string& error)
		{ return Bind<T>(name, std::move(factory), error, true); }
	private:
		friend class FSerializationArchive;
		template<class T> friend std::unique_ptr<T> SerializationDetail::CreateConcrete(SerializationDetail::SContext&, std::string&);
		struct SEntry
		{
			const void* Token;
			std::function<std::unique_ptr<ISerializable>()> Factory;
		};
		struct SPin
		{
			const FSerializableTypeRegistry* Registry;
			explicit SPin(const FSerializableTypeRegistry* registry) : Registry(registry) { if (Registry) ++Registry->m_uses; }
			~SPin() { if (Registry) --Registry->m_uses; }
		};
		template<class T> static inline const char kToken = 0;
		template<class T> bool Bind(std::string_view name, std::function<std::unique_ptr<T>()> factory, std::string& error, bool replace)
		{
			static_assert(std::is_base_of_v<ISerializable, T>);
			using namespace SerializationDetail;
			error.clear();
			try
			{
				if (m_uses != 0 || !Name(name) || name != T::kTypeName || !factory) return Fail(error, "invalid or busy registry binding");
				const auto found = m_entries.find(name);
				if ((!replace && found != m_entries.end()) || (replace && (found == m_entries.end() || found->second.Token != &kToken<T>)))
					return Fail(error, "duplicate or incompatible registry binding");
				SEntry entry{&kToken<T>, [factory = std::move(factory)]() -> std::unique_ptr<ISerializable> { return factory(); }};
				if (replace) found->second = std::move(entry);
				else m_entries.emplace(std::string(name), std::move(entry));
				return true;
			}
			catch (...) { return Fail(error, "registry binding exception"); }
		}
		std::map<std::string, SEntry, std::less<>> m_entries;
		mutable std::size_t m_uses = 0;
	};
	namespace SerializationDetail
	{
		template<class T> std::unique_ptr<T> CreateConcrete(SContext& context, std::string& error)
		{
			if constexpr (!std::is_base_of_v<ISerializable, T>) return std::make_unique<T>();
			else
			{
				if (context.Registry)
				{
					const auto found = context.Registry->m_entries.find(T::kTypeName);
					if (found != context.Registry->m_entries.end())
					{
						if (found->second.Token != &FSerializableTypeRegistry::kToken<T>)
						{ Fail(error, "registry declared type mismatch"); return {}; }
						auto erased = found->second.Factory();
						auto* typed = dynamic_cast<T*>(erased.get());
						if (!typed || typed->GetSerializableTypeName() != T::kTypeName)
						{ Fail(error, "registry factory null or type mismatch"); return {}; }
						(void)erased.release(); return std::unique_ptr<T>(typed);
					}
				}
				return std::make_unique<T>();
			}
		}
	}
	class FBinaryWriter
	{
	public:
		template<class T> bool Field(std::string_view name, const T& value, std::string& error)
		{
			try
			{
				if (!m_fieldsOpen || m_failed || m_fieldRunning) return Reject(error, "writer field reentry or outside field dispatch");
				SerializationDetail::SFieldGuard guard(m_fieldRunning);
				if (!SerializationDetail::Name(name) || !m_names.emplace(name).second || m_names.size() > SerializationDetail::kMaxCount)
					return Reject(error, "invalid or duplicate field name");
				std::vector<std::uint8_t> payload;
				if (!SerializationDetail::TCodec<T>::Write(m_context, value, payload, error)) return Reject(error, name);
				if (!SerializationDetail::Text(m_bytes, name, error) ||
					!SerializationDetail::Text(m_bytes, SerializationDetail::TCodec<T>::Signature(), error) ||
					!SerializationDetail::Blob(m_bytes, payload, error)) return Reject(error, name);
				return true;
			}
			catch (...) { return Reject(error, "writer field exception"); }
		}
	private:
		friend class ISerializable;
		friend bool SerializationDetail::WriteObject(SerializationDetail::SContext&, const ISerializable&,
			std::vector<std::uint8_t>&, std::string&);
		explicit FBinaryWriter(SerializationDetail::SContext& context) : m_context(context) {}
		bool Reject(std::string&, std::string_view);
		SerializationDetail::SContext& m_context;
		std::vector<std::uint8_t> m_bytes;
		std::set<std::string, std::less<>> m_names;
		std::set<const ISerializable*> m_forwarding;
		const ISerializable* m_owner = nullptr;
		bool m_fieldsOpen = false, m_running = false, m_failed = false, m_fieldRunning = false;
	};
	class FBinaryReader
	{
	public:
		std::uint32_t GetSchemaVersion() const noexcept { return m_context.Version; }
		template<class T> bool Field(std::string_view name, T& value, std::string& error, bool required = true)
		{
			try
			{
				if (!m_fieldsOpen || m_failed || m_fieldRunning) return Reject(error, "reader field reentry or outside field dispatch");
				SerializationDetail::SFieldGuard guard(m_fieldRunning);
				const auto found = m_fields.find(name);
				if (found == m_fields.end()) return !required || Reject(error, "missing required field");
				auto& field = found->second;
				if (field.Read || field.Signature != SerializationDetail::TCodec<T>::Signature())
					return Reject(error, "duplicate request or field type mismatch");
				SerializationDetail::SCursor in{field.Bytes};
				if (!SerializationDetail::TCodec<T>::Read(m_context, in, value, error) || !in.End(error)) return Reject(error, name);
				field.Read = true; return true;
			}
			catch (...) { return Reject(error, "reader field exception"); }
		}
	private:
		friend class ISerializable;
		friend bool SerializationDetail::ReadObject(SerializationDetail::SContext&, std::span<const std::uint8_t>,
			ISerializable&, std::string&);
		struct SField { std::string Signature; std::span<const std::uint8_t> Bytes; bool Read = false; };
		explicit FBinaryReader(SerializationDetail::SContext& context) : m_context(context) {}
		bool Reject(std::string&, std::string_view);
		SerializationDetail::SContext& m_context;
		std::map<std::string, SField, std::less<>> m_fields;
		std::set<const ISerializable*> m_forwarding;
		const ISerializable* m_owner = nullptr;
		bool m_fieldsOpen = false, m_running = false, m_failed = false, m_fieldRunning = false;
	};
	class FSerializationArchive
	{
	public:
		static bool Serialize(const ISerializable&, std::vector<std::uint8_t>&, std::string&, std::uint32_t schemaVersion = 1,
			const FSerializableTypeRegistry* registry = nullptr);
		static bool Deserialize(const std::vector<std::uint8_t>&, ISerializable&, std::string&, std::uint32_t schemaVersion = 1,
			const FSerializableTypeRegistry* registry = nullptr);
	};
}
