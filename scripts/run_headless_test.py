#!/usr/bin/env python3
"""Run one GUI test on a private Xvfb display, including on headless hosts."""
import os
import select
import shutil
import signal
import subprocess
import sys
import time


class Interrupted(Exception):
    def __init__(self, signum):
        self.signum = signum


def interrupt(signum, _frame):
    raise Interrupted(signum)


def main():
    if len(sys.argv) < 2:
        print("expected a test command", file=sys.stderr)
        return 2
    xvfb = shutil.which("Xvfb")
    if not xvfb:
        print("Xvfb is required for headless GUI tests", file=sys.stderr)
        return 2
    read_fd, write_fd = os.pipe()
    server = None
    child = None
    try:
        server = subprocess.Popen([xvfb, "-noreset", "-displayfd", str(write_fd),
                                   "-screen", "0", "1280x1024x24"],
                                  pass_fds=(write_fd,), stdout=subprocess.DEVNULL,
                                  stderr=subprocess.PIPE)
        os.close(write_fd)
        write_fd = -1
        deadline = time.monotonic() + 15
        data = b""
        while b"\n" not in data:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError("Xvfb did not provide a display within 15 seconds")
            ready, _, _ = select.select([read_fd], [], [], remaining)
            if not ready:
                raise RuntimeError("Xvfb did not provide a display within 15 seconds")
            chunk = os.read(read_fd, 32)
            if not chunk:
                raise RuntimeError("Xvfb exited before providing a display")
            data += chunk
            if len(data) > 32:
                raise RuntimeError("Xvfb returned an invalid display number")
        number = data.split(b"\n", 1)[0]
        if not number.isdigit() or server.poll() is not None:
            raise RuntimeError("Xvfb returned an invalid or unavailable display")
        env = os.environ.copy()
        env["DISPLAY"] = ":" + number.decode("ascii")
        env["GDK_BACKEND"] = "x11"
        env["GIO_USE_VFS"] = "local"
        env["DBUS_SESSION_BUS_ADDRESS"] = "disabled:"
        env.pop("GTK_USE_PORTAL", None)
        env.pop("WAYLAND_DISPLAY", None)
        env.pop("XAUTHORITY", None)
        child = subprocess.Popen(sys.argv[1:], env=env, start_new_session=True)
        return child.wait()
    except Interrupted as error:
        if child and child.poll() is None:
            try:
                os.killpg(child.pid, error.signum)
            except ProcessLookupError:
                pass
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                child.wait()
        return 128 + error.signum
    except (OSError, RuntimeError) as error:
        print(f"headless test setup failed: {error}", file=sys.stderr)
        return 2
    finally:
        os.close(read_fd)
        if write_fd >= 0:
            os.close(write_fd)
        if server:
            if server.poll() is None:
                server.terminate()
                try:
                    server.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()
            diagnostics = server.stderr.read()
            server.stderr.close()
            if diagnostics and server.returncode not in (0, -signal.SIGTERM):
                print(diagnostics.decode(errors="replace"), file=sys.stderr, end="")


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, interrupt)
    signal.signal(signal.SIGINT, interrupt)
    sys.exit(main())
