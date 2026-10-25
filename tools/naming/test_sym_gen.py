"""Regression tests for aligned semantic symbol-generator rows."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent / "sym_gen"))
from common import Gen  # noqa: E402


class MemberPairTests(unittest.TestCase):
    def setUp(self):
        self.gen = Gen.__new__(Gen)
        self.gen.members = {"T00001": [
            {"id": "T00001.000", "name": "first", "final_name": "first",
             "meaning": "First value.", "confidence": "med"},
            {"id": "T00001.001", "name": "second", "final_name": "second",
             "meaning": "Second value.", "confidence": "med"},
        ]}
        self.gen.types = {"T00001": {"name": "Fixture", "files": "fixture.c"}}
        self.gen.TYPES = []
        self.gen.FIELDS = []

    def layout(self, count):
        return {"size": count * 4, "fields": [
            {"offset": index * 32} for index in range(count)
        ]}

    def specs(self):
        return [
            (member["name"], member["final_name"], member["meaning"],
             member["confidence"], "source:fixture.c:1")
            for member in self.gen.members["T00001"]
        ]

    def test_generic_specs_preserve_all_member_offsets(self):
        with (
            patch.object(self.gen, "member_offsets", return_value=[0, 32]),
            patch.object(self.gen, "is_used", return_value=False),
        ):
            self.assertEqual(self.gen.generic_specs("T00001"), [
                ("first", "reserved0", "Not read by decompiled code", "low"),
                ("second", "reserved4", "Not read by decompiled code", "low"),
            ])

    def test_generic_specs_reject_missing_or_extra_offsets(self):
        for offsets in ([0], [0, 32, 64]):
            with (
                self.subTest(offsets=offsets),
                patch.object(self.gen, "member_offsets", return_value=offsets),
                patch.object(self.gen, "is_used", return_value=False),
                self.assertRaisesRegex(ValueError, "zip"),
            ):
                self.gen.generic_specs("T00001")

    def test_named_views_preserve_all_member_spans(self):
        with patch.object(self.gen, "layout", return_value=self.layout(2)):
            result = self.gen.names_from_views("T00001", ["T00001"])
        self.assertEqual(result, {
            "first": ("first", "First value. (same bits as T00001 first)", "med"),
            "second": ("second", "Second value. (same bits as T00001 second)", "med"),
        })

    def test_named_views_reject_missing_or_extra_layout_fields(self):
        for count in (1, 3):
            with (
                self.subTest(count=count),
                patch.object(self.gen, "layout", return_value=self.layout(count)),
                self.assertRaisesRegex(ValueError, "zip"),
            ):
                self.gen.names_from_views("T00001", [])

    def duplicate(self):
        self.gen.TYPES = [["T00001", "Value", "State::Fixture", "fixture.h",
                           "Fixture values.", "med", "source:fixture.c:1"]]
        self.gen.FIELDS = [
            [member["id"], *spec[1:]]
            for member, spec in zip(self.gen.members["T00001"], self.specs(), strict=True)
        ]
        self.gen.members["T00002"] = [
            {**member, "id": member["id"].replace("T00001", "T00002")}
            for member in self.gen.members["T00001"]
        ]

    def test_duplicate_rows_preserve_all_member_identities(self):
        self.duplicate()
        self.gen.D("T00001", ["T00002"])
        self.assertEqual([row[0] for row in self.gen.FIELDS], [
            "T00001.000", "T00001.001", "T00002.000", "T00002.001",
        ])
        self.assertEqual(
            [row[1:] for row in self.gen.FIELDS[:2]],
            [row[1:] for row in self.gen.FIELDS[2:]],
        )

    def test_duplicate_length_guard_stays_explicit(self):
        self.duplicate()
        self.gen.members["T00002"].pop()
        with self.assertRaisesRegex(SystemExit, "canonical T00001 has 2"):
            self.gen.D("T00001", ["T00002"])

    def test_type_specs_preserve_ordered_field_identities(self):
        self.gen.T("T00001", "Value", "State::Fixture", "fixture.h",
                   "Fixture values.", "med", "source:fixture.c:1", self.specs())
        self.assertEqual(
            self.gen.FIELDS,
            [[member["id"], *spec[1:]]
             for member, spec in zip(self.gen.members["T00001"], self.specs(), strict=True)],
        )

    def test_type_member_order_guard_stays_explicit(self):
        with self.assertRaisesRegex(SystemExit, "member order"):
            self.gen.T("T00001", "Value", "State::Fixture", "fixture.h",
                       "Fixture values.", "med", "source:fixture.c:1",
                       list(reversed(self.specs())))


if __name__ == "__main__":
    unittest.main()
