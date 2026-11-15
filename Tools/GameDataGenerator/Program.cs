using System.Text;

internal static class Program
{
    public static int Main(string[] args)
    {
        Console.OutputEncoding = new UTF8Encoding(false);
        try
        {
            FGameDataGeneratorOptions options = FGameDataGeneratorOptions.Parse(args);
            if (options.SelfTest)
            {
                FGameDataGeneratorSelfTest.Run();
                Console.WriteLine("GameDataGenerator self-test passed.");
                return 0;
            }

            return Run(options);
        }
        catch (Exception exception)
        {
            FOutputManager.ReportFailure("cli", Directory.GetCurrentDirectory(), null, exception);
            try
            {
                Console.Error.WriteLine($"GameDataGenerator failed: {exception.Message}");
            }
            catch (Exception)
            {
                // 진단 sink 실패도 원래 실패의 종료 코드 계약을 바꾸지 않는다.
                return 1;
            }
            return 1;
        }
    }

    private static int Run(FGameDataGeneratorOptions options)
    {
        ValidateOutputBoundary(options);
        if (!Directory.Exists(options.InputRoot))
        {
            throw new InvalidOperationException($"Input root not found: {options.InputRoot}");
        }

        var diagnostics = new FDiagnosticBag();
        IReadOnlyList<FGameDataTable> tables = FWorkbookParser.ParseDirectory(options.InputRoot, diagnostics);
        IReadOnlyDictionary<string, FGameDataEnumDefinition> enumDefinitions =
            FGameDataValidator.Validate(tables, diagnostics);
        if (diagnostics.HasErrors)
        {
            PrintDiagnostics(diagnostics);
            Console.Error.WriteLine($"Validation failed with {diagnostics.Count} error(s). No generated files were changed.");
            return 1;
        }

        Console.WriteLine($"Validated {tables.Count} table(s) from {options.InputRoot}.");
        if (options.ValidateOnly)
        {
            Console.WriteLine("Validation completed. Output generation was skipped (--validate-only).");
            return 0;
        }

        IReadOnlyList<SGeneratedFile> files = FGameDataGenerators.Generate(tables, enumDefinitions);
        FOutputManager.Apply(options.OutputRoot, options.YamlOutputRoot, files, options.Check);
        Console.WriteLine(options.Check
            ? $"Game-data generation check passed: {files.Count} file(s)."
            : $"Generated C++ under {options.OutputRoot} and YAML under {options.YamlOutputRoot} in one transaction.");
        return 0;
    }

    private static void ValidateOutputBoundary(FGameDataGeneratorOptions options)
    {
        FOutputManager.ValidateRoots([options.InputRoot, options.OutputRoot, options.YamlOutputRoot]);
    }

    private static void PrintDiagnostics(FDiagnosticBag diagnostics)
    {
        foreach (SGameDataDiagnostic diagnostic in diagnostics.GetSorted())
        {
            Console.Error.WriteLine(diagnostic);
        }
    }
}

internal sealed class FGameDataGeneratorOptions
{
    public required string InputRoot { get; init; }

    public required string OutputRoot { get; init; }

    public required string YamlOutputRoot { get; init; }

    public required bool Check { get; init; }

    public required bool ValidateOnly { get; init; }

    public required bool SelfTest { get; init; }

    public static FGameDataGeneratorOptions Parse(string[] args)
    {
        string solutionRoot = FindSolutionRoot();
        string inputRoot = Path.Combine(solutionRoot, "GameData", "Excel");
        string outputRoot = Path.Combine(solutionRoot, "GameData", "Class");
        string yamlOutputRoot = Path.Combine(solutionRoot, "GameData", "Data");
        bool check = false;
        bool validateOnly = false;
        bool selfTest = false;

        for (int index = 0; index < args.Length; ++index)
        {
            switch (args[index])
            {
                case "--input-root":
                    inputRoot = RequirePathArgument(args, ref index, "--input-root");
                    break;
                case "--output-root":
                    outputRoot = RequirePathArgument(args, ref index, "--output-root");
                    break;
                case "--yaml-output-root":
                    yamlOutputRoot = RequirePathArgument(args, ref index, "--yaml-output-root");
                    break;
                case "--check":
                    check = true;
                    break;
                case "--validate-only":
                    validateOnly = true;
                    break;
                case "--self-test":
                    selfTest = true;
                    break;
                default:
                    throw new InvalidOperationException($"Unknown argument: {args[index]}");
            }
        }

        if (check && validateOnly)
        {
            throw new InvalidOperationException("--check and --validate-only cannot be used together.");
        }
        if (selfTest && args.Length != 1)
        {
            throw new InvalidOperationException("--self-test cannot be combined with other arguments.");
        }

        return new FGameDataGeneratorOptions
        {
            InputRoot = Path.GetFullPath(inputRoot),
            OutputRoot = Path.GetFullPath(outputRoot),
            YamlOutputRoot = Path.GetFullPath(yamlOutputRoot),
            Check = check,
            ValidateOnly = validateOnly,
            SelfTest = selfTest,
        };
    }

    private static string RequirePathArgument(string[] args, ref int index, string argument)
    {
        if (index + 1 >= args.Length || args[index + 1].StartsWith("--", StringComparison.Ordinal))
        {
            throw new InvalidOperationException($"Missing value for {argument}.");
        }

        return args[++index];
    }

    private static string FindSolutionRoot()
    {
        DirectoryInfo? directory = new(AppContext.BaseDirectory);
        while (directory != null)
        {
            if (Directory.Exists(Path.Combine(directory.FullName, "Tools", "GameDataGenerator")))
            {
                return directory.FullName;
            }
            directory = directory.Parent;
        }

        directory = new DirectoryInfo(Directory.GetCurrentDirectory());
        while (directory != null)
        {
            if (Directory.Exists(Path.Combine(directory.FullName, "Tools", "GameDataGenerator")))
            {
                return directory.FullName;
            }
            directory = directory.Parent;
        }

        return Directory.GetCurrentDirectory();
    }
}
