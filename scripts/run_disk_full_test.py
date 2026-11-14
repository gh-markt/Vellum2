#!/usr/bin/env python3
"""Run disk-full integration on an existing or private bounded tmpfs."""
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


def mounted_test(directory, program):
    mount = shutil.which("mount")
    unmount = shutil.which("umount")
    if not mount or not unmount:
        print("mount and umount are required for the private tmpfs", file=sys.stderr)
        return 2
    result = subprocess.run([mount, "-t", "tmpfs", "-o", "size=1m,mode=0700",
                             "tmpfs", directory], check=False)
    if result.returncode:
        print("could not create the private bounded tmpfs", file=sys.stderr)
        return result.returncode
    try:
        environment = os.environ.copy()
        environment["VELLUM_DISK_FULL_TEST_DIR"] = directory
        test_status = subprocess.run([program], env=environment, check=False).returncode
    finally:
        result = subprocess.run([unmount, directory], check=False)
        if result.returncode:
            print("could not unmount the private bounded tmpfs", file=sys.stderr)
    return test_status or result.returncode


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "--mounted":
        return mounted_test(sys.argv[2], sys.argv[3])
    if len(sys.argv) != 2:
        print("expected the disk-full test executable", file=sys.stderr)
        return 2
    program = str(pathlib.Path(sys.argv[1]).resolve())
    if os.environ.get("VELLUM_DISK_FULL_TEST_DIR"):
        return subprocess.run([program], check=False).returncode
    unshare = shutil.which("unshare")
    if not unshare:
        print("a configured bounded tmpfs or unshare is required", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory(prefix="vellum-disk-full-") as directory:
        command = [unshare, "--user", "--map-root-user", "--mount", "--fork",
                   sys.executable, str(pathlib.Path(__file__).resolve()), "--mounted",
                   directory, program]
        result = subprocess.run(command, check=False)
        if result.returncode:
            print("disk-full test could not run in its private bounded filesystem",
                  file=sys.stderr)
        return result.returncode


if __name__ == "__main__":
    sys.exit(main())
