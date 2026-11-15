using System.Globalization;

internal static class FSceneDataValidation
{
    private static readonly IReadOnlyDictionary<string, (EGameDataScalarKind Kind, string? EnumName, bool Primary)> SceneFields =
        new Dictionary<string, (EGameDataScalarKind, string?, bool)>(StringComparer.Ordinal)
        {
            ["SceneId"] = (EGameDataScalarKind.String, null, true),
            ["SceneType"] = (EGameDataScalarKind.Enum, "SceneType", false),
            ["IsStartupScene"] = (EGameDataScalarKind.Bool, null, false),
        };

    private static readonly IReadOnlyDictionary<string, (EGameDataScalarKind Kind, string? EnumName, bool Primary)> SceneOptionFields =
        new Dictionary<string, (EGameDataScalarKind, string?, bool)>(StringComparer.Ordinal)
        {
            ["OptionId"] = (EGameDataScalarKind.UInt32, null, true),
            ["SceneType"] = (EGameDataScalarKind.Enum, "SceneType", false),
            ["OptionOrder"] = (EGameDataScalarKind.UInt32, null, false),
            ["OptionTextKey"] = (EGameDataScalarKind.String, null, false),
            ["ActionType"] = (EGameDataScalarKind.Enum, "SceneActionType", false),
            ["TargetSceneId"] = (EGameDataScalarKind.String, null, false),
            ["IsEnabled"] = (EGameDataScalarKind.Bool, null, false),
        };

    private static readonly IReadOnlyDictionary<string, (EGameDataScalarKind Kind, string? EnumName, bool Primary)> SceneTypeConfigFields =
        new Dictionary<string, (EGameDataScalarKind, string?, bool)>(StringComparer.Ordinal)
        {
            ["SceneType"] = (EGameDataScalarKind.Enum, "SceneType", true),
            ["MaxVisibleOptions"] = (EGameDataScalarKind.UInt32, null, false),
            ["OptionOverflowPolicy"] = (EGameDataScalarKind.Enum, "OptionOverflowPolicy", false),
        };

    public static bool IsNullableReference(string tableName, FGameDataField field) =>
        field.Type?.Kind == EGameDataScalarKind.String &&
        tableName == "SceneOption" && field.Name == "TargetSceneId";

    private static readonly IReadOnlyDictionary<string, (EGameDataScalarKind Kind, string? EnumName, bool Primary)> TextArtFields =
        new Dictionary<string, (EGameDataScalarKind, string?, bool)>(StringComparer.Ordinal)
        {
            ["ArtId"] = (EGameDataScalarKind.String, null, true),
            ["AssetPath"] = (EGameDataScalarKind.String, null, false),
            ["Width"] = (EGameDataScalarKind.UInt32, null, false),
            ["Height"] = (EGameDataScalarKind.UInt32, null, false),
        };

    private static readonly IReadOnlyDictionary<string, (EGameDataScalarKind Kind, string? EnumName, bool Primary)> LocalizationFields =
        new Dictionary<string, (EGameDataScalarKind, string?, bool)>(StringComparer.Ordinal)
        {
            ["TextKey"] = (EGameDataScalarKind.String, null, true),
            ["KoKR"] = (EGameDataScalarKind.String, null, false),
            ["EnUS"] = (EGameDataScalarKind.String, null, false),
        };

    public static void Validate(IReadOnlyList<FGameDataTable> tables, IReadOnlyDictionary<string, FGameDataEnumDefinition> enums, FDiagnosticBag diagnostics)
    {
        FGameDataTable? artTable = FindTable(tables, "TextArt");
        bool artSchemaValid = artTable != null && ValidateFields(artTable, TextArtFields, diagnostics);
        ValidateSceneContracts(tables, enums, diagnostics);
        if (artSchemaValid) FTextArtValidation.Validate(artTable!, diagnostics);
        ValidateAssetPaths(tables, diagnostics);
    }

