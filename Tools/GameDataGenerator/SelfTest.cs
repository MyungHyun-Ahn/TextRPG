using ClosedXML.Excel;

internal static class FGameDataGeneratorSelfTest
{
    public static void Run()
    {
        string testRoot = Path.Combine(Environment.GetEnvironmentVariable("TMPDIR") ?? Path.GetTempPath(), $"GameDataGeneratorSelfTest-{Guid.NewGuid():N}");
        string validInputRoot = Path.Combine(testRoot, "Valid", "Input");
        string validOutputRoot = Path.Combine(testRoot, "Valid", "Output");
        string serverInputRoot = Path.Combine(testRoot, "Server", "Input");
        Directory.CreateDirectory(validInputRoot);
        Directory.CreateDirectory(serverInputRoot);

        try
        {
            CreateEnumWorkbook(Path.Combine(validInputRoot, "Enums.xlsx"));
            FInGameSchemaSelfTest.Run(testRoot, validInputRoot);
            CreateLocalizationWorkbook(Path.Combine(validInputRoot, "Localization.xlsx"), "Client");
            var diagnostics = new FDiagnosticBag();
            IReadOnlyList<FGameDataTable> tables = FWorkbookParser.ParseDirectory(validInputRoot, diagnostics);
            IReadOnlyDictionary<string, FGameDataEnumDefinition> enums = FGameDataValidator.Validate(tables, diagnostics);
            Assert(!diagnostics.HasErrors, "valid client workbook produced diagnostics:\n" + string.Join('\n', diagnostics.GetSorted()));

            IReadOnlyList<SGeneratedFile> files = FGameDataGenerators.Generate(tables, enums);
            AssertCppHeaderContract(files);
            AssertCppUmbrellaContract(files,
                ["Common/GameDataEnums.g.h", "Localization/LocalizationData.g.h"]);
            AssertCppUmbrellaVariants(testRoot, tables, enums);
            Assert(files.Any(file => file.RelativePath == "Cpp/Localization/LocalizationData.g.h"),
                "client C++ table header was not generated");
            Assert(files.Any(file => file.RelativePath == "Data/Client/Localization.yaml"),
                "client YAML table data was not generated");
            Assert(files.Any(file => file.RelativePath == "Cpp/Common/GameDataEnums.g.h"),
                "client enum C++ header was not generated");
            Assert(!files.Any(file => file.RelativePath.StartsWith("Schema/", StringComparison.Ordinal) ||
                                      file.RelativePath.EndsWith(".schema.yaml", StringComparison.Ordinal)),
                "schema YAML must not be generated for tables or enums");

            FOutputManager.Apply(validOutputRoot, files, check: false);
            FOutputManager.Apply(validOutputRoot, files, check: true);

            string headerPath = Path.Combine(validOutputRoot, "Cpp", "Localization", "LocalizationData.g.h");
            string header = File.ReadAllText(headerPath);
            Assert(header.Contains("std::wstring", StringComparison.Ordinal), "generated strings are not std::wstring");
            Assert(header.Contains("struct SLocalizationData final", StringComparison.Ordinal) &&
                   header.Contains("using KeyType = ::std::wstring;", StringComparison.Ordinal),
                "client table type/key contract is missing");
            Assert(!header.Contains("한글", StringComparison.Ordinal) && !header.Contains("TITLE_MAIN", StringComparison.Ordinal),
                "actual localization data leaked into the type header");
            string yaml = files.Single(file => file.RelativePath == "Data/Client/Localization.yaml").Content;
            Assert(yaml == "# TextRPG.GameData 1\n" +
                "\"LocalizationTITLE_MAIN\":\n  \"TextId\": \"TITLE_MAIN\"\n  \"Korean\": \"한글\"\n  \"English\": \"Title\"\n" +
                "\"LocalizationUI_START\":\n  \"TextId\": \"UI_START\"\n  \"Korean\": \"새 게임\"\n  \"English\": \"New Game\"\n",
                "Client YAML must contain the exact version line followed by unchanged mapping/scalar data");
            foreach (SGeneratedFile clientYaml in files.Where(file => file.RelativePath.StartsWith("Data/Client/", StringComparison.Ordinal)))
            {
                Assert(clientYaml.Content.StartsWith("# TextRPG.GameData 1\n", StringComparison.Ordinal),
                    "Client YAML version marker missing: " + clientYaml.RelativePath);
            }
            Assert(yaml.Contains("\"Korean\": \"한글\"", StringComparison.Ordinal),
                "Korean text was not preserved in Client YAML");

            string[] generatedPaths = Directory.GetFiles(validOutputRoot, "*", SearchOption.AllDirectories);
            Assert(!generatedPaths.Any(path => path.EndsWith(".json", StringComparison.OrdinalIgnoreCase) ||
                                               path.EndsWith(".cs", StringComparison.OrdinalIgnoreCase)),
                "client-only generator emitted JSON or C# output");

            FSceneContractSelfTest.Run(testRoot);
            FOutputMigrationSelfTest.Run(validInputRoot, testRoot);

            CreateEnumWorkbook(Path.Combine(serverInputRoot, "Enums.xlsx"));
            CreateLocalizationWorkbook(Path.Combine(serverInputRoot, "ServerTable.xlsx"), "Server");
            var serverDiagnostics = new FDiagnosticBag();
            IReadOnlyList<FGameDataTable> serverTables = FWorkbookParser.ParseDirectory(serverInputRoot, serverDiagnostics);
            _ = FGameDataValidator.Validate(serverTables, serverDiagnostics);
            Assert(serverDiagnostics.GetSorted().Any(diagnostic => diagnostic.Code == "GD1901"),
                "Server-target table was not rejected by the client-only validation");
        }
        finally
        {
            if (Directory.Exists(testRoot))
            {
                Directory.Delete(testRoot, recursive: true);
            }
        }
    }

