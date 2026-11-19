using ClosedXML.Excel;

internal static class FSceneContractSelfTest
{
    public static void Run(string testRoot)
    {
        Check(testRoot, "NumericKey_FutureSkillStringRejected", workbook =>
            Add(workbook, "FutureSkill", ["SkillId"], ["string"], [["Fireball"]]), "GD0307");
        Check(testRoot, "NumericKey_TextCellRejected", workbook =>
            Add(workbook, "FutureSkill", ["SkillId"], ["uint32"], [["001"]]), "GD0210");
        Check(testRoot, "NumericKey_ItemDefinitionPositive", _ => { }, null, combat: true);
        Check(testRoot, "NumericKey_RemovedItemNameReference", workbook =>
            Add(workbook, "ItemDefinition", ["ItemId", "NameTextKey", "MaxStack"], ["uint32", "uint32", "uint32"], [[1, 1, 99]]), "GD2120");
        CheckNumericKeys(testRoot);
        CheckCombatContracts(testRoot);
        CheckCombatBounds(testRoot);
        Check(testRoot, "Combat_PartialCohort", workbook =>
            Add(workbook, "CharacterCombat", ["CharacterId", "Strength", "Dexterity", "Intelligence", "Luck"],
                ["uint32", "uint32", "uint32", "uint32", "uint32"], [[1, 5, 5, 3, 5]]), "GD2201");
        Check(testRoot, "StaticSceneThreeFieldsWithoutLegacyAssets", _ => { }, null);
        foreach (string field in new[] { "PlayerArtId", "ContentArtId", "TitleTextKey", "RegionTextKey" })
            Check(testRoot, "Scene_ForbiddenPresentation_" + field, workbook =>
            {
                var sheet = workbook.Worksheet("Scene");
                sheet.Cell("E5").Value = "Client";
                sheet.Cell("E6").Value = "string";
                sheet.Cell("E7").Value = true;
                sheet.Cell("E13").Value = field;
                for (int row = 14; row <= 16; row++) sheet.Cell(row, 5).Value = "Unbound";
            }, "GD2132", expectedCell: "E13");
        foreach (string scope in new[] { "Shared", "Client", "Ignore" })
        foreach (string field in new[] { "CurrentMonsterName", "AssetPath" })
            Check(testRoot, "Scene_Unexpected" + scope + "_" + field, workbook =>
            {
                var sheet = workbook.Worksheet("Scene");
                sheet.Cell("E5").Value = scope;
                sheet.Cell("E6").Value = "string";
                sheet.Cell("E7").Value = true;
                sheet.Cell("E13").Value = field;
                for (int row = 14; row <= 16; row++) sheet.Cell(row, 5).Value = "Unbound";
            }, "GD2133", expectedCell: "E13");
        Check(testRoot, "ValidScene", _ => { }, null);
        Check(testRoot, "GenericExtraFieldAllowed", workbook =>
            Add(workbook, "Independent", ["Id", "CurrentMonsterName"], ["uint32", "string"], [[1, "Unbound"]]), null);
        Check(testRoot, "UI_MissingSceneField", workbook => workbook.Worksheet("Scene").Column(4).Delete(), "GD2101");
        Check(testRoot, "GEN01_ClientFields", workbook => workbook.Worksheet("Scene").Cell("D5").Value = "Ignore", "GD2101");
        Check(testRoot, "GEN02_NoOptionConfig", workbook =>
        {
            workbook.Worksheet("SceneTypeConfig").Row(16).Delete();
            workbook.Worksheet("SceneOption").Rows(22, 25).Delete();
        }, "GD2104");
        Check(testRoot, "GEN04_NestedWorkbook", _ => { }, null, nested: true);
        Check(testRoot, "GEN03_DeletedReference", workbook => workbook.Worksheet("SceneOption").Cell("G12").Value = "", "GD2120");
        Check(testRoot, "GEN03_EmptyStartTarget", workbook =>
        {
            var sheet = workbook.Worksheet("SceneOption");
            sheet.Cell("G7").Value = false;
            sheet.Cell("G12").Value = "";
            sheet.Cell("G14").Value = "";
        }, "GD2121");
        Check(testRoot, "GEN03_InvalidStartTargetWithoutMetadata", workbook =>
        {
            workbook.Worksheet("SceneOption").Cell("G12").Value = "";
            workbook.Worksheet("SceneOption").Cell("G14").Value = 999;
        }, "GD2121");
        Check(testRoot, "SceneWithoutTextArt", workbook => workbook.Worksheet("TextArt").Delete(), null);
        Check(testRoot, "Art_RenamedIndependentIdsAndFiles", workbook =>
        {
            var sheet = workbook.Worksheet("TextArt");
            sheet.Cell("B14").Value = 101;
            sheet.Cell("C14").Value = "Asset/banner.txt";
            sheet.Cell("B15").Value = 102;
            sheet.Cell("C15").Value = "Asset/portrait.txt";
        }, null, mutateAssets: root =>
        {
            File.Move(Path.Combine(root, "Asset", "logo.txt"), Path.Combine(root, "Asset", "banner.txt"));
            File.Move(Path.Combine(root, "Asset", "square.txt"), Path.Combine(root, "Asset", "portrait.txt"));
        });
        Check(testRoot, "Art_NewIndependentIdAndFile", workbook =>
        {
            var sheet = workbook.Worksheet("TextArt");
            sheet.Cell("B16").Value = 103;
            sheet.Cell("C16").Value = "Asset/encounter.txt";
            sheet.Cell("D16").Value = 36;
            sheet.Cell("E16").Value = 36;
        }, null, mutateAssets: root => File.Copy(Path.Combine(root, "Asset", "square.txt"), Path.Combine(root, "Asset", "encounter.txt")));
        Check(testRoot, "UI_IgnoredTextArtWidth", workbook => workbook.Worksheet("TextArt").Cell("D5").Value = "Ignore", "GD2101");
        Check(testRoot, "UI_MalformedLocalization", workbook => workbook.Worksheet("Localization").Cell("C6").Value = "uint32", "GD2101");
        Check(testRoot, "UI_UnknownTextWithoutReference", workbook =>
        {
            workbook.Worksheet("SceneOption").Cell("E12").Value = "";
            workbook.Worksheet("SceneOption").Cell("E14").Value = 999;
        }, "GD2122");
        Check(testRoot, "UI_FourSlotsIncludesDisabled", workbook => workbook.Worksheet("SceneOption").Row(25).Delete(), "GD2105");
        Check(testRoot, "UI_MaximumFixedFour", workbook => workbook.Worksheet("SceneTypeConfig").Cell("C14").Value = 8, "GD2102");
        Check(testRoot, "UI_NoScrollOnly", workbook => workbook.Worksheet("SceneTypeConfig").Cell("D14").Value = "Scroll", "GD2109");

        Check(testRoot, "Art_AlternateDataStream", workbook => workbook.Worksheet("TextArt").Cell("C14").Value = "Asset/logo.txt:stream", "GD2114");
        Check(testRoot, "Art_ReparseComponent", workbook => workbook.Worksheet("TextArt").Cell("C14").Value = "Asset/link/logo.txt", "GD2115", mutateAssets: root =>
        {
            string target = Path.Combine(root, "Actual");
            Directory.CreateDirectory(target);
            File.Copy(Path.Combine(root, "Asset", "logo.txt"), Path.Combine(target, "logo.txt"));
            string link = Path.Combine(root, "Asset", "link");
            if (OperatingSystem.IsWindows())
            {
                var start = new System.Diagnostics.ProcessStartInfo("cmd.exe") { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true };
                foreach (string argument in new[] { "/c", "mklink", "/J", link, target }) start.ArgumentList.Add(argument);
                using var process = System.Diagnostics.Process.Start(start)!;
                process.WaitForExit();
                if (process.ExitCode != 0) throw new InvalidOperationException("Could not create junction fixture: " + process.StandardError.ReadToEnd());
            }
            else Directory.CreateSymbolicLink(link, target);
        });
        Check(testRoot, "Art_TrailingDotAlias", workbook => workbook.Worksheet("TextArt").Cell("C14").Value = "Asset/logo.txt.", "GD2114");
        Check(testRoot, "Art_UnsupportedDimensionsEvenUnused", workbook =>
        {
            var sheet = workbook.Worksheet("TextArt");
            sheet.Cell("B16").Value = 104;
            sheet.Cell("C16").Value = "Asset/square.txt";
            sheet.Cell("D16").Value = 1;
            sheet.Cell("E16").Value = 1;
        }, "GD2140");
        Check(testRoot, "Art_StrictUtf8", _ => { }, "GD2141", mutateAssets: root => File.WriteAllBytes(Path.Combine(root, "Asset", "square.txt"), [0xC0, 0xAF]));
        Check(testRoot, "Art_ExactCellWidth", _ => { }, "GD2142", mutateAssets: root => File.WriteAllText(Path.Combine(root, "Asset", "square.txt"), string.Join('\n', Enumerable.Repeat(new string(' ', 35), 36))));
        Check(testRoot, "Art_BomCrlfTrailingSpaces", _ => { }, null, mutateAssets: root => File.WriteAllText(Path.Combine(root, "Asset", "square.txt"), string.Join("\r\n", Enumerable.Repeat(new string(' ', 36), 36)) + "\r\n", new System.Text.UTF8Encoding(true)));
        foreach (string glyph in new[] { "\t", "한", "\u0301", "😀", "\0", "●", "\uFEFF", "\r" })
            Check(testRoot, "Art_RejectGlyph_" + char.ConvertToUtf32(glyph, 0).ToString("X"), _ => { }, "GD2143", mutateAssets: root =>
                File.WriteAllText(Path.Combine(root, "Asset", "square.txt"), string.Join('\n', new[] { new string(' ', 1) + glyph + new string(' ', 34) }.Concat(Enumerable.Repeat(new string(' ', 36), 35)))));
        Check(testRoot, "Art_ExactApprovedGlyphsIncludingClock", _ => { }, null, mutateAssets: root =>
        {
            string glyphs = string.Concat(Enumerable.Range(0x20, 0x7F - 0x20).Concat(Enumerable.Range(0x2500, 0xA0)).Concat(Enumerable.Range(0x2190, 4)).Concat(new[] { 0x25B2, 0x25B6, 0x25BC, 0x25C0, 0x25CB }).Select(value => (char)value));
            string pixels = glyphs.PadRight(36 * 36);
            File.WriteAllText(Path.Combine(root, "Asset", "square.txt"), string.Join('\n', Enumerable.Range(0, 36).Select(row => pixels.Substring(row * 36, 36))));
        });
        Check(testRoot, "UI_AllEnumSceneTypes", workbook =>
        {
            workbook.Worksheet("Scene").Row(16).Delete();
            workbook.Worksheet("SceneTypeConfig").Row(16).Delete();
            workbook.Worksheet("SceneOption").Rows(22, 25).Delete();
        }, "GD2104");
        Check(testRoot, "UI_PartialSceneFamilyFailsClosed", workbook =>
        {
            workbook.Worksheet("Scene").Delete();
            workbook.Worksheet("SceneOption").Cell("G12").Value = "";
        }, "GD2101");
        Check(testRoot, "Art_ClientFieldScopeExact", workbook => workbook.Worksheet("TextArt").Cell("D5").Value = "Shared", "GD2101");
        Check(testRoot, "Art_PathWhitespaceNotRepaired", workbook => workbook.Worksheet("TextArt").Cell("C14").Value = "Asset/logo.txt ", "GD2114");
        Check(testRoot, "Art_DefaultWhitespaceNotRepaired", workbook =>
        {
            workbook.Worksheet("TextArt").Cell("C10").Value = "Asset/logo.txt ";
            workbook.Worksheet("TextArt").Cell("C14").Value = "";
        }, "GD2114");
        CheckExistingBoundaries(testRoot);
        CheckCppQualification(testRoot);
    }

