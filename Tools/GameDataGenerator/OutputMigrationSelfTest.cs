internal static class FOutputMigrationSelfTest
{
    public static void Run(string inputRoot, string testRoot)
    {
        string cppRoot = Path.Combine(testRoot, "Migration", "Generated");
        string yamlRoot = Path.Combine(testRoot, "Migration", "Data");
        FGameDataGeneratorOptions defaults = FGameDataGeneratorOptions.Parse([]);
        Require(defaults.OutputRoot.EndsWith(Path.Combine("GameData", "Generated", "GameData"), StringComparison.Ordinal), "default C++ root is not GameData/Generated/GameData");
        Require(defaults.YamlOutputRoot.EndsWith(Path.Combine("GameData", "Data"), StringComparison.Ordinal), "default YAML root is not GameData/Data");
        string[] args = ["--input-root", inputRoot, "--output-root", cppRoot, "--yaml-output-root", yamlRoot];
        string inputBefore = Snapshot(inputRoot);
        FOutputManager.Apply(cppRoot, [
            new SGeneratedFile(".gamedata-output", "GameDataGenerator owned output v1\n"),
            new SGeneratedFile("Cpp/old.g.h", "previous header"),
            new SGeneratedFile("Data/Client/old.yaml", "previous YAML"),
            new SGeneratedFile("Schema/old.schema.yaml", "previous schema")
        ], check: false);
        Directory.CreateDirectory(yamlRoot);
        FOutputManager.Apply(cppRoot, yamlRoot, [
            new SGeneratedFile(".gamedata-output", "GameDataGenerator owned output v1\n"),
            new SGeneratedFile("Cpp/old.g.h", "previous header"),
            new SGeneratedFile("Data/Client/old.yaml", "previous YAML"),
            new SGeneratedFile("Schema/obsolete.schema.yaml", "obsolete route")
        ], check: false);
        Require(!Directory.Exists(Path.Combine(yamlRoot, "Schema")), "obsolete schema route is still published");
        Directory.CreateDirectory(Path.Combine(yamlRoot, "Schema"));
        File.WriteAllText(Path.Combine(yamlRoot, "Schema", "old.schema.yaml"), "previous table schema");
        File.WriteAllText(Path.Combine(yamlRoot, "Schema", "GameDataEnums.schema.yaml"), "previous enum schema");
        string beforeValidate = Snapshot(testRoot);
        Require(Program.Main([.. args, "--validate-only"]) == 0, "validate-only with old schema failed");
        Require(Snapshot(testRoot) == beforeValidate, "validate-only changed old schema or outputs");
        Require(Program.Main(args) == 0, "explicit YAML output override failed");
        Require(File.Exists(Path.Combine(cppRoot, "Localization", "LocalizationData.g.h")), "C++ output moved");
        Require(File.Exists(Path.Combine(yamlRoot, "Client", "Localization.yaml")), "client YAML missing");
        Require(!Directory.Exists(Path.Combine(yamlRoot, "Schema")) &&
            !Directory.GetFiles(yamlRoot, "*.schema.yaml", SearchOption.AllDirectories).Any(), "schema output remains under custom YAML root");
        Require(!Directory.GetFiles(cppRoot, "*.yaml", SearchOption.AllDirectories).Any(), "old YAML duplicates remain");
        Require(File.Exists(Path.Combine(yamlRoot, ".gamedata-output")), "YAML ownership marker missing");
        Require(!Directory.Exists(Path.Combine(cppRoot, "Cpp")), "legacy Cpp child remains");
        Require(Program.Main([.. args, "--check"]) == 0, "two-root check failed");
        Directory.CreateDirectory(Path.Combine(yamlRoot, "Schema"));
        File.WriteAllText(Path.Combine(yamlRoot, "Schema", "Localization.schema.yaml"), "old table schema");
        File.WriteAllText(Path.Combine(yamlRoot, "Schema", "GameDataEnums.schema.yaml"), "old enum schema");
        string beforeSchemaCheck = Snapshot(testRoot);
        Require(Program.Main([.. args, "--check"]) == 1, "check missed extra owned schema files");
        Require(Snapshot(testRoot) == beforeSchemaCheck, "check changed old schema or outputs");
        Require(Program.Main([.. args, "--validate-only"]) == 0, "validate-only rejected old schema files");
        Require(Snapshot(testRoot) == beforeSchemaCheck, "validate-only changed old schema files");
        Require(Program.Main(args) == 0, "owned schema cleanup failed");
        Require(!Directory.Exists(Path.Combine(yamlRoot, "Schema")), "owned schema directory was not removed");
        Require(Program.Main(["--input-root", Path.GetRelativePath(Directory.GetCurrentDirectory(), inputRoot),
            "--output-root", Path.GetRelativePath(Directory.GetCurrentDirectory(), cppRoot),
            "--yaml-output-root", Path.GetRelativePath(Directory.GetCurrentDirectory(), yamlRoot), "--check"]) == 0,
            "relative disjoint roots failed");
        foreach (string driftPath in new[] { Path.Combine(yamlRoot, "Client", "Localization.yaml"), Path.Combine(cppRoot, "Localization", "LocalizationData.g.h") })
        {
            string original = File.ReadAllText(driftPath);
            File.WriteAllText(driftPath, "drift");
            string beforeCheck = Snapshot(testRoot);
            Require(Program.Main([.. args, "--check"]) == 1, "check missed output drift");
            Require(Snapshot(testRoot) == beforeCheck, "check wrote output");
            File.WriteAllText(driftPath, original);
        }
        string absentCpp = Path.Combine(testRoot, "ValidateOnly", "Class");
        string absentYaml = Path.Combine(testRoot, "ValidateOnly", "Data");
        Require(Program.Main(["--input-root", inputRoot, "--output-root", absentCpp, "--yaml-output-root", absentYaml, "--validate-only"]) == 0, "validate-only failed");
        Require(!Directory.Exists(Path.GetDirectoryName(absentCpp)), "validate-only created output parent");
        foreach (var roots in new[] { (cppRoot, cppRoot), (cppRoot, Path.Combine(cppRoot, "nested")), (Path.Combine(yamlRoot, "nested"), yamlRoot), (cppRoot, inputRoot), (inputRoot, yamlRoot) })
        {
            string beforeRejected = Snapshot(testRoot);
            Require(Program.Main(["--input-root", inputRoot, "--output-root", roots.Item1, "--yaml-output-root", roots.Item2]) == 1, "overlapping roots accepted");
            Require(Snapshot(testRoot) == beforeRejected, "overlap rejection changed output or input");
        }
        Require(Program.Main(["--input-root", inputRoot, "--output-root", cppRoot + Path.DirectorySeparatorChar,
            "--yaml-output-root", yamlRoot + Path.DirectorySeparatorChar]) == 0, "trailing separator output roots failed");
        if (OperatingSystem.IsWindows())
        {
            foreach (string alias in new[] { @"\\?\" + cppRoot, @"\\?\" + Path.Combine(cppRoot, "nested"),
                @"\\.\" + cppRoot, @"\\localhost\C$\unsupported-root" })
            {
                string beforeAlias = Snapshot(testRoot);
                Require(Program.Main(["--input-root", inputRoot, "--output-root", cppRoot, "--yaml-output-root", alias]) == 1,
                    "extended physical root alias accepted");
                Require(Snapshot(testRoot) == beforeAlias, "physical alias rejection changed fixtures");
            }
        }
        if (OperatingSystem.IsWindows())
        {
            foreach (string component in new[] { "bad*name", "bad?name", "bad<name", "bad>name", "bad|name", "bad\"name",
                "bad:name", "bad\u0001name", "CON", "con.txt", "PRN", "AUX", "NUL", "COM1", "LPT9.log", "CONIN$", "CONOUT$",
                "COM\u00B9", "LPT\u00B2.txt", "COM\u00B3" })
            {
                string invalidRoot = Path.Combine(testRoot, "AbsentInvalidComponent", component, "child");
                string beforeInvalid = Snapshot(testRoot);
                bool rejected = false;
                try { FOutputManager.ValidateRoots([cppRoot, invalidRoot]); }
                catch (InvalidOperationException) { rejected = true; }
                Require(rejected, "unsupported Windows root component accepted: " + component);
                Require(Program.Main(["--input-root", inputRoot, "--output-root", cppRoot, "--yaml-output-root", invalidRoot]) == 1,
                    "invalid Windows root was not rejected by CLI");
                Require(Snapshot(testRoot) == beforeInvalid, "invalid component rejection changed files or directories");
            }
        }
        if (OperatingSystem.IsWindows())
        {
            var shortPath = new System.Text.StringBuilder(32768);
            uint length = GetShortPathName(cppRoot, shortPath, (uint)shortPath.Capacity);
            string shortRoot = shortPath.ToString().Replace('/', '\\');
            if (length > 0 && length < shortPath.Capacity && !shortRoot.Equals(Path.GetFullPath(cppRoot), StringComparison.OrdinalIgnoreCase))
            {
                foreach (var roots in new[] { new[] { cppRoot, shortRoot },
                    new[] { cppRoot, Path.Combine(shortRoot, "missing", "nested") } })
                {
                    bool rejected = false;
                    try { FOutputManager.ValidateRoots(roots); }
                    catch (InvalidOperationException) { rejected = true; }
                    Require(rejected, "short-name physical root alias accepted");
                }
                Console.WriteLine("Output migration short-name alias cases passed.");
            }
            else Console.WriteLine("Output migration short-name alias cases unavailable (8.3 alias not supplied by filesystem).");
        }
        if (OperatingSystem.IsWindows())
        {
            string[] occupiedDrives = Environment.GetLogicalDrives();
            string drive = Enumerable.Range('D', 'Z' - 'D' + 1).Select(value => ((char)value) + ":")
                .Reverse().First(value => !occupiedDrives.Contains(value + @"\", StringComparer.OrdinalIgnoreCase));
            RunSubst(drive, testRoot);
            try
            {
                Require(File.Exists(Path.Combine(drive + @"\", "Migration", "Generated", ".gamedata-output")), "fixture drive mapping was not verified");
                foreach (string alias in new[] { Path.Combine(drive + @"\", "Migration", "Generated"),
                    Path.Combine(drive + @"\", "Migration", "Generated", "missing", "nested") })
                {
                    bool rejected = false;
                    try { FOutputManager.ValidateRoots([cppRoot, alias]); }
                    catch (InvalidOperationException) { rejected = true; }
                    Require(rejected, "drive-mapped physical root alias accepted");
                }
                bool missingAliasRejected = false;
                try { FOutputManager.ValidateRoots([Path.Combine(cppRoot, "missing"), Path.Combine(drive + @"\", "Migration", "Generated", "missing", "nested")]); }
                catch (InvalidOperationException) { missingAliasRejected = true; }
                Require(missingAliasRejected, "missing suffix physical ancestry accepted");
                FOutputManager.ValidateRoots([cppRoot, Path.Combine(drive + @"\", "ordinary-disjoint", "missing")]);
            }
            finally
            {
                RunSubst(drive, "/D");
                Require(!Environment.GetLogicalDrives().Contains(drive + @"\", StringComparer.OrdinalIgnoreCase), "fixture drive mapping leaked");
            }
        }
        if (OperatingSystem.IsWindows())
        {
            bool unresolvedAliasRejected = false;
            try { FOutputManager.ValidateRoots([Path.Combine(testRoot, "AbsentAlias", "LongDirectoryName"), Path.Combine(testRoot, "AbsentAlias", "LONGDI~1")]); }
            catch (InvalidOperationException) { unresolvedAliasRejected = true; }
            Require(unresolvedAliasRejected, "unresolved prospective short-name alias accepted");
        }
        string blockedYamlRoot = Path.Combine(testRoot, "NonOwned");
        Directory.CreateDirectory(blockedYamlRoot);
        string unrelated = Path.Combine(blockedYamlRoot, "notes.txt");
        File.WriteAllText(unrelated, "unrelated data");
        string preservedHeader = Path.Combine(cppRoot, "Localization", "LocalizationData.g.h");
        File.WriteAllText(preservedHeader, "preserve C++ before validation");
        Require(Program.Main(["--input-root", inputRoot, "--output-root", cppRoot, "--yaml-output-root", blockedYamlRoot]) == 1,
            "non-owned YAML target was accepted");
        Require(File.ReadAllText(preservedHeader) == "preserve C++ before validation", "non-owned YAML target changed C++");
        Require(File.ReadAllText(unrelated) == "unrelated data", "non-owned YAML target lost unrelated data");
#if GAMEDATA_TEST_HOOKS
        string originalHeader = Path.Combine(cppRoot, "Localization", "LocalizationData.g.h");
        File.WriteAllText(originalHeader, "previous C++");
        File.WriteAllText(Path.Combine(yamlRoot, "Client", "Localization.yaml"), "previous YAML");
        Directory.CreateDirectory(Path.Combine(yamlRoot, "Schema"));
        File.WriteAllText(Path.Combine(yamlRoot, "Schema", "Localization.schema.yaml"), "preserve old table schema");
        File.WriteAllText(Path.Combine(yamlRoot, "Schema", "GameDataEnums.schema.yaml"), "preserve old enum schema");
        string beforeRollback = Snapshot(testRoot);
        int publishCount = 0;
        bool sawMoveFailure = false;
        FOutputManager.BeforePublishForTest = () =>
        {
            if (++publishCount == 1)
                Require(Directory.GetDirectories(Path.GetDirectoryName(yamlRoot)!, ".Data.staging.*")
                    .Any(stage => File.Exists(Path.Combine(stage, "Client", "Localization.yaml"))), "both outputs were not staged before first publication");
            if (publishCount == 2) File.WriteAllText(yamlRoot, "block second publish");
        };
        FOutputManager.MoveFailedForTest = (operation, _) =>
        {
            if (operation == "publish-move" && File.Exists(yamlRoot))
            {
                sawMoveFailure = true;
                File.Delete(yamlRoot);
            }
        };
        try
        {
            Require(Program.Main(args) == 1, "second-root failure succeeded");
            Require(sawMoveFailure, "second publish did not exercise Directory.Move failure");
            Require(Snapshot(testRoot) == beforeRollback, "second-root failure changed original input or old output tree including schemas");
            Require(File.ReadAllText(originalHeader) == "previous C++", "second-root failure did not roll back C++");
            Require(File.ReadAllText(Path.Combine(yamlRoot, "Client", "Localization.yaml")) == "previous YAML", "second-root failure did not roll back YAML");
        }
        finally
        {
            FOutputManager.BeforePublishForTest = null;
            FOutputManager.MoveFailedForTest = null;
        }
#endif
        if (OperatingSystem.IsWindows())
        {
            string readOnlyFile = Path.Combine(cppRoot, "Localization", "LocalizationData.g.h");
            File.SetAttributes(readOnlyFile, File.GetAttributes(readOnlyFile) | FileAttributes.ReadOnly);
            TextWriter previousError = Console.Error;
            using var capturedError = new StringWriter();
            try
            {
                Console.SetError(capturedError);
                Require(Program.Main(args) == 0, "committed generation reported failure after backup cleanup denial");
                Require(File.ReadAllText(readOnlyFile) != "previous C++", "committed C++ output missing");
                Require(File.ReadAllText(Path.Combine(yamlRoot, "Client", "Localization.yaml")) != "previous YAML", "committed YAML output missing");
                string[] backups = Directory.GetDirectories(Path.GetDirectoryName(cppRoot)!, ".*.backup.*");
                Require(backups.Length > 0, "read-only fixture did not leave a cleanup remnant");
                var records = capturedError.ToString().Split('\n', StringSplitOptions.RemoveEmptyEntries)
                    .Select(line => System.Text.Json.JsonDocument.Parse(line)).ToArray();
                try
                {
                    var cleanup = records.Single(record => record.RootElement.GetProperty("operation").GetString() == "backup-cleanup").RootElement;
                    Require(cleanup.GetProperty("transactionState").GetString() == "committed", "cleanup diagnostic omitted committed state");
                    string[] reported = cleanup.GetProperty("remainingBackupPaths").EnumerateArray().Select(value => value.GetString()!).ToArray();
                    Require(reported.Order().SequenceEqual(backups.Order()), "cleanup diagnostic does not match remaining backup paths");
                    Require(backups.Contains(cleanup.GetProperty("path").GetString()), "cleanup diagnostic omitted failed backup path");
                    Require(cleanup.GetProperty("backupContentsMayBePartial").GetBoolean(), "cleanup diagnostic promised intact old backups");
                    Require(Program.Main([.. args, "--check"]) == 0, "committed generated output failed content check");
                    previousError.Write(capturedError.ToString());
                }
                finally { foreach (var record in records) record.Dispose(); }
            }
            finally
            {
                Console.SetError(previousError);
                foreach (string backup in Directory.GetDirectories(Path.GetDirectoryName(cppRoot)!, ".*.backup.*"))
                {
                    foreach (string file in Directory.GetFiles(backup, "*", SearchOption.AllDirectories))
                        File.SetAttributes(file, File.GetAttributes(file) & ~FileAttributes.ReadOnly);
                    Directory.Delete(backup, recursive: true);
                }
            }
        }
        Require(Snapshot(inputRoot) == inputBefore, "original workbooks were changed");
        Require(!Directory.GetDirectories(Path.GetDirectoryName(cppRoot)!, ".*.staging.*").Any(), "staging directories leaked");
        Require(!Directory.GetDirectories(Path.GetDirectoryName(cppRoot)!, ".*.backup.*").Any(), "backup directories leaked");
    }

    private static void RunSubst(string drive, string target)
    {
        var start = new System.Diagnostics.ProcessStartInfo("subst.exe") { UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true };
        start.ArgumentList.Add(drive);
        start.ArgumentList.Add(target);
        using var process = System.Diagnostics.Process.Start(start)!;
        string diagnostic = process.StandardOutput.ReadToEnd() + process.StandardError.ReadToEnd();
        process.WaitForExit();
        Require(process.ExitCode == 0, "fixture drive mapping failed: " + diagnostic);
    }

    [System.Runtime.InteropServices.DllImport("kernel32.dll", EntryPoint = "GetShortPathNameW", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
    private static extern uint GetShortPathName(string path, System.Text.StringBuilder shortPath, uint capacity);

    private static string Snapshot(string root) => string.Join("\n", Directory.GetFileSystemEntries(root, "*", SearchOption.AllDirectories)
        .OrderBy(path => path, StringComparer.Ordinal)
        .Select(path => Directory.Exists(path)
            ? "D:" + Path.GetRelativePath(root, path)
            : "F:" + Path.GetRelativePath(root, path) + ":" + File.GetLastWriteTimeUtc(path).Ticks + ":" + Convert.ToBase64String(File.ReadAllBytes(path))));

    private static void Require(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException("Output migration self-test: " + message);
    }
}
