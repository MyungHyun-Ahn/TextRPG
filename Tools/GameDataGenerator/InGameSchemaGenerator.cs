using System.Globalization;
using System.Text;

internal sealed record FInGameType(string Name, IReadOnlyList<FInGameType> Arguments)
{
    public FGameDataEnumDefinition? EnumDefinition { get; set; }

    public string RenderCpp(string namespaceName) => Name switch
    {
        "enum" => EnumDefinition is { } definition ? "::GameData::Common::E" + definition.Name :
            throw new InvalidOperationException("Enum reference has not been bound to a validated Excel catalog."),
        "bool" or "float" or "double" => Name,
        "int32" or "uint32" or "int64" or "uint64" => "::std::" + Name + "_t",
        "string" => "::std::wstring",
        _ when Arguments.Count != 0 => "::std::" + Name + "<" + string.Join(", ", Arguments.Select(arg => arg.RenderCpp(namespaceName))) + ">",
        _ => "::" + namespaceName + "::" + Name,
    };
}
internal sealed record FInGameField(string Name, FInGameType Type, string Missing, uint Since, string Default, YamlDotNet.Core.Mark Location);
internal sealed record FInGameStruct(string Name, IReadOnlyList<FInGameField> Fields);
internal sealed record FInGameSchema(uint Version, IReadOnlyList<FInGameStruct> Types);

internal static class FInGameSchemaGenerator
{
    public static void ValidateNamespace(string namespaceName)
    {
        if (namespaceName.Length > 256 || namespaceName.Split("::", StringSplitOptions.None).Any(segment => !FInGameTypeParser.IsName(segment) || segment == "std"))
            throw new InvalidOperationException("Invalid --ingame-namespace: expected nonreserved C++ identifier segments separated by ::.");
    }

    public static int Run(FGameDataGeneratorOptions options)
    {
        string input = options.InGameSchema!;
        string output = options.InGameOutputRoot!;
        FOutputManager.ValidateRoots([Path.GetDirectoryName(input)!, output]);
        FInGameSchema schema = Parse(input, options.InputRoot);
        if (schema.Types.SelectMany(type => type.Fields).Any(field => EnumReferences(field.Type).Any()))
            FOutputManager.ValidateRoots([options.InputRoot, output]);
        if (!options.ValidateOnly) FOutputManager.ApplyInGame(output, Generate(schema, options.InGameNamespace), options.Check);
        Console.WriteLine($"InGameData schema validated: {schema.Types.Count} type(s); " +
            (options.ValidateOnly ? "generation skipped." : options.Check ? "output check passed." : "output generated."));
        return 0;
    }