    private static void CheckNumericKeys(string testRoot)
    {
        foreach (string type in new[] { "string", "bool", "float", "double", "enum<SceneType>" })
            Check(testRoot, "NumericKey_RejectPk_" + type.Replace('<', '_').Replace('>', '_'), workbook =>
                Add(workbook, "FutureItem", ["ItemId"], [type], [[1]]), "GD0307");
        foreach ((object value, string code) in new (object, string)[]
        {
            (0, "GD0331"), (-1, "GD0202"), (4294967296d, "GD0202"),
            (1.5, "GD0210"), (true, "GD0210"), ("1", "GD0210"), ("001", "GD0210"),
            ("MainMenu", "GD0210"), ("NaN", "GD0210"), ("Infinity", "GD0210"),
        })
            Check(testRoot, "NumericKey_InvalidCell_" + value.ToString()!.Replace('.', '_'), workbook =>
                Add(workbook, "FutureSkill", ["SkillId"], ["uint32"], [[value]]), code);
        Check(testRoot, "NumericKey_Duplicate", workbook =>
            Add(workbook, "FutureSkill", ["SkillId"], ["uint32"], [[1], [1]]), "GD0330");
        Check(testRoot, "NumericKey_Missing", workbook =>
            Add(workbook, "FutureSkill", ["SkillId", "Name"], ["uint32", "string"], [["", "Present"]]), "GD0201");
        Check(testRoot, "NumericKey_NoDefaultIdentity", workbook =>
        {
            Add(workbook, "FutureSkill", ["SkillId", "Name"], ["uint32", "string"], [["", "Present"]]);
            workbook.Worksheet("FutureSkill").Cell("B10").Value = 1;
        }, "GD0210");
        foreach (string type in new[] { "int32", "uint32", "int64", "uint64" })
            Check(testRoot, "NumericKey_GenericInteger_" + type, workbook =>
                Add(workbook, "FutureSkill", ["SkillId"], [type], [[1], [2147483647]]), null);
        Check(testRoot, "NumericKey_UInt32Maximum", workbook =>
            Add(workbook, "FutureSkill", ["SkillId"], ["uint32"], [[4294967295d]]), null);
        foreach ((string name, string reference, string type, object value, string code) in new[]
        {
            ("NonPkTarget", "Independent.Label", "uint32", (object)1, "GD0342"),
            ("StringTarget", "Independent.Label", "string", (object)"Label", "GD0343"),
            ("Mismatched", "Independent.Id", "int32", (object)1, "GD0343"),
            ("MissingTarget", "Absent.Id", "uint32", (object)1, "GD0340"),
            ("MissingField", "Independent.Missing", "uint32", (object)1, "GD0341"),
            ("Unknown", "Independent.Id", "uint32", (object)999, "GD0344"),
            ("TextFk", "Independent.Id", "uint32", (object)"001", "GD0210"),
        })
            Check(testRoot, "NumericKey_Reference_" + name, workbook =>
            {
                Add(workbook, "Independent", ["Id", "Label"], ["uint32", "string"], [[1, "Label"]]);
                Add(workbook, "FutureSkill", ["SkillId", "Ref"], ["uint32", type], [[1, value]]);
                workbook.Worksheet("FutureSkill").Cell("C12").Value = reference;
            }, code);
        Check(testRoot, "NumericKey_DisabledNonzeroTarget", workbook =>
        {
            workbook.Worksheet("SceneOption").Cell("G15").Value = 999;
            workbook.Worksheet("SceneOption").Cell("G12").Value = "";
        }, "GD2122");
        Check(testRoot, "NumericKey_DefaultZeroRequired", workbook =>
            workbook.Worksheet("SceneOption").Cell("G10").Value = "", "GD0324");
        Check(testRoot, "NumericKey_DuplicateConfigType", workbook =>
            workbook.Worksheet("SceneTypeConfig").Cell("B15").Value = "MainMenu", "GD2116");
        Check(testRoot, "NumericKey_EnumNameUnique", workbook =>
            workbook.Worksheet("Enums").Cell("B15").Value = "SceneType", "GD0903");
        foreach ((string table, string key, string field, string target) in new[]
        {
            ("CharacterDefinition", "CharacterId", "NameTextKey", "Localization.TextKey"),
            ("CharacterDefinition", "CharacterId", "ArtId", "TextArt.ArtId"),
            ("NewGameProfile", "ProfileId", "CharacterId", "CharacterDefinition.CharacterId"),
            ("NewGameProfile", "ProfileId", "InitialLevel", "LevelDefinition.Level"),
            ("AchievementDefinition", "AchievementId", "NameTextKey", "Localization.TextKey"),
        })
            Check(testRoot, "NumericKey_RequiredReference_" + table + field, workbook =>
                Add(workbook, table, [key, field], ["uint32", "uint32"], [[1, 1]]), "GD2120");
    }