    internal static void AssertCppHeaderContract(IReadOnlyList<SGeneratedFile> files)
    {
        SGeneratedFile[] headers = files.Where(file => file.RelativePath.EndsWith(".g.h", StringComparison.Ordinal)).ToArray();
        Assert(headers.Length != 0, "header contract fixture emitted no C++ headers");
        foreach (SGeneratedFile file in headers)
        {
            string header = file.Content;
            Assert(!System.Text.RegularExpressions.Regex.IsMatch(header, @"(?m)^\s*#\s*include\b"),
                $"generated header must be include-free: {file.RelativePath}");
            Assert(header.Contains("// 소비 번역 단위:", StringComparison.Ordinal) &&
                   header.Contains("<cstdint>", StringComparison.Ordinal),
                $"generated header must document standard prerequisites: {file.RelativePath}");
            if (file.RelativePath == "Cpp/Common/GameDataEnums.g.h")
            {
                Assert(header.Contains("데이터 헤더보다 먼저 명시적으로 포함", StringComparison.Ordinal),
                    "enum header must document explicit ordering before enum-using data headers");
            }
            else
            {
                Assert(!header.Contains("::std::array<", StringComparison.Ordinal) &&
                       !System.Text.RegularExpressions.Regex.IsMatch(header, @"\bFind\w*\s*\(") &&
                       !header.Contains("L\"", StringComparison.Ordinal),
                    $"type-only header contains row storage, lookup, or value literals: {file.RelativePath}");
                Assert(header.Contains("<array>", StringComparison.Ordinal) && header.Contains("<string>", StringComparison.Ordinal),
                    $"table header must document array and string prerequisites: {file.RelativePath}");
                if (header.Contains("::GameData::Common::E", StringComparison.Ordinal))
                {
                    Assert(header.Contains("// Common/GameDataEnums.g.h를 이 헤더보다 먼저 명시적으로 포함", StringComparison.Ordinal),
                        $"enum-using table must document explicit enum-header ordering: {file.RelativePath}");
                }
            }
            Assert(!System.Text.RegularExpressions.Regex.IsMatch(header, @"(?<!:)\bstd::"),
                $"generated standard types must remain globally qualified: {file.RelativePath}");
        }
    }

    private static void AssertCppUmbrellaContract(IReadOnlyList<SGeneratedFile> files, IReadOnlyList<string> expectedHeaders)
    {
        SGeneratedFile[] umbrellas = files.Where(file => file.RelativePath == "Cpp/GameDataAll.h").ToArray();
        Assert(umbrellas.Length == 1, "exactly one generated GameDataAll.h umbrella is required");
        string expected = "#pragma once\n\n// Generated by GameDataGenerator. Do not edit.\n" +
            "#include <array>\n#include <cstdint>\n#include <string>\n" +
            string.Concat(expectedHeaders.Select(path => $"#include \"{path}\"\n"));
        Assert(umbrellas[0].Content == expected,
            "umbrella must contain only stable standard prerequisites, enum first, and ordinal emitted client headers");
        Assert(!files.Any(file => file.RelativePath == "Cpp/GameData.h"), "a secondary GameData.h alias is forbidden");
    }