    public static FInGameSchema Parse(string file, string? inputRoot = null)
    {
        FInGameYamlNode root = FInGameYaml.Read(file);
        var mapping = Map(root, "schema_version", "types");
        uint version = Positive(Required(mapping, "schema_version", root));
        FInGameYamlNode typesNode = Required(mapping, "types", root);
        var types = new List<FInGameStruct>();
        var typeNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (FInGameYamlNode item in Sequence(typesNode))
        {
            var type = Map(item, "name", "fields");
            string name = Text(Required(type, "name", item));
            Identifier(name, item);
            if (FInGameTypeParser.Primitives.Contains(name) || FInGameTypeParser.Templates.Contains(name) ||
                System.Text.RegularExpressions.Regex.IsMatch(name, "^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$", System.Text.RegularExpressions.RegexOptions.IgnoreCase | System.Text.RegularExpressions.RegexOptions.CultureInvariant) ||
                name.Equals("InGameDataAll", StringComparison.OrdinalIgnoreCase) ||
                name.Equals("InGameDataRegistry", StringComparison.OrdinalIgnoreCase) ||
                name is "kSchemaVersion" or "RegisterInGameDataTypes" or
                    "kTypeName" or "SerializeFields" or "DeserializeFields" or "GetSerializableTypeName")
                throw FInGameYaml.Error(file, type["name"].Mark, "Reserved type name: " + name);
            if (!typeNames.Add(name)) throw FInGameYaml.Error(file, item.Mark, "Duplicate type: " + name);
            if (typeNames.Count > 256) throw FInGameYaml.Error(file, item.Mark, "Type count limit exceeded.");
            var fields = new List<FInGameField>();
            var fieldNames = new HashSet<string>(StringComparer.Ordinal);
            foreach (FInGameYamlNode fieldNode in Sequence(Required(type, "fields", item)))
            {
                var field = Map(fieldNode, "name", "type", "missing", "since", "default");
                string fieldName = Text(Required(field, "name", fieldNode));
                Identifier(fieldName, fieldNode);
                if (fieldName == name || fieldName is "Serialize" or "Deserialize" or "SerializeFields" or "DeserializeFields" or
                    "GetSerializableTypeName" or "kTypeName" or "OnBeforeSerialize" or "OnAfterDeserialize")
                    throw FInGameYaml.Error(file, fieldNode.Mark, "Reserved field name: " + fieldName);
                if (!fieldNames.Add(fieldName)) throw FInGameYaml.Error(file, fieldNode.Mark, "Duplicate field: " + fieldName);
                if (fieldNames.Count > 256) throw FInGameYaml.Error(file, fieldNode.Mark, "Field count limit exceeded.");
                string sourceType = Text(Required(field, "type", fieldNode));
                FInGameType parsedType = FInGameTypeParser.Parse(sourceType, file, field["type"].Mark);
                string missing = Text(Required(field, "missing", fieldNode));
                if (missing is not ("reject" or "default")) throw FInGameYaml.Error(file, field["missing"].Mark, "missing must be reject or default.");
                uint since = Positive(Required(field, "since", fieldNode));
                if (since > version) throw FInGameYaml.Error(file, field["since"].Mark, "since exceeds schema_version.");
                if (field.TryGetValue("default", out var defaultNode))
                {
                    if (missing != "default") throw FInGameYaml.Error(file, defaultNode.Mark, "default requires missing: default.");
                    if (!NeutralDefault(parsedType, defaultNode)) throw FInGameYaml.Error(file, defaultNode.Mark, "Expected a compatible neutral default; gameplay defaults remain in Excel.");
                }
                fields.Add(new(fieldName, parsedType, missing, since, "{}", field["type"].Mark));
            }
            types.Add(new(name, fields));
        }
        var ordered = new List<FInGameStruct>();
        var visited = new HashSet<string>(StringComparer.Ordinal);
        var active = new HashSet<string>(StringComparer.Ordinal);
        var byName = types.ToDictionary(type => type.Name, StringComparer.Ordinal);
        IReadOnlyDictionary<string, FGameDataEnumDefinition>? catalog = null;
        foreach (var type in types)
            foreach (var field in type.Fields)
            {
                ValidateType(field.Type, field.Location);
                if (field.Missing == "default" && field.Type.EnumDefinition is { } definition &&
                    !definition.Values.Any(value => value.NumericValue == 0))
                    throw FInGameYaml.Error(file, field.Location, "Enum neutral default requires a catalog value equal to zero: " + definition.Name);
            }
        void ValidateType(FInGameType type, YamlDotNet.Core.Mark mark)
        {
            if (type.Name == "enum")
            {
                if (inputRoot == null || !Directory.Exists(inputRoot))
                    throw FInGameYaml.Error(file, mark, "Enum catalog input root is required and must exist (--input-root).");
                if (catalog == null)
                {
                    var diagnostics = new FDiagnosticBag();
                    var tables = FWorkbookParser.ParseDirectory(inputRoot, diagnostics);
                    var enumTables = tables.Where(FGameDataEnumCatalog.IsDefinitionTable).ToArray();
                    catalog = enumTables.Length == 0 ? FGameDataEnumCatalog.Build(tables, diagnostics) :
                        FGameDataValidator.Validate(enumTables, diagnostics);
                    if (diagnostics.HasErrors)
                        throw new InvalidOperationException(string.Join('\n', diagnostics.GetSorted()));
                }
                string name = type.Arguments[0].Name;
                if (!catalog.TryGetValue(name, out var definition))
                    throw FInGameYaml.Error(file, mark, "Unknown enum in Excel catalog: " + name);
                if (!definition.IsIncludedOnClient)
                    throw FInGameYaml.Error(file, mark, "Enum is not available on Client: " + name);
                if (!FInGameTypeParser.IsName("E" + definition.Name) || definition.Values.Any(value => !FInGameTypeParser.IsName(value.Name)))
                    throw FInGameYaml.Error(file, mark, "Enum catalog contains a reserved C++ identifier: " + name);
                type.EnumDefinition = definition;
                return;
            }
            if (!FInGameTypeParser.Primitives.Contains(type.Name) && !FInGameTypeParser.Templates.Contains(type.Name) && !byName.ContainsKey(type.Name))
                throw FInGameYaml.Error(file, mark, "Unknown type: " + type.Name);
            foreach (var argument in type.Arguments) ValidateType(argument, mark);
            if (type.Name is "map" or "unordered_map" && type.Arguments[0].Name is not ("int32" or "uint32" or "int64" or "uint64" or "string" or "enum"))
                throw FInGameYaml.Error(file, mark, "Map keys must be integers, string or validated enums.");
            if (type.Name is "shared_ptr" or "unique_ptr" && (type.Arguments[0].Arguments.Count != 0 || !byName.ContainsKey(type.Arguments[0].Name)))
                throw FInGameYaml.Error(file, mark, "Pointer target must be a generated object type.");
        }
        void Visit(FInGameStruct type)
        {
            if (visited.Contains(type.Name)) return;
            active.Add(type.Name);
            foreach (var field in type.Fields)
                foreach (string dependency in FInGameTypeParser.ValueDependencies(field.Type).Distinct(StringComparer.Ordinal).Order(StringComparer.Ordinal))
                {
                    if (active.Contains(dependency)) throw FInGameYaml.Error(file, field.Location, "Value cycle: " + type.Name + "." + field.Name + " -> " + dependency);
                    Visit(byName[dependency]);
                }
            active.Remove(type.Name);
            visited.Add(type.Name);
            ordered.Add(type);
        }
        foreach (var type in types.OrderBy(type => type.Name, StringComparer.Ordinal)) Visit(type);
        return new(version, ordered);

        IReadOnlyDictionary<string, FInGameYamlNode> Map(FInGameYamlNode node, params string[] allowed)
        {
            if (node.Members == null) throw FInGameYaml.Error(file, node.Mark, "Expected a mapping.");
            foreach (string key in node.Members.Keys)
                if (!allowed.Contains(key, StringComparer.Ordinal)) throw FInGameYaml.Error(file, node.Members[key].Mark, "Unknown metadata: " + key);
            return node.Members;
        }
        IReadOnlyList<FInGameYamlNode> Sequence(FInGameYamlNode node) => node.Items ?? throw FInGameYaml.Error(file, node.Mark, "Expected a sequence.");
        FInGameYamlNode Required(IReadOnlyDictionary<string, FInGameYamlNode> map, string key, FInGameYamlNode owner) =>
            map.TryGetValue(key, out var node) ? node : throw FInGameYaml.Error(file, owner.Mark, "Missing metadata: " + key);
        string Text(FInGameYamlNode node) => node.Scalar ?? throw FInGameYaml.Error(file, node.Mark, "Expected a scalar.");
        uint Positive(FInGameYamlNode node) => uint.TryParse(Text(node), NumberStyles.None, CultureInfo.InvariantCulture, out uint value) && value > 0
            ? value : throw FInGameYaml.Error(file, node.Mark, "Expected a positive schema version.");
        void Identifier(string value, FInGameYamlNode node)
        {
            if (!FInGameTypeParser.IsName(value))
                throw FInGameYaml.Error(file, node.Mark, "Invalid identifier: " + value);
        }

        bool NeutralDefault(FInGameType type, FInGameYamlNode node) => type.Name switch
        {
            "string" => node.Scalar == "" && !node.Plain,
            "bool" => node.Plain && node.Scalar == "false",
            "int32" or "uint32" or "int64" or "uint64" => node.Plain && node.Scalar == "0",
            "float" or "double" => node.Plain && (node.Scalar == "0" || node.Scalar == "0.0"),
            "enum" => node.Plain && node.Scalar == "0",
            "unique_ptr" or "shared_ptr" => node.Plain && node.Scalar == "null",
            "vector" => node.Items?.Count == 0,
            _ => node.Members?.Count == 0,
        };
    }

