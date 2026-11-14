#!/usr/bin/env python3
"""A worker must fail closed if either security setup operation fails."""
import os
import subprocess
import sys

try:
    glibc = os.confstr("CS_GNU_LIBC_VERSION")
except (ValueError, OSError):
    glibc = None
if not glibc:
    sys.exit(77)

worker, interceptor = sys.argv[1:]
for mode, expected_trace in (("limit", b"R"), ("prctl", b"RP")):
    read_fd, write_fd = os.pipe()
    try:
        env = os.environ.copy()
        env.update(LD_PRELOAD=interceptor, VELLUM_TEST_SECURITY_FAILURE=mode,
                   VELLUM_TEST_TRACE_FD=str(write_fd))
        result = subprocess.run([worker], env=env, pass_fds=(write_fd,),
                                capture_output=True, timeout=10, check=False)
        os.close(write_fd)
        write_fd = -1
        trace = os.read(read_fd, 32)
        if result.returncode != 2 or trace != expected_trace:
            raise AssertionError(f"{mode}: exit={result.returncode}, trace={trace!r}, "
                                 f"stderr={result.stderr!r}")
    finally:
        os.close(read_fd)
        if write_fd >= 0:
            os.close(write_fd)
