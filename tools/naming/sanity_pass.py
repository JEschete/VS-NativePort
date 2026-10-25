"""Sanity pass over low/med ledger rows: report candidate upgrades and corrections from evidence revealed since the
rows were named. Read-only; writes tools\\naming\\sanity\\report.csv for review.

Checks
  F-RESERVED-USED   a `reserved<Off>` member is now read by a pinned or upstream function (field refs)
  F-UPSTREAM        upstream's member at the same bit offset and width has a meaningful name that differs
  F-SIBLING         another layout of the same struct (per-overlay copy) names the same bits with higher confidence
  F-CONFLICT        two high-confidence names for the same bits of one struct disagree
  G-UPSTREAM        upstream's symbol at the same address (symbol_addrs.txt) has a meaningful name that differs
  T-UPSTREAM        a low/med type's upstream name (same address-free key) is now meaningful

  python tools\\naming\\sanity_pass.py
"""
from __future__ import annotations

import collections
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "sym_gen"))
from common import ROOT, Gen, ledger_rows  # noqa: E402

OUT = ROOT / "tools" / "naming" / "sanity"
GENERIC = re.compile(r"^(unk|pad|field|_|D_|func_|filler|dummy|var|temp|tmp)|^[a-z]$|_t\d*$|^(unk|pad)?[0-9A-Fa-f]+$",
                     re.I)


def meaningful(n: str | None) -> bool:
    return bool(n) and not GENERIC.search(n) and not Gen.PLACEHOLDER.search(n)


def symbol_addrs(root: Path) -> dict[tuple[str, int], str]:
    out = {}
    for p in (root / "config").rglob("symbol_addrs.txt"):
        ov = p.parent.relative_to(root / "config").as_posix()
        for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
            m = re.match(r"\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+)", line)
            if m:
                out[(ov, int(m.group(2), 16))] = m.group(1)
    return out


