from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from windows import download, replace_upstream_sources, require_windows_binary, utility


class WindowsBuildTests(unittest.TestCase):
    def test_upstream_wins_without_overwriting_windows_layer(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            snapshot, destination = root / "upstream", root / "windows"
            (snapshot / "src").mkdir(parents=True)
            (destination / "src").mkdir(parents=True)
            (snapshot / "src" / "game.c").write_text("upstream")
            (destination / "src" / "game.c").write_text("local")
            (destination / "src" / "obsolete.c").write_text("old")
            (destination / "Makefile.windows").write_text("windows")
            manifest = replace_upstream_sources(snapshot, destination)
            self.assertEqual((destination / "src" / "game.c").read_text(), "upstream")
            self.assertFalse((destination / "src" / "obsolete.c").exists())
            self.assertEqual((destination / "Makefile.windows").read_text(), "windows")
            self.assertIn("src/game.c", manifest)

    def test_failed_download_does_not_create_final_artifact(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / "tools.zip"
            with patch("windows.run", side_effect=RuntimeError("transfer failed")):
                with self.assertRaisesRegex(RuntimeError, "transfer failed"):
                    download("https://example.invalid/tools.zip", destination)
            self.assertFalse(destination.exists())
            self.assertFalse(destination.with_suffix(".zip.download").exists())

    def test_rejects_linux_compiler(self):
        with tempfile.TemporaryDirectory() as directory:
            compiler = Path(directory) / "cc1.exe"
            compiler.write_bytes(b"\x7fELFfake")
            with self.assertRaisesRegex(RuntimeError, "Not a Windows executable"):
                require_windows_binary(compiler)

    def test_windows_file_utilities(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "input.bin"
            destination = Path(directory) / "subdirectory" / "output.bin"
            source.write_bytes(b"\x01\x02\x03")
            utility("mkdir", [str(destination.parent)])
            utility("copy", [str(source), str(destination)])
            self.assertEqual(destination.read_bytes(), source.read_bytes())
            utility("touch", [str(destination.parent / "stamp")])
            self.assertTrue((destination.parent / "stamp").exists())


if __name__ == "__main__":
    unittest.main()