    private static void CheckExistingBoundaries(string testRoot)
    {
        foreach ((string table, int fields) in new[] { ("Scene", 3), ("SceneOption", 7), ("SceneTypeConfig", 4), ("TextArt", 4), ("Localization", 3) })
        {
            for (int column = 2; column <= fields + 1; column++)
            {
                int fieldColumn = column;
                Check(testRoot, $"Metadata_Client_{table}_{column}", workbook => workbook.Worksheet(table).Cell(5, fieldColumn).Value = "Ignore", "GD2101");
                Check(testRoot, $"Metadata_Required_{table}_{column}", workbook =>
                {
                    var cell = workbook.Worksheet(table).Cell(7, fieldColumn);
                    cell.Value = !cell.GetValue<bool>();
                }, "GD2101");
            }
            Check(testRoot, "Metadata_Target_" + table, workbook => workbook.Worksheet(table).Cell("B2").Value = "Server", "GD2101");
        }
        Check(testRoot, "Art_MissingFile", _ => { }, "GD2113", mutateAssets: root => File.Delete(Path.Combine(root, "Asset", "square.txt")));
        Check(testRoot, "Art_ParentTraversal", workbook => workbook.Worksheet("TextArt").Cell("C14").Value = "../outside.txt", "GD2111");
        Check(testRoot, "Art_RootedPath", workbook => workbook.Worksheet("TextArt").Cell("C14").Value = "C:/outside.txt", "GD2111");
        Check(testRoot, "Art_ReservedDevice", workbook => workbook.Worksheet("TextArt").Cell("C14").Value = "Asset/CON.txt", "GD2114");
        Check(testRoot, "Art_ExactRows", _ => { }, "GD2142", mutateAssets: root => File.WriteAllText(Path.Combine(root, "Asset", "square.txt"), string.Join('\n', Enumerable.Repeat(new string(' ', 36), 35))));
        Check(testRoot, "Art_SecondTerminalNewline", _ => { }, "GD2142", mutateAssets: root => File.AppendAllText(Path.Combine(root, "Asset", "square.txt"), "\n\n"));
        Check(testRoot, "UI_DeletedOptionTextReference", workbook => workbook.Worksheet("SceneOption").Cell("E12").Value = "", "GD2120");
        Check(testRoot, "UI_DisabledStartMayHaveBlankTarget", workbook =>
        {
            workbook.Worksheet("SceneOption").Cell("H14").Value = false;
            workbook.Worksheet("SceneOption").Cell("G14").Value = "";
        }, null);
        Check(testRoot, "UI_FifthDisabledSlot", workbook =>
        {
            var sheet = workbook.Worksheet("SceneOption");
            object[] values = [999, "BattleField", 5, 1, "None", 0, false];
            for (int column = 0; column < values.Length; column++) sheet.Cell(26, column + 2).Value = XLCellValue.FromObject(values[column]);
        }, "GD2105");
        Check(testRoot, "UI_DuplicateOrder", workbook => workbook.Worksheet("SceneOption").Cell("D15").Value = 1, "GD2106");
        Check(testRoot, "UI_NonContiguousOrder", workbook => workbook.Worksheet("SceneOption").Cell("D17").Value = 5, "GD2107");
        Check(testRoot, "UI_MissingTranslation", workbook => workbook.Worksheet("Localization").Cell("D14").Value = "", "GD0201");
        Check(testRoot, "StandaloneTextArt", workbook =>
        {
            foreach (string name in new[] { "Scene", "SceneOption", "SceneTypeConfig", "Localization" }) workbook.Worksheet(name).Delete();
        }, null);
        Check(testRoot, "StandaloneTextArtStillValidatesFiles", workbook =>
        {
            foreach (string name in new[] { "Scene", "SceneOption", "SceneTypeConfig", "Localization" }) workbook.Worksheet(name).Delete();
        }, "GD2113", mutateAssets: root => File.Delete(Path.Combine(root, "Asset", "square.txt")));
        foreach (string field in new[] { "PlayerArtId", "RegionTextKey", "TargetSceneId" })
            Check(testRoot, "GenericOptionalNoDefault_" + field, workbook =>
            {
                Add(workbook, "Independent", ["Id", field], ["uint32", "string"], [[1, "Value"]]);
                workbook.Worksheet("Independent").Cell("C7").Value = false;
            }, "GD0324");
    }

