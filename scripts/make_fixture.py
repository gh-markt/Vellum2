#!/usr/bin/env python3
"""Generate an exact, deterministic two-page PDF; no third-party fixture bytes."""
import pathlib
import sys
objects = [b"<< /Type /Catalog /Pages 2 0 R >>", b"<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>",
           b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Resources << >> /Contents 5 0 R >>",
           b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 100] /Resources << >> >>",
           b"<< /Length 26 >>\nstream\n1 0 0 rg 0 0 200 200 re f\nendstream"]
result = bytearray(b"%PDF-1.7\n")
offsets = [0]
for index, value in enumerate(objects, 1):
    offsets.append(len(result))
    result.extend(f"{index} 0 obj\n".encode() + value + b"\nendobj\n")
xref = len(result)
result.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode())
for offset in offsets[1:]:
    result.extend(f"{offset:010} 00000 n \n".encode())
result.extend(f"trailer\n<< /Size {len(offsets)} /Root 1 0 R >>\nstartxref\n{xref}\n%%EOF\n".encode())
pathlib.Path(sys.argv[1]).write_bytes(result)
