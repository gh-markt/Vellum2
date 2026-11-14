#!/usr/bin/env python3
"""Verification of source packaging with real isolated Git repositories."""
import importlib.util
import contextlib
import io
import os
from pathlib import Path
import runpy
import subprocess
import tempfile
import unittest
from unittest import mock
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/package_source.py"
spec = importlib.util.spec_from_file_location("package_source", SCRIPT)
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "repo"
        self.root.mkdir()
        self.git("init", "-q")
        self.output = Path(self.temp.name) / "out" / "vellum-0.34.zip"

    def git(self, *args):
        subprocess.run(["git", "-C", str(self.root), *args], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def test_worktree_staged_files_names_and_symlinks(self):
        (self.root / "nested").mkdir()
        (self.root / "nested/a file\n.txt").write_text("staged")
        (self.root / "nested/link").symlink_to("a file\n.txt")
        (self.root / "unstaged").write_text("excluded")
        self.git("add", "nested")
        (self.root / "nested/a file\n.txt").write_text("working tree")
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            package.create_package(self.root, self.output)
        self.assertEqual(set(output.getvalue().splitlines()),
                         {"Added: 'nested/a file\\n.txt'", "Added: 'nested/link'"})
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(set(archive.namelist()), {"nested/a file\n.txt", "nested/link"})
            self.assertEqual(archive.read("nested/a file\n.txt"), b"working tree")
            self.assertEqual(archive.read("nested/link"), b"a file\n.txt")
            self.assertTrue((archive.getinfo("nested/link").external_attr >> 16) & 0o120000)

    def test_missing_file_preserves_previous_archive_and_cleans_temporary(self):
        (self.root / "file").write_text("x")
        self.git("add", "file")
        (self.root / "file").unlink()
        self.output.parent.mkdir()
        self.output.write_bytes(b"previous")
        output = io.StringIO()
        with contextlib.redirect_stdout(output), self.assertRaisesRegex(ValueError, "missing"):
            package.create_package(self.root, self.output)
        self.assertEqual(output.getvalue(), "")
        self.assertEqual(self.output.read_bytes(), b"previous")
        self.assertEqual(list(self.output.parent.iterdir()), [self.output])

    def test_empty_index_and_output_collision(self):
        with self.assertRaisesRegex(ValueError, "no files"):
            package.create_package(self.root, self.output)
        (self.root / "file").write_text("x")
        self.git("add", "file")
        with self.assertRaisesRegex(ValueError, "tracked source"):
            package.create_package(self.root, self.root / "file")

    def test_committed_and_staged_files(self):
        (self.root / "committed").write_text("original")
        self.git("add", "committed")
        self.git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                 "commit", "-qm", "fixture")
        (self.root / "committed").write_text("modified")
        (self.root / "staged").write_text("new")
        self.git("add", "staged")
        package.create_package(self.root, self.output)
        with zipfile.ZipFile(self.output) as archive:
            self.assertEqual(set(archive.namelist()), {"committed", "staged"})
            self.assertEqual(archive.read("committed"), b"modified")
            self.assertEqual(archive.read("staged"), b"new")

    def test_invalid_index_entries(self):
        for entry in (b"160000 abc 0\tmodule\0", b"100644 abc 1\tconflict\0",
                      b"100644 abc 0\t../escape\0", b"100644 abc 0\t/absolute\0"):
            with self.subTest(entry=entry), mock.patch.object(package.subprocess, "run") as run:
                run.return_value.stdout = entry
                with self.assertRaises(ValueError):
                    package.create_package(self.root, self.output)

    def test_regular_file_replaced_by_symlink(self):
        (self.root / "file").write_text("x")
        self.git("add", "file")
        (self.root / "file").unlink()
        (self.root / "file").symlink_to("/etc/passwd")
        with self.assertRaisesRegex(ValueError, "not regular"):
            package.create_package(self.root, self.output)

    def test_command_line_and_non_repository(self):
        (self.root / "file").write_text("x")
        self.git("add", "file")
        result = subprocess.run([os.sys.executable, str(SCRIPT), "--root", str(self.root),
                                 "--output", str(self.output)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(str(self.output), result.stdout)
        with mock.patch.object(os.sys, "argv", [str(SCRIPT), "--root", str(self.root),
                                                "--output", str(self.output)]):
            runpy.run_path(str(SCRIPT), run_name="__main__")
        with self.assertRaises(subprocess.CalledProcessError):
            package.create_package(Path(self.temp.name), self.output)


if __name__ == "__main__":
    unittest.main()
