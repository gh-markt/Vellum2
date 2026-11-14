#!/usr/bin/env python3
"""Exercise Editor-mode transition allocation failures with isolated homes."""
import concurrent.futures
import os
import subprocess
import sys
import tempfile


def execute(binary, fixture, index):
    with tempfile.TemporaryDirectory(prefix="vellum-editor-allocation-") as home:
        environment = dict(os.environ, HOME=home,
                           XDG_DATA_HOME=os.path.join(home, "data"),
                           XDG_CONFIG_HOME=os.path.join(home, "config"),
                           XDG_CACHE_HOME=os.path.join(home, "cache"))
        process = subprocess.run([binary, fixture, str(index)], capture_output=True, text=True,
                                 timeout=30, check=False, env=environment)
    if process.returncode != 0:
        raise RuntimeError(f"Editor allocation {index} failed (exit {process.returncode}): {process.stderr[-1000:]}")
    values = [line.removeprefix("VELLUM_ALLOCATIONS=") for line in process.stdout.splitlines()
              if line.startswith("VELLUM_ALLOCATIONS=")]
    if len(values) != 1:
        raise RuntimeError(f"Editor allocation {index} did not report its count")
    count = int(values[0])
    if index >= 0 and count != index + 1:
        raise RuntimeError(f"Editor allocation {index} was not injected at the expected point")
    return count


def main():
    if len(sys.argv) != 3:
        raise RuntimeError("expected allocation-test binary and PDF fixture")
    binary, fixture = sys.argv[1:]
    count = execute(binary, fixture, -1)
    if count == 0 or count > 1000:
        raise RuntimeError(f"unexpected number of Editor transition allocations: {count}")
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(4, os.cpu_count() or 1)) as pool:
        list(pool.map(lambda index: execute(binary, fixture, index), range(count)))
    print(f"Editor transition: {count} Vellum-origin allocation-failure cases")


if __name__ == "__main__":
    main()
