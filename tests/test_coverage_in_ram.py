#!/usr/bin/env python3
"""Exercise RAM coverage reporting against an independently built gcov fixture."""
import importlib.util
import contextlib
import io
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

PROJECT = pathlib.Path(__file__).resolve().parents[1]
PROGRAM = PROJECT / "scripts" / "coverage_in_ram.py"


class CoverageInRamTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="vellum-coverage-test-")
        cls.root = pathlib.Path(cls.temporary.name)
        (cls.root / "main.cc").write_text(
            '#include "vellum/view_zoom.hh"\n#include <iostream>\n#include <string_view>\n'
            'int main(int argc, char **argv) {\n'
            '  if (argc > 1 && std::string_view(argv[1]) == "badgcov") {\n'
            '    std::cerr << "libgcov profiling error: simulated write failure\\n"; return 0; }\n'
            '  if (argc > 1 && std::string_view(argv[1]) == "fail") return 1;\n'
            '  return vellum::calculate_view_scale(vellum::zoom_mode::percentage, '
            '{200, 200}, {100, 100}, 100) == 1 ? 0 : 1;\n}\n')
        (cls.root / "CMakeLists.txt").write_text(
            'cmake_minimum_required(VERSION 3.25)\nproject(coverage_fixture LANGUAGES CXX)\n'
            'enable_testing()\nadd_executable(probe main.cc "' + str(PROJECT / 'src/view_zoom.cc') + '")\n'
            'target_include_directories(probe PRIVATE "' + str(PROJECT / 'include') + '")\n'
            'target_compile_features(probe PRIVATE cxx_std_23)\n'
            'target_compile_options(probe PRIVATE --coverage -O0 -g)\n'
            'target_link_options(probe PRIVATE --coverage)\n'
            'add_test(NAME sample COMMAND probe)\n'
            'add_test(NAME failing COMMAND probe fail)\n'
            'add_test(NAME badgcov COMMAND probe badgcov)\n')
        cls.build = cls.root / "build"
        subprocess.run(["cmake", "-S", str(cls.root), "-B", str(cls.build)],
                       capture_output=True, text=True, check=True, timeout=30)
        subprocess.run(["cmake", "--build", str(cls.build), "--parallel", "2"],
                       capture_output=True, text=True, check=True, timeout=60)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def invoke(self, regex, *extra):
        ram_root = os.environ.get("VELLUM_COVERAGE_RAM_ROOT", "/dev/shm")
        result = subprocess.run([sys.executable, PROGRAM, "--build", self.build,
                                 "--ram-root", ram_root, "--regex", regex, *extra],
                                capture_output=True, text=True, timeout=90, check=False)
        directories = [line.partition("=")[2] for line in result.stdout.splitlines()
                       if line.startswith("VELLUM_RAM_COVERAGE_DIR=")]
        self.addCleanup(lambda: [shutil.rmtree(path, ignore_errors=True) for path in directories])
        return result, pathlib.Path(directories[0]) if directories else None

    def test_real_counters_and_persistent_reports(self):
        output = self.root / "persisted"
        result, workspace = self.invoke("^sample$", "--persist", str(output))
        self.assertEqual(result.returncode, 0, result.stderr + result.stdout)
        self.assertTrue(list(workspace.rglob("*.gcda")))
        self.assertTrue(list(workspace.rglob("*.gcno")))
        summary = json.loads((output / "summary.json").read_text())
        self.assertGreater(summary["line_covered"], 0)
        self.assertGreater(summary["branch_covered"], 0)
        self.assertTrue((output / "index.html").is_file())
        self.assertTrue((workspace / "reports" / "branches.txt").is_file())
        marker = output / "marker"
        marker.write_text("keep")
        rejected, _ = self.invoke("^sample$", "--persist", str(output))
        self.assertEqual(rejected.returncode, 2)
        self.assertEqual(marker.read_text(), "keep")

    def test_failing_missing_and_corrupt_counter_diagnostics(self):
        failed, workspace = self.invoke("^failing$")
        self.assertNotEqual(failed.returncode, 0)
        self.assertTrue((workspace / "reports" / "summary.json").exists())
        missing, _ = self.invoke("^does-not-exist$")
        self.assertNotEqual(missing.returncode, 0)
        corrupt, _ = self.invoke("^badgcov$")
        self.assertNotEqual(corrupt.returncode, 0)
        self.assertIn("counter write error", corrupt.stderr)

    def test_insufficient_space_is_rejected_before_testing(self):
        spec = importlib.util.spec_from_file_location("vellum_coverage_in_ram", PROGRAM)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        usage = shutil.disk_usage("/dev/shm")._replace(free=1)
        with mock.patch.object(sys, "argv", [str(PROGRAM), "--build", str(self.build)]), \
                mock.patch.object(module.shutil, "disk_usage", return_value=usage), \
                mock.patch("sys.stderr"):
            with self.assertRaises(SystemExit) as error:
                module.main()
        self.assertEqual(error.exception.code, 2)

    def test_setup_rejects_missing_tools_notes_and_non_ram_storage(self):
        spec = importlib.util.spec_from_file_location("vellum_coverage_setup", PROGRAM)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        cases = [
            (["--build", "/definitely/missing"], {}, "existing directories"),
            (["--build", str(self.build)], {"which": [None]}, "stat is required"),
            (["--build", str(self.build)], {"filesystem": "ext4"}, "tmpfs or ramfs"),
            (["--build", str(self.build)], {"which": ["/usr/bin/stat", None], "no_sibling": True},
             "gcovr is required"),
        ]
        empty = self.root / "empty-build"
        empty.mkdir(exist_ok=True)
        cases.append((["--build", str(empty)], {}, "no gcov notes"))
        for arguments, settings, message in cases:
            with self.subTest(message=message), contextlib.ExitStack() as stack:
                stack.enter_context(mock.patch.object(sys, "argv", [str(PROGRAM), *arguments]))
                stderr = stack.enter_context(contextlib.redirect_stderr(io.StringIO()))
                if "which" in settings:
                    stack.enter_context(mock.patch.object(module.shutil, "which",
                                                          side_effect=settings["which"]))
                if settings.get("no_sibling"):
                    stack.enter_context(mock.patch.object(pathlib.Path, "is_file", return_value=False))
                if "filesystem" in settings:
                    stack.enter_context(mock.patch.object(module.subprocess, "run", return_value=
                                                          subprocess.CompletedProcess([], 0,
                                                                                      stdout="ext4\n")))
                with self.assertRaises(SystemExit) as error:
                    module.main()
                self.assertEqual(error.exception.code, 2)
                self.assertIn(message, stderr.getvalue())

    def test_gcovr_sibling_fallback_and_missing_ctest_log(self):
        spec = importlib.util.spec_from_file_location("vellum_coverage_fallback", PROGRAM)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        usage = shutil.disk_usage("/dev/shm")._replace(free=1)
        with mock.patch.object(sys, "argv", [str(PROGRAM), "--build", str(self.build)]), \
                mock.patch.object(module.shutil, "which", side_effect=["/usr/bin/stat", None]), \
                mock.patch.object(module.shutil, "disk_usage", return_value=usage), \
                contextlib.redirect_stderr(io.StringIO()) as errors:
            with self.assertRaises(SystemExit) as error:
                module.main()
        self.assertEqual(error.exception.code, 2)
        self.assertIn("RAM filesystem", errors.getvalue())

        fake_build = self.root / "no-ctest-log"
        fake_build.mkdir(exist_ok=True)
        (fake_build / "probe.gcno").touch()
        original_run = subprocess.run

        def no_ctest(command, **kwargs):
            if command[0] == "ctest":
                return subprocess.CompletedProcess(command, 0)
            return original_run(command, **kwargs)

        with mock.patch.object(sys, "argv", [str(PROGRAM), "--build", str(fake_build)]), \
                mock.patch.object(module.subprocess, "run", side_effect=no_ctest), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            with self.assertRaisesRegex(RuntimeError, "detailed test log"):
                module.main()
        for line in output.getvalue().splitlines():
            if line.startswith("VELLUM_RAM_COVERAGE_DIR="):
                shutil.rmtree(line.partition("=")[2], ignore_errors=True)

    def test_cpu_fallback_and_unfiltered_test_run(self):
        spec = importlib.util.spec_from_file_location("vellum_coverage_cpu", PROGRAM)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with mock.patch.object(sys, "argv", [str(PROGRAM), "--build", str(self.build)]), \
                mock.patch.object(module.os, "sched_getaffinity", side_effect=AttributeError), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            with self.assertRaisesRegex(RuntimeError, "counter write error"):
                module.main()
        for line in output.getvalue().splitlines():
            if line.startswith("VELLUM_RAM_COVERAGE_DIR="):
                shutil.rmtree(line.partition("=")[2], ignore_errors=True)


if __name__ == "__main__":
    unittest.main()
