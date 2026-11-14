#!/usr/bin/env python3
"""Verify private headless displays and test-process lifecycle."""
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time
import unittest

RUNNER = pathlib.Path(__file__).resolve().parents[1] / "scripts" / "run_headless_test.py"


def run_test(*command, **kwargs):
    return subprocess.run([sys.executable, RUNNER, *command], capture_output=True,
                          text=True, timeout=20, check=False, **kwargs)


class HeadlessRunnerTests(unittest.TestCase):
    def test_desktop_environment_cannot_redirect_the_private_display(self):
        environment = os.environ.copy()
        environment.update(DISPLAY=":9999", WAYLAND_DISPLAY="unused-wayland",
                           XAUTHORITY="/nonexistent", GDK_BACKEND="wayland",
                           GIO_USE_VFS="gvfs", DBUS_SESSION_BUS_ADDRESS="autolaunch:",
                           GTK_USE_PORTAL="1")
        probe = ("import ctypes,ctypes.util,os; "
                 "assert os.environ['DISPLAY'] != ':9999'; "
                 "assert os.environ['GDK_BACKEND'] == 'x11'; "
                 "assert os.environ['GIO_USE_VFS'] == 'local'; "
                 "assert os.environ['DBUS_SESSION_BUS_ADDRESS'] == 'disabled:'; "
                 "assert 'GTK_USE_PORTAL' not in os.environ; "
                 "assert 'WAYLAND_DISPLAY' not in os.environ; "
                 "assert 'XAUTHORITY' not in os.environ; "
                 "x=ctypes.CDLL(ctypes.util.find_library('X11')); "
                 "x.XOpenDisplay.argtypes=[ctypes.c_char_p]; "
                 "x.XOpenDisplay.restype=ctypes.c_void_p; assert x.XOpenDisplay(None)")
        result = run_test(sys.executable, "-c", probe, env=environment)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_failure_and_missing_command(self):
        self.assertEqual(run_test(sys.executable, "-c", "raise SystemExit(23)").returncode, 23)
        self.assertEqual(run_test().returncode, 2)
        environment = os.environ.copy()
        environment["PATH"] = "/nonexistent"
        missing = run_test(sys.executable, "-c", "pass", env=environment)
        self.assertEqual(missing.returncode, 2)
        self.assertIn("Xvfb is required", missing.stderr)

    def test_concurrent_displays_are_private_and_closed(self):
        probe = ("import ctypes,ctypes.util,os,sys; "
                 "x=ctypes.CDLL(ctypes.util.find_library('X11')); "
                 "x.XOpenDisplay.argtypes=[ctypes.c_char_p];x.XOpenDisplay.restype=ctypes.c_void_p; "
                 "assert x.XOpenDisplay(None);print(os.environ['DISPLAY'],flush=True);sys.stdin.readline()")
        processes = [subprocess.Popen([sys.executable, RUNNER, sys.executable, "-c", probe],
                                      stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                      stderr=subprocess.PIPE, text=True)
                     for _ in range(2)]
        try:
            displays = [process.stdout.readline().strip() for process in processes]
            self.assertTrue(all(display.startswith(":") for display in displays), displays)
            self.assertNotEqual(*displays)
            server_pids = []
            for process in processes:
                children = pathlib.Path(f"/proc/{process.pid}/task/{process.pid}/children")
                child_pids = map(int, children.read_text().split())
                servers = [pid for pid in child_pids
                           if pathlib.Path(f"/proc/{pid}/comm").read_text().strip() == "Xvfb"]
                self.assertEqual(len(servers), 1, servers)
                server_pids.extend(servers)
            for process in processes:
                _, stderr = process.communicate(input="\n", timeout=15)
                self.assertEqual(process.returncode, 0, stderr)
            for pid in server_pids:
                with self.assertRaises(ProcessLookupError):
                    os.kill(pid, 0)
        finally:
            for process in processes:
                if process.poll() is None:
                    process.kill()
                process.communicate(timeout=15)

    def test_termination_stops_test_process(self):
        with tempfile.TemporaryDirectory(prefix="vellum-headless-") as directory:
            marker = pathlib.Path(directory) / "pid"
            script = ("import os,pathlib,subprocess,sys,time;"
                      "child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(60)']);"
                      "marker=pathlib.Path(sys.argv[1]);temporary=marker.with_suffix('.tmp');"
                      "temporary.write_text(f'{os.getpid()} {child.pid}');os.replace(temporary,marker);"
                      "time.sleep(60)")
            runner = subprocess.Popen([sys.executable, RUNNER, sys.executable, "-c", script, marker],
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            try:
                deadline = time.monotonic() + 15
                while not marker.exists() and runner.poll() is None and time.monotonic() < deadline:
                    time.sleep(.01)
                self.assertTrue(marker.exists(), "child did not start")
                pid, grandchild = map(int, marker.read_text().split())
                runner.send_signal(signal.SIGTERM)
                _, stderr = runner.communicate(timeout=10)
                self.assertEqual(runner.returncode, 128 + signal.SIGTERM, stderr)
                with self.assertRaises(ProcessLookupError):
                    os.kill(pid, 0)
                deadline = time.monotonic() + 5
                proc_stat = pathlib.Path(f"/proc/{grandchild}/stat")
                while proc_stat.exists() and time.monotonic() < deadline:
                    try:
                        if proc_stat.read_text().split()[2] == "Z":
                            break
                    except FileNotFoundError:
                        break
                    time.sleep(.01)
                else:
                    self.assertFalse(proc_stat.exists(), "grandchild survived termination")
            finally:
                if runner.poll() is None:
                    runner.kill()
                    runner.wait()


if __name__ == "__main__":
    unittest.main()
