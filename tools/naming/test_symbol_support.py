"""Regression tests for shared symbol layouts, citations and copier contracts."""
from __future__ import annotations

import ast
import csv
import re
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parent))
from symbol_support import Gen, canonical_type_key, ledger_rows  # noqa: E402

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "port"))
from copy_to_native import Copier  # noqa: E402
from ref_index import Ledgers, ledger_layout_type, norm_key  # noqa: E402


class ReviewedConsumerTests(unittest.TestCase):
    root = Path(__file__).resolve().parents[2]

    def reference_ledgers(self, index):
        read_text = Path.read_text

        def read_text_without_generated_context(path, *args, **kwargs):
            if path.name == "symbol_context.json":
                return "{}"
            return read_text(path, *args, **kwargs)

        with patch.object(Path, "read_text", read_text_without_generated_context):
            return Ledgers(index)

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

    def test_anonymous_type_keys_ignore_checkout_root(self):
        repository_key = (
            r"struct:(unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src"
            r"\BATTLE\BATTLE.PRG\..\..\SLUS_010.40\main.h:370:5)"
        )
        worktree_key = (
            r"struct:(unnamed at D:\Dev\NativePort\wt\batch-loot-inventory\resources\rood-reverseWin\src"
            r"\BATTLE\BATTLE.PRG\..\..\SLUS_010.40\main.h:370:5)"
        )
        self.assertEqual(canonical_type_key(repository_key), canonical_type_key(worktree_key))
        self.assertNotEqual(
            canonical_type_key(repository_key.replace("rood-reverseWin", "rood-reverse")),
            canonical_type_key(repository_key),
        )

    def test_nested_anonymous_type_keys_ignore_checkout_root(self):
        repository_type = (
            r"union (unnamed union at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src"
            r"\MENU\MENU7.PRG\260.c:138:5) *"
        )
        worktree_type = (
            r"union (unnamed union at D:\Dev\NativePort\wt\batch-loot-screen\resources\rood-reverseWin\src"
            r"\MENU\MENU7.PRG\260.c:138:5) *"
        )

        self.assertEqual(canonical_type_key(repository_type), canonical_type_key(worktree_type))

    def test_reference_type_keys_ignore_checkout_root(self):
        repository_key = (
            r"struct:(unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src"
            r"\BATTLE\BATTLE.PRG\..\..\SLUS_010.40\main.h:370:5)"
        )
        worktree_key = (
            r"struct:(unnamed at D:\Dev\NativePort\wt\batch-loot-screen\resources\rood-reverseWin\src"
            r"\BATTLE\BATTLE.PRG\..\..\SLUS_010.40\main.h:370:5)"
        )

        self.assertEqual(norm_key(repository_key), norm_key(worktree_key))
        self.assertNotEqual(
            norm_key(repository_key.replace("rood-reverseWin", "rood-reverse")),
            norm_key(repository_key),
        )

    def test_layout_hash_types_use_the_ledger_source_root(self):
        root = "D:\\Dev\\NativePort\\VS-NativePort\\"
        ledger_type = (
            r"union (unnamed union at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src"
            r"\MENU\MENU7.PRG\260.c:138:5) *"
        )
        worktree_type = (
            r"union (unnamed union at D:\Dev\NativePort\wt\batch-loot-screen\resources\rood-reverseWin\src"
            r"\MENU\MENU7.PRG\260.c:138:5) *"
        )

        self.assertEqual(
            ledger_layout_type(worktree_type, {"rood-reversewin": root}),
            ledger_type,
        )

    def test_layout_hash_keeps_missing_enum_field_type_null(self):
        type_record = {
            "key": "enum:state",
            "file": "src/MENU/MENU7.PRG/260.c",
            "line": 565,
            "size": 4,
            "fields": [
                {"name": "init", "value": 0},
                {"name": "ready", "value": 1},
                {"name": "unformatted", "value": 2},
                {"name": "confirmed", "value": 3},
                {"name": "loadReady", "value": 4},
                {"name": "loaded", "value": 5},
            ],
        }
        ledgers = self.reference_ledgers({"types": {"enum:state": type_record}})
        layout = ledgers.layout[("enum:state", "src/MENU/MENU7.PRG/260.c", 565)]

        self.assertIn(f"enum:state#{layout}", ledgers.t_by_key)

    def test_ledger_aggregate_type_resolves_split_pinned_member(self):
        ledgers = self.reference_ledgers({"types": {}})
        members = ledgers.gen.members["T00532"]
        fields = []
        for i, member in enumerate(members):
            if member["id"] == "T00532.038":
                offset = 416
            elif member["id"] == "T00532.039":
                offset = 448
            elif i < 38:
                offset = i * 8
            else:
                offset = 456 + (i - 40) * 8
            fields.append({"name": member["name"], "offset": offset, "type": "u_char"})

        nested_type = next(m["type"] for m in ledgers.gen.members["T00815"] if m["id"] == "T00815.001")
        ledgers._layouts["T00815"] = [
            (0, 32, "T00815.000", "unsigned int"),
            (0, 32, "T00815.001", nested_type),
        ]
        ledgers._layouts["T00135"] = [
            (0, 8, "T00135.000", "unsigned int"),
            (8, 8, "T00135.001", "unsigned int"),
            (16, 8, "T00135.002", "unsigned int"),
            (24, 8, "T00135.003", "unsigned int"),
        ]
        with patch.object(ledgers.gen, "layout", return_value={"size": 1156, "fields": fields}):
            self.assertEqual(
                ledgers.field_by_offset("T00532", 424, 8),
                "T00532.038>T00815.001>T00135.001",
            )

    def test_layout_matches_member_lists_across_checkout_roots(self):
        repository_key = (
            r"struct:(unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin\src"
            r"\BATTLE\BATTLE.PRG\..\..\SLUS_010.40\main.h:370:5)"
        )
        worktree_key = (
            r"struct:(unnamed at D:\Dev\NativePort\wt\batch-loot-inventory\resources\rood-reverseWin\src"
            r"\BATTLE\BATTLE.PRG\..\..\SLUS_010.40\main.h:370:5)"
        )
        layout = {"key": worktree_key, "fields": [{"name": "weaponCategories"}]}
        gen = Gen.__new__(Gen)
        gen.types = {"T00123": {"key": repository_key}}
        gen.members = {"T00123": [{"name": "weaponCategories"}]}
        gen.trees = {
            "": SimpleNamespace(ix={"types": {"target": layout}}),
            "upstream:": SimpleNamespace(ix={"types": {}}),
        }

        self.assertIs(gen.layout("T00123"), layout)

    def test_generated_union_preserves_renamed_fields_across_checkout_roots(self):
        coordinator = r"D:\Dev\NativePort\VS-NativePort"
        suffix = r"\resources\rood-reverseWin\src\include\sound.h:589:16)"
        row = {"id": "T00796", "kind": "union", "key": "union:(unnamed at " + coordinator + suffix}
        gen = Gen.__new__(Gen)
        gen.types = {"T00796": row}
        gen.members = {"T00796": [
            {"id": "T00796.000", "name": "s32"},
            {"id": "T00796.001", "name": "u8"},
        ]}
        copier = Copier.__new__(Copier)
        copier._gen = gen
        copier._tname = {}
        copier.ptr_fields = set()
        copier.fields = {
            "T00796.000": {"final_name": "word", "type": "int"},
            "T00796.001": {"final_name": "bytes", "type": "u_char[4]"},
        }
        expected = (
            "typedef union Audio_StreamWordBytes {\n"
            "    /* 0x00 */ int word;\n"
            "    /* 0x00 */ u_char bytes[4];\n"
            "} Audio_StreamWordBytes;"
        )
        for checkout in (coordinator, r"D:\Dev\NativePort\wt\equipment-status"):
            with self.subTest(checkout=checkout):
                layout = {"key": "union:(unnamed at " + checkout + suffix, "fields": [
                    {"name": "s32", "offset": 0, "type": "int"},
                    {"name": "u8", "offset": 0, "type": "u_char[4]"},
                ]}
                decoy = {"key": layout["key"].replace("sound.h", "other.h"), "fields": [
                    {"name": "s32", "offset": 0, "type": "short"},
                    {"name": "u8", "offset": 0, "type": "u_char[2]"},
                ]}
                gen.trees = {
                    "": SimpleNamespace(ix={"types": {"decoy": decoy, "target": layout}}),
                    "upstream:": SimpleNamespace(ix={"types": {}}),
                }
                self.assertEqual(copier.generated_type(row, "Audio_StreamWordBytes"), expected)

    def test_adopted_ai_state_layout_uses_upstream_member_spans(self):
        key = "struct:AiState"
        pinned = {"key": key, "size": 0x40, "fields": [
            {"name": "unk0", "offset": 0, "type": "int"},
            {"name": "unkB4", "offset": 0x20 * 8, "type": "u_char"},
        ]}
        upstream = {"key": key, "size": 0x484, "fields": [
            {"name": "unk0", "offset": 0, "type": "char"},
            {"name": "unkB4", "offset": 0xB4 * 8, "type": "int"},
            {"name": "unkC0", "offset": 0xC0 * 8, "type": "u_short"},
            {"name": "unkC2", "offset": 0xC2 * 8, "type": "u_short"},
        ]}
        gen = Gen.__new__(Gen)
        gen.types = {"T00532": {"key": key}, "T00999": {"key": key}}
        wanted = [{"name": "unk0"}, {"name": "unkB4"}, {"name": "unkC0"}]
        gen.members = {"T00532": wanted, "T00999": wanted}
        gen.trees = {
            "": SimpleNamespace(ix={"types": {"pinned": pinned}}),
            "upstream:": SimpleNamespace(ix={"types": {"upstream": upstream}}),
        }

        def spans(layout):
            return [(field["name"], field["offset"], field["type"]) for field in layout["fields"]]

        adopted = gen.layout("T00532")
        self.assertEqual(adopted["size"], 0x484)
        self.assertEqual(spans(adopted), [
            ("unk0", 0, "char"), ("unkB4", 0xB4 * 8, "int"), ("unkC0", 0xC0 * 8, "u_short"),
        ])
        default = gen.layout("T00999")
        self.assertEqual(default["size"], 0x40)
        self.assertEqual(spans(default), [
            ("unk0", 0, "int"), ("unkB4", 0x20 * 8, "u_char"), ("unkC0", 0xC0 * 8, "u_short"),
        ])

    def test_ledger_reader_preserves_rows_and_closes_on_success_and_error(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "rows.csv").write_bytes(
                b'id,note\r\n1,"with,comma"\r\n2,"line1\r\nline2"\r\n'
            )
            opened = []
            original_open = Path.open

            def track_open(path, *args, **kwargs):
                stream = original_open(path, *args, **kwargs)
                opened.append(stream)
                return stream

            for path, name in (
                (r"tools\port\ref_index.py", "read_rows"),
                (r"tools\naming\symbol_support.py", "ledger_rows"),
            ):
                with self.subTest(reader=name):
                    opened.clear()
                    reader = self.function(path, name, {"LEDGERS": root, "csv": csv})
                    with patch.object(Path, "open", track_open):
                        rows = reader("rows.csv")
                    self.assertEqual(rows, [
                        {"id": "1", "note": "with,comma"},
                        {"id": "2", "note": "line1\nline2"},
                    ])
                    self.assertTrue(opened and all(stream.closed for stream in opened))
                    with (
                        patch.object(Path, "open", track_open),
                        patch.object(csv, "DictReader", side_effect=ValueError("invalid rows")),
                        self.assertRaisesRegex(ValueError, "invalid rows"),
                    ):
                        reader("rows.csv")
                    self.assertTrue(all(stream.closed for stream in opened))

    def test_reviewed_one_to_one_consumers_use_strict_pairing(self):
        paths = (
            r"tools\naming\sanity_pass.py",
            r"tools\naming\symbol_support.py",
            r"tools\naming\upstream_layout.py",
            r"tools\port\check_native_views.py",
            r"tools\port\copy_to_native.py",
            r"tools\port\field_at.py",
            r"tools\port\layout_check.py",
            r"tools\port\ref_index.py",
        )
        for path in paths:
            calls = [node for node in ast.walk(self.tree(path))
                     if isinstance(node, ast.Call) and isinstance(node.func, ast.Name)
                     and node.func.id == "zip"]
            self.assertTrue(calls, path)
            for call in calls:
                with self.subTest(path=path, line=call.lineno):
                    self.assertTrue(any(
                        keyword.arg == "strict" and isinstance(keyword.value, ast.Constant)
                        and keyword.value.value is True
                        for keyword in call.keywords
                    ))

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
        fields = ledger_rows("FIELD_LEDGER.csv")
        for tid, member, final in (
            ("T00271", "unkB", "reservedB"),
            ("T00273", "unk8", "reserved8"),
            ("T00273", "unk15", "reserved15"),
        ):
            with self.subTest(tid=tid, member=member):
                row = next(row for row in fields
                           if row["type_id"] == tid and row["name"] == member)
                self.assertEqual((row["final_name"], row["meaning"], row["confidence"]),
                                 (final, "Not read", "low"))
                self.assertRegex(row["notes"],
                                 r"^declaration-only;source:upstream:6E644\.c:\d+$")


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