    private static void ValidateSceneContracts(IReadOnlyList<FGameDataTable> tables, IReadOnlyDictionary<string, FGameDataEnumDefinition> enums, FDiagnosticBag diagnostics)
    {
        FGameDataTable? sceneTable = FindTable(tables, "Scene");
        if (sceneTable == null)
        {
            FGameDataTable? partial = FindTable(tables, "SceneOption") ?? FindTable(tables, "SceneTypeConfig");
            if (partial != null) diagnostics.Add("GD2101", "SceneOption/SceneTypeConfig require a Scene table.", partial.Locate(1, 2));
            return;
        }

        foreach (FGameDataField field in sceneTable.Fields.Where(field => !SceneFields.ContainsKey(field.Name)))
        {
            if (field.Name is "PlayerArtId" or "ContentArtId" or "TitleTextKey" or "RegionTextKey")
                diagnostics.Add("GD2132", $"Scene.{field.Name} is forbidden: current presentation belongs to runtime UI component members, not static Scene data.", sceneTable.Locate(13, field.ColumnNumber));
            else
                diagnostics.Add("GD2133", $"Scene.{field.Name} is unexpected: static Scene data permits only SceneId, SceneType and IsStartupScene, regardless of field scope.", sceneTable.Locate(13, field.ColumnNumber));
        }

        FGameDataTable? optionsTable = FindTable(tables, "SceneOption");
        FGameDataTable? configTable = FindTable(tables, "SceneTypeConfig");
        FGameDataTable? localizationTable = FindTable(tables, "Localization");
        if (optionsTable == null || configTable == null || localizationTable == null)
        {
            diagnostics.Add("GD2101", "Scene data requires SceneOption, SceneTypeConfig and Localization tables.", sceneTable.Locate(1, 2));
            return;
        }

        bool valid = ValidateFields(sceneTable, SceneFields, diagnostics) &
            ValidateFields(optionsTable, SceneOptionFields, diagnostics) &
            ValidateFields(configTable, SceneTypeConfigFields, diagnostics) &
            ValidateFields(localizationTable, LocalizationFields, diagnostics);
        if (!valid)
        {
            return;
        }

        RequireReference(optionsTable, "TargetSceneId", sceneTable, "SceneId", diagnostics);
        RequireReference(optionsTable, "OptionTextKey", localizationTable, "TextKey", diagnostics);

        var sceneIds = sceneTable.Rows.Select(row => GetString(row, "SceneId")).ToHashSet(StringComparer.Ordinal);
        foreach (FGameDataRow row in optionsTable.Rows)
        {
            if (row.Values.TryGetValue("IsEnabled", out object? enabled) && enabled is true && GetString(row, "ActionType") == "StartGame" &&
                (string.IsNullOrWhiteSpace(GetString(row, "TargetSceneId")) || !sceneIds.Contains(GetString(row, "TargetSceneId"))))
                diagnostics.Add("GD2121", "Enabled StartGame requires a nonempty existing Scene.SceneId target, independently of #Reference.",
                    optionsTable.Locate(row.SourceRowNumber, optionsTable.Fields.First(field => field.Name == "TargetSceneId").ColumnNumber));
        }

        FGameDataField startupField = sceneTable.Fields.Single(field => field.Name == "IsStartupScene");
        FGameDataRow[] startupScenes = sceneTable.Rows
            .Where(row => row.Values.TryGetValue(startupField.Name, out object? value) && value is bool isStartup && isStartup)
            .ToArray();
        int startupSceneCount = startupScenes.Length;
        if (startupSceneCount != 1)
        {
            SSourceLocation location = startupSceneCount > 1
                ? sceneTable.Locate(startupScenes[1].SourceRowNumber, startupField.ColumnNumber)
                : sceneTable.Locate(14, startupField.ColumnNumber);
            diagnostics.Add("GD2108", $"Scene table must define exactly one startup scene, but found {startupSceneCount}.", location);
        }

        FGameDataField optionType = optionsTable.Fields.Single(field => field.Name == "SceneType");
        FGameDataField optionOrder = optionsTable.Fields.Single(field => field.Name == "OptionOrder");
        FGameDataField configType = configTable.Fields.Single(field => field.Name == "SceneType");
        FGameDataField maxVisible = configTable.Fields.Single(field => field.Name == "MaxVisibleOptions");
        FGameDataField overflowPolicy = configTable.Fields.Single(field => field.Name == "OptionOverflowPolicy");

        var configurations = new Dictionary<string, FGameDataRow>(StringComparer.Ordinal);
        foreach (FGameDataRow row in configTable.Rows)
        {
            string sceneType = GetString(row, configType.Name);
            if (!TryGetUInt32(row, maxVisible.Name, out uint maximum) || maximum != 4)
            {
                diagnostics.Add("GD2102", "MaxVisibleOptions must be exactly 4.", configTable.Locate(row.SourceRowNumber, maxVisible.ColumnNumber));
                continue;
            }

            string policy = GetString(row, overflowPolicy.Name);
            if (policy != "NoScroll")
                diagnostics.Add("GD2109", "OptionOverflowPolicy must be NoScroll; four slots never paginate.", configTable.Locate(row.SourceRowNumber, overflowPolicy.ColumnNumber));
            configurations.TryAdd(sceneType, row);
        }

        var usedTypes = sceneTable.Rows.Select(row => GetString(row, "SceneType")).ToHashSet(StringComparer.Ordinal);
        if (enums.TryGetValue("SceneType", out FGameDataEnumDefinition? sceneTypeEnum))
            usedTypes.UnionWith(sceneTypeEnum.Values.Select(value => value.Name));
        foreach (FGameDataRow row in optionsTable.Rows)
        {
            string sceneType = GetString(row, optionType.Name);
            if (string.IsNullOrWhiteSpace(sceneType))
            {
                continue;
            }
            usedTypes.Add(sceneType);

            if (!TryGetUInt32(row, optionOrder.Name, out uint order) || order == 0)
            {
                diagnostics.Add("GD2103", "OptionOrder must be a positive integer.", optionsTable.Locate(row.SourceRowNumber, optionOrder.ColumnNumber));
            }
        }

        foreach (string sceneType in usedTypes)
        {
            if (!configurations.ContainsKey(sceneType))
            {
                diagnostics.Add("GD2104", $"SceneTypeConfig has no row for SceneType '{sceneType}'.", optionsTable.Locate(14, optionType.ColumnNumber));
            }
        }

        foreach ((string sceneType, FGameDataRow configRow) in configurations)
        {
            FGameDataRow[] sceneOptions = optionsTable.Rows
                .Where(row => string.Equals(GetString(row, optionType.Name), sceneType, StringComparison.Ordinal))
                .OrderBy(row => TryGetUInt32(row, optionOrder.Name, out uint order) ? order : uint.MaxValue)
                .ToArray();

            if (sceneOptions.Length != 4)
            {
                diagnostics.Add(
                    "GD2105",
                    $"SceneType '{sceneType}' must have exactly four options including disabled rows, but found {sceneOptions.Length}.",
                    configTable.Locate(configRow.SourceRowNumber, overflowPolicy.ColumnNumber));
            }

            var orders = new HashSet<uint>();
            for (int index = 0; index < sceneOptions.Length; index++)
            {
                FGameDataRow row = sceneOptions[index];
                if (!TryGetUInt32(row, optionOrder.Name, out uint order))
                {
                    continue;
                }
                if (!orders.Add(order))
                {
                    diagnostics.Add("GD2106", $"SceneType '{sceneType}' has duplicate OptionOrder {order}.", optionsTable.Locate(row.SourceRowNumber, optionOrder.ColumnNumber));
                }
                else if (order != (uint)index + 1)
                {
                    diagnostics.Add("GD2107", $"SceneType '{sceneType}' OptionOrder values must be contiguous and start at 1.", optionsTable.Locate(row.SourceRowNumber, optionOrder.ColumnNumber));
                }
            }
        }
    }