    private static void CheckCppQualification(string testRoot)
    {
        using var workbook = CreateWorkbook();
        foreach (string name in new[] { "Scene", "SceneOption", "SceneTypeConfig", "Localization", "TextArt" }) workbook.Worksheet(name).Delete();
        Add(workbook, "std", ["Id", "Label", "SceneType"], ["uint32", "string", "enum<SceneType>"], [[1, "test", "MainMenu"]]);
        string input = Path.Combine(testRoot, "StdProbe");
        Directory.CreateDirectory(input);
        workbook.SaveAs(Path.Combine(input, "std.xlsx"));
        var diagnostics = new FDiagnosticBag();
        var tables = FWorkbookParser.ParseDirectory(input, diagnostics);
        var enums = FGameDataValidator.Validate(tables, diagnostics);
        if (diagnostics.HasErrors) throw new InvalidOperationException(string.Join('\n', diagnostics.GetSorted()));
        IReadOnlyList<SGeneratedFile> files = FGameDataGenerators.Generate(tables, enums);
        FGameDataGeneratorSelfTest.AssertCppHeaderContract(files);
        string header = files.Single(file => file.RelativePath.EndsWith("stdData.g.h", StringComparison.Ordinal)).Content;
        if (System.Text.RegularExpressions.Regex.IsMatch(header, @"(?<!:)\bstd::"))
            throw new InvalidOperationException("Scene self-test GEN05: generated std table contains shadowable std:: names.");
        File.WriteAllText(Path.Combine(input, "stdData.g.h"), header);
        Console.WriteLine("PASS SceneContract/GEN05_StdQualification");
    }

