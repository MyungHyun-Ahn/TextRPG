using System.Text;

internal static class FOutputManager
{
    private static readonly UTF8Encoding Utf8WithoutBom = new(false);
#if GAMEDATA_TEST_HOOKS
    internal static Action? BeforePublishForTest;
    internal static Action<string, Exception>? MoveFailedForTest;
#endif

    public static void Apply(string outputRoot, string yamlOutputRoot, IReadOnlyList<SGeneratedFile> files, bool check)
    {
        SGeneratedFile marker = files.Single(file => file.RelativePath == ".gamedata-output");
        ApplyTransaction([
            (outputRoot, files.Where(file => file.RelativePath.StartsWith("Cpp/", StringComparison.Ordinal)).Select(file => file with { RelativePath = file.RelativePath[4..] }).Append(marker).ToArray()),
            (yamlOutputRoot, files.Where(file => file.RelativePath.StartsWith("Data/Client/", StringComparison.Ordinal))
                .Select(file => file with { RelativePath = file.RelativePath[5..] }).Append(marker).ToArray())
        ], check);
    }

    public static void Apply(string outputRoot, IReadOnlyList<SGeneratedFile> files, bool check) =>
        ApplyTransaction([(outputRoot, files)], check);

    internal const string InGameMarkerName = ".ingamedata-output";
    internal const string InGameMarkerContent = "GameDataGenerator InGameData owned output v1\n";

    public static void ApplyInGame(string outputRoot, IReadOnlyList<SGeneratedFile> files, bool check) =>
        ApplyTransaction([(outputRoot, files)], check, InGameMarkerName, InGameMarkerContent);

