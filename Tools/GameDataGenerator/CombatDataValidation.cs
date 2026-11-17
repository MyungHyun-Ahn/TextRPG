internal static class FCombatDataValidation
{
    private static readonly IReadOnlyDictionary<string, string[]> Fields = new Dictionary<string, string[]>
    {
        ["CharacterCombat"] = ["CharacterId", "Strength", "Dexterity", "Intelligence", "Luck"],
        ["MonsterDefinition"] = ["MonsterId", "NameTextKey", "ArtId", "MaxHp", "Attack", "Defense",
            "Strength", "Dexterity", "Intelligence", "Luck"],
        ["CombatRule"] = ["RuleId", "MonsterId", "BattleSceneId", "TownSceneId", "Scale", "StrengthDefense", "DexteritySpeed",
            "BaseSpeed", "LuckCritical", "CriticalCap", "CriticalMultiplier", "MinimumDamage", "StrengthCoefficient",
            "DexterityCoefficient", "IntelligenceCoefficient", "LuckCoefficient", "MaxStat", "MaxRounds", "HealOnReturn",
            "HitTextKey", "CriticalTextKey", "VictoryTextKey", "DefeatTextKey", "ReadyTextKey", "RoundTextKey"],
    };

    public static void Validate(IReadOnlyList<FGameDataTable> tables, FDiagnosticBag diagnostics)
    {
        FGameDataTable[] cohort = tables.Where(table => Fields.ContainsKey(table.Name)).ToArray();
        if (cohort.Length == 0) return;
        string[] progressTables = ["CharacterDefinition", "LevelDefinition", "NewGameProfile", "ItemDefinition", "AchievementDefinition"];
        if (cohort.Length != Fields.Count || progressTables.Any(name => !tables.Any(table => table.Name == name)))
        {
            diagnostics.Add("GD2201", "Combat requires the complete character, monster and rule cohort.", cohort[0].Locate(1, 2));
            return;
        }
        foreach (FGameDataTable table in cohort)
        {
            string[] expected = Fields[table.Name];
            if (table.Fields.Count != expected.Length || expected.Any(name => !table.Fields.Any(field =>
                field.Name == name && field.Type?.Kind == EGameDataScalarKind.UInt32 && field.Required)))
            {
                diagnostics.Add("GD2202", "Combat fields must match the required uint32 contract.", table.Locate(13, 2));
                return;
            }
            if (!table.IsIncludedOnClient)
                diagnostics.Add("GD2202", "Combat table must be included on Client.", table.Locate(2, 2));
            foreach (FGameDataField field in table.Fields.Where(field => !field.IsIncludedOnClient))
                diagnostics.Add("GD2202", "Mandatory combat field must be included on Client.", table.Locate(5, field.ColumnNumber));
        }
        // 작성자가 바꾸는 메타데이터와 별개로 Combat.xlsx 9행의 기술적 검증 상한을 적용한다.
        const uint CombatWorkbookMaximum = 1_000_000;
        foreach (FGameDataTable table in cohort)
        foreach (FGameDataRow row in table.Rows)
        foreach (FGameDataField field in table.Fields)
        {
            bool fullWidthKey = field.Name.EndsWith("TextKey", StringComparison.Ordinal) || field.Name == "ArtId" ||
                field.Name is "BattleSceneId" or "TownSceneId" || (table.Name == "CharacterCombat" && field.Name == "CharacterId");
            if (!fullWidthKey && row.Values.TryGetValue(field.Name, out object? value) && value is uint number &&
                number > (field.Name == "HealOnReturn" ? 1U : CombatWorkbookMaximum))
                diagnostics.Add("GD2206", "Combat workbook range exceeded.", table.Locate(row.SourceRowNumber, field.ColumnNumber));
        }
        FGameDataTable rules = cohort.Single(table => table.Name == "CombatRule");
        FGameDataTable monsters = cohort.Single(table => table.Name == "MonsterDefinition");
        foreach (FGameDataRow monster in monsters.Rows)
        {
            FGameDataRow? art = FindReferencedRow(tables, "TextArt", "ArtId", GetNumber(monster, "ArtId"));
            if (art == null || GetNumber(art, "Width") != 36 || GetNumber(art, "Height") != 36)
                diagnostics.Add("GD2208", "MonsterDefinition.ArtId must reference 36x36 TextArt.",
                    monsters.Locate(monster.SourceRowNumber, monsters.Fields.Single(field => field.Name == "ArtId").ColumnNumber));
        }
        var battleScenes = new HashSet<uint>();
        foreach (FGameDataRow row in rules.Rows)
        {
            uint Number(string name) => row.Values.TryGetValue(name, out object? value) && value is uint number ? number : 0;
            foreach ((string fieldName, string sceneType) in new[] { ("BattleSceneId", "BattleField"), ("TownSceneId", "Town") })
            {
                FGameDataRow? scene = FindReferencedRow(tables, "Scene", "SceneId", Number(fieldName));
                if (scene == null || !scene.Values.TryGetValue("SceneType", out object? type) || type is not string name || name != sceneType)
                    diagnostics.Add("GD2207", $"CombatRule.{fieldName} must reference a {sceneType} Scene.",
                        rules.Locate(row.SourceRowNumber, rules.Fields.Single(field => field.Name == fieldName).ColumnNumber));
            }
            uint scale = Number("Scale");
            if (scale == 0 || Number("BaseSpeed") == 0 || Number("MinimumDamage") == 0 || Number("MaxStat") == 0 ||
                Number("MaxRounds") == 0 || Number("CriticalCap") > scale || Number("CriticalMultiplier") < scale ||
                Number("HealOnReturn") > 1 || !battleScenes.Add(Number("BattleSceneId")))
                diagnostics.Add("GD2203", "Combat rule bounds or encounter identity are invalid.", rules.Locate(row.SourceRowNumber, 2));
            FGameDataRow? monster = monsters.Rows.FirstOrDefault(candidate =>
                candidate.Values.TryGetValue("MonsterId", out object? value) && value is uint id && id == Number("MonsterId"));
            if (monster is not null && monster.Values["MaxHp"] is uint hp && Number("MinimumDamage") is uint minimum && minimum != 0 &&
                Number("MaxRounds") < hp / minimum + (hp % minimum != 0 ? 1U : 0U))
                diagnostics.Add("GD2205", "Combat rule MaxRounds cannot guarantee termination.", rules.Locate(row.SourceRowNumber, 2));
            foreach (FGameDataTable stats in cohort.Where(table => table.Name != "CombatRule"))
            {
                foreach (FGameDataRow original in stats.Rows)
                {
                    foreach (string name in new[] { "Strength", "Dexterity", "Intelligence", "Luck" })
                    {
                        if (original.Values.TryGetValue(name, out object? value) && value is uint stat && stat > Number("MaxStat"))
                            diagnostics.Add("GD2204", "Original stat exceeds combat rule limit.", stats.Locate(original.SourceRowNumber,
                                stats.Fields.Single(field => field.Name == name).ColumnNumber));
                    }
                }
            }
        }
    }

    private static uint GetNumber(FGameDataRow row, string field) =>
        row.Values.TryGetValue(field, out object? value) && value is uint number ? number : 0;

    private static FGameDataRow? FindReferencedRow(IReadOnlyList<FGameDataTable> tables, string table, string key, uint value) =>
        tables.FirstOrDefault(candidate => candidate.Name == table)?.Rows.FirstOrDefault(row => GetNumber(row, key) == value);
}