    private static void AddCombatCohort(XLWorkbook workbook)
    {
        var enums = workbook.Worksheet("Enums");
        foreach ((int column, object value) in new (int, object)[]
            { (2, "CurrencyType"), (3, "Shared"), (4, "uint32"), (5, "Gold=1"), (6, 4) })
            enums.Cell(17, column).Value = XLCellValue.FromObject(value);
        Add(workbook, "CharacterDefinition", ["CharacterId", "NameTextKey", "ArtId"],
            ["uint32", "uint32", "uint32"], [[1, 1, 2]]);
        Add(workbook, "LevelDefinition", ["Level", "RequiredExperience", "MaxHp", "Attack", "Defense"],
            Enumerable.Repeat("uint32", 5).ToArray(), [[1, 0, 100, 10, 5]]);
        Add(workbook, "NewGameProfile", ["ProfileId", "CharacterId", "InitialLevel", "InitialExperience", "InitialGold",
            "InitialMaxHpBonus", "InitialAttackBonus", "InitialDefenseBonus", "ContentVersion", "IsDefault"],
            Enumerable.Repeat("uint32", 9).Append("bool").ToArray(), [[1, 1, 1, 0, 0, 0, 0, 0, 1, true]]);
        Add(workbook, "ItemDefinition", ["ItemId", "NameTextKey", "MaxStack"],
            ["uint32", "uint32", "uint32"], [[1, 1, 99]]);
        Add(workbook, "AchievementDefinition", ["AchievementId", "NameTextKey", "TargetCount"],
            ["uint32", "uint32", "uint32"], [[1, 1, 1]]);
        Add(workbook, "CharacterCombat", ["CharacterId", "Strength", "Dexterity", "Intelligence", "Luck"],
            Enumerable.Repeat("uint32", 5).ToArray(), [[1, 5, 5, 3, 5]]);
        Add(workbook, "MonsterDefinition", ["MonsterId", "NameTextKey", "ArtId", "MaxHp", "Attack", "Defense",
            "Strength", "Dexterity", "Intelligence", "Luck",
            "RewardExperience", "RewardCurrencyType", "RewardAmountMin", "RewardAmountMax"],
            Enumerable.Repeat("uint32", 11).Append("enum<CurrencyType>").Concat(Enumerable.Repeat("uint32", 2)).ToArray(),
            [[1, 1, 2, 45, 7, 2, 3, 4, 0, 3, 300, "Gold", 10, 20]]);
        Add(workbook, "CombatRule", ["RuleId", "MonsterId", "BattleSceneId", "TownSceneId", "Scale", "StrengthDefense",
            "DexteritySpeed", "BaseSpeed", "LuckCritical", "CriticalCap", "CriticalMultiplier", "MinimumDamage",
            "StrengthCoefficient", "DexterityCoefficient", "IntelligenceCoefficient", "LuckCoefficient", "MaxStat", "MaxRounds",
            "HealOnReturn", "HitTextKey", "CriticalTextKey", "VictoryTextKey", "DefeatTextKey", "ReadyTextKey", "RoundTextKey"],
            Enumerable.Repeat("uint32", 25).ToArray(),
            [[1, 1, 3, 2, 10000, 10000, 10000, 1, 100, 5000, 15000, 1, 10000, 2000, 0, 0, 1000000, 1000000, 1, 1, 1, 1, 1, 1, 1]]);
        foreach ((string table, string column, string target) in new[]
        {
            ("CharacterDefinition", "C", "Localization.TextKey"), ("CharacterDefinition", "D", "TextArt.ArtId"),
            ("NewGameProfile", "C", "CharacterDefinition.CharacterId"), ("NewGameProfile", "D", "LevelDefinition.Level"),
            ("ItemDefinition", "C", "Localization.TextKey"), ("AchievementDefinition", "C", "Localization.TextKey"),
            ("CharacterCombat", "B", "CharacterDefinition.CharacterId"),
            ("MonsterDefinition", "C", "Localization.TextKey"), ("MonsterDefinition", "D", "TextArt.ArtId"),
            ("CombatRule", "C", "MonsterDefinition.MonsterId"), ("CombatRule", "D", "Scene.SceneId"),
            ("CombatRule", "E", "Scene.SceneId"),
        }) workbook.Worksheet(table).Cell(column + "12").Value = target;
        for (int column = 21; column <= 26; column++)
            workbook.Worksheet("CombatRule").Cell(12, column).Value = "Localization.TextKey";
    }

    private static void CheckCombatContracts(string testRoot)
    {
        Check(testRoot, "Combat_CompletePositive", _ => { }, null, combat: true);
        foreach (string minimum in new[] { "", "0" })
        {
            Check(testRoot, "Reward_PositiveHpRelaxedMin_" + minimum, workbook =>
                workbook.Worksheet("LevelDefinition").Cell("D8").Value = minimum, null, combat: true);
            Check(testRoot, "Reward_ZeroHpRelaxedMin_" + minimum, workbook =>
            {
                workbook.Worksheet("LevelDefinition").Cell("D8").Value = minimum;
                workbook.Worksheet("LevelDefinition").Cell("D14").Value = 0;
            }, "GD2213", expectedCell: "D14", expectedSheet: "LevelDefinition", combat: true);
        }
        Check(testRoot, "Reward_LevelHpCannotBeIgnored", workbook =>
            workbook.Worksheet("LevelDefinition").Cell("D5").Value = "Ignore", "GD2212",
            expectedCell: "D5", expectedSheet: "LevelDefinition", combat: true);
        Check(testRoot, "Reward_LevelTableMustBeClientVisible", workbook =>
            workbook.Worksheet("LevelDefinition").Cell("B2").Value = "Server", "GD2212",
            expectedCell: "B2", expectedSheet: "LevelDefinition", combat: true);
        Check(testRoot, "Reward_InvertedRange", workbook =>
            workbook.Worksheet("MonsterDefinition").Cell("N14").Value = 21, "GD2209",
            expectedCell: "N14", expectedSheet: "MonsterDefinition", combat: true);
        Check(testRoot, "Reward_CurrencyTypeNotRegistered", workbook =>
            workbook.Worksheet("MonsterDefinition").Cell("M14").Value = "Unknown", "GD0911",
            expectedCell: "M14", expectedSheet: "MonsterDefinition", combat: true);
        Check(testRoot, "Reward_CurveMustStartAtOne", workbook =>
            workbook.Worksheet("LevelDefinition").Cell("C14").Value = 1, "GD2210",
            expectedCell: "B14", expectedSheet: "LevelDefinition", combat: true);
        Check(testRoot, "Combat_WholeCohortAbsent", workbook =>
        {
            foreach (string table in new[] { "CharacterCombat", "MonsterDefinition", "CombatRule" })
                workbook.Worksheet(table).Delete();
        }, null, combat: true);
        string[] progressTables = ["NewGameProfile", "CharacterDefinition", "LevelDefinition", "ItemDefinition", "AchievementDefinition"];
        foreach (string missing in progressTables)
        {
            Check(testRoot, "Progress_PartialWithoutCombat_" + missing, workbook =>
            {
                foreach (string table in new[] { "CharacterCombat", "MonsterDefinition", "CombatRule", missing })
                    workbook.Worksheet(table).Delete();
            }, "GD2214", expectedCell: "B1",
                expectedSheet: missing == "AchievementDefinition" ? "CharacterDefinition" : "AchievementDefinition", combat: true);
        }
        Check(testRoot, "Progress_WholeFamilyAbsent", workbook =>
        {
            foreach (string table in progressTables.Concat(new[] { "CharacterCombat", "MonsterDefinition", "CombatRule" }))
                workbook.Worksheet(table).Delete();
        }, null, combat: true);
        foreach ((string cell, int value) in new[] { ("D14", 2), ("E14", 3) })
            Check(testRoot, "Combat_SceneRole_" + cell, workbook =>
                workbook.Worksheet("CombatRule").Cell(cell).Value = value, "GD2207",
                expectedCell: cell, expectedSheet: "CombatRule", combat: true);
        Check(testRoot, "Combat_MonsterLogoSlot", workbook =>
            workbook.Worksheet("MonsterDefinition").Cell("D14").Value = 1, "GD2208",
            expectedCell: "D14", expectedSheet: "MonsterDefinition", combat: true);
        Check(testRoot, "Combat_CharacterReferenceDeleted", workbook =>
            workbook.Worksheet("CharacterCombat").Cell("B12").Value = "", "GD2120",
            expectedCell: "B12", expectedSheet: "CharacterCombat", combat: true);
        Check(testRoot, "Combat_CharacterMembershipWithoutReference", workbook =>
        {
            workbook.Worksheet("CharacterCombat").Cell("B12").Value = "";
            workbook.Worksheet("CharacterCombat").Cell("B14").Value = 999;
        }, "GD2122", expectedCell: "B14", expectedSheet: "CharacterCombat", combat: true);
        Check(testRoot, "Combat_CharacterReferenceWrongTarget", workbook =>
            workbook.Worksheet("CharacterCombat").Cell("B12").Value = "ItemDefinition.ItemId", "GD2120",
            expectedCell: "B12", expectedSheet: "CharacterCombat", combat: true);
        foreach ((string cell, string value) in new[] { ("B4", ""), ("B5", "Client") })
            Check(testRoot, "Combat_CharacterPrimaryRole_" + cell, workbook =>
                workbook.Worksheet("CharacterCombat").Cell(cell).Value = value, "GD2101", combat: true);
        foreach ((string table, int count) in new[] { ("CharacterCombat", 5), ("MonsterDefinition", 14), ("CombatRule", 25) })
        {
            for (int column = 2; column <= count + 1; column++)
            foreach (string scope in new[] { "Ignore", "Server" })
            {
                int fieldColumn = column;
                Check(testRoot, $"Combat_Visibility_{table}_{column}_{scope}", workbook =>
                    workbook.Worksheet(table).Cell(5, fieldColumn).Value = scope, "GD2202",
                    expectedCell: FCellAddress.ToA1(5, column), expectedSheet: table, combat: true);
            }
            Check(testRoot, "Combat_TargetServer_" + table, workbook =>
                workbook.Worksheet(table).Cell("B2").Value = "Server", "GD2202",
                expectedCell: "B2", expectedSheet: table, combat: true);
            Check(testRoot, "Combat_TargetShared_" + table, workbook =>
                workbook.Worksheet(table).Cell("B2").Value = "Shared", null, combat: true);
        }
        Check(testRoot, "Combat_SharedFieldsPositive", workbook =>
        {
            foreach (string table in new[] { "CharacterCombat", "MonsterDefinition", "CombatRule" })
                for (int column = 2; column <= workbook.Worksheet(table).LastColumnUsed()!.ColumnNumber(); column++)
                    workbook.Worksheet(table).Cell(5, column).Value = "Shared";
        }, null, combat: true);
        Check(testRoot, "GenericIgnorePreserved", workbook =>
        {
            Add(workbook, "Independent", ["Id", "Value"], ["uint32", "uint32"], [[1, 2]]);
            workbook.Worksheet("Independent").Cell("C5").Value = "Ignore";
        }, null);
        Check(testRoot, "GenericServerPreserved", workbook =>
        {
            Add(workbook, "Independent", ["Id", "Value"], ["uint32", "uint32"], [[1, 2]]);
            workbook.Worksheet("Independent").Cell("C5").Value = "Server";
        }, "GD1902");
    }

