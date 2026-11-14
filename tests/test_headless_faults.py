#!/usr/bin/env python3
"""Test malformed virtual-display startup and process cleanup failures."""
import contextlib
import importlib.util
import io
import os
import pathlib
import runpy
import signal
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

PROGRAM = pathlib.Path(__file__).resolve().parents[1] / "scripts" / "run_headless_test.py"
spec = importlib.util.spec_from_file_location("vellum_headless", PROGRAM)
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class HeadlessFaultTests(unittest.TestCase):
    def test_in_process_startup_rejections_and_signal(self):
        with mock.patch.object(sys, "argv", [str(PROGRAM)]), \
             contextlib.redirect_stderr(io.StringIO()) as errors:
            self.assertEqual(runner.main(), 2)
            self.assertIn("expected a test command", errors.getvalue())
        with mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]), \
             mock.patch.object(runner.shutil, "which", return_value=None), \
             contextlib.redirect_stderr(io.StringIO()) as errors:
            self.assertEqual(runner.main(), 2)
            self.assertIn("Xvfb is required", errors.getvalue())
        with self.assertRaises(runner.Interrupted) as raised:
            runner.interrupt(signal.SIGTERM, None)
        self.assertEqual(raised.exception.signum, signal.SIGTERM)

    def test_entry_point_registers_signal_handlers(self):
        with mock.patch.object(sys, "argv", [str(PROGRAM)]), \
             mock.patch.object(signal, "signal") as register, \
             contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as raised:
                runpy.run_path(str(PROGRAM), run_name="__main__")
        self.assertEqual(raised.exception.code, 2)
        self.assertEqual([call.args[0] for call in register.call_args_list],
                         [signal.SIGTERM, signal.SIGINT])

    def test_invalid_display_stream_in_process(self):
        cases = (([b""], None, "exited before"),
                 ([b"x" * 32, b"x"], None, "invalid"),
                 ([b"invalid\n"], None, "invalid"),
                 ([b"99\n"], 1, "invalid"))
        for chunks, status, message in cases:
            with self.subTest(chunks=chunks, status=status):
                server = mock.Mock()
                server.poll.return_value = status
                server.returncode = 1
                server.stderr = io.BytesIO(b"server diagnostic\n")
                with mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]), \
                     mock.patch.object(runner.shutil, "which", return_value="/fake/Xvfb"), \
                     mock.patch.object(runner.subprocess, "Popen", return_value=server), \
                     mock.patch.object(runner.select, "select", return_value=([1], [], [])), \
                     mock.patch.object(runner.os, "read", side_effect=chunks), \
                     contextlib.redirect_stderr(io.StringIO()) as errors:
                    self.assertEqual(runner.main(), 2)
                self.assertIn(message, errors.getvalue())
                self.assertIn("server diagnostic", errors.getvalue())

    def test_bad_display_responses(self):
        for payload, diagnostic in ((b"", "exited before"), (b"invalid\n", "invalid"),
                                    (b"x" * 33, "invalid")):
            with self.subTest(payload=payload), tempfile.TemporaryDirectory() as directory:
                fake = pathlib.Path(directory) / "Xvfb"
                fake.write_text(f"#!{sys.executable}\nimport os,sys,time\n"
                                f"fd=int(sys.argv[sys.argv.index('-displayfd')+1])\n"
                                f"payload={payload!r}\n"
                                "os.write(fd,payload)\n"
                                "if payload: time.sleep(60)\n")
                fake.chmod(0o755)
                env = os.environ.copy()
                env["PATH"] = directory
                process = subprocess.run([sys.executable, PROGRAM, sys.executable, "-c", "pass"],
                                         env=env, capture_output=True, text=True, timeout=15,
                                         check=False)
                self.assertEqual(process.returncode, 2, process.stderr)
                self.assertIn(diagnostic, process.stderr)

    def test_missing_test_executable(self):
        process = subprocess.run([sys.executable, PROGRAM, "/definitely/missing/vellum-test"],
                                 capture_output=True, text=True, timeout=15, check=False)
        self.assertEqual(process.returncode, 2)
        self.assertIn("headless test setup failed", process.stderr)

    def test_server_spawn_failure_closes_pipe(self):
        descriptors = os.pipe()
        with mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]), \
                mock.patch.object(runner.shutil, "which", return_value="/fake/Xvfb"), \
                mock.patch.object(runner.os, "pipe", return_value=descriptors), \
                mock.patch.object(runner.subprocess, "Popen", side_effect=OSError("spawn failed")), \
                contextlib.redirect_stderr(io.StringIO()) as errors:
            self.assertEqual(runner.main(), 2)
            self.assertIn("spawn failed", errors.getvalue())
        for descriptor in descriptors:
            with self.assertRaises(OSError):
                os.fstat(descriptor)

    def test_server_failure_reports_diagnostics(self):
        with tempfile.TemporaryDirectory() as directory:
            fake = pathlib.Path(directory) / "Xvfb"
            fake.write_text(f"#!{sys.executable}\nimport sys\n"
                            "sys.stderr.write('server failed deliberately\\n')\nsys.exit(4)\n")
            fake.chmod(0o755)
            env = os.environ.copy()
            env["PATH"] = directory
            process = subprocess.run([sys.executable, PROGRAM, sys.executable, "-c", "pass"],
                                     env=env, capture_output=True, text=True, timeout=15,
                                     check=False)
            self.assertEqual(process.returncode, 2)
            self.assertIn("server failed deliberately", process.stderr)

    def test_interruption_before_child_starts(self):
        with mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]), \
                mock.patch.object(runner.select, "select",
                                  side_effect=runner.Interrupted(signal.SIGTERM)):
            self.assertEqual(runner.main(), 128 + signal.SIGTERM)

    def test_display_timeout_paths(self):
        for time_values in ((0, 16), (0, 1)):
            with self.subTest(clock=time_values), \
                    mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]), \
                    mock.patch.object(runner.time, "monotonic", side_effect=time_values), \
                    mock.patch.object(runner.select, "select", return_value=([], [], [])), \
                    contextlib.redirect_stderr(io.StringIO()) as errors:
                self.assertEqual(runner.main(), 2)
                self.assertIn("did not provide a display", errors.getvalue())

    def fake_processes(self, first_wait, kill_side_effect=None, server_wait=None):
        server = mock.Mock()
        server.poll.return_value = None
        server.stderr = io.BytesIO(b"")
        server.returncode = -signal.SIGTERM
        server.wait.side_effect = server_wait
        child = mock.Mock()
        child.pid = 12345
        child.poll.return_value = None
        child.wait.side_effect = first_wait
        patches = [mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]),
                   mock.patch.object(runner.shutil, "which", return_value="/fake/Xvfb"),
                   mock.patch.object(runner.subprocess, "Popen", side_effect=[server, child]),
                   mock.patch.object(runner.select, "select", return_value=([1], [], [])),
                   mock.patch.object(runner.os, "read", return_value=b"99\n"),
                   mock.patch.object(runner.os, "killpg", side_effect=kill_side_effect)]
        with contextlib.ExitStack() as stack:
            entered = [stack.enter_context(patch) for patch in patches]
            self.assertEqual(runner.main(), 128 + signal.SIGTERM)
        return server, child, entered[-1]

    def test_termination_escalates_for_unresponsive_child(self):
        server, child, kill = self.fake_processes(
            [runner.Interrupted(signal.SIGTERM), subprocess.TimeoutExpired("child", 5), 0])
        self.assertEqual(kill.call_args_list, [mock.call(child.pid, signal.SIGTERM),
                                               mock.call(child.pid, signal.SIGKILL)])
        server.terminate.assert_called_once()

    def test_already_exited_child_during_termination(self):
        _, child, kill = self.fake_processes([runner.Interrupted(signal.SIGTERM), 0],
                                             kill_side_effect=ProcessLookupError())
        kill.assert_called_once_with(child.pid, signal.SIGTERM)

    def test_child_exits_before_forced_kill(self):
        _, child, kill = self.fake_processes(
            [runner.Interrupted(signal.SIGTERM), subprocess.TimeoutExpired("child", 5), 0],
            kill_side_effect=[None, ProcessLookupError()])
        self.assertEqual(kill.call_args_list, [mock.call(child.pid, signal.SIGTERM),
                                               mock.call(child.pid, signal.SIGKILL)])

    def test_server_kill_after_termination_timeout(self):
        server = mock.Mock()
        server.poll.return_value = None
        server.stderr = io.BytesIO(b"")
        server.returncode = -signal.SIGKILL
        server.wait.side_effect = [subprocess.TimeoutExpired("server", 5), 0]
        child = mock.Mock()
        child.wait.return_value = 0
        with mock.patch.object(sys, "argv", [str(PROGRAM), "/test"]), \
                mock.patch.object(runner.shutil, "which", return_value="/fake/Xvfb"), \
                mock.patch.object(runner.subprocess, "Popen", side_effect=[server, child]), \
                mock.patch.object(runner.select, "select", return_value=([1], [], [])), \
                mock.patch.object(runner.os, "read", return_value=b"99\n"):
            self.assertEqual(runner.main(), 0)
        server.kill.assert_called_once()


if __name__ == "__main__":
    unittest.main()
