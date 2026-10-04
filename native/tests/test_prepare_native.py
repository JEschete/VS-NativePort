from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from prepare_native import PARSER, declaration_name, transform_source


class NativePreparationTests(unittest.TestCase):
    def test_upstream_c_implementation_takes_precedence(self):
        source = b"void vs_main_memcpy(void* dest, void* src, unsigned sz) { upstream_copy(dest, src, sz); }"
        self.assertEqual(transform_source(Path("src/main.c"), source), source)

    def test_banked_effect_entry_is_native_namespaced(self):
        source = b"int func_800F9800(void) { return 7; }"
        result = transform_source(Path("src/EFFECT/PLG001.BIN/0.c"), source)
        self.assertIn(b"int vs_effect_PLG001_BIN_entry(void) { return 7; }", result)

    def test_typedef_name_is_read_from_real_parser_node(self):
        tree = PARSER.parse(b"typedef struct textHeader_t { char text[14]; } textHeader_t;")
        node = tree.root_node.named_children[0]
        self.assertEqual(declaration_name(node.child_by_field_name("declarator")), "textHeader_t")

    def test_upstream_type_is_completed_before_extern_array(self):
        definition = b"typedef struct textHeader_t { char text[14]; char disabled; char x; } textHeader_t;"
        source = b"extern struct textHeader_t _textHeaders[];"
        result = transform_source(Path("src/MENU/MAINMENU.PRG/58EC.h"), source, definition)
        self.assertEqual(result, definition + b"\n" + source)

    def test_removes_only_top_level_mips_assembly(self):
        source = b'int value = 7;\n__asm__("glabel copy;" "jr $ra;");\nint untouched(void) { return value; }'
        result = transform_source(Path("src/example.c"), source)
        self.assertNotIn(b"glabel copy", result)
        self.assertIn(b"int value = 7;", result)
        self.assertIn(b"int untouched(void) { return value; }", result)

    def test_replaces_native_body_without_changing_signature(self):
        source = b'static void _asmNop(void) { __asm__(".nop;"); }\nint upstream = 9;'
        result = transform_source(Path("src/example.c"), source)
        self.assertIn(b"static void _asmNop(void) { }", result)
        self.assertIn(b"int upstream = 9;", result)

    def test_save_adapter_preserves_upstream_fields(self):
        source = b'static void _packageGameSaveData(int index) { func_80042CB0(); savedata->actionsLearned = index; }'
        result = transform_source(Path("src/menu.c"), source)
        self.assertNotIn(b"func_80042CB0();", result)
        self.assertIn(b"savedata->actionsLearned = index;", result)

    def test_header_drift_is_not_silently_overwritten(self):
        with self.assertRaises(ValueError):
            transform_source(Path("src/include/common.h"), b"upstream changed\n")


if __name__ == "__main__":
    unittest.main()
