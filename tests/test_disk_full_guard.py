#!/usr/bin/env python3
"""Verify the disk-full test refuses to exhaust an ordinary directory."""
import os
import pathlib
import resource
import signal
import subprocess
import sys
import tempfile
import unittest

PROGRAM = sys.argv.pop(1)
RUNNER = sys.argv.pop(1)


def limit_child_file_size():
    # If the guard regresses, the negative test must still not fill the host disk.
    resource.setrlimit(resource.RLIMIT_FSIZE, (8192, 8192))
    signal.signal(signal.SIGXFSZ, signal.SIG_IGN)


class DiskFullGuardTests(unittest.TestCase):
    def test_unconfigured_and_unsafe_directories(self):
        env = os.environ.copy()
        env.pop("VELLUM_DISK_FULL_TEST_DIR", None)
        # Prevent gcov output from exceeding the guard's small file-size limit.
        env["GCOV_PREFIX"] = "/dev/null"
        result = subprocess.run([PROGRAM], env=env, capture_output=True, check=False,
                                preexec_fn=limit_child_file_size, timeout=10)
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"not configured", result.stderr)
        with tempfile.TemporaryDirectory() as directory:
            marker = pathlib.Path(directory) / "preserve"
            marker.write_text("unchanged")
            env["VELLUM_DISK_FULL_TEST_DIR"] = directory
            result = subprocess.run([PROGRAM], env=env, capture_output=True, check=False,
                                    preexec_fn=limit_child_file_size, timeout=10)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(marker.read_text(), "unchanged")
            self.assertEqual(list(pathlib.Path(directory).iterdir()), [marker])
            result = subprocess.run([sys.executable, RUNNER, PROGRAM], env=env,
                                    capture_output=True, check=False,
                                    preexec_fn=limit_child_file_size, timeout=10)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(marker.read_text(), "unchanged")
            self.assertEqual(list(pathlib.Path(directory).iterdir()), [marker])


if __name__ == "__main__":
    unittest.main()