    private static void CheckCombatBounds(string testRoot)
    {
        string input = Path.Combine(testRoot, "CombatBounds", "Excel");
        Directory.CreateDirectory(input);
        using var workbook = new XLWorkbook();
        Add(workbook, "CharacterCombat", ["CharacterId", "Strength", "Dexterity", "Intelligence", "Luck"],
            Enumerable.Repeat("uint32", 5).ToArray(), [[1, 5, 5, 3, 5]]);
        Add(workbook, "MonsterDefinition", ["MonsterId", "NameTextKey", "ArtId", "MaxHp", "Attack", "Defense",
            "Strength", "Dexterity", "Intelligence", "Luck",
            "RewardExperience", "RewardCurrencyType", "RewardAmountMin", "RewardAmountMax"],
            Enumerable.Repeat("uint32", 11).Append("enum<CurrencyType>").Concat(Enumerable.Repeat("uint32", 2)).ToArray(),
            [[1, 37, 3, 45, 7, 2, 3, 4, 0, 3, 300, "Gold", 10, 20]]);
        Add(workbook, "CombatRule", ["RuleId", "MonsterId", "BattleSceneId", "TownSceneId", "Scale", "StrengthDefense",
            "DexteritySpeed", "BaseSpeed", "LuckCritical", "CriticalCap", "CriticalMultiplier", "MinimumDamage",
            "StrengthCoefficient", "DexterityCoefficient", "IntelligenceCoefficient", "LuckCoefficient", "MaxStat", "MaxRounds",
            "HealOnReturn", "HitTextKey", "CriticalTextKey", "VictoryTextKey", "DefeatTextKey", "ReadyTextKey", "RoundTextKey"],
            Enumerable.Repeat("uint32", 25).ToArray(),
            [[1, 1, 3, 2, 10000, 10000, 10000, 1, 100, 5000, 15000, 1, 10000, 2000, 0, 0, 1000000, 1000000, 1, 38, 39, 40, 41, 42, 43]]);
        // 이 컴포넌트 검사는 도메인 검증기를 검사하며 전체 카탈로그는 실제 CLI/native 회귀에서 검증한다.
        foreach (string table in new[] { "CharacterDefinition", "NewGameProfile", "ItemDefinition", "AchievementDefinition" })
            Add(workbook, table, ["Id"], ["uint32"], [[1]]);
        Add(workbook, "LevelDefinition", ["Level", "RequiredExperience", "MaxHp", "Attack", "Defense"],
            Enumerable.Repeat("uint32", 5).ToArray(), [[1, 0, 100, 10, 5]]);
        Add(workbook, "Scene", ["SceneId", "SceneType", "IsStartupScene"],
            ["uint32", "enum<SceneType>", "bool"], [[2, "Town", false], [3, "BattleField", false]]);
        Add(workbook, "TextArt", ["ArtId", "AssetPath", "Width", "Height"],
            ["uint32", "string", "uint32", "uint32"], [[3, "Asset/square.txt", 36, 36]]);
        void CheckDomain(string label, string? expected)
        {
            workbook.SaveAs(Path.Combine(input, "Combat.xlsx"));
            var diagnostics = new FDiagnosticBag();
            IReadOnlyList<FGameDataTable> tables = FWorkbookParser.ParseDirectory(input, diagnostics);
            if (diagnostics.HasErrors) throw new InvalidOperationException("Combat component parser control failed: " + label);
            FCombatDataValidation.Validate(tables, diagnostics);
            SGameDataDiagnostic[] actual = diagnostics.GetSorted().ToArray();
            if (expected is null ? actual.Length != 0 : actual.Length != 1 || actual[0].Code != expected)
                throw new InvalidOperationException("Combat component diagnostic mismatch: " + label + " " + string.Join('\n', actual.AsEnumerable()));
            Console.WriteLine("PASS CombatBounds/" + label);
        }
        CheckDomain("CompletePositive", null);
        workbook.Worksheet("CombatRule").Cell("S14").Value = 1;
        CheckDomain("CapOne", "GD2205");
        workbook.Worksheet("CombatRule").Cell("M14").Value = 2;
        workbook.Worksheet("CombatRule").Cell("S14").Value = 23;
        CheckDomain("CeilingExact", null);
        workbook.Worksheet("CombatRule").Cell("S14").Value = 22;
        CheckDomain("CeilingOneBelow", "GD2205");
        workbook.Worksheet("CombatRule").Cell("S14").Value = 1000000;
        workbook.Worksheet("MonsterDefinition").Cell("F14").Value = 1000000;
        CheckDomain("AttackMaximum", null);
        workbook.Worksheet("MonsterDefinition").Cell("F14").Value = 1000001;
        CheckDomain("AttackMaximumPlusOne", "GD2206");
    }

