internal static class FNumericKeyValidation
{
    private static readonly IReadOnlyDictionary<string, string> PrimaryKeys = new Dictionary<string, string>(StringComparer.Ordinal)
    {
        ["CharacterDefinition"] = "CharacterId",
        ["LevelDefinition"] = "Level",
        ["NewGameProfile"] = "ProfileId",
        ["ItemDefinition"] = "ItemId",
        ["AchievementDefinition"] = "AchievementId",
        ["Localization"] = "TextKey",
    };

    private static readonly (string Table, string Field, string Target, string Key)[] References =
    [
        ("CharacterDefinition", "NameTextKey", "Localization", "TextKey"),
        ("CharacterDefinition", "ArtId", "TextArt", "ArtId"),
        ("NewGameProfile", "CharacterId", "CharacterDefinition", "CharacterId"),
        ("NewGameProfile", "InitialLevel", "LevelDefinition", "Level"),
        ("ItemDefinition", "NameTextKey", "Localization", "TextKey"),
        ("AchievementDefinition", "NameTextKey", "Localization", "TextKey"),
    ];

    public static bool IsKey(string table, FGameDataField field) => field.IsPrimaryKey || field.Reference != null ||
        (PrimaryKeys.TryGetValue(table, out string? key) && field.Name == key) ||
        References.Any(reference => reference.Table == table && reference.Field == field.Name) ||
        (table == "SceneOption" && field.Name is "OptionTextKey" or "TargetSceneId");

    public static void Validate(IReadOnlyList<FGameDataTable> tables, FDiagnosticBag diagnostics)
    {
        foreach (FGameDataTable table in tables)
        {
            if (PrimaryKeys.TryGetValue(table.Name, out string? name))
                RequireField(table, name, primary: true, diagnostics);
            foreach (var reference in References.Where(reference => reference.Table == table.Name))
            {
                FGameDataField? field = RequireField(table, reference.Field, primary: false, diagnostics);
                if (field == null) continue;
                string targetName = reference.Target + "." + reference.Key;
                if (field.Reference != targetName)
                    diagnostics.Add("GD2120", $"{table.Name}.{field.Name} must reference exactly '{targetName}'.",
                        table.Locate(12, field.ColumnNumber));
                FGameDataTable? target = tables.FirstOrDefault(candidate => candidate.Name == reference.Target);
                var keys = target?.Rows.Select(row => GetKey(row, reference.Key)).Where(key => key != 0).ToHashSet() ?? [];
                foreach (FGameDataRow row in table.Rows)
                {
                    uint value = GetKey(row, field.Name);
                    if (value == 0 || !keys.Contains(value))
                        diagnostics.Add("GD2122", $"Value '{value}' must exist in '{targetName}', independently of #Reference metadata.",
                            table.Locate(row.SourceRowNumber, field.ColumnNumber));
                }
            }
        }
    }

    private static FGameDataField? RequireField(FGameDataTable table, string name, bool primary, FDiagnosticBag diagnostics)
    {
        FGameDataField? field = table.Fields.FirstOrDefault(candidate => candidate.Name == name);
        if (field == null)
            diagnostics.Add("GD2101", $"{table.Name} table requires field '{name}'.", table.Locate(13, 2));
        else if (field.Type?.Kind != EGameDataScalarKind.UInt32 || !field.Required ||
            field.IsPrimaryKey != primary || !field.IsIncludedOnClient || (primary && field.Scope != EGameDataScope.Shared))
            diagnostics.Add("GD2101", $"{table.Name}.{name} must be required Client-visible uint32{(primary ? " Shared primary key" : " non-key reference")}.",
                field.Location);
        return field;
    }

    private static uint GetKey(FGameDataRow row, string name) =>
        row.Values.TryGetValue(name, out object? raw) && raw is uint key ? key : 0;
}
