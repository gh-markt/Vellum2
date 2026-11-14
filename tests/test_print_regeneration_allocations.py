#!/usr/bin/env python3
"""Exercise print-plan regeneration allocation failures across layouts."""
import concurrent.futures
import os
import subprocess
import sys


def execute(binary, fixture, layout, index):
    process = subprocess.run([binary, fixture, str(index), str(layout)], capture_output=True,
                             text=True, timeout=30, check=False)
    if process.returncode != 0:
        raise RuntimeError(f"layout {layout} allocation {index} failed (exit {process.returncode}): {process.stderr[-1000:]}")
    values = [line.removeprefix("VELLUM_ALLOCATIONS=") for line in process.stdout.splitlines()
              if line.startswith("VELLUM_ALLOCATIONS=")]
    if len(values) != 1:
        raise RuntimeError(f"layout {layout} allocation {index} did not report its count")
    count = int(values[0])
    if index >= 0 and count != index + 1:
        raise RuntimeError(f"layout {layout} allocation {index} was not injected at the expected point")
    return count


def main():
    if len(sys.argv) not in (3, 4):
        raise RuntimeError("expected allocation-test binary, PDF fixture and optional label")
    binary, fixture = sys.argv[1:3]
    label = sys.argv[3] if len(sys.argv) == 4 else "Print regeneration"
    for layout in range(4):
        count = execute(binary, fixture, layout, -1)
        if count == 0 or count > 1000:
            raise RuntimeError(f"unexpected allocation count for layout {layout}: {count}")
        with concurrent.futures.ThreadPoolExecutor(max_workers=min(4, os.cpu_count() or 1)) as pool:
            list(pool.map(lambda index: execute(binary, fixture, layout, index), range(count)))
        print(f"{label} mode {layout}: {count} Vellum-origin allocation-failure cases")


if __name__ == "__main__":
    main()
