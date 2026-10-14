from pathlib import Path
import unittest

import yaml

from matching import retail_code_segment, retail_readonly_section


class ReadOnlyRangeTests(unittest.TestCase):
    def setUp(self):
        root = Path(__file__).resolve().parents[2]
        self.configuration = yaml.safe_load((root / "config/BATTLE/BATTLE.PRG/splat.yaml").read_text())

    def test_numeric_yaml_owner_uses_complete_configured_range(self):
        source = Path("src/BATTLE/BATTLE.PRG/49268.c")
        self.assertEqual(retail_readonly_section(self.configuration, source), {
            "section": ".rodata", "file_offset": 0xDF4, "address": 0x800695F4, "size": 40,
        })

    def test_string_yaml_owner_uses_complete_configured_range(self):
        source = Path("src/BATTLE/BATTLE.PRG/146C.c")
        self.assertEqual(retail_readonly_section(self.configuration, source), {
            "section": ".rodata", "file_offset": 0, "address": 0x80068800, "size": 1716,
        })

    def test_unknown_owner_has_no_verified_range(self):
        self.assertIsNone(retail_readonly_section(self.configuration, Path("src/BATTLE/BATTLE.PRG/absent.c")))


class CodeSegmentTests(unittest.TestCase):
    def test_main_header_is_not_the_code_segment(self):
        root = Path(__file__).resolve().parents[2]
        configuration = yaml.safe_load((root / "config/SLUS_010.40/splat.yaml").read_text())
        segment = retail_code_segment(configuration)
        self.assertEqual(segment["start"], 0x800)
        self.assertEqual(segment["vram"], 0x80010000)
        self.assertEqual(str(segment.get("name") or f"{segment['start']:X}"), "800")

    def test_named_battle_code_segment_is_preserved(self):
        root = Path(__file__).resolve().parents[2]
        configuration = yaml.safe_load((root / "config/BATTLE/BATTLE.PRG/splat.yaml").read_text())
        segment = retail_code_segment(configuration)
        self.assertEqual(segment["start"], 0)
        self.assertEqual(segment["vram"], 0x80068800)
        self.assertEqual(segment["name"], "BATTLE.PRG")

    def test_missing_code_segment_is_refused(self):
        with self.assertRaises(RuntimeError):
            retail_code_segment({"segments": [[0, "header"], {"start": 0x800, "type": "data"}]})


if __name__ == "__main__":
    unittest.main()