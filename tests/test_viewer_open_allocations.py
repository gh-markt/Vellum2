#!/usr/bin/env python3
"""Exercise allocation failures while replacing a real viewer document."""
import concurrent.futures
import os
import pathlib
import subprocess
import sys
import tempfile


def one_page_pdf(path):
    objects = [b"<< /Type /Catalog /Pages 2 0 R >>",
               b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
               b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] >>"]
    result = bytearray(b"%PDF-1.7\n")
    offsets = [0]
    for number, value in enumerate(objects, 1):
        offsets.append(len(result))
        result.extend(f"{number} 0 obj\n".encode() + value + b"\nendobj\n")
    xref = len(result)
    result.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode())
    for offset in offsets[1:]:
        result.extend(f"{offset:010} 00000 n \n".encode())
    result.extend(f"trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
    path.write_bytes(result)


def execute(binary, fixture, target, index):
    process = subprocess.run([binary, fixture, str(index), str(target)], capture_output=True,
                             text=True, timeout=30, check=False)
    if process.returncode != 0:
        raise RuntimeError(f"allocation {index} failed (exit {process.returncode}): {process.stderr[-1000:]}")
    values = [line.removeprefix("VELLUM_ALLOCATIONS=") for line in process.stdout.splitlines()
              if line.startswith("VELLUM_ALLOCATIONS=")]
    if len(values) != 1:
        raise RuntimeError(f"allocation {index} did not report its count")
    count = int(values[0])
    if index >= 0 and count != index + 1:
        raise RuntimeError(f"allocation {index} was not injected at the expected point")
    return count


def main():
    if len(sys.argv) != 3:
        raise RuntimeError("expected allocation-test binary and PDF fixture")
    with tempfile.TemporaryDirectory(prefix="vellum-open-allocation-") as temporary:
        target = pathlib.Path(temporary) / "one-page.pdf"
        one_page_pdf(target)
        binary, fixture = sys.argv[1:]
        count = execute(binary, fixture, target, -1)
        if count == 0 or count > 1000:
            raise RuntimeError(f"unexpected number of document-open allocation points: {count}")
        with concurrent.futures.ThreadPoolExecutor(max_workers=min(4, os.cpu_count() or 1)) as pool:
            list(pool.map(lambda index: execute(binary, fixture, target, index), range(count)))
        print(f"Document replacement: {count} Vellum-origin allocation-failure cases")


if __name__ == "__main__":
    main()
