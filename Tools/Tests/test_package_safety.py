"""Dependency-free Windows packer integration tests; all fixtures live in scratch.

Run with --packer <absolute exe path> [--scratch <absolute scratch directory>].
Concurrent publishers use independent temporary files: last successful publish wins.
These tests assume input files and parent directories are not externally mutated.
"""
import argparse
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import tempfile
import unittest


class PackageSafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.env = os.environ.copy()
        cls.env["TEXT_RPG_GAMEDATA_KEY"] = secrets.token_hex(32)

    def setUp(self):
        self.root = Path(tempfile.mkdtemp(prefix="package_safety_", dir=SCRATCH))
        self.addCleanup(shutil.rmtree, self.root)
        self.inputs = self.root / "input"
        self.inputs.mkdir()
        self.source = self.inputs / "one.yaml"
        self.original = b"answer: 42\n"
        self.source.write_bytes(self.original)

    def run_packer(self, *args):
        return subprocess.run([str(PACKER), *map(str, args)], cwd=self.root,
                              env=self.env, capture_output=True, timeout=60)

    def assert_rejected_alias(self, output):
        result = self.run_packer("pack", self.inputs, output)
        self.assertEqual(self.source.read_bytes(), self.original, "source YAML was replaced")
        self.assertNotEqual(result.returncode, 0, "input alias was accepted as output")
        self.assertIn(b"input", result.stderr.lower())
        self.assertEqual(list(self.root.rglob("*.tmp")), [])

    def test_explicit_asset_root_packs_namespaced_raw_files(self):
        assets = self.root / "assets"
        (assets / "Nested").mkdir(parents=True)
        raw = b"\xef\xbb\xbf  art  \r\n\x00\xff"
        (assets / "Nested/MixedCase.bin").write_bytes(raw)
        (assets / "empty.dat").write_bytes(b"")
        output = self.root / "assets.pkg"
        result = self.run_packer("pack", self.inputs, output, "--asset-root", assets)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(output.read_bytes()[:8], b"TGDP\x02\x00\x00\x00")
        result = self.run_packer("verify", output)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"Data/Client/one.yaml", result.stdout)
        self.assertIn(b"Asset/Nested/MixedCase.bin (" + str(len(raw)).encode() + b" bytes)", result.stdout)
        self.assertIn(b"Asset/empty.dat (0 bytes)", result.stdout)
        self.assertEqual((assets / "Nested/MixedCase.bin").read_bytes(), raw)
        self.assertEqual(list(self.root.rglob("*.tmp")), [])

    def test_unspecified_asset_root_is_not_collected(self):
        assets = self.root / "Asset"
        assets.mkdir()
        (assets / "private.bin").write_bytes(b"not an implicit input")
        output = self.root / "explicit.pkg"
        self.assertEqual(self.run_packer("pack", self.inputs, output).returncode, 0)
        result = self.run_packer("verify", output)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b"Data/Client/one.yaml", result.stdout)
        self.assertNotIn(b"private.bin", result.stdout)

    def test_asset_output_hardlink_alias_preserves_input_and_previous_output(self):
        assets = self.root / "Asset"
        assets.mkdir()
        raw = b"  raw\r\n\x00\xff"
        source = assets / "raw.bin"
        source.write_bytes(raw)
        output = self.root / "alias.pkg"
        os.link(source, output)
        result = self.run_packer("pack", self.inputs, output, "--asset-root", assets)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"input", result.stderr.lower())
        self.assertEqual(output.read_bytes(), raw)
        self.assertEqual(source.read_bytes(), raw)
        self.assertEqual(list(self.root.rglob("*.tmp")), [])

    def test_missing_asset_root_preserves_previous_output(self):
        output = self.root / "previous.pkg"
        output.write_bytes(b"previous output")
        result = self.run_packer("pack", self.inputs, output, "--asset-root", self.root / "missing")
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_bytes(), b"previous output")
        self.assertEqual(self.source.read_bytes(), self.original)
        self.assertEqual(list(self.root.rglob("*.tmp")), [])

    def test_asset_root_junction_is_rejected(self):
        if os.name != "nt":
            self.skipTest("Windows junction test")
        target = self.root / "target"
        target.mkdir()
        (target / "raw.bin").write_bytes(b"outside input")
        junction = self.root / "junction"
        result = subprocess.run([os.environ.get("COMSPEC", "cmd.exe"), "/d", "/c", "mklink", "/J", str(junction), str(target)],
                                capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        output = self.root / "junction.pkg"
        output.write_bytes(b"previous output")
        result = self.run_packer("pack", self.inputs, output, "--asset-root", junction)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(b"reparse", result.stderr.lower())
        self.assertEqual(output.read_bytes(), b"previous output")
        self.assertEqual((target / "raw.bin").read_bytes(), b"outside input")

    def test_same_yaml_absolute_output_rejected(self):
        self.assert_rejected_alias(self.source)

    def test_same_yaml_relative_output_rejected(self):
        self.assert_rejected_alias(Path("input") / ".." / "input" / "one.yaml")

    def test_same_yaml_case_alias_rejected(self):
        alias = self.source.with_name("ONE.YAML")
        if not alias.exists():
            self.skipTest("filesystem is case-sensitive")
        self.assert_rejected_alias(alias)

    def test_same_yaml_hardlink_output_rejected(self):
        alias = self.root / "alias.pkg"
        try:
            os.link(self.source, alias)
        except OSError as error:
            self.skipTest(f"hardlinks unavailable: {error}")
        self.assert_rejected_alias(alias)
        self.assertEqual(alias.read_bytes(), self.original)

    def test_preexisting_tmp_survives_publish_failure(self):
        output = self.root / "blocked.pkg"
        output.mkdir()
        (output / "keep").write_bytes(b"destination marker")
        collision = self.root / "blocked.pkg.tmp"
        collision.write_bytes(b"unowned temporary marker")
        before = {p.relative_to(self.root) for p in self.root.rglob("*")}
        result = self.run_packer("pack", self.inputs, output)
        self.assertNotEqual(result.returncode, 0)
        self.assertTrue(collision.exists(), "unowned .tmp file was deleted")
        self.assertEqual(collision.read_bytes(), b"unowned temporary marker")
        self.assertEqual((output / "keep").read_bytes(), b"destination marker")
        self.assertEqual({p.relative_to(self.root) for p in self.root.rglob("*")}, before,
                         "failed publish leaked a temporary file")

    def test_preexisting_tmp_directory_does_not_block_pack(self):
        output = self.root / "normal.pkg"
        collision = self.root / "normal.pkg.tmp"
        collision.mkdir()
        (collision / "keep").write_bytes(b"unowned directory marker")
        result = self.run_packer("pack", self.inputs, output)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.run_packer("verify", output).returncode, 0)
        self.assertEqual((collision / "keep").read_bytes(), b"unowned directory marker")

    def test_yaml_output_tmp_hardlink_does_not_damage_input(self):
        output = self.root / "output.yaml"
        collision = self.root / "output.yaml.tmp"
        try:
            os.link(self.source, collision)
        except OSError as error:
            self.skipTest(f"hardlinks unavailable: {error}")
        result = self.run_packer("pack", self.inputs, output)
        self.assertEqual(self.source.read_bytes(), self.original, "temporary write damaged YAML input")
        self.assertTrue(collision.exists(), "unowned hardlink was consumed")
        self.assertEqual(collision.read_bytes(), self.original)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.run_packer("verify", output).returncode, 0)
        self.assertEqual(set(self.root.glob("*.tmp")), {collision})

    def test_locked_existing_output_survives_publish_failure(self):
        if os.name != "nt":
            self.skipTest("Win32 sharing semantics required")
        import ctypes
        from ctypes import wintypes
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                      ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD,
                                      wintypes.HANDLE]
        kernel.CreateFileW.restype = wintypes.HANDLE
        kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        kernel.CloseHandle.restype = wintypes.BOOL
        output = self.root / "locked.pkg"
        original = b"previous output must survive"
        output.write_bytes(original)
        # 읽기 공유만 허용하여 대상 교체를 실제 Windows 파일 공유 규칙으로 막는다.
        handle = kernel.CreateFileW(str(output), 0x80000000, 1, None, 3, 0x80, None)
        self.assertNotEqual(handle, ctypes.c_void_p(-1).value, "could not lock test output")
        try:
            result = self.run_packer("pack", self.inputs, output)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"publish", result.stderr.lower())
            self.assertEqual(output.read_bytes(), original)
            self.assertEqual(list(self.root.rglob("*.tmp")), [])
            self.assertEqual(self.source.read_bytes(), self.original)
        finally:
            self.assertTrue(kernel.CloseHandle(handle))

    def test_successful_replacement_and_authentication(self):
        output = self.root / "normal.pkg"
        output.write_bytes(b"previous output")
        result = self.run_packer("pack", self.inputs, output)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(self.run_packer("verify", output).returncode, 0)
        self.assertEqual(self.source.read_bytes(), self.original)
        self.assertEqual(list(self.root.rglob("*.tmp")), [])
        corrupted = bytearray(output.read_bytes())
        corrupted[-1] ^= 1
        output.write_bytes(corrupted)
        self.assertNotEqual(self.run_packer("verify", output).returncode, 0)

    def test_concurrent_publishers_leave_complete_package(self):
        output = self.root / "concurrent.pkg"
        processes = [subprocess.Popen([str(PACKER), "pack", str(self.inputs), str(output)],
                                     cwd=self.root, env=self.env, stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE) for _ in range(8)]
        try:
            for process in processes:
                process.communicate(timeout=60)
            self.assertTrue(any(p.returncode == 0 for p in processes))
            self.assertTrue(all(p.returncode in (0, 1) for p in processes))
            self.assertEqual(self.run_packer("verify", output).returncode, 0)
            self.assertEqual(self.source.read_bytes(), self.original)
            self.assertEqual(list(self.root.rglob("*.tmp")), [])
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                process.communicate()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--packer", required=True, type=Path)
    parser.add_argument("--scratch", type=Path,
                        default=Path.home() / "AppData/Local/hermes/cache/scratch")
    options, tests = parser.parse_known_args()
    if not options.packer.is_absolute() or not options.packer.is_file():
        parser.error("--packer must name an existing absolute executable path")
    if not options.scratch.is_absolute():
        parser.error("--scratch must be absolute (OS temporary directory is never used)")
    PACKER = options.packer
    SCRATCH = options.scratch
    SCRATCH.mkdir(parents=True, exist_ok=True)
    unittest.main(argv=[__file__, *tests], verbosity=2)
