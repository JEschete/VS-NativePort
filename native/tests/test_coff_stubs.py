from pathlib import Path
import struct
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from coff_stubs import CRT_SYMBOLS, generate, symbols


class CoffSymbolTests(unittest.TestCase):
    def test_compiler_arithmetic_helpers_are_never_stubbed(self):
        self.assertIn("_aulldiv", CRT_SYMBOLS)
        self.assertIn("_allmul", CRT_SYMBOLS)

    def test_c_name_starting_with_underscore_is_not_a_crt_symbol(self):
        header = struct.pack("<HHIIIHH", 0x14C, 0, 0, 20, 1, 0, 0)
        symbol = struct.pack("<8sIhHBB", b"__state\0", 0, 0, 0, 2, 0)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "test.obj").write_bytes(header + symbol + struct.pack("<I", 4))
            output = root / "stubs.c"
            generate(root, root, output)
            self.assertIn("unsigned char _state[4096]", output.read_text())
            timestamp = output.stat().st_mtime_ns
            generate(root, root, output)
            self.assertEqual(output.stat().st_mtime_ns, timestamp)

    def test_external_function_symbol(self):
        header = struct.pack("<HHIIIHH", 0x14C, 0, 0, 20, 1, 0, 0)
        symbol = struct.pack("<8sIhHBB", b"_hello\0\0", 0, 0, 0x20, 2, 0)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.obj"
            path.write_bytes(header + symbol + struct.pack("<I", 4))
            self.assertEqual(list(symbols(path)), [("_hello", 0, 0x20)])

    def test_invalid_symbol_bounds_fail(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "test.obj"
            path.write_bytes(struct.pack("<HHIIIHH", 0x14C, 0, 0, 1000, 10, 0, 0))
            with self.assertRaisesRegex(ValueError, "symbol bounds"):
                list(symbols(path))


if __name__ == "__main__":
    unittest.main()
