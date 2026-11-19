"""Tests for the native-evidence classification, resolution, citation refresh and spelling report in trace_queue.py
(#445, #457, #458)."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import trace_queue  # noqa: E402

HEADER = "Native\\Game\\src\\Rec.h"
SOURCE = "Native\\Game\\src\\Rec.cpp"
TEST = trace_queue.TESTS + "RecTests.cpp"


def field(name: str, line: int) -> tuple[str, dict]:
    return f"c:@S@Rec@FI@{name}", {"kind": "field", "record": "Rec", "path": HEADER, "name": name, "line": line}


def index() -> dict:
    decls = dict(field(name, line) for line, name in enumerate(
        ("read", "written", "copied", "sink", "flows", "tested", "unused", "described", "pad_4"), start=1))
    decls["c:@Rec_global"] = {"kind": "variable", "record": "", "path": HEADER, "name": "Rec_global", "line": 20}
    member = "c:@S@Rec@FI@"
    return {
        "decls": decls,
        "refs": {
            f"{member}read": [[SOURCE, 10, "VS::Use", "read", None]],
            f"{member}written": [[SOURCE, 12, "VS::Store", "write", None], [SOURCE, 11, "VS::Clear", "write", None]],
            f"{member}copied": [[SOURCE, 13, "VS::Copy", "copy", f"{member}sink"]],
            f"{member}flows": [[SOURCE, 14, "VS::Copy", "copy", f"{member}read"]],
            f"{member}tested": [[TEST, 5, "Check", "read", None], [SOURCE, 15, "", "layout", None]],
            "c:@Rec_global": [[SOURCE, 16, "VS::Store", "write", None]],
        },
        "loose": [],
    }


def member_row(name: str, meaning: str = "Not read", notes: str = "source:rec.c:1") -> dict:
    return {"id": f"T00001.{name}", "type_id": "T00001", "name": name, "final_name": name, "value": "",
            "meaning": meaning, "confidence": "low", "notes": notes}


TYPE = {"id": "T00001", "name": "Rec", "final_name": "Rec", "aliases": "", "final_location": HEADER,
        "copied": "x", "notes": "", "confidence": "high"}


class ClassifyTests(unittest.TestCase):
    def setUp(self):
        self.native = trace_queue.Native(index())

    def test_use_classes(self):
        member = "c:@S@Rec@FI@"
        for name, expected in (("read", "read"), ("written", "write-only"), ("copied", "copy-only"),
                               ("sink", "none"), ("flows", "read"), ("tested", "none")):
            with self.subTest(name=name):
                self.assertEqual(self.native.classify(member + name), expected)
        self.assertEqual(self.native.classify(None), "missing")

    def test_evidence_cites_the_first_writer_else_the_declaration(self):
        self.assertEqual(self.native.evidence("c:@S@Rec@FI@written", "write-only"),
                         f"native-refs:write-only;source:native:{SOURCE}:11")
        self.assertEqual(self.native.evidence("c:@S@Rec@FI@unused", "none"),
                         f"native-refs:none;source:native:{HEADER}:7")

    def test_split_finds_a_member_declared_only_as_pieces(self):
        self.assertTrue(self.native.split("Rec", member_row("pad")))
        self.assertFalse(self.native.split("Rec", member_row("unused")))
        self.assertFalse(self.native.split(None, member_row("pad")))


class ResolveTests(unittest.TestCase):
    def test_unread_rows_are_settled_and_read_rows_stay_queued(self):
        fields = [member_row("read"), member_row("written"), member_row("unused", "No access found"),
                  member_row("described", "Section 7 length.")]
        data = [{"id": "G00001", "name": "D_80010000", "final_name": "Rec_global", "purpose": "Kept argument.",
                 "confidence": "low", "copied": "x", "notes": "source:rec.c:2"}]
        counts = trace_queue.resolve(trace_queue.Native(index()), fields, [TYPE], data)
        self.assertEqual(dict(counts), {"field write-only": 1, "field none": 2, "global write-only": 1})
        read, written, unused, described = fields
        self.assertEqual((read["meaning"], read["confidence"], read["notes"]), ("Not read", "low", "source:rec.c:1"))
        self.assertEqual((written["meaning"], written["confidence"]),
                         (trace_queue.MEANING["write-only"], "med"))
        self.assertEqual(written["notes"], f"native-refs:write-only;source:native:{SOURCE}:11;source:rec.c:1")
        self.assertEqual(unused["meaning"], trace_queue.MEANING["none"])
        self.assertEqual(described["meaning"],
                         "Section 7 length; not used by native code; kept for the original layout")
        self.assertEqual((data[0]["purpose"], data[0]["confidence"]),
                         ("Kept argument; written but never read by native code; kept for the original data "
                          "layout.", "med"))

    def test_retired_types_and_globals_are_not_selected(self):
        fields = [member_row("written")]
        data = [{"id": "G00001", "name": "D_80010000", "final_name": "Rec_global", "purpose": "Kept argument.",
                 "confidence": "low", "copied": "retired", "notes": "retired-by:nothing"}]
        counts = trace_queue.resolve(trace_queue.Native(index()), fields, [dict(TYPE, copied="retired")], data)
        self.assertEqual(dict(counts), {})
        self.assertEqual(fields[0]["confidence"], "low")


class RefreshTests(unittest.TestCase):
    def test_citations_follow_the_index_and_changed_classes_are_reported(self):
        fields = [
            member_row("unused", "Not used", f"native-refs:none;source:native:{HEADER}:99;source:rec.c:1"),
            member_row("written", "Written", f"native-refs:write-only;source:native:{SOURCE}:11"),
            member_row("read", "Not used", f"native-refs:none;source:native:{HEADER}:1"),
            member_row("gone", "Not used", f"native-refs:none;source:native:{HEADER}:3"),
        ]
        data = [{"id": "G00001", "name": "D_80010000", "final_name": "Rec_global", "copied": "x",
                 "notes": f"native-refs:write-only;source:native:{SOURCE}:2"}]
        counts, review = trace_queue.refresh(trace_queue.Native(index()), fields, [TYPE], data)
        self.assertEqual(dict(counts), {"refreshed": 2, "review": 2})
        self.assertEqual([row["notes"] for row in fields], [
            f"native-refs:none;source:native:{HEADER}:7;source:rec.c:1",
            f"native-refs:write-only;source:native:{SOURCE}:11",
            f"native-refs:none;source:native:{HEADER}:1",
            f"native-refs:none;source:native:{HEADER}:3",
        ])
        self.assertEqual(data[0]["notes"], f"native-refs:write-only;source:native:{SOURCE}:16")
        self.assertEqual(review, ["T00001.read: recorded none, native read",
                                  "T00001.gone: recorded none, native missing"])

    def test_retired_rows_keep_their_evidence(self):
        stale = f"native-refs:none;source:native:{HEADER}:99"
        fields = [member_row("unused", "Not used", stale)]
        data = [{"id": "G00001", "name": "D_80010000", "final_name": "Rec_global", "copied": "retired",
                 "notes": stale}]
        counts, review = trace_queue.refresh(trace_queue.Native(index()), fields, [dict(TYPE, copied="retired")],
                                             data)
        self.assertEqual((dict(counts), review), ({}, []))
        self.assertEqual((fields[0]["notes"], data[0]["notes"]), (stale, stale))


class SpellingTests(unittest.TestCase):
    def test_members_found_only_by_letter_case_are_reported(self):
        fields = [member_row("read"), dict(member_row("written"), final_name="Written", name="unk2"),
                  dict(member_row("unused"), final_name="Unused")]
        native = trace_queue.Native(index())
        self.assertEqual(trace_queue.spelling(native, fields, [TYPE]),
                         ["T00001.written: final_name Written, native written"])
        self.assertEqual(trace_queue.spelling(native, fields, [dict(TYPE, copied="retired")]), [])


if __name__ == "__main__":
    unittest.main()
