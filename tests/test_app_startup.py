#!/usr/bin/env python3
"""Open a PDF in the real application and request a normal X11 window close."""
import ctypes
import ctypes.util
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time


class ClientData(ctypes.Union):
    _fields_ = [("longs", ctypes.c_long * 5), ("bytes", ctypes.c_byte * 20)]


class ClientMessage(ctypes.Structure):
    _fields_ = [("type", ctypes.c_int), ("serial", ctypes.c_ulong),
                ("send_event", ctypes.c_int), ("display", ctypes.c_void_p),
                ("window", ctypes.c_ulong), ("message_type", ctypes.c_ulong),
                ("format", ctypes.c_int), ("data", ClientData)]


class XEvent(ctypes.Union):
    _fields_ = [("client", ClientMessage), ("padding", ctypes.c_long * 24)]


class WindowAttributes(ctypes.Structure):
    _fields_ = [("x", ctypes.c_int), ("y", ctypes.c_int), ("width", ctypes.c_int),
                ("height", ctypes.c_int), ("border_width", ctypes.c_int), ("depth", ctypes.c_int),
                ("visual", ctypes.c_void_p), ("root", ctypes.c_ulong), ("window_class", ctypes.c_int),
                ("bit_gravity", ctypes.c_int), ("win_gravity", ctypes.c_int),
                ("backing_store", ctypes.c_int), ("backing_planes", ctypes.c_ulong),
                ("backing_pixel", ctypes.c_ulong), ("save_under", ctypes.c_int),
                ("colormap", ctypes.c_ulong), ("map_installed", ctypes.c_int),
                ("map_state", ctypes.c_int), ("all_event_masks", ctypes.c_long),
                ("your_event_mask", ctypes.c_long), ("do_not_propagate_mask", ctypes.c_long),
                ("override_redirect", ctypes.c_int), ("screen", ctypes.c_void_p)]


def configure_xlib():
    library = ctypes.util.find_library("X11")
    if not library:
        raise RuntimeError("X11 library is required for the headless application test")
    x11 = ctypes.CDLL(library)
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
    x11.XDefaultRootWindow.restype = ctypes.c_ulong
    x11.XGrabServer.argtypes = [ctypes.c_void_p]
    x11.XUngrabServer.argtypes = [ctypes.c_void_p]
    x11.XQueryTree.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                              ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong),
                              ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)),
                              ctypes.POINTER(ctypes.c_uint)]
    x11.XFetchName.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_void_p)]
    x11.XGetWindowAttributes.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                                         ctypes.POINTER(WindowAttributes)]
    x11.XGetWMProtocols.argtypes = [ctypes.c_void_p, ctypes.c_ulong,
                                    ctypes.POINTER(ctypes.POINTER(ctypes.c_ulong)),
                                    ctypes.POINTER(ctypes.c_int)]
    x11.XGetWindowProperty.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_ulong,
                                      ctypes.c_long, ctypes.c_long, ctypes.c_int, ctypes.c_ulong,
                                      ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_int),
                                      ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_ulong),
                                      ctypes.POINTER(ctypes.POINTER(ctypes.c_ubyte))]
    x11.XFree.argtypes = [ctypes.c_void_p]
    x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    x11.XInternAtom.restype = ctypes.c_ulong
    x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int,
                               ctypes.c_long, ctypes.POINTER(XEvent)]
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]
    return x11


def belongs_to_process(x11, display, window, pid):
    atom = x11.XInternAtom(display, b"_NET_WM_PID", 0)
    property_type = ctypes.c_ulong()
    property_format = ctypes.c_int()
    length = ctypes.c_ulong()
    remaining = ctypes.c_ulong()
    data = ctypes.POINTER(ctypes.c_ubyte)()
    result = x11.XGetWindowProperty(display, window, atom, 0, 1, 0, 0,
                                    ctypes.byref(property_type), ctypes.byref(property_format),
                                    ctypes.byref(length), ctypes.byref(remaining), ctypes.byref(data))
    if result != 0 or not data:
        return False
    try:
        return (property_format.value == 32 and length.value == 1 and
                ctypes.cast(data, ctypes.POINTER(ctypes.c_ulong))[0] == pid)
    finally:
        x11.XFree(data)


