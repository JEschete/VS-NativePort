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
            output.write_text("previous output")
            timestamp = output.stat().st_mtime_ns
            for allow_function_stubs in (False, True):
                with self.subTest(allow_function_stubs=allow_function_stubs):
                    with self.assertRaisesRegex(ValueError, "_state.*Generic data buffers are forbidden"):
                        generate(root, root, output, allow_function_stubs=allow_function_stubs)
            self.assertEqual(output.read_text(), "previous output")
            self.assertEqual(output.stat().st_mtime_ns, timestamp)

    def test_missing_function_requires_explicit_diagnostic_opt_in(self):
        header = struct.pack("<HHIIIHH", 0x14C, 0, 0, 20, 1, 0, 0)
        symbol = struct.pack("<8sIhHBB", b"_hello\0\0", 0, 0, 0x20, 2, 0)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "test.obj").write_bytes(header + symbol + struct.pack("<I", 4))
            output = root / "stubs.c"
            with self.assertRaisesRegex(ValueError, "hello \\(function\\)"):
                generate(root, root, output)
            self.assertFalse(output.exists())
            generate(root, root, output, allow_function_stubs=True)
            self.assertIn('_native_unimplemented("hello")', output.read_text())
            self.assertNotIn("unsigned char", output.read_text())
            timestamp = output.stat().st_mtime_ns
            generate(root, root, output, allow_function_stubs=True)
            self.assertEqual(output.stat().st_mtime_ns, timestamp)

    def test_crt_only_object_needs_no_replacement_definitions(self):
        header = struct.pack("<HHIIIHH", 0x14C, 0, 0, 20, 1, 0, 0)
        symbol = struct.pack("<8sIhHBB", b"_malloc\0", 0, 0, 0, 2, 0)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "test.obj").write_bytes(header + symbol + struct.pack("<I", 4))
            output = root / "stubs.c"
            generate(root, root, output)
            self.assertNotIn("void malloc", output.read_text())
            self.assertNotIn("[4096]", output.read_text())

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
