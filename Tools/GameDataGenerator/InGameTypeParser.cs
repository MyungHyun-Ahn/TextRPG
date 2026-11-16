using System.Text.RegularExpressions;
using YamlDotNet.Core;

internal static class FInGameTypeParser
{
    internal static readonly HashSet<string> Primitives = new(StringComparer.Ordinal)
        { "bool", "int32", "uint32", "int64", "uint64", "float", "double", "string" };
    internal static readonly HashSet<string> Templates = new(StringComparer.Ordinal)
        { "vector", "map", "unordered_map", "unique_ptr", "shared_ptr" };

    public static FInGameType Parse(string source, string file, Mark mark)
    {
        int position = 0;
        FInGameType result = Read(0);
        Space();
        if (position != source.Length) Fail("Unexpected type token; raw pointers/references and arbitrary C++ are unsupported.");
        return result;

        void Space() { while (position < source.Length && char.IsWhiteSpace(source[position])) ++position; }
        void Fail(string message) => throw FInGameYaml.Error(file, mark, $"Type offset {position + 1}: {message}");
        FInGameType Read(int depth)
        {
            if (depth > 32) Fail("Type nesting limit exceeded.");
            Space();
            int start = position;
            while (position < source.Length && (char.IsAsciiLetterOrDigit(source[position]) || source[position] == '_')) ++position;
            string name = source[start..position];
            if (name.Length == 0 || !char.IsAsciiLetter(name[0])) Fail("Expected a type identifier.");
            if (name is "weak_ptr" or "raw_ptr" or "alias")
                Fail("Unsupported type: " + name + ".");
            Space();
            var arguments = new List<FInGameType>();
            if (position < source.Length && source[position] == '<')
            {
                ++position;
                arguments.Add(Read(depth + 1));
                Space();
                while (position < source.Length && source[position] == ',')
                {
                    ++position;
                    arguments.Add(Read(depth + 1));
                    if (arguments.Count > 2) Fail("Template arity exceeds two.");
                    Space();
                }
                if (position >= source.Length || source[position] != '>') Fail("Expected closing >.");
                ++position;
            }
            int arity = name is "map" or "unordered_map" ? 2 : name == "enum" || Templates.Contains(name) ? 1 : 0;
            if (arguments.Count != arity) Fail("Invalid template arity for " + name + ".");
            if (name == "enum" && (arguments[0].Arguments.Count != 0 || !IsName(arguments[0].Name)))
                Fail("Enum reference requires one catalog identifier.");
            return new(name, arguments);
        }
    }

    internal static bool IsName(string name) => name.Length <= 128 &&
        Regex.IsMatch(name, "^[A-Za-z][A-Za-z0-9_]*$", RegexOptions.CultureInvariant) &&
        !name.Contains("__", StringComparison.Ordinal) && FGameDataNaming.IsCppIdentifier(name);

    internal static IEnumerable<string> ValueDependencies(FInGameType type)
    {
        if (type.Name is "shared_ptr" or "unique_ptr" or "enum") yield break;
        if (!Primitives.Contains(type.Name) && !Templates.Contains(type.Name)) yield return type.Name;
        foreach (var argument in type.Arguments)
            foreach (string name in ValueDependencies(argument)) yield return name;
    }
}
