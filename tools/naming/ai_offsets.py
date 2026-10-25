"""Per-offset evidence for the AI actor state (upstream func_800E78F4_t, ~0x484 bytes) and every fork view of it.

python ai_offsets.py [out]  -> lines: +0xOFF  <member names>  then use lines (pinned views, then upstream)
"""
import collections
import csv
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PIN, UPS = ROOT / "resources/rood-reverseWin", ROOT / "resources/rood-reverse"
AIF = ("6FC00.c", "71618.c", "73700.c", "74088.c", "771A8.c", "7CD68.c", "7EE98.c", "6F014.c")
NOT_STATE = {"actorObject", "actionMetadataStats", "actorInitializationModel", "movementStepActor", "radialMotionContext",
             "actionSetupContext"}
pix = json.loads((ROOT / "build/port/index.json").read_text(encoding="utf-8"))
uix = json.loads((ROOT / "build/port/index_upstream.json").read_text(encoding="utf-8"))
with (ROOT / "Docs/ledgers/FUNCTION_CATALOG.csv").open(encoding="utf-8") as stream:
    cat = {(r["file"].replace("\\", "/"), r["name"]): r["final_name"].split("::")[-1] or r["name"]
           for r in csv.DictReader(stream)}


def views():
    out = {}
    for t in pix["types"].values():
        n = t["key"].split(":", 1)[1]
        if (t["file"].endswith(AIF) and t["fields"] and not t["key"].startswith("typedef") and t["size"]
                and t["size"] >= 300 and n not in NOT_STATE):
            out[n] = t
    return out


def users(ix):
    u = collections.defaultdict(list)
    for fn in ix["functions"].values():
        for k in (fn.get("refs") or {}).get("field", {}):
            u[k].append(fn)
    return u


cache = {}


def lines(root, rel):
    key = (root, rel)
    if key not in cache:
        cache[key] = (root / rel).read_text(errors="ignore").splitlines()
    return cache[key]


def hits(root, ix_users, tname, member, mx, label):
    rx = re.compile(r"(->|\.)" + re.escape(member) + r"\b")
    out, seen = [], set()
    for fn in ix_users.get(f"{tname}.{member}", []):
        L = lines(root, fn["file"])
        for i in range(fn["line"] - 1, min(fn["end_line"], len(L))):
            s = L[i].strip()
            if rx.search(s) and s not in seen:
                seen.add(s)
                name = cat.get((fn["file"].removeprefix("src/"), fn["name"]), fn["name"]) if label == "W" else fn["name"]
                out.append(f"    {label} {name} [{fn['file'].rsplit('/', 1)[-1]}:{i + 1}] {s[:105]}")
                break
        if len(out) >= mx:
            break
    return out


def main():
    V = views()
    pu, uu = users(pix), users(uix)
    can = next(t for t in uix["types"].values() if t["key"] == "struct:func_800E78F4_t")
    by = collections.defaultdict(list)
    for n, t in V.items():
        for f in t["fields"]:
            by[f["offset"] // 8].append((n, f))
    out = []
    cf = collections.defaultdict(list)
    for f in can["fields"]:
        cf[f["offset"] // 8].append(f)
    for off in sorted(set(by) | set(cf)):
        names = sorted({f"{f['name']}:{f['type']}{':' + str(f['bits']) if f['bits'] else ''}" for _, f in by.get(off, [])
                        if not re.fullmatch(r"pad\w*", f["name"])})
        ups = [f"{f['name']}:{f['type']}" for f in cf.get(off, [])]
        out.append(f"+0x{off:03X} up={ups} fork={names[:6]}")
        ev = []
        for n, f in by.get(off, []):
            if not re.fullmatch(r"pad\w*", f["name"]):
                ev += hits(PIN, pu, n, f["name"], 1, "W")
            if len(ev) >= 3:
                break
        for f in cf.get(off, []):
            ev += hits(UPS, uu, "func_800E78F4_t", f["name"], 2, "U")
        out += ev
    text = "\n".join(out)
    if len(sys.argv) > 1:
        Path(sys.argv[1]).write_text(text, encoding="utf-8")
        print(len(out), "lines")
    else:
        print(text)


if __name__ == "__main__":
    main()
