#!/usr/bin/env python3
import hashlib
import json
import pathlib
root = pathlib.Path(__file__).resolve().parents[1]
expected = json.loads((root / "docs/catalog-checksums.json").read_text())
for name, digest in expected.items():
    data = (root / "docs" / name).read_bytes()
    if hashlib.sha256(data).hexdigest() != digest:
        raise SystemExit(f"immutable catalog changed: {name}")
    if not name.endswith(".json"):
        continue
    catalog = json.loads(data)
    ids = [feature["id"] for feature in catalog["features"]]
    assert len(ids) == len(set(ids)) == catalog["summary"]["features"]
print("Catalog bytes, JSON syntax and unique IDs verified")
