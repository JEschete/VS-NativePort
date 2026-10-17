"""Corrections from the low/med sanity pass (tools\\naming\\sanity_pass.py), each from read code.

Patches the owning batch output (tools\\naming\\sym_out\\<batch>\\{fields,types,globals}.csv) row by id, mirrors field
fixes onto same-as duplicate types, re-applies the touched batches, then runs tools\\ledger\\reconcile_locations.py.
Re-run after regenerating any batch whose generator does not include these fixes.

  python tools\\naming\\sanity_fixes.py [--dry-run]
"""
from __future__ import annotations

import csv
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent / "sym_gen"))
from common import Gen, ROOT  # noqa: E402

OUTS = ROOT / "tools" / "naming" / "sym_out"
H, M, L = "high", "med", "low"
B = "BATTLE/BATTLE.PRG"
UP = "upstream:" + B

# field id: (final name, meaning, confidence, [(file, text)])
FIELDS = {
    # reserved members that are read (F-RESERVED-USED)
    "T00255.004": ("reserved4", "Copied from the model template's word 4 when the gimmick is created; not read", L,
                   [(f"{B}/30DB0.c", "s1->unk4 = temp_v1_2->unk4;")]),
    "T00255.032": ("rotationTicks", "Frames left in the current rotation tween", H,
                   [(f"{UP}/44F14.c", "step = object->unk3C / object->unk18;"), (f"{UP}/44F14.c", "--object->unk18;")]),
    "T00255.050": ("rotationDeltaX", "X rotation still to apply over rotationTicks", H,
                   [(f"{UP}/44F14.c", "step = object->unk3C / object->unk18;")]),
    "T00255.051": ("rotationDeltaY", "Y rotation still to apply over rotationTicks", H,
                   [(f"{UP}/44F14.c", "step = object->unk3E / object->unk18;")]),
    "T00255.052": ("rotationDeltaZ", "Z rotation still to apply over rotationTicks", H,
                   [(f"{UP}/44F14.c", "step = object->unk40 / object->unk18;")]),
    "T00278.014": ("effectPitchScaleSlideStep", "Per-tick step of the effect pitch scale slide (command 0xA5/0xAD)", H,
                   [("SLUS_010.40/25AC.c", "var_a2->unk44 ="), ("SLUS_010.40/25AC.c", "temp_s1 = var_a1_2->unk40 + var_a1_2->unk44;")]),
    "T00278.020": ("keyOnVolume", "Volume a key-on starts from when a key-on volume slide is set (opcode FE19)", H,
                   [("SLUS_010.40/25AC.c", "in_pChannel->unk5C = ((signed char)*in_pChannel->ProgramCounter++) << 0x17;"),
                    ("SLUS_010.40/25AC.c", "arg0->Volume = arg0->unk5C;")]),
    "T00303.008": ("poseLockSlot", "Locked-pose slot (1 or 2) holding this bone's saved transform; 0 when unlocked", H,
                   [(f"{B}/40564.c", "temp_s2->unkC = i + 1;"), (f"{B}/40564.c", "arg0->unk0.unk68->armatures[i].unkC = 0;")]),
    "T00333.002": ("blocksSphereTerrain", "Target-sphere terrain sampling treats the tile as solid and unoccupiable", M,
                   [(f"{B}/58578.c", "if (room->unk0_6) {")]),
    "T00333.007": ("hasEventTrigger", "Entering the tile evaluates the room's event triggers (may start a cutscene)", H,
                   [(f"{B}/146C.c", "if (tile->unk0_13 && func_800BEC58(2, 0, NULL, 0) == 1) {")]),
    "T00333.010": ("hasRoomTrigger", "The tile has a room trigger (trap, chest or transition) looked up on entry", H,
                   [(f"{B}/146C.c", "if (tile->unk0_16 && !(phaseValue & 0x504000)) {"), (f"{B}/146C.c", "if (temp_s4->unk0_16) {")]),
    "T00333.012": ("occlusionAndFloorFlags", "Bit 0 applies the tile's camera geometry occlusion; bit 1 keeps it sampled as floor", M,
                   [(f"{B}/146C.c", "if (func_8008B764(var_a0, var_a1, var_a2)->unk0_18 & 1) {"),
                    (f"{B}/58578.c", "} else if (room->unk0_10 < 5 || (room->unk0_18 >> 1)) {")]),
    "T00336.006": ("environmentFlags", "Room environment flag byte returned by RoomEnvironmentFlags", M,
                   [(f"{B}/146C.c", "int func_8008D3E8(void) { return vs_battle_roomData.sectionB->unk31; }")]),
    "T00336.021": ("fallDestinationZone", "Zone a fall through this room's pit leads to (high half of RoomFallDestination)", H,
                   [(f"{B}/146C.c", "return (vs_battle_roomData.sectionB->unk52 << 0x10)"),
                    (f"{B}/146C.c", "destination.zndId = phaseValue / 65536;")]),
    "T00336.022": ("fallDestinationRoom", "Room a fall through this room's pit leads to (low half of RoomFallDestination)", H,
                   [(f"{B}/146C.c", "| vs_battle_roomData.sectionB->unk54;"),
                    (f"{B}/146C.c", "destination.mpdId = phaseValue - (phaseValue / 65536) * 65536;")]),
    "T00336.023": ("opaque56", "Returned by RoomEnvironmentParameter; no caller in decompiled code", L,
                   [(f"{B}/146C.c", "short func_8008E4AC(void) { return vs_battle_roomData.sectionB->unk56; }")]),
    "T00435.001": ("actorIndex", "Actor that triggered the trap or opens the chest", H,
                   [(f"{B}/146C.c", "D_800F1910.unk4.unkC = arg1;"),
                    (f"{B}/146C.c", "func_80073E30(D_800F1910.unk0, D_800F1910.unk4.unkC);")]),
    "T00675.015": ("opaque14", "Copied into the weapon stats (unk10B) and inventory blade; meaning not established", L,
                   [(f"{B}/146C.c", "target->unk10B = source->blade.unk14;")]),
    "T00683.010": ("ownerMotionState", "Copy of the owner's motion/appearance state during appearance transitions", M,
                   [(f"{B}/40564.c", "weapon->unk9_0 = arg0->unk9_0;")]),
    "T00269.000": ("assetCount", "Number of effect texture files to load", H,
                   [(f"{B}/6F014.c", "void func_800D7FFC(int arg0) { D_800F5638.unk0 = arg0; }"),
                    (f"{B}/6F014.c", "if (D_800F5638.unk0 == D_800F5630) {")]),
    "T00269.001": ("loadContexts", "Disc location (lba, size) of each texture file", H,
                   [(f"{B}/6F014.c", "D_800F5638.loadContexts[index].lba = lba;")]),
    "T00269.002": ("slots", "Texture or CLUT slot each file is uploaded to", H,
                   [(f"{B}/6F014.c", "D_800F5638.slots[index] = slot;")]),
    "T00269.003": ("isClut", "Set when the file is a CLUT rather than texture pixels", H,
                   [(f"{B}/6F014.c", "D_800F5638.isClut[index] = isClut;")]),
    "T00483.000": ("kind", "Volume kind (as TargetSphereSpec.kind); 2 is transformed by its scale and angle", M,
                   [(f"{B}/58578.c", "switch (shape->unk0) {")]),
    "T00483.002": ("angle", "Rotation angle passed to BuildSphereVolumeTransform", M,
                   [(f"{B}/58578.c", "m = func_800C085C(shape->unk4.values, shape->unk2);")]),
    "T00483.003": ("scale", "Scale values passed to BuildSphereVolumeTransform (y doubled first)", M,
                   [(f"{B}/58578.c", "shape->unk4.values[1] <<= 1;")]),
    "T00483.004": ("position", "Volume position; offset by half the transformed height and sampled for terrain", M,
                   [(f"{B}/58578.c", "shape->unk8[j] += m->m[j][1] >> 1;"),
                    (f"{B}/5BF94.c", "func_800C2B0C((SVECTOR*)state->unk0[0].unk8, state->unk2A - 1);")]),
    "T00217.004": ("reserved4", "Bits of Battle::TargetVolume.scale; not read through this view", L,
                   [(f"{B}/58578.c", "shape->unk4.values[1] <<= 1;")]),
    "T00517.008": ("targetLimb", "Limb the candidate's action targets (passed to PreviewActionAgainstLimb)", H,
                   [(f"{B}/74088.c", "func_8008631C(arg2->unk4_5, arg0->unk88, temp_s0, arg2->unkC_4, D_800F5900);")]),
    "T00714.002": ("storedContainer", "Container contents copied into the working container when the screen opens", M,
                   [("MENU/MENUD.PRG/234.c", "&vs_menu_inventoryStorage->unkFB0,")]),
    "T00714.003": ("transferPreview", "Destination contents with the staged miscellaneous items added, for capacity counts", M,
                   [("MENU/MENUD.PRG/234.c", "vs_menu_containerData* container = &vs_menu_inventoryStorage->unk4BB0;"),
                    ("MENU/MENUD.PRG/234.c", "_countContainerItems(itemCategoryMisc, &vs_menu_inventoryStorage->unk4BB0);")]),
    "T00714.004": ("stagedTransfer", "Items staged for transfer, counted with the destination for capacity", M,
                   [("MENU/MENUD.PRG/234.c", "&vs_menu_inventoryStorage->unk87B0, sizeof vs_menu_inventoryStorage->unk87B0);"),
                    ("MENU/MENUD.PRG/234.c", "temp_s0_2 + _countContainerItems(s3, &vs_menu_inventoryStorage->unk87B0);")]),
    "T00714.007": ("gadgetBag", "Working gadget bag; browsed with the gadget bag item capacities", M,
                   [("MENU/MENUD.PRG/234.c", "vs_menuD_containerData = &vs_menu_inventoryStorage->unk105B0;")]),
    "T00266.005": ("stepCurve", "Phase step: a curve id sampled when flag bit 5 is set, else the constant step", M,
                   [(f"{B}/5BF94.c", "var_v0 = vs_battle_sampleCurve(arg0->unk2, arg0->unk0);")]),
    "T00266.006": ("randomStepCurve", "Second step value: a curve id sampled when flag bit 6 is set, else a constant", M,
                   [(f"{B}/5BF94.c", "var_s0 = vs_battle_sampleCurve(arg0->unk3, arg0->unk0);")]),
    "T00683.027": ("boneWorldMatrices", "World matrix of each weapon bone", H,
                   [(f"{B}/49268.c", "weapon->unk20[i] = scratch->matrix;"), (f"{B}/49268.c", "scratch->parent = &weapon->unk20[parentBone];")]),
}
# global id: (final name, purpose, confidence, [(file, text)])
GLOBALS = {
    "G00389": ("Content::legacySequenceBufferOwned",
               "Is set when the sequence file was read into a heap buffer that parsing must free; 0 for shared sequences.",
               H, [(f"{B}/30DB0.c", "D_800F22D0 = 1;"), (f"{B}/30DB0.c", "if (D_800F22D0) {")]),
}
# type id: (final name, purpose, confidence, [(file, text)])
TYPES = {
    "T00269": ("Effects::TextureLoadList",
               "Lists the effect texture files to load: count, disc locations, upload slots and CLUT flags.", H,
               [(f"{B}/6F014.c", "D_800F5638.loadContexts[index].lba = lba;"),
                (f"{B}/6F014.c", "var_s0 = _loadfile(D_800F5638.loadContexts[D_800F5630].lba + VS_E000_P_LBA,")]),
}


