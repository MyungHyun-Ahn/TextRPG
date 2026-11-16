using System.Globalization;

internal static partial class FGameDataValidator
{
    public static IReadOnlyDictionary<string, FGameDataEnumDefinition> Validate(
        IReadOnlyList<FGameDataTable> tables,
        FDiagnosticBag diagnostics)
    {
        IReadOnlyDictionary<string, FGameDataEnumDefinition> enumDefinitions =
            FGameDataEnumCatalog.Build(tables, diagnostics);
        ValidateTableSet(tables, diagnostics);
        ValidatePortableRepresentation(tables, diagnostics);
        foreach (FGameDataTable table in tables)
        {
            ValidateTable(table, diagnostics);
        }

        FGameDataEnumCatalog.ValidateReferences(tables, enumDefinitions, diagnostics);
        ValidateReferences(tables, diagnostics);
        FNumericKeyValidation.Validate(tables, diagnostics);
        ValidateClientOnly(tables, enumDefinitions, diagnostics);
        FSceneDataValidation.Validate(tables, enumDefinitions, diagnostics);
        return enumDefinitions;
    }

    private static void ValidateTableSet(IReadOnlyList<FGameDataTable> tables, FDiagnosticBag diagnostics)
    {
        foreach (IGrouping<string, FGameDataTable> duplicateGroup in tables.GroupBy(table => table.Name, StringComparer.OrdinalIgnoreCase))
        {
            if (duplicateGroup.Count() <= 1)
            {
                continue;
            }

            foreach (FGameDataTable table in duplicateGroup)
            {
                diagnostics.Add(
                    "GD0301",
                    $"Table name '{table.Name}' is declared by more than one worksheet.",
                    table.Locate(1, 2));
            }
        }
    }

    private static void ValidateTable(
        FGameDataTable table,
        FDiagnosticBag diagnostics)
    {
        if (table.Fields.Count == 0)
        {
            diagnostics.Add("GD0302", "Table has no fields.", table.Locate(13, 2));
            return;
        }
        if (table.Rows.Count == 0)
        {
            diagnostics.Add("GD0308", "Table must contain at least one data row.", table.Locate(14, 2));
        }

        foreach (IGrouping<string, FGameDataField> duplicateGroup in table.Fields.GroupBy(field => field.Name, StringComparer.Ordinal))
        {
            if (duplicateGroup.Count() <= 1)
            {
                continue;
            }

            foreach (FGameDataField field in duplicateGroup)
            {
                diagnostics.Add("GD0303", $"Duplicate field name '{field.Name}'.", field.Location);
            }
        }

        FGameDataField[] keyFields = table.Fields.Where(field => field.IsPrimaryKey).ToArray();
        if (keyFields.Length != 1)
        {
            diagnostics.Add(
                "GD0304",
                $"Table must define exactly one primary key, but found {keyFields.Length}.",
                table.Locate(4, 2));
        }
        else
        {
            FGameDataField key = keyFields[0];
            if (!key.Required)
            {
                diagnostics.Add("GD0305", "Primary key must be required.", key.Location);
            }
            if (key.Scope != EGameDataScope.Shared)
            {
                diagnostics.Add("GD0306", "Primary key scope must be Shared.", key.Location);
            }
            if (key.Type != null && !FGameDataIntegerRange.IsInteger(key.Type))
            {
                diagnostics.Add("GD0307", $"Type '{key.Type.SourceName}' cannot be used as a primary key.", key.Location);
            }
        }

        ValidateScopes(table, diagnostics);
        ValidateFieldDefinitions(table, diagnostics);
        ValidateDuplicateKeys(table, diagnostics);
    }

    private static void ValidateClientOnly(
        IReadOnlyList<FGameDataTable> tables,
        IReadOnlyDictionary<string, FGameDataEnumDefinition> enumDefinitions,
        FDiagnosticBag diagnostics)
    {
        foreach (FGameDataTable table in tables)
        {
            if (table.Target == EGameDataTarget.Server)
            {
                diagnostics.Add("GD1901", "TextRPG GameDataGenerator is client-only; table Target cannot be Server.", table.Locate(2, 2));
            }

            foreach (FGameDataField field in table.Fields.Where(field => field.Scope == EGameDataScope.Server))
            {
                diagnostics.Add("GD1902", "TextRPG GameDataGenerator is client-only; field Scope cannot be Server.", field.Location);
            }
        }

        foreach (FGameDataEnumDefinition definition in enumDefinitions.Values.Where(definition => definition.Target == EGameDataTarget.Server))
        {
            diagnostics.Add("GD1903", "TextRPG GameDataGenerator is client-only; enum Target cannot be Server.", definition.Location);
        }
    }