    internal static void ValidateRoots(IReadOnlyList<string> roots)
    {
        if (OperatingSystem.IsWindows() && roots.Any(root => root.Replace('/', '\\').StartsWith(@"\\", StringComparison.Ordinal)))
            throw new InvalidOperationException("UNC, device and extended root namespaces are unsupported.");
        string[] fullRoots = roots.Select(root => Path.TrimEndingDirectorySeparator(Path.GetFullPath(root))).ToArray();
        string[] physicalRoots = new string[fullRoots.Length];
        for (int index = 0; index < fullRoots.Length; ++index)
        {
            string root = fullRoots[index];
            if (Path.GetDirectoryName(root) == null) throw new InvalidOperationException("Filesystem roots cannot be output/input roots.");
            for (int other = 0; other < index; ++other)
            {
                bool Contains(string parent, string child) => child.Equals(parent, StringComparison.OrdinalIgnoreCase) ||
                    child.StartsWith(parent + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
                if (Contains(root, fullRoots[other]) || Contains(fullRoots[other], root))
                    throw new InvalidOperationException("Input/output directories and output roots must be disjoint.");
            }
            for (DirectoryInfo? directory = new(root); directory != null; directory = directory.Parent)
                if (directory.Exists && directory.Attributes.HasFlag(FileAttributes.ReparsePoint))
                    throw new InvalidOperationException("Input/output reparse point is unsupported.");
            if (File.Exists(root)) throw new InvalidOperationException($"Root is a file: {root}");
            if (Directory.Exists(root)) ValidateTree(root);
            physicalRoots[index] = OperatingSystem.IsWindows() ? ResolvePhysicalRoot(root) : root;
            for (int other = 0; other < index; ++other)
            {
                bool Contains(string parent, string child) => child.Equals(parent, StringComparison.OrdinalIgnoreCase) ||
                    child.StartsWith(parent + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase);
                if (Contains(physicalRoots[index], physicalRoots[other]) || Contains(physicalRoots[other], physicalRoots[index]))
                    throw new InvalidOperationException("Physical input/output directories and output roots must be disjoint.");
            }
        }
    }

    private static string ResolvePhysicalRoot(string root)
    {
        // 기존 조상은 볼륨 GUID + 정규화된 긴 이름으로 식별하고 없는 접미사만 덧붙인다.
        // UNC/장치 경로 및 해석 실패는 추측하지 않고 거부한다. 검증~게시 동안 외부 변경은 금지한다.
        if (root.StartsWith(@"\\", StringComparison.Ordinal) || root[1] != ':' || root[2] != '\\' ||
            root[3..].Split('\\').Any(IsUnsupportedWindowsComponent))
            throw new InvalidOperationException("Unsupported or ambiguous Windows root: " + root);
        var suffix = new Stack<string>();
        string ancestor = root;
        while (true)
        {
            using var handle = CreateFile(ancestor, 0, 7, IntPtr.Zero, 3, 0x02000000, IntPtr.Zero);
            if (!handle.IsInvalid)
            {
                var canonical = new StringBuilder(32768);
                uint length = GetFinalPathNameByHandle(handle, canonical, (uint)canonical.Capacity, 1);
                if (length == 0 || length >= canonical.Capacity || !canonical.ToString().StartsWith(@"\\?\Volume{", StringComparison.OrdinalIgnoreCase))
                    throw new InvalidOperationException("Cannot resolve physical root: " + ancestor,
                        new System.ComponentModel.Win32Exception(System.Runtime.InteropServices.Marshal.GetLastWin32Error()));
                if (!File.GetAttributes(ancestor).HasFlag(FileAttributes.Directory))
                    throw new InvalidOperationException("Root ancestor is not a directory: " + ancestor);
                string physical = canonical.ToString().TrimEnd('\\');
                foreach (string part in suffix) physical += "\\" + part;
                return physical;
            }
            int error = System.Runtime.InteropServices.Marshal.GetLastWin32Error();
            if (error is not (2 or 3))
                throw new InvalidOperationException("Cannot open physical root ancestor: " + ancestor,
                    new System.ComponentModel.Win32Exception(error));
            string missingName = Path.GetFileName(ancestor);
            if (missingName.Contains('~'))
                throw new InvalidOperationException("Unresolvable prospective short-name alias is unsupported: " + ancestor);
            suffix.Push(missingName);
            ancestor = Path.GetDirectoryName(ancestor) ?? throw new InvalidOperationException("Cannot resolve root: " + root);
        }
    }

    private static bool IsUnsupportedWindowsComponent(string component)
    {
        if (component.Length == 0 || component.EndsWith(' ') || component.EndsWith('.') ||
            component.IndexOfAny(Path.GetInvalidFileNameChars()) >= 0)
            return true;

        // 장치 이름은 확장자가 붙어도 예약된다. 없는 조상의 접미사도 생성 전에 검사한다.
        string stem = component.Split('.')[0].TrimEnd(' ');
        if (stem.Equals("CON", StringComparison.OrdinalIgnoreCase) || stem.Equals("PRN", StringComparison.OrdinalIgnoreCase) ||
            stem.Equals("AUX", StringComparison.OrdinalIgnoreCase) || stem.Equals("NUL", StringComparison.OrdinalIgnoreCase) ||
            stem.Equals("CONIN$", StringComparison.OrdinalIgnoreCase) || stem.Equals("CONOUT$", StringComparison.OrdinalIgnoreCase))
            return true;
        return stem.Length == 4 &&
            (stem.StartsWith("COM", StringComparison.OrdinalIgnoreCase) || stem.StartsWith("LPT", StringComparison.OrdinalIgnoreCase)) &&
            (stem[3] is >= '1' and <= '9' or '\u00B9' or '\u00B2' or '\u00B3');
    }

    [System.Runtime.InteropServices.DllImport("kernel32.dll", EntryPoint = "CreateFileW", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
    private static extern Microsoft.Win32.SafeHandles.SafeFileHandle CreateFile(string path, uint access, uint share, IntPtr security, uint creation, uint flags, IntPtr template);

    [System.Runtime.InteropServices.DllImport("kernel32.dll", EntryPoint = "GetFinalPathNameByHandleW", CharSet = System.Runtime.InteropServices.CharSet.Unicode, SetLastError = true)]
    private static extern uint GetFinalPathNameByHandle(Microsoft.Win32.SafeHandles.SafeFileHandle handle, StringBuilder path, uint capacity, uint flags);

    private static void ValidateTree(string root)
    {
        foreach (string entry in Directory.EnumerateFileSystemEntries(root))
        {
            FileAttributes attributes = File.GetAttributes(entry);
            if (attributes.HasFlag(FileAttributes.ReparsePoint))
                throw new InvalidOperationException($"Input/output reparse point is unsupported: {entry}");
            if (attributes.HasFlag(FileAttributes.Directory)) ValidateTree(entry);
        }
    }

    private static void ValidateOwnership(string root, string markerName, string markerContent)
    {
        if (Directory.Exists(root) && Directory.EnumerateFileSystemEntries(root).Any() &&
            (!File.Exists(Path.Combine(root, markerName)) ||
             File.ReadAllText(Path.Combine(root, markerName)) != markerContent ||
             File.Exists(Path.Combine(root, markerName == InGameMarkerName ? ".gamedata-output" : InGameMarkerName))))
            throw new InvalidOperationException($"Refusing to replace/check a nonempty directory not owned by GameDataGenerator: {root}");
    }

    private sealed class FPendingOutput
    {
        public FPendingOutput(string root, IReadOnlyList<SGeneratedFile> files)
        {
            Root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(root));
            Files = files;
            string parent = Path.GetDirectoryName(Root)!;
            string name = Path.GetFileName(Root);
            Staging = Path.Combine(parent, $".{name}.staging.{Guid.NewGuid():N}");
            Backup = Path.Combine(parent, $".{name}.backup.{Guid.NewGuid():N}");
        }

        public string Root { get; }
        public IReadOnlyList<SGeneratedFile> Files { get; }
        public string Staging { get; }
        public string Backup { get; }
        public bool BackedUp { get; set; }
        public bool Published { get; set; }
    }

    private static void ApplyTransaction(IReadOnlyList<(string Root, IReadOnlyList<SGeneratedFile> Files)> outputs, bool check,
        string markerName = ".gamedata-output", string markerContent = "GameDataGenerator owned output v1\n")
    {
        ValidateRoots(outputs.Select(output => output.Root).ToArray());
        foreach (var output in outputs)
        {
            ValidateUniquePaths(output.Files);
            ValidateOwnership(output.Root, markerName, markerContent);
        }
        if (check)
        {
            foreach (var output in outputs) Check(output.Root, output.Files, ignoreBuildArtifacts: markerName != InGameMarkerName);
            return;
        }

        // 내용이 같은 루트는 이동하지 않아 mtime과 증분 빌드 캐시를 보존한다.
        outputs = outputs.Where(output => !IsCurrent(output.Root, output.Files)).ToArray();
        if (outputs.Count == 0) return;

        var pending = outputs.Select(output => new FPendingOutput(output.Root, output.Files)).ToArray();
        Exception? applyFailure = null;
        bool committed = false;
        try
        {
            // 두 루트의 파일 쓰기를 모두 끝낸 뒤에만 기존 출력을 이동한다.
            foreach (var output in pending)
            {
                string parent = Path.GetDirectoryName(output.Root)!;
                Execute("output-parent-create", parent, null, () => Directory.CreateDirectory(parent));
                Execute("staging-create", output.Staging, null, () => Directory.CreateDirectory(output.Staging));
                WriteFiles(output.Staging, output.Files, output.Root);
            }
            try
            {
                foreach (var output in pending)
                {
                    if (Directory.Exists(output.Root))
                    {
                        MoveDirectory("backup-move", output.Root, output.Backup);
                        output.BackedUp = true;
                    }
#if GAMEDATA_TEST_HOOKS
                    BeforePublishForTest?.Invoke();
#endif
                    MoveDirectory("publish-move", output.Staging, output.Root);
                    output.Published = true;
                }
            }
            catch (Exception publishFailure)
            {
                var failures = new List<Exception> { publishFailure };
                foreach (var output in pending.Reverse())
                {
                    try
                    {
                        if (output.Published)
                            Execute("rollback-delete", output.Root, null, () => Directory.Delete(output.Root, recursive: true));
                        if (output.BackedUp) MoveDirectory("rollback-move", output.Backup, output.Root);
                    }
                    catch (Exception rollbackFailure) { failures.Add(rollbackFailure); }
                }
                if (failures.Count > 1)
                    throw new AggregateException("Output publication and rollback failed. Previous output remains in backup where restoration failed.", failures);
                throw;
            }
            // 모든 게시가 성공한 시점이 커밋 경계다. 이후 GC 실패는 새 출력을 되돌리지 않는다.
            committed = true;
            var backupFailures = new List<(string Path, Exception Failure)>();
            foreach (var output in pending)
                if (output.BackedUp)
                    try { Directory.Delete(output.Backup, recursive: true); }
                    catch (Exception exception) { backupFailures.Add((output.Backup, exception)); }
            string[] remainingBackups = pending.Where(output => Directory.Exists(output.Backup)).Select(output => output.Backup).ToArray();
            foreach (var failure in backupFailures)
                ReportFailure("backup-cleanup", failure.Path, null, failure.Failure,
                    committed: true, remainingBackupPaths: remainingBackups);
        }
        catch (Exception exception)
        {
            applyFailure = exception;
            throw;
        }
        finally
        {
            var cleanupFailures = new List<Exception>();
            foreach (var output in pending)
                if (Directory.Exists(output.Staging))
                    try
                    {
                        if (committed) Directory.Delete(output.Staging, recursive: true);
                        else Execute("staging-delete", output.Staging, null, () => Directory.Delete(output.Staging, recursive: true));
                    }
                    catch (Exception cleanupFailure)
                    {
                        if (committed)
                            ReportFailure("staging-cleanup", output.Staging, null, cleanupFailure, committed: true,
                                remainingBackupPaths: pending.Where(item => Directory.Exists(item.Backup)).Select(item => item.Backup).ToArray());
                        else cleanupFailures.Add(cleanupFailure);
                    }
            if (cleanupFailures.Count > 0)
            {
                if (applyFailure != null) cleanupFailures.Insert(0, applyFailure);
                throw new AggregateException("Output operation or staging cleanup failed.", cleanupFailures);
            }
        }
    }

    internal static void ReportFailure(string operation, string path, string? destination, Exception exception, int attempt = 1,
        bool committed = false, IReadOnlyList<string>? remainingBackupPaths = null)
    {
        try
        {
            Console.Error.WriteLine(System.Text.Json.JsonSerializer.Serialize(new
            {
                timestamp = DateTimeOffset.UtcNow,
                operation,
                transactionState = committed ? "committed" : "not-committed",
                severity = committed ? "warning" : "error",
                remainingBackupPaths,
                backupContentsMayBePartial = committed,
                attempt,
                path,
                destination,
                type = exception.GetType().FullName,
                hresult = $"0x{exception.HResult:X8}",
                stack = exception.ToString()
            }));
        }
        catch (Exception)
        {
            // 진단 스트림 실패로 원래 I/O 예외를 바꾸지 않는다.
            return;
        }
    }

    private static void Execute(string operation, string path, string? destination, Action action, int attempt = 1)
    {
        try { action(); }
        catch (Exception exception)
        {
            ReportFailure(operation, path, destination, exception, attempt);
#if GAMEDATA_TEST_HOOKS
            if (operation.EndsWith("-move", StringComparison.Ordinal)) MoveFailedForTest?.Invoke(operation, exception);
#endif
            throw;
        }
    }

    private static void MoveDirectory(string operation, string source, string destination)
    {
        const int kMaxAttempts = 6;
        for (int attempt = 1; ; ++attempt)
        {
            try
            {
                Execute(operation, source, destination, () => Directory.Move(source, destination), attempt);
                return;
            }
            catch (IOException exception) when (OperatingSystem.IsWindows() && attempt < kMaxAttempts &&
                exception.HResult is unchecked((int)0x80070005) or unchecked((int)0x80070020) or unchecked((int)0x80070021))
            {
                // Windows는 삭제 공유 없는 하위 파일 핸들도 디렉터리 rename의 ERROR_ACCESS_DENIED로 반환한다.
                // 동일 rename만 총 6회/최대 620ms backoff로 제한한다. 영구 거부는 그대로 실패한다.
                Thread.Sleep(20 << (attempt - 1));
            }
        }
    }

    private static void Check(string outputRoot, IReadOnlyList<SGeneratedFile> files, bool ignoreBuildArtifacts)
    {
        if (!Directory.Exists(outputRoot))
        {
            throw new InvalidOperationException($"Generated output is missing: {outputRoot}");
        }

        Dictionary<string, string> expected = files.ToDictionary(
            file => NormalizeRelativePath(file.RelativePath),
            file => file.Content,
            StringComparer.Ordinal);
        string[] actualPaths = Directory.GetFiles(outputRoot, "*", SearchOption.AllDirectories)
            .Where(path => !ignoreBuildArtifacts || !IsBuildArtifactPath(outputRoot, path))
            .ToArray();
        var failures = new List<string>();

        foreach ((string relativePath, string content) in expected)
        {
            string fullPath = Path.Combine(outputRoot, relativePath.Replace('/', Path.DirectorySeparatorChar));
            if (!File.Exists(fullPath))
            {
                failures.Add($"missing: {relativePath}");
                continue;
            }
            if (!File.ReadAllBytes(fullPath).AsSpan().SequenceEqual(Utf8WithoutBom.GetBytes(content)))
            {
                failures.Add($"changed: {relativePath}");
            }
        }

        foreach (string actualPath in actualPaths)
        {
            string relativePath = NormalizeRelativePath(Path.GetRelativePath(outputRoot, actualPath));
            if (!expected.ContainsKey(relativePath))
            {
                failures.Add($"unexpected: {relativePath}");
            }
        }

        if (!ignoreBuildArtifacts)
            failures.AddRange(UnexpectedDirectories(outputRoot, files).Select(path => "unexpected directory: " + path));

        if (failures.Count != 0)
        {
            throw new InvalidOperationException("Generated game data is out of date:\n  " + string.Join("\n  ", failures.OrderBy(value => value, StringComparer.Ordinal)));
        }
    }

    private static IEnumerable<string> UnexpectedDirectories(string root, IReadOnlyList<SGeneratedFile> files)
    {
        var expected = new HashSet<string>(StringComparer.Ordinal);
        foreach (var file in files)
        {
            string path = NormalizeRelativePath(file.RelativePath);
            for (int slash = path.LastIndexOf('/'); slash >= 0; slash = path.LastIndexOf('/'))
            {
                path = path[..slash];
                expected.Add(path);
            }
        }
        return Directory.GetDirectories(root, "*", SearchOption.AllDirectories)
            .Select(path => NormalizeRelativePath(Path.GetRelativePath(root, path)))
            .Where(path => !expected.Contains(path));
    }

    private static bool IsCurrent(string root, IReadOnlyList<SGeneratedFile> files)
    {
        if (!Directory.Exists(root)) return false;
        if (UnexpectedDirectories(root, files).Any()) return false;
        string[] actual = Directory.GetFiles(root, "*", SearchOption.AllDirectories)
            .Select(path => NormalizeRelativePath(Path.GetRelativePath(root, path))).Order(StringComparer.Ordinal).ToArray();
        if (!actual.SequenceEqual(files.Select(file => NormalizeRelativePath(file.RelativePath)).Order(StringComparer.Ordinal))) return false;
        return files.All(file => File.ReadAllBytes(Path.Combine(root, file.RelativePath.Replace('/', Path.DirectorySeparatorChar)))
            .AsSpan().SequenceEqual(Utf8WithoutBom.GetBytes(file.Content)));
    }

    private static void WriteFiles(string root, IReadOnlyList<SGeneratedFile> files, string previousRoot)
    {
        foreach (SGeneratedFile file in files.OrderBy(file => file.RelativePath, StringComparer.Ordinal))
        {
            string fullPath = Path.Combine(root, file.RelativePath.Replace('/', Path.DirectorySeparatorChar));
            Execute("parent-create", Path.GetDirectoryName(fullPath)!, null, () => Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!));
            Execute("file-write", fullPath, null, () => File.WriteAllText(fullPath, file.Content, Utf8WithoutBom));
            string previous = Path.Combine(previousRoot, file.RelativePath.Replace('/', Path.DirectorySeparatorChar));
            if (File.Exists(previous) && File.ReadAllBytes(previous).AsSpan().SequenceEqual(Utf8WithoutBom.GetBytes(file.Content)))
                Execute("mtime-preserve", fullPath, null, () => File.SetLastWriteTimeUtc(fullPath, File.GetLastWriteTimeUtc(previous)));
        }
    }

    private static void ValidateUniquePaths(IReadOnlyList<SGeneratedFile> files)
    {
        foreach (SGeneratedFile file in files)
        {
            string path = NormalizeRelativePath(file.RelativePath);
            if (Path.IsPathRooted(path) || path.Split('/').Any(segment =>
                segment is "" or "." or ".." || segment.IndexOfAny([':', '*', '?', '"', '<', '>', '|']) >= 0))
                throw new InvalidOperationException($"Unsafe generated path: {path}");
        }
        string? duplicate = files
            .GroupBy(file => NormalizeRelativePath(file.RelativePath), StringComparer.OrdinalIgnoreCase)
            .FirstOrDefault(group => group.Count() > 1)?.Key;
        if (duplicate != null)
        {
            throw new InvalidOperationException($"Duplicate generated path: {duplicate}");
        }
    }

    private static bool IsBuildArtifactPath(string outputRoot, string path)
    {
        return Path.GetRelativePath(outputRoot, path)
            .Split([Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar], StringSplitOptions.RemoveEmptyEntries)
            .Any(segment => segment.Equals("bin", StringComparison.OrdinalIgnoreCase) || segment.Equals("obj", StringComparison.OrdinalIgnoreCase));
    }

    private static string NormalizeRelativePath(string path) => path.Replace('\\', '/');
}
