#pragma once

namespace TextRPG
{
	class FBinaryWriter;
	class FBinaryReader;

	class ISerializable
	{
	public:
		virtual ~ISerializable() = default;
		virtual std::string_view GetSerializableTypeName() const noexcept = 0;
		bool Serialize(FBinaryWriter& writer, std::string& error) const;
		bool Deserialize(FBinaryReader& reader, std::string& error);
	protected:
		static bool ForwardSerializeFields(const ISerializable& data, FBinaryWriter& writer, std::string& error);
		static bool ForwardDeserializeFields(ISerializable& data, FBinaryReader& reader, std::string& error);
		virtual bool SerializeFields(FBinaryWriter& writer, std::string& error) const = 0;
		virtual bool DeserializeFields(FBinaryReader& reader, std::string& error) = 0;
		virtual bool OnBeforeSerialize(std::string&) const { return true; }
		virtual bool OnAfterDeserialize(std::string&) { return true; }
	};
}