    private static void AssertCppUmbrellaVariants(string testRoot, IReadOnlyList<FGameDataTable> validTables,
        IReadOnlyDictionary<string, FGameDataEnumDefinition> enums)
    {
        var noEnums = new Dictionary<string, FGameDataEnumDefinition>(StringComparer.Ordinal);
        FGameDataTable CreateTable(string name, EGameDataTarget target = EGameDataTarget.Client) => new()
        {
            Name = name, Target = target, SourceRelativePath = "Umbrella.xlsx", SourcePath = "Umbrella.xlsx", SheetName = name,
            Fields = [
                new FGameDataField
                {
                    Name = "Id", ColumnNumber = 2, Scope = EGameDataScope.Shared, Required = true, IsPrimaryKey = true,
                    Type = new FGameDataType { SourceName = "uint32", Kind = EGameDataScalarKind.UInt32 },
                    AllowedValues = [], Location = new SSourceLocation("Umbrella.xlsx", name, "B13"),
                },
                new FGameDataField
                {
                    Name = "Ignored", ColumnNumber = 3, Scope = EGameDataScope.Ignore, Required = false, IsPrimaryKey = false,
                    AllowedValues = [], Location = new SSourceLocation("Umbrella.xlsx", name, "C13"),
                },
            ],
            Rows = [],
        };
        // 발행기의 빈 입력 경계이며 CLI의 필수 Enums/비어 있지 않은 행 검증을 완화하지 않는다.
        AssertCppUmbrellaContract(FGameDataGenerators.Generate([], noEnums), []);
        FGameDataTable alpha = CreateTable("Alpha");
        FGameDataTable zulu = CreateTable("Zulu");
        FGameDataTable lower = CreateTable("apple");
        FGameDataTable server = CreateTable("ServerOnly", EGameDataTarget.Server);
        IReadOnlyList<SGeneratedFile> noEnumFiles = FGameDataGenerators.Generate([zulu, server, lower, alpha], noEnums);
        AssertCppUmbrellaContract(noEnumFiles, ["Alpha/AlphaData.g.h", "Zulu/ZuluData.g.h", "apple/appleData.g.h"]);
        AssertCppHeaderContract(noEnumFiles);
        Assert(!noEnumFiles.Any(file => file.RelativePath.Contains("ServerOnly", StringComparison.Ordinal)),
            "non-client tables must not enter the emitted header set");
        Assert(!noEnumFiles.Any(file => file.Content.Contains("Ignored", StringComparison.Ordinal)),
            "ignored fields must not enter type/YAML/umbrella output");
        Assert(noEnumFiles.Single(file => file.RelativePath == "Data/Client/Alpha.yaml").Content == "# TextRPG.GameData 1\n",
            "empty producer table must still emit a type and version-only YAML");
        IReadOnlyList<SGeneratedFile> withEnums = FGameDataGenerators.Generate([zulu, lower, alpha], enums);
        AssertCppUmbrellaContract(withEnums,
            ["Common/GameDataEnums.g.h", "Alpha/AlphaData.g.h", "Zulu/ZuluData.g.h", "apple/appleData.g.h"]);
        Assert(withEnums.SequenceEqual(FGameDataGenerators.Generate([lower, alpha, zulu], enums)),
            "reordered input tables must produce byte-identical ordered files");
        AssertCppUmbrellaContract(FGameDataGenerators.Generate([], enums), ["Common/GameDataEnums.g.h"]);
        var emptyDiagnostics = new FDiagnosticBag();
        _ = FGameDataValidator.Validate([validTables.Single(FGameDataEnumCatalog.IsDefinitionTable), alpha], emptyDiagnostics);
        Assert(emptyDiagnostics.GetSorted().Any(item => item.Code == "GD0308"),
            "umbrella support must not permit empty tables through CLI validation");

        string cppRoot = Path.Combine(testRoot, "Umbrella", "Class");
        string yamlRoot = Path.Combine(testRoot, "Umbrella", "Data");
        FOutputManager.Apply(cppRoot, yamlRoot, withEnums, check: false);
        // 추가/삭제를 게시하기 전 --check는 두 루트의 기존 바이트를 보존하며 정확한 drift를 보고한다.
        void AssertDriftPreserves(IReadOnlyList<SGeneratedFile> next)
        {
            string[] paths = Directory.GetFiles(cppRoot, "*", SearchOption.AllDirectories)
                .Concat(Directory.GetFiles(yamlRoot, "*", SearchOption.AllDirectories)).Order(StringComparer.Ordinal).ToArray();
            byte[][] before = paths.Select(File.ReadAllBytes).ToArray();
            bool rejected = false;
            try { FOutputManager.Apply(cppRoot, yamlRoot, next, check: true); }
            catch (InvalidOperationException exception)
            {
                rejected = exception.Message.Contains("changed: GameDataAll.h", StringComparison.Ordinal);
            }
            Assert(rejected, "check must detect exact umbrella drift before publishing a changed emitted set");
            Assert(paths.SequenceEqual(Directory.GetFiles(cppRoot, "*", SearchOption.AllDirectories)
                    .Concat(Directory.GetFiles(yamlRoot, "*", SearchOption.AllDirectories)).Order(StringComparer.Ordinal)) &&
                paths.Select((path, index) => File.ReadAllBytes(path).SequenceEqual(before[index])).All(equal => equal),
                "failed umbrella check changed existing output files");
        }
        IReadOnlyList<SGeneratedFile> removed = FGameDataGenerators.Generate([alpha], noEnums);
        AssertCppUmbrellaContract(removed, ["Alpha/AlphaData.g.h"]);
        AssertDriftPreserves(removed);
        FOutputManager.Apply(cppRoot, yamlRoot, removed, check: false);
        FOutputManager.Apply(cppRoot, yamlRoot, removed, check: true);
        Assert(!File.Exists(Path.Combine(cppRoot, "Zulu", "ZuluData.g.h")) &&
               !File.Exists(Path.Combine(cppRoot, "Common", "GameDataEnums.g.h")) &&
               !File.Exists(Path.Combine(yamlRoot, "Client", "Zulu.yaml")), "removed table/enum output survived regeneration");
        Assert(File.ReadAllText(Path.Combine(cppRoot, "GameDataAll.h")) ==
            removed.Single(file => file.RelativePath == "Cpp/GameDataAll.h").Content, "published umbrella contains stale includes");
        AssertDriftPreserves(withEnums);
        FOutputManager.Apply(cppRoot, yamlRoot, withEnums, check: false);
        FOutputManager.Apply(cppRoot, yamlRoot, withEnums, check: true);
        Assert(File.Exists(Path.Combine(cppRoot, "Zulu", "ZuluData.g.h")), "added table did not enter regenerated output");
        Console.WriteLine("Generated umbrella empty/no-enum/enum-only/ignored/non-client/ordinal/add-remove/check-preservation cases passed.");
    }