def find_window(x11, display, root, title, pid, exact=False):
    atom = x11.XInternAtom(display, b"_NET_WM_NAME", 0)
    property_type = ctypes.c_ulong()
    property_format = ctypes.c_int()
    length = ctypes.c_ulong()
    remaining = ctypes.c_ulong()
    data = ctypes.POINTER(ctypes.c_ubyte)()
    result = x11.XGetWindowProperty(display, root, atom, 0, 1024, 0, 0,
                                    ctypes.byref(property_type), ctypes.byref(property_format),
                                    ctypes.byref(length), ctypes.byref(remaining), ctypes.byref(data))
    if result == 0 and data:
        try:
            if property_format.value == 8:
                name = ctypes.string_at(data, length.value).decode("utf-8", errors="replace")
                if ((name == title if exact else title in name) and
                        belongs_to_process(x11, display, root, pid)):
                    return root
        finally:
            x11.XFree(data)
    name = ctypes.c_void_p()
    if x11.XFetchName(display, root, ctypes.byref(name)) and name.value:
        try:
            text = ctypes.string_at(name).decode("utf-8", errors="replace")
            if ((text == title if exact else title in text) and
                    belongs_to_process(x11, display, root, pid)):
                return root
        finally:
            x11.XFree(name)
    root_return = ctypes.c_ulong()
    parent = ctypes.c_ulong()
    children = ctypes.POINTER(ctypes.c_ulong)()
    count = ctypes.c_uint()
    if not x11.XQueryTree(display, root, ctypes.byref(root_return), ctypes.byref(parent),
                          ctypes.byref(children), ctypes.byref(count)):
        return None
    try:
        for index in range(count.value):
            found = find_window(x11, display, children[index], title, pid, exact)
            if found:
                return found
    finally:
        if children:
            x11.XFree(children)
    return None


def ready_for_close(x11, display, window):
    attributes = WindowAttributes()
    if not x11.XGetWindowAttributes(display, window, ctypes.byref(attributes)):
        return False
    if attributes.map_state != 2:  # IsViewable
        return False
    protocols = ctypes.POINTER(ctypes.c_ulong)()
    count = ctypes.c_int()
    if not x11.XGetWMProtocols(display, window, ctypes.byref(protocols), ctypes.byref(count)):
        return False
    try:
        delete = x11.XInternAtom(display, b"WM_DELETE_WINDOW", 0)
        return any(protocols[index] == delete for index in range(count.value))
    finally:
        x11.XFree(protocols)


def wait_for_window(x11, display, process, title, exact=False):
    root = x11.XDefaultRootWindow(display)
    deadline = time.monotonic() + 25
    while time.monotonic() < deadline and process.poll() is None:
        x11.XGrabServer(display)
        try:
            window = find_window(x11, display, root, title, process.pid, exact)
            ready = window and ready_for_close(x11, display, window)
        finally:
            x11.XUngrabServer(display)
            x11.XFlush(display)
        if ready:
            return window
        time.sleep(0.05)
    raise RuntimeError("application did not open the expected window: " + title)


def close_window(x11, display, window):
    event = XEvent()
    event.client.type = 33  # ClientMessage
    event.client.display = display
    event.client.window = window
    event.client.message_type = x11.XInternAtom(display, b"WM_PROTOCOLS", 0)
    event.client.format = 32
    event.client.data.longs[0] = x11.XInternAtom(display, b"WM_DELETE_WINDOW", 0)
    if not x11.XSendEvent(display, window, 0, 0, ctypes.byref(event)):
        raise RuntimeError("could not send the window-close request")
    x11.XFlush(display)


def main():
    if len(sys.argv) != 3:
        raise RuntimeError("expected application and PDF fixture paths")
    x11 = configure_xlib()
    display = x11.XOpenDisplay(None)
    if not display:
        raise RuntimeError("could not open X display; run the test under Xvfb")
    try:
        with tempfile.TemporaryDirectory(prefix="vellum-startup-") as directory:
            fixture = pathlib.Path(directory) / (pathlib.Path(directory).name + ".pdf")
            shutil.copyfile(sys.argv[2], fixture)
            process = subprocess.Popen([sys.argv[1], str(fixture)], stdout=subprocess.DEVNULL,
                                       stderr=subprocess.PIPE)
            try:
                close_window(x11, display, wait_for_window(x11, display, process, fixture.name))
                if process.wait(timeout=10) != 0:
                    raise RuntimeError("application did not exit successfully")
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                if process.stderr:
                    process.stderr.close()
            process = subprocess.Popen([sys.argv[1]], stdout=subprocess.DEVNULL,
                                       stderr=subprocess.PIPE)
            other = subprocess.Popen([sys.argv[1]], stdout=subprocess.DEVNULL,
                                     stderr=subprocess.PIPE)
            try:
                other_window = wait_for_window(x11, display, other, "Vellum")
                close_window(x11, display, wait_for_window(x11, display, process, "Vellum"))
                if process.wait(timeout=10) != 0:
                    raise RuntimeError("application did not exit from an empty startup")
                if other.poll() is not None:
                    raise RuntimeError("closing one application closed a different process")
                close_window(x11, display, other_window)
                if other.wait(timeout=10) != 0:
                    raise RuntimeError("second empty application did not exit")
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                if process.stderr:
                    process.stderr.close()
                if other.poll() is None:
                    other.kill()
                    other.wait()
                if other.stderr:
                    other.stderr.close()
            process = subprocess.Popen([sys.argv[1], str(fixture) + ".missing"],
                                       stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            try:
                close_window(x11, display, wait_for_window(x11, display, process, "Cannot open PDF"))
                close_window(x11, display, wait_for_window(x11, display, process, "Vellum"))
                if process.wait(timeout=10) != 0:
                    raise RuntimeError("application did not recover from the failed open")
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
                if process.stderr:
                    process.stderr.close()
    finally:
        x11.XCloseDisplay(display)


if __name__ == "__main__":
    main()
