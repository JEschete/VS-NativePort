"""Shared generator for the menu global batches (Game_Menu_03..06), from code read directly.

For each global the generator collects its use lines in its own overlay sources (and shared MENU headers) and the
catalog name of the enclosing function:
- a meaningful fork name is kept (restyled), cited by its first use, in the namespace and file of its main user;
- an address-named (or forbidden-token) global is named from a recognised use pattern (cursor animation, OK blink,
  yes/no selection, selected row, step switch, slide counters...), the pattern line being the citation, prefixed
  with the enclosing function's context; anything else must be in MANUAL (written from the read uses) or fails;
- a static defined in a shared header compiles into several overlays: later copies are views of the first;
- a global with no use is dropped as unreferenced.
"""
from __future__ import annotations

import csv
import json
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from common import Gen, ROOT, LEDGERS  # noqa: E402

SRC = ROOT / "resources" / "rood-reverseWin" / "src"
L, M, H = "low", "med", "high"
FORB = re.compile(r"(^|_)(unk|temp|Unk|Temp)|[0-9A-Fa-f]{8}|^(D|B)_")
OVERLAY_DEFAULT = {
    "MENU/MAINMENU.PRG": ("Menu::Framework", "Native\\Game\\src\\Menu\\Framework\\MainMenu.c"),
    "MENU/MENU0.PRG": ("Menu::Magic", "Native\\Game\\src\\Menu\\Magic\\Magic.c"),
    "MENU/MENU1.PRG": ("Menu::BreakArts", "Native\\Game\\src\\Menu\\BreakArts\\BreakArts.c"),
    "MENU/MENU2.PRG": ("Menu::BattleAbilities", "Native\\Game\\src\\Menu\\BattleAbilities\\BattleAbilities.c"),
    "MENU/MENU3.PRG": ("Menu::Inventory", "Native\\Game\\src\\Menu\\Inventory\\Inventory.c"),
    "MENU/MENU4.PRG": ("Menu::Status", "Native\\Game\\src\\Menu\\Status\\Status.c"),
    "MENU/MENU5.PRG": ("Menu::Map", "Native\\Game\\src\\Menu\\Map\\Map.c"),
    "MENU/MENU7.PRG": ("Menu::Files", "Native\\Game\\src\\Menu\\Files\\Files.c"),
    "MENU/MENU8.PRG": ("Menu::Options", "Native\\Game\\src\\Menu\\Options\\Options.c"),
    "MENU/MENU9.PRG": ("Menu::Score", "Native\\Game\\src\\Menu\\Score\\Score.c"),
    "MENU/MENUB.PRG": ("Menu::Loot", "Native\\Game\\src\\Menu\\Loot\\Loot.c"),
    "MENU/MENUC.PRG": ("Menu::Setup", "Native\\Game\\src\\Menu\\Setup\\Setup.c"),
    "MENU/MENUD.PRG": ("Menu::Container", "Native\\Game\\src\\Menu\\Container\\Container.c"),
    "MENU/MENUE.PRG": ("Menu::Manual", "Native\\Game\\src\\Menu\\Manual\\Manual.c"),
    "MENU/MENUF.PRG": ("Menu::Results", "Native\\Game\\src\\Menu\\Results\\Results.c"),
    "TITLE/TITLE.PRG": ("Title", "Native\\Game\\src\\Title\\TitleScreen.c"),
    "BATTLE/BATTLE.PRG": ("Menu::Framework", "Native\\Game\\src\\Menu\\Framework\\Menus.c"),
    "SLUS_010.40": ("Core", "Native\\Core\\src\\Main.c"),
}

_ix = json.loads((ROOT / "build" / "port" / "index.json").read_text(encoding="utf-8"))
_cat = {(r["file"].replace("\\", "/"), r["name"]): r for r in csv.DictReader(open(LEDGERS / "FUNCTION_CATALOG.csv", encoding="utf-8"))}
FUNCS = [(f["file"].removeprefix("src/"), f["line"], f["end_line"], f["name"]) for f in _ix["functions"].values()]
_files = sorted({p.relative_to(SRC).as_posix() for p in SRC.rglob("*") if p.suffix in (".c", ".h")})
_lines: dict[str, list[str]] = {}


