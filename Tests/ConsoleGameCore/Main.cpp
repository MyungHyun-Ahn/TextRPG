#include "pch.h"
#ifdef CORE_DEPENDENCY_NEGATIVE
#include "Common/GameDataEnums.g.h"
#endif
#include "Serialization/FSerializationArchive.h"
#include "Rendering/FFrameBuffer.h"
#include "Input/FInputManager.h"
#include "UI/Common/FUiElement.h"
#include "UI/Common/FUiPanel.h"
#include "UI/Common/FUiButton.h"
#include "UI/Common/FSceneUiRoot.h"
#include "UI/FUIManager.h"
#include "Scene/TScene.h"
#include "Scene/FSceneHost.h"
#include "Frame/FrameTypes.h"
#include "Frame/IFrameClock.h"
#include "Frame/FFrame.h"
#include <cassert>
#include <cstdlib>
#include <crtdbg.h>

enum class EFixtureMood : std::int32_t { Calm = 2, Alert = 7 };
enum class EWidthI8 : std::int8_t { Low = -128, High = 127 };
enum class EWidthU8 : std::uint8_t { Low = 0, High = 255 };
enum class EWidthI16 : std::int16_t { Low = -32768, High = 32767 };
enum class EWidthU16 : std::uint16_t { Low = 0, High = 65535 };
enum class EWidthU32 : std::uint32_t { Low = 0, High = 0xffffffffU };
enum class EWidthI64 : std::int64_t { Low = (-9223372036854775807LL - 1), High = 9223372036854775807LL };
enum class EWidthU64 : std::uint64_t { Low = 0, High = 0xffffffffffffffffULL };
namespace TextRPG
{
	template<class E> struct TFixtureWidthTraits
	{
		static bool IsValid(E value) noexcept { return value == E::Low || value == E::High; }
	};
	template<> struct TSerializationEnumTraits<EWidthI8> : TFixtureWidthTraits<EWidthI8>
	{ static constexpr std::string_view kTypeName = "EWidthI8"; };
	template<> struct TSerializationEnumTraits<EWidthU8> : TFixtureWidthTraits<EWidthU8>
	{ static constexpr std::string_view kTypeName = "EWidthU8"; };
	template<> struct TSerializationEnumTraits<EWidthI16> : TFixtureWidthTraits<EWidthI16>
	{ static constexpr std::string_view kTypeName = "EWidthI16"; };
	template<> struct TSerializationEnumTraits<EWidthU16> : TFixtureWidthTraits<EWidthU16>
	{ static constexpr std::string_view kTypeName = "EWidthU16"; };
	template<> struct TSerializationEnumTraits<EWidthU32> : TFixtureWidthTraits<EWidthU32>
	{ static constexpr std::string_view kTypeName = "EWidthU32"; };
	template<> struct TSerializationEnumTraits<EWidthI64> : TFixtureWidthTraits<EWidthI64>
	{ static constexpr std::string_view kTypeName = "EWidthI64"; };
	template<> struct TSerializationEnumTraits<EWidthU64> : TFixtureWidthTraits<EWidthU64>
	{ static constexpr std::string_view kTypeName = "EWidthU64"; };
	template<> struct TSerializationEnumTraits<EFixtureMood>
	{
		static constexpr std::string_view kTypeName = "EFixtureMood";
		static bool IsValid(EFixtureMood value) noexcept { return value == EFixtureMood::Calm || value == EFixtureMood::Alert; }
	};
}
using namespace TextRPG;
namespace
{
	struct SScalar : ISerializable
	{
		static constexpr std::string_view kTypeName = "SScalar";
		std::int32_t Number = -17;
		std::wstring Text = L"가\U0001F600";
		mutable int Before = 0;
		int After = 0;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ return w.Field("Number", Number, e) && w.Field("Text", Text, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{ return r.Field("Number", Number, e) && r.Field("Text", Text, e); }
		bool OnBeforeSerialize(std::string&) const override { ++Before; return true; }
		bool OnAfterDeserialize(std::string&) override { ++After; return true; }
	};

	void TestSerializationScalar()
	{
		SScalar source, candidate;
		candidate.Number = 0; candidate.Text.clear();
		std::vector<std::uint8_t> bytes = {91};
		std::string error;
		const bool written = FSerializationArchive::Serialize(source, bytes, error);
		std::cout << "TRACE SERIAL-01 written=" << written << " error=" << error << '\n';
		assert(written);
		assert(FSerializationArchive::Deserialize(bytes, candidate, error));
		assert(candidate.Number == -17 && candidate.Text == source.Text && source.Before == 1 && candidate.After == 1);
		std::cout << "PASS SERIAL-01 scalar fieldwise UTF16 hooks roundtrip\n";
	}

	struct SNode : ISerializable
	{
		static constexpr std::string_view kTypeName = "SNode";
		std::int32_t Value = 0;
		std::shared_ptr<SNode> Next;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ return w.Field("Value", Value, e) && w.Field("Next", Next, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{ return r.Field("Next", Next, e) && r.Field("Value", Value, e); }
	};
	struct SGraph : ISerializable
	{
		static constexpr std::string_view kTypeName = "SGraph";
		std::vector<std::map<std::int32_t, std::vector<std::shared_ptr<SNode>>>> Nested;
		std::unordered_map<std::wstring, std::shared_ptr<SNode>> Lookup;
		std::vector<SScalar> Values;
		std::unique_ptr<SNode> Unique;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ return w.Field("Nested", Nested, e) && w.Field("Lookup", Lookup, e) && w.Field("Values", Values, e) && w.Field("Unique", Unique, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{ return r.Field("Unique", Unique, e) && r.Field("Lookup", Lookup, e) && r.Field("Values", Values, e) && r.Field("Nested", Nested, e); }
	};
	void TestSerializationGraph()
	{
		SGraph source, restored;
		auto child = std::make_shared<SNode>(); child->Value = 42;
		auto leaf = std::make_shared<SNode>(); leaf->Value = 73; child->Next = leaf;
		source.Nested = {{{7, {child, nullptr, leaf, child}}}};
		source.Lookup.emplace(L"z", leaf); source.Lookup.emplace(L"a", child);
		source.Values.emplace_back(); source.Unique = std::make_unique<SNode>(); source.Unique->Value = -55;
		std::vector<std::uint8_t> bytes, again; std::string error;
		const bool written = FSerializationArchive::Serialize(source, bytes, error);
		std::cout << "TRACE SERIAL-02 written=" << written << " error=" << error << '\n';
		assert(written);
		assert(FSerializationArchive::Deserialize(bytes, restored, error));
		const auto& entries = restored.Nested.at(0).at(7);
		assert(entries[0] == entries[3] && !entries[1] && entries[0]->Next == entries[2]);
		assert(entries[0] == restored.Lookup.at(L"a") && entries[2] == restored.Lookup.at(L"z"));
		assert(entries[0] != child && restored.Unique->Value == -55 && restored.Values[0].Number == -17);
		source.Lookup.clear(); source.Lookup.emplace(L"a", child); source.Lookup.emplace(L"z", leaf);
		assert(FSerializationArchive::Serialize(source, again, error) && bytes == again);
		std::cout << "PASS SERIAL-02 recursive containers shared identity null forward field order unique values deterministic unordered\n";
	}

	struct SEnumFields : ISerializable
	{
		static constexpr std::string_view kTypeName = "SEnumFields";
		EFixtureMood Mood = EFixtureMood::Alert;
		std::map<EFixtureMood, std::uint64_t> Counts{{EFixtureMood::Calm, 99}};
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override { return w.Field("Mood", Mood, e) && w.Field("Counts", Counts, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override { return r.Field("Mood", Mood, e) && r.Field("Counts", Counts, e); }
	};
	void TestSerializationEnum()
	{
		SEnumFields source, restored; std::vector<std::uint8_t> bytes; std::string error;
		const bool written = FSerializationArchive::Serialize(source, bytes, error);
		std::cout << "TRACE SERIAL-03 written=" << written << " error=" << error << '\n';
		assert(written && FSerializationArchive::Deserialize(bytes, restored, error));
		assert(restored.Mood == EFixtureMood::Alert && restored.Counts.at(EFixtureMood::Calm) == 99);
		source.Mood = static_cast<EFixtureMood>(3); const auto before = bytes;
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == before);
		std::cout << "PASS SERIAL-03 explicit enum trait keys invalid enum preserves output\n";
	}

	struct FCustomNode : SNode
	{
		static inline int BeforeHits = 0, AfterHits = 0, DestructorHits = 0;
		static inline std::function<void()> FactoryCallback, DestructorCallback;
		static inline bool ThrowAfter = false;
		~FCustomNode() override { ++DestructorHits; if (DestructorCallback) DestructorCallback(); }
		bool OnBeforeSerialize(std::string&) const override { ++BeforeHits; return true; }
		bool OnAfterDeserialize(std::string&) override { ++AfterHits; if (ThrowAfter) throw 61; return true; }
	};
	struct FCustomScalar : SScalar
	{
		bool OnAfterDeserialize(std::string&) override { ++FCustomNode::AfterHits; return true; }
	};
	void TestSerializationRegistry()
	{
		FSerializableTypeRegistry registry; std::string error;
		const bool registered = registry.Register<SNode>(SNode::kTypeName, [] { return std::make_unique<SNode>(); }, error);
		std::cout << "TRACE SERIAL-04 registered=" << registered << " error=" << error << '\n';
		assert(registered);
		assert(!registry.Register<SNode>(SNode::kTypeName, [] { return std::make_unique<SNode>(); }, error));
		assert(!registry.Replace<SNode>("Other", [] { return std::make_unique<SNode>(); }, error));
		assert(registry.Replace<SNode>(SNode::kTypeName, []() -> std::unique_ptr<SNode>
		{ if (FCustomNode::FactoryCallback) FCustomNode::FactoryCallback(); return std::make_unique<FCustomNode>(); }, error));
		assert(registry.Register<SScalar>(SScalar::kTypeName, []() -> std::unique_ptr<SScalar>
		{ if (FCustomNode::FactoryCallback) FCustomNode::FactoryCallback(); return std::make_unique<FCustomScalar>(); }, error));
		SGraph source, candidate;
		source.Values.emplace_back(); source.Values.emplace_back();
		auto node = std::make_shared<SNode>(); node->Value = 111;
		auto separate = std::make_shared<SNode>(); separate->Value = node->Value;
		source.Nested = {{{5, {node, node, separate}}}}; source.Lookup.emplace(L"item", node);
		source.Unique = std::make_unique<SNode>(); source.Unique->Value = 222;
		std::vector<std::uint8_t> bytes;
		assert(FSerializationArchive::Serialize(source, bytes, error));
		int factoryHits = 0;
		FCustomNode::FactoryCallback = [&]
		{
			++factoryHits;
			std::string nestedError;
			assert(!registry.Replace<SNode>(SNode::kTypeName, [] { return std::make_unique<SNode>(); }, nestedError));
		};
		FCustomNode::AfterHits = 0;
		assert(FSerializationArchive::Deserialize(bytes, candidate, error, 1, &registry));
		assert(factoryHits == 5 && FCustomNode::AfterHits == 5);
		assert(candidate.Values.size() == 2 && candidate.Values[0].Number == -17 &&
			!dynamic_cast<FCustomScalar*>(&candidate.Values[0]));
		assert(candidate.Nested[0].at(5)[2] != candidate.Nested[0].at(5)[0] &&
			candidate.Nested[0].at(5)[2]->Value == candidate.Nested[0].at(5)[0]->Value);
		assert(dynamic_cast<FCustomNode*>(candidate.Unique.get()) && dynamic_cast<FCustomNode*>(candidate.Lookup.at(L"item").get()));
		assert(candidate.Lookup.at(L"item") == candidate.Nested[0].at(5)[1]);
		FCustomNode::FactoryCallback = {};
		assert(registry.Replace<SNode>(SNode::kTypeName, []() -> std::unique_ptr<SNode> { return {}; }, error));
		SGraph rejected;
		assert(!FSerializationArchive::Deserialize(bytes, rejected, error, 1, &registry));
		assert(!error.empty());
		assert(registry.Replace<SNode>(SNode::kTypeName, []() -> std::unique_ptr<SNode> { throw 31; }, error));
		assert(!FSerializationArchive::Deserialize(bytes, rejected, error, 1, &registry));
		assert(FSerializationArchive::Deserialize(bytes, rejected, error));
		std::cout << "PASS SERIAL-04 nested Custom factories hooks exact hits null throw no fallback registry pin recovery\n";
	}

