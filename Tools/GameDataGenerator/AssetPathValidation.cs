internal static class FAssetPathValidation
{
    public static string? Resolve(FGameDataTable table, FGameDataField field, FGameDataRow row, FDiagnosticBag diagnostics)
    {
        string assetPath = row.Values.TryGetValue(field.Name, out object? value) ? value as string ?? string.Empty : string.Empty;
        SSourceLocation location = table.Locate(row.SourceRowNumber, field.ColumnNumber);
        if (string.IsNullOrWhiteSpace(assetPath))
        {
            if (field.Required) diagnostics.Add("GD2114", "AssetPath must be nonempty.", location);
            return null;
        }
        if (Path.IsPathRooted(assetPath) || assetPath.StartsWith('\\') || assetPath.StartsWith('/'))
        {
            diagnostics.Add("GD2111", $"AssetPath must be relative to the GameData root: '{assetPath}'.", location);
            return null;
        }
        if (assetPath.Any(character => character < 0x20 || ":<>\"|?*".Contains(character)))
        {
            diagnostics.Add("GD2114", $"AssetPath contains an invalid character or alternate-data-stream colon: '{assetPath}'.", location);
            return null;
        }
        string[] segments = assetPath.Split(['/', '\\']);
        if (segments.Contains("..", StringComparer.Ordinal))
        {
            diagnostics.Add("GD2111", $"AssetPath must not traverse parent directories: '{assetPath}'.", location);
            return null;
        }
        if (segments.Any(segment => segment.Length == 0 || segment.EndsWith(' ') || segment.EndsWith('.') ||
            System.Text.RegularExpressions.Regex.IsMatch(segment.Split('.')[0].TrimEnd(), @"\A(CON|PRN|AUX|NUL|CONIN\$|CONOUT\$|COM[1-9¹²³]|LPT[1-9¹²³])\z", System.Text.RegularExpressions.RegexOptions.IgnoreCase | System.Text.RegularExpressions.RegexOptions.CultureInvariant)))
        {
            diagnostics.Add("GD2114", $"AssetPath contains an ambiguous or reserved Windows path component: '{assetPath}'.", location);
            return null;
        }
        if (table.DataRoot == null)
        {
            diagnostics.Add("GD2112", "The parser's explicit input root is required for AssetPath validation.", location);
            return null;
        }
        try
        {
            string root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(table.DataRoot));
            string candidate = Path.GetFullPath(Path.Combine(root, assetPath.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar)));
            if (!candidate.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase))
            {
                diagnostics.Add("GD2111", $"AssetPath '{assetPath}' escapes GameData root '{root}': '{candidate}'.", location);
                return null;
            }
            for (string? component = candidate; component != null; component = Path.GetDirectoryName(component))
            {
                try
                {
                    if ((File.GetAttributes(component) & FileAttributes.ReparsePoint) != 0)
                    {
                        diagnostics.Add("GD2115", $"AssetPath '{assetPath}' resolved to '{candidate}' contains reparse/symlink component '{component}'.", location);
                        return null;
                    }
                }
                catch (FileNotFoundException)
                {
                    // 미존재 leaf도 상위 reparse 검사를 계속한 뒤 파일 누락으로 진단한다.
                }
                catch (DirectoryNotFoundException)
                {
                    // 미존재 중간 경로도 기존 상위 구성요소 검사를 생략하지 않는다.
                }
            }
            if (!File.Exists(candidate))
            {
                diagnostics.Add("GD2113", $"AssetPath file does not exist: '{assetPath}' resolved to '{candidate}'.", location);
                return null;
            }
            return candidate;
        }
        catch (Exception exception) when (exception is ArgumentException or NotSupportedException or IOException or UnauthorizedAccessException)
        {
            diagnostics.Add("GD2114", $"AssetPath '{assetPath}' is invalid: {exception.Message}", location);
            return null;
        }
    }
}