def overlay_default(ov: str):
    """Majority namespace of the overlay's catalogued functions and that namespace's majority file."""
    rows = [r for r in _cat.values() if r["final_name"] and (r["file"].startswith(ov + "/") or r["file"] == ov)]
    if not rows:
        return OVERLAY_DEFAULT.get(ov, ("Menu", "Native\\Game\\src\\Menu\\Menu.c"))
    ns = Counter(r["final_name"].rsplit("::", 1)[0] for r in rows).most_common(1)[0][0]
    loc = Counter(r["final_location"] for r in rows if r["final_name"].rsplit("::", 1)[0] == ns).most_common(1)[0][0]
    return ns, loc


def lines(rel):
    if rel not in _lines:
        _lines[rel] = (SRC / rel).read_text(encoding="utf-8", errors="replace").splitlines()
    return _lines[rel]


def overlay_files(ov: str) -> list[str]:
    if ov == "SLUS_010.40":
        return [f for f in _files if f.startswith("SLUS_010.40/")]
    own = [f for f in _files if f.startswith(ov + "/")]
    if ov.startswith(("MENU/", "TITLE/")):
        own += [f for f in _files if f.startswith("MENU/") and f.count("/") == 1]  # shared MENU headers
    return own


def enclosing(rel, n):
    fn = next((x for x in FUNCS if x[0] == rel and x[1] <= n <= x[2]), None)
    if not fn:
        return None
    row = _cat.get((fn[0], fn[3]))
    return (row["final_name"], row["final_location"]) if row and row["final_name"] else (fn[3], None)


def scope(r) -> list[tuple[str, int, int]]:
    """(file, first line, last line) ranges where the global is visible, from the ledger key."""
    where = r["key"].split("@", 1)[1].removeprefix("src/")
    if r["linkage"] == "local-static":
        rel, fn = where.split(":")
        x = next(x for x in FUNCS if x[0] == rel and x[3] == fn)
        return [(rel, x[1], x[2])]
    if r["linkage"] == "file-static":
        if where.endswith(".h"):  # a header static compiles into the including overlay
            return [(f, 1, 10**9) for f in dict.fromkeys([where] + [f for f in overlay_files(r["overlay"]) if f.endswith(".c")])]
        return [(where, 1, 10**9)]
    if r["linkage"] == "per-instance":
        files = [where] + [f for ov in r["instances"].split() for f in overlay_files(ov) if not f.endswith(".h")]
        return [(f, 1, 10**9) for f in dict.fromkeys(files)]
    return [(f, 1, 10**9) for f in overlay_files(r["overlay"])]


def uses(r):
    rx = re.compile(r"(?<!\w)" + re.escape(r["name"]) + r"(?!\w)")
    out = []
    for f, lo, hi in scope(r):
        ls = lines(f)
        for i in range(lo - 1, min(hi, len(ls))):
            if rx.search(ls[i]) and not ls[i].lstrip().startswith("#define"):
                out.append((f, i + 1, ls[i].strip(), enclosing(f, i + 1)))
    return out


def label(rel, n):
    base = rel.rsplit("/", 1)[-1]
    unique = sum(1 for f in _files if f.rsplit("/", 1)[-1] == base) == 1
    return f"source:{base if unique else rel}:{n}"


def camel(n: str) -> str:
    orig = n
    n = re.sub(r"^_+", "", n)
    n = re.sub(r"^vs_(?:battle|main|mainMenu|mainmenu|menu[0-9A-F]?)_", "", n)
    parts = [p for p in n.split("_") if p] or [p for p in orig.split("_") if p and p != "vs"]
    s = parts[0][0].lower() + parts[0][1:] + "".join(p[0].upper() + p[1:] for p in parts[1:])
    return re.sub(r"^([a-z])([A-Z])(?=[A-Z][a-z])", lambda m: m[1] + m[2].lower(), s)  # uIElement -> uiElement


def context(fn_name: str) -> str:
    short = fn_name.rsplit("::", 1)[-1]
    short = re.sub(r"^(Update|Select|Draw|Render|Handle|Show)", "", short) or short
    return short[0].lower() + short[1:]


