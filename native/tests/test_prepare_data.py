from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from prepare_data import asset_ranges, asset_symbols, disc_entries


class NativeDataTests(unittest.TestCase):
    def test_publisher_palette_precedes_pixels(self):
        config = {"segments": [{"subsegments": [{"start": 0, "type": "indexed",
            "name": "publisher", "width": 256, "height": 48}]}]}
        self.assertEqual(asset_symbols(config, "publisher"), [("publisher_clut", 0), ("publisher", 32)])

    def test_developer_palettes_follow_pixels(self):
        config = {"segments": [{"subsegments": [{"start": 0, "type": "indexed",
            "name": "developer", "width": 128, "height": 13,
            "num_cluts": 2, "cluts_after": True}]}]}
        self.assertEqual(asset_symbols(config, "developer"), [("developer", 0), ("developer_clut", 832)])

    def test_eight_bit_menu_palette_has_512_bytes(self):
        config = {"segments": [{"subsegments": [{"start": 0, "type": "indexed",
            "name": "_saveMenuBg", "width": 192, "height": 240, "bitdepth": 8}]}]}
        self.assertEqual(asset_symbols(config, "_saveMenuBg"), [("_saveMenuBg_clut", 0), ("_saveMenuBg", 512)])

    def test_sized_rgb_header_and_pixels_share_storage(self):
        config = {"segments": [{"subsegments": [[0, "rgb555Sized", "_nowLoading"]]}, [100]]}
        self.assertEqual(asset_symbols(config, "_nowLoading"), [("_nowLoading_header", 0), ("_nowLoading_data", 4)])
        self.assertEqual(list(asset_ranges(config)), [(0, 100, "_nowLoading")])

    def test_asset_range_uses_next_upstream_boundary(self):
        config = {"segments": [{"type": "code", "subsegments": [
            [0, "c", "code"], {"start": 16, "type": "indexed", "name": "font"},
            [48, ".bss", "tail"]]}, [64]]}
        self.assertEqual(list(asset_ranges(config)), [(16, 48, "font")])

    def test_disc_sector_rounding(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "data").mkdir()
            (root / "data" / "TITLE.STR").write_bytes(b"x" * 2049)
            layout = root / "layout.txt"
            layout.write_text(f'File | TITLE.STR | 2 | 23 | 00:02:23 | 2049 | {root / "data" / "TITLE.STR"}\n')
            self.assertEqual(disc_entries(layout, root / "data"), [(23, 2, "TITLE.STR", "TITLE.STR")])

    def test_form2_uses_reported_sector_count(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "MOV").mkdir()
            movie = root / "MOV" / "TITLE.STR"
            movie.write_bytes(b"x" * 2336)
            layout = root / "layout.txt"
            layout.write_text(f'XA | TITLE.STR | 1 | 23 | 00:02:23 | 2336 | {movie}\n')
            self.assertEqual(disc_entries(layout, root)[0][1], 1)


if __name__ == "__main__":
    unittest.main()
