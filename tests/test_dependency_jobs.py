#!/usr/bin/env python3
"""Resource-budget and dependency-builder parallelism tests."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("dependency_jobs_builder", ROOT / "scripts/prepare_dependencies.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
GIB = builder.GIB


class JobTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.proc = self.root / "proc"
        (self.proc / "self").mkdir(parents=True)
        self.mem(64 * GIB)

    def mem(self, value):
        (self.proc / "meminfo").write_text(f"MemAvailable: {value // 1024} kB\n")

    def file(self, directory, name, value):
        directory.mkdir(parents=True, exist_ok=True)
        (directory / name).write_text(str(value))

    def mount(self, kind, controllers, group, root="/"):
        mount = self.root / f"mount-{kind}-{controllers.replace(',', '_')}"
        mount.mkdir(exist_ok=True)
        location = mount / Path(group).relative_to(root)
        location.mkdir(parents=True, exist_ok=True)
        (self.proc / "self/cgroup").write_text(f"1:{controllers}:{group}\n")
        (self.proc / "self/mountinfo").write_text(
            f"1 0 0:1 {root} {mount} rw - {kind} cgroup rw,{controllers}\n")
        return mount, location

    def jobs(self, kind, cpu=24):
        with mock.patch.object(builder.os, "sched_getaffinity", return_value=set(range(cpu))), \
             mock.patch.object(builder.subprocess, "check_output", return_value=str(cpu)):
            return builder.automatic_jobs(kind, self.proc)

    def test_nproc_caps_jobs_and_unavailable_nproc_falls_back(self):
        with mock.patch.object(builder.os, "sched_getaffinity", return_value=set(range(24))), \
             mock.patch.object(builder.subprocess, "check_output", return_value="3\n"):
            self.assertEqual(builder.automatic_jobs("mupdf", self.proc), 3)
            self.assertEqual(builder.automatic_jobs("v8", self.proc), 3)
        for result in ("invalid", "0", "-1"):
            with self.subTest(result=result), \
                 mock.patch.object(builder.os, "sched_getaffinity", return_value=set(range(8))), \
                 mock.patch.object(builder.subprocess, "check_output", return_value=result):
                self.assertEqual(builder.automatic_jobs("v8", self.proc), 8)
        with mock.patch.object(builder.os, "sched_getaffinity", return_value=set(range(8))), \
             mock.patch.object(builder.subprocess, "check_output", side_effect=OSError()):
            self.assertEqual(builder.automatic_jobs("v8", self.proc), 8)

    def test_memory_budget_cpu_cap_and_minimum(self):
        self.assertEqual(self.jobs("v8"), 24)
        self.assertEqual(self.jobs("mupdf", 7), 7)
        self.mem(40 * GIB)
        self.assertEqual(self.jobs("v8"), 16)
        self.assertEqual(self.jobs("mupdf"), 24)
        self.mem(10 * GIB)
        self.assertEqual(self.jobs("v8"), 4)
        self.assertEqual(self.jobs("mupdf"), 8)
        self.mem(2 * GIB)
        self.assertEqual(self.jobs("v8"), 1)
        (self.proc / "meminfo").write_text("MemAvailable: invalid kB\n")
        self.assertEqual(self.jobs("v8"), 1)
        (self.proc / "meminfo").unlink()
        self.assertEqual(self.jobs("v8"), 1)

    def test_nested_v2_memory_and_cpu_quotas(self):
        mount, leaf = self.mount("cgroup2", "", "/team/build")
        self.file(mount, "memory.max", 12 * GIB)
        self.file(mount, "memory.current", 2 * GIB)
        self.file(mount / "team", "memory.max", 8 * GIB)
        self.file(mount / "team", "memory.current", 4 * GIB)
        self.file(leaf, "memory.max", "max")
        self.file(leaf, "cpu.max", "250000 100000")
        self.file(mount, "cpu.max", "400000 100000")
        self.assertEqual(self.jobs("v8"), 1)
        self.assertEqual(self.jobs("mupdf"), 2)
        self.file(mount / "team", "memory.current", 0)
        self.assertEqual(self.jobs("v8"), 3)
        self.assertEqual(self.jobs("mupdf"), 3)
        self.file(leaf, "memory.max", GIB)
        self.assertEqual(self.jobs("mupdf"), 1)  # Missing current is treated conservatively.

    def test_delegated_v2_mount_and_unlimited_cpu(self):
        _, leaf = self.mount("cgroup2", "", "/docker/group", root="/docker")
        self.file(leaf, "memory.max", 6 * GIB)
        self.file(leaf, "memory.current", GIB)
        self.file(leaf, "cpu.max", "max 100000")
        self.assertEqual(self.jobs("mupdf"), 3)

    def test_v1_limits_unlimited_and_escaped_paths(self):
        mount, leaf = self.mount("cgroup", "memory", "/tenant")
        self.file(mount, "memory.limit_in_bytes", 2 ** 63 - 4096)
        self.file(leaf, "memory.limit_in_bytes", 6 * GIB)
        self.file(leaf, "memory.usage_in_bytes", GIB)
        self.assertEqual(self.jobs("mupdf"), 3)
        self.assertEqual(builder._mount_path(r"/some\040path"), Path("/some path"))
        mount, leaf = self.mount("cgroup", "cpu,cpuacct", "/tenant")
        self.file(leaf, "cpu.cfs_quota_us", 150000)
        self.file(leaf, "cpu.cfs_period_us", 100000)
        self.assertEqual(self.jobs("v8"), 2)
        self.file(leaf, "cpu.cfs_quota_us", -1)
        self.assertEqual(self.jobs("v8"), 24)

    def test_bad_cgroup_data_and_affinity_fallback(self):
        (self.proc / "self/cgroup").write_text("malformed\n")
        (self.proc / "self/mountinfo").write_text("broken\n")
        with mock.patch.object(builder.os, "sched_getaffinity", side_effect=OSError()), \
             mock.patch.object(builder.os, "cpu_count", return_value=4), \
             mock.patch.object(builder.subprocess, "check_output", return_value="4"):
            self.assertEqual(builder.automatic_jobs("v8", self.proc), 4)
        mount, leaf = self.mount("cgroup2", "", "/group")
        self.file(leaf, "memory.max", "bogus")
        self.file(leaf, "cpu.max", "broken")
        self.assertEqual(self.jobs("v8", 5), 5)
        (self.proc / "self/cgroup").write_text("1::/other/path\n")
        (self.proc / "self/mountinfo").write_text(f"1 0 0:1 /delegated {mount} rw - cgroup2 cgroup rw\n")
        self.assertEqual(self.jobs("v8", 5), 5)

    def test_builder_selects_per_dependency_and_respects_override(self):
        prefix, work = self.root / "prefix", self.root / "work"
        def execute(value):
            args = ["builder", "--prefix", str(prefix), "--work", str(work)]
            if value is not None:
                args += ["--jobs", value]
            with mock.patch.object(sys, "argv", args), \
                 mock.patch.object(builder, "compatible", return_value=False), \
                 mock.patch.object(builder, "automatic_jobs", side_effect=lambda kind: {"mupdf": 7, "v8": 3}[kind]) as auto, \
                 mock.patch.object(builder, "processor_limit", return_value=24), \
                 mock.patch.object(builder, "build_mupdf") as mupdf, mock.patch.object(builder, "build_v8") as v8:
                builder.main()
                return auto, mupdf, v8
        auto, mupdf, v8 = execute("auto")
        self.assertEqual(mupdf.call_args.args[2], 7)
        self.assertEqual(v8.call_args.args[2], 3)
        self.assertEqual(auto.call_count, 2)
        auto, mupdf, v8 = execute(None)
        self.assertEqual((mupdf.call_args.args[2], v8.call_args.args[2]), (7, 3))
        self.assertEqual(auto.call_count, 2)
        auto, mupdf, v8 = execute("11")
        self.assertEqual(mupdf.call_args.args[2], 11)
        self.assertEqual(v8.call_args.args[2], 11)
        auto.assert_not_called()
        auto, mupdf, v8 = execute("99")
        self.assertEqual((mupdf.call_args.args[2], v8.call_args.args[2]), (24, 24))
        auto.assert_not_called()
        for bad in ("0", "-1", "invalid", "1.5"):
            with self.subTest(bad=bad), mock.patch.object(sys, "argv", ["builder", "--prefix", str(prefix), "--work", str(work), "--jobs", bad]):
                with self.assertRaises(SystemExit):
                    builder.main()


if __name__ == "__main__":
    unittest.main()
