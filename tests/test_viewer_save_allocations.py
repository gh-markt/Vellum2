#!/usr/bin/env python3
"""Exercise viewer Save under allocation and external-replacement failures."""
import concurrent.futures
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


def execute(binary, fixture, mode, index):
    with tempfile.TemporaryDirectory(prefix="vellum-save-allocation-") as temporary:
        source = pathlib.Path(temporary) / "source.pdf"
        shutil.copyfile(fixture, source)
        process = subprocess.run([binary, str(source), str(index), mode], capture_output=True,
                                 text=True, timeout=30, check=False)
    if process.returncode != 0:
        raise RuntimeError(f"{mode} allocation {index} failed (exit {process.returncode}): {process.stderr[-1000:]}")
    values = [line.removeprefix("VELLUM_ALLOCATIONS=") for line in process.stdout.splitlines()
              if line.startswith("VELLUM_ALLOCATIONS=")]
    if len(values) != 1:
        raise RuntimeError(f"{mode} allocation {index} did not report its count")
    count = int(values[0])
    if index >= 0 and count != index + 1:
        raise RuntimeError(f"{mode} allocation {index} was not injected at the expected point")
    return count


def main():
    if len(sys.argv) != 3:
        raise RuntimeError("expected allocation-test binary and PDF fixture")
    binary, fixture = sys.argv[1:]
    for mode in ("unchanged", "external"):
        count = execute(binary, fixture, mode, -1)
        if count == 0 or count > 1000:
            raise RuntimeError(f"unexpected number of {mode} save allocations: {count}")
        with concurrent.futures.ThreadPoolExecutor(max_workers=min(4, os.cpu_count() or 1)) as pool:
            list(pool.map(lambda index: execute(binary, fixture, mode, index), range(count)))
        print(f"Viewer Save ({mode}): {count} Vellum-origin allocation-failure cases")


if __name__ == "__main__":
    main()
