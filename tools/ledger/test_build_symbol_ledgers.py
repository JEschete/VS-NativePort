from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.ledger.build_symbol_ledgers import (  # noqa: E402
    existing_types_by_identity,
    match_existing_type,
    stable_type_spelling,
)


class SymbolLedgerPathNormalizationTests(unittest.TestCase):
    def test_existing_type_id_matches_across_worktree_roots(self) -> None:
        repository_key = (
            r"struct:(unnamed at D:\Dev\NativePort\VS-NativePort\resources\rood-reverseWin"
            r"\src\BATTLE\BATTLE.PRG\file.c:10:2)"
        )
        worktree_key = (
            r"struct:(unnamed at D:\Dev\NativePort\wt\batch-27\resources\rood-reverseWin"
            r"\src\BATTLE\BATTLE.PRG\file.c:10:2)"
        )
        existing = {
            "old": {
                "id": "T00001", "key": repository_key + "#oldhash",
                "files": "BATTLE/BATTLE.PRG/file.c",
            },
        }
        indexed = existing_types_by_identity(existing)
        current_decl = {"file": "src/BATTLE/BATTLE.PRG/file.c"}

        self.assertEqual(
            match_existing_type(
                indexed,
                {},
                "struct:(unnamed at resources/rood-reverseWin/src/BATTLE/BATTLE.PRG/file.c:10:2)",
                "newhash",
                [current_decl],
            )["id"],
            "T00001",
        )
        self.assertEqual(stable_type_spelling(worktree_key, repository_key), repository_key)

    def test_existing_rows_for_same_type_key_match_by_declaration_file(self) -> None:
        key = "struct:D_800EB9B8_t"
        existing = {
            "first": {
                "id": "T00001", "key": key + "#first", "files": "BATTLE/BATTLE.PRG/58578.c",
            },
            "second": {
                "id": "T00002", "key": key + "#second", "files": "BATTLE/BATTLE.PRG/5BE50.c",
            },
        }

        match = match_existing_type(
            existing_types_by_identity(existing),
            {},
            key,
            "newhash",
            [{"file": "src/BATTLE/BATTLE.PRG/5BE50.c"}],
        )

        self.assertIsNotNone(match)
        self.assertEqual(match["id"], "T00002")


if __name__ == "__main__":
    unittest.main()
