internal static partial class FGameDataValidator
{
    // 생성 코드/Windows 경로로 표현할 수 없는 입력은 staging을 만들기 전에 거부한다.
    private static void ValidatePortableRepresentation(IReadOnlyList<FGameDataTable> tables, FDiagnosticBag diagnostics)
    {
        string[] devices = ["CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
            "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9", "Common"];
        foreach (FGameDataTable table in tables)
        {
            if (table.IsIncludedOnServer && table.Name == "TGameDataRow")
                diagnostics.Add("GD0353", "Table namespace conflicts with the C++ runtime template GameData::TGameDataRow.", table.Locate(1, 2));
            if (devices.Contains(table.Name, StringComparer.OrdinalIgnoreCase))
                diagnostics.Add("GD0350", "Table name conflicts with a reserved Windows/generated directory.", table.Locate(1, 2));
            string rowName = table.Name == "Item" ? "ItemData" : table.Name + "Data";
            foreach (FGameDataField field in table.Fields)
            {
                if (FGameDataNaming.ToPascalCase(field.Name) == rowName)
                    diagnostics.Add("GD0351", "Property name conflicts with its generated enclosing type.", field.Location);
                if (HasUnsupportedControl(field.DefaultText))
                    diagnostics.Add("GD0352", "Unsupported control character in default.", table.Locate(10, field.ColumnNumber));
                foreach (FGameDataRow row in table.Rows)
                    if (row.Values.TryGetValue(field.Name, out object? value) && value is string text && HasUnsupportedControl(text))
                        diagnostics.Add("GD0352", "Unsupported control character in value.", table.Locate(row.SourceRowNumber, field.ColumnNumber));
            }
        }
    }

    private static bool HasUnsupportedControl(string? text) => text?.Any(character =>
        char.IsControl(character) && character is not ('\n' or '\r' or '\t')) == true;
}
