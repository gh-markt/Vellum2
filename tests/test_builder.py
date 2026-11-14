#!/usr/bin/env python3
import importlib.util
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
root = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("builder", root / "scripts/prepare_dependencies.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
class BuilderTests(unittest.TestCase):
    def test_system_icu_version_validation(self):
        with mock.patch.object(builder.subprocess, "check_output", return_value="79.1\n79.1\n"):
            self.assertEqual(builder.system_icu_version(), "79.1")
        with mock.patch.object(builder.subprocess, "check_output", return_value="73.0\n73.0\n"):
            self.assertEqual(builder.system_icu_version(), "73.0")
        with mock.patch.object(builder.subprocess, "check_output", return_value="72.1\n72.1\n"):
            with self.assertRaisesRegex(RuntimeError, "requires ICU 73 or newer"):
                builder.system_icu_version()
        for output in ("", "79.1\n", "79.1\n78.2\n", "79.x\n79.x\n",
                       "79..1\n79..1\n", "７９\n７９\n", "9" * 5000 + "\n" + "9" * 5000):
            with self.subTest(output=output), mock.patch.object(
                    builder.subprocess, "check_output", return_value=output):
                with self.assertRaisesRegex(RuntimeError, "inconsistent or invalid"):
                    builder.system_icu_version()
        for failure in (FileNotFoundError(), subprocess.CalledProcessError(1, "pkg-config")):
            with self.subTest(failure=failure), mock.patch.object(
                    builder.subprocess, "check_output", side_effect=failure):
                with self.assertRaisesRegex(RuntimeError, "development packages"):
                    builder.system_icu_version()

    def test_system_icu_library_directory_validation(self):
        for directory, expected in (("/usr/lib64", "lib64"),
                                    ("/usr/lib/x86_64-linux-gnu", "lib/x86_64-linux-gnu")):
            with self.subTest(directory=directory), mock.patch.object(
                    builder.subprocess, "check_output", return_value=directory + "\n"):
                self.assertEqual(builder.system_icu_libdir(), expected)
        for paths in (("/usr/lib64", "/usr/lib"), ("/usr/lib/../tmp", "/usr/lib/../tmp"),
                      ("/opt/icu/lib", "/opt/icu/lib"), ("/usr", "/usr"), ("", "")):
            with self.subTest(paths=paths), mock.patch.object(
                    builder.subprocess, "check_output", side_effect=paths):
                with self.assertRaises(RuntimeError):
                    builder.system_icu_libdir()
        for failure in (FileNotFoundError(), subprocess.CalledProcessError(1, "pkg-config")):
            with self.subTest(failure=failure), mock.patch.object(
                    builder.subprocess, "check_output", side_effect=failure):
                with self.assertRaisesRegex(RuntimeError, "development packages"):
                    builder.system_icu_libdir()

    def test_v8_system_icu_configuration_is_checked_and_resumable(self):
        with tempfile.TemporaryDirectory() as path:
            source = pathlib.Path(path)
            target = source / "third_party/icu/BUILD.gn"
            shim = source / "build/linux/unbundle/icu.gn"
            target.parent.mkdir(parents=True)
            shim.parent.mkdir(parents=True)
            target.write_text("bundled\n")
            shim.write_text("system\n")
            subprocess.run(["git", "init", "-q", str(target.parent)], check=True)
            subprocess.run(["git", "-C", str(target.parent), "add", "BUILD.gn"], check=True)
            subprocess.run(["git", "-C", str(target.parent), "-c", "user.name=Test",
                            "-c", "user.email=test@example.invalid", "commit", "-qm", "fixture"],
                           check=True)
            script = shim.parent / "replace_gn_files.py"
            script.write_text(
                "import pathlib\n"
                "root = pathlib.Path(__file__).resolve().parents[3]\n"
                "target = root / 'third_party/icu/BUILD.gn'\n"
                "target.with_name('BUILD.gn.orig').write_bytes(target.read_bytes())\n"
                "target.write_bytes((pathlib.Path(__file__).parent / 'icu.gn').read_bytes())\n")
            (source / "example.txt").write_text("before\n")
            patch = source / "test.patch"
            patch.write_text("--- a/example.txt\n+++ b/example.txt\n@@ -1 +1 @@\n-before\n+after\n")
            with mock.patch.object(builder, "V8_ICU_PATCH", patch):
                original_run = builder.run
                def fail_patch(arguments, cwd=None, env=None):
                    if arguments[:2] == ["git", "apply"]:
                        raise subprocess.CalledProcessError(1, arguments)
                    return original_run(arguments, cwd, env)
                with mock.patch.object(builder, "run", side_effect=fail_patch):
                    with self.assertRaises(subprocess.CalledProcessError):
                        builder.configure_v8_system_icu(source)
                self.assertEqual(target.read_text(), "system\n")
                self.assertEqual(target.with_name("BUILD.gn.orig").read_text(), "bundled\n")
                self.assertEqual((source / "example.txt").read_text(), "before\n")
                builder.configure_v8_system_icu(source)
                self.assertEqual(target.read_text(), "system\n")
                self.assertEqual(target.with_name("BUILD.gn.orig").read_text(), "bundled\n")
                self.assertEqual((source / "example.txt").read_text(), "after\n")
                builder.configure_v8_system_icu(source)
                self.assertEqual((source / "example.txt").read_text(), "after\n")
                target.write_text("bundled\n")
                builder.configure_v8_system_icu(source)
                self.assertEqual(target.read_text(), "system\n")
                self.assertEqual(target.with_name("BUILD.gn.orig").read_text(), "bundled\n")
                (source / "example.txt").write_text("unexpected\n")
                with self.assertRaisesRegex(RuntimeError, "does not match"):
                    builder.configure_v8_system_icu(source)
                self.assertEqual(target.read_text(), "system\n")
                (source / "example.txt").write_text("before\n")
                target.write_text("local changes\n")
                with self.assertRaisesRegex(RuntimeError, "backup does not match"):
                    builder.configure_v8_system_icu(source)
                self.assertEqual(target.read_text(), "local changes\n")
                self.assertEqual((source / "example.txt").read_text(), "before\n")
                target.with_name("BUILD.gn.orig").unlink()
                with self.assertRaisesRegex(RuntimeError, "modified or lacks"):
                    builder.configure_v8_system_icu(source)
                self.assertEqual(target.read_text(), "local changes\n")
                target.write_text("bundled\n")
                shutil.rmtree(target.parent / ".git")
                with self.assertRaisesRegex(RuntimeError, "modified or lacks"):
                    builder.configure_v8_system_icu(source)
                self.assertEqual(target.read_text(), "bundled\n")

    def test_system_icu_identity_includes_backport_digest(self):
        with tempfile.TemporaryDirectory() as path:
            patch = pathlib.Path(path) / "v8.patch"
            patch.write_bytes(b"backport fixture")
            with mock.patch.object(builder, "V8_ICU_PATCH", patch), mock.patch.object(
                    builder, "system_icu_version", return_value="79.1"), mock.patch.object(
                    builder, "system_icu_libdir", return_value="lib64"):
                self.assertEqual(builder.system_icu_identity(), {
                    "linkage": "system", "version": "79.1", "libdir": "lib64",
                    "patch_sha256": hashlib.sha256(b"backport fixture").hexdigest()})
                patch.unlink()
                with self.assertRaises(FileNotFoundError):
                    builder.system_icu_identity()
            for name in ("system_icu_version", "system_icu_libdir"):
                with self.subTest(name=name), mock.patch.object(
                        builder, "system_icu_version", return_value="79.1"), mock.patch.object(
                        builder, "system_icu_libdir", return_value="lib64"), mock.patch.object(
                        builder, name, side_effect=RuntimeError("invalid ICU")):
                    with self.assertRaisesRegex(RuntimeError, "invalid ICU"):
                        builder.system_icu_identity()

    def test_system_icu_gn_arguments(self):
        for directory, cpu in (("lib64", "x64"), ("lib/aarch64-linux-gnu", "arm64")):
            with self.subTest(directory=directory, cpu=cpu):
                arguments = builder.v8_system_gn_args(directory, cpu)
                self.assertIn('use_sysroot=true', arguments)
                self.assertIn('target_sysroot="/"', arguments)
                self.assertIn('v8_depend_on_icu_data_file=false', arguments)
                self.assertIn('system_libdir=' + json.dumps(directory), arguments)
                self.assertIn('target_cpu=' + json.dumps(cpu), arguments)
        escaped = builder.v8_system_gn_args('lib" unexpected=true', 'x64')
        self.assertIn('system_libdir="lib\\" unexpected=true"', escaped)
        with self.assertRaisesRegex(ValueError, "supports Linux"):
            builder.v8_system_gn_args('lib', 'unsupported')

    def test_v8_system_icu_receipt_matches_only_its_build_environment(self):
        identity = {"linkage": "system", "version": "79.1", "libdir": "lib64",
                    "patch_sha256": "fixture digest"}
        with mock.patch.object(builder, "system_icu_identity", return_value=identity):
            for machine, cpu in (("x86_64", "x64"), ("aarch64", "arm64")):
                with self.subTest(machine=machine), mock.patch.object(
                        builder.platform, "machine", return_value=machine):
                    data = {"system_icu": identity,
                            "effective_gn_args": builder.v8_system_gn_args("lib64", cpu)}
                    self.assertTrue(builder.v8_system_receipt_matches(data))
                    for changed in ({}, {**data, "system_icu": {**identity, "version": "78.2"}},
                                    {**data, "system_icu": {**identity, "patch_sha256": "other"}},
                                    {**data, "effective_gn_args": "incorrect"}):
                        self.assertFalse(builder.v8_system_receipt_matches(changed))
            with mock.patch.object(builder.platform, "machine", return_value="unknown"):
                self.assertFalse(builder.v8_system_receipt_matches({}))
        for error in (RuntimeError("missing ICU"), FileNotFoundError()):
            with self.subTest(error=error), mock.patch.object(
                    builder, "system_icu_identity", side_effect=error):
                self.assertFalse(builder.v8_system_receipt_matches({}))

    def test_v8_archive_install_is_atomic_on_copy_failure(self):
        with tempfile.TemporaryDirectory() as path:
            directory = pathlib.Path(path)
            destination = directory / "libv8_monolith.a"
            source = directory / "source.a"
            destination.write_bytes(b"old archive")
            with mock.patch.object(builder.tempfile, "mkstemp", side_effect=PermissionError("injected")):
                with self.assertRaises(PermissionError):
                    builder.install_v8_archive(source, destination)
            close = builder.os.close
            def close_then_fail(descriptor):
                close(descriptor)
                raise OSError("injected close failure")
            with mock.patch.object(builder.os, "close", side_effect=close_then_fail):
                with self.assertRaises(OSError):
                    builder.install_v8_archive(source, destination)
            self.assertEqual(destination.read_bytes(), b"old archive")
            self.assertEqual(sorted(p.name for p in directory.iterdir()), ["libv8_monolith.a"])
            with self.assertRaises(FileNotFoundError):
                builder.install_v8_archive(source, destination)
            self.assertEqual(destination.read_bytes(), b"old archive")
            self.assertEqual(sorted(p.name for p in directory.iterdir()), ["libv8_monolith.a"])
            source.write_bytes(b"new archive")
            with mock.patch.object(builder.os, "replace", side_effect=PermissionError("injected")):
                with self.assertRaises(PermissionError):
                    builder.install_v8_archive(source, destination)
            self.assertEqual(destination.read_bytes(), b"old archive")
            self.assertEqual(sorted(p.name for p in directory.iterdir()),
                             ["libv8_monolith.a", "source.a"])
            builder.install_v8_archive(source, destination)
            self.assertEqual(destination.read_bytes(), b"new archive")
            self.assertEqual(sorted(p.name for p in directory.iterdir()),
                             ["libv8_monolith.a", "source.a"])

    def test_v8_build_records_environment_and_rejects_icu_changes(self):
        identity = {"linkage": "system", "version": "79.1", "libdir": "lib64",
                    "patch_sha256": "fixture"}
        with tempfile.TemporaryDirectory() as path:
            directory = pathlib.Path(path)
            work, prefix = directory / "work", directory / "prefix"
            source = work / "v8-tree/v8"
            (source / "out/vellum/obj").mkdir(parents=True)
            (source / "out/vellum/obj/libv8_monolith.a").write_bytes(b"new")
            (source / "include").mkdir()
            (source / "include/v8.h").write_text("header")
            (source / "out/vellum/gen/include").mkdir(parents=True)
            (source / "out/vellum/gen/include/generated.h").write_text("generated")
            (prefix / "lib").mkdir(parents=True)
            installed = prefix / "lib/libv8_monolith.a"
            installed.write_bytes(b"old")
            with mock.patch.object(builder, "checkout"), mock.patch.object(
                    builder, "configure_v8_system_icu") as configure, mock.patch.object(
                    builder, "copy_licenses"), mock.patch.object(builder, "run") as run, mock.patch.object(
                    builder.subprocess, "check_output", return_value="fixture-commit\n"), mock.patch.object(
                    builder.platform, "machine", return_value="x86_64"), mock.patch.object(
                    builder, "system_icu_identity", side_effect=[identity, {**identity, "version": "80"}]):
                with self.assertRaisesRegex(RuntimeError, "changed during the build"):
                    builder.build_v8(work, prefix, 2)
                self.assertEqual(installed.read_bytes(), b"old")
                self.assertFalse((prefix / "share/vellum/v8.json").exists())
                configure.assert_called_once_with(source)
                self.assertTrue(any(call.args[0][0] == "gn" for call in run.call_args_list))
            with mock.patch.object(builder, "checkout"), mock.patch.object(
                    builder, "configure_v8_system_icu"), mock.patch.object(builder, "copy_licenses"), mock.patch.object(
                    builder, "run"), mock.patch.object(builder.subprocess, "check_output", return_value="fixture-commit\n"), mock.patch.object(
                    builder.platform, "machine", return_value="x86_64"), mock.patch.object(
                    builder, "system_icu_identity", return_value=identity):
                builder.build_v8(work, prefix, 2)
            self.assertEqual(installed.read_bytes(), b"new")
            data = json.loads((prefix / "share/vellum/v8.json").read_text())
            self.assertEqual(data["system_icu"], identity)
            self.assertEqual(data["effective_gn_args"], builder.v8_system_gn_args("lib64", "x64"))
            self.assertEqual((prefix / "include/v8/v8.h").read_text(), "header")
            self.assertEqual((prefix / "include/v8/generated.h").read_text(), "generated")
            with mock.patch.object(builder, "system_icu_identity", return_value=identity), mock.patch.object(
                    builder.platform, "machine", return_value="unsupported"):
                with self.assertRaisesRegex(SystemExit, "supports Linux"):
                    builder.build_v8(work, prefix, 2)
    def test_explicit_invalid_root_fails_without_building(self):
        with tempfile.TemporaryDirectory() as path:
            directory = pathlib.Path(path)
            work = directory / "work"
            selected = directory / "missing-explicit-root"
            result = subprocess.run(
                [sys.executable, str(root / "scripts/prepare_dependencies.py"),
                 "--prefix", str(directory / "install"), "--work", str(work),
                 "--mupdf-root", str(selected)],
                capture_output=True, text=True, timeout=10, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("supplied root lacks compatible", result.stderr)
            self.assertFalse(selected.exists())
            self.assertEqual(list(work.iterdir()), [])

    def test_explicit_wrong_commit_is_rejected(self):
        with tempfile.TemporaryDirectory() as path:
            directory = pathlib.Path(path)
            selected = directory / "external"
            for name in ("lib/libmupdf.a", "lib/libmupdf-third.a", "include/mupdf/fitz.h", "include/mupdf/pdf.h"):
                target = selected / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(b"fixture")
            receipt = selected / "share/vellum/mupdf.json"
            receipt.parent.mkdir(parents=True)
            receipt.write_text(json.dumps({"version": builder.LOCK["mupdf"]["version"],
                                           "commit": "wrong", "machine": builder.platform.machine()}))
            work = directory / "work"
            result = subprocess.run(
                [sys.executable, str(root / "scripts/prepare_dependencies.py"),
                 "--prefix", str(directory / "install"), "--work", str(work),
                 "--mupdf-root", str(selected)],
                capture_output=True, text=True, timeout=10, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("supplied root lacks compatible", result.stderr)
            self.assertEqual(list(work.iterdir()), [])

    def test_explicit_v8_root_is_checked_after_valid_mupdf(self):
        with tempfile.TemporaryDirectory() as path:
            directory = pathlib.Path(path)
            selected = directory / "mupdf"
            for name in ("lib/libmupdf.a", "lib/libmupdf-third.a", "include/mupdf/fitz.h", "include/mupdf/pdf.h"):
                target = selected / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(b"fixture")
            receipt = selected / "share/vellum/mupdf.json"
            receipt.parent.mkdir(parents=True)
            receipt.write_text(json.dumps({"version": builder.LOCK["mupdf"]["version"],
                                           "commit": builder.LOCK["mupdf"]["revision"],
                                           "machine": builder.platform.machine(),
                                           "make_args": builder.LOCK["mupdf_make_args"],
                                           "build_output": builder.LOCK["mupdf_build_output"],
                                           "headers": ["include/mupdf/fitz.h", "include/mupdf/pdf.h"]}))
            work = directory / "work"
            result = subprocess.run(
                [sys.executable, str(root / "scripts/prepare_dependencies.py"),
                 "--prefix", str(directory / "install"), "--work", str(work),
                 "--mupdf-root", str(selected), "--v8-root", str(directory / "missing-v8")],
                capture_output=True, text=True, timeout=10, check=False)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Reusing compatible mupdf", result.stdout)
            self.assertIn("v8: supplied root lacks compatible", result.stderr)
            self.assertEqual(list(work.iterdir()), [])

    def test_missing_and_invalid_receipts(self):
        with tempfile.TemporaryDirectory() as path:
            prefix = pathlib.Path(path)
            self.assertFalse(builder.compatible(prefix, "mupdf"))
            receipt = prefix / "share/vellum/mupdf.json"
            receipt.parent.mkdir(parents=True)
            receipt.write_text("broken")
            self.assertFalse(builder.compatible(prefix, "mupdf"))
    @mock.patch.object(builder.platform, "machine", return_value="x86_64")
    @mock.patch.object(builder, "system_icu_identity", return_value={
        "linkage": "system", "version": "79.1", "libdir": "lib64", "patch_sha256": "fixture"})
    def test_static_install_contract(self, identity, machine):
        with tempfile.TemporaryDirectory() as path:
            prefix = pathlib.Path(path)
            for name in ("lib/libv8_monolith.a", "include/v8/v8.h"):
                target = prefix / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(b"fixture")
            data = {"version": builder.LOCK["v8"]["version"], "commit": builder.LOCK["v8"]["revision"],
                    "machine": builder.platform.machine(),
                    "headers": ["include/v8/v8.h"],
                    "gn_args": builder.LOCK["v8_gn_args"], "system_icu": identity.return_value,
                    "effective_gn_args": builder.v8_system_gn_args("lib64", "x64")}
            receipt = prefix / "share/vellum/v8.json"
            receipt.parent.mkdir(parents=True)
            receipt.write_text(json.dumps(data))
            self.assertTrue(builder.compatible(prefix, "v8"))
            bundled = {k: v for k, v in data.items() if k != "system_icu"}
            receipt.write_text(json.dumps(bundled))
            self.assertFalse(builder.compatible(prefix, "v8"))
            data["commit"] = "wrong-commit"
            receipt.write_text(json.dumps(data))
            self.assertFalse(builder.compatible(prefix, "v8"))
            data["commit"] = builder.LOCK["v8"]["revision"]
            data["gn_args"] = "incompatible pointer compression"
            receipt.write_text(json.dumps(data))
            self.assertFalse(builder.compatible(prefix, "v8"))
            data["gn_args"] = builder.LOCK["v8_gn_args"]
            data["machine"] = "wrong-machine"
            receipt.write_text(json.dumps(data))
            self.assertFalse(builder.compatible(prefix, "v8"))
    def test_mupdf_requires_pinned_commit(self):
        with tempfile.TemporaryDirectory() as path:
            prefix = pathlib.Path(path)
            for name in ("lib/libmupdf.a", "lib/libmupdf-third.a", "include/mupdf/fitz.h", "include/mupdf/pdf.h"):
                target = prefix / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(b"fixture")
            receipt = prefix / "share/vellum/mupdf.json"
            receipt.parent.mkdir(parents=True)
            data = {"version": builder.LOCK["mupdf"]["version"],
                    "machine": builder.platform.machine(),
                    "make_args": builder.LOCK["mupdf_make_args"],
                    "build_output": builder.LOCK["mupdf_build_output"],
                    "headers": ["include/mupdf/fitz.h", "include/mupdf/pdf.h"]}
            receipt.write_text(json.dumps(data))
            self.assertFalse(builder.compatible(prefix, "mupdf"))
            data["commit"] = builder.LOCK["mupdf"]["revision"]
            receipt.write_text(json.dumps(data))
            self.assertTrue(builder.compatible(prefix, "mupdf"))
if __name__ == "__main__":
    unittest.main()