    private static bool ValidateFields(
        FGameDataTable table,
        IReadOnlyDictionary<string, (EGameDataScalarKind Kind, string? EnumName, bool Primary)> expected,
        FDiagnosticBag diagnostics)
    {
        bool valid = table.IsIncludedOnClient;
        if (!valid) diagnostics.Add("GD2101", $"{table.Name} table must be included on Client.", table.Locate(2, 2));
        Dictionary<string, FGameDataField> fields = table.Fields
            .GroupBy(field => field.Name, StringComparer.Ordinal)
            .ToDictionary(group => group.Key, group => group.First(), StringComparer.Ordinal);

        foreach ((string name, (EGameDataScalarKind kind, string? enumName, bool primary)) in expected)
        {
            if (!fields.TryGetValue(name, out FGameDataField? field))
            {
                diagnostics.Add("GD2101", $"{table.Name} table requires field '{name}'.", table.Locate(13, 2));
                valid = false;
                continue;
            }

            if (!field.IsIncludedOnClient || (table.Name == "TextArt" && field.Scope != (primary ? EGameDataScope.Shared : EGameDataScope.Client)) || field.Type?.Kind != kind ||
                (kind == EGameDataScalarKind.Enum && !string.Equals(field.Type.EnumName, enumName, StringComparison.Ordinal)) ||
                field.IsPrimaryKey != primary || field.Required == IsNullableReference(table.Name, field) ||
                table.Fields.Count(candidate => candidate.Name == name) != 1)
            {
                diagnostics.Add("GD2101", $"{table.Name}.{name} has an incompatible Client scope, type, required or primary-key setting.", field.Location);
                valid = false;
            }
        }

        return valid;
    }