	struct SOldEvolution : ISerializable
	{
		static constexpr std::string_view kTypeName = "SEvolution";
		std::shared_ptr<SNode> RemovedObject, KeptObject;
		std::shared_ptr<SScalar> Retired;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ return w.Field("RemovedObject", RemovedObject, e) && w.Field("KeptObject", KeptObject, e) && w.Field("Retired", Retired, e); }
		bool DeserializeFields(FBinaryReader&, std::string&) override { return false; }
	};
	struct SNewEvolution : ISerializable
	{
		static constexpr std::string_view kTypeName = "SEvolution";
		std::shared_ptr<SNode> KeptObject;
		std::int32_t Added = 89;
		int Reads = 0, After = 0;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ return w.Field("KeptObject", KeptObject, e) && w.Field("Added", Added, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{ ++Reads; return r.Field("Added", Added, e, r.GetSchemaVersion() >= 2) && r.Field("KeptObject", KeptObject, e); }
		bool OnAfterDeserialize(std::string&) override { ++After; return true; }
	};
	std::uint32_t FixtureU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
	{
		assert(offset + 4 <= bytes.size());
		return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8) |
			(std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
	}
	void FixtureSetU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
	{
		assert(offset + 4 <= bytes.size());
		for (std::size_t i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
	}
	void TestSerializationEvolutionFraming()
	{
		SOldEvolution old;
		old.RemovedObject = std::make_shared<SNode>(); old.RemovedObject->Value = 481;
		old.KeptObject = old.RemovedObject; old.Retired = std::make_shared<SScalar>();
		std::vector<std::uint8_t> bytes; std::string error;
		assert(FSerializationArchive::Serialize(old, bytes, error));
		FSerializableTypeRegistry registry; int retiredFactories = 0;
		assert(registry.Register<SScalar>(SScalar::kTypeName, [&] { ++retiredFactories; return std::make_unique<SScalar>(); }, error));
		SNewEvolution current;
		assert(FSerializationArchive::Deserialize(bytes, current, error, 2, &registry));
		assert(current.KeptObject->Value == 481 && current.Added == 89 && current.After == 1 && retiredFactories == 0);
		// 실제 사용하지 않는 은퇴 타입도 객체/필드 framing 전체를 검사해야 한다.
		std::size_t table = 16 + FixtureU32(bytes, 12);
		assert(FixtureU32(bytes, table) == 2); table += 4;
		table += 4 + FixtureU32(bytes, table);
		const auto retiredBegin = table + 4;
		const auto retiredField = retiredBegin + 4 + FixtureU32(bytes, retiredBegin) + 4;
		const auto signature = retiredField + 4 + FixtureU32(bytes, retiredField);
		const auto payloadLength = signature + 4 + FixtureU32(bytes, signature);
		auto malformed = bytes; FixtureSetU32(malformed, payloadLength, 0xffffffffU);
		SNewEvolution rejected;
		const bool accepted = FSerializationArchive::Deserialize(malformed, rejected, error, 2, &registry);
		std::cout << "TRACE SERIAL-05 malformed retired accepted=" << accepted << " reads=" << rejected.Reads << " error=" << error << '\n';
		assert(!accepted && rejected.Reads == 0 && rejected.After == 0 && retiredFactories == 0 && !error.empty());
		for (std::size_t cut = 0; cut < bytes.size(); ++cut)
		{
			std::vector<std::uint8_t> prefix(bytes.begin(), bytes.begin() + cut);
			SNewEvolution candidate;
			assert(!FSerializationArchive::Deserialize(prefix, candidate, error, 2) && !error.empty());
		}
		malformed = bytes; malformed.push_back(0);
		assert(!FSerializationArchive::Deserialize(malformed, rejected, error, 2) && rejected.Reads == 0);
		malformed = bytes; FixtureSetU32(malformed, 8, 3);
		assert(!FSerializationArchive::Deserialize(malformed, rejected, error, 2) && rejected.Reads == 0);
		malformed = bytes; FixtureSetU32(malformed, 8, 2);
		assert(!FSerializationArchive::Deserialize(malformed, rejected, error, 2)); // 같은 버전의 필수 필드는 누락 불가
		old.RemovedObject->Next = old.RemovedObject;
		const auto preserved = bytes;
		assert(!FSerializationArchive::Serialize(old, bytes, error) && bytes == preserved);
		old.RemovedObject->Next.reset();
		std::cout << "PASS SERIAL-05 deletion forward references retired skip bounded full framing prefixes future version default cycle reject\n";
	}

	struct FBoundaryNode : SNode
	{
		std::function<bool(std::string&)> BeforeCallback;
		bool OnBeforeSerialize(std::string& error) const override { return !BeforeCallback || BeforeCallback(error); }
	};
	struct SBoundaryRoot : ISerializable
	{
		static constexpr std::string_view kTypeName = "SBoundaryRoot";
		std::shared_ptr<FBoundaryNode> Child;
		mutable FBinaryWriter* OuterWriter = nullptr;
		FBinaryReader* OuterReader = nullptr;
		mutable int Queries = 0;
		mutable std::function<void()> QueryCallback;
		std::string_view GetSerializableTypeName() const noexcept override
		{ ++Queries; if (QueryCallback) QueryCallback(); return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ OuterWriter = &w; return w.Field("Child", Child, e) && w.Field("Other", std::int32_t{6}, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{ OuterReader = &r; std::int32_t other = 0; return r.Field("Child", Child, e) && r.Field("Other", other, e); }
	};
	void TestSerializationEntryBoundaries()
	{
		SBoundaryRoot source; source.Child = std::make_shared<FBoundaryNode>();
		std::vector<std::uint8_t> bytes{71}; std::string error;
		int queryReentries = 0;
		source.QueryCallback = [&]
		{
			++queryReentries; std::string nestedError;
			assert(!FSerializationArchive::Serialize(source, bytes, nestedError) && bytes == std::vector<std::uint8_t>{71});
		};
		bool nestedFieldAccepted = false; int fieldReentries = 0;
		source.Child->BeforeCallback = [&](std::string& e)
		{ ++fieldReentries; nestedFieldAccepted = source.OuterWriter->Field("Injected", std::int32_t{42}, e); return true; };
		const bool written = FSerializationArchive::Serialize(source, bytes, error);
		std::cout << "TRACE SERIAL-06 nested field=" << nestedFieldAccepted << " written=" << written << " error=" << error << '\n';
		assert(!written && !nestedFieldAccepted && fieldReentries == 1 && queryReentries == 1 && source.Queries == 1);
		assert(bytes == std::vector<std::uint8_t>{71});
		source.QueryCallback = {}; source.Child->BeforeCallback = {};
		assert(FSerializationArchive::Serialize(source, bytes, error));
		FSerializableTypeRegistry registry; SBoundaryRoot candidate; int readReentries = 0;
		assert(registry.Register<FBoundaryNode>(FBoundaryNode::kTypeName, [&]
		{
			++readReentries; std::int32_t ignored = 0; std::string nestedError;
			assert(!candidate.OuterReader->Field("Other", ignored, nestedError));
			return std::make_unique<FBoundaryNode>();
		}, error));
		assert(!FSerializationArchive::Deserialize(bytes, candidate, error, 1, &registry) && readReentries == 1);
		assert(FSerializationArchive::Deserialize(bytes, candidate, error));
		std::cout << "PASS SERIAL-06 first virtual query archive reentry nested field rejection output preservation recovery\n";
	}

	std::size_t FixtureFieldPayload(const std::vector<std::uint8_t>& bytes, std::string_view wanted)
	{
		std::size_t position = 16 + 4 + FixtureU32(bytes, 16);
		const auto count = FixtureU32(bytes, position); position += 4;
		for (std::uint32_t i = 0; i < count; ++i)
		{
			const auto nameSize = FixtureU32(bytes, position); position += 4;
			const std::string name(bytes.begin() + position, bytes.begin() + position + nameSize); position += nameSize;
			position += 4 + FixtureU32(bytes, position);
			const auto size = FixtureU32(bytes, position); position += 4;
			if (name == wanted) return position;
			position += size;
		}
		assert(false); return 0;
	}
	struct SPrimitives : ISerializable
	{
		static constexpr std::string_view kTypeName = "SPrimitives";
		bool Flag = true;
		std::int32_t I32 = (std::numeric_limits<std::int32_t>::min)();
		std::uint32_t U32 = (std::numeric_limits<std::uint32_t>::max)();
		std::int64_t I64 = (std::numeric_limits<std::int64_t>::min)();
		std::uint64_t U64 = (std::numeric_limits<std::uint64_t>::max)();
		float F32 = -0.0f;
		double F64 = -1.25e100;
		std::wstring Text = L"한글\U0001F600";
		std::vector<bool> Bits{true, false, true};
		std::map<std::int32_t, std::int32_t> Map{{-9, 11}, {9, 22}};
		std::unique_ptr<std::int32_t> Unique = std::make_unique<std::int32_t>(-21);
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{
			return w.Field("Flag", Flag, e) && w.Field("I32", I32, e) && w.Field("U32", U32, e) && w.Field("I64", I64, e) &&
				w.Field("U64", U64, e) && w.Field("F32", F32, e) && w.Field("F64", F64, e) && w.Field("Text", Text, e) &&
				w.Field("Bits", Bits, e) && w.Field("Map", Map, e) && w.Field("Unique", Unique, e);
		}
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{
			return r.Field("Flag", Flag, e) && r.Field("I32", I32, e) && r.Field("U32", U32, e) && r.Field("I64", I64, e) &&
				r.Field("U64", U64, e) && r.Field("F32", F32, e) && r.Field("F64", F64, e) && r.Field("Text", Text, e) &&
				r.Field("Bits", Bits, e) && r.Field("Map", Map, e) && r.Field("Unique", Unique, e);
		}
	};
	void TestSerializationMalformedPrimitives()
	{
		SPrimitives source, candidate; std::string error; std::vector<std::uint8_t> bytes;
		assert(FSerializationArchive::Serialize(source, bytes, error) && FSerializationArchive::Deserialize(bytes, candidate, error));
		assert(candidate.Flag && candidate.I32 == source.I32 && candidate.U32 == source.U32 && candidate.I64 == source.I64 && candidate.U64 == source.U64);
		assert(std::signbit(candidate.F32) && candidate.F64 == source.F64 && candidate.Text == source.Text && candidate.Bits == source.Bits);
		assert(candidate.Map == source.Map && *candidate.Unique == -21);
		auto broken = bytes; FixtureSetU32(broken, 12, 0xffffffffU);
		bool accepted = FSerializationArchive::Deserialize(broken, candidate, error);
		std::cout << "TRACE SERIAL-07 oversized blob accepted=" << accepted << " error=" << error << '\n';
		assert(!accepted && !error.empty());
		auto reject = [&](const std::vector<std::uint8_t>& packet)
		{ error.clear(); assert(!FSerializationArchive::Deserialize(packet, candidate, error) && !error.empty()); };
		broken = bytes; broken[FixtureFieldPayload(bytes, "Flag")] = 2; reject(broken);
		broken = bytes; FixtureSetU32(broken, FixtureFieldPayload(bytes, "F32"), 0x7f800000U); reject(broken);
		broken = bytes; const auto text = FixtureFieldPayload(bytes, "Text"); broken[text + 4] = 0; broken[text + 5] = 0xdc; reject(broken);
		broken = bytes; FixtureSetU32(broken, FixtureFieldPayload(bytes, "Bits"), 0xffffffffU); reject(broken);
		broken = bytes; const auto map = FixtureFieldPayload(bytes, "Map");
		assert(FixtureU32(bytes, map) == 2);
		for (std::size_t i = 0; i < 4; ++i) broken[map + 24 + i] = broken[map + 8 + i];
		reject(broken);
		broken = bytes; broken[FixtureFieldPayload(bytes, "Unique")] = 3; reject(broken);
		const auto before = bytes;
		source.Text = std::wstring(1, static_cast<wchar_t>(0xd800));
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == before);
		source.Text.assign(32769, L'a'); assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == before);
		source.Text.clear(); source.F64 = std::numeric_limits<double>::infinity();
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == before);
		source.F64 = 0; source.Bits.assign(4097, false); assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == before);
		std::vector<std::uint8_t> huge(1024 * 1024 + 1); reject(huge);
		std::cout << "PASS SERIAL-07 primitive widths limits bool floats UTF16 duplicate keys overflow and output preservation\n";
	}

