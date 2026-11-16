internal static class FInGameSchemaSelfTest
{
    public static void Run(string testRoot, string catalogRoot)
    {
        string root = Path.Combine(testRoot, "InGame");
        string input = Path.Combine(root, "Input", "Types.yaml");
        string output = Path.Combine(root, "Generated");
        Directory.CreateDirectory(Path.GetDirectoryName(input)!);
        File.WriteAllText(input, """
            # 실제 블록 YAML 입력
            schema_version: 1
            types:
              - name: SLeaf
                fields:
                  - name: Count
                    type: int32
                    missing: reject
                    since: 1
            """);
        string[] args = ["--ingame-schema", input, "--ingame-output-root", output];
        Require(Program.Main(args) == 0, "YAML schema CLI generation must succeed");
        Require(File.Exists(Path.Combine(output, "SLeaf.g.h")), "schema type header missing");
        Require(File.ReadAllText(Path.Combine(output, "SLeaf.g.cpp")).StartsWith("#include \"GameData/pch.h\"\n", StringComparison.Ordinal), "generated CPP PCH must be first");
        Require(Program.Main([.. args, "--check"]) == 0, "schema check must succeed");
        FGameDataGeneratorOptions defaults = FGameDataGeneratorOptions.Parse([]);
        Require(defaults.OutputRoot.EndsWith(Path.Combine("GameData", "Generated", "GameData"), StringComparison.Ordinal), "static default root must move to Generated/GameData");
        TestReservedTypeNames(root, input);
        TestEnums(root, input, output, catalogRoot);
        TestRecursiveTypes(root, input, output, args);
        TestValidation(input);
        TestNamespace(root, input);
        TestOutputSafety(root, input, output, args);
        TestGeneratedMemberSafety(input);
        TestBounds(input);
        Console.WriteLine("InGameData YAML CLI vertical slice passed.");
    }

    private static void TestReservedTypeNames(string root, string input)
    {
        string output = Path.Combine(root, "ReservedNames");
        string absent = Path.Combine(root, "ReservedNamesAbsent", "Generated");
        string[] positiveNames = ["SAllowedType", "KTypeName", "serializeFields", "deserializeFields", "getSerializableTypeName",
            "kTypeNameExtra", "SerializeFieldsExtra", "DeserializeFieldsExtra", "GetSerializableTypeNameExtra",
            "InGameDataAllExtra", "InGameDataRegistryExtra", "kSchemaVersionExtra", "RegisterInGameDataTypesExtra"];
        File.WriteAllText(input, "schema_version: 1\ntypes:\n" + string.Join("\n", positiveNames.Select(name =>
            "  - {name: " + name + ", fields: [{name: Value, type: int32, missing: reject, since: 1}]}")));
        string[] args = ["--ingame-schema", input, "--ingame-output-root", output];
        Require(Program.Main(args) == 0, "reserved type names complete positive control generation");
        foreach (string[] mode in new[] { new[] { "--validate-only" }, new[] { "--check" } })
            Require(Program.Main([.. args, .. mode]) == 0, "reserved type names positive control mode");
        foreach (string name in positiveNames)
        {
            string header = File.ReadAllText(Path.Combine(output, name + ".g.h"));
            Require(header.Contains("struct " + name + " :", StringComparison.Ordinal) &&
                header.Contains("kTypeName = \"" + name + "\"", StringComparison.Ordinal), "positive identity must not be normalized: " + name);
        }
        string before = TreeSnapshot(output);
        string[] reservedNames = ["kTypeName", "SerializeFields", "DeserializeFields", "GetSerializableTypeName",
            "ingamedataregistry", "InGameDataRegistry", "INGAMEDATAREGISTRY", "iNgAmEdAtArEgIsTrY",
            "ingamedataall", "InGameDataAll", "INGAMEDATAALL", "iNgAmEdAtAaLl", "kSchemaVersion", "RegisterInGameDataTypes"];
        foreach (string name in reservedNames)
        {
            File.WriteAllText(input, "schema_version: 1\ntypes:\n  - name: " + name + "\n    fields: []\n");
            string inputBefore = TreeSnapshot(Path.GetDirectoryName(input)!);
            string expected = input + "(3,11): IGD1001: Reserved type name: " + name;
            string? actual = null;
            try { _ = FInGameSchemaGenerator.Parse(input); }
            catch (InvalidOperationException exception) { actual = exception.Message; }
            Require(actual == expected, "exact reserved type diagnostic; expected=[" + expected + "] actual=[" + actual + "]");
            foreach (string destination in new[] { output, absent })
                foreach (string[] mode in new[] { new[] { "--validate-only" }, new[] { "--check" }, Array.Empty<string>() })
                {
                    var diagnostic = new StringWriter();
                    TextWriter previous = Console.Error;
                    int exit;
                    try
                    {
                        Console.SetError(diagnostic);
                        exit = Program.Main(["--ingame-schema", input, "--ingame-output-root", destination, .. mode]);
                    }
                    finally { Console.SetError(previous); }
                    Require(exit == 1 && diagnostic.ToString().Split('\n', StringSplitOptions.RemoveEmptyEntries).Last().TrimEnd('\r') ==
                        "GameDataGenerator failed: " + expected, "CLI exact reserved type diagnostic: " + name + " " + string.Join(' ', mode));
                    Require(TreeSnapshot(output) == before && !Directory.Exists(Path.GetDirectoryName(absent)) &&
                        TreeSnapshot(Path.GetDirectoryName(input)!) == inputBefore, "reserved type failure must preserve input/output bytes, mtimes and directory tree");
                }
        }
        Console.WriteLine($"InGameData reserved type exact located CLI negatives passed: {reservedNames.Length} names x 3 modes x 2 output states; complete near-miss positives passed.");
    }

    private static string TreeSnapshot(string root) => Snapshot(root) + "\nDirectories:\n" + string.Join("\n",
        Directory.GetDirectories(root, "*", SearchOption.AllDirectories).Select(path => Path.GetRelativePath(root, path)).Order(StringComparer.Ordinal));

    private static void TestEnums(string root, string input, string output, string catalogRoot)
    {
        File.WriteAllText(input, """
            schema_version: 1
            types:
              - name: SEnumFixture
                fields:
                  - {name: Kind, type: 'enum<TestKind>', missing: reject, since: 1}
                  - {name: Kinds, type: 'vector<enum<TestKind>>', missing: reject, since: 1}
                  - {name: Ordered, type: 'map<enum<TestKind>, enum<TestKind>>', missing: reject, since: 1}
                  - {name: Hashed, type: 'unordered_map<enum<TestKind>, vector<enum<TestKind>>>', missing: reject, since: 1}
            """);
        string[] args = ["--ingame-schema", input, "--ingame-output-root", output, "--input-root", catalogRoot];
        Require(Program.Main(args) == 0, "validated Excel enum reference must generate");
        string header = File.ReadAllText(Path.Combine(output, "SEnumFixture.g.h"));
        Require(header.Contains("::GameData::Common::ETestKind Kind", StringComparison.Ordinal) &&
            header.Contains("::std::map<::GameData::Common::ETestKind, ::GameData::Common::ETestKind>", StringComparison.Ordinal) &&
            !header.Contains("#include", StringComparison.Ordinal), "enum AST binding and include-free fields");
        string aggregate = File.ReadAllText(Path.Combine(output, "InGameDataAll.h"));
        Require(aggregate.Contains("#include \"Common/GameDataEnums.g.h\"", StringComparison.Ordinal) &&
            aggregate.Contains("TSerializationEnumTraits<::GameData::Common::ETestKind>", StringComparison.Ordinal) &&
            aggregate.Contains("kTypeName = \"ETestKind\"", StringComparison.Ordinal) &&
            aggregate.Contains("IsValid(::GameData::Common::ETestKind _value) noexcept", StringComparison.Ordinal) &&
            aggregate.Contains("case ::GameData::Common::ETestKind::One:", StringComparison.Ordinal) &&
            aggregate.Contains("#ifndef TEXTRPG_SERIALIZATION_ENUM_GameData_Common_ETestKind", StringComparison.Ordinal),
            "validated enum traits, Core-compatible identity and cross-schema specialization guard");
        Require(Program.Main([.. args, "--check"]) == 0, "enum output check");
        TestEnumNegatives(root, input, output, catalogRoot, args);
        Console.WriteLine("InGameData validated Excel enum scalar/container/map-key generation passed.");
    }

    private static void TestEnumNegatives(string root, string input, string output, string catalogRoot, string[] args)
    {
        string source = File.ReadAllText(input);
        string before = Snapshot(output);
        string catalogBefore = Snapshot(catalogRoot);
        void Reject(string type, string? catalog, string expected)
        {
            File.WriteAllText(input, "schema_version: 1\ntypes:\n  - name: SEnumFixture\n    fields:\n      - {name: Kind, type: '" + type + "', missing: reject, since: 1}\n");
            string inputBefore = File.ReadAllText(input);
            string? actual = null;
            try { _ = FInGameSchemaGenerator.Parse(input, catalog); }
            catch (InvalidOperationException exception) { actual = exception.Message; }
            Require(actual == expected, "exact enum diagnostic mismatch; expected=[" + expected + "] actual=[" + actual + "]");
            string[] negativeArgs = ["--ingame-schema", input, "--ingame-output-root", output, "--input-root", catalog ?? Path.Combine(root, "AbsentCatalog")];
            foreach (string[] mode in new[] { Array.Empty<string>(), new[] { "--check" }, new[] { "--validate-only" } })
                Require(Program.Main([.. negativeArgs, .. mode]) == 1 && Snapshot(output) == before && File.ReadAllText(input) == inputBefore,
                    "enum failure must preserve schema/output bytes and mtimes");
            Require(Snapshot(catalogRoot) == catalogBefore, "enum lookup changed original Excel catalog");
        }
        string located = input + "(5,28): IGD1001: ";
        Reject("enum<TestKind>", null, located + "Enum catalog input root is required and must exist (--input-root).");
        Reject("enum<MissingKind>", catalogRoot, located + "Unknown enum in Excel catalog: MissingKind");
        Reject("enum<testkind>", catalogRoot, located + "Unknown enum in Excel catalog: testkind");
        string nestedRoot = Path.Combine(root, "EnumCatalog");
        string workbookPath = Path.Combine(nestedRoot, "Nested", "Enums.xlsx");
        Directory.CreateDirectory(Path.GetDirectoryName(workbookPath)!);
        File.Copy(Path.Combine(catalogRoot, "Enums.xlsx"), workbookPath);
        using (var workbook = new ClosedXML.Excel.XLWorkbook(workbookPath))
        {
            workbook.Worksheet("Enums").Cell("B1").Value = "Other";
            workbook.Save();
        }
        Reject("enum<TestKind>", nestedRoot, workbookPath + " [Enums] B1: error GD0901: A shared enum definition table named 'Enums' is required.");
        File.Copy(Path.Combine(catalogRoot, "Enums.xlsx"), workbookPath, overwrite: true);
        using (var workbook = new ClosedXML.Excel.XLWorkbook(workbookPath))
        {
            workbook.Worksheet("Enums").Cell("C14").Value = "Server";
            workbook.Save();
        }
        Reject("enum<TestKind>", nestedRoot, workbookPath + " [Enums] B14: error GD1903: TextRPG GameDataGenerator is client-only; enum Target cannot be Server.");
        File.Copy(Path.Combine(catalogRoot, "Enums.xlsx"), workbookPath, overwrite: true);
        using (var workbook = new ClosedXML.Excel.XLWorkbook(workbookPath))
        {
            var sheet = workbook.Worksheet("Enums");
            sheet.Cell("G5").Value = "Shared";
            sheet.Cell("G6").Value = "string";
            sheet.Cell("G7").Value = "true";
            sheet.Cell("G13").Value = "Values";
            sheet.Cell("G14").Value = "One=1|Two=2";
            workbook.Save();
        }
        Reject("enum<TestKind>", nestedRoot, workbookPath + " [Enums] E13: error GD0303: Duplicate field name 'Values'.\n" +
            workbookPath + " [Enums] G13: error GD0303: Duplicate field name 'Values'.\n" +
            workbookPath + " [Enums] G13: error GD0312: Field 'Values' collides with another generated C++ member name.\n" +
            workbookPath + " [Enums] G13: error GD0313: Field 'Values' collides with another generated C# property name.");
        File.Copy(Path.Combine(catalogRoot, "Enums.xlsx"), workbookPath, overwrite: true);
        foreach (var item in new[]
        {
            ("One=1|Two=1", "GD0906", "Enum 'TestKind' numeric value '1' is duplicated."),
            ("One=256|Two=2", "GD0907", "Enum 'TestKind' value 'One=256' exceeds underlying type 'uint8'."),
            ("One=1|Bad;Call=2", "GD0906", "Enum 'TestKind' value 'Bad' must use Name=Number."),
        })
        {
            using (var workbook = new ClosedXML.Excel.XLWorkbook(workbookPath))
            {
                workbook.Worksheet("Enums").Cell("E14").Value = item.Item1;
                workbook.Save();
            }
            Reject("enum<TestKind>", nestedRoot, workbookPath + " [Enums] E14: error " + item.Item2 + ": " + item.Item3);
        }
        File.Copy(Path.Combine(catalogRoot, "Enums.xlsx"), workbookPath, overwrite: true);
        File.WriteAllText(input, source);
        Require(Program.Main(["--ingame-schema", input, "--ingame-output-root", output, "--input-root", nestedRoot]) == 0,
            "nested --input-root enum catalog lookup");
        Require(Snapshot(output) == before, "identical validated enum catalog must preserve output bytes/mtimes");
        File.WriteAllText(input, "schema_version: 2\ntypes: [{name: SDefault, fields: [{name: Kind, type: 'enum<TestKind>', missing: default, since: 2, default: 0}]}]");
        bool badDefault = false;
        try { _ = FInGameSchemaGenerator.Parse(input, catalogRoot); }
        catch (InvalidOperationException exception) { badDefault = exception.Message.EndsWith("Enum neutral default requires a catalog value equal to zero: TestKind", StringComparison.Ordinal); }
        Require(badDefault, "enum neutral defaults must be validated against Excel zero value");
        foreach (var item in new[] { ("uint8", "Zero=0|Maximum=255"), ("uint16", "Zero=0|Maximum=65535"),
            ("int32", "Minimum=-2147483648|Zero=0|Maximum=2147483647"), ("uint32", "Zero=0|Maximum=4294967295") })
        {
            using (var workbook = new ClosedXML.Excel.XLWorkbook(workbookPath))
            {
                workbook.Worksheet("Enums").Cell("D14").Value = item.Item1;
                workbook.Worksheet("Enums").Cell("E14").Value = item.Item2;
                workbook.Save();
            }
            var schema = FInGameSchemaGenerator.Parse(input, nestedRoot);
            Require(FInGameSchemaGenerator.Generate(schema, "OtherFixture::InGameData").Any(file =>
                file.RelativePath == "InGameDataAll.h" && file.Content.Contains("#ifndef TEXTRPG_SERIALIZATION_ENUM_GameData_Common_ETestKind", StringComparison.Ordinal)),
                "enum width boundary/zero default and namespace-independent specialization guard: " + item.Item1);
        }
        File.WriteAllText(input, "schema_version: 1\ntypes: [{name: SIndependent, fields: [{name: Value, type: int32, missing: reject, since: 1}]}]");
        Require(Program.Main(["--ingame-schema", input, "--ingame-output-root", output, "--input-root", Path.Combine(root, "AbsentCatalog"), "--validate-only"]) == 0,
            "non-enum schemas must remain independent of missing Excel");
        File.WriteAllText(input, source);
        Require(Program.Main(args) == 0, "restore enum output after negative cases");
        Console.WriteLine("InGameData exact located enum catalog negatives and output/input preservation passed.");
    }

    private static void TestRecursiveTypes(string root, string input, string output, string[] args)
    {
        File.WriteAllText(input, """
            schema_version: 2
            types:
              - name: SRoot
                fields:
                  - {name: Leaf, type: SLeaf, missing: reject, since: 1}
                  - {name: Shared, type: 'vector<map<string, unordered_map<uint32, shared_ptr<SLeaf>>>>', missing: default, since: 2, default: []}
                  - {name: Unique, type: 'unique_ptr<SLeaf>', missing: default, since: 2, default: null}
                  - {name: Flag, type: bool, missing: default, since: 2, default: false}
                  - {name: Ratio, type: double, missing: default, since: 2, default: 0}
                  - {name: Label, type: string, missing: default, since: 2, default: ''}
              - name: SLeaf
                fields:
                  - {name: Count, type: int32, missing: reject, since: 1}
                  - {name: Parent, type: 'shared_ptr<SRoot>', missing: reject, since: 1}
            """);
        Require(Program.Main(args) == 0, "recursive container/value/shared/unique YAML must generate");
        string aggregate = File.ReadAllText(Path.Combine(output, "InGameDataAll.h"));
        Require(aggregate.IndexOf("#include \"SLeaf.g.h\"", StringComparison.Ordinal) < aggregate.IndexOf("#include \"SRoot.g.h\"", StringComparison.Ordinal), "by-value dependency order");
        string header = File.ReadAllText(Path.Combine(output, "SRoot.g.h"));
        Require(header.Contains("::std::vector<::std::map<::std::wstring, ::std::unordered_map<::std::uint32_t, ::std::shared_ptr<::GameData::InGameData::SLeaf>>>>", StringComparison.Ordinal), "nested type AST rendering");
        Require(header.Contains("operator==", StringComparison.Ordinal) && header.Contains("SRoot(SRoot&&", StringComparison.Ordinal), "member equality and move construction");
        Require(!header.Contains("#include", StringComparison.Ordinal) && !header.Contains("struct SRoot final", StringComparison.Ordinal), "leaf include-free/non-final contract");
        string cpp = File.ReadAllText(Path.Combine(output, "SRoot.g.cpp"));
        Require(cpp.Contains("reader.GetSchemaVersion() >= 2u", StringComparison.Ordinal), "defaults may apply only to older archives");
        Require(cpp.Contains("Shared = ::std::vector<", StringComparison.Ordinal), "missing defaults must reset reused candidate fields");
        Require(File.ReadAllText(Path.Combine(output, "InGameDataRegistry.g.cpp")).Contains("registry.Register<SLeaf>", StringComparison.Ordinal), "type registry factories");
    }

    private static void TestValidation(string input)
    {
        string Field(string type = "int32", string metadata = "missing: reject, since: 1") =>
            "schema_version: 2\ntypes:\n  - name: SLeaf\n    fields:\n      - {name: Count, type: '" + type + "', " + metadata + "}\n";
        var cases = new List<(string Name, string Source, string Diagnostic)>
        {
            ("unknown root metadata", "schema_version: 1\ntypes: []\nextra: true", "Unknown metadata"),
            ("duplicate YAML key", "schema_version: 1\nschema_version: 2\ntypes: []", "Duplicate YAML key"),
            ("duplicate type", "schema_version: 1\ntypes: [{name: SLeaf, fields: []}, {name: SLeaf, fields: []}]", "Duplicate type"),
            ("case insensitive file collision", "schema_version: 1\ntypes: [{name: SLeaf, fields: []}, {name: Sleaf, fields: []}]", "Duplicate type"),
            ("duplicate field", Field().Replace("      - {", "      - {name: Count, type: int32, missing: reject, since: 1}\n      - {", StringComparison.Ordinal), "Duplicate field"),
            ("unknown field metadata", Field(metadata: "missing: reject, since: 1, ownership: forced"), "Unknown metadata"),
            ("invalid missing", Field(metadata: "missing: ignore, since: 1"), "missing must"),
            ("missing since", Field(metadata: "missing: reject"), "Missing metadata: since"),
            ("zero since", Field(metadata: "missing: reject, since: 0"), "positive schema version"),
            ("future since", Field(metadata: "missing: reject, since: 3"), "since exceeds"),
            ("required default", Field(metadata: "missing: reject, since: 1, default: 0"), "default requires"),
            ("content default", Field(metadata: "missing: default, since: 2, default: 100"), "neutral default"),
            ("wrong default", Field("string", "missing: default, since: 2, default: null"), "neutral default"),
            ("quoted null pointer", Field("shared_ptr<SLeaf>", "missing: default, since: 2, default: 'null'"), "neutral default"),
            ("nonempty vector default", Field("vector<int32>", "missing: default, since: 2, default: [1]"), "neutral default"),
            ("unknown target", Field("shared_ptr<SMissing>"), "Unknown type"),
            ("raw pointer", Field("SLeaf*"), "raw pointers"),
            ("weak pointer", Field("weak_ptr<SLeaf>"), "Unsupported type"),
            ("enum without catalog", Field("enum<SceneType>"), "Enum catalog input root is required"),
            ("enum missing argument", Field("enum"), "arity"),
            ("enum extra argument", Field("enum<SceneType, int32>"), "arity"),
            ("enum nested template", Field("enum<vector<int32>>"), "catalog identifier"),
            ("enum numeric identifier", Field("enum<123>"), "Expected a type identifier"),
            ("enum arbitrary C++", Field("enum<SceneType;Call()>"), "closing >"),
            ("enum value definitions", "schema_version: 1\ntypes: []\nenums: {SceneType: {Town: 1}}", "Unknown metadata: enums"),
            ("extra template argument", Field("vector<int32, uint32>"), "arity"),
            ("missing template argument", Field("map<string>"), "arity"),
            ("bad key", Field("map<float, int32>"), "Map keys"),
            ("object key", Field("unordered_map<SLeaf, int32>"), "Map keys"),
            ("primitive pointer", Field("unique_ptr<int32>"), "Pointer target"),
            ("value self cycle", Field("SLeaf"), "Value cycle"),
            ("container value cycle", Field("vector<SLeaf>"), "Value cycle"),
            ("code injection", Field("int32; system()"), "raw pointers"),
            ("type identifier", "schema_version: 1\ntypes: [{name: class, fields: []}]", "Invalid identifier"),
            ("reserved field", Field().Replace("name: Count", "name: SerializeFields", StringComparison.Ordinal), "Reserved field"),
            ("Windows device type", "schema_version: 1\ntypes: [{name: CON, fields: []}]", "Reserved type name"),
            ("YAML anchor", "schema_version: 1\ntypes: &types []", "anchors"),
            ("YAML alias", "schema_version: 1\ntypes: [*missing]", "aliases"),
            ("YAML tag", "schema_version: 1\ntypes: !!seq []", "tags"),
            ("multiple documents", "schema_version: 1\ntypes: []\n---\ntypes: []", "one YAML document"),
        };
        var failures = new List<string>();
        foreach (var item in cases)
        {
            File.WriteAllText(input, item.Source);
            bool rejected = false;
            try { _ = FInGameSchemaGenerator.Parse(input); }
            catch (InvalidOperationException exception)
            {
                rejected = exception.Message.StartsWith(input + "(", StringComparison.Ordinal) &&
                    exception.Message.Contains("IGD1001", StringComparison.Ordinal) && exception.Message.Contains(item.Diagnostic, StringComparison.Ordinal);
                if (!rejected) failures.Add(item.Name + " wrong diagnostic: " + exception.Message);
            }
            catch (Exception exception) { failures.Add(item.Name + " unlocated exception: " + exception.GetType().Name); }
            if (!rejected) failures.Add(item.Name + " did not reject with located expected diagnostic");
        }
        Require(failures.Count == 0, string.Join("\n", failures));
        foreach (string primitive in new[] { "bool", "int32", "uint32", "int64", "uint64", "float", "double", "string" })
        {
            File.WriteAllText(input, Field(primitive));
            Require(FInGameSchemaGenerator.Parse(input).Types.Count == 1, "primitive positive: " + primitive);
        }
        foreach (var item in new[] { ("bool", "false"), ("double", "0.0"), ("string", "''"), ("vector<int32>", "[]"), ("map<string, uint32>", "{}"), ("shared_ptr<SLeaf>", "null") })
        {
            File.WriteAllText(input, Field(item.Item1, "missing: default, since: 2, default: " + item.Item2));
            Require(FInGameSchemaGenerator.Parse(input).Types.Count == 1, "neutral default positive: " + item.Item1);
        }
        Console.WriteLine($"InGameData located negative validation cases passed: {cases.Count}.");
    }

    private static void TestNamespace(string root, string input)
    {
        File.WriteAllText(input, "schema_version: 3\ntypes: [{name: SLeaf, fields: [{name: m_child, type: 'shared_ptr<SLeaf>', missing: reject, since: 1}]}]");
        string output = Path.Combine(root, "FixtureNamespace");
        Require(Program.Main(["--ingame-schema", input, "--ingame-output-root", output, "--ingame-namespace", "TextRPGTest::InGameData"]) == 0, "fixture namespace CLI");
        string all = File.ReadAllText(Path.Combine(output, "InGameDataAll.h"));
        Require(all.Contains("namespace TextRPGTest::InGameData", StringComparison.Ordinal) && all.Contains("kSchemaVersion = 3u", StringComparison.Ordinal), "namespace and schema version constant");
        Require(File.ReadAllText(Path.Combine(output, "SLeaf.g.h")).Contains("::TextRPGTest::InGameData::SLeaf", StringComparison.Ordinal), "qualified recursive fixture type");
        foreach (string bad in new[] { "", "std", "class::Test", "A::", "::A", "A;bad", "A::__hidden", "A::1Bad", "A B" })
            Require(Program.Main(["--ingame-schema", input, "--ingame-output-root", output, "--ingame-namespace", bad, "--validate-only"]) == 1, "bad namespace accepted: " + bad);
    }

    private static void TestOutputSafety(string root, string input, string output, string[] args)
    {
        const string source = "schema_version: 1\ntypes: [{name: SLeaf, fields: [{name: Count, type: int32, missing: reject, since: 1}]}]";
        File.WriteAllText(input, source);
        Require(Program.Main(args) == 0, "output safety positive generation");
        foreach (string path in Directory.GetFiles(output, "*", SearchOption.AllDirectories)) File.SetLastWriteTimeUtc(path, new DateTime(2001, 1, 1, 0, 0, 0, DateTimeKind.Utc));
        string before = Snapshot(output);
        Require(Program.Main(args) == 0 && Snapshot(output) == before, "unchanged generation changed bytes/mtimes");
        Require(Program.Main([.. args, "--check"]) == 0 && Snapshot(output) == before, "check changed output");
        File.WriteAllText(input, source.Replace("Count", "Added", StringComparison.Ordinal));
        Require(Program.Main([.. args, "--check"]) == 1 && Snapshot(output) == before, "schema drift check must preserve previous outputs");
        Require(Program.Main([.. args, "--validate-only"]) == 0 && Snapshot(output) == before, "validate-only must preserve previous outputs");
        long aggregateTime = File.GetLastWriteTimeUtc(Path.Combine(output, "InGameDataAll.h")).Ticks;
        Require(Program.Main(args) == 0 && File.GetLastWriteTimeUtc(Path.Combine(output, "InGameDataAll.h")).Ticks == aggregateTime,
            "changed schema must retain unchanged aggregate mtime");
        File.WriteAllText(input, source);
        Require(Program.Main(args) == 0, "restore output positive");
        before = Snapshot(output);
        File.WriteAllText(input, source + "\nunknown: true");
        Require(Program.Main(args) == 1 && Snapshot(output) == before, "parse failure must preserve outputs");
        File.WriteAllText(input, source);
        string staticOutput = Path.Combine(root, "StaticOutput");
        FOutputManager.Apply(staticOutput, [new(".gamedata-output", "GameDataGenerator owned output v1\n"), new("Static.g.h", "protected")], false);
        string staticBefore = Snapshot(staticOutput);
        Require(Program.Main(["--ingame-schema", input, "--ingame-output-root", staticOutput]) == 1 && Snapshot(staticOutput) == staticBefore, "schema mode must not replace static generator-owned outputs");
        bool rejected = false;
        try { FOutputManager.Apply(output, [new(".gamedata-output", "GameDataGenerator owned output v1\n")], false); }
        catch (InvalidOperationException) { rejected = true; }
        Require(rejected && Snapshot(output) == before, "static mode must not replace schema-owned outputs");
        string unowned = Path.Combine(root, "Custom");
        Directory.CreateDirectory(unowned);
        File.WriteAllText(Path.Combine(unowned, "User.h"), "protected custom");
        string customBefore = Snapshot(unowned);
        Require(Program.Main(["--ingame-schema", input, "--ingame-output-root", unowned]) == 1 && Snapshot(unowned) == customBefore, "Custom files must be protected");
        string inputBefore = File.ReadAllText(input);
        Require(Program.Main(["--ingame-schema", input, "--ingame-output-root", Path.Combine(Path.GetDirectoryName(input)!, "nested")]) == 1 && File.ReadAllText(input) == inputBefore, "input/output overlap must be rejected");
        string absent = Path.Combine(root, "Absent", "Generated");
        Require(Program.Main(["--ingame-schema", input, "--ingame-output-root", absent, "--validate-only"]) == 0 && !Directory.Exists(Path.GetDirectoryName(absent)), "validate-only must not create directories");
        File.WriteAllText(input, "schema_version: 1\ntypes: []");
        Require(Program.Main(args) == 0 && !File.Exists(Path.Combine(output, "SLeaf.g.cpp")), "removed schema type must be cleaned");
        File.WriteAllText(input, source);
        Require(Program.Main(args) == 0 && File.Exists(Path.Combine(output, "SLeaf.g.cpp")), "added schema type must be emitted");
        string hidden = Path.Combine(output, "bin", "unknown.txt");
        Directory.CreateDirectory(Path.GetDirectoryName(hidden)!);
        File.WriteAllText(hidden, "not an approved generated artifact");
        string unexpectedBefore = Snapshot(output);
        Require(Program.Main([.. args, "--check"]) == 1 && Snapshot(output) == unexpectedBefore, "schema check must not ignore bin/obj contents");
        Require(Program.Main(args) == 0 && !Directory.Exists(Path.GetDirectoryName(hidden)), "stale output cleanup");
        string emptyStale = Path.Combine(output, "StaleEmpty");
        Directory.CreateDirectory(emptyStale);
        Require(Program.Main([.. args, "--check"]) == 1 && Directory.Exists(emptyStale), "check must reject and preserve an unexpected empty directory");
        Require(Program.Main(args) == 0 && !Directory.Exists(emptyStale), "generation must clean stale empty directories");
#if GAMEDATA_TEST_HOOKS
        string rollbackBefore = Snapshot(output);
        File.WriteAllText(input, source.Replace("Count", "Changed", StringComparison.Ordinal));
        int publishHits = 0;
        FOutputManager.BeforePublishForTest = () =>
        {
            ++publishHits;
            Require(!Directory.Exists(output) && Directory.GetDirectories(Path.GetDirectoryName(output)!, ".Generated.backup.*").Length == 1,
                "schema rollback fault must occur after real backup move");
            throw new IOException("injected schema prepublication failure");
        };
        try
        {
            Require(Program.Main(args) == 1 && publishHits == 1 && Snapshot(output) == rollbackBefore, "schema failed publish must restore exact prior bytes/mtimes");
            Require(!Directory.GetDirectories(Path.GetDirectoryName(output)!, ".Generated.staging.*").Any() &&
                !Directory.GetDirectories(Path.GetDirectoryName(output)!, ".Generated.backup.*").Any(), "schema rollback must clean owned staging and backup");
        }
        finally { FOutputManager.BeforePublishForTest = null; }
        Console.WriteLine("InGameData real backup/publish-failure/rollback exact-hit preservation passed.");
#endif
        Console.WriteLine("InGameData output isolation/check/validate/add-remove/raw-byte/mtime preservation passed.");
    }

    private static string Snapshot(string root) => string.Join("\n", Directory.GetFiles(root, "*", SearchOption.AllDirectories).Order(StringComparer.Ordinal).Select(path =>
        Path.GetRelativePath(root, path) + ":" + Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(File.ReadAllBytes(path))) + ":" + File.GetLastWriteTimeUtc(path).Ticks));

    private static void TestGeneratedMemberSafety(string input)
    {
        File.WriteAllText(input, "schema_version: 1\ntypes: [{name: SEmpty, fields: []}, {name: SLeaf, fields: [{name: writer, type: int32, missing: reject, since: 1}, {name: error, type: string, missing: reject, since: 1}, {name: other, type: 'vector<unique_ptr<SEmpty>>', missing: reject, since: 1}]}]");
        var schema = FInGameSchemaGenerator.Parse(input);
        var files = FInGameSchemaGenerator.Generate(schema);
        string cpp = files.Single(file => file.RelativePath == "SLeaf.g.cpp").Content;
        Require(cpp.Contains("_writer.Field(\"writer\", this->writer, _error)", StringComparison.Ordinal) &&
            cpp.Contains("this->other == _other.other", StringComparison.Ordinal), "generated member accesses must not bind local parameters");
        string empty = files.Single(file => file.RelativePath == "SEmpty.g.cpp").Content;
        Require(empty.Contains("static_cast<void>(_writer)", StringComparison.Ordinal) && empty.Contains("static_cast<void>(_reader)", StringComparison.Ordinal) && empty.Contains("static_cast<void>(_other)", StringComparison.Ordinal), "empty schema types must not introduce unused-parameter warnings");
        string header = files.Single(file => file.RelativePath == "SLeaf.g.h").Content;
        Require(header.Contains("SLeaf(const SLeaf& _other) = delete", StringComparison.Ordinal), "recursive unique_ptr combinations must remain move-only");
        Require(files.SequenceEqual(FInGameSchemaGenerator.Generate(schema)), "deterministic generation API");
    }

    private static void TestBounds(string input)
    {
        void Reject(string source, string expected)
        {
            File.WriteAllText(input, source);
            bool rejected = false;
            try { _ = FInGameSchemaGenerator.Parse(input); }
            catch (InvalidOperationException exception) { rejected = exception.Message.StartsWith(input + "(", StringComparison.Ordinal) && exception.Message.Contains(expected, StringComparison.Ordinal); }
            Require(rejected, "bounded parser did not reject: " + expected);
        }
        Reject(new string(' ', 1024 * 1024 + 1), "1 MiB");
        Reject("schema_version: 1\ntypes: " + new string('[', 70) + new string(']', 70), "node/depth limit");
        Reject("schema_version: 1\ntypes: []\nunknown: " + new string('x', 4097), "Scalar length limit");
        Reject("schema_version: 1\ntypes: [" + string.Join(",", Enumerable.Range(0, 257).Select(index => $"{{name: SType{index}, fields: []}}")) + "]", "Type count limit");
        Reject("schema_version: 1\ntypes: [{name: SType, fields: [" + string.Join(",", Enumerable.Range(0, 257).Select(index => $"{{name: Field{index}, type: int32, missing: reject, since: 1}}")) + "]}]", "Field count limit");
        Reject("schema_version: 1\ntypes: [{name: SType, fields: [{name: Field, type: '" + string.Concat(Enumerable.Repeat("vector<", 40)) + "int32" + new string('>', 40) + "', missing: reject, since: 1}]}]", "Type nesting limit");
        Reject("schema_version: 1\ntypes: [{name: SA, fields: [{name: B, type: SB, missing: reject, since: 1}]}, {name: SB, fields: [{name: A, type: 'vector<SA>', missing: reject, since: 1}]}]", "Value cycle");
        File.WriteAllText(input, "schema_version: 1\ntypes: []\nextra: true");
        try { _ = FInGameSchemaGenerator.Parse(input); throw new InvalidOperationException("negative control accepted"); }
        catch (InvalidOperationException exception) { Require(exception.Message.StartsWith(input + "(3,8): IGD1001:", StringComparison.Ordinal), "exact YAML mark diagnostic"); }
        File.WriteAllBytes(input, [0xFF]);
        bool badUtf8 = false;
        try { _ = FInGameSchemaGenerator.Parse(input); }
        catch (InvalidOperationException exception) { badUtf8 = exception.Message.Contains("IGD1001", StringComparison.Ordinal) && exception.Message.Contains("UTF-8", StringComparison.Ordinal); }
        catch (System.Text.DecoderFallbackException) { }
        Require(badUtf8, "invalid UTF-8 must have a located schema diagnostic");
        Console.WriteLine("InGameData bounded YAML/type parser and exact location diagnostics passed.");
    }

    private static void Require(bool value, string message)
    {
        if (!value) throw new InvalidOperationException("InGameData self-test: " + message);
    }
}
