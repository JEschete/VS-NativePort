"""Size budgets for the repository's Markdown documents.

An agent reads at most about 20 KB of a file at once. These documents grew past that by accumulating evidence and
history that belong on the issues, so this check fails when a Markdown file exceeds its budget. Split a document by
topic, or move evidence to its issue, instead of raising a budget.

Sizes are UTF-8 bytes with LF line endings, so a CRLF checkout measures the same as CI. The read-only references
under resources\\ are not checked.

Usage: python tools\\lint\\doc_budget.py --check
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BUDGET = 20_000
BUDGETS = {
    "Docs/HANDOFF.md": 12_000,
    # Loaded into every agent request.
    ".github/copilot-instructions.md": 14_000,
}
# Documents that were already larger than the default. These ceilings may only go down: lower one when its document
# shrinks, and remove it once the document fits the default.
LEGACY_CEILINGS = {
    "Docs/BUILDING.md": 53_000,
    "Docs/COMBAT_LAB.md": 33_000,
    "Docs/NAMING_CONVENTIONS.md": 34_000,
    "Docs/PORTING_WATCHLIST.md": 25_000,
    "Docs/QUALITY.md": 22_000,
    "Docs/ledgers/SDK_API_LEDGER.md": 32_000,
    "Native/Player/README.md": 30_000,
}
EXCLUDED_PREFIXES = ("resources/",)


def markdown_files(root: Path) -> list[str]:
    """Tracked and untracked, non-ignored Markdown files, as forward-slash paths."""
    listed = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", "*.md"],
                            cwd=root, capture_output=True, check=True).stdout.decode("utf-8")
    return sorted({path for path in listed.split("\0")
                   if path and not path.startswith(EXCLUDED_PREFIXES) and (root / path).is_file()})


def size(path: Path) -> int:
    return len(path.read_bytes().replace(b"\r\n", b"\n"))


def budget(path: str) -> int:
    return BUDGETS.get(path, LEGACY_CEILINGS.get(path, DEFAULT_BUDGET))


def check(root: Path, files: list[str]) -> tuple[list[str], list[str]]:
    """Return (violations, notes) for the given repository-relative Markdown files."""
    violations: list[str] = []
    notes: list[str] = []
    for path in files:
        measured, limit = size(root / path), budget(path)
        if measured > limit:
            violations.append(f"{path}: {measured:,} bytes exceeds its {limit:,}-byte budget")
        elif path in LEGACY_CEILINGS and measured <= DEFAULT_BUDGET:
            notes.append(f"{path}: {measured:,} bytes now fits the default budget; remove its legacy ceiling")
    return violations, notes


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true", help="fail when a document exceeds its budget")
    args = parser.parse_args(argv)
    files = markdown_files(ROOT)
    violations, notes = check(ROOT, files)
    for line in violations + notes:
        print(line)
    if not args.check:
        for path in files:
            print(f"{size(ROOT / path):>8,} / {budget(path):>7,}  {path}")
    print(f"doc_budget: {len(files)} documents, {len(violations)} over budget")
    return 1 if args.check and violations else 0


if __name__ == "__main__":
    sys.exit(main())
