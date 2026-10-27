"""Inventory lexical candidates; accept only explicit, exact-snapshot reviews."""

import argparse
import csv
import hashlib
import json
import re
import subprocess
from bisect import bisect_right
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path

from tools.port.audit_assembly_gaps import mask_literals

ROOT = Path(__file__).resolve().parents[2]
SOURCE_SUFFIXES = frozenset((
    ".c", ".cc", ".cpp", ".cxx", ".c++", ".h", ".hh", ".hpp", ".hxx", ".h++",
    ".inl", ".inc", ".ipp", ".tpp", ".ixx", ".cppm",
))
HEADER_SUFFIXES = frozenset((".h", ".hh", ".hpp", ".hxx", ".h++"))
INLINE_SUFFIXES = frozenset((".inl", ".inc", ".ipp", ".tpp"))
IDENTIFIER = re.compile(r"\b[A-Za-z_]\w*\b")
CANDIDATE = re.compile(
    r"(?:arg\d+|temp_\w+|var_\w+|new_var\d*|sp[0-9A-Fa-f]+|"
    r"[avts]\d|unk\w*|func_[0-9A-Fa-f]+|D_[0-9A-Fa-f]+)\Z"
)
REVIEW_COLUMNS = (
    "path", "source_sha256", "identifier", "binding", "positions", "rationale", "evidence",
)


@dataclass(frozen=True)
class Occurrence:
    path: str
    source_sha256: str
    identifier: str
    line: int
    column: int
    ownership: str
    source_kind: str

    @property
    def key(self) -> tuple[str, str, str, int, int]:
        return self.path, self.source_sha256, self.identifier, self.line, self.column


def source_digest(text: str) -> str:
    return hashlib.sha256(text.replace("\r\n", "\n").replace("\r", "\n").encode()).hexdigest()


def occurrences(path: str, text: str, ownership: str, suffix: str) -> list[Occurrence]:
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    digest = source_digest(text)
    starts = [0] + [match.end() for match in re.finditer("\n", text)]
    kind = "header" if suffix in HEADER_SUFFIXES else (
        "inline" if suffix in INLINE_SUFFIXES else "implementation"
    )
    result = []
    for match in IDENTIFIER.finditer(mask_literals(text)):
        if CANDIDATE.fullmatch(match[0]):
            line = bisect_right(starts, match.start())
            result.append(Occurrence(
                path, digest, match[0], line, match.start() - starts[line - 1] + 1,
                ownership, kind,
            ))
    return result


def scan(root: Path) -> tuple[list[Occurrence], dict[str, str]]:
    manifest = json.loads((root / "tools" / "port" / "copy_manifest.json").read_text(
        encoding="utf-8"
    ))
    with (root / "tools" / "port" / "owned.csv").open(encoding="utf-8", newline="") as stream:
        owned = {row[key] for row in csv.DictReader(stream)
                 for key in ("path", "header") if row[key]}
    result, sources = [], {}
    for path in sorted((root / "Native").rglob("*")):
        suffix = path.suffix.lower()
        if suffix not in SOURCE_SUFFIXES or not path.is_file():
            continue
        relative = "\\".join(path.relative_to(root).parts)
        text = path.read_text(encoding="utf-8")
        sources[relative] = source_digest(text)
        ownership = "registered-owned" if relative in owned else (
            "copier-managed" if relative in manifest else "maintained"
        )
        result.extend(occurrences(relative, text, ownership, suffix))
    return result, sources


def read_reviews(path: Path) -> dict[tuple[str, str, str, int, int], dict[str, str]]:
    result = {}
    with path.open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames != list(REVIEW_COLUMNS):
            raise ValueError(f"{path}: expected review columns {REVIEW_COLUMNS}")
        for row_number, row in enumerate(reader, 2):
            if (set(row) != set(REVIEW_COLUMNS)
                    or not all(isinstance(value, str) and value for value in row.values())
                    or not re.fullmatch(r"[0-9a-f]{64}", row["source_sha256"])):
                raise ValueError(f"{path}:{row_number}: incomplete review or invalid source hash")
            for position in row["positions"].split(";"):
                if not re.fullmatch(r"[1-9]\d*:[1-9]\d*", position):
                    raise ValueError(f"{path}:{row_number}: invalid position {position!r}")
                line, column = map(int, position.split(":"))
                key = row["path"], row["source_sha256"], row["identifier"], line, column
                if key in result:
                    raise ValueError(f"{path}:{row_number}: duplicate review position {key}")
                result[key] = row
    return result


def reconcile(
    candidates: list[Occurrence],
    sources: dict[str, str],
    reviews: dict[tuple[str, str, str, int, int], dict[str, str]],
) -> dict:
    keys = {item.key for item in candidates}
    active = {key for key in reviews if sources.get(key[0]) == key[1]}
    return {
        "source_files": len(sources),
        "candidate_files": len({item.path for item in candidates}),
        "file_identifier_pairs": len({(item.path, item.identifier) for item in candidates}),
        "occurrences": len(candidates),
        "reviewed_occurrences": len(keys & active),
        "unreviewed": [asdict(item) for item in candidates if item.key not in active],
        "extra_review_positions": [list(key) for key in sorted(active - keys)],
        "inactive_snapshot_positions": len(reviews.keys() - active),
        "ownership": dict(sorted(Counter(item.ownership for item in candidates).items())),
        "source_kinds": dict(sorted(Counter(item.source_kind for item in candidates).items())),
        "source_sha256": sources,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--review", type=Path, help="Explicit per-binding retention CSV")
    parser.add_argument("--check", action="store_true", help="Reject uncovered or extra active positions")
    parser.add_argument("--out", type=Path, default=ROOT / "build" / "quality")
    args = parser.parse_args()
    if args.check and args.review is None:
        parser.error("--check requires --review; an inventory is not a semantic review")
    candidates, sources = scan(ROOT)
    reviews = read_reviews(args.review) if args.review else {}
    report = reconcile(candidates, sources, reviews)
    report["revision"] = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    report["dirty"] = subprocess.check_output(
        ["git", "status", "--short"], cwd=ROOT, text=True
    ).splitlines()
    report["candidate_pattern"] = CANDIDATE.pattern
    report["source_suffixes"] = sorted(SOURCE_SUFFIXES)
    report["limits"] = (
        "Lexical candidates remain visible. Review records approve only their exact normalized-LF "
        "source snapshot and positions; inactive historical/dirty variants confer no approval. "
        "This does not certify behavior, C++ promotion, compatibility retirement or every short name."
    )
    args.out.mkdir(parents=True, exist_ok=True)
    with (args.out / "native-symbol-inventory.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(Occurrence.__dataclass_fields__))
        writer.writeheader()
        writer.writerows(asdict(item) for item in candidates)
    (args.out / "native-symbol-review-report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps({key: value for key, value in report.items()
                      if key not in {"source_sha256", "dirty", "unreviewed", "extra_review_positions"}}))
    print(f"Unreviewed: {len(report['unreviewed'])}; extra active review positions: "
          f"{len(report['extra_review_positions'])}")
    return int(args.check and bool(report["unreviewed"] or report["extra_review_positions"]))


if __name__ == "__main__":
    raise SystemExit(main())
