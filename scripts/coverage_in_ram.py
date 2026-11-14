#!/usr/bin/env python3
"""Run instrumented CTest and generate project-owned coverage on a tmpfs."""
import argparse
import contextlib
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import threading



@contextlib.contextmanager
def monitor_ram_usage(root):
    initial = shutil.disk_usage(root).free
    peak = [0]
    stopped = threading.Event()

    def sample():
        while True:
            peak[0] = max(peak[0], initial - shutil.disk_usage(root).free)
            if stopped.wait(.2):
                break
        peak[0] = max(peak[0], initial - shutil.disk_usage(root).free)

    monitor = threading.Thread(target=sample, daemon=True)
    monitor.start()
    try:
        yield
    finally:
        stopped.set()
        monitor.join()
        print(f"VELLUM_RAM_PEAK_BYTES={peak[0]}", flush=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=pathlib.Path, required=True,
                        help="configured CMake build with VELLUM_COVERAGE=ON")
    parser.add_argument("--ram-root", type=pathlib.Path, default=pathlib.Path("/dev/shm"),
                        help="existing tmpfs or ramfs directory")
    parser.add_argument("--regex", help="run only CTest names matching this expression")
    parser.add_argument("--persist", type=pathlib.Path,
                        help="copy reports to this new directory after generating them")
    args = parser.parse_args()
    project = pathlib.Path(__file__).resolve().parents[1]
    build = args.build.resolve()
    ram_root = args.ram_root.resolve()
    if not build.is_dir() or not ram_root.is_dir():
        parser.error("build and RAM root must be existing directories")
    if args.persist and args.persist.exists():
        parser.error("persistent report destination already exists")
    stat = shutil.which("stat")
    if not stat:
        parser.error("stat is required to verify the RAM filesystem")
    fs = subprocess.run([stat, "-f", "-c", "%T", str(ram_root)], capture_output=True,
                        text=True, check=False)
    if fs.returncode or fs.stdout.strip() not in ("tmpfs", "ramfs"):
        parser.error("RAM root must reside on tmpfs or ramfs")
    gcovr = shutil.which("gcovr")
    if not gcovr:
        sibling = pathlib.Path(sys.executable).parent / "gcovr"
        if sibling.is_file() and os.access(sibling, os.X_OK):
            gcovr = str(sibling)
    if not gcovr:
        parser.error("gcovr is required to produce coverage reports")
    notes = list(build.rglob("*.gcno"))
    if not notes:
        parser.error("build contains no gcov notes; enable VELLUM_COVERAGE")
    required = max(64 * 1024 * 1024, sum(note.stat().st_size for note in notes) * 4 +
                   32 * 1024 * 1024)
    available = shutil.disk_usage(ram_root).free
    if available < required:
        parser.error(f"RAM filesystem has {available} bytes free; at least {required} required")
    workspace = pathlib.Path(tempfile.mkdtemp(prefix="vellum-coverage-", dir=ram_root))
    reports = workspace / "reports"
    reports.mkdir()
    print(f"VELLUM_RAM_COVERAGE_DIR={workspace}", flush=True)
    with monitor_ram_usage(ram_root):
        environment = os.environ.copy()
        environment["GCOV_PREFIX"] = str(workspace)
        environment["GCOV_PREFIX_STRIP"] = "0"
        environment["VELLUM_COVERAGE_RAM_ROOT"] = str(ram_root)
        try:
            workers = len(os.sched_getaffinity(0))
        except AttributeError:
            workers = os.cpu_count() or 1
        command = ["ctest", "--test-dir", str(build), "-j", str(workers),
                   "--output-on-failure", "--no-tests=error"]
        if args.regex:
            command += ["-R", args.regex]
            print(f"Coverage is limited to tests matching: {args.regex}", flush=True)
        result = subprocess.run(command, env=environment, check=False)
        ctest_log = build / "Testing" / "Temporary" / "LastTest.log"
        if not ctest_log.is_file():
            raise RuntimeError("CTest did not produce its detailed test log")
        log = ctest_log.read_text(errors="replace").lower()
        if "libgcov profiling error:" in log or "\nprofiling:" in log:
            raise RuntimeError("gcov reported a counter write error; coverage would be incomplete")
        for source in notes:
            destination = workspace / source.resolve().relative_to(source.anchor)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, destination)
        mirrored_build = workspace / build.relative_to(build.anchor)
        base = [gcovr, "--root", str(project), "--filter", "(src|include)/",
                "--gcov-exclude-directories", ".*vellum_fake_worker[.]dir.*",
                "--object-directory", str(mirrored_build)]
        temporary = workspace / "temporary"
        temporary.mkdir()
        report_environment = os.environ.copy()
        report_environment["TMPDIR"] = str(temporary)
        for options in (["--txt-metric", "line", "--txt", str(reports / "lines.txt")],
                        ["--txt-metric", "branch", "--txt", str(reports / "branches.txt")],
                        ["--json-summary", str(reports / "summary.json"),
                         "--html-details", str(reports / "index.html")]):
            subprocess.run([*base, *options, str(mirrored_build)], cwd=project,
                           env=report_environment, check=True)
        if args.persist:
            shutil.copytree(reports, args.persist)
            print(f"Reports copied to {args.persist}", flush=True)
        print(f"RAM-backed reports: {reports}", flush=True)
        return result.returncode


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"RAM-backed coverage failed: {error}", file=sys.stderr)
        sys.exit(1)
