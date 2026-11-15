using System.Text;

internal static class FTextArtValidation
{
    private static bool IsSupportedGlyph(int value) =>
        value is >= 0x20 and <= 0x7E or >= 0x2500 and <= 0x259F or >= 0x2190 and <= 0x2193 or
            0x25B2 or 0x25B6 or 0x25BC or 0x25C0 or 0x25CB;

    public static void Validate(FGameDataTable table, FDiagnosticBag diagnostics)
    {
        FGameDataField widthField = table.Fields.First(field => field.Name == "Width");
        foreach (FGameDataRow row in table.Rows)
        {
            uint width = row.Values.TryGetValue("Width", out object? rawWidth) && rawWidth is uint w ? w : 0;
            uint height = row.Values.TryGetValue("Height", out object? rawHeight) && rawHeight is uint h ? h : 0;
            if (!((width == 36 && height == 36) || (width == 83 && height == 41)))
            {
                diagnostics.Add("GD2140", "TextArt dimensions must be exactly 36x36 or 83x41.", table.Locate(row.SourceRowNumber, widthField.ColumnNumber));
                continue;
            }
            FGameDataField pathField = table.Fields.First(field => field.Name == "AssetPath");
            string? path = FAssetPathValidation.Resolve(table, pathField, row, diagnostics);
            if (path == null) continue;
            try
            {
                string text = new UTF8Encoding(false, true).GetString(File.ReadAllBytes(path));
                if (text.StartsWith('\uFEFF')) text = text[1..];
                string[] lines = text.Replace("\r\n", "\n", StringComparison.Ordinal).Split('\n');
                if (lines.Length > 0 && lines[^1].Length == 0) lines = lines[..^1];
                if (lines.Length != height)
                    diagnostics.Add("GD2142", $"TextArt '{path}' requires exactly {height} rows, but found {lines.Length}.", table.Locate(row.SourceRowNumber, pathField.ColumnNumber));
                for (int index = 0; index < lines.Length; index++)
                {
                    int column = 0;
                    foreach (Rune rune in lines[index].EnumerateRunes())
                    {
                        column++;
                        if (!IsSupportedGlyph(rune.Value))
                            diagnostics.Add("GD2143", $"TextArt '{path}' row {index + 1}, cell {column} contains unsupported glyph U+{rune.Value:X4}.", table.Locate(row.SourceRowNumber, pathField.ColumnNumber));
                    }
                    if (column != width)
                        diagnostics.Add("GD2142", $"TextArt '{path}' row {index + 1} requires exactly {width} cells, but found {column}; trailing spaces are significant.", table.Locate(row.SourceRowNumber, pathField.ColumnNumber));
                }
            }
            catch (DecoderFallbackException exception)
            {
                diagnostics.Add("GD2141", $"TextArt '{path}' must be strict UTF-8: {exception.Message}", table.Locate(row.SourceRowNumber, pathField.ColumnNumber));
            }
            catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
            {
                diagnostics.Add("GD2114", $"Could not read TextArt '{path}': {exception.Message}", table.Locate(row.SourceRowNumber, pathField.ColumnNumber));
            }
        }
    }
}