def batches_with(kind: str) -> dict[str, str]:
    out = {}
    for d in OUTS.iterdir():
        p = d / f"{kind}.csv"
        if p.exists():
            for r in csv.DictReader(open(p, encoding="utf-8")):
                out[r["id"]] = d.name
    return out


def patch(kind: str, fixes: dict, g: Gen, dry: bool) -> set[str]:
    owner = batches_with(kind)
    touched = set()
    # same-as duplicates mirror their canonical's field names
    if kind == "fields":
        dups = {}
        for d in OUTS.iterdir():
            p = d / "types.csv"
            if p.exists():
                for r in csv.DictReader(open(p, encoding="utf-8")):
                    if "same-as:" in r["notes"]:
                        dups.setdefault(r["notes"].split("same-as:")[1][:6], []).append(r["id"])
        for fid in list(fixes):
            tid, idx = fid.split(".")
            for dup in dups.get(tid, []):
                fixes.setdefault(f"{dup}.{idx}", fixes[fid] + ("same-as",))
    for rid, fx in fixes.items():
        b = owner.get(rid)
        if not b:
            sys.exit(f"{rid}: no batch output contains it")
        p = OUTS / b / f"{kind}.csv"
        rows = list(csv.DictReader(open(p, encoding="utf-8")))
        cols = list(rows[0].keys())
        r = next(x for x in rows if x["id"] == rid)
        if kind == "fields":
            name, meaning, conf, cites, *same = fx
            notes = "same-as row" if same else g.c(*cites) + ";sanity pass 2026-10-17"
            if same:
                notes = r["notes"]
            r.update(final_name=name, meaning=meaning, confidence=conf, notes=notes)
        elif kind == "globals":
            name, purpose, conf, cites = fx
            keep = [t for t in r["notes"].split(";") if t.startswith(("replaced-by:", "dup-of:", "view-of:"))]
            r.update(final_name=name, purpose=purpose, confidence=conf,
                     notes=";".join(keep + [g.c(*cites), "sanity pass 2026-10-17"]))
        else:
            name, purpose, conf, cites = fx
            r.update(final_name=name, purpose=purpose, confidence=conf,
                     notes=";".join([g.c(*cites), "sanity pass 2026-10-17"]))
        print(f"{b}: {rid} -> {r['final_name']} ({r['confidence']})")
        if not dry:
            with open(p, "w", newline="", encoding="utf-8") as fh:
                w = csv.DictWriter(fh, fieldnames=cols, lineterminator="\n")
                w.writeheader()
                w.writerows(rows)
        touched.add(b)
    return touched


