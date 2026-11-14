#!/usr/bin/env python3
"""Exercise observed Vellum-origin GUI allocation failures in isolated processes."""
import concurrent.futures
import os
import subprocess
import sys


def execute(binary, fixture, index):
    process = subprocess.run([binary, fixture, str(index)], capture_output=True, text=True,
                             timeout=30, check=False)
    if process.returncode != 0:
        raise RuntimeError(f"allocation {index} failed (exit {process.returncode}): {process.stderr[-1000:]}")
    counts = [line.removeprefix("VELLUM_ALLOCATIONS=") for line in process.stdout.splitlines()
              if line.startswith("VELLUM_ALLOCATIONS=")]
    if len(counts) != 1:
        raise RuntimeError(f"allocation {index} did not report its count")
    count = int(counts[0])
    if index >= 0 and count != index + 1:
        raise RuntimeError(f"allocation {index} was not injected at the expected point")
    return count


def main():
    if len(sys.argv) not in (3, 4):
        raise RuntimeError("expected allocation-test binary, PDF fixture and optional label")
    binary, fixture = sys.argv[1:3]
    label = sys.argv[3] if len(sys.argv) == 4 else "Print dialog"
    count = execute(binary, fixture, -1)
    if count == 0 or count > 1000:
        raise RuntimeError(f"unexpected number of Vellum-origin allocation points: {count}")
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(4, os.cpu_count() or 1)) as pool:
        list(pool.map(lambda index: execute(binary, fixture, index), range(count)))
    print(f"{label}: {count} Vellum-origin allocation-failure cases")


if __name__ == "__main__":
    main()