PATTERNS = [  # (regex on a use line with NAME substituted, role, meaning)
    (r"NAME\s*=\s*vs_mainMenu_renderCursor\(\s*NAME", "CursorAnimation", "Animation phase of the menu cursor"),
    (r"_drawOk\([^;]*NAME\)", "OkBlinkPhase", "Blink phase of the OK marker (advanced by 16)"),
    (r"getMenuItem\(\s*(?:30|0x1E) \+ NAME\)->selected", "YesNoChoice", "Selected answer of the yes/no prompt (0 yes, 1 no)"),
    (r"NAME = (?:_getSelectedRow|vs_mainmenu_getSelectedRow)\(\) \+ 1", "SelectedRow", "Selected row plus one (0 none, -1 cancelled)"),
    (r"NAME = vs_mainMenu_actionInputProcessed\(\) \+ 1", "SelectedAction", "Selected action plus one (0 none)"),
    (r"switch \(NAME\)", "Step", "Step of this function's state machine"),
    (r"if \(NAME < (?:0xAU|10)\)", "SlideStep", "Slide-in step, counted up to 10"),
    (r"if \(NAME < 5\)", "AppearStep", "Appearance step, counted up to 5"),
    (r"NAME = (?:arg0|weaponIndex|bladeIndex) >> 8;", "ItemSource", "High byte of the argument: where the shown item comes from"),
    (r"NAME = \(?(?:arg0|bladeIndex) - 1\)?;", "ItemIndex", "Index of the shown item (argument minus one)"),
]


def from_pattern(name, us):
    """First use whose line (joined with the next one) matches a known role pattern."""
    for rx, role, meaning in PATTERNS:
        r = re.compile(rx.replace("NAME", re.escape(name)))
        for k, (f, n, ln, fn) in enumerate(us):
            nxt = lines(f)[n].strip() if n < len(lines(f)) else ""
            if (r.search(ln) or r.search(ln + " " + nxt)) and fn:
                return role, meaning, label(f, n), fn
    return None