def cite_declaration_only(g: Gen, dry: bool) -> set[str]:
    """Replace `declaration-only` evidence with a use when libclang shows the member is referenced and the citing
    function references no other type with a member of that name (so the receiver cannot be another type)."""
    import re
    owner = batches_with("fields")
    fields = {}
    for d in OUTS.iterdir():
        p = d / "fields.csv"
        if p.exists():
            fields[d.name] = list(csv.DictReader(open(p, encoding="utf-8")))
    members = {m["id"]: m for ms in g.members.values() for m in ms}
    by_member: dict[str, list[tuple]] = {}
    for p in ("", "upstream:"):
        for fn in g.trees[p].ix["functions"].values():
            for k in (fn.get("refs") or {}).get("field", {}):
                by_member.setdefault(k.rsplit(".", 1)[1], []).append((p, fn, k.rsplit(".", 1)[0]))
    touched, n = set(), 0
    for b, rows in fields.items():
        changed = False
        for r in rows:
            if "declaration-only" not in r["notes"] or r["id"] not in members:
                continue
            m = members[r["id"]]
            t = g.types[m["type_id"]]
            names = {t["name"], *t.get("aliases", "").split()}
            rx = re.compile(r"(\.|->)\s*" + re.escape(m["name"]) + r"(?!\w)")
            cite = None
            for p, fn, owner_type in by_member.get(m["name"], []):
                if owner_type not in names:
                    continue
                others = {k.rsplit(".", 1)[0] for k in fn["refs"]["field"] if k.rsplit(".", 1)[1] == m["name"]}
                if others - names:
                    continue
                tree = g.trees[p]
                rel = fn["file"].removeprefix("src/")
                ls = tree.lines(rel)
                for i in range(fn["line"] - 1, min(fn["end_line"], len(ls))):
                    if rx.search(ls[i]):
                        cite = f"{tree.label(rel)}:{i + 1}"
                        break
                if cite:
                    break
            if cite:
                r["notes"] = r["notes"].replace("declaration-only;", f"{cite};use cited by sanity pass;", 1)
                changed, n = True, n + 1
        if changed and not dry:
            with open(OUTS / b / "fields.csv", "w", newline="", encoding="utf-8") as fh:
                w = csv.DictWriter(fh, fieldnames=list(rows[0].keys()), lineterminator="\n")
                w.writeheader()
                w.writerows(rows)
            touched.add(b)
    print(f"declaration-only members given a use citation: {n}")
    return touched


def main() -> int:
    dry = "--dry-run" in sys.argv
    g = Gen("Shared")
    touched = patch("fields", dict(FIELDS), g, dry) | patch("globals", GLOBALS, g, dry) | patch("types", TYPES, g, dry)
    touched |= cite_declaration_only(g, dry)
    if dry:
        return 0
    merge = ROOT / "tools" / "naming" / "merge_symbols.py"
    for b in sorted(touched):
        r = subprocess.run([sys.executable, str(merge), "--apply-batch", b], capture_output=True, text=True)
        print(b, (r.stdout + r.stderr).strip().splitlines()[-1])
        if r.returncode:
            print(r.stdout, r.stderr)
            return 1
    # globals placed in data-only files move to the catalog file that uses them (tools\ledger\reconcile_locations.py)
    sys.path.insert(0, str(ROOT / "tools" / "ledger"))
    import reconcile_locations
    sys.argv = [sys.argv[0]]
    return reconcile_locations.main()


if __name__ == "__main__":
    sys.exit(main())