    private static void ValidateAssetPaths(IReadOnlyList<FGameDataTable> tables, FDiagnosticBag diagnostics)
    {
        // TextArt는 자체 검증에서 경로 확인 후 실제 파일까지 읽는다.
        foreach (FGameDataTable table in tables.Where(table => table.Name != "TextArt"))
        {
            foreach (FGameDataField field in table.Fields.Where(field => string.Equals(field.Name, "AssetPath", StringComparison.Ordinal)))
            {
                if (field.Type?.Kind != EGameDataScalarKind.String)
                {
                    diagnostics.Add("GD2110", "AssetPath must use string type.", field.Location);
                    continue;
                }

                foreach (FGameDataRow row in table.Rows)
                    _ = FAssetPathValidation.Resolve(table, field, row, diagnostics);
            }
        }
    }
    private static void RequireReference(FGameDataTable table, string name, FGameDataTable target, string key, FDiagnosticBag diagnostics)
    {
        string reference = target.Name + "." + key;
        FGameDataField field = table.Fields.First(field => field.Name == name);
        if (!string.Equals(field.Reference, reference, StringComparison.Ordinal))
            diagnostics.Add("GD2120", $"{table.Name}.{name} must reference exactly '{reference}'.", table.Locate(12, field.ColumnNumber));
        var keys = target.Rows.Select(row => GetString(row, key)).ToHashSet(StringComparer.Ordinal);
        foreach (FGameDataRow row in table.Rows)
        {
            string value = GetString(row, name);
            if ((!string.IsNullOrWhiteSpace(value) && !keys.Contains(value)) || (field.Required && string.IsNullOrWhiteSpace(value)))
                diagnostics.Add("GD2122", $"Value '{value}' must exist in '{reference}', independently of #Reference metadata.", table.Locate(row.SourceRowNumber, field.ColumnNumber));
        }
    }

    private static FGameDataTable? FindTable(IReadOnlyList<FGameDataTable> tables, string name) =>
        tables.FirstOrDefault(table => string.Equals(table.Name, name, StringComparison.Ordinal));

    private static string GetString(FGameDataRow row, string fieldName) =>
        row.Values.TryGetValue(fieldName, out object? value) ? value as string ?? string.Empty : string.Empty;

    private static bool TryGetUInt32(FGameDataRow row, string fieldName, out uint value)
    {
        value = 0;
        if (!row.Values.TryGetValue(fieldName, out object? raw) || raw == null)
        {
            return false;
        }

        try
        {
            value = Convert.ToUInt32(raw, CultureInfo.InvariantCulture);
            return true;
        }
        catch (Exception exception) when (exception is FormatException or InvalidCastException or OverflowException)
        {
            return false;
        }
    }
}
