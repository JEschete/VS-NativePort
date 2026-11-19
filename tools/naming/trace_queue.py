"""Build the runtime-tracing queue (Docs\\ledgers\\TRACE_QUEUE.csv) from the low-confidence ledger rows.

Each row is one item whose meaning neither the original names nor static reading established, with what an emulator
session needs to settle it: the native functions that use it (with the original RAM address where the function
catalog and the upstream symbol files give one, for breakpoints) and, where a global instance of the owning struct
has a fixed address, the member's watch address (for read/write watchpoints). The native tree is the source of
truth (D24): uses come from tools\\naming\\native_refs.py, a type-aware libclang index of every Native translation
unit, cached in build\\naming\\native_refs.json. The `note` column gives the native use class (see `Native.classify`;
`split` marks a member native code declares only as named pieces, see `Native.split`).

  python tools\\naming\\trace_queue.py [--rebuild]   regenerate the queue (--rebuild re-indexes the native tree)
  python tools\\naming\\trace_queue.py --resolve     first record native evidence for unread rows, then regenerate

`--resolve` settles the low-confidence fields and globals that production native code never reads (no use, only
writes, or only copies into members that are themselves never read): it records that meaning at `med` with a
`native-refs:` note and a `source:native:` citation (Docs\\NAMING_CONVENTIONS.md §10.1). Rows that native code
reads stay in the queue for tracing. See #347 and #445. Every run recomputes those citations' lines from the index
and lists, for review, recorded findings whose native use has since changed (#457), and field rows whose
`final_name` differs from the native member's spelling (#458).
"""
from __future__ import annotations

import argparse
import collections
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import native_refs  # noqa: E402

ROOT = native_refs.ROOT
LEDGERS = ROOT / "Docs" / "ledgers"
OUT = LEDGERS / "TRACE_QUEUE.csv"
UPSTREAM = ROOT / "resources" / "rood-reverse" / "config"
TESTS = "Native\\Tests\\"
PRIORITY = {"opaque": 1, "low-named": 2, "global": 2, "type": 3, "reserved": 4}
COLUMNS = ["id", "kind", "priority", "name", "original", "offset", "current_meaning", "touched_by", "watch", "note"]
ANCHOR = re.compile(r"unk([0-9A-Fa-f]+)(?:_(\d+))?")
CITATION = re.compile(r"native-refs:(?P<use>[\w-]+);source:native:[^;]+:\d+")
UNREAD = ("none", "write-only", "copy-only")
MEANING = {
    "none": "Not used by native code; kept for the original layout",
    "write-only": "Written but never read by native code; kept for the original layout",
    "copy-only": "Only copied into members native code never reads; kept for the original layout",
}
GLOBAL_PURPOSE = {
    "none": "Not used by native code; kept for the original data layout.",
    "write-only": "Written but never read by native code; kept for the original data layout.",
    "copy-only": "Only copied into storage native code never reads; kept for the original data layout.",
}
# Recorded meanings that only state how (un)used an item is; anything else describes the original and is kept.
PLACEHOLDER = re.compile(r"(Not (read|used|referenced)|No (access|reader)|Unreferenced|Used, but its meaning is "
                         r"not established|Written but never read|Only copied)\b", re.IGNORECASE)


def native_meaning(current: str, finding: str) -> str:
    """The native finding, after the current meaning when that says what the item is."""
    current = current.strip()
    if not current or PLACEHOLDER.match(current):
        return finding
    return f"{current.rstrip('.')}; {finding[0].lower()}{finding[1:]}"