    private static void ValidateScopes(FGameDataTable table, FDiagnosticBag diagnostics)
    {
        foreach (FGameDataField field in table.Fields)
        {
            if (table.Target == EGameDataTarget.Server && field.Scope == EGameDataScope.Client)
            {
                diagnostics.Add("GD0310", "A Server table cannot contain a Client-only field.", field.Location);
            }
            else if (table.Target == EGameDataTarget.Client && field.Scope == EGameDataScope.Server)
            {
                diagnostics.Add("GD0311", "A Client table cannot contain a Server-only field.", field.Location);
            }
        }
    }

    private static void ValidateFieldDefinitions(FGameDataTable table, FDiagnosticBag diagnostics)
    {
        var generatedCppMembers = new HashSet<string>(StringComparer.Ordinal);
        var generatedCSharpProperties = new HashSet<string>(StringComparer.Ordinal);

        foreach (FGameDataField field in table.Fields)
        {
            string cppMemberName = FGameDataNaming.ToCppMemberName(table.Name, field.Name);
            string csharpPropertyName = FGameDataNaming.ToPascalCase(field.Name);
            if (!FGameDataNaming.IsCppIdentifier(cppMemberName))
            {
                diagnostics.Add("GD0325", $"Field '{field.Name}' produces invalid or reserved C++ member name '{cppMemberName}'.", field.Location);
            }
            if (!FGameDataNaming.IsCSharpIdentifier(csharpPropertyName))
            {
                diagnostics.Add("GD0326", $"Field '{field.Name}' produces invalid or reserved C# property name '{csharpPropertyName}'.", field.Location);
            }
            if (string.Equals(csharpPropertyName, "DataId", StringComparison.Ordinal))
            {
                diagnostics.Add("GD0327", "Generated C# property name 'DataId' conflicts with GameDataRow.DataId.", field.Location);
            }
            if (!generatedCppMembers.Add(cppMemberName))
            {
                diagnostics.Add("GD0312", $"Field '{field.Name}' collides with another generated C++ member name.", field.Location);
            }
            if (!generatedCSharpProperties.Add(csharpPropertyName))
            {
                diagnostics.Add("GD0313", $"Field '{field.Name}' collides with another generated C# property name.", field.Location);
            }

            if (!field.Required && field.DefaultText == null)
            {
                diagnostics.Add(
                    "GD0324",
                    $"Optional field '{field.Name}' must define #Default so generated C++/C# types have one consistent value contract.",
                    table.Locate(10, field.ColumnNumber));
            }

            if (field.Type == null)
            {
                continue;
            }

            if ((field.MinimumText != null || field.MaximumText != null) &&
                !field.Type.IsNumeric && field.Type.Kind != EGameDataScalarKind.String)
            {
                diagnostics.Add("GD0314", "#Min/#Max are only valid for numeric values or string lengths.", field.Location);
            }

            if (field.MinimumText != null && (!double.TryParse(field.MinimumText, NumberStyles.Float, CultureInfo.InvariantCulture, out double finiteMinimum) || !double.IsFinite(finiteMinimum)))
            {
                diagnostics.Add("GD0315", $"Invalid #Min value '{field.MinimumText}'.", table.Locate(8, field.ColumnNumber));
            }
            if (field.MaximumText != null && (!double.TryParse(field.MaximumText, NumberStyles.Float, CultureInfo.InvariantCulture, out double finiteMaximum) || !double.IsFinite(finiteMaximum)))
            {
                diagnostics.Add("GD0316", $"Invalid #Max value '{field.MaximumText}'.", table.Locate(9, field.ColumnNumber));
            }
            if (field.MinimumText != null && field.MaximumText != null &&
                double.TryParse(field.MinimumText, NumberStyles.Float, CultureInfo.InvariantCulture, out double minimum) &&
                double.TryParse(field.MaximumText, NumberStyles.Float, CultureInfo.InvariantCulture, out double maximum) &&
                minimum > maximum)
            {
                diagnostics.Add("GD0317", "#Min cannot be greater than #Max.", table.Locate(9, field.ColumnNumber));
            }

            if (field.DefaultText != null)
            {
                if (!FWorkbookParser.TryParseScalar(field.Type, field.DefaultText, out object? defaultValue))
                {
                    diagnostics.Add(
                        "GD0318",
                        $"Default value '{field.DefaultText}' is invalid for type '{field.Type.SourceName}'.",
                        table.Locate(10, field.ColumnNumber));
                }
                else if (defaultValue != null)
                {
                    ValidateDefaultValue(table, field, defaultValue, diagnostics);
                }
            }

            if (field.Type.Kind != EGameDataScalarKind.Enum)
            {
                foreach (SEnumValue allowedValue in field.AllowedValues)
                {
                    if (!FWorkbookParser.TryParseScalar(field.Type, allowedValue.Name, out _))
                    {
                        diagnostics.Add(
                            "GD0322",
                            $"#Allowed value '{allowedValue.Name}' is invalid for type '{field.Type.SourceName}'.",
                            table.Locate(11, field.ColumnNumber));
                    }
                    if (allowedValue.NumericValue.HasValue)
                    {
                        diagnostics.Add(
                            "GD0322",
                            "Numeric assignments in #Allowed are only valid for enum fields.",
                            table.Locate(11, field.ColumnNumber));
                    }
                }
            }

            // Enum values and underlying types are defined once in the global Enums table.
            // Cross-table references and row values are validated after all tables are parsed.
        }
    }

