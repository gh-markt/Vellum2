#!/usr/bin/env python3
"""Resumable local static dependency builder. Does not install OS packages."""
import argparse
import fcntl
import hashlib
import json
import os
import pathlib
import platform
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / "docs/dependencies.lock.json").read_text())
V8_ICU_PATCH = ROOT / "patches/v8-icu79-unicode18.patch"

GIB = 1024 ** 3
# Conservative planning estimates, not measured per-process guarantees.
JOB_MEMORY = {"mupdf": GIB, "v8": 2 * GIB}

def _mount_path(text):
    return pathlib.PurePosixPath(re.sub(r"\\([0-7]{3})", lambda m: chr(int(m.group(1), 8)), text))

def _cgroup_mounts(proc):
    """Find this process's visible cgroup paths, including delegated mounts."""
    try:
        groups = [line.split(":", 2) for line in (proc / "self/cgroup").read_text().splitlines()]
        mounts = (proc / "self/mountinfo").read_text().splitlines()
    except OSError:
        return []
    result = []
    for line in mounts:
        fields = line.split()
        try:
            separator = fields.index("-")
            filesystem = fields[separator + 1]
            controllers = set(fields[separator + 3].split(","))
            root, mount = _mount_path(fields[3]), pathlib.Path(str(_mount_path(fields[4])))
            for _, names, group in groups:
                names = set(names.split(","))
                relevant = filesystem == "cgroup2" and names == {""}
                relevant |= filesystem == "cgroup" and bool(names & controllers & {"memory", "cpu"})
                if relevant:
                    try:
                        location = mount / _mount_path(group).relative_to(root)
                    except ValueError:
                        continue
                    result.append((filesystem, names, mount, location))
        except (IndexError, ValueError):
            continue
    return result

def _read_integer(path):
    try:
        value = path.read_text().strip()
        return int(value) if value != "max" else None
    except (OSError, ValueError):
        return None

