"""Synthetic controls for the content ledger; no repository files or disc data are read."""

from __future__ import annotations

import csv
import io
import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import content  # noqa: E402
from gates import VERIFY_FIELDS, read_csv  # noqa: E402

TIM_TEST = content.EVIDENCE["tim-png"][0]
FILES = [
    ("BG/001OP01A.FAR", 780, "raw"),
    ("BG/001OP01A.TIM", 33312, "raw"),
    ("EFFECT/PLG000.BIN", 0, "raw"),
    ("ENDING/ENDING.XA", 1024, "stream"),
    ("MAP/MAP001.MPD", 4096, "raw"),
    ("SYSTEM.CNF", 68, "raw"),
]
OUTPUT = {
    "id": "image.bg.001op01a", "source": "BG/001OP01A.TIM", "source_sha256": "0" * 64, "converter": "tim-png",
    "files": ["textures/bg/001OP01A.png", "textures/bg/001OP01A.image.json"],
    "dependencies": [{"source": "BG/001OP01A.FAR", "source_sha256": "1" * 64}],
}


class Fixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.write("Docs/ASSET_PIPELINE.md", f"# Pipeline\n\n{content.SUMMARY_BEGIN}\n\n{content.SUMMARY_END}\n")
        self.write("Native/Content/include/Content/ConversionManifest.hpp",
                   'inline constexpr std::array kRequiredConverters{\n'
                   '    ConverterVersion{.name = "tim-png", .version = 1},\n};\n')
        self.write("Native/Tests/src/ImageTests.cpp", f'TEST_CASE("{TIM_TEST}", "[extract]") {{}}\n')
        self.verify("pass")
        self.data(FILES, [OUTPUT])

    def write(self, path: str, text: str) -> None:
        target = self.root / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(text, encoding="utf-8")

    def verify(self, outcome: str) -> None:
        record = {name: "x" for name in VERIFY_FIELDS}
        record.update(record="VR-TIM", references=f"test:{TIM_TEST}", outcome=outcome)
        buffer = io.StringIO()
        writer = csv.DictWriter(buffer, fieldnames=VERIFY_FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerow(record)
        self.write("Docs/ledgers/VERIFY_LEDGER.csv", buffer.getvalue())

    def data(self, files: list[tuple[str, int, str]], outputs: list[dict]) -> None:
        listing = "lba\tsize\tform\tpath\n" + "".join(f"{index}\t{size}\t{form}\t{path}\n"
                                                     for index, (path, size, form) in enumerate(files))
        self.write("Native/data/files.tsv", listing)
        self.write("Native/data/manifest.json", json.dumps({"schema_version": 1, "outputs": outputs}))

    def rows(self) -> dict[str, dict[str, str]]:
        return {row["path"]: row for row in read_csv(content.ledger_path(self.root))}

    def edit(self, path: str, changes: dict[str, str]) -> None:
        rows = list(self.rows().values())
        for row in rows:
            if row["path"] == path:
                row.update(changes)
        content.write_ledger(self.root, rows)

    def rules(self) -> list[str]:
        return sorted(issue.rule for issue in content.check(self.root))


class ContentLedgerTests(unittest.TestCase):
    def setUp(self) -> None:
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.fixture = Fixture(Path(directory.name))

    def test_sync_gives_every_disc_file_a_status_and_reason(self) -> None:
        content.sync(self.fixture.root)
        rows = self.fixture.rows()
        self.assertEqual(list(rows), sorted(path for path, _, _ in FILES))
        tim = rows["BG/001OP01A.TIM"]
        self.assertEqual((tim["status"], tim["converter"], tim["decoder"], tim["round_trip"]),
                         ("converted", "tim-png", "Content::DecodeTim", "pass"))
        self.assertEqual(tim["output"], "textures/bg/001OP01A.png;textures/bg/001OP01A.image.json")
        layout = rows["BG/001OP01A.FAR"]
        self.assertEqual((layout["status"], layout["notes"]),
                         ("unconverted", "dependency of image.bg.001op01a; converts under #210"))
        placeholder = rows["EFFECT/PLG000.BIN"]
        self.assertEqual((placeholder["status"], placeholder["kind"], placeholder["converter"], placeholder["notes"]),
                         ("skipped", "placeholder", "n/a", "zero-byte placeholder"))
        self.assertEqual((rows["SYSTEM.CNF"]["status"], rows["SYSTEM.CNF"]["notes"]),
                         ("skipped", content.DROPPED["SYSTEM.CNF"]))
        self.assertIn("extracted in stream form", rows["ENDING/ENDING.XA"]["notes"])
        self.assertEqual((rows["MAP/MAP001.MPD"]["status"], rows["MAP/MAP001.MPD"]["round_trip"]),
                         ("unconverted", "pending"))
        self.assertEqual(self.fixture.rules(), [])

    def test_material_section_does_not_claim_whole_executable_conversion(self) -> None:
        material = {"id": "catalog.items.materials", "source": "SLUS_010.40",
                    "converter": "material-csv", "files": ["catalogs/items/materials.csv"]}
        self.fixture.data([*FILES, ("SLUS_010.40", 337920, "raw")], [OUTPUT, material])
        content.sync(self.fixture.root)
        row = self.fixture.rows()["SLUS_010.40"]
        self.assertEqual((row["status"], row["round_trip"], row["converter"], row["output"]),
                         ("unconverted", "pending", "", ""))
        self.assertIn("partial catalog.items.materials (material-csv; G01905 at 0x3F5DC, 240 bytes)", row["notes"])
        self.assertIn("converts under #499, #500, #502", row["notes"])
        self.assertEqual(self.fixture.rules(), [])

    def test_section_converters_refuse_mismatched_sources(self) -> None:
        material = {"id": "catalog.items.materials", "source": "BG/001OP01A.TIM",
                    "converter": "material-csv", "files": ["catalogs/items/materials.csv"]}
        self.fixture.data(FILES, [OUTPUT, material])
        with self.assertRaisesRegex(SystemExit, "invalid section-only conversion registration"):
            content.sync(self.fixture.root)

    def test_native_text_sections_do_not_claim_whole_font_or_text_sources(self) -> None:
        outputs = [{"id": identity, "source": source, "converter": "save-text-json",
                    "files": ["text/strings.en.json"]}
                   for identity, (source, _) in content.TEXT_SECTIONS.items()]
        self.fixture.data([*FILES, ("MENU/MCMAN.BIN", 3014, "raw"),
                           ("TITLE/TITLE.PRG", 554568, "raw")], [OUTPUT, *outputs])
        content.sync(self.fixture.root)
        for source in ("MENU/MCMAN.BIN", "TITLE/TITLE.PRG"):
            row = self.fixture.rows()[source]
            self.assertEqual((row["status"], row["converter"], row["output"]),
                             ("unconverted", "", ""))
            self.assertIn("partial", row["notes"])
        self.assertEqual(self.fixture.rules(), [])

    def test_round_trip_passes_only_with_a_passing_record_for_an_existing_test(self) -> None:
        self.fixture.verify("partial")
        content.sync(self.fixture.root)
        self.assertEqual(self.fixture.rows()["BG/001OP01A.TIM"]["round_trip"], "pending")
        self.assertEqual(self.fixture.rules(), [])
        self.fixture.edit("BG/001OP01A.TIM", {"round_trip": "pass"})
        self.assertIn("round-trip-unbacked", self.fixture.rules())
        self.fixture.verify("pass")
        self.fixture.write("Native/Tests/src/ImageTests.cpp", "")
        self.assertIn("round-trip-unbacked", self.fixture.rules())

    def test_check_without_extracted_data_still_enforces_the_row_rules(self) -> None:
        content.sync(self.fixture.root)
        shutil.rmtree(self.fixture.root / "Native" / "data")
        self.assertEqual(self.fixture.rules(), [])
        cases = [
            ("MAP/MAP001.MPD", {"status": "skipped", "decoder": "n/a", "converter": "n/a", "output": "n/a",
                                "round_trip": "n/a"}, "skip-unexplained"),
            ("EFFECT/PLG000.BIN", {"status": "unconverted"}, "skip-missing"),
            ("MAP/MAP001.MPD", {"kind": "zone"}, "kind-mismatch"),
            ("MAP/MAP001.MPD", {"status": "done"}, "status-unknown"),
            ("BG/001OP01A.TIM", {"converter": "gim-png"}, "converter-unknown"),
            ("BG/001OP01A.TIM", {"output": ""}, "output-missing"),
            ("BG/001OP01A.FAR", {"converter": "tim-png"}, "unconverted-resolved"),
        ]
        for path, changes, rule in cases:
            with self.subTest(rule=rule):
                original = dict(self.fixture.rows()[path])
                self.fixture.edit(path, changes)
                self.assertIn(rule, self.fixture.rules())
                self.fixture.edit(path, original)

    def test_check_fails_on_a_ledger_or_summary_the_data_no_longer_gives(self) -> None:
        content.sync(self.fixture.root)
        self.fixture.data([*FILES, ("MAP/MAP002.MPD", 4096, "raw")], [OUTPUT])
        self.assertEqual(self.fixture.rules(), ["ledger-stale"])
        content.sync(self.fixture.root)
        self.fixture.write("Docs/ASSET_PIPELINE.md",
                           f"# Pipeline\n\n{content.SUMMARY_BEGIN}\n\nold\n\n{content.SUMMARY_END}\n")
        self.assertEqual(self.fixture.rules(), ["tracker-stale"])

    def test_a_runtime_converter_without_a_decoder_or_evidence_fails(self) -> None:
        content.sync(self.fixture.root)
        self.fixture.write("Native/Content/include/Content/ConversionManifest.hpp",
                           'ConverterVersion{.name = "tim-png", .version = 1},\n'
                           'ConverterVersion{.name = "gim-png", .version = 1},\n')
        self.assertEqual(self.fixture.rules(), ["converter-untracked"])

    def test_an_alias_lists_its_targets_files(self) -> None:
        alias = {"id": "image.bg.002op01a", "source": "BG/002OP01A.TIM", "source_sha256": "0" * 64,
                 "converter": "tim-png", "files": [], "dependencies": [], "alias_of": OUTPUT["id"]}
        self.fixture.data([*FILES, ("BG/002OP01A.TIM", 33312, "raw")], [OUTPUT, alias])
        content.sync(self.fixture.root)
        row = self.fixture.rows()["BG/002OP01A.TIM"]
        self.assertEqual((row["status"], row["output"], row["notes"]),
                         ("converted", "textures/bg/001OP01A.png;textures/bg/001OP01A.image.json",
                          "image.bg.002op01a (alias of image.bg.001op01a)"))
        self.assertEqual(self.fixture.rules(), [])

    def test_sync_refuses_a_file_no_rule_covers(self) -> None:
        self.fixture.data([*FILES, ("NEW/THING.XYZ", 4, "raw")], [OUTPUT])
        with self.assertRaises(SystemExit):
            content.sync(self.fixture.root)

    def test_only_one_zero_byte_is_a_one_byte_placeholder(self) -> None:
        single = {"SE/EFFECT00.DAT": b"\0", "SE/EFFECT01.DAT": b"\1", "ENDING/NULL.DAT": b"\0"}
        self.fixture.data([*FILES, *((path, 1, "raw") for path in single)], [OUTPUT])
        for path, byte in single.items():
            target = self.fixture.root / "Native" / "data" / "disc" / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(byte)
        content.sync(self.fixture.root)
        rows = self.fixture.rows()
        self.assertEqual([(rows[path]["status"], rows[path]["kind"], rows[path]["notes"]) for path in single], [
            ("skipped", "placeholder", "one-byte zero placeholder"),
            ("unconverted", "sound-effect sequences", "converts under #501"),
            ("skipped", "filler", content.DROPPED["ENDING/NULL.DAT"]),
        ])
        self.assertEqual(self.fixture.rules(), [])
        shutil.rmtree(self.fixture.root / "Native" / "data")
        self.assertEqual(self.fixture.rules(), [])


if __name__ == "__main__":
    unittest.main()
