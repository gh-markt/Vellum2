#!/usr/bin/env python3
"""Exercise application window discovery while another client destroys windows."""
import ctypes
import ctypes.util
import pathlib
import subprocess
import sys
import time


def churn():
    library = ctypes.util.find_library("X11")
    if not library:
        raise RuntimeError("X11 library is required")
    x11 = ctypes.CDLL(library)
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    x11.XDefaultRootWindow.restype = ctypes.c_ulong
    x11.XCreateSimpleWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                                       ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                                       ctypes.c_uint, ctypes.c_ulong, ctypes.c_ulong]
    x11.XCreateSimpleWindow.restype = ctypes.c_ulong
    x11.XDestroyWindow.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    x11.XFlush.argtypes = [ctypes.c_void_p]
    display = x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError("could not open X display")
    root = x11.XDefaultRootWindow(display)
    print("ready", flush=True)
    while True:
        window = x11.XCreateSimpleWindow(display, root, 0, 0, 8, 8, 0, 0, 0)
        x11.XDestroyWindow(display, window)
        x11.XFlush(display)
        time.sleep(.001)


def main():
    if sys.argv[1:] == ["--churn"]:
        churn()
        return
    if len(sys.argv) != 3:
        raise RuntimeError("expected application and PDF fixture paths")
    helper = subprocess.Popen([sys.executable, __file__, "--churn"], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True)
    try:
        if helper.stdout.readline().strip() != "ready":
            raise RuntimeError("window churn helper did not start")
        startup = pathlib.Path(__file__).with_name("test_app_startup.py")
        for _ in range(3):
            if helper.poll() is not None:
                raise RuntimeError("window churn helper exited prematurely")
            result = subprocess.run([sys.executable, startup, *sys.argv[1:]], capture_output=True,
                                    text=True, timeout=40, check=False)
            if result.returncode:
                raise RuntimeError(f"startup failed during window churn: {result.stderr[-2000:]}")
    finally:
        helper.terminate()
        try:
            helper.wait(timeout=5)
        except subprocess.TimeoutExpired:
            helper.kill()
            helper.wait()
        helper.stdout.close()
        helper.stderr.close()


if __name__ == "__main__":
    main()