    private static void ValidateDefaultValue(
        FGameDataTable table,
        FGameDataField field,
        object defaultValue,
        FDiagnosticBag diagnostics)
    {
        if (field.AllowedValues.Count != 0 && !IsAllowedValue(field, defaultValue))
        {
            diagnostics.Add(
                "GD0322",
                $"Default value '{field.DefaultText}' is not included in #Allowed.",
                table.Locate(10, field.ColumnNumber));
        }

        if (field.Type == null || (!field.Type.IsNumeric && field.Type.Kind != EGameDataScalarKind.String))
        {
            return;
        }

        if (FGameDataIntegerRange.IsInteger(field.Type))
        {
            FGameDataIntegerRange.Validate(field, defaultValue, table.Locate(10, field.ColumnNumber), diagnostics, "GD0323", "GD0323");
            return;
        }
        double measuredValue = field.Type.Kind == EGameDataScalarKind.String
            ? ((string)defaultValue).Length
            : Convert.ToDouble(defaultValue, CultureInfo.InvariantCulture);
        if (field.MinimumText != null &&
            double.TryParse(field.MinimumText, NumberStyles.Float, CultureInfo.InvariantCulture, out double minimum) &&
            measuredValue < minimum)
        {
            diagnostics.Add(
                "GD0323",
                $"Default value '{field.DefaultText}' is below #Min '{field.MinimumText}'.",
                table.Locate(10, field.ColumnNumber));
        }
        if (field.MaximumText != null &&
            double.TryParse(field.MaximumText, NumberStyles.Float, CultureInfo.InvariantCulture, out double maximum) &&
            measuredValue > maximum)
        {
            diagnostics.Add(
                "GD0323",
                $"Default value '{field.DefaultText}' exceeds #Max '{field.MaximumText}'.",
                table.Locate(10, field.ColumnNumber));
        }
    }

    private static bool IsAllowedValue(FGameDataField field, object value)
    {
        if (field.Type == null)
        {
            return false;
        }

        string canonicalValue = FGameDataText.FormatScalar(value, field.Type);
        foreach (SEnumValue allowedValue in field.AllowedValues)
        {
            if (FWorkbookParser.TryParseScalar(field.Type, allowedValue.Name, out object? parsedAllowed) &&
                parsedAllowed != null &&
                string.Equals(canonicalValue, FGameDataText.FormatScalar(parsedAllowed, field.Type), StringComparison.Ordinal))
            {
                return true;
            }
        }
        return false;
    }