    private static void Check(string testRoot, string name, Action<XLWorkbook> mutate, string? expectedCode, bool nested = false,
        Action<string>? mutateAssets = null, string? expectedCell = null, string expectedSheet = "Scene", bool combat = false)
    {
        string root = Path.Combine(testRoot, name);
        string input = Path.Combine(root, "Excel");
        Directory.CreateDirectory(input);
        Directory.CreateDirectory(Path.Combine(root, "Asset"));

        File.WriteAllText(Path.Combine(root, "Asset", "logo.txt"), string.Join('\n', Enumerable.Repeat(new string(' ', 83), 41)));
        File.WriteAllText(Path.Combine(root, "Asset", "square.txt"), string.Join('\n', Enumerable.Repeat(new string(' ', 36), 36)));
        mutateAssets?.Invoke(root);
        string workbookRoot = nested ? Path.Combine(input, "Sub", "Deep") : input;
        Directory.CreateDirectory(workbookRoot);
        string path = Path.Combine(workbookRoot, "Scene.xlsx");
        using (XLWorkbook workbook = CreateWorkbook())
        {
            if (combat) AddCombatCohort(workbook);
            mutate(workbook);
            workbook.SaveAs(path);
        }
        var diagnostics = new FDiagnosticBag();
        var tables = FWorkbookParser.ParseDirectory(input, diagnostics);
        var enums = FGameDataValidator.Validate(tables, diagnostics);
        if (expectedCode == null && !diagnostics.HasErrors && tables.Any(table => table.Name == "SceneOption"))
        {
            string yaml = FGameDataGenerators.Generate(tables, enums).Single(file => file.RelativePath == "Data/Client/SceneOption.yaml").Content;
            if (!yaml.Contains("  \"TargetSceneId\": 0\n", StringComparison.Ordinal) ||
                yaml.Contains("  \"TargetSceneId\": null\n", StringComparison.Ordinal))
                throw new InvalidOperationException("Optional numeric targets must emit zero, never null: " + name);
        }
        string messages = string.Join('\n', diagnostics.GetSorted());
        string fixtureLink = Path.Combine(root, "Asset", "link");
        if (Directory.Exists(fixtureLink)) Directory.Delete(fixtureLink);
        bool success = expectedCode == null ? !diagnostics.HasErrors : diagnostics.GetSorted().Any(item => item.Code == expectedCode &&
            (expectedCell == null || (item.Location.FilePath == path && item.Location.SheetName == expectedSheet &&
                item.Location.CellAddress == expectedCell)));
        if (!success) throw new InvalidOperationException($"Scene self-test {name}: expected {expectedCode ?? "valid"}{(expectedCell == null ? "" : $" [{expectedSheet}] {expectedCell}")}; actual:\n{messages}");
        if (diagnostics.HasErrors && (combat || name.StartsWith("NumericKey_", StringComparison.Ordinal) ||
            name is "GEN01_ClientFields" or "GEN02_NoOptionConfig" or "UI_UnknownTextWithoutReference" or "Art_MissingFile" or "Art_ExactCellWidth"))
        {
            byte[] inputBefore = File.ReadAllBytes(path);
            string cpp = Path.Combine(root, "PreviousCpp");
            string yaml = Path.Combine(root, "PreviousYaml");
            foreach (string output in new[] { cpp, yaml })
            {
                Directory.CreateDirectory(output);
                File.WriteAllText(Path.Combine(output, ".gamedata-output"), "GameDataGenerator owned output v1\n");
                File.WriteAllText(Path.Combine(output, "sentinel.txt"), "previous output must survive");
            }
            foreach (string mode in combat ? new[] { "", "--validate-only", "--check" } : new[] { "" })
            {
                string[] arguments = ["--input-root", input, "--output-root", cpp, "--yaml-output-root", yaml];
                if (mode.Length != 0) arguments = [.. arguments, mode];
                if (Program.Main(arguments) != 1 || !File.ReadAllBytes(path).SequenceEqual(inputBefore) ||
                    new[] { cpp, yaml }.Any(output => Directory.GetFiles(output, "*", SearchOption.AllDirectories).Length != 2 ||
                        File.ReadAllText(Path.Combine(output, "sentinel.txt")) != "previous output must survive" ||
                        File.ReadAllText(Path.Combine(output, ".gamedata-output")) != "GameDataGenerator owned output v1\n"))
                    throw new InvalidOperationException("Failed Scene CLI changed previous output: " + name + " " + mode);
            }
            Console.WriteLine($"PASS SceneContract/PreservedOutputs_{name}");
        }
        if (diagnostics.GetSorted().Any(item => item.Location.FilePath != path || string.IsNullOrWhiteSpace(item.Location.SheetName) || string.IsNullOrWhiteSpace(item.Location.CellAddress)))
            throw new InvalidOperationException("Scene diagnostic lost workbook/sheet/cell location: " + name);
        Console.WriteLine($"PASS SceneContract/{name}");
    }

