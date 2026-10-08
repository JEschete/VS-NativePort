from pathlib import Path
from contextlib import redirect_stdout
import hashlib
import io
import json
import re
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from windows import check_sync_destination, download, replace_upstream_sources, require_windows_binary, sync_upstream, utility, verify_upstream


class WindowsBuildTests(unittest.TestCase):
    def test_progress_tracker_matches_current_unit_and_retires_old_harness(self):
        workspace = Path(__file__).resolve().parents[2]
        decomp = workspace / "resources" / "rood-reverseWin"
        tracker = (decomp / "DECOMP_PROGRESS.md").read_text(encoding="utf-8")
        current = tracker.split("## Current File:", 1)[1].split("## Required Approach", 1)[0]
        for name in ("38C1C.c", "3A1A0.h"):
            with self.subTest(source=name):
                source = decomp / "src" / "BATTLE" / "BATTLE.PRG" / name
                self.assertIn(hashlib.sha256(source.read_bytes()).hexdigest(), current)
        source = (decomp / "src/BATTLE/BATTLE.PRG/38C1C.c").read_text(encoding="utf-8")
        rows = {name: (state, acceptance) for name, state, acceptance in re.findall(
            r"^\| (func_[0-9A-F]{8}) \|[^|]*\|[^|]*\| ([^|]+) \| ([^|]+) \|", current, re.MULTILINE)}
        assembly = set(re.findall(r'INCLUDE_ASM\(\s*"[^"]+"\s*,\s*(\w+)\s*\)', source))
        self.assertEqual({name for name, (state, _) in rows.items() if "INCLUDE_ASM" in state}, assembly)
        self.assertEqual(source.count("__asm__ volatile("), 2)
        accepted = [name for name, (_, acceptance) in rows.items() if "Accepted canonical MATCH" in acceptance]
        self.assertTrue(accepted)
        for name in accepted:
            with self.subTest(accepted=name):
                self.assertNotIn(name, assembly)
                self.assertRegex(source, rf"\n\w[^\n;]*\b{name}\([^;]*\)\n\{{")
        harness = (decomp / "tools/verification/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("Retired battle candidate harness", harness)
        self.assertLess(harness.index("message(FATAL_ERROR"), harness.index("project("))

    def test_explicit_sync_refuses_to_discard_local_work(self):
        for failure in ("local edit", "missing source", "added source", "missing manifest"):
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / "src" / "game.c"
                source.parent.mkdir()
                source.write_bytes(b"upstream")
                manifest = {"files": {"src/game.c": hashlib.sha256(source.read_bytes()).hexdigest()}}
                if failure != "missing manifest":
                    (root / "upstream.json").write_text(json.dumps(manifest))
                if failure == "local edit":
                    source.write_bytes(b"local work")
                elif failure == "missing source":
                    source.unlink()
                elif failure == "added source":
                    (source.parent / "new.c").write_bytes(b"new work")
                before = {path.relative_to(root): path.read_bytes()
                          for path in root.rglob("*") if path.is_file()}
                with patch("windows.DECOMP", root), patch("windows.run") as run:
                    with self.assertRaisesRegex(RuntimeError, "No files were changed"):
                        sync_upstream(fetch=True)
                    run.assert_not_called()
                after = {path.relative_to(root): path.read_bytes()
                         for path in root.rglob("*") if path.is_file()}
                self.assertEqual(after, before)

    def test_sync_destination_accepts_pristine_source_and_ignores_python_cache(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "src" / "game.c"
            source.parent.mkdir()
            source.write_bytes(b"upstream")
            (root / "upstream.json").write_text(json.dumps({
                "files": {"src/game.c": hashlib.sha256(source.read_bytes()).hexdigest()}}))
            cache = root / "tools" / "etc" / "__pycache__"
            cache.mkdir(parents=True)
            (cache / "converter.pyc").write_bytes(b"cache")
            with patch("windows.DECOMP", root):
                check_sync_destination()

    def test_sync_destination_accepts_empty_initial_snapshot(self):
        with tempfile.TemporaryDirectory() as directory, patch("windows.DECOMP", Path(directory)):
            check_sync_destination()

    def test_verification_reports_divergence_without_synchronizing_or_replacing_source(self):
        expected = {"missing manifest": "WARNING: Missing upstream manifest",
                    "new upstream": "WARNING: Upstream reference is new",
                    "local edit": "Local decompilation changes: 1 files",
                    "missing source": "Local decompilation changes: 1 files"}
        for failure, message in expected.items():
            with self.subTest(failure=failure), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                snapshot = root / "windows"
                source = snapshot / "src" / "game.c"
                source.parent.mkdir(parents=True)
                source.write_bytes(b"upstream")
                manifest = {"commit": "pinned", "files": {
                    "src/game.c": hashlib.sha256(source.read_bytes()).hexdigest()}}
                if failure != "missing manifest":
                    (snapshot / "upstream.json").write_text(json.dumps(manifest))
                if failure == "local edit":
                    source.write_bytes(b"local work")
                elif failure == "missing source":
                    source.unlink()
                before = {path.relative_to(snapshot): path.read_bytes()
                          for path in snapshot.rglob("*") if path.is_file()}
                revision = b"new\n" if failure == "new upstream" else b"pinned\n"
                output = io.StringIO()
                with patch("windows.ROOT", root), patch("windows.DECOMP", snapshot), \
                        patch("windows.run", return_value=revision), patch("windows.sync_upstream") as sync, \
                        redirect_stdout(output):
                    verify_upstream()
                    sync.assert_not_called()
                self.assertIn(message, output.getvalue())
                self.assertNotIn("Authoritative upstream source verified", output.getvalue())
                after = {path.relative_to(snapshot): path.read_bytes()
                         for path in snapshot.rglob("*") if path.is_file()}
                self.assertEqual(after, before)

    def test_verification_accepts_unchanged_snapshot_without_synchronization(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            snapshot = root / "windows"
            source = snapshot / "src" / "game.c"
            source.parent.mkdir(parents=True)
            source.write_bytes(b"upstream")
            manifest = {"commit": "pinned", "files": {
                "src/game.c": hashlib.sha256(source.read_bytes()).hexdigest()}}
            (snapshot / "upstream.json").write_text(json.dumps(manifest))
            with patch("windows.ROOT", root), patch("windows.DECOMP", snapshot), \
                    patch("windows.run", return_value=b"pinned\n"), patch("windows.sync_upstream") as sync:
                verify_upstream()
                sync.assert_not_called()

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
