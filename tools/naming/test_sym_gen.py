"""Regression tests for aligned semantic symbol-generator rows."""
from __future__ import annotations

import ast
import re
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent / "sym_gen"))
from common import Gen  # noqa: E402


class ReviewedConsumerTests(unittest.TestCase):
    root = Path(__file__).resolve().parents[2]

    def tree(self, path):
        return ast.parse((self.root / path).read_text(encoding="utf-8-sig"))

    def function(self, path, name, namespace):
        node = next(node for node in ast.walk(self.tree(path))
                    if isinstance(node, ast.FunctionDef) and node.name == name)
        exec(compile(ast.Module(body=[node], type_ignores=[]), path, "exec"), namespace)
        return namespace[name]

    def callbacks(self, argument, namespace):
        return [
            eval(compile(ast.Expression(body=node), "copy_to_native.py", "eval"), namespace)
            for node in ast.walk(self.tree(r"tools\port\copy_to_native.py"))
            if isinstance(node, ast.Lambda)
            and any(arg.arg == argument for arg in node.args.args)
        ]

    def test_reviewed_one_to_one_consumers_use_strict_pairing(self):
        paths = (
            r"tools\naming\sanity_pass.py",
            r"tools\naming\sym_gen\Audio.py",
            r"tools\naming\sym_gen\Engine_01.py",
            r"tools\naming\sym_gen\Game_Battle_01.py",
            r"tools\naming\sym_gen\Game_Battle_02.py",
            r"tools\naming\sym_gen\Game_Menu_01.py",
            r"tools\naming\sym_gen\ai_state.py",
            r"tools\naming\upstream_layout.py",
            r"tools\port\check_native_views.py",
            r"tools\port\copy_to_native.py",
            r"tools\port\field_at.py",
            r"tools\port\layout_check.py",
            r"tools\port\ref_index.py",
        )
        count = 0
        for path in paths:
            calls = [node for node in ast.walk(self.tree(path))
                     if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                     and node.func.id == "zip"]
            self.assertTrue(calls, path)
            count += len(calls)
            for call in calls:
                with self.subTest(path=path, line=call.lineno):
                    self.assertTrue(any(
                        keyword.arg == "strict" and isinstance(keyword.value, ast.Constant)
                        and keyword.value.value is True
                        for keyword in call.keywords
                    ))
        self.assertEqual(count, 24)

    def test_signature_callback_keeps_its_current_native_type(self):
        namespace = {"t": "uintptr_t"}
        callbacks = self.callbacks("native_type", namespace)
        self.assertEqual(len(callbacks), 1)
        namespace["t"] = "changed"
        self.assertEqual(callbacks[0](re.match(r"(\s*)", "    ")), "    uintptr_t")

    def test_global_callbacks_keep_each_current_symbol(self):
        namespace = {"name": "Original"}
        callbacks = self.callbacks("symbol_name", namespace)
        self.assertEqual(len(callbacks), 2)
        namespace["name"] = "Changed"
        for callback in callbacks:
            self.assertEqual(callback(re.match(r"(\w+)(.*)", "placeholder[2]")),
                             "Original[2]")

    def test_anonymous_type_callback_keeps_its_current_kind_and_name(self):
        namespace = {"kw": "struct", "name": "Original"}
        callbacks = self.callbacks("kind", namespace)
        self.assertEqual(len(callbacks), 1)
        namespace.update(kw="union", name="Changed")
        self.assertEqual(callbacks[0](re.match(r"(typedef\s+)?", "typedef ")),
                         "typedef struct Original {")

    def test_asset_boundary_keeps_its_source_module(self):
        namespace = {"module": "ORIGINAL", "known": {("ORIGINAL", "image_clut")}}
        boundary = self.function(r"tools\port\copy_to_native.py", "boundary", namespace)
        namespace["module"] = "CHANGED"
        self.assertTrue(boundary("image"))
        self.assertTrue(boundary(None))
        self.assertFalse(boundary("other"))

    def test_first_use_keeps_its_declaration_and_name_pattern(self):
        namespace = {
            "rel": "original.c", "ln": 1, "pattern": re.compile(r"\bvalue\b"), "re": re,
            "lines": lambda unused: ["int value;", "value = 1;", "extern int value;"],
        }
        first_use = self.function(r"tools\naming\cite_globals.py", "first_use", namespace)
        namespace.update(rel="changed.c", ln=2, pattern=re.compile("missing"))
        self.assertEqual(first_use("original.c", 1, 3), 2)

    def test_upstream_member_groups_preserve_boundary_members(self):
        groups = self.function(r"tools\naming\upstream_layout.py", "groups", {})
        pinned = [{"offset": offset} for offset in (0, 8, 16)]
        upstream = [{"offset": offset} for offset in (0, 16)]
        self.assertEqual(groups(pinned, upstream), [
            (pinned[:2], upstream[:1]), (pinned[2:], upstream[1:]),
        ])

    def test_adopted_padding_cites_the_upstream_declaration(self):
        calls = []

        def cite(*pairs):
            calls.extend(pairs)
            return "source:upstream:6E644.c:1"

        tree = self.tree(r"tools\naming\sym_gen\Game_Battle_01.py")
        for tid, member, final, declaration in (
            ("T00271", "unkB", "reservedB", "char unkB[9];"),
            ("T00273", "unk8", "reserved8", "char unk8[2];"),
            ("T00273", "unk15", "reserved15", "char unk15;"),
        ):
            with self.subTest(tid=tid, member=member):
                calls.clear()
                definition = next(
                    node for node in ast.walk(tree)
                    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                    and node.func.id == "T" and isinstance(node.args[0], ast.Constant)
                    and node.args[0].value == tid
                )
                node = next(
                    node for node in ast.walk(definition)
                    if isinstance(node, ast.Tuple) and len(node.elts) >= 2
                    and isinstance(node.elts[0], ast.Constant) and node.elts[0].value == member
                    and isinstance(node.elts[1], ast.Constant) and node.elts[1].value == final
                )
                spec = eval(compile(ast.Expression(body=node), "padding-spec", "eval"),
                            {"LOW": "low", "c": cite})
                self.assertEqual(spec, (member, final, "Not read", "low",
                                        "declaration-only;source:upstream:6E644.c:1"))
                self.assertEqual(calls, [("upstream:6E644.c", declaration)])


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