	void TestSerializationCleanupAndGraphLimits()
	{
		SGraph source; source.Unique = std::make_unique<SNode>();
		std::string error; std::vector<std::uint8_t> bytes;
		assert(FSerializationArchive::Serialize(source, bytes, error));
		FSerializableTypeRegistry registry;
		assert(registry.Register<SNode>(SNode::kTypeName, []() -> std::unique_ptr<SNode> { return std::make_unique<FCustomNode>(); }, error));
		int destructorReentries = 0;
		FCustomNode::DestructorCallback = [&]
		{
			++destructorReentries; std::string nestedError; std::vector<std::uint8_t> untouched{19};
			assert(!FSerializationArchive::Serialize(source, untouched, nestedError) && untouched == std::vector<std::uint8_t>{19});
			assert(!registry.Replace<SNode>(SNode::kTypeName, [] { return std::make_unique<SNode>(); }, nestedError));
		};
		FCustomNode::ThrowAfter = true;
		SGraph failed;
		assert(!FSerializationArchive::Deserialize(bytes, failed, error, 1, &registry) && destructorReentries == 1 && !failed.Unique);
		FCustomNode::ThrowAfter = false; FCustomNode::DestructorCallback = {};
		assert(FSerializationArchive::Deserialize(bytes, failed, error, 1, &registry));
		const auto preserved = bytes;
		auto chain = std::make_shared<SNode>(); auto tail = chain;
		for (int i = 0; i < 70; ++i) { tail->Next = std::make_shared<SNode>(); tail = tail->Next; }
		source.Lookup.emplace(L"deep", chain);
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == preserved);
		source.Lookup.clear();
		// 동일 제어 블록의 다른 완전 객체 주소는 별칭으로 거부한다.
		struct SPair { SNode First, Second; };
		auto pair = std::make_shared<SPair>();
		source.Lookup.emplace(L"first", std::shared_ptr<SNode>(pair, &pair->First));
		source.Lookup.emplace(L"second", std::shared_ptr<SNode>(pair, &pair->Second));
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == preserved);
		source.Lookup.clear();
		source.Lookup.emplace(L"mixed", std::shared_ptr<SNode>(source.Unique.get(), [](SNode*) {}));
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == preserved);
		source.Lookup.clear();
		SNode external;
		source.Lookup.emplace(L"ownerless", std::shared_ptr<SNode>(std::shared_ptr<SNode>{}, &external));
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == preserved);
		source.Lookup.clear();
		assert(FSerializationArchive::Serialize(source, bytes, error));
		SEnumFields enumeration;
		assert(FSerializationArchive::Serialize(enumeration, bytes, error));
		FixtureSetU32(bytes, FixtureFieldPayload(bytes, "Mood"), 3);
		assert(!FSerializationArchive::Deserialize(bytes, enumeration, error));
		std::cout << "PASS SERIAL-08 failed Custom destructor exact reentry hit guard registry lifetime depth alias witness mixed ownership enum wire reject\n";
	}

	struct SStringList : ISerializable
	{
		static constexpr std::string_view kTypeName = "SStringList";
		std::vector<std::shared_ptr<SScalar>> Items;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override { return w.Field("Items", Items, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override { return r.Field("Items", Items, e); }
	};
	void TestSerializationAggregateBudget()
	{
		SStringList source;
		for (int i = 0; i < 20; ++i)
		{
			auto item = std::make_shared<SScalar>(); item->Text.assign(32768, L'a'); source.Items.push_back(std::move(item));
		}
		std::vector<std::uint8_t> bytes{56}; std::string error;
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == std::vector<std::uint8_t>{56});
		int visited = 0; for (const auto& item : source.Items) visited += item->Before;
		std::cout << "TRACE SERIAL-09 oversized shared table visited=" << visited << " error=" << error << '\n';
		assert(visited > 0 && visited < 20 && !error.empty());
		source.Items.resize(2);
		assert(FSerializationArchive::Serialize(source, bytes, error));
		SStringList restored; assert(FSerializationArchive::Deserialize(bytes, restored, error) && restored.Items.size() == 2);
		std::cout << "PASS SERIAL-09 aggregate shared byte budget stops before staging entire oversized graph and recovers\n";
	}

	struct SForwardData : ISerializable
	{
		static constexpr std::string_view kTypeName = "SForwardData";
		std::int32_t Value = 37;
		mutable int Before = 0;
		int After = 0;
		mutable std::function<bool(FBinaryWriter&, std::string&)> WriteCallback;
		std::function<bool(FBinaryReader&, std::string&)> ReadCallback;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
		{
			if (WriteCallback && !WriteCallback(writer, error)) return false;
			return writer.Field("Value", Value, error);
		}
		bool DeserializeFields(FBinaryReader& reader, std::string& error) override
		{
			if (ReadCallback && !ReadCallback(reader, error)) return false;
			return reader.Field("Value", Value, error);
		}
		bool OnBeforeSerialize(std::string&) const override { ++Before; return true; }
		bool OnAfterDeserialize(std::string&) override { ++After; return true; }
	};
	class FForwardWrapper : public ISerializable
	{
	public:
		static constexpr std::string_view kTypeName = SForwardData::kTypeName;
		mutable int Before = 0;
		int After = 0;
		bool ForwardAfterHook = false;
		mutable bool IgnoreForwardFailure = false;
		SForwardData& Data() { return m_data; }
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		static bool ProbeWrite(const ISerializable& data, FBinaryWriter& writer, std::string& error)
		{ return ForwardSerializeFields(data, writer, error); }
		static bool ProbeRead(ISerializable& data, FBinaryReader& reader, std::string& error)
		{ return ForwardDeserializeFields(data, reader, error); }
	protected:
		bool SerializeFields(FBinaryWriter& writer, std::string& error) const override
		{
			const bool result = ForwardSerializeFields(m_data, writer, error);
			return IgnoreForwardFailure || result;
		}
		bool DeserializeFields(FBinaryReader& reader, std::string& error) override
		{
			m_reader = &reader;
			const bool result = ForwardDeserializeFields(m_data, reader, error);
			return IgnoreForwardFailure || result;
		}
		bool OnBeforeSerialize(std::string&) const override { ++Before; return true; }
		bool OnAfterDeserialize(std::string& error) override
		{
			++After;
			if (ForwardAfterHook)
			{
				assert(!ForwardDeserializeFields(m_data, *m_reader, error));
				return true; // 잘못된 전달을 무시해도 reader 실패가 중독된 상태여야 한다.
			}
			return true;
		}
	private:
		SForwardData m_data;
		FBinaryReader* m_reader = nullptr;
	};
	void TestSerializationCompositionForwarding()
	{
		FForwardWrapper wrapper, restored;
		std::vector<std::uint8_t> wrapped{71}, direct;
		std::string error;
		assert(FSerializationArchive::Serialize(wrapper.Data(), direct, error));
		const bool written = FSerializationArchive::Serialize(wrapper, wrapped, error);
		std::cout << "TRACE SERIAL-10 wrapper forwarding=" << written << " error=" << error << '\n';
		assert(written && wrapped == direct && wrapper.Before == 1 && wrapper.Data().Before == 1);
		restored.Data().Value = -1;
		assert(FSerializationArchive::Deserialize(wrapped, restored, error));
		assert(restored.Data().Value == 37 && restored.After == 1 && restored.Data().After == 0);
		assert(FSerializationArchive::Serialize(restored, wrapped, error) && wrapped == direct);
		// 완전 객체 NVI에 같은 writer/reader를 다시 전달하는 경로는 여전히 거부한다.
		wrapper.Data().WriteCallback = [&](FBinaryWriter& writer, std::string& e)
		{ assert(!wrapper.Data().Serialize(writer, e)); return true; };
		assert(!FSerializationArchive::Serialize(wrapper, wrapped, error) && wrapped == direct);
		wrapper.Data().WriteCallback = {};
		restored.Data().ReadCallback = [&](FBinaryReader& reader, std::string& e)
		{ assert(!restored.Data().Deserialize(reader, e)); return true; };
		assert(!FSerializationArchive::Deserialize(direct, restored, error));
		restored.Data().ReadCallback = {};
		restored.ForwardAfterHook = true;
		assert(!FSerializationArchive::Deserialize(direct, restored, error));
		restored.ForwardAfterHook = false;
		wrapper.IgnoreForwardFailure = true;
		wrapper.Data().WriteCallback = [](FBinaryWriter&, std::string&) -> bool { throw 83; };
		assert(!FSerializationArchive::Serialize(wrapper, wrapped, error) && wrapped == direct);
		wrapper.Data().WriteCallback = {}; wrapper.IgnoreForwardFailure = false;
		wrapper.Data().WriteCallback = [&](FBinaryWriter& writer, std::string& e)
		{ assert(!FForwardWrapper::ProbeWrite(wrapper.Data(), writer, e)); return true; };
		assert(!FSerializationArchive::Serialize(wrapper, wrapped, error) && wrapped == direct);
		wrapper.Data().WriteCallback = {};
		SBoundaryRoot boundary; boundary.Child = std::make_shared<FBoundaryNode>();
		boundary.Child->BeforeCallback = [&](std::string& e)
		{ assert(!FForwardWrapper::ProbeWrite(wrapper.Data(), *boundary.OuterWriter, e)); return true; };
		assert(!FSerializationArchive::Serialize(boundary, wrapped, error) && wrapped == direct);
		restored.IgnoreForwardFailure = true;
		restored.Data().ReadCallback = [](FBinaryReader&, std::string&) -> bool { throw 84; };
		assert(!FSerializationArchive::Deserialize(direct, restored, error));
		restored.Data().ReadCallback = {}; restored.IgnoreForwardFailure = false;
		restored.Data().ReadCallback = [&](FBinaryReader& reader, std::string& e)
		{ assert(!FForwardWrapper::ProbeRead(restored.Data(), reader, e)); return true; };
		assert(!FSerializationArchive::Deserialize(direct, restored, error));
		restored.Data().ReadCallback = {};
		assert(FSerializationArchive::Deserialize(direct, restored, error));
		std::cout << "PASS SERIAL-10 composition identical frame outer hooks once delegates no hooks phase rejection NVI rejection exception poison recovery\n";
	}

	struct SBudgetOrder : ISerializable
	{
		static constexpr std::string_view kTypeName = "SBudgetOrder";
		SScalar Scalar;
		std::vector<std::vector<std::int32_t>> Empty;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ return w.Field("Scalar", Scalar, e) && w.Field("Empty", Empty, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{ return r.Field("Empty", Empty, e) && r.Field("Scalar", Scalar, e); }
	};
	struct SThreeInts : ISerializable
	{
		static constexpr std::string_view kTypeName = "SThreeInts";
		std::int32_t A = 1, B = 2, C = 3;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ return w.Field("A", A, e) && w.Field("B", B, e) && w.Field("C", C, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{ return r.Field("C", C, e) && r.Field("B", B, e) && r.Field("A", A, e); }
	};
	struct SObjectBudget : ISerializable
	{
		static constexpr std::string_view kTypeName = "SObjectBudget";
		SScalar Prefix;
		std::vector<std::vector<SThreeInts>> Objects;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ return w.Field("Prefix", Prefix, e) && w.Field("Objects", Objects, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{ return r.Field("Objects", Objects, e) && r.Field("Prefix", Prefix, e); }
	};
	struct SStructuralBudget : ISerializable
	{
		static constexpr std::string_view kTypeName = "SStructuralBudget";
		std::vector<std::vector<std::int32_t>> Values;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override { return w.Field("Values", Values, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override { return r.Field("Values", Values, e); }
	};
	void TestSerializationOrderIndependentBudgets()
	{
		SBudgetOrder source, restored; source.Empty.resize(4096);
		std::string error; std::vector<std::uint8_t> bytes;
		assert(FSerializationArchive::Serialize(source, bytes, error));
		const bool read = FSerializationArchive::Deserialize(bytes, restored, error);
		std::cout << "TRACE SERIAL-11 reversed field budget read=" << read << " error=" << error << '\n';
		SObjectBudget objects, restoredObjects; objects.Objects.resize(1); objects.Objects[0].resize(4096);
		const bool objectsWritten = FSerializationArchive::Serialize(objects, bytes, error);
		std::cout << "TRACE SERIAL-11 4096 value objects written=" << objectsWritten << " error=" << error << '\n';
		assert(read && restored.Empty.size() == 4096 && restored.Scalar.Number == -17);
		assert(objectsWritten);
		assert(FSerializationArchive::Deserialize(bytes, restoredObjects, error));
		assert(restoredObjects.Objects.size() == 1 && restoredObjects.Objects[0].size() == 4096 &&
			restoredObjects.Objects[0].back().C == 3 && restoredObjects.Prefix.Number == -17);
		const auto preserved = bytes;
		source.Empty.resize(4097);
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == preserved);
		objects.Objects.resize(3, objects.Objects[0]);
		assert(!FSerializationArchive::Serialize(objects, bytes, error) && bytes == preserved);
		assert(error == "object depth or count limit");
		SStructuralBudget structural; structural.Values.resize(16, std::vector<std::int32_t>(4096));
		assert(!FSerializationArchive::Serialize(structural, bytes, error) && bytes == preserved);
		assert(error == "value depth or count limit");
		std::cout << "PASS SERIAL-11 independent budgets reversed reads 4096 value objects container object structural bounds\n";
	}

	struct SReentryProbe : ISerializable
	{
		static constexpr std::string_view kTypeName = "SReentryProbe";
		std::int32_t Value = 27;
		std::function<void()> Query, Before, After;
		std::string_view GetSerializableTypeName() const noexcept override { if (Query) Query(); return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override { return w.Field("Value", Value, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override { return r.Field("Value", Value, e); }
		bool OnBeforeSerialize(std::string&) const override { if (Before) Before(); return true; }
		bool OnAfterDeserialize(std::string&) override { if (After) After(); return true; }
	};
	struct FReentryScalar : SScalar
	{
		static inline std::function<void()> Cleanup;
		~FReentryScalar() override { if (Cleanup) Cleanup(); }
	};
	struct SDiscardShared : ISerializable
	{
		static constexpr std::string_view kTypeName = "SDiscardShared";
		std::shared_ptr<SNode> Child;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override { return w.Field("Child", Child, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override { return r.Field("Child", Child, e); }
		bool OnAfterDeserialize(std::string&) override { Child.reset(); return true; }
	};
	void TestSerializationArchivePoison()
	{
		SScalar nestedSource, nestedCandidate;
		std::vector<std::uint8_t> nestedBytes; std::string error;
		assert(FSerializationArchive::Serialize(nestedSource, nestedBytes, error));
		int cases = 0, correct = 0;
		for (const bool nestedRead : {false, true})
		{
			int hits = 0;
			auto reenter = [&]
			{
				++hits; std::string distinctError; std::vector<std::uint8_t> untouched{83};
				nestedCandidate.Number = -983;
				const bool accepted = nestedRead ? FSerializationArchive::Deserialize(nestedBytes, nestedCandidate, distinctError) :
					FSerializationArchive::Serialize(nestedSource, untouched, distinctError);
				assert(!accepted && distinctError == "reentrant archive call" &&
					untouched == std::vector<std::uint8_t>{83} && nestedCandidate.Number == -983);
			};
			auto observe = [&](std::string_view phase, bool accepted, bool preserved = true)
			{
				++cases;
				const bool rejected = !accepted && preserved && hits == 1 && error == "reentrant archive call";
				correct += rejected ? 1 : 0;
				std::cout << "TRACE SERIAL-12 phase=" << phase << " nested-read=" << nestedRead << " hits=" << hits <<
					" outer=" << accepted << " preserved=" << preserved << " error=" << error << '\n';
			};
			for (const bool outerRead : {false, true})
			{
				for (const bool query : {true, false})
				{
					SReentryProbe probe; std::vector<std::uint8_t> packet;
					assert(FSerializationArchive::Serialize(probe, packet, error));
					std::vector<std::uint8_t> output{91}; hits = 0;
					if (query) probe.Query = reenter;
					else if (outerRead) probe.After = reenter;
					else probe.Before = reenter;
					const bool accepted = outerRead ? FSerializationArchive::Deserialize(packet, probe, error) :
						FSerializationArchive::Serialize(probe, output, error);
					observe(query ? (outerRead ? "read-query" : "write-query") : (outerRead ? "after-hook" : "before-hook"),
						accepted, outerRead || output == std::vector<std::uint8_t>{91});
					probe.Query = {}; probe.Before = {}; probe.After = {};
					assert(FSerializationArchive::Serialize(probe, output, error) && FSerializationArchive::Deserialize(output, probe, error));
				}
			}
			SGraph source; source.Values.emplace_back(); std::vector<std::uint8_t> packet;
			assert(FSerializationArchive::Serialize(source, packet, error));
			FSerializableTypeRegistry registry;
			assert(registry.Register<SScalar>(SScalar::kTypeName, [&]() -> std::unique_ptr<SScalar>
			{ reenter(); return std::make_unique<SScalar>(); }, error));
			SGraph candidate; hits = 0;
			const bool factoryAccepted = FSerializationArchive::Deserialize(packet, candidate, error, 1, &registry);
			observe("factory", factoryAccepted);
			assert(registry.Replace<SScalar>(SScalar::kTypeName, []() -> std::unique_ptr<SScalar>
			{ return std::make_unique<FReentryScalar>(); }, error));
			FReentryScalar::Cleanup = reenter; hits = 0;
			const bool temporaryAccepted = FSerializationArchive::Deserialize(packet, candidate, error, 1, &registry);
			observe("value-temporary-destructor", temporaryAccepted);
			FReentryScalar::Cleanup = {};
			assert(FSerializationArchive::Deserialize(packet, candidate, error, 1, &registry));
			SDiscardShared shared, restored; shared.Child = std::make_shared<SNode>();
			assert(FSerializationArchive::Serialize(shared, packet, error));
			assert(registry.Register<SNode>(SNode::kTypeName, []() -> std::unique_ptr<SNode>
			{ return std::make_unique<FCustomNode>(); }, error));
			FCustomNode::DestructorCallback = reenter; hits = 0;
			const bool cleanupAccepted = FSerializationArchive::Deserialize(packet, restored, error, 1, &registry);
			observe("context-destructor", cleanupAccepted);
			FCustomNode::DestructorCallback = {};
			assert(FSerializationArchive::Deserialize(packet, restored, error, 1, &registry) && !restored.Child);
		}
		std::cout << "TRACE SERIAL-12 exact rejected cases=" << correct << '/' << cases << '\n';
		assert(cases == 14 && correct == cases);
		std::cout << "PASS SERIAL-12 ignored Serialize Deserialize reentry poisons queries hooks factories temporary context cleanup recovery\n";
	}

	template<class E> struct SEnumWidth : ISerializable
	{
		static constexpr std::string_view kTypeName = "SEnumWidth";
		E Value{};
		std::map<E, std::vector<E>> Keys;
		std::string_view GetSerializableTypeName() const noexcept override { return kTypeName; }
		bool SerializeFields(FBinaryWriter& w, std::string& e) const override
		{ return w.Field("Value", Value, e) && w.Field("Keys", Keys, e); }
		bool DeserializeFields(FBinaryReader& r, std::string& e) override
		{ return r.Field("Keys", Keys, e) && r.Field("Value", Value, e); }
	};
	struct SEnumWidthSkip : ISerializable
	{
		int Reads = 0;
		std::string_view GetSerializableTypeName() const noexcept override { return "SEnumWidth"; }
		bool SerializeFields(FBinaryWriter&, std::string&) const override { return true; }
		bool DeserializeFields(FBinaryReader&, std::string&) override { ++Reads; return true; }
	};
	template<class E> void CheckEnumWidth(E low, E high, std::string_view signature,
		const std::vector<std::uint8_t>& lowBytes, const std::vector<std::uint8_t>& highBytes)
	{
		SEnumWidth<E> source, candidate; source.Value = high; source.Keys.emplace(low, std::vector<E>{low, high});
		std::string error; std::vector<std::uint8_t> bytes;
		const bool written = FSerializationArchive::Serialize(source, bytes, error);
		std::cout << "TRACE SERIAL-13 signature=" << SerializationDetail::TCodec<E>::Signature() <<
			" written=" << written << " error=" << error << '\n';
		assert(written && SerializationDetail::TCodec<E>::Signature() == signature);
		assert(FSerializationArchive::Deserialize(bytes, candidate, error) && candidate.Value == high && candidate.Keys == source.Keys);
		auto offset = FixtureFieldPayload(bytes, "Value");
		assert(FixtureU32(bytes, offset - 4) == highBytes.size() &&
			std::equal(highBytes.begin(), highBytes.end(), bytes.begin() + offset));
		SEnumWidthSkip skipped;
		assert(FSerializationArchive::Deserialize(bytes, skipped, error) && skipped.Reads == 1);
		auto truncated = bytes;
		FixtureSetU32(truncated, offset - 4, static_cast<std::uint32_t>(highBytes.size() - 1));
		FixtureSetU32(truncated, 12, FixtureU32(truncated, 12) - 1);
		truncated.erase(truncated.begin() + offset + highBytes.size() - 1);
		skipped.Reads = 0;
		assert(!FSerializationArchive::Deserialize(truncated, skipped, error) && skipped.Reads == 0 && error == "truncated framing");
		const auto preserved = bytes;
		const E invalid = static_cast<E>(1);
		source.Value = invalid;
		assert(!FSerializationArchive::Serialize(source, bytes, error) && bytes == preserved && error == "invalid enum value");
		auto malformed = bytes;
		std::fill(malformed.begin() + offset, malformed.begin() + offset + highBytes.size(), std::uint8_t{0});
		malformed[offset] = 1;
		assert(!FSerializationArchive::Deserialize(malformed, candidate, error) && error == "invalid enum value");
		source.Value = low;
		assert(FSerializationArchive::Serialize(source, bytes, error) && FSerializationArchive::Deserialize(bytes, candidate, error));
		offset = FixtureFieldPayload(bytes, "Value");
		assert(candidate.Value == low && FixtureU32(bytes, offset - 4) == lowBytes.size() &&
			std::equal(lowBytes.begin(), lowBytes.end(), bytes.begin() + offset));
	}
	void TestSerializationEnumWidths()
	{
		CheckEnumWidth(EWidthI8::Low, EWidthI8::High, "enum<EWidthI8,i8>", {0x80}, {0x7f});
		CheckEnumWidth(EWidthU8::Low, EWidthU8::High, "enum<EWidthU8,u8>", {0}, {0xff});
		CheckEnumWidth(EWidthI16::Low, EWidthI16::High, "enum<EWidthI16,i16>", {0, 0x80}, {0xff, 0x7f});
		CheckEnumWidth(EWidthU16::Low, EWidthU16::High, "enum<EWidthU16,u16>", {0, 0}, {0xff, 0xff});
		CheckEnumWidth(EFixtureMood::Calm, EFixtureMood::Alert, "enum<EFixtureMood,i32>", {2, 0, 0, 0}, {7, 0, 0, 0});
		CheckEnumWidth(EWidthU32::Low, EWidthU32::High, "enum<EWidthU32,u32>", {0, 0, 0, 0}, {0xff, 0xff, 0xff, 0xff});
		CheckEnumWidth(EWidthI64::Low, EWidthI64::High, "enum<EWidthI64,i64>",
			{0, 0, 0, 0, 0, 0, 0, 0x80}, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f});
		CheckEnumWidth(EWidthU64::Low, EWidthU64::High, "enum<EWidthU64,u64>",
			{0, 0, 0, 0, 0, 0, 0, 0}, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
		std::cout << "PASS SERIAL-13 enum signed unsigned 8 16 32 64 exact little endian bounds map vector invalid skip truncation recovery\n";
	}

	class FFakeFrameClock final : public IFrameClock
	{
	public:
		double Time = 0;
		int Samples = 0;
		std::vector<double> Waits;
		std::function<void()> Sample;
		std::function<void(double)> Wait;
		double NowSeconds() override { ++Samples; if (Sample) Sample(); return Time; }
		void WaitUntil(double deadlineSeconds) override
		{
			Waits.push_back(deadlineSeconds);
			if (Wait) Wait(deadlineSeconds); else Time = deadlineSeconds;
		}
	};

	class FCallbackFrame final : public FFrame
	{
	public:
		FCallbackFrame(const SFrameOptions& options, IFrameClock& clock) : FFrame(options, clock) {}
		std::function<void()> Start, StartFrame, Input, Render, EndFrame, Stop;
		std::function<void(float)> Update;
		void Exit(int code = 0) { RequestExit(code); }
		void SkipUpdates() { SkipRemainingUpdates(); }
	private:
		void OnStart() override { if (Start) Start(); }
		void OnStartFrame() override { if (StartFrame) StartFrame(); }
		void OnInput() override { if (Input) Input(); }
		void OnUpdate(float seconds) override { if (Update) Update(seconds); }
		void OnRender() override { if (Render) Render(); }
		void OnEndFrame() override { if (EndFrame) EndFrame(); }
		void OnStop() override { if (Stop) Stop(); }
	};

	// 필요한 종료 훅 하나만 구현하는 독립 소비자다.
	class FOptionalFrame final : public FFrame
	{
	public:
		using FFrame::FFrame;
	private:
		void OnEndFrame() override { RequestExit(); }
	};

	void TestFrameInheritance()
	{
		FFakeFrameClock clock;
		SFrameOptions options;
		FOptionalFrame optional(options, clock);
		options.TargetFps = 30;
		assert(optional.GetOptions().TargetFps == 60);
		assert(optional.Run() == 0);
		assert(optional.GetFailureStage() == EFrameFailureStage::None && !optional.GetException());
		assert(optional.GetStatistics().CompletedFrames == 1 && optional.GetStatistics().UpdateCalls == 1);
		assert(optional.GetStatistics().RenderCalls == 1);
		const int samples = clock.Samples;
		assert(optional.Run() != 0 && clock.Samples == samples);
		assert(optional.GetStatistics().CompletedFrames == 1 && optional.GetFailureStage() == EFrameFailureStage::None);
		std::cout << "PASS FRAME-01 lib Run optional inheritance copied options one-shot statistics\n";
	}

	void TestFrameLifecycle()
	{
		const std::array<EFrameFailureStage, 7> stages = {EFrameFailureStage::Start, EFrameFailureStage::StartFrame,
			EFrameFailureStage::Input, EFrameFailureStage::Update, EFrameFailureStage::Render,
			EFrameFailureStage::EndFrame, EFrameFailureStage::Stop};
		for (std::size_t target = 0; target != stages.size(); ++target)
		{
			for (int action = 0; action != 3; ++action)
			{
				FFakeFrameClock clock;
				FCallbackFrame frame({}, clock);
				std::vector<std::size_t> entries;
				int nested = 0;
				auto hook = [&](std::size_t stage)
				{
					entries.push_back(stage);
					assert(frame.Run() != 0); // 정리 중에도 바깥 실행 상태/원인을 건드리지 않는다.
					++nested;
					if (stage == target)
					{
						if (action == 2) throw 71;
						frame.Exit(action == 1 ? 9 : 0);
					}
					if (stage == 5 || stage == 6) frame.Exit();
				};
				frame.Start = [&] { hook(0); };
				frame.StartFrame = [&] { hook(1); };
				frame.Input = [&] { hook(2); };
				frame.Update = [&](float) { hook(3); };
				frame.Render = [&] { hook(4); };
				frame.EndFrame = [&] { hook(5); };
				frame.Stop = [&] { hook(6); };
				bool escaped = false;
				int result = 0;
				try { result = frame.Run(); }
				catch (...) { escaped = true; }
				std::cout << "TRACE FRAME-02 target=" << target << " action=" << action << " escaped=" << escaped << '\n';
				assert(!escaped);
				assert(action == 0 ? result == 0 : action == 1 ? result == 9 : result != 0);
				assert(frame.GetFailureStage() == (action == 0 ? EFrameFailureStage::None : stages[target]));
				assert(static_cast<bool>(frame.GetException()) == (action == 2));
				if (action == 2)
				{
					int cause = 0;
					try { std::rethrow_exception(frame.GetException()); } catch (int value) { cause = value; }
					assert(cause == 71);
				}
				const auto last = std::min(target, std::size_t{5});
				std::vector<std::size_t> expected;
				for (std::size_t stage = 0; stage <= last; ++stage) expected.push_back(stage);
				expected.push_back(6);
				assert(entries == expected && nested == static_cast<int>(entries.size()));
				assert(frame.GetStatistics().CompletedFrames == ((target == 5 && action == 0) || target == 6 ? 1u : 0u));
				assert(frame.GetStatistics().UpdateCalls == (target >= 3 ? 1u : 0u));
				assert(frame.GetStatistics().RenderCalls == (target >= 4 ? 1u : 0u));
				assert(frame.Run() != 0 && entries == expected);
			}
		}
		FFakeFrameClock clock;
		FCallbackFrame firstFailure({}, clock);
		firstFailure.Start = [&] { firstFailure.Exit(23); throw 41; };
		firstFailure.Stop = [&] { firstFailure.Exit(); firstFailure.Exit(24); throw 42; };
		assert(firstFailure.Run() == 23);
		assert(firstFailure.GetFailureStage() == EFrameFailureStage::Start);
		assert(!firstFailure.GetException()); // 명시된 첫 원인을 후속 throw가 교체하지 않는다.
		FOptionalFrame production(SFrameOptions{});
		assert(production.Run() == 0);
		std::cout << "PASS FRAME-02 all hook exits throws cleanup reentry first cause default clock\n";
	}

	void TestFrameOptions()
	{
		std::vector<SFrameOptions> invalid;
		auto add = [&](auto change) { SFrameOptions options; change(options); invalid.push_back(options); };
		add([](auto& o) { o.Mode = static_cast<EFrameMode>(77); });
		for (double value : {0.0, -1.0, 0.5, 1000.01, std::numeric_limits<double>::infinity(),
			std::numeric_limits<double>::quiet_NaN(), (std::numeric_limits<double>::max)()})
			add([&](auto& o) { o.TargetFps = value; });
		for (int value : {0, -1, 1001, (std::numeric_limits<int>::max)()})
		{
			add([&](auto& o) { o.MaxCatchUpUpdates = value; });
			add([&](auto& o) { o.MaxConsecutiveRenderSkips = value; });
		}
		for (double value : {0.0, -1.0, 60.01, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
			add([&](auto& o) { o.MaxAccumulatedElapsedSeconds = value; });
		for (const auto& options : invalid)
		{
			FFakeFrameClock clock;
			FCallbackFrame frame(options, clock);
			int hooks = 0;
			frame.Start = [&] { ++hooks; frame.Exit(); };
			frame.Stop = [&] { ++hooks; };
			const int result = frame.Run();
			std::cout << "TRACE FRAME-03 invalid result=" << result << " hooks=" << hooks << " samples=" << clock.Samples << '\n';
			assert(result != 0 && frame.GetFailureStage() == EFrameFailureStage::Options);
			assert(hooks == 0 && clock.Samples == 0 && clock.Waits.empty() && !frame.GetException());
			assert(frame.Run() != 0 && hooks == 0 && clock.Samples == 0);
		}
		for (double fps : {1.0, 1000.0})
		{
			SFrameOptions options;
			options.TargetFps = fps;
			options.MaxCatchUpUpdates = fps == 1 ? 1 : 1000;
			options.MaxConsecutiveRenderSkips = options.MaxCatchUpUpdates;
			options.MaxAccumulatedElapsedSeconds = fps == 1 ? (std::numeric_limits<double>::min)() : 60;
			FFakeFrameClock clock;
			FOptionalFrame frame(options, clock);
			assert(frame.Run() == 0);
		}
		std::cout << "PASS FRAME-03 invalid options no hooks clock and valid inclusive boundaries\n";
	}

	void TestBasicFrameTiming()
	{
		SFrameOptions options;
		options.TargetFps = 8;
		FFakeFrameClock clock;
		clock.Time = 10;
		FCallbackFrame frame(options, clock);
		std::vector<float> deltas;
		int inputs = 0;
		int renders = 0;
		int ends = 0;
		frame.Start = [&] { clock.Time += 2; };
		frame.Input = [&] { ++inputs; };
		frame.Update = [&](float seconds)
		{
			deltas.push_back(seconds);
			if (ends == 0) clock.Time += 0.03125;
			if (ends == 1) clock.Time += 0.25;
		};
		frame.Render = [&] { ++renders; if (ends < 2) clock.Time += 0.03125; };
		frame.EndFrame = [&] { if (++ends == 3) frame.Exit(); };
		assert(frame.Run() == 0);
		std::cout << "TRACE FRAME-04 deltas=" << deltas[0] << ',' << deltas[1] << ',' << deltas[2]
			<< " waits=" << clock.Waits.size() << '\n';
		assert(deltas == std::vector<float>({0, 0.125f, 0.28125f}));
		assert(clock.Waits == std::vector<double>({12.125}));
		assert(inputs == 3 && renders == 3 && ends == 3);
		assert(frame.GetStatistics().CompletedFrames == 3 && frame.GetStatistics().DroppedSimulationSeconds == 0);
		FFakeFrameClock skipClock;
		FCallbackFrame skipped(options, skipClock);
		int outer = 0;
		int updates = 0;
		skipped.StartFrame = [&] { if (outer == 0) skipped.SkipUpdates(); };
		skipped.Input = [&] { if (outer == 1) skipped.SkipUpdates(); };
		skipped.Update = [&](float) { ++updates; if (outer == 2) skipped.SkipUpdates(); };
		skipped.EndFrame = [&] { if (++outer == 4) skipped.Exit(); };
		assert(skipped.Run() == 0 && updates == 2);
		assert(skipped.GetStatistics().CompletedFrames == 4 && skipped.GetStatistics().RenderCalls == 4);
		std::cout << "PASS FRAME-04 Basic actual elapsed remaining-budget wait overrun optional update skip reset\n";
	}

	void TestFrameClockFailures()
	{
		for (const auto mode : {EFrameMode::Basic, EFrameMode::FixedUpdate, EFrameMode::FrameSkip})
		{
			for (int fault = 0; fault != 10; ++fault)
			{
				SFrameOptions options;
				options.Mode = mode;
				options.TargetFps = 8;
				FFakeFrameClock clock;
				clock.Time = 1;
				FCallbackFrame frame(options, clock);
				int starts = 0;
				int stops = 0;
				int ends = 0;
				frame.Start = [&] { ++starts; };
				frame.EndFrame = [&] { if (++ends == 3) frame.Exit(); };
				frame.Stop = [&] { ++stops; frame.Exit(); throw 99; };
				clock.Sample = [&]
				{
					if (clock.Samples == 1)
					{
						if (fault == 0) clock.Time = std::numeric_limits<double>::quiet_NaN();
						if (fault == 1) clock.Time = std::numeric_limits<double>::infinity();
						if (fault == 2) clock.Time = -std::numeric_limits<double>::infinity();
						if (fault == 3) clock.Time = (std::numeric_limits<double>::max)();
						if (fault == 4) clock.Time = -(std::numeric_limits<double>::max)();
						if (fault == 7) throw 71;
					}
					if (ends == 1 && fault == 5) clock.Time = 0.5;
					if (ends == 1 && fault == 6) clock.Time = 1e40;
				};
				if (fault == 8) clock.Wait = [](double) { throw 72; };
				if (fault == 9) clock.Wait = [&](double) { clock.Time = -1; };
				const int result = frame.Run();
				std::cout << "TRACE FRAME-05 mode=" << static_cast<int>(mode) << " fault=" << fault
					<< " stage=" << static_cast<int>(frame.GetFailureStage()) << " ends=" << ends << '\n';
				assert(result != 0 && frame.GetFailureStage() == EFrameFailureStage::Clock);
				assert(starts == 1 && stops == 1);
				assert(frame.GetStatistics().CompletedFrames == (fault == 5 || fault == 6 || fault == 8 || fault == 9 ? 1u : 0u));
				assert(static_cast<bool>(frame.GetException()) == (fault == 7 || fault == 8));
				if (frame.GetException())
				{
					int cause = 0;
					try { std::rethrow_exception(frame.GetException()); } catch (int value) { cause = value; }
					assert(cause == (fault == 7 ? 71 : 72));
				}
			}
		}
		std::cout << "PASS FRAME-05 invalid backward unrepresentable throwing clocks wait cleanup preserves completed frame\n";
	}

	void TestFixedFrameTicks()
	{
		SFrameOptions options;
		options.Mode = EFrameMode::FixedUpdate;
		options.TargetFps = 8;
		options.MaxAccumulatedElapsedSeconds = 1;
		FFakeFrameClock clock;
		FCallbackFrame frame(options, clock);
		std::vector<int> counts;
		std::vector<float> deltas;
		int updates = 0;
		int inputs = 0;
		int ends = 0;
		frame.Input = [&] { ++inputs; };
		frame.Update = [&](float seconds) { ++updates; deltas.push_back(seconds); };
		frame.Render = [&]
		{
			counts.push_back(updates);
			updates = 0;
			if (ends == 0) clock.Time += 0.125;
			if (ends == 1) clock.Time += 0.375;
		};
		frame.EndFrame = [&] { if (++ends == 3) frame.Exit(); };
		assert(frame.Run() == 0);
		std::cout << "TRACE FRAME-06 ticks=" << counts[0] << ',' << counts[1] << ',' << counts[2] << '\n';
		assert(counts == std::vector<int>({0, 1, 3}));
		assert(deltas == std::vector<float>(4, 0.125f));
		assert(inputs == 3 && ends == 3 && clock.Waits.empty());
		assert(frame.GetStatistics().CompletedFrames == 3 && frame.GetStatistics().UpdateCalls == 4);
		assert(frame.GetStatistics().RenderCalls == 3 && frame.GetStatistics().DroppedSimulationSeconds == 0);
		std::cout << "PASS FRAME-06 Fixed initial zero exact one multiple ticks constant delta single input render EndFrame\n";
	}

	void TestFixedFrameBudget()
	{
		SFrameOptions options;
		options.Mode = EFrameMode::FixedUpdate;
		options.TargetFps = 8;
		options.MaxCatchUpUpdates = 2;
		options.MaxAccumulatedElapsedSeconds = 0.5625;
		FFakeFrameClock clock;
		FCallbackFrame frame(options, clock);
		std::vector<int> counts;
		int updates = 0;
		int outer = 0;
		frame.Update = [&](float seconds) { assert(seconds == 0.125f); ++updates; };
		frame.Render = [&]
		{
			counts.push_back(updates);
			updates = 0;
			if (outer == 0) clock.Time += 1.0625;
			if (outer == 1) clock.Time += 0.1875;
		};
		frame.EndFrame = [&] { if (++outer == 3) frame.Exit(); };
		assert(frame.Run() == 0);
		std::cout << "TRACE FRAME-07 ticks=" << counts[0] << ',' << counts[1] << ',' << counts[2]
			<< " dropped=" << frame.GetStatistics().DroppedSimulationSeconds << '\n';
		assert(counts == std::vector<int>({0, 2, 2}));
		assert(frame.GetStatistics().DroppedSimulationSeconds == 0.75);
		assert(frame.GetStatistics().CompletedFrames == 3 && clock.Waits.empty());
		for (int action = 0; action != 3; ++action)
		{
			FFakeFrameClock exitClock;
			FCallbackFrame exitFrame(options, exitClock);
			int ticks = 0;
			int ends = 0;
			int renders = 0;
			int stops = 0;
			exitFrame.Update = [&](float) { ++ticks; if (action == 2) throw 85; exitFrame.Exit(action); };
			exitFrame.Render = [&] { ++renders; exitClock.Time += 0.5; };
			exitFrame.EndFrame = [&] { if (++ends == 3) exitFrame.Exit(); };
			exitFrame.Stop = [&] { ++stops; };
			const int result = exitFrame.Run();
			assert(action == 0 ? result == 0 : result != 0);
			assert(ticks == 1 && ends == 1 && renders == 1 && stops == 1);
			assert(exitFrame.GetFailureStage() == (action == 0 ? EFrameFailureStage::None : EFrameFailureStage::Update));
			assert(exitFrame.GetStatistics().CompletedFrames == 1 && exitFrame.GetStatistics().UpdateCalls == 1);
		}
		std::cout << "PASS FRAME-07 Fixed clamp budget discard whole debt preserve fraction exit between ticks\n";
	}

	void TestFixedFrameTransitionDebt()
	{
		for (int skipStage = 0; skipStage != 3; ++skipStage)
		{
			SFrameOptions options;
			options.Mode = EFrameMode::FixedUpdate;
			options.TargetFps = 8;
			options.MaxAccumulatedElapsedSeconds = 1;
			FFakeFrameClock clock;
			FCallbackFrame frame(options, clock);
			int outer = 0;
			int updates = 0;
			int inputs = 0;
			int skipHits = 0;
			std::vector<int> counts;
			auto skip = [&](int stage)
			{
				if (outer == 1 && stage == skipStage) { ++skipHits; frame.SkipUpdates(); }
			};
			frame.StartFrame = [&] { skip(0); };
			frame.Input = [&] { ++inputs; skip(1); };
			frame.Update = [&](float seconds) { assert(seconds == 0.125f); ++updates; skip(2); };
			frame.Render = [&]
			{
				counts.push_back(updates);
				updates = 0;
				if (outer == 0) clock.Time += 0.4375;
				if (outer == 1) clock.Time += 0.1875;
			};
			frame.EndFrame = [&] { if (++outer == 3) frame.Exit(); };
			assert(frame.Run() == 0);
			std::cout << "TRACE FRAME-08 stage=" << skipStage << " ticks=" << counts[0] << ',' << counts[1] << ',' << counts[2]
				<< " dropped=" << frame.GetStatistics().DroppedSimulationSeconds << '\n';
			assert(counts == std::vector<int>({0, skipStage == 2 ? 1 : 0, 1}));
			assert(skipHits == 1 && inputs == 3 && frame.GetStatistics().CompletedFrames == 3);
			assert(frame.GetStatistics().DroppedSimulationSeconds == (skipStage == 2 ? 0.3125 : 0.4375));
		}
		std::cout << "PASS FRAME-08 transition skips remaining fixed ticks drops all old fractional debt resets next frame\n";
	}

	void TestFrameSkipTiming()
	{
		SFrameOptions options;
		options.Mode = EFrameMode::FrameSkip;
		options.TargetFps = 8;
		FFakeFrameClock clock;
		FCallbackFrame frame(options, clock);
		std::vector<int> renderFrames;
		std::vector<float> deltas;
		int inputs = 0;
		int outer = 0;
		frame.Input = [&] { ++inputs; };
		frame.Update = [&](float seconds) { deltas.push_back(seconds); };
		frame.Render = [&] { renderFrames.push_back(outer); if (outer == 0) clock.Time += 0.5; };
		frame.EndFrame = [&] { if (++outer == 5) frame.Exit(); };
		assert(frame.Run() == 0);
		std::cout << "TRACE FRAME-09 renders=" << renderFrames.size() << " skips=" << frame.GetStatistics().SkippedRenders << '\n';
		assert(renderFrames == std::vector<int>({0, 3, 4}));
		assert(deltas == std::vector<float>(5, 0.125f));
		assert(inputs == 5 && outer == 5 && frame.GetStatistics().CompletedFrames == 5);
		assert(frame.GetStatistics().UpdateCalls == 5 && frame.GetStatistics().RenderCalls == 3);
		assert(frame.GetStatistics().SkippedRenders == 2 && frame.GetStatistics().DroppedSimulationSeconds == 0);
		assert(clock.Waits.empty());
		FFakeFrameClock aheadClock;
		FCallbackFrame ahead(options, aheadClock);
		int ends = 0;
		ahead.Input = [&] { aheadClock.Time += 0.03125; };
		ahead.Render = [&] { aheadClock.Time += 0.03125; };
		ahead.EndFrame = [&] { if (++ends == 3) ahead.Exit(); };
		assert(ahead.Run() == 0 && ahead.GetStatistics().SkippedRenders == 0);
		assert(aheadClock.Waits == std::vector<double>({0.125, 0.25}));
		std::cout << "PASS FRAME-09 Skip fixed update input EndFrame retained logical deadlines catch-up ahead wait\n";
	}

	void TestFrameSkipBudget()
	{
		for (const bool slowForcedRender : {false, true})
		{
			SFrameOptions options;
			options.Mode = EFrameMode::FrameSkip;
			options.TargetFps = 8;
			options.MaxConsecutiveRenderSkips = 2;
			FFakeFrameClock clock;
			FCallbackFrame frame(options, clock);
			std::vector<int> renderFrames;
			int outer = 0;
			int inputs = 0;
			frame.Input = [&] { ++inputs; };
			frame.Update = [&](float seconds) { assert(seconds == 0.125f); };
			frame.Render = [&]
			{
				renderFrames.push_back(outer);
				if (outer == 0) clock.Time += 0.875;
				if (outer == 3 && slowForcedRender) clock.Time += 0.375;
			};
			frame.EndFrame = [&] { if (++outer == (slowForcedRender ? 6 : 5)) frame.Exit(); };
			assert(frame.Run() == 0);
			std::cout << "TRACE FRAME-10 slow-forced=" << slowForcedRender << " renders=" << renderFrames.size()
				<< " skipped=" << frame.GetStatistics().SkippedRenders << " dropped="
				<< frame.GetStatistics().DroppedSimulationSeconds << '\n';
			assert(renderFrames == std::vector<int>({0, 3, slowForcedRender ? 5 : 4}));
			assert(frame.GetStatistics().DroppedSimulationSeconds == 0.375);
			assert(frame.GetStatistics().SkippedRenders == (slowForcedRender ? 3u : 2u));
			assert(frame.GetStatistics().CompletedFrames == static_cast<std::uint64_t>(outer));
			assert(frame.GetStatistics().UpdateCalls == static_cast<std::uint64_t>(inputs));
			assert(clock.Waits == (slowForcedRender ? std::vector<double>{} : std::vector<double>{1.0}));
		}
		std::cout << "PASS FRAME-10 Skip bounded forced render rebase dropped lag fresh budget slow render keeps next skips\n";
	}

	void TestFixedFrameExactBoundaries()
	{
		for (double fps : {3.0, 7.0, 10.0, 30.0, 50.0, 60.0, 144.0, 1000.0})
		{
			for (int ticks : {1, 5, 10, 100})
			{
				SFrameOptions options;
				options.Mode = EFrameMode::FixedUpdate;
				options.TargetFps = fps;
				options.MaxCatchUpUpdates = 1000;
				options.MaxAccumulatedElapsedSeconds = 60;
				FFakeFrameClock clock;
				FCallbackFrame frame(options, clock);
				int outer = 0;
				int updates = 0;
				frame.Update = [&](float seconds) { assert(seconds == static_cast<float>(1.0 / fps)); ++updates; };
				frame.Render = [&] { if (outer == 0) clock.Time += ticks * (1.0 / fps); };
				frame.EndFrame = [&] { if (++outer == 2) frame.Exit(); };
				assert(frame.Run() == 0);
				std::cout << "TRACE FRAME-11 fps=" << fps << " expected=" << ticks << " actual=" << updates << '\n';
				assert(updates == ticks && frame.GetStatistics().DroppedSimulationSeconds == 0);
			}
			SFrameOptions options;
			options.Mode = EFrameMode::FixedUpdate;
			options.TargetFps = fps;
			options.MaxAccumulatedElapsedSeconds = 60;
			FFakeFrameClock clock;
			FCallbackFrame below(options, clock);
			int outer = 0;
			int updates = 0;
			below.Update = [&](float) { ++updates; };
			below.Render = [&] { if (outer == 0) clock.Time = std::nextafter(3 * (1.0 / fps), 0.0); };
			below.EndFrame = [&] { if (++outer == 2) below.Exit(); };
			assert(below.Run() == 0 && updates == 2);
		}
		std::cout << "PASS FRAME-11 nonbinary exact tick multiples no repeated subtraction loss strict below-boundary control\n";
	}

	class FQueuedInput final : public IInputSource
	{
	public:
		std::vector<SInputEvent> Events;
		std::size_t Next = 0;
		EInputReadResult ReadEvent(SInputEvent& event) noexcept override
		{
			if (Next == Events.size()) return EInputReadResult::Timeout;
			event = Events[Next++];
			return EInputReadResult::Event;
		}
	};
	struct SResult { bool Done = false; explicit operator bool() const noexcept { return Done; } };
	class FConsumerUi final : public FUiPanel
	{
	public:
		explicit FConsumerUi(int& activated) : FUiPanel({0, 0, 20, 5})
		{
			SetTitle(L"neutral");
			assert(AddChild(std::make_unique<FUiButton>(SCellRect{0, 0, 12, 3}, L"activate", [&activated] { ++activated; })));
		}
	};
	class FConsumerScene final : public TScene<SResult>, public IUiInputTarget
	{
	public:
		FConsumerScene() : Root(SCellRect{0, 0, 24, 8}) {}
		void DispatchUiInput(const SInputEvent& event) override { (void)HandleInput(event); }
		void CancelUiInput() override { Root.CancelInput(); }
		FSceneUiRoot* GetUiBaseRoot() const noexcept override { return const_cast<FSceneUiRoot*>(&Root); }
		FSceneUiRoot Root;
		std::function<void()> Callback;
		std::function<void()> BusyQuery;
	private:
		void OnEnterImpl() override {}
		void OnExitImpl() override {}
		void UpdateImpl(float) override { if (Callback) Callback(); }
		SResult HandleInputImpl(const SInputEvent& event) override { Root.Dispatch(event, true); return {}; }
		void RenderImpl(FFrameBuffer& frame) const override { Root.Render(frame, false); }
		bool IsOwnedUiBusy() const noexcept override { if (BusyQuery) BusyQuery(); return Root.IsBusy(); }
	};
#ifdef _DEBUG
	struct SResizeAllocationFault final
	{
		bool Armed = false;
		int Ordinal = 0;
		int Seen = 0;
		int Hits = 0;
		std::size_t Bytes = 0;
	};
	SResizeAllocationFault g_resizeFault;

	int __cdecl ResizeAllocationHook(int type, void*, std::size_t size, int block, long, const unsigned char*, int)
	{
		if (type == _HOOK_ALLOC && block == _NORMAL_BLOCK && g_resizeFault.Armed && size == g_resizeFault.Bytes)
		{
			if (++g_resizeFault.Seen == g_resizeFault.Ordinal)
			{
				g_resizeFault.Armed = false;
				++g_resizeFault.Hits;
				return 0;
			}
		}
		return 1;
	}
#endif

	void TestFrameResizeTransaction()
	{
#ifdef _DEBUG
		bool preservedAll = true;
		for (const int ordinal : {1, 2})
		{
			FFrameBuffer frame(3, 2);
			frame.DrawText(0, 0, L"가A", 0x0003);
			const auto cells = frame.GetCells();
			const auto attributes = frame.GetAttributes();
			g_resizeFault = {true, ordinal, 0, 0, 17 * 11 * sizeof(wchar_t)};
			const auto previous = _CrtSetAllocHook(ResizeAllocationHook);
			bool caught = false;
			try { frame.Resize(17, 11); }
			catch (const std::bad_alloc&) { caught = true; }
			(void)_CrtSetAllocHook(previous);
			g_resizeFault.Armed = false;
			const bool preserved = frame.GetWidth() == 3 && frame.GetHeight() == 2 &&
				frame.GetCells() == cells && frame.GetAttributes() == attributes;
			std::cout << "TRACE CORE-03 allocation=" << ordinal << " seen=" << g_resizeFault.Seen
				<< " fault-hit=" << g_resizeFault.Hits << " bad-alloc=" << caught << " preserved=" << preserved << '\n';
			assert(caught && g_resizeFault.Hits == 1 && g_resizeFault.Seen == ordinal);
			preservedAll = preservedAll && preserved;
			if (!preserved) continue; // RED에서 불변식이 깨진 객체에 DrawText를 호출하여 실제 OOB를 만들지 않는다.
			frame.DrawText(2, 1, L"Z", 0x0005);
			assert(frame.GetCells()[5] == L'Z' && frame.GetAttributes()[5] == 0x0005);
			frame.Resize(17, 11);
			assert(frame.GetWidth() == 17 && frame.GetHeight() == 11);
			assert(frame.GetCells() == std::vector<wchar_t>(17 * 11, L' '));
			assert(frame.GetAttributes() == std::vector<std::uint16_t>(17 * 11, 0x0007));
			frame.DrawText(16, 10, L"R", 0x0002);
			assert(frame.GetCells().back() == L'R' && frame.GetAttributes().back() == 0x0002);
		}
		if (!preservedAll)
		{
			std::cout << "FAIL CORE-03 throwing Resize published inconsistent dimensions/storage\n";
			std::exit(90);
		}
		g_resizeFault = {false, 1, 0, 0, 17 * 11 * sizeof(wchar_t)};
		const auto previous = _CrtSetAllocHook(ResizeAllocationHook);
		FFrameBuffer control(17, 11);
		(void)_CrtSetAllocHook(previous);
		assert(g_resizeFault.Hits == 0 && g_resizeFault.Seen == 0 && control.GetCells().size() == 17 * 11);
#else
		std::cout << "SCOPE CORE-03 CRT allocation fault injection is Debug-only\n";
#endif
		FFrameBuffer frame(3, 2);
#ifndef _WIN64
		bool lengthCaught = false;
		try { frame.Resize((std::numeric_limits<int>::max)(), (std::numeric_limits<int>::max)()); }
		catch (const std::length_error&) { lengthCaught = true; }
		assert(lengthCaught && frame.GetWidth() == 3 && frame.GetHeight() == 2 && frame.GetCells().size() == 6);
		std::cout << "TRACE CORE-03 Win32 checked-product length-error=1 previous-storage=1\n";
#endif
		frame.Resize(-1, 2);
		assert(frame.GetWidth() == 0 && frame.GetHeight() == 2 && frame.GetCells().empty() && frame.GetAttributes().empty());
		frame.DrawText(0, 1, L"ignored");
		frame.Resize(2, -1);
		assert(frame.GetWidth() == 2 && frame.GetHeight() == 0 && frame.GetCells().empty() && frame.GetAttributes().empty());
		frame.Clear();
		frame.Resize(2, 1);
		frame.DrawText(0, 0, L"ok");
		assert(frame.GetCells() == std::vector<wchar_t>({L'o', L'k'}));
		frame.Resize(2, 1);
		assert(frame.GetCells() == std::vector<wchar_t>(2, L' ') && frame.GetAttributes() == std::vector<std::uint16_t>(2, 7));
		std::cout << "PASS CORE-03 first second allocation strong rollback old drawable content recovery clamp empty reset controls\n";
	}
}
int main()
{
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	_set_abort_behavior(0, _CALL_REPORTFAULT | _WRITE_ABORT_MSG);
#ifdef _DEBUG
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
	std::cout << std::unitbuf;
	TestSerializationScalar();
	TestSerializationGraph();
	TestSerializationEnum();
	TestSerializationRegistry();
	TestSerializationEvolutionFraming();
	TestSerializationEntryBoundaries();
	TestSerializationMalformedPrimitives();
	TestSerializationCleanupAndGraphLimits();
	TestSerializationAggregateBudget();
	TestSerializationCompositionForwarding();
	TestSerializationOrderIndependentBudgets();
	TestSerializationArchivePoison();
	TestSerializationEnumWidths();
	TestFrameInheritance();
	TestFrameLifecycle();
	TestFrameOptions();
	TestBasicFrameTiming();
	TestFrameClockFailures();
	TestFixedFrameTicks();
	TestFixedFrameBudget();
	TestFixedFrameTransitionDebt();
	TestFrameSkipTiming();
	TestFrameSkipBudget();
	TestFixedFrameExactBoundaries();
	TestFrameResizeTransaction();
	// 외부 소유 핸들은 실행 중인 Scene과 직접 root callback에서 해제되지 않는다.
	FSceneHost lifetimeHost;
	auto active = std::make_unique<FConsumerScene>();
	auto pending = std::make_unique<FConsumerScene>();
	int firstQueryHits = 0;
	int forbiddenPublications = 0;
	active->BusyQuery = [&]
	{
		if (++firstQueryHits != 1) return;
		const bool nested = lifetimeHost.TryEndFrame(active.get(), pending.get(), [&] { ++forbiddenPublications; });
		assert(!nested && forbiddenPublications == 0);
	};
	assert(lifetimeHost.IsSafePointEligible(active.get(), pending.get()));
	assert(firstQueryHits == 1 && forbiddenPublications == 0);
	active->BusyQuery = {};
	int published = 0;
	auto publish = [&] { ++published; active = std::move(pending); };
	active->Callback = [&]
	{
		assert(!lifetimeHost.TryEndFrame(active.get(), pending.get(), publish));
		assert(active && pending && published == 0);
	};
	active->Update(0);
	assert(published == 0 && active && pending);
	assert(lifetimeHost.TryEndFrame(active.get(), pending.get(), [&]
	{
		assert(!lifetimeHost.TryEndFrame(active.get(), pending.get(), publish));
		publish();
	}));
	assert(published == 1 && active && !pending);
	try { (void)lifetimeHost.TryEndFrame(active.get(), nullptr, [] { throw 7; }); }
	catch (int) {}
	assert(lifetimeHost.IsSafePointEligible(active.get(), nullptr));
	std::cout << "PASS generic safe-point blocks active callbacks/reentry and recovers throw before lifetime publication\n";

	FConsumerScene scene;
	int activated = 0;
	assert(scene.Root.AddBase(std::make_unique<FConsumerUi>(activated)) != 0);
	FFrameBuffer frame;
	frame.Resize(24, 8);
	FSceneHost host;
	int callbacks = 0;
	scene.Callback = [&]
	{
		++callbacks;
		assert(scene.IsOperating());
		assert(!host.IsSafePointEligible(&scene, nullptr));
	};
	std::vector<int> order;
	FFakeFrameClock frameClock;
	FCallbackFrame pipeline({}, frameClock);
	FQueuedInput source;
	SInputEvent key;
	key.Type = EInputEventType::KeyDown;
	key.Key = EInputKey::Enter;
	key.ScreenWidth = 24;
	key.ScreenHeight = 8;
	source.Events.push_back(key);
	FInputManager input(source);
	FUIManager bridge;
	bridge.Bind(scene);
	assert(bridge.GetBoundTarget() == &scene && bridge.GetBoundRoot() == &scene.Root);
	pipeline.Input = [&] { order.push_back(1); assert(input.Update() == EInputReadResult::Event);
		assert(input.WasKeyPressed(EInputKey::Enter));
		for (const auto& event : input.GetEvents()) bridge.HandleInput(event); };
	pipeline.Update = [&](float) { order.push_back(2); scene.Update(0); };
	pipeline.Render = [&] { order.push_back(3); scene.Render(frame); };
	pipeline.EndFrame = [&] { order.push_back(4); assert(host.IsSafePointEligible(&scene, nullptr)); pipeline.Exit(); };
	const bool success = pipeline.Run() == 0;
	assert(frame.GetCells()[0] == L'┌' && frame.GetCells()[2] == L'n');
	assert(success && callbacks == 1 && activated == 1 && order == std::vector<int>({1, 2, 3, 4}));
	std::cout << "PASS standalone derived UI input/render and generic Scene safe-point/frame order\n";
	order.clear();
	FCallbackFrame failedPresent({}, frameClock);
	failedPresent.Input = [&] { order.push_back(1); };
	failedPresent.Update = [&](float) { order.push_back(2); };
	failedPresent.Render = [&] { order.push_back(3); failedPresent.Exit(1); };
	failedPresent.EndFrame = [&] { order.push_back(4); };
	assert(failedPresent.Run() != 0);
	assert(failedPresent.GetFailureStage() == EFrameFailureStage::Render);
	assert(failedPresent.GetStatistics().CompletedFrames == 0);
	assert(order == std::vector<int>({1, 2, 3}));
	std::cout << "PASS present failure does not publish EndFrame\n";
	bridge.Clear();
	assert(bridge.GetBoundTarget() == nullptr && bridge.GetBoundRoot() == nullptr);
	return 0;
}