class MenuGlobals:
    def __init__(self, batch: str, manual: dict):
        self.g = Gen(batch)
        self.rows = list(csv.DictReader(open(ROOT / "tools" / "naming" / "sym_batches" / batch / "globals.csv", encoding="utf-8")))
        self.manual = manual
        self.unresolved = []
        self.used_names: dict[str, str] = {}
        self.ledger = list(csv.DictReader(open(LEDGERS / "DATA_LEDGER.csv", encoding="utf-8")))
        mine = {r["id"] for r in self.rows}
        for x in self.ledger:
            if x["final_name"] and x["id"] not in mine and x["linkage"] != "local-static":
                self.used_names[x["final_name"].replace("::", "_")] = x["id"]
        self.named_by_name = {x["name"]: x for x in self.ledger if x["final_name"] and x["id"] not in mine}
        self.pad: dict[str, int] = {}

    def emit(self, r, disp, final, loc, purpose, conf, notes):
        cname = final.replace("::", "_")
        if cname in self.used_names and r["linkage"] != "local-static":
            final = final + r["overlay"].split("/")[-1].split(".")[0].title().replace("Mainmenu", "MainMenu")
            cname = final.replace("::", "_")
        self.used_names[cname] = r["id"]
        if disp == "constexpr":
            ns, _, leaf = final.rpartition("::")
            final = f"{ns}::k{leaf[0].upper()}{leaf[1:]}" if not leaf.startswith("k") or not leaf[1:2].isupper() else final
        self.g.G(r["id"], disp, final, loc, purpose, conf, notes)

    def run(self):
        for r in self.rows:
            gid, name, ov = r["id"], r["name"], r["overlay"]
            us = uses(r)
            decl_rx = re.compile(r"^(static\s+)?(const\s+)?[\w\s\*]+?\b" + re.escape(name) + r"\b\s*(\[[^\]]*\])*\s*(__attribute__|=|;|\[)")
            definition = next((u for u in us if decl_rx.match(u[2])), None) or next((u for u in us if u[2].startswith("extern")), None)
            us = [u for u in us if u is definition or not u[2].startswith("extern")]
            real = [u for u in us if u is not definition]
            users = [u[3] for u in real if u[3] and u[3][1]]
            ns, loc = overlay_default(ov)
            ns_counts = Counter(fn.rsplit("::", 1)[0] for fn, _ in users if "::" in fn)
            if ns_counts:
                ns = ns_counts.most_common(1)[0][0]
                loc = Counter(l for fn, l in users if fn.rsplit("::", 1)[0] == ns).most_common(1)[0][0]
            owner = None
            if r["linkage"] == "local-static":
                rel, fn = r["key"].split("@", 1)[1].removeprefix("src/").split(":")
                row = _cat.get((rel, fn))
                if row and row["final_name"]:
                    owner = row["final_name"]
                    ns, loc = owner.rsplit("::", 1)[0], row["final_location"]
            if definition and definition[2].startswith("extern") and name in self.named_by_name:
                x = self.named_by_name[name]
                self.g.G(gid, x["disposition"], x["final_name"], x["final_location"], x["purpose"], x["confidence"],
                         f"dup-of:{x['id']};" + label(*definition[:2]) + ";extern declaration of the same object")
                continue
            if gid in self.manual:
                final, disp, purpose, conf, cites, *over = self.manual[gid]
                loc = over[0] if over else loc
                notes = self.g.c(*cites) if cites else ""
                self.emit(r, disp, final if "::" in final else f"{ns}::{final}", loc, purpose, conf, notes)
                continue
            if re.fullmatch(r"_\d?", name):
                real = [u for u in real if not decl_rx.match(u[2])]  # other padding declarations of the same name
                stores = [u for u in real if re.fullmatch(re.escape(name) + r"(\[\d+\])? = -?(0x)?[0-9A-Fa-f]+;", u[2])]
                if len(stores) != len(real) or not definition:
                    self.unresolved.append((gid, name, "padding name with real uses", [u[2] for u in real][:4]))
                    continue
                self.pad[ns] = self.pad.get(ns, 0) + 1
                leaf = (context(owner) + "LayoutPadding") if owner else f"layoutPadding{self.pad[ns]}"
                why = "Pads the original data layout" + ("; only ever cleared, never read." if stores else ".")
                self.emit(r, "drop", f"{ns}::{leaf}", loc, why, H,
                          "replaced-by:nothing;layout padding;" + label(*definition[:2]) + "".join(";" + label(*u[:2]) for u in stores[:1]))
                continue
            if not real:
                if not definition:
                    self.unresolved.append((gid, name, "no uses and no definition"))
                    continue
                if FORB.search(name):
                    self.pad[ns + "#unused"] = self.pad.get(ns + "#unused", 0) + 1
                    leaf = context(owner) + "Value" if owner else f"global{self.pad[ns + '#unused']}"
                else:
                    leaf = camel(name)
                self.emit(r, "drop", f"{ns}::unused{leaf[0].upper()}{leaf[1:]}", loc,
                          "Is declared but not used by the decompiled code.", H,
                          "replaced-by:nothing;no uses;" + label(*definition[:2]))
                continue
            if FORB.search(name):
                p = from_pattern(name, real)
                if not p:
                    self.unresolved.append((gid, name, [f"{u[0].rsplit('/', 1)[-1]}:{u[1]} [{u[3][0] if u[3] else '-'}] {u[2][:100]}" for u in real[:5]]))
                    continue
                role, meaning, cite, fn = p
                final = f"{fn[0].rsplit('::', 1)[0]}::{context(fn[0])}{role}"
                self.emit(r, "state", final, fn[1] or loc, f"{meaning}; in {fn[0]}.", M, cite)
                continue
            leaf = camel(name)
            if owner and not leaf.lower().startswith(context(owner).lower()[:5]):
                leaf = context(owner) + leaf[0].upper() + leaf[1:]
            fnames = sorted({u[3][0] for u in real if u[3]})
            purpose = "Fork name kept; used by " + ", ".join(fnames[:3]) + (f" and {len(fnames) - 3} more" if len(fnames) > 3 else "") + "."
            disp = r["proposed_disposition"] if r["proposed_disposition"] in ("state", "catalog", "constexpr", "asset") else "state"
            self.emit(r, disp, f"{ns}::{leaf}", loc, purpose[:200], M, label(*real[0][:2]) + ";fork name kept")
        return self

    def write(self, partial=True):
        if self.unresolved:
            for u in self.unresolved:
                print("UNRESOLVED", u)
        self.g.write(partial=partial or bool(self.unresolved))
