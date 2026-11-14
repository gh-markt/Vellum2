#!/usr/bin/env python3
"""Check dependency reuse, repair and interrupted preparation."""
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("dependency_builder", ROOT / "scripts/prepare_dependencies.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)


class DependencyRepairTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.prefix = self.root / "install"
        self.work = self.root / "work"
        self.headers = {
            "mupdf": ["include/mupdf/fitz.h", "include/mupdf/pdf.h", "include/mupdf/nested/extra.h"],
            "v8": ["include/v8/v8.h", "include/v8/v8-extra.h"],
        }
        self.libraries = {
            "mupdf": ["lib/libmupdf.a", "lib/libmupdf-third.a"],
            "v8": ["lib/libv8_monolith.a"],
        }
        for kind in ("mupdf", "v8"):
            self.install(kind)

    def install(self, kind):
        for name in self.headers[kind] + self.libraries[kind]:
            path = self.prefix / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("fixture")
        receipt = {"version": builder.LOCK[kind]["version"],
                   "commit": builder.LOCK[kind]["revision"],
                   "machine": builder.platform.machine(), "headers": self.headers[kind]}
        if kind == "mupdf":
            receipt["make_args"] = builder.LOCK["mupdf_make_args"]
            receipt["build_output"] = builder.LOCK["mupdf_build_output"]
        else:
            receipt["gn_args"] = builder.LOCK["v8_gn_args"]
        path = self.prefix / f"share/vellum/{kind}.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(receipt))

    def prepare(self):
        args = ["prepare_dependencies.py", "--prefix", str(self.prefix), "--work", str(self.work)]
        with mock.patch.object(sys, "argv", args), mock.patch.object(
                builder, "v8_system_receipt_matches", return_value=True):
            builder.main()

    def test_reuse_missing_nested_header_and_interrupted_repair(self):
        with mock.patch.object(builder, "build_mupdf") as rebuild, mock.patch.object(builder, "build_v8") as v8:
            self.prepare()
            rebuild.assert_not_called()
            v8.assert_not_called()
            (self.prefix / self.headers["mupdf"][-1]).unlink()
            rebuild.side_effect = RuntimeError("interrupted")
            with self.assertRaisesRegex(RuntimeError, "interrupted"):
                self.prepare()
            self.assertFalse((self.prefix / "share/vellum/mupdf.json").exists())
            rebuild.side_effect = lambda *args: self.install("mupdf")
            self.prepare()
            self.assertTrue((self.prefix / self.headers["mupdf"][-1]).is_file())
            self.prepare()
            self.assertEqual(rebuild.call_count, 2)
            v8.assert_not_called()

    def test_changed_options_and_missing_v8_header_trigger_independent_repairs(self):
        lock = dict(builder.LOCK)
        lock["mupdf_make_args"] = builder.LOCK["mupdf_make_args"] + ["HAVE_CURL=yes"]
        (self.prefix / self.headers["v8"][-1]).unlink()
        with mock.patch.object(builder, "LOCK", lock), mock.patch.object(
                builder, "build_mupdf", side_effect=lambda *args: self.install("mupdf")) as mupdf, \
             mock.patch.object(builder, "build_v8", side_effect=lambda *args: self.install("v8")) as v8:
            self.prepare()
            self.prepare()
            self.assertEqual(mupdf.call_count, 1)
            self.assertEqual(v8.call_count, 1)

    def test_malformed_manifest_does_not_escape_prefix(self):
        path = self.prefix / "share/vellum/mupdf.json"
        data = json.loads(path.read_text())
        for bad in ([], ["include/mupdf/../../outside.h"], [None], "not-a-list"):
            data["headers"] = bad
            path.write_text(json.dumps(data))
            self.assertFalse(builder.compatible(self.prefix, "mupdf"))

    def test_receipt_records_headers_and_build_options(self):
        with mock.patch.object(builder.subprocess, "check_output",
                               return_value=builder.LOCK["mupdf"]["revision"] + "\n"):
            builder.receipt(self.prefix, "mupdf", self.root)
        data = json.loads((self.prefix / "share/vellum/mupdf.json").read_text())
        self.assertEqual(data["headers"], sorted(self.headers["mupdf"]))
        self.assertEqual(data["make_args"], builder.LOCK["mupdf_make_args"])
        self.assertEqual(data["build_output"], builder.LOCK["mupdf_build_output"])
        self.assertTrue(builder.compatible(self.prefix, "mupdf"))

    def test_mupdf_cleans_old_objects_before_rebuilding(self):
        with mock.patch.object(builder, "checkout"), mock.patch.object(builder, "run") as run, \
             mock.patch.object(builder.shutil, "copy2"), mock.patch.object(builder.shutil, "copytree"), \
             mock.patch.object(builder, "copy_licenses"), mock.patch.object(builder, "receipt"):
            builder.build_mupdf(self.work, self.prefix, 3)
        commands = [call.args[0] for call in run.call_args_list]
        self.assertEqual(commands[1], ["make", *builder.LOCK["mupdf_make_args"], "clean"])
        self.assertEqual(commands[2], ["make", "-j3", *builder.LOCK["mupdf_make_args"], "libs"])

    def test_checkout_clones_once_and_updates_pinned_revision(self):
        checkout = self.root / "checkout"
        revision = builder.LOCK["mupdf"]["revision"]
        with mock.patch.object(builder, "run") as run:
            builder.checkout("mupdf", checkout)
            self.assertEqual(run.call_args_list[0].args[0],
                             ["git", "clone", "--no-checkout", builder.LOCK["mupdf"]["url"], checkout])
            self.assertEqual(run.call_args_list[1].args[0],
                             ["git", "fetch", "--depth=1", "origin", revision])
            self.assertEqual(run.call_args_list[2].args[0], ["git", "checkout", "--detach", "FETCH_HEAD"])
            (checkout / ".git").mkdir(parents=True)
            run.reset_mock()
            builder.checkout("mupdf", checkout)
            self.assertEqual(run.call_count, 2)
            self.assertEqual(run.call_args_list[0].args[0][1], "fetch")

    def test_copy_licenses_preserves_paths_and_excludes_builds(self):
        source = self.root / "source"
        output = self.root / "licenses"
        for name in ("LICENSE", "nested/COPYING", "out/LICENSE", ".git/LICENSE", "nested/code.cc"):
            path = source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(name)
        output.mkdir()
        (output / "LICENSE").write_text("old read-only license")
        (output / "LICENSE").chmod(0o444)
        builder.copy_licenses(source, output)
        self.assertEqual((output / "LICENSE").read_text(), "LICENSE")
        self.assertEqual((output / "nested/COPYING").read_text(), "nested/COPYING")
        self.assertFalse((output / "out/LICENSE").exists())
        self.assertFalse((output / ".git/LICENSE").exists())
        self.assertFalse((output / "nested/code.cc").exists())

    def test_license_copy_failure_preserves_old_file_and_cleans_temporary(self):
        source = self.root / "LICENSE"
        target = self.root / "installed-license"
        source.write_text("new license")
        target.write_text("old license")
        target.chmod(0o444)
        for owner, method in ((builder.os, "replace"), (builder.shutil, "copy2")):
            with self.subTest(operation=method), \
                 mock.patch.object(owner, method, side_effect=PermissionError("injected")):
                with self.assertRaises(PermissionError):
                    builder._copy_file_atomic(source, target)
            self.assertEqual(target.read_text(), "old license")
            self.assertEqual(target.stat().st_mode & 0o777, 0o444)
            self.assertFalse(list(self.root.glob(".vellum-license-*")))
        builder._copy_file_atomic(source, target)
        self.assertEqual(target.read_text(), "new license")

    def test_cmake_rechecks_and_repairs_missing_header(self):
        source = self.root / "cmake-source"
        (source / "scripts").mkdir(parents=True)
        (source / "docs").mkdir()
        (source / "docs/dependencies.lock.json").write_bytes(
            (ROOT / "docs/dependencies.lock.json").read_bytes())
        (source / "cmake").mkdir()
        (source / "cmake/FindPkgConfig.cmake").write_text(
            'set(PkgConfig_FOUND TRUE)\n'
            'function(pkg_check_modules)\n'
            '  add_library(PkgConfig::VELLUM_SYSTEM_ICU INTERFACE IMPORTED)\n'
            'endfunction()\n')
        (source / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.25)\n'
            'project(dependency_fixture LANGUAGES NONE)\n'
            f'set(Python3_EXECUTABLE "{sys.executable}")\n'
            'set(VELLUM_DEPS_PREFIX "${PROJECT_BINARY_DIR}/install")\n'
            'if(NOT DEFINED VELLUM_DEPS_WORK)\n'
            '  set(VELLUM_DEPS_WORK "${PROJECT_BINARY_DIR}/work")\n'
            'endif()\n'
            'set(VELLUM_DEPENDENCY_JOBS 1)\n'
            'set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")\n'
            'add_library(Threads::Threads INTERFACE IMPORTED)\n'
            f'include("{ROOT / "cmake/Dependencies.cmake"}")\n')
        (source / "scripts/prepare_dependencies.py").write_text(
            'import pathlib, sys\n'
            'prefix = pathlib.Path(sys.argv[sys.argv.index("--prefix") + 1])\n'
            'prefix.mkdir(parents=True, exist_ok=True)\n'
            'counter = prefix / "calls"\n'
            'counter.write_text(str(int(counter.read_text()) + 1 if counter.exists() else 1))\n'
            'for name in ["lib/libmupdf.a", "lib/libmupdf-third.a", "lib/libv8_monolith.a", '
            '"include/mupdf/fitz.h", "include/mupdf/pdf.h", "include/v8/v8.h"]:\n'
            '    path = prefix / name\n'
            '    path.parent.mkdir(parents=True, exist_ok=True)\n'
            '    if not path.exists(): path.write_text("fixture")\n')
        generators = ["Unix Makefiles"] + (["Ninja"] if shutil.which("ninja") else [])
        for index, generator in enumerate(generators):
            with self.subTest(generator=generator):
                build = self.root / f"cmake-build-{index}"
                subprocess.run(["cmake", "-S", str(source), "-B", str(build), "-G", generator],
                               check=True, capture_output=True, text=True)
                def run_build():
                    subprocess.run(["cmake", "--build", str(build), "--target", "vellum_dependencies"],
                                   check=True, capture_output=True, text=True)
                run_build()
                run_build()
                header = build / "install/include/mupdf/pdf.h"
                header.unlink()
                run_build()
                self.assertEqual((build / "install/calls").read_text(), "3")
                self.assertEqual(header.read_text(), "fixture")
                managed = build / "work/v8-tree/v8/out/vellum/obj/example.o"
                other = build / "work/v8-tree/v8/out/other/keep.o"
                source_file = build / "work/v8-tree/v8/src/user.txt"
                for path in (managed, other, source_file):
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("keep except managed output")
                subprocess.run(["cmake", "--build", str(build), "--target", "clean"], check=True,
                               capture_output=True, text=True)
                artifacts = ("lib/libmupdf.a", "lib/libmupdf-third.a", "lib/libv8_monolith.a",
                             "include/mupdf/fitz.h", "include/mupdf/pdf.h", "include/v8/v8.h")
                for name in artifacts:
                    self.assertFalse((build / "install" / name).exists(), name)
                self.assertFalse(managed.exists())
                self.assertTrue(other.is_file())
                self.assertTrue(source_file.is_file())
                run_build()
                self.assertEqual((build / "install/calls").read_text(), "4")
                for name in artifacts:
                    self.assertEqual((build / "install" / name).read_text(), "fixture")
        external_build = self.root / "cmake-build-external"
        subprocess.run(["cmake", "-S", str(source), "-B", str(external_build),
                        f"-DVELLUM_V8_ROOT={self.prefix}"], check=True,
                       capture_output=True, text=True)
        unrelated = external_build / "work/v8-tree/v8/out/vellum/keep.o"
        unrelated.parent.mkdir(parents=True)
        unrelated.write_text("external root must not clean managed work")
        subprocess.run(["cmake", "--build", str(external_build), "--target", "clean"],
                       check=True, capture_output=True, text=True)
        self.assertTrue(unrelated.is_file())
        unsafe = self.root / "do-not-delete"
        unsafe.mkdir()
        marker = unsafe / "keep"
        marker.write_text("user data")
        rejected = subprocess.run(
            ["cmake", "-S", str(source), "-B", str(self.root / "cmake-build-unsafe"),
             f"-DVELLUM_DEPS_WORK={unsafe};unexpected"], capture_output=True, text=True, check=False)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("cannot contain semicolons", rejected.stderr)
        self.assertEqual(marker.read_text(), "user data")


if __name__ == "__main__":
    unittest.main()