    private static void CreateEnumWorkbook(string path)
    {
        using var workbook = new XLWorkbook();
        IXLWorksheet sheet = workbook.AddWorksheet("Enums");
        WriteMetadataLabels(sheet, "Enums", "Shared");
        string[] fields = ["EnumName", "Target", "UnderlyingType", "Values"];
        for (int index = 0; index < fields.Length; ++index)
        {
            int column = index + 2;
            sheet.Cell(4, column).Value = index == 0 ? "Primary" : string.Empty;
            sheet.Cell(5, column).Value = "Shared";
            sheet.Cell(6, column).Value = "string";
            sheet.Cell(7, column).Value = "true";
            sheet.Cell(13, column).Value = fields[index];
        }
        WriteDataRow(sheet, 14, ["TestKind", "Client", "uint8", "One=1|Two=2"]);
        workbook.SaveAs(path);
    }

    private static void CreateLocalizationWorkbook(string path, string tableTarget)
    {
        using var workbook = new XLWorkbook();
        IXLWorksheet sheet = workbook.AddWorksheet("Localization");
        WriteMetadataLabels(sheet, "Localization", tableTarget);
        string[] fields = ["TextId", "Korean", "English"];
        string[] scopes = ["Shared", "Client", "Client"];
        string[] types = ["string", "string", "string"];
        for (int index = 0; index < fields.Length; ++index)
        {
            int column = index + 2;
            sheet.Cell(4, column).Value = index == 0 ? "Primary" : string.Empty;
            sheet.Cell(5, column).Value = scopes[index];
            sheet.Cell(6, column).Value = types[index];
            sheet.Cell(7, column).Value = "true";
            sheet.Cell(13, column).Value = fields[index];
        }
        WriteDataRow(sheet, 14, ["TITLE_MAIN", "한글", "Title"]);
        WriteDataRow(sheet, 15, ["UI_START", "새 게임", "New Game"]);
        workbook.SaveAs(path);
    }

    private static void WriteMetadataLabels(IXLWorksheet sheet, string tableName, string target)
    {
        sheet.Cell("A1").Value = "#Table";
        sheet.Cell("B1").Value = tableName;
        sheet.Cell("A2").Value = "#Target";
        sheet.Cell("B2").Value = target;
        sheet.Cell("A4").Value = "#Key";
        sheet.Cell("A5").Value = "#Scope";
        sheet.Cell("A6").Value = "#Type";
        sheet.Cell("A7").Value = "#Required";
        sheet.Cell("A8").Value = "#Min";
        sheet.Cell("A9").Value = "#Max";
        sheet.Cell("A10").Value = "#Default";
        sheet.Cell("A11").Value = "#Allowed";
        sheet.Cell("A12").Value = "#Reference";
        sheet.Cell("A13").Value = "#Field";
    }

    private static void WriteDataRow(IXLWorksheet sheet, int row, IReadOnlyList<object> values)
    {
        for (int index = 0; index < values.Count; ++index)
        {
            sheet.Cell(row, index + 2).Value = XLCellValue.FromObject(values[index]);
        }
    }

    private static void Assert(bool condition, string message)
    {
        if (!condition)
        {
            throw new InvalidOperationException($"Self-test assertion failed: {message}");
        }
    }
}
