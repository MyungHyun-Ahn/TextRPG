using YamlDotNet.Core;
using YamlDotNet.Core.Events;

// 역직렬화 바인더를 사용하지 않고 제한된 YAML 노드만 읽는다.
internal sealed record FInGameYamlNode(Mark Mark, string? Scalar = null,
    IReadOnlyList<FInGameYamlNode>? Items = null,
    IReadOnlyDictionary<string, FInGameYamlNode>? Members = null, bool Plain = false);

internal static class FInGameYaml
{
    internal static InvalidOperationException Error(string file, Mark mark, string message) =>
        new($"{file}({mark.Line},{mark.Column}): IGD1001: {message}");

    public static FInGameYamlNode Read(string file)
    {
        if (new FileInfo(file).Length > 1024 * 1024)
            throw Error(file, Mark.Empty, "Schema exceeds 1 MiB.");
        using var input = new StreamReader(file, new System.Text.UTF8Encoding(false, true), detectEncodingFromByteOrderMarks: false);
        var parser = new Parser(input);
        int count = 0;
        try
        {
            parser.Consume<StreamStart>();
            parser.Consume<DocumentStart>();
            FInGameYamlNode root = ReadNode(0);
            parser.Consume<DocumentEnd>();
            if (!parser.Accept<StreamEnd>(out _))
                throw Error(file, parser.Current!.Start, "Exactly one YAML document is required.");
            return root;
        }
        catch (System.Text.DecoderFallbackException)
        {
            throw Error(file, Mark.Empty, "Invalid UTF-8 schema input.");
        }
        catch (YamlException exception)
        {
            throw Error(file, exception.Start, "Invalid YAML: " + exception.Message);
        }

        FInGameYamlNode ReadNode(int depth)
        {
            if (++count > 20000 || depth > 64)
                throw Error(file, parser.Current!.Start, "YAML node/depth limit exceeded.");
            ParsingEvent current = parser.Current!;
            if (current is AnchorAlias)
                throw Error(file, current.Start, "YAML aliases are unsupported.");
            if (current is NodeEvent node && (!node.Anchor.IsEmpty || !node.Tag.IsEmpty))
                throw Error(file, current.Start, "YAML anchors and explicit tags are unsupported.");
            if (parser.TryConsume<Scalar>(out var scalar))
            {
                if (scalar.Value.Length > 4096) throw Error(file, scalar.Start, "Scalar length limit exceeded.");
                return new(scalar.Start, Scalar: scalar.Value, Plain: scalar.Style == ScalarStyle.Plain);
            }
            if (parser.TryConsume<SequenceStart>(out var sequence))
            {
                var items = new List<FInGameYamlNode>();
                while (!parser.TryConsume<SequenceEnd>(out _)) items.Add(ReadNode(depth + 1));
                return new(sequence.Start, Items: items);
            }
            if (parser.TryConsume<MappingStart>(out var mapping))
            {
                var members = new Dictionary<string, FInGameYamlNode>(StringComparer.Ordinal);
                while (!parser.TryConsume<MappingEnd>(out _))
                {
                    FInGameYamlNode key = ReadNode(depth + 1);
                    if (key.Scalar == null || key.Scalar.Length == 0)
                        throw Error(file, key.Mark, "Mapping keys must be nonempty scalars.");
                    if (members.ContainsKey(key.Scalar)) throw Error(file, key.Mark, "Duplicate YAML key: " + key.Scalar);
                    members.Add(key.Scalar, ReadNode(depth + 1));
                }
                return new(mapping.Start, Members: members);
            }
            throw Error(file, current.Start, "Expected YAML scalar, sequence or mapping.");
        }
    }
}