    private static void ValidateDuplicateKeys(FGameDataTable table, FDiagnosticBag diagnostics)
    {
        FGameDataField? primaryKey = table.PrimaryKey;
        if (primaryKey?.Type == null)
        {
            return;
        }

        var firstRows = new Dictionary<string, int>(StringComparer.Ordinal);
        foreach (FGameDataRow row in table.Rows)
        {
            row.Values.TryGetValue(primaryKey.Name, out object? keyValue);
            if (keyValue == null)
            {
                continue;
            }

            if (FGameDataIntegerRange.IsInteger(primaryKey.Type) &&
                Convert.ToDecimal(keyValue, CultureInfo.InvariantCulture) <= 0)
                diagnostics.Add("GD0331", "Primary key must be a positive integer; zero is reserved for absent optional references.",
                    table.Locate(row.SourceRowNumber, primaryKey.ColumnNumber));
            string canonicalKey = FGameDataText.FormatScalar(keyValue, primaryKey.Type!);
            if (firstRows.TryGetValue(canonicalKey, out int firstRow))
            {
                diagnostics.Add(
                    "GD0330",
                    $"Duplicate primary key '{canonicalKey}'. The first occurrence is row {firstRow}.",
                    table.Locate(row.SourceRowNumber, primaryKey.ColumnNumber));
            }
            else
            {
                firstRows.Add(canonicalKey, row.SourceRowNumber);
            }
        }
    }

    private static void ValidateReferences(IReadOnlyList<FGameDataTable> tables, FDiagnosticBag diagnostics)
    {
        Dictionary<string, FGameDataTable> tableByName = tables
            .GroupBy(table => table.Name, StringComparer.Ordinal)
            .Where(group => group.Count() == 1)
            .ToDictionary(group => group.Key, group => group.Single(), StringComparer.Ordinal);

        foreach (FGameDataTable table in tables)
        {
            foreach (FGameDataField field in table.Fields.Where(field => !string.IsNullOrWhiteSpace(field.Reference)))
            {
                string[] parts = field.Reference!.Split('.', StringSplitOptions.TrimEntries);
                if (parts.Length != 2 || !tableByName.TryGetValue(parts[0], out FGameDataTable? referencedTable))
                {
                    diagnostics.Add("GD0340", $"Reference '{field.Reference}' must identify an existing Table.Field.", table.Locate(12, field.ColumnNumber));
                    continue;
                }

                FGameDataField? referencedField = referencedTable.Fields.FirstOrDefault(candidate => candidate.Name == parts[1]);
                if (referencedField == null)
                {
                    diagnostics.Add("GD0341", $"Referenced field '{field.Reference}' does not exist.", table.Locate(12, field.ColumnNumber));
                    continue;
                }
                if (!referencedField.IsPrimaryKey)
                {
                    diagnostics.Add("GD0342", $"Referenced field '{field.Reference}' must be the target table's primary key.", table.Locate(12, field.ColumnNumber));
                }
                if (field.Type == null || referencedField.Type == null ||
                    !FGameDataIntegerRange.IsInteger(field.Type) || !FGameDataIntegerRange.IsInteger(referencedField.Type) ||
                    !AreEquivalentTypes(field.Type, referencedField.Type))
                {
                    diagnostics.Add("GD0343", $"Reference type mismatch: '{field.Type?.SourceName}' -> '{referencedField.Type?.SourceName}'.", table.Locate(12, field.ColumnNumber));
                    continue;
                }
                if (field.Type == null || referencedField.Type == null)
                {
                    continue;
                }

                var referencedValues = referencedTable.Rows
                    .Select(row => row.Values.TryGetValue(referencedField.Name, out object? value) ? value : null)
                    .Where(value => value != null)
                    .Select(value => FGameDataText.FormatScalar(value!, referencedField.Type!))
                    .ToHashSet(StringComparer.Ordinal);
                foreach (FGameDataRow row in table.Rows)
                {
                    row.Values.TryGetValue(field.Name, out object? value);
                    if (value == null)
                    {
                        continue;
                    }
                    string canonical = FGameDataText.FormatScalar(value, field.Type!);
                    if (!field.Required && canonical == "0") continue;
                    if (!referencedValues.Contains(canonical))
                    {
                        diagnostics.Add("GD0344", $"Value '{canonical}' does not exist in referenced field '{field.Reference}'.", table.Locate(row.SourceRowNumber, field.ColumnNumber));
                    }
                }
            }
        }
    }

    private static bool AreEquivalentTypes(FGameDataType? left, FGameDataType? right)
    {
        return left != null && right != null &&
            left.Kind == right.Kind &&
            string.Equals(left.EnumName, right.EnumName, StringComparison.Ordinal);
    }
}