def _cgroup_resources(proc):
    memory = None
    cpus = None
    for filesystem, controllers, mount, location in _cgroup_mounts(proc):
        for directory in (location, *location.parents):
            if directory != mount and mount not in directory.parents:
                break
            if filesystem == "cgroup2" or "memory" in controllers:
                limit_name = "memory.max" if filesystem == "cgroup2" else "memory.limit_in_bytes"
                usage_name = "memory.current" if filesystem == "cgroup2" else "memory.usage_in_bytes"
                limit = _read_integer(directory / limit_name)
                if limit is not None and 0 <= limit < 2 ** 60:
                    used = _read_integer(directory / usage_name)
                    available = max(0, limit - used) if used is not None and used >= 0 else 0
                    memory = available if memory is None else min(memory, available)
            if filesystem == "cgroup2":
                try:
                    quota, period = (directory / "cpu.max").read_text().split()
                    quota = None if quota == "max" else int(quota)
                    period = int(period)
                except (OSError, ValueError):
                    quota, period = None, None
            elif "cpu" in controllers:
                quota = _read_integer(directory / "cpu.cfs_quota_us")
                period = _read_integer(directory / "cpu.cfs_period_us")
            else:
                quota, period = None, None
            if quota is not None and period and quota > 0 and period > 0:
                count = max(1, (quota + period - 1) // period)
                cpus = count if cpus is None else min(cpus, count)
    return memory, cpus

def processor_limit():
    """Use the smaller of the available affinity and nproc counts."""
    try:
        cpus = len(os.sched_getaffinity(0))
    except (AttributeError, OSError):
        cpus = os.cpu_count() or 1
    try:
        nproc = int(subprocess.check_output(["nproc"], text=True,
                                            stderr=subprocess.DEVNULL, timeout=5).strip())
        if nproc > 0:
            cpus = min(cpus, nproc)
    except (OSError, ValueError, subprocess.SubprocessError):
        pass  # Affinity and cgroup quotas still provide a cap when nproc is unavailable.
    return max(1, cpus)

def automatic_jobs(kind, proc=pathlib.Path("/proc")):
    """Budget jobs from available RAM, nproc, affinity and visible cgroup limits."""
    cpus = processor_limit()
    try:
        memory_info = (proc / "meminfo").read_text().splitlines()
        available = next(int(line.split()[1]) * 1024 for line in memory_info
                         if line.startswith("MemAvailable:"))
    except (OSError, ValueError, IndexError, StopIteration):
        available = 0
    group_memory, group_cpus = _cgroup_resources(proc)
    if group_memory is not None:
        available = min(available, group_memory)
    if group_cpus is not None:
        cpus = min(cpus, group_cpus)
    reserve = max(2 * GIB, available // 5)
    return max(1, min(cpus, max(0, available - reserve) // JOB_MEMORY[kind]))

def system_icu_version():
    """Identify the matching ICU libraries that the local build will use."""
    try:
        output = subprocess.check_output(
            ["pkg-config", "--modversion", "icu-uc", "icu-i18n"],
            text=True, stderr=subprocess.DEVNULL)
    except (FileNotFoundError, subprocess.CalledProcessError) as error:
        raise RuntimeError("system ICU development packages and pkg-config are required") from error
    versions = output.splitlines()
    if (len(versions) != 2 or versions[0] != versions[1] or
            not all(part.isascii() and part.isdigit() and len(part) <= 9
                    for part in versions[0].split("."))):
        raise RuntimeError("system ICU libraries have inconsistent or invalid versions")
    if int(versions[0].split(".")[0]) < 73:
        raise RuntimeError("pinned V8 requires ICU 73 or newer")
    return versions[0]

def system_icu_libdir():
    """Return the ICU library directory relative to /usr for GN pkg-config."""
    try:
        values = [subprocess.check_output(
            ["pkg-config", "--variable=libdir", name],
            text=True, stderr=subprocess.DEVNULL).strip() for name in ("icu-uc", "icu-i18n")]
    except (FileNotFoundError, subprocess.CalledProcessError) as error:
        raise RuntimeError("system ICU development packages and pkg-config are required") from error
    path = pathlib.PurePosixPath(values[0])
    if values[0] != values[1] or ".." in path.parts:
        raise RuntimeError("system ICU libraries have inconsistent or invalid library paths")
    try:
        relative = path.relative_to("/usr")
    except ValueError as error:
        raise RuntimeError("system ICU library directory must be under /usr") from error
    if not relative.parts:
        raise RuntimeError("system ICU library directory must be under /usr")
    return relative.as_posix()

def system_icu_identity():
    """Record the ICU and backport used by a V8 archive."""
    return {"linkage": "system", "version": system_icu_version(),
            "libdir": system_icu_libdir(),
            "patch_sha256": hashlib.sha256(V8_ICU_PATCH.read_bytes()).hexdigest()}

def v8_system_gn_args(libdir, cpu):
    if cpu not in ("x64", "arm64"):
        raise ValueError("V8 builder currently supports Linux x86_64 and aarch64")
    return (LOCK["v8_gn_args"] + ' v8_depend_on_icu_data_file=false use_sysroot=true'
            ' target_sysroot="/" system_libdir=' + json.dumps(libdir) +
            ' target_cpu=' + json.dumps(cpu))

def v8_system_receipt_matches(receipt):
    try:
        identity = system_icu_identity()
        cpu = {"x86_64": "x64", "aarch64": "arm64"}.get(platform.machine())
        if cpu is None:
            return False
        return (receipt.get("system_icu") == identity and
                receipt.get("effective_gn_args") == v8_system_gn_args(identity["libdir"], cpu))
    except (RuntimeError, OSError):
        return False

def run(arguments, cwd=None, env=None):
    print("+", " ".join(map(str, arguments)), flush=True)
    return subprocess.run(list(map(str, arguments)), cwd=cwd, env=env, check=True)

def configure_v8_system_icu(source):
    """Apply the pinned checkout's unbundle shim and the reviewed V8 backport."""
    patch = str(V8_ICU_PATCH)
    check = subprocess.run(["git", "apply", "--check", patch], cwd=source,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    already_applied = False
    if check.returncode != 0:
        reverse = subprocess.run(["git", "apply", "--reverse", "--check", patch], cwd=source,
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if reverse.returncode != 0:
            raise RuntimeError("V8 ICU compatibility patch does not match the source checkout")
        already_applied = True
    target = source / "third_party/icu/BUILD.gn"
    shim = source / "build/linux/unbundle/icu.gn"
    backup = target.with_name("BUILD.gn.orig")
    if target.read_bytes() != shim.read_bytes():
        if backup.exists() and backup.read_bytes() != target.read_bytes():
            raise RuntimeError("existing ICU GN backup does not match the current build file")
        original = subprocess.run(["git", "show", "HEAD:BUILD.gn"], cwd=target.parent,
                                  capture_output=True)
        if original.returncode != 0 or original.stdout != target.read_bytes():
            raise RuntimeError("ICU GN build file is modified or lacks a verifiable checkout")
        run([sys.executable, source / "build/linux/unbundle/replace_gn_files.py",
             "--system-libraries", "icu"], source)
    if not already_applied:
        run(["git", "apply", patch], source)

def install_v8_archive(source, destination):
    descriptor, temporary = tempfile.mkstemp(prefix=".v8-monolith-", dir=destination.parent)
    try:
        os.close(descriptor)
        shutil.copy2(source, temporary)
        os.replace(temporary, destination)
    finally:
        pathlib.Path(temporary).unlink(missing_ok=True)

def compatible(prefix, kind):
    try:
        receipt = json.loads((prefix / f"share/vellum/{kind}.json").read_text())
        files = LOCK[f"{kind}_artifacts"]
        headers = receipt.get("headers") if isinstance(receipt, dict) else None
        header_root = f"include/{kind}/"
        return (isinstance(receipt, dict) and receipt.get("version") == LOCK[kind]["version"] and
                receipt.get("commit") == LOCK[kind]["revision"] and
                receipt.get("machine") == platform.machine() and
                isinstance(headers, list) and bool(headers) and
                all(isinstance(name, str) and name.startswith(header_root) and
                    ".." not in pathlib.PurePosixPath(name).parts and (prefix / name).is_file()
                    for name in headers) and
                (kind != "mupdf" or (receipt.get("make_args") == LOCK["mupdf_make_args"] and
                                     receipt.get("build_output") == LOCK["mupdf_build_output"])) and
                (kind != "v8" or (receipt.get("gn_args") == LOCK["v8_gn_args"] and
                                   v8_system_receipt_matches(receipt))) and
                all((prefix / name).is_file() for name in files))
    except (OSError, ValueError):
        return False

def receipt(prefix, kind, source, extra=None):
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    data = {"version": LOCK[kind]["version"], "commit": commit, "machine": platform.machine()}
    if kind == "v8":
        data["gn_args"] = LOCK["v8_gn_args"]
    else:
        data["make_args"] = LOCK["mupdf_make_args"]
        data["build_output"] = LOCK["mupdf_build_output"]
    data["headers"] = sorted(path.relative_to(prefix).as_posix()
                             for path in (prefix / "include" / kind).rglob("*.h")
                             if path.is_file())
    data.update(extra or {})
    path = prefix / f"share/vellum/{kind}.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(data, indent=2) + "\n")
    temporary.replace(path)

def checkout(name, destination):
    specification = LOCK[name]
    if not (destination / ".git").exists():
        run(["git", "clone", "--no-checkout", specification["url"], destination])
    run(["git", "fetch", "--depth=1", "origin", specification["revision"]], destination)
    run(["git", "checkout", "--detach", "FETCH_HEAD"], destination)

def copy_licenses(source, destination):
    destination.mkdir(parents=True, exist_ok=True)
    # Preserve upstream notices, including bundled dependencies, with relative paths.
    for path in source.rglob("*"):
        if path.is_file() and any(word in path.name.lower() for word in ("license", "copying", "copyright")):
            if ".git" in path.parts or "out" in path.relative_to(source).parts:
                continue
            target = destination / path.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)

def build_mupdf(work, prefix, jobs):
    source = work / "mupdf"
    checkout("mupdf", source)
    run(["git", "submodule", "update", "--init", "--recursive", "--depth=1"], source)
    # MuPDF's makefiles do not record all option changes in object dependencies.
    run(["make", *LOCK["mupdf_make_args"], "clean"], source)
    run(["make", f"-j{jobs}", *LOCK["mupdf_make_args"], "libs"], source)
    (prefix / "lib").mkdir(parents=True, exist_ok=True)
    for artifact in LOCK["mupdf_artifacts"]:
        if artifact.endswith(".a"):
            shutil.copy2(source / LOCK["mupdf_build_output"] / pathlib.Path(artifact).name,
                         prefix / artifact)
    shutil.copytree(source / "include/mupdf", prefix / "include/mupdf", dirs_exist_ok=True)
    copy_licenses(source, prefix / "share/vellum/licenses/mupdf")
    receipt(prefix, "mupdf", source)

def build_v8(work, prefix, jobs):
    icu = system_icu_identity()
    cpu = {"x86_64": "x64", "aarch64": "arm64"}.get(platform.machine())
    if not cpu:
        raise SystemExit("V8 builder currently supports Linux x86_64 and aarch64")
    depot = work / "depot_tools"
    checkout("depot_tools", depot)
    env = os.environ.copy()
    env["PATH"] = str(depot) + os.pathsep + env["PATH"]
    env["DEPOT_TOOLS_UPDATE"] = "0"
    env["DEPOT_TOOLS_METRICS"] = "0"
    run(["bash", depot / "ensure_bootstrap"], work, env)
    tree = work / "v8-tree"
    tree.mkdir(exist_ok=True)
    config = tree / ".gclient"
    config.write_text("solutions = " + repr([{"name": "v8", "url": LOCK["v8"]["url"],
                                            "deps_file": "DEPS", "managed": False,
                                            "custom_deps": {}}]) + "\n")
    run(["gclient", "sync", "--no-history", "--revision", "v8@" + LOCK["v8"]["revision"]], tree, env)
    source = tree / "v8"
    configure_v8_system_icu(source)
    output = source / "out/vellum"
    arguments = v8_system_gn_args(icu["libdir"], cpu)
    run(["gn", "gen", output, "--args=" + arguments], source, env)
    run(["autoninja", "-C", output, f"-j{jobs}", "v8_monolith"], source, env)
    if system_icu_identity() != icu:
        raise RuntimeError("system ICU or V8 backport changed during the build")
    (prefix / "lib").mkdir(parents=True, exist_ok=True)
    install_v8_archive(output / "obj/libv8_monolith.a", prefix / LOCK["v8_artifacts"][0])
    shutil.copytree(source / "include", prefix / "include/v8", dirs_exist_ok=True)
    # Some V8 releases generate public configuration headers.
    for header in (output / "gen/include").glob("*.h"):
        shutil.copy2(header, prefix / "include/v8" / header.name)
    copy_licenses(source, prefix / "share/vellum/licenses/v8")
    depot_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=depot, text=True).strip()
    receipt(prefix, "v8", source, {"depot_tools_commit": depot_commit, "system_icu": icu,
                                   "effective_gn_args": arguments})

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=pathlib.Path, required=True)
    parser.add_argument("--work", type=pathlib.Path, required=True)
    parser.add_argument("--jobs", default="auto", help="auto or an explicit positive job count")
    parser.add_argument("--mupdf-root", default="")
    parser.add_argument("--v8-root", default="")
    args = parser.parse_args()
    if args.jobs != "auto":
        try:
            args.jobs = int(args.jobs)
        except ValueError:
            parser.error("--jobs must be auto or a positive integer")
        if args.jobs < 1:
            parser.error("--jobs must be auto or a positive integer")
    if platform.system() != "Linux":
        parser.error("bootstrap dependency preparation currently supports Linux")
    args.prefix = args.prefix.resolve()
    args.work = args.work.resolve()
    args.work.mkdir(parents=True, exist_ok=True)
    args.prefix.mkdir(parents=True, exist_ok=True)
    # Serialize builders targeting the same prefix, even across CMake build directories.
    with (args.prefix / ".prepare.lock").open("w") as lockfile:
        fcntl.flock(lockfile, fcntl.LOCK_EX)
        for kind, selected, build in (("mupdf", args.mupdf_root, build_mupdf), ("v8", args.v8_root, build_v8)):
            prefix = pathlib.Path(selected).resolve() if selected else args.prefix
            if compatible(prefix, kind):
                print(f"Reusing compatible {kind}: {prefix}", flush=True)
            elif selected:
                raise SystemExit(f"{kind}: supplied root lacks compatible receipt/static headers/libraries: {prefix}. "
                                 "See docs/BUILDING.md for the external installation contract.")
            else:
                (prefix / f"share/vellum/{kind}.json").unlink(missing_ok=True)
                jobs = (automatic_jobs(kind) if args.jobs == "auto"
                        else min(args.jobs, processor_limit()))
                print(f"Building {kind} with {jobs} parallel job(s) ({args.jobs})", flush=True)
                build(args.work, prefix, jobs)
    print("Static dependencies prepared successfully", flush=True)

if __name__ == "__main__":
    main()
