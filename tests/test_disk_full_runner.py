#!/usr/bin/env python3
"""Verify bounded-filesystem provisioning and cleanup failure handling."""
import contextlib
import importlib.util
import io
import os
import pathlib
import subprocess
import sys
import unittest
from types import SimpleNamespace
from unittest import mock

PROGRAM = pathlib.Path(__file__).resolve().parents[1] / "scripts" / "run_disk_full_test.py"
spec = importlib.util.spec_from_file_location("vellum_disk_full_runner", PROGRAM)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


def result(code):
    return SimpleNamespace(returncode=code)


class DiskFullRunnerTests(unittest.TestCase):
    def test_missing_mount_tools(self):
        for tools in ((None, "/bin/umount"), ("/bin/mount", None)):
            with self.subTest(tools=tools), mock.patch.object(runner.shutil, "which", side_effect=tools), \
                    mock.patch.object(runner.subprocess, "run") as execute, \
                    contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(runner.mounted_test("/safe", "/test"), 2)
                execute.assert_not_called()

    def test_mount_failure_never_runs_test_or_unmounts(self):
        with mock.patch.object(runner.shutil, "which", side_effect=["/bin/mount", "/bin/umount"]), \
                mock.patch.object(runner.subprocess, "run", return_value=result(9)) as execute, \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(runner.mounted_test("/safe", "/test"), 9)
            self.assertEqual(execute.call_count, 1)

    def test_result_and_unmount_failure(self):
        for test_code, unmount_code, expected in ((0, 0, 0), (7, 0, 7), (0, 8, 8), (7, 8, 7)):
            with self.subTest(test=test_code, unmount=unmount_code), \
                    mock.patch.object(runner.shutil, "which", side_effect=["/bin/mount", "/bin/umount"]), \
                    mock.patch.object(runner.subprocess, "run", side_effect=[result(0), result(test_code),
                                                                            result(unmount_code)]) as execute, \
                    contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(runner.mounted_test("/safe", "/test"), expected)
                self.assertEqual(execute.call_count, 3)
                self.assertEqual(execute.call_args_list[1].kwargs["env"]["VELLUM_DISK_FULL_TEST_DIR"],
                                 "/safe")
                self.assertEqual(execute.call_args_list[2].args[0], ["/bin/umount", "/safe"])

    def test_unmount_on_test_execution_error(self):
        with mock.patch.object(runner.shutil, "which", side_effect=["/bin/mount", "/bin/umount"]), \
                mock.patch.object(runner.subprocess, "run", side_effect=[result(0), OSError("exec failed"),
                                                                        result(0)]) as execute:
            with self.assertRaisesRegex(OSError, "exec failed"):
                runner.mounted_test("/safe", "/test")
            self.assertEqual(execute.call_args_list[2].args[0], ["/bin/umount", "/safe"])

    def test_argument_and_namespace_failures(self):
        with mock.patch.object(sys, "argv", [str(PROGRAM)]), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(runner.main(), 2)
        with mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]), \
                mock.patch.dict(os.environ, {"VELLUM_DISK_FULL_TEST_DIR": ""}), \
                mock.patch.object(runner.shutil, "which", return_value=None), \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(runner.main(), 2)
        with mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]), \
                mock.patch.dict(os.environ, {"VELLUM_DISK_FULL_TEST_DIR": ""}), \
                mock.patch.object(runner.shutil, "which", return_value="/bin/unshare"), \
                mock.patch.object(runner.subprocess, "run", return_value=result(4)), \
                contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(runner.main(), 4)

    def test_supplied_mount_and_internal_entry(self):
        with mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]), \
                mock.patch.dict(os.environ, {"VELLUM_DISK_FULL_TEST_DIR": "/provided"}), \
                mock.patch.object(runner.subprocess, "run", return_value=result(3)) as execute:
            self.assertEqual(runner.main(), 3)
            self.assertEqual(execute.call_args.args[0], ["/test"])
        with mock.patch.object(sys, "argv", [str(PROGRAM), "--mounted", "/safe", "/test"]), \
                mock.patch.object(runner, "mounted_test", return_value=5) as run:
            self.assertEqual(runner.main(), 5)
            run.assert_called_once_with("/safe", "/test")


if __name__ == "__main__":
    unittest.main()