    private static XLWorkbook CreateWorkbook()
    {
        var workbook = new XLWorkbook();
        Add(workbook, "Enums", ["EnumName", "Target", "UnderlyingType", "Values"], ["string", "string", "string", "string"],
            [["SceneType", "Client", "uint8", "MainMenu=1|Town=2|BattleField=3"],
             ["SceneActionType", "Client", "uint8", "None=0|StartGame=1|Quit=2"],
             ["OptionOverflowPolicy", "Client", "uint8", "NoScroll=1|Scroll=2"]]);
        workbook.Worksheet("Enums").Cell("B2").Value = "Shared";
        for (int column = 2; column <= 6; column++) workbook.Worksheet("Enums").Cell(5, column).Value = "Shared";
        var enumSheet = workbook.Worksheet("Enums");
        enumSheet.Cell("B4").Value = "";
        enumSheet.Cell("F4").Value = "Primary";
        enumSheet.Cell("F6").Value = "uint32";
        enumSheet.Cell("F7").Value = true;
        enumSheet.Cell("F13").Value = "EnumId";
        for (int row = 14; row <= 16; row++) enumSheet.Cell(row, 6).Value = row - 13;
        Add(workbook, "Scene", ["SceneId", "SceneType", "IsStartupScene"],
            ["uint32", "enum<SceneType>", "bool"],
            [[1, "MainMenu", true],
             [2, "Town", false],
             [3, "BattleField", false]]);
        Add(workbook, "TextArt", ["ArtId", "AssetPath", "Width", "Height"], ["uint32", "string", "uint32", "uint32"],
            [[1, "Asset/logo.txt", 83, 41], [2, "Asset/square.txt", 36, 36]]);
        Add(workbook, "SceneOption", ["OptionId", "SceneType", "OptionOrder", "OptionTextKey", "ActionType", "TargetSceneId", "IsEnabled"],
            ["uint32", "enum<SceneType>", "uint32", "uint32", "enum<SceneActionType>", "uint32", "bool"],
            [[1, "MainMenu", 1, 1, "StartGame", 1, true],
             [2, "MainMenu", 2, 1, "None", 0, false],
             [3, "MainMenu", 3, 1, "None", 0, false],
             [4, "MainMenu", 4, 1, "Quit", 0, true]]);
        for (int type = 1; type <= 2; type++)
            for (int order = 1; order <= 4; order++)
            {
                object[] values = [type * 100 + order, type == 1 ? "Town" : "BattleField", order, 1, "None", 0, false];
                for (int column = 0; column < values.Length; column++)
                    workbook.Worksheet("SceneOption").Cell(13 + type * 4 + order, column + 2).Value = XLCellValue.FromObject(values[column]);
            }
        Add(workbook, "SceneTypeConfig", ["SceneType", "MaxVisibleOptions", "OptionOverflowPolicy"],
            ["enum<SceneType>", "uint32", "enum<OptionOverflowPolicy>"], [["MainMenu", 4, "NoScroll"], ["Town", 4, "NoScroll"], ["BattleField", 4, "NoScroll"]]);
        var config = workbook.Worksheet("SceneTypeConfig");
        config.Cell("B4").Value = "";
        config.Cell("E4").Value = "Primary";
        config.Cell("E5").Value = "Shared";
        config.Cell("E6").Value = "uint32";
        config.Cell("E7").Value = true;
        config.Cell("E13").Value = "ConfigId";
        for (int row = 14; row <= 16; row++) config.Cell(row, 5).Value = row - 13;
        Add(workbook, "Localization", ["TextKey", "ko-KR", "en-US"], ["uint32", "string", "string"], [[1, "시작", "Start"]]);
        // 실제 프로젝트 현지화 필드명은 식별자여야 하므로 언어 열의 정식 이름을 쓴다.
        workbook.Worksheet("Localization").Cell("C13").Value = "KoKR";
        workbook.Worksheet("Localization").Cell("D13").Value = "EnUS";
        workbook.Worksheet("SceneOption").Cell("E12").Value = "Localization.TextKey";
        workbook.Worksheet("SceneOption").Cell("G12").Value = "Scene.SceneId";
        workbook.Worksheet("SceneOption").Cell("G7").Value = false;
        workbook.Worksheet("SceneOption").Cell("G10").Value = 0;
        return workbook;
    }

    private static void Add(XLWorkbook workbook, string name, string[] fields, string[] types, object[][] rows)
    {
        IXLWorksheet sheet = workbook.AddWorksheet(name);
        sheet.Cell("A1").Value = "#Table";
        sheet.Cell("B1").Value = name;
        sheet.Cell("A2").Value = "#Target";
        sheet.Cell("B2").Value = "Client";
        string[] labels = ["#Key", "#Scope", "#Type", "#Required", "#Min", "#Max", "#Default", "#Allowed", "#Reference", "#Field"];
        for (int index = 0; index < labels.Length; index++) sheet.Cell(index + 4, 1).Value = labels[index];
        for (int index = 0; index < fields.Length; index++)
        {
            int column = index + 2;
            sheet.Cell(4, column).Value = index == 0 ? "Primary" : "";
            sheet.Cell(5, column).Value = index == 0 ? "Shared" : "Client";
            sheet.Cell(6, column).Value = types[index];
            sheet.Cell(7, column).Value = "true";
            sheet.Cell(13, column).Value = fields[index];
        }
        for (int row = 0; row < rows.Length; row++)
            for (int column = 0; column < rows[row].Length; column++)
                sheet.Cell(row + 14, column + 2).Value = XLCellValue.FromObject(rows[row][column]);
    }
}