def read(name: str) -> tuple[list[str], list[dict[str, str]]]:
    with (LEDGERS / name).open(encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        return list(reader.fieldnames or []), list(reader)


def write(name: str, columns: list[str], rows: list[dict[str, str]]) -> None:
    with (LEDGERS / name).open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, columns, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def selection(fields: list[dict], types: list[dict], data: list[dict]) -> tuple[list[dict], list[dict], list[dict]]:
    """The ledger rows the queue lists: low-confidence members of live types, globals and types.

    A retired type or global was replaced natively, so its rows have nothing left to trace; neither does a type row
    that only duplicates another (`same-as:`)."""
    live = {row["id"]: row for row in types if row["copied"] != "retired" and "same-as:" not in row["notes"]}
    members = [row for row in fields if row["confidence"] == "low" and row["value"] == "" and row["type_id"] in live]
    globals_ = [row for row in data if row["confidence"] == "low" and row["copied"] != "retired"]
    return members, globals_, [row for row in live.values() if row["confidence"] == "low"]


def member_kind(final_name: str) -> str:
    return "reserved" if final_name.startswith("reserved") else \
        "opaque" if final_name.startswith("opaque") else "low-named"


class Native:
    """Ledger rows resolved to native declarations and their production (non-test, non-layout) uses."""

    def __init__(self, index: dict) -> None:
        self.decls: dict[str, dict] = index["decls"]
        self.refs = {usr: [ref for ref in refs if not ref[0].startswith(TESTS) and ref[3] != "layout"]
                     for usr, refs in index["refs"].items()}
        self.records: dict[str, dict[str, str]] = collections.defaultdict(dict)
        self.nested: dict[str, dict[str, str]] = collections.defaultdict(dict)
        self.globals: dict[str, str] = {}
        self.limits: dict[str, int] = {}
        for usr, decl in self.decls.items():
            if decl["kind"] == "field":
                self.records[self.key(decl["record"], decl["path"])].setdefault(decl["name"], usr)
                if "owner" in decl:
                    self.nested[self.key(decl["owner"], decl["path"])].setdefault(decl["name"], usr)
            else:
                name = f"{decl['record']}::{decl['name']}" if decl["record"] else decl["name"]
                self.globals.setdefault(name, usr)
        self.consumed = self.consumers(set(index["loose"]))

    @staticmethod
    def key(record: str, path: str) -> str:
        """Records by qualified name; an unnamed one also by its file, as its name is only its line."""
        return f"{record}@{path}" if "(anonymous:" in record else record

    def consumers(self, loose: set[str]) -> set[str]:
        """Declarations whose value production code uses: read, updated, address taken, copied somewhere untracked,
        or copied into a consumer. A member spelled in an unresolved (template) expression counts as used."""
        consumed, flows = set(), collections.defaultdict(set)
        for usr, refs in self.refs.items():
            for _path, _line, _function, access, peer in refs:
                if access in ("read", "update", "address") or (access == "copy" and not peer):
                    consumed.add(usr)
                elif access == "copy":
                    flows[peer].add(usr)
        consumed |= {usr for usr, decl in self.decls.items() if decl["name"] in loose}
        pending = list(consumed)
        while pending:
            for source in flows.get(pending.pop(), ()):
                if source not in consumed:
                    consumed.add(source)
                    pending.append(source)
        return consumed

    def record(self, row: dict, member_names: set[str]) -> str | None:
        """The native record of a ledger type: by name, else the unnamed record in its file sharing at least half
        its member names, else the one unnamed record anywhere that has all of them."""
        final = row["final_name"]
        for name in (final.replace("::", "_"), f"VS::{final}", final, row["name"], *row["aliases"].split()):
            if name in self.records:
                return name
        suffix = f"@{row['final_location']}"
        scored = [(len(member_names & set(members)), record) for record, members in self.records.items()
                  if record.endswith(suffix)]
        best = max(scored, default=(0, None))
        if best[0] and best[0] * 2 >= len(member_names):
            return best[1]
        complete = [record for record, members in self.records.items()
                    if "(anonymous:" in record and member_names and member_names <= set(members)]
        return complete[0] if len(complete) == 1 else None

    def member(self, record: str | None, row: dict) -> str | None:
        """The member, declared in the record or in an anonymous struct or union in it; exact names before a
        case-insensitive match."""
        if record is None:
            return None
        scopes = (self.records.get(record, {}), self.nested.get(record, {}))
        wanted = (row["final_name"], row["name"])
        folded = {name.lower() for name in wanted}
        for match in (lambda name: name in wanted, lambda name: name.lower() in folded):
            for members in scopes:
                for name, usr in members.items():
                    if match(name):
                        return usr
        return None

    def layout(self, record: str, usr: str, prefix: str = "") -> tuple[int | None, int | None]:
        """Bit offset of a member in the record and the record's size in bytes; `prefix` "32" selects the layout by
        the original's rules."""
        decl = self.decls[usr]
        if self.key(decl["record"], decl["path"]) == record:
            return decl.get(f"offset{prefix}"), decl.get(f"record_size{prefix}")
        return decl.get(f"owner_offset{prefix}"), decl.get(f"owner_size{prefix}")

    def layout_limit(self, record: str, rows: list[dict]) -> int:
        """Bit offset below which a record laid out by the original's rules is taken to match the original although
        its size differs: up to the first native-only member or the first original `unk<Hex>[_<bit>]` name whose
        offset the native layout contradicts. Without either, nothing is taken."""
        if record not in self.limits:
            known = {name.lower() for row in rows for name in (row["final_name"], row["name"])}
            marks = []
            for members in (self.records.get(record, {}), self.nested.get(record, {})):
                for name, usr in members.items():
                    offset = self.layout(record, usr, "32")[0]
                    if offset is not None and name.lower() not in known:
                        marks.append(offset)
            for row in rows:
                anchor = ANCHOR.fullmatch(row["name"])
                usr = self.member(record, row) if anchor else None
                offset = self.layout(record, usr, "32")[0] if usr else None
                if offset is not None:
                    original = int(anchor.group(1), 16) * 8 + int(anchor.group(2) or 0)
                    if offset != original:
                        marks.append(min(offset, original))
            self.limits[record] = min(marks, default=0)
        return self.limits[record]

    def variable(self, row: dict) -> str | None:
        final = row["final_name"]
        for name in (final.replace("::", "_"), f"VS::{final}", final, row["name"]):
            if name in self.globals:
                return self.globals[name]
        return None

    def classify(self, usr: str | None) -> str:
        """missing (no native declaration), read, copy-only, write-only or none."""
        if usr is None:
            return "missing"
        if usr in self.consumed:
            return "read"
        accesses = {ref[3] for ref in self.refs.get(usr, ())}
        return "copy-only" if "copy" in accesses else "write-only" if "write" in accesses else "none"

    def split(self, record: str | None, row: dict) -> bool:
        """Whether native code declares the member only as named pieces (`<name>_<offset>`), as it does where it
        replaced part of an original padding array with a native member."""
        if record is None:
            return False
        prefixes = tuple(f"{name}_".lower() for name in (row["final_name"], row["name"]))
        return any(name.lower().startswith(prefixes)
                   for members in (self.records.get(record, {}), self.nested.get(record, {})) for name in members)

    def evidence(self, usr: str, use: str) -> str:
        """The line that shows the class: the first writer or copy, else the declaration."""
        access = {"write-only": "write", "copy-only": "copy"}.get(use)
        refs = sorted(ref for ref in self.refs.get(usr, ()) if ref[3] == access)
        path, line = (refs[0][0], refs[0][1]) if refs else (self.decls[usr]["path"], self.decls[usr]["line"])
        return f"native-refs:{use};source:native:{path}:{line}"

    def functions(self, usr: str | None) -> list[str]:
        seen: dict[str, None] = {}
        for _path, _line, function, _access, _peer in sorted(self.refs.get(usr or "", ())):
            if function:
                seen.setdefault(function, None)
        return list(seen)


class Labels:
    """Native function name -> `name@ADDRESS` where the catalog row has an original address."""

    def __init__(self, catalog: list[dict]) -> None:
        addresses = {}
        for path in sorted(UPSTREAM.rglob("symbol_addrs.txt")) if UPSTREAM.is_dir() else ():
            for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
                match = re.match(r"\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+)", line)
                if match:
                    addresses[(path.parent.name, match.group(1))] = int(match.group(2), 16)
        self.addresses: dict[str, int] = {}
        for row in catalog:
            match = re.fullmatch(r"func_([0-9A-F]{8})", row["name"])
            address = int(match.group(1), 16) if match else addresses.get((row["overlay"], row["name"]))
            if address is None or not row["final_name"]:
                continue
            final = row["final_name"]
            for name in (final, f"VS::{final}", final.replace("::", "_")):
                self.addresses.setdefault(name, address)

    def __call__(self, function: str) -> str:
        address = self.addresses.get(function)
        return f"{function}@{address:08X}" if address is not None else function


def member_offset(native: Native, record: str | None, usr: str | None, type_row: dict, rows: list[dict]) -> int | None:
    """Original bit offset of a member, from its native declaration laid out by the original's rules (native_refs
    `offset32`), else by the native ones. A layout is taken whole where it keeps the original record size; a
    differing original-rules layout only below `Native.layout_limit`. A member with no native declaration has no
    offset: the names do not reliably encode one."""
    if record is None or usr is None:
        return None
    size = int(type_row["size"] or 0)
    for prefix in ("32", ""):
        offset, record_size = native.layout(record, usr, prefix)
        if offset is not None and record_size == size:
            return offset
    offset = native.layout(record, usr, "32")[0]
    return offset if offset is not None and offset < native.layout_limit(record, rows) else None


def resolve(native: Native, fields: list[dict], types: list[dict], data: list[dict]) -> collections.Counter:
    """Record native evidence for selected rows that production code never reads."""
    type_by_id = {row["id"]: row for row in types}
    by_type = collections.defaultdict(list)
    for row in fields:
        by_type[row["type_id"]].append(row)
    members, globals_, _types = selection(fields, types, data)
    counts: collections.Counter = collections.Counter()
    for row in members:
        type_row = type_by_id[row["type_id"]]
        names = {member["final_name"] for member in by_type[row["type_id"]]}
        usr = native.member(native.record(type_row, names), row)
        use = native.classify(usr)
        if use in UNREAD:
            row["meaning"], row["confidence"] = native_meaning(row["meaning"], MEANING[use]), "med"
            row["notes"] = ";".join(filter(None, (native.evidence(usr, use), row["notes"])))
            counts[f"field {use}"] += 1
    for row in globals_:
        usr = native.variable(row)
        use = native.classify(usr)
        if use in UNREAD:
            row["purpose"], row["confidence"] = native_meaning(row["purpose"], GLOBAL_PURPOSE[use]), "med"
            row["notes"] = ";".join(filter(None, (native.evidence(usr, use), row["notes"])))
            counts[f"global {use}"] += 1
    return counts


def refresh(native: Native, fields: list[dict], types: list[dict], data: list[dict]) -> tuple[collections.Counter,
                                                                                               list[str]]:
    """Recompute the `source:native:` line of every recorded native finding from the current index: edits to the
    cited file move it (#457). A row whose native declaration is gone or whose use class changed keeps its note and
    is returned for review instead, since its recorded meaning may no longer hold. Retired types and globals keep
    the evidence they were retired with."""
    type_by_id = {row["id"]: row for row in types}
    by_type = collections.defaultdict(list)
    for row in fields:
        by_type[row["type_id"]].append(row)
    counts: collections.Counter = collections.Counter()
    review: list[str] = []

    def update(row: dict, usr: str | None) -> None:
        match = CITATION.match(row["notes"])
        use = native.classify(usr)
        if use != match["use"]:
            counts["review"] += 1
            review.append(f"{row['id']}: recorded {match['use']}, native {use}")
            return
        citation = native.evidence(usr, use)
        if citation != match[0]:
            row["notes"] = citation + row["notes"][match.end():]
            counts["refreshed"] += 1

    for row in fields:
        type_row = type_by_id.get(row["type_id"])
        if CITATION.match(row["notes"]) and type_row and type_row["copied"] != "retired":
            names = {member["final_name"] for member in by_type[row["type_id"]]}
            update(row, native.member(native.record(type_row, names), row))
    for row in data:
        if CITATION.match(row["notes"]) and row["copied"] != "retired":
            update(row, native.variable(row))
    return counts, review


def spelling(native: Native, fields: list[dict], types: list[dict]) -> list[str]:
    """Rows whose member the index finds only by the case-insensitive fallback of `Native.member`: the code spells it
    neither as `final_name` nor as the original name, so `final_name` is wrong (#458)."""
    type_by_id = {row["id"]: row for row in types}
    by_type = collections.defaultdict(list)
    for row in fields:
        by_type[row["type_id"]].append(row)
    records: dict[str, str | None] = {}
    differ = []
    for row in fields:
        type_row = type_by_id.get(row["type_id"])
        if not row["final_name"] or not type_row or type_row["copied"] == "retired":
            continue
        if row["type_id"] not in records:
            names = {member["final_name"] for member in by_type[row["type_id"]]}
            records[row["type_id"]] = native.record(type_row, names)
        usr = native.member(records[row["type_id"]], row)
        declared = native.decls[usr]["name"] if usr else None
        if declared and declared not in (row["final_name"], row["name"]):
            differ.append(f"{row['id']}: final_name {row['final_name']}, native {declared}")
    return differ


def queue(native: Native, labels: Labels, fields: list[dict], types: list[dict], data: list[dict]) -> list[list]:
    type_by_id = {row["id"]: row for row in types}
    by_type = collections.defaultdict(list)
    for row in fields:
        by_type[row["type_id"]].append(row)
    instances = collections.defaultdict(list)
    for row in data:
        if row["type_id"] and row["address"].startswith("0x"):
            instances[row["type_id"]].append((row["final_name"] or row["name"], int(row["address"], 16), row["type"]))
    members, globals_, low_types = selection(fields, types, data)
    rows = []
    for row in members:
        type_row = type_by_id[row["type_id"]]
        names = {member["final_name"] for member in by_type[row["type_id"]]}
        record = native.record(type_row, names)
        usr = native.member(record, row)
        kind = member_kind(row["final_name"])
        bit = member_offset(native, record, usr, type_row, by_type[row["type_id"]])
        offset = "" if bit is None else f"0x{bit // 8:X}" + (f".{bit % 8}" if row["bits"] else "")
        watch = []
        if bit is not None:
            for name, base, declared in instances.get(row["type_id"], [])[:3]:
                stride = f" stride 0x{int(type_row['size']):X}" if "[" in declared or "*" in declared else ""
                watch.append(f"*{name}+{offset}" if "*" in declared else f"{name}=0x{base + bit // 8:08X}{stride}")
        touched = [labels(function) for function in native.functions(usr)[:6]]
        use = native.classify(usr)
        if use == "missing" and native.split(record, row):
            use = "split"
        rows.append([row["id"], kind, PRIORITY[kind], f"{type_row['final_name']}.{row['final_name']}", row["name"],
                     offset, row["meaning"], " ".join(touched), " ".join(watch), f"native:{use}"])
    for row in globals_:
        usr = native.variable(row)
        touched = [labels(function) for function in native.functions(usr)[:6]]
        watch = f"{row['final_name']}={row['address']}" if row["address"].startswith("0x") else ""
        note = ";".join(filter(None, (row["disposition"], f"native:{native.classify(usr)}")))
        rows.append([row["id"], "global", PRIORITY["global"], row["final_name"], row["name"], "", row["purpose"],
                     " ".join(touched), watch, note])
    for row in low_types:
        found = " ".join(f"{name}=0x{address:08X}" for name, address, _declared in instances.get(row["id"], [])[:3])
        rows.append([row["id"], "type", PRIORITY["type"], row["final_name"], row["name"][:60],
                     f"size 0x{int(row['size'] or 0):X}", row["purpose"], "", found, row["role"]])
    rows.sort(key=lambda row: (row[2], row[3], row[0]))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--rebuild", action="store_true", help="re-index the native tree first")
    parser.add_argument("--resolve", action="store_true", help="record native evidence for unread rows first")
    args = parser.parse_args()
    native = Native(native_refs.load(rebuild=args.rebuild))
    field_columns, fields = read("FIELD_LEDGER.csv")
    type_columns, types = read("TYPE_LEDGER.csv")
    data_columns, data = read("DATA_LEDGER.csv")
    _catalog_columns, catalog = read("FUNCTION_CATALOG.csv")
    if args.resolve:
        counts = resolve(native, fields, types, data)
        print("resolved:", dict(sorted(counts.items())))
    refreshed, review = refresh(native, fields, types, data)
    if args.resolve or refreshed["refreshed"]:
        write("FIELD_LEDGER.csv", field_columns, fields)
        write("DATA_LEDGER.csv", data_columns, data)
    print(f"native citations: {refreshed['refreshed']} refreshed, {len(review)} to review")
    for line in review:
        print(f"  review {line}")
    differ = spelling(native, fields, types)
    print(f"member spelling: {len(differ)} final_name differ from the native member")
    for line in differ:
        print(f"  spelling {line}")
    if not UPSTREAM.is_dir():
        print(f"warning: {UPSTREAM} is missing; functions without a func_ address are listed without one")
    rows = queue(native, Labels(catalog), fields, types, data)
    with OUT.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(COLUMNS)
        writer.writerows(rows)
    kinds = collections.Counter(row[1] for row in rows)
    uses = collections.Counter(row[9].rsplit("native:", 1)[-1] for row in rows if "native:" in row[9])
    print(f"{len(rows)} rows: {dict(kinds)}; native use: {dict(uses)}; with watch address: "
          f"{sum(1 for row in rows if row[8])}; with touching functions: {sum(1 for row in rows if row[7])}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
