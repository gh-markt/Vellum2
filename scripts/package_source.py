#!/usr/bin/env python3
"""Package the current contents of Git-indexed files as a source ZIP."""
import argparse
import os
from pathlib import Path
import stat
import subprocess
import tempfile
import zipfile


def create_package(root: Path, output: Path) -> None:
    root = root.resolve()
    output = output.resolve()
    listing = subprocess.run(
        ["git", "-C", str(root), "ls-files", "--stage", "-z"],
        check=True, stdout=subprocess.PIPE,
    ).stdout
    entries = []
    for item in listing.split(b"\0"):
        if not item:
            continue
        metadata, name = item.split(b"\t", 1)
        mode, _, stage = metadata.split()
        if stage != b"0" or mode not in (b"100644", b"100755", b"120000"):
            raise ValueError("unmerged entries and submodules cannot be packaged")
        path = Path(os.fsdecode(name))
        if path.is_absolute() or ".." in path.parts:
            raise ValueError("unsafe Git index path")
        source = root / path
        if source == output:
            raise ValueError("package output is a tracked source file")
        entries.append((path, source, mode))
    if not entries:
        raise ValueError("Git index contains no files")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=output.parent, prefix=".vellum-package-",
                                         suffix=".tmp", delete=False) as temp:
            temporary = Path(temp.name)
        with zipfile.ZipFile(temporary, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for path, source, mode in entries:
                if mode == b"120000":
                    info = zipfile.ZipInfo(path.as_posix())
                    info.create_system = 3
                    info.external_attr = (stat.S_IFLNK | 0o777) << 16
                    archive.writestr(info, os.fsencode(os.readlink(source)))
                else:
                    if source.is_symlink() or not source.is_file():
                        raise ValueError(f"indexed file missing or not regular: {path}")
                    archive.write(source, path.as_posix())
                print(f"Added: {path.as_posix()!r}", flush=True)
        os.replace(temporary, output)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    create_package(args.root, args.output)
    print(f"Source package: {args.output}")


if __name__ == "__main__":
    main()
