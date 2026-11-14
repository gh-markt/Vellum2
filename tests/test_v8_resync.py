#!/usr/bin/env python3
"""Exercise safe resynchronization of a patched V8 checkout."""
import importlib.util
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("v8_resync_builder", ROOT / "scripts/prepare_dependencies.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class V8ResyncTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "v8"
        self.source.mkdir()
        self.git(self.source, "init", "-q")
        self.example = self.source / "example.txt"
        self.example.write_text("before\n")
        self.shim = self.source / "build/linux/unbundle/icu.gn"
        self.shim.parent.mkdir(parents=True)
        self.shim.write_text("system shim\n")
        (self.shim.parent / "replace_gn_files.py").write_text(
            "import pathlib\n"
            "root = pathlib.Path(__file__).resolve().parents[3]\n"
            "target = root / 'third_party/icu/BUILD.gn'\n"
            "target.with_name('BUILD.gn.orig').write_bytes(target.read_bytes())\n"
            "target.write_bytes((pathlib.Path(__file__).parent / 'icu.gn').read_bytes())\n")
        self.git(self.source, "add", "example.txt", "build")
        self.commit(self.source)
        self.revision = self.git(self.source, "rev-parse", "HEAD").strip()
        self.patch = self.root / "change.patch"
        self.patch.write_text("--- a/example.txt\n+++ b/example.txt\n@@ -1 +1 @@\n-before\n+after\n")
        self.git(self.source, "apply", str(self.patch))
        self.icu = self.source / "third_party/icu"
        self.icu.mkdir(parents=True)
        self.git(self.icu, "init", "-q")
        self.target = self.icu / "BUILD.gn"
        self.target.write_text("original ICU build\n")
        (self.icu / "other.txt").write_text("untouched\n")
        self.git(self.icu, "add", "BUILD.gn", "other.txt")
        self.commit(self.icu)
        self.backup = self.icu / "BUILD.gn.orig"
        self.backup.write_text("original ICU build\n")
        self.target.write_text("system shim\n")
        self.untracked = self.source / "local-notes.txt"
        self.untracked.write_text("user data")
        self.lock = dict(builder.LOCK)
        self.lock["v8"] = dict(self.lock["v8"], revision=self.revision)
        self.patches = (mock.patch.object(builder, "LOCK", self.lock),
                        mock.patch.object(builder, "V8_ICU_PATCH", self.patch))
        for patch in self.patches:
            patch.start()
            self.addCleanup(patch.stop)

    def git(self, directory, *args):
        return subprocess.check_output(["git", "-C", str(directory), *args], text=True)

    def commit(self, directory):
        self.git(directory, "-c", "user.name=Test", "-c", "user.email=test@example.invalid",
                 "commit", "-qm", "fixture")

    def assert_restored(self):
        self.assertEqual(self.example.read_text(), "after\n")
        self.assertEqual(self.target.read_text(), "system shim\n")
        self.assertEqual(self.backup.read_text(), "original ICU build\n")
        self.assertEqual(self.untracked.read_text(), "user data")
        self.assertEqual(self.git(self.source, "diff", "--cached", "--name-only"), "")

    def test_sync_sees_clean_checkouts_and_restores_after_failure(self):
        self.target.chmod(0o640)
        example_time = self.example.stat().st_mtime_ns
        target_time = self.target.stat().st_mtime_ns
        for interrupted in (False, True):
            with self.subTest(interrupted=interrupted):
                try:
                    with builder.clean_v8_for_sync(self.source):
                        self.assertEqual(builder._tracked_status(self.source), "")
                        self.assertEqual(builder._tracked_status(self.icu), "")
                        self.assertEqual(self.example.read_text(), "before\n")
                        self.assertEqual(self.target.read_text(), "original ICU build\n")
                        self.assertEqual(self.target.stat().st_mode & 0o777, 0o640)
                        if interrupted:
                            raise RuntimeError("sync failed")
                except RuntimeError as error:
                    self.assertTrue(interrupted)
                    self.assertEqual(str(error), "sync failed")
                self.assert_restored()
                self.assertEqual(self.target.stat().st_mode & 0o777, 0o640)
                self.assertEqual(self.example.stat().st_mtime_ns, example_time)
                self.assertEqual(self.target.stat().st_mtime_ns, target_time)

    def test_process_termination_during_sync_is_resumable(self):
        code = (
            "import importlib.util, os, pathlib, sys\n"
            "s=importlib.util.spec_from_file_location('b',sys.argv[1]); "
            "b=importlib.util.module_from_spec(s); s.loader.exec_module(b)\n"
            "b.V8_ICU_PATCH=pathlib.Path(sys.argv[3]); "
            "b.LOCK=dict(b.LOCK, v8=dict(b.LOCK['v8'], revision=sys.argv[4]))\n"
            "with b.clean_v8_for_sync(pathlib.Path(sys.argv[2])):\n"
            "    os._exit(97)\n")
        result = subprocess.run([sys.executable, "-c", code, str(ROOT / "scripts/prepare_dependencies.py"),
                                 str(self.source), str(self.patch), self.revision],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 97, result.stderr)
        self.assertEqual(self.example.read_text(), "before\n")
        self.assertEqual(self.target.read_text(), "original ICU build\n")
        with builder.clean_v8_for_sync(self.source):
            self.assertEqual(builder._tracked_status(self.source), "")
            self.assertEqual(builder._tracked_status(self.icu), "")
        builder.configure_v8_system_icu(self.source)
        self.assert_restored()

    def test_unrelated_v8_changes_are_not_rewritten(self):
        self.example.write_text("after\nuser edit\n")
        with self.assertRaisesRegex(RuntimeError, "beyond"):
            with builder.clean_v8_for_sync(self.source):
                self.fail("must not synchronize")
        self.assertEqual(self.example.read_text(), "after\nuser edit\n")
        self.assertEqual(self.target.read_text(), "system shim\n")
        self.git(self.source, "add", "example.txt")
        with self.assertRaisesRegex(RuntimeError, "staged"):
            with builder.clean_v8_for_sync(self.source):
                self.fail("must not synchronize")
        self.assertEqual(self.git(self.source, "diff", "--cached", "--name-only").strip(), "example.txt")

    def test_unrelated_icu_changes_are_not_rewritten(self):
        (self.icu / "other.txt").write_text("user edit\n")
        with self.assertRaisesRegex(RuntimeError, "ICU checkout"):
            with builder.clean_v8_for_sync(self.source):
                self.fail("must not synchronize")
        self.assertEqual((self.icu / "other.txt").read_text(), "user edit\n")
        self.assert_restored()

    def test_bad_icu_backup_is_rejected_without_changes(self):
        self.backup.write_text("user-modified backup\n")
        with self.assertRaisesRegex(RuntimeError, "backup was modified"):
            with builder.clean_v8_for_sync(self.source):
                self.fail("must not synchronize")
        self.assertEqual(self.example.read_text(), "after\n")
        self.assertEqual(self.target.read_text(), "system shim\n")
        self.assertEqual(self.backup.read_text(), "user-modified backup\n")

    def test_patched_checkout_with_different_pin_is_not_changed(self):
        lock = dict(self.lock)
        lock["v8"] = dict(lock["v8"], revision="not-the-current-commit")
        with mock.patch.object(builder, "LOCK", lock):
            with self.assertRaisesRegex(RuntimeError, "not at the pinned revision"):
                with builder.clean_v8_for_sync(self.source):
                    self.fail("must not synchronize")
        self.assert_restored()

    def test_unpatched_checkout_with_shim_is_restored(self):
        self.git(self.source, "apply", "--reverse", str(self.patch))
        with builder.clean_v8_for_sync(self.source):
            self.assertEqual(self.example.read_text(), "before\n")
            self.assertEqual(self.target.read_text(), "original ICU build\n")
        self.assertEqual(self.example.read_text(), "before\n")
        self.assertEqual(self.target.read_text(), "system shim\n")

    def test_changed_icu_during_sync_is_not_overwritten(self):
        with self.assertRaisesRegex(RuntimeError, "not overwriting"):
            with builder.clean_v8_for_sync(self.source):
                self.target.write_text("external change\n")
        self.assertEqual(self.target.read_text(), "external change\n")
        self.assertEqual(self.example.read_text(), "after\n")

    def test_failed_icu_replacement_preserves_both_checkouts(self):
        with mock.patch.object(builder.os, "replace", side_effect=PermissionError("injected")):
            with self.assertRaises(PermissionError):
                with builder.clean_v8_for_sync(self.source):
                    self.fail("must not synchronize")
        self.assert_restored()
        self.assertFalse(list(self.icu.glob(".vellum-icu-*")))

    def test_timestamp_restore_never_hides_a_changed_file(self):
        digest = hashlib.sha256(self.example.read_bytes()).digest()
        self.example.write_text("user edit after sync\n")
        edited_time = self.example.stat().st_mtime_ns
        builder._restore_file_times(self.example, digest, (1, 1))
        self.assertEqual(self.example.stat().st_mtime_ns, edited_time)
        self.assertEqual(self.example.read_text(), "user edit after sync\n")

    def test_exception_after_icu_replace_still_restores_shim(self):
        original = builder._replace_file_contents
        def replace_then_fail(path, contents):
            original(path, contents)
            if contents == b"original ICU build\n":
                raise OSError("injected after replacement")
        with mock.patch.object(builder, "_replace_file_contents", side_effect=replace_then_fail):
            with self.assertRaisesRegex(OSError, "injected after"):
                with builder.clean_v8_for_sync(self.source):
                    self.fail("must not synchronize")
        self.assert_restored()

    def test_new_checkout_needs_no_temporary_changes(self):
        with builder.clean_v8_for_sync(self.root / "new"):
            pass


if __name__ == "__main__":
    unittest.main()
