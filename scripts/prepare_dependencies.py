#!/usr/bin/env python3
"""Resumable local static dependency builder. Does not install OS packages."""
import argparse
import fcntl
import json
import os
import pathlib
import platform
import shutil
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]
LOCK = json.loads((ROOT / "docs/dependencies.lock.json").read_text())

def run(arguments, cwd=None, env=None):
    print("+", " ".join(map(str, arguments)), flush=True)
    return subprocess.run(list(map(str, arguments)), cwd=cwd, env=env, check=True)

def compatible(prefix, kind):
    try:
        receipt = json.loads((prefix / f"share/vellum/{kind}.json").read_text())
        files = (["lib/libmupdf.a", "lib/libmupdf-third.a", "include/mupdf/fitz.h"]
                 if kind == "mupdf" else ["lib/libv8_monolith.a", "include/v8/v8.h"])
        return (isinstance(receipt, dict) and receipt.get("version") == LOCK[kind]["version"] and
                receipt.get("machine") == platform.machine() and
                (kind != "v8" or receipt.get("gn_args") == LOCK["v8_gn_args"]) and
                all((prefix / name).is_file() for name in files))
    except (OSError, ValueError):
        return False

def receipt(prefix, kind, source, extra=None):
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip()
    data = {"version": LOCK[kind]["version"], "commit": commit, "machine": platform.machine()}
    if kind == "v8":
        data["gn_args"] = LOCK["v8_gn_args"]
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
    options = ["build=release", "USE_MUJS=no", "USE_SYSTEM_LIBS=no", "HAVE_X11=no", "HAVE_GLUT=no",
               "HAVE_CURL=no", "HAVE_LEPTONICA=no", "HAVE_TESSERACT=no", "XCFLAGS=-fPIC"]
    run(["make", f"-j{jobs}", *options, "libs"], source)
    (prefix / "lib").mkdir(parents=True, exist_ok=True)
    for name in ("libmupdf.a", "libmupdf-third.a"):
        shutil.copy2(source / "build/release" / name, prefix / "lib" / name)
    shutil.copytree(source / "include/mupdf", prefix / "include/mupdf", dirs_exist_ok=True)
    copy_licenses(source, prefix / "share/vellum/licenses/mupdf")
    receipt(prefix, "mupdf", source)

def build_v8(work, prefix, jobs):
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
    cpu = {"x86_64": "x64", "aarch64": "arm64"}.get(platform.machine())
    if not cpu:
        raise SystemExit("V8 builder currently supports Linux x86_64 and aarch64")
    output = source / "out/vellum"
    run(["gn", "gen", output, "--args=" + LOCK["v8_gn_args"] + f' target_cpu="{cpu}"'], source, env)
    run(["autoninja", "-C", output, f"-j{jobs}", "v8_monolith"], source, env)
    (prefix / "lib").mkdir(parents=True, exist_ok=True)
    shutil.copy2(output / "obj/libv8_monolith.a", prefix / "lib/libv8_monolith.a")
    shutil.copytree(source / "include", prefix / "include/v8", dirs_exist_ok=True)
    # Some V8 releases generate public configuration headers.
    for header in (output / "gen/include").glob("*.h"):
        shutil.copy2(header, prefix / "include/v8" / header.name)
    copy_licenses(source, prefix / "share/vellum/licenses/v8")
    depot_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=depot, text=True).strip()
    receipt(prefix, "v8", source, {"depot_tools_commit": depot_commit})

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prefix", type=pathlib.Path, required=True)
    parser.add_argument("--work", type=pathlib.Path, required=True)
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--mupdf-root", default="")
    parser.add_argument("--v8-root", default="")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
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
                build(args.work, prefix, args.jobs)
    print("Static dependencies prepared successfully", flush=True)

if __name__ == "__main__":
    main()
