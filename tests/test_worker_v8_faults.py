#!/usr/bin/env python3
"""Check the real worker's handling of V8 initialization and source failures."""
import fcntl
import mmap
import os
import signal
import socket
import struct
import subprocess
import sys

worker = sys.argv[1]
startup_cases = [("initialize", b"I"), ("isolate", b"N"), ("platform-allocation", b"A"),
                 ("allocator-allocation", b"BA"), ("isolate-allocation", b"JA"),
                 ("platform-init-allocation", b"TA")]
if os.environ.get("VELLUM_TEST_ASAN") == "1":
    # V8 leaks an allocation internally when construction throws before returning
    # the isolate; the short-lived worker's OS cleanup reclaims it. See audit inventory.
    startup_cases.remove(("isolate-allocation", b"JA"))
    print("Skipping V8 internal isolate allocation fault under LeakSanitizer", flush=True)
for mode, marker in startup_cases:
    read_fd, write_fd = os.pipe()
    try:
        env = dict(os.environ, VELLUM_TEST_V8_FAILURE=mode, VELLUM_TEST_TRACE_FD=str(write_fd))
        result = subprocess.run([worker], env=env, pass_fds=(write_fd,),
                                capture_output=True, timeout=10, check=False)
        os.close(write_fd)
        write_fd = -1
        trace = os.read(read_fd, 32)
        if result.returncode != 2 or trace != marker:
            raise AssertionError(f"{mode}: exit={result.returncode}, trace={trace!r}, "
                                 f"stderr={result.stderr!r}")
    finally:
        os.close(read_fd)
        if write_fd >= 0:
            os.close(write_fd)

packet = struct.Struct("=4I")
capacity = 1024 * 1024
def exchange(mode, source):
    parent, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    memory_fd = os.memfd_create("vellum-v8-fault", os.MFD_CLOEXEC)
    os.ftruncate(memory_fd, capacity)
    child_fd = fcntl.fcntl(child.fileno(), fcntl.F_DUPFD_CLOEXEC, 10)
    high_memory_fd = fcntl.fcntl(memory_fd, fcntl.F_DUPFD_CLOEXEC, 10)
    read_fd, write_fd = os.pipe()
    os.set_inheritable(write_fd, True)
    pid = None
    try:
        with mmap.mmap(memory_fd, capacity) as memory:
            memory[:len(source)] = source
            actions = [(os.POSIX_SPAWN_DUP2, child_fd, 3),
                       (os.POSIX_SPAWN_DUP2, high_memory_fd, 4)]
            env = dict(os.environ, VELLUM_TEST_V8_FAILURE=mode, VELLUM_TEST_TRACE_FD=str(write_fd))
            pid = os.posix_spawn(worker, [worker], env,
                                 file_actions=actions)
            child.close()
            os.close(child_fd)
            child_fd = None
            parent.settimeout(10)
            magic = 0 if mode.endswith("-invalid") else 0x564C4D31
            parent.send(packet.pack(magic, 1, len(source), 0))
            reply = parent.recv(packet.size + 1)
            _, status = os.waitpid(pid, 0)
            pid = None
            os.close(write_fd)
            write_fd = -1
            trace = os.read(read_fd, 32)
            if mode == "source":
                expected = b"invalid script source"
                if (os.waitstatus_to_exitcode(status) != 0 or
                        packet.unpack(reply) != (0x564C4D31, 1, len(expected), 1) or
                        memory[:len(expected)] != expected or trace != b"S"):
                    raise AssertionError("worker did not report source conversion failure")
            elif mode == "conversion-internal-allocation":
                expected = b"x" * 128
                if (os.waitstatus_to_exitcode(status) != 0 or trace != b"VA" or
                        packet.unpack(reply) != (0x564C4D31, 1, len(expected), 0) or
                        memory[:len(expected)] != expected):
                    raise AssertionError("worker did not recover the result after conversion allocation failure")
            elif mode in ("dispose-isolate", "dispose-v8", "dispose-isolate-invalid",
                          "dispose-v8-invalid"):
                if os.waitstatus_to_exitcode(status) != 2 or trace != b"DA" or reply != b"":
                    raise AssertionError("worker did not report its shutdown allocation failure")
            elif mode == "run-fatal-allocation":
                if (os.waitstatus_to_exitcode(status) != -signal.SIGABRT or
                        trace != b"FA" or reply != b""):
                    raise AssertionError("worker did not terminate on the injected fatal V8 allocation")
            elif (os.waitstatus_to_exitcode(status) != 2 or reply != b"" or
                  trace != {"source-allocation": b"SA", "compile-allocation": b"CA",
                            "run-allocation": b"EA", "context-allocation": b"XA",
                            "enter-allocation": b"IA",
                            "compile-internal-allocation": b"KA",
                            "conversion-internal-allocation": b"VA"}.get(mode, b"RA")):
                raise AssertionError(f"result allocation: status={status}, reply={reply!r}, trace={trace!r}")
    finally:
        if pid is not None:
            os.kill(pid, 9)
            os.waitpid(pid, 0)
        parent.close()
        child.close()
        if child_fd is not None:
            os.close(child_fd)
        os.close(high_memory_fd)
        os.close(memory_fd)
        os.close(read_fd)
        if write_fd >= 0:
            os.close(write_fd)


exchange("source", b"6 * 7")
exchange("result-allocation", b"'x'.repeat(128)")
exchange("conversion-allocation", b"({toString(){throw Error('bad')}})")
exchange("source-allocation", b"6 * 7")
exchange("compile-allocation", b"function {")
exchange("run-allocation", b"throw Error('bad')")
exchange("context-allocation", b"6 * 7")
exchange("enter-allocation", b"6 * 7")
exchange("dispose-isolate", b"6 * 7")
exchange("dispose-v8", b"6 * 7")
exchange("dispose-isolate-invalid", b"6 * 7")
exchange("dispose-v8-invalid", b"6 * 7")
exchange("run-fatal-allocation", b"JSON.stringify(Array.from({length:200},(_,i)=>({x:i})))")
exchange("compile-internal-allocation", b"'x'.repeat(128)")
exchange("conversion-internal-allocation", b"'x'.repeat(128)")