def main() -> int:
    g = Gen("Shared")
    F = ledger_rows("FIELD_LEDGER.csv")
    T = {r["id"]: r for r in ledger_rows("TYPE_LEDGER.csv")}
    G = ledger_rows("DATA_LEDGER.csv")
    by_type = collections.defaultdict(list)
    for f in F:
        by_type[f["type_id"]].append(f)
    up_types = collections.defaultdict(list)
    for x in g.trees["upstream:"].ix["types"].values():
        up_types[x["key"]].append(x)

    def spans(tid):
        try:
            lay = g.layout(tid)
        except KeyError:
            return []
        ms, offs = by_type[tid], [f["offset"] for f in lay["fields"]]
        out = []
        for i, (m, f) in enumerate(zip(ms, lay["fields"], strict=True)):
            if f["offset"] is None or f["offset"] < 0:
                continue
            width = int(m["bits"] or 0) or (
                (offs[i + 1] if i + 1 < len(offs) and offs[i + 1] is not None else lay["size"] * 8) - offs[i])
            out.append(((f["offset"], width), m))
        return out

    rows = []
    span_of = {}
    for tid in T:
        if T[tid]["kind"] not in ("struct", "union") or "same-as:" in T[tid]["notes"]:
            continue
        for key, m in spans(tid):
            span_of[m["id"]] = (tid, key)

    # F-RESERVED-USED
    for f in F:
        if f["final_name"].startswith("reserved") and f["type_id"] in T and g.is_used(f["type_id"], f["name"]):
            rows.append(["F-RESERVED-USED", f["id"], T[f["type_id"]]["final_name"], f["final_name"], f["name"],
                         "", f["confidence"], "member is read now"])

    # F-UPSTREAM: same struct key upstream, same bits
    for f in F:
        if f["confidence"] == "high" or f["id"] not in span_of:
            continue
        tid, key = span_of[f["id"]]
        base = T[tid]["key"].partition("#")[0]
        for x in up_types.get(base, []):
            offs = [y["offset"] for y in x["fields"]]
            for i, y in enumerate(x["fields"]):
                if y["offset"] is None:
                    continue
                w = y["bits"] or ((offs[i + 1] if i + 1 < len(offs) and offs[i + 1] is not None else x["size"] * 8)
                                  - y["offset"])
                if (y["offset"], w) == key and meaningful(y["name"]) and y["name"] != f["name"] \
                        and y["name"].lower() != f["final_name"].lower():
                    rows.append(["F-UPSTREAM", f["id"], T[tid]["final_name"], f["final_name"], f["name"],
                                 y["name"], f["confidence"], f"upstream {x['file']}:{x['line']}"])
                    break

    # F-SIBLING / F-CONFLICT: layouts of the same struct key
    groups = collections.defaultdict(list)
    for fid, (tid, key) in span_of.items():
        groups[(T[tid]["key"].partition("#")[0], key)].append(fid)
    fby = {f["id"]: f for f in F}
    rank = {"high": 2, "med": 1, "low": 0}
    for (base, key), fids in groups.items():
        if len(fids) < 2:
            continue
        good = [fby[i] for i in fids if not fby[i]["final_name"].startswith(("reserved", "opaque"))]
        highs = {fby[i]["final_name"] for i in fids if fby[i]["confidence"] == "high"}
        if len(highs) > 1:
            rows.append(["F-CONFLICT", " ".join(sorted(fids)), base, " / ".join(sorted(highs)), "", "", "high", str(key)])
        for i in fids:
            f = fby[i]
            better = max((x for x in good if x["id"] != i and rank[x["confidence"]] > rank[f["confidence"]]
                          and x["final_name"] != f["final_name"]), key=lambda x: rank[x["confidence"]], default=None)
            if better:
                rows.append(["F-SIBLING", i, T[f["type_id"]]["final_name"], f["final_name"], f["name"],
                             better["final_name"], f["confidence"], f"{better['id']} ({better['confidence']})"])

    # G-UPSTREAM
    pin_addr = symbol_addrs(ROOT / "resources" / "rood-reverseWin")
    up_addr = symbol_addrs(ROOT / "resources" / "rood-reverse")
    pin_by_name = {(ov, n): a for (ov, a), n in pin_addr.items()}
    for r in G:
        if r["confidence"] == "high" or not r["final_name"]:
            continue
        ov = r["overlay"]
        addr = int(r["address"], 16) if r["address"].startswith("0x") else pin_by_name.get((ov, r["name"]))
        if addr is None:
            m = re.fullmatch(r"D_([0-9A-F]{8})", r["name"])
            addr = int(m.group(1), 16) if m else None
        if addr is None:
            continue
        up = up_addr.get((ov, addr))
        if up and meaningful(up) and up != r["name"]:
            rows.append(["G-UPSTREAM", r["id"], r["final_name"], r["final_name"], r["name"], up, r["confidence"],
                         f"{ov} 0x{addr:08X}"])

    # T-UPSTREAM: types whose pinned name is generic but upstream has a meaningful typedef for the same layout key
    for tid, t in T.items():
        if t["confidence"] == "high" or meaningful(t["name"]):
            continue
        base = t["key"].partition("#")[0]
        for x in up_types.get(base, []):
            n = x["key"].split(":", 1)[1]
            if meaningful(n) and n != t["name"]:
                rows.append(["T-UPSTREAM", tid, t["final_name"], t["final_name"], t["name"], n, t["confidence"], ""])

    # F-MEMBER-TYPE: the member's C type is a struct/enum named since (hint for opaque/reserved/low members)
    tname = {}
    for t in T.values():
        for n in [t["name"], *t["aliases"].split()]:
            tname.setdefault(n, t)
    for f in F:
        if f["confidence"] == "high" or f["value"] != "":
            continue
        base = re.sub(r"\b(struct|union|enum|const|volatile)\b|\*|\[.*?\]", "", f["type"]).strip()
        t = tname.get(base)
        if t and t["final_name"] and t["confidence"] == "high" and t["id"] != f["type_id"]:
            leaf = t["final_name"].rsplit("::", 1)[1]
            cand = leaf[0].lower() + leaf[1:] + ("s" if "[" in f["type"] and not leaf.endswith("s") else "")
            if cand.lower() not in f["final_name"].lower():
                rows.append(["F-MEMBER-TYPE", f["id"], T[f["type_id"]]["final_name"], f["final_name"], f["name"],
                             cand, f["confidence"], f"{f['type']} = {t['id']} {t['final_name']}"])

    # F-USE-AVAILABLE: declaration-only members that libclang shows are referenced (cite a use now)
    for f in F:
        if "declaration-only" in f["notes"] and f["type_id"] in T and g.is_used(f["type_id"], f["name"]):
            rows.append(["F-USE-AVAILABLE", f["id"], T[f["type_id"]]["final_name"], f["final_name"], f["name"], "",
                         f["confidence"], "field refs exist"])

    # F-PARAM: an opaque/low member passed straight to a function whose parameter has a meaningful name
    params = {}
    for p in ("", "upstream:"):
        for fn in g.trees[p].ix["functions"].values():
            names = [x["name"] for x in fn.get("params") or []]
            if any(meaningful(n) and not re.fullmatch(r"arg\d+|a\d|p\d?", n) for n in names):
                params.setdefault(fn["name"], names)
    call = re.compile(r"\b(\w+)\s*\(")

    def split_args(s: str, start: int) -> list[str]:
        depth, cur, out = 0, "", []
        for ch in s[start:]:
            if ch == "(":
                depth += 1
                if depth == 1:
                    continue
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    out.append(cur.strip())
                    return out
            elif ch == "," and depth == 1:
                out.append(cur.strip())
                cur = ""
                continue
            if depth >= 1:
                cur += ch
        return out

    for f in F:
        if f["confidence"] == "high" or not (f["final_name"].startswith("opaque") or f["confidence"] == "low") \
                or f["type_id"] not in T or f["value"] != "":
            continue
        t = T[f["type_id"]]
        tnames = {t["name"], *t["aliases"].split()}
        member = re.compile(r"(\.|->)\s*" + re.escape(f["name"]) + r"$")
        found = None
        for p in ("", "upstream:"):
            tree = g.trees[p]
            for fn in tree.ix["functions"].values():
                if not any(f"{n}.{f['name']}" in (fn.get("refs") or {}).get("field", {}) for n in tnames):
                    continue
                ls = tree.lines(fn["file"].removeprefix("src/"))
                for k in range(fn["line"] - 1, min(fn["end_line"], len(ls))):
                    line = ls[k]
                    for m in call.finditer(line):
                        callee = m.group(1)
                        if callee not in params:
                            continue
                        args = split_args(line, m.end() - 1)
                        for i, a in enumerate(args):
                            if member.search(a) and i < len(params[callee]) and meaningful(params[callee][i]) \
                                    and not re.fullmatch(r"arg\d+", params[callee][i]):
                                found = (params[callee][i], f"{callee} param {i} ({p or 'pinned'}"
                                         f" {fn['file'].removeprefix('src/')}:{k + 1})")
                                break
                        if found:
                            break
                    if found:
                        break
                if found:
                    break
            if found:
                break
        if found and found[0].lower() not in f["final_name"].lower():
            rows.append(["F-PARAM", f["id"], t["final_name"], f["final_name"], f["name"], found[0], f["confidence"],
                         found[1]])

    OUT.mkdir(parents=True, exist_ok=True)
    with open(OUT / "report.csv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh, lineterminator="\n")
        w.writerow(["check", "id", "owner", "final_name", "original", "candidate", "confidence", "evidence"])
        w.writerows(rows)
    print(collections.Counter(r[0] for r in rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
