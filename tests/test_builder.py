#!/usr/bin/env python3
import importlib.util
import json
import pathlib
import tempfile
import unittest
root = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("builder", root / "scripts/prepare_dependencies.py")
builder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(builder)
class BuilderTests(unittest.TestCase):
    def test_missing_and_invalid_receipts(self):
        with tempfile.TemporaryDirectory() as path:
            prefix = pathlib.Path(path)
            self.assertFalse(builder.compatible(prefix, "mupdf"))
            receipt = prefix / "share/vellum/mupdf.json"
            receipt.parent.mkdir(parents=True)
            receipt.write_text("broken")
            self.assertFalse(builder.compatible(prefix, "mupdf"))
    def test_static_install_contract(self):
        with tempfile.TemporaryDirectory() as path:
            prefix = pathlib.Path(path)
            for name in ("lib/libv8_monolith.a", "include/v8/v8.h"):
                target = prefix / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(b"fixture")
            data = {"version": builder.LOCK["v8"]["version"], "machine": builder.platform.machine(),
                    "gn_args": builder.LOCK["v8_gn_args"]}
            receipt = prefix / "share/vellum/v8.json"
            receipt.parent.mkdir(parents=True)
            receipt.write_text(json.dumps(data))
            self.assertTrue(builder.compatible(prefix, "v8"))
            data["gn_args"] = "incompatible pointer compression"
            receipt.write_text(json.dumps(data))
            self.assertFalse(builder.compatible(prefix, "v8"))
            data["gn_args"] = builder.LOCK["v8_gn_args"]
            data["machine"] = "wrong-machine"
            receipt.write_text(json.dumps(data))
            self.assertFalse(builder.compatible(prefix, "v8"))
if __name__ == "__main__":
    unittest.main()
