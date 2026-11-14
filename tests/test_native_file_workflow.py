#!/usr/bin/env python3
"""Drive the native file chooser and verify Save As and reopen under X11."""
import ctypes
import ctypes.util
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time

import test_app_startup as ui


class XErrorEvent(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int), ("display", ctypes.c_void_p),
                ("resource_id", ctypes.c_ulong), ("serial", ctypes.c_ulong),
                ("error_code", ctypes.c_ubyte), ("request_code", ctypes.c_ubyte),
                ("minor_code", ctypes.c_ubyte)]


def configure_keyboard(x11):
    library = ctypes.util.find_library("Xtst")
    if not library:
        raise RuntimeError("XTest library is required for native file-dialog tests")
    xtest = ctypes.CDLL(library)
    xtest.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
    x11.XSetInputFocus.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_ulong]
    x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
    x11.XKeysymToKeycode.restype = ctypes.c_ubyte
    x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
    x11.XStringToKeysym.restype = ctypes.c_ulong
    x11.XSetErrorHandler.argtypes = [ctypes.c_void_p]
    x11.XSetErrorHandler.restype = ctypes.c_void_p
    x11.XSync.argtypes = [ctypes.c_void_p, ctypes.c_int]
    return xtest


def press(x11, xtest, display, names):
    codes = []
    for name in names:
        code = x11.XKeysymToKeycode(display, x11.XStringToKeysym(name.encode("ascii")))
        if not code:
            raise RuntimeError("keyboard has no key for " + name)
        codes.append(code)
    for code in codes:
        xtest.XTestFakeKeyEvent(display, code, 1, 0)
    for code in reversed(codes):
        xtest.XTestFakeKeyEvent(display, code, 0, 0)
    x11.XFlush(display)


def type_path(x11, xtest, display, path):
    names = {"/": "slash", ".": "period", "-": "minus"}
    for character in str(path):
        if character == "_":
            press(x11, xtest, display, ("Shift_L", "minus"))
        elif character.isascii() and character.isupper():
            press(x11, xtest, display, ("Shift_L", character.lower()))
        elif character.isascii() and (character.islower() or character.isdigit() or character in names):
            press(x11, xtest, display, (names.get(character, character),))
        else:
            raise RuntimeError("test path has an unsupported character")


def focus(x11, display, window):
    x11.XSetInputFocus(display, window, 1, 0)  # RevertToParent, CurrentTime
    x11.XFlush(display)


def choose_path(x11, xtest, display, process, title, path):
    dialog = ui.wait_for_window(x11, display, process, title)
    focus(x11, display, dialog)
    press(x11, xtest, display, ("Control_L", "l"))
    # GTK creates its location entry asynchronously after the shortcut.
    time.sleep(.2)
    type_path(x11, xtest, display, path)
    # Let GTK's filename completion settle before activating the dialog.
    time.sleep(1)
    press(x11, xtest, display, ("Return",))


def wait_for_file(path, process):
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline and process.poll() is None:
        if path.exists():
            return
        time.sleep(.05)
    raise RuntimeError("Save As did not produce the requested file")


def main():
    if len(sys.argv) != 3:
        raise RuntimeError("expected application and PDF fixture paths")
    x11 = ui.configure_xlib()
    xtest = configure_keyboard(x11)
    display = x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError("could not open X display; run under Xvfb")
    unexpected_errors = []
    callback_type = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.POINTER(XErrorEvent))

    @callback_type
    def handle_x_error(_display, error):
        # Window-tree snapshots can contain a dialog that closes during traversal.
        if error.contents.error_code != 3:  # BadWindow
            unexpected_errors.append(error.contents.error_code)
        return 0

    previous_handler = x11.XSetErrorHandler(handle_x_error)
    try:
        with tempfile.TemporaryDirectory(prefix="vellum-native-") as temporary:
            source = pathlib.Path(temporary) / "source.pdf"
            saved = pathlib.Path(temporary) / "saved.pdf"
            invalid = pathlib.Path(temporary) / "invalid.pdf"
            shutil.copyfile(sys.argv[2], source)
            invalid.write_bytes(b"not a PDF")
            expected = source.read_bytes()
            process = subprocess.Popen([sys.argv[1], str(source)], stdout=subprocess.DEVNULL,
                                       stderr=subprocess.PIPE)
            try:
                window = ui.wait_for_window(x11, display, process, source.name)
                source.write_bytes(b"external replacement")
                focus(x11, display, window)
                press(x11, xtest, display, ("Control_L", "s"))
                error = ui.wait_for_window(x11, display, process, "Vellum", exact=True)
                ui.close_window(x11, display, error)
                window = ui.wait_for_window(x11, display, process, source.name)
                if source.read_bytes() != b"external replacement":
                    raise RuntimeError("failed save overwrote externally changed data")
                focus(x11, display, window)
                press(x11, xtest, display, ("Control_L", "Shift_L", "s"))
                choose_path(x11, xtest, display, process, "Save As", saved)
                wait_for_file(saved, process)
                if saved.read_bytes() != expected or source.read_bytes() != b"external replacement":
                    raise RuntimeError("Save As changed the PDF bytes or externally changed original")
                source.write_bytes(expected)
                window = ui.wait_for_window(x11, display, process, saved.name)
                for path in (source, saved):
                    focus(x11, display, window)
                    press(x11, xtest, display, ("Control_L", "o"))
                    choose_path(x11, xtest, display, process, "Open PDF", path)
                    window = ui.wait_for_window(x11, display, process, path.name)
                focus(x11, display, window)
                press(x11, xtest, display, ("Control_L", "o"))
                choose_path(x11, xtest, display, process, "Open PDF", invalid)
                error = ui.wait_for_window(x11, display, process, "Vellum", exact=True)
                ui.close_window(x11, display, error)
                window = ui.wait_for_window(x11, display, process, saved.name)
                if saved.read_bytes() != expected:
                    raise RuntimeError("failed open changed the previously saved document")
                for path, should_fail in ((saved, False), (invalid, True)):
                    focus(x11, display, window)
                    press(x11, xtest, display, ("Alt_L", "f"))
                    press(x11, xtest, display, ("r",))
                    choose_path(x11, xtest, display, process, "Recover committed snapshot", path)
                    if should_fail:
                        error = ui.wait_for_window(x11, display, process, "Vellum", exact=True)
                        ui.close_window(x11, display, error)
                    window = ui.wait_for_window(x11, display, process, "recovered snapshot")
                if saved.read_bytes() != expected:
                    raise RuntimeError("recovery changed the committed snapshot")
                ui.close_window(x11, display, window)
                if process.wait(timeout=10) != 0:
                    raise RuntimeError("application failed to close after reopening")
                x11.XSync(display, 0)
                if unexpected_errors:
                    raise RuntimeError("unexpected X11 errors: " + str(unexpected_errors))
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                if process.stderr:
                    process.stderr.close()
    finally:
        x11.XSetErrorHandler(previous_handler)
        x11.XCloseDisplay(display)


if __name__ == "__main__":
    main()