    public static IReadOnlyList<SGeneratedFile> Generate(FInGameSchema schema, string namespaceName = "GameData::InGameData")
    {
        ValidateNamespace(namespaceName);
        string Cpp(FInGameType type) => type.RenderCpp(namespaceName);
        var files = new List<SGeneratedFile> { new(FOutputManager.InGameMarkerName, FOutputManager.InGameMarkerContent) };
        var aggregate = new StringBuilder("#pragma once\n\n// Generated by GameDataGenerator. Do not edit.\n");
        aggregate.Append("#include <cstdint>\n#include <string>\n#include <string_view>\n#include <memory>\n#include <utility>\n#include <vector>\n#include <map>\n#include <unordered_map>\n");
        aggregate.Append("#include <algorithm>\n#include <bit>\n#include <cmath>\n#include <cstddef>\n#include <functional>\n#include <set>\n#include <span>\n#include <type_traits>\n");
        aggregate.Append("#include \"Serialization/ISerializable.h\"\n#include \"Serialization/FSerializationArchive.h\"\n");
        var enums = schema.Types.SelectMany(type => type.Fields).SelectMany(field => EnumReferences(field.Type))
            .Select(type => type.EnumDefinition ?? throw new InvalidOperationException("Unbound enum reference."))
            .DistinctBy(definition => definition.Name).OrderBy(definition => definition.Name, StringComparer.Ordinal).ToArray();
        if (enums.Length != 0) aggregate.Append("#include \"Common/GameDataEnums.g.h\"\n");
        foreach (var definition in enums)
        {
            string cppName = "::GameData::Common::E" + definition.Name;
            string guard = "TEXTRPG_SERIALIZATION_ENUM_GameData_Common_E" + definition.Name;
            aggregate.Append($"\n#ifndef {guard}\n#define {guard}\nnamespace TextRPG\n{{\n");
            aggregate.Append($"\ttemplate<> struct TSerializationEnumTraits<{cppName}>\n\t{{\n");
            aggregate.Append($"\t\tstatic constexpr ::std::string_view kTypeName = \"E{definition.Name}\";\n");
            aggregate.Append($"\t\tstatic bool IsValid({cppName} _value) noexcept\n\t\t{{\n\t\t\tswitch (_value)\n\t\t\t{{\n");
            foreach (var value in definition.Values) aggregate.Append($"\t\t\tcase {cppName}::{value.Name}:\n");
            aggregate.Append("\t\t\t\treturn true;\n\t\t\tdefault: return false;\n\t\t\t}\n\t\t}\n\t};\n}\n#endif\n");
        }
        aggregate.Append("\nnamespace GameData::InGameData\n{\n");
        aggregate.Append($"\tinline constexpr ::std::uint32_t kSchemaVersion = {schema.Version}u;\n");
        foreach (var type in schema.Types) aggregate.Append($"\tstruct {type.Name};\n");
        aggregate.Append("}\n\n");
        foreach (var type in schema.Types)
        {
            aggregate.Append($"#include \"{type.Name}.g.h\"\n");
            var header = new StringBuilder("#pragma once\n\n// Generated by GameDataGenerator. Do not edit.\n// 소비 번역 단위: InGameDataAll.h 또는 동등한 표준/코어/의존 타입 전제가 필요하다.\n\nnamespace GameData::InGameData\n{\n");
            header.Append($"\tstruct {type.Name} : public ::TextRPG::ISerializable\n\t{{\n");
            header.Append($"\t\tstatic constexpr ::std::string_view kTypeName = \"{type.Name}\";\n");
            header.Append($"\t\t{type.Name}();\n\t\t~{type.Name}() override;\n");
            header.Append($"\t\t{type.Name}({type.Name}&& _other) noexcept;\n\t\t{type.Name}& operator=({type.Name}&& _other) noexcept;\n");
            bool copyable = !type.Fields.Any(field => HasUnique(field.Type, new HashSet<string>(StringComparer.Ordinal)));
            header.Append($"\t\t{type.Name}(const {type.Name}& _other) = " + (copyable ? "default" : "delete") + ";\n");
            header.Append($"\t\t{type.Name}& operator=(const {type.Name}& _other) = " + (copyable ? "default" : "delete") + ";\n");
            if (type.Fields.Count != 0)
                header.Append($"\t\t{type.Name}(\n\t\t\t" + string.Join(",\n\t\t\t", type.Fields.Select((field, index) => Cpp(field.Type) + " _field" + index)) + ");\n");
            foreach (var field in type.Fields) header.Append($"\t\t{Cpp(field.Type)} {field.Name}{field.Default};\n");
            header.Append($"\t\tbool operator==(const {type.Name}& _other) const;\n");
            header.Append("\t\t::std::string_view GetSerializableTypeName() const noexcept final;\n\n\tprotected:\n");
            header.Append("\t\tbool SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const final;\n\t\tbool DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error) final;\n\t};\n}\n");
            files.Add(new(type.Name + ".g.h", header.ToString()));
            var cpp = new StringBuilder("#include \"GameData/pch.h\"\n#include \"Serialization/FSerializationArchive.h\"\n#include \"InGameDataAll.h\"\n\nnamespace GameData::InGameData\n{\n");
            cpp.Append($"\t{type.Name}::{type.Name}() = default;\n\t{type.Name}::~{type.Name}() = default;\n\n");
            cpp.Append($"\t{type.Name}::{type.Name}({type.Name}&& _other) noexcept = default;\n\t{type.Name}& {type.Name}::operator=({type.Name}&& _other) noexcept = default;\n\n");
            if (type.Fields.Count != 0)
            {
                cpp.Append($"\t{type.Name}::{type.Name}(\n\t\t" + string.Join(",\n\t\t", type.Fields.Select((field, index) => Cpp(field.Type) + " _field" + index)) + ")\n");
                cpp.Append(string.Join("\n", type.Fields.Select((field, index) => $"\t\t{(index == 0 ? ':' : ',')} {field.Name}(::std::move(_field{index}))")));
                cpp.Append("\n\t{\n\t}\n\n");
            }
            cpp.Append($"\tbool {type.Name}::operator==(const {type.Name}& _other) const\n\t{{\n");
            if (type.Fields.Count == 0) cpp.Append("\t\tstatic_cast<void>(_other);\n");
            cpp.Append("\t\treturn " + (type.Fields.Count == 0 ? "true" : string.Join(" &&\n\t\t\t", type.Fields.Select(field => "this->" + field.Name + " == _other." + field.Name))) + ";\n\t}\n\n");
            cpp.Append($"\t::std::string_view {type.Name}::GetSerializableTypeName() const noexcept\n\t{{\n\t\treturn kTypeName;\n\t}}\n\n");
            cpp.Append($"\tbool {type.Name}::SerializeFields(::TextRPG::FBinaryWriter& _writer, ::std::string& _error) const\n\t{{\n");
            if (type.Fields.Count == 0) cpp.Append("\t\tstatic_cast<void>(_writer);\n\t\tstatic_cast<void>(_error);\n");
            foreach (var field in type.Fields) cpp.Append($"\t\tif (!_writer.Field(\"{field.Name}\", this->{field.Name}, _error)) return false;\n");
            cpp.Append("\t\treturn true;\n\t}\n\n");
            cpp.Append($"\tbool {type.Name}::DeserializeFields(::TextRPG::FBinaryReader& _reader, ::std::string& _error)\n\t{{\n");
            if (type.Fields.Count == 0) cpp.Append("\t\tstatic_cast<void>(_reader);\n\t\tstatic_cast<void>(_error);\n");
            foreach (var field in type.Fields)
            {
                if (field.Missing == "default") cpp.Append($"\t\tif (_reader.GetSchemaVersion() < {field.Since}u) this->{field.Name} = {Cpp(field.Type)}{field.Default};\n");
                cpp.Append($"\t\tif (!_reader.Field(\"{field.Name}\", this->{field.Name}, _error, " +
                    (field.Missing == "default" ? $"_reader.GetSchemaVersion() >= {field.Since}u" : "true") + ")) return false;\n");
            }
            cpp.Append("\t\treturn true;\n\t}\n}\n");
            files.Add(new(type.Name + ".g.cpp", cpp.ToString()));
        }
        aggregate.Append("\nnamespace GameData::InGameData\n{\n\tbool RegisterInGameDataTypes(::TextRPG::FSerializableTypeRegistry& _registry, ::std::string& _error);\n}\n");
        files.Add(new("InGameDataAll.h", aggregate.ToString()));
        var _registry = new StringBuilder("#include \"GameData/pch.h\"\n#include \"Serialization/FSerializationArchive.h\"\n#include \"InGameDataAll.h\"\n\nnamespace GameData::InGameData\n{\n\tbool RegisterInGameDataTypes(::TextRPG::FSerializableTypeRegistry& _registry, ::std::string& _error)\n\t{\n");
        if (schema.Types.Count == 0) _registry.Append("\t\tstatic_cast<void>(_registry);\n\t\tstatic_cast<void>(_error);\n");
        foreach (var type in schema.Types)
            _registry.Append($"\t\tif (!_registry.Register<{type.Name}>({type.Name}::kTypeName, []() {{ return ::std::make_unique<{type.Name}>(); }}, _error)) return false;\n");
        _registry.Append("\t\treturn true;\n\t}\n}\n");
        files.Add(new("InGameDataRegistry.g.cpp", _registry.ToString()));
        return files.Select(file => file with { Content = file.Content.Replace("namespace GameData::InGameData", "namespace " + namespaceName, StringComparison.Ordinal) }).ToArray();

        bool HasUnique(FInGameType node, HashSet<string> visited)
        {
            if (node.Name == "unique_ptr") return true;
            if (node.Name == "shared_ptr") return false;
            if (node.Arguments.Any(argument => HasUnique(argument, visited))) return true;
            if (visited.Add(node.Name) && schema.Types.FirstOrDefault(type => type.Name == node.Name) is { } nested)
                return nested.Fields.Any(field => HasUnique(field.Type, visited));
            return false;
        }
    }

    private static IEnumerable<FInGameType> EnumReferences(FInGameType type)
    {
        if (type.Name == "enum") { yield return type; yield break; }
        foreach (var argument in type.Arguments)
            foreach (var reference in EnumReferences(argument)) yield return reference;
    }
}
