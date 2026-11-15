using System.Globalization;

internal static class FGameDataIntegerRange
{
    public static bool IsInteger(FGameDataType type) => type.Kind is EGameDataScalarKind.Int32 or EGameDataScalarKind.UInt32
        or EGameDataScalarKind.Int64 or EGameDataScalarKind.UInt64;

    public static void Validate(FGameDataField field, object value, SSourceLocation location, FDiagnosticBag diagnostics,
        string minimumCode, string maximumCode)
    {
        decimal measured = Convert.ToDecimal(value, CultureInfo.InvariantCulture);
        if (field.MinimumText != null && Compare(measured, field.MinimumText) < 0)
            diagnostics.Add(minimumCode, $"Value {measured} is below minimum {field.MinimumText}.", location);
        if (field.MaximumText != null && Compare(measured, field.MaximumText) > 0)
            diagnostics.Add(maximumCode, $"Value {measured} exceeds maximum {field.MaximumText}.", location);
    }

    private static int Compare(decimal value, string bound)
    {
        if (decimal.TryParse(bound, NumberStyles.Float, CultureInfo.InvariantCulture, out decimal exact))
            return value.CompareTo(exact);
        if (double.TryParse(bound, NumberStyles.Float, CultureInfo.InvariantCulture, out double wide) && double.IsFinite(wide))
            return ((double)value).CompareTo(wide);
        return 0; // 잘못된 메타데이터 자체는 필드 정의 검증이 거부한다.
    }
}
