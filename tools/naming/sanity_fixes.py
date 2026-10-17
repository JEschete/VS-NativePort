"""Corrections from the low/med sanity pass (tools\\naming\\sanity_pass.py), each from read code.

Patches the owning batch output (tools\\naming\\sym_out\\<batch>\\{fields,types,globals}.csv) row by id, mirrors field
fixes onto same-as duplicate types, then re-applies the touched batches. Re-run after regenerating any batch whose
generator does not include these fixes.

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
    "T00683.027": ("boneWorldMatrices", "World matrix of each weapon bone", H,
                   [(f"{B}/49268.c", "weapon->unk20[i] = scratch->matrix;"), (f"{B}/49268.c", "scratch->parent = &weapon->unk20[parentBone];")]),
}
GLOBALS: dict = {}
TYPES: dict = {}


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
        else:
            r.update(fx)
        print(f"{b}: {rid} -> {r['final_name']} ({r['confidence']})")
        if not dry:
            with open(p, "w", newline="", encoding="utf-8") as fh:
                w = csv.DictWriter(fh, fieldnames=cols, lineterminator="\n")
                w.writeheader()
                w.writerows(rows)
        touched.add(b)
    return touched


def main() -> int:
    dry = "--dry-run" in sys.argv
    g = Gen("Shared")
    touched = patch("fields", dict(FIELDS), g, dry) | patch("globals", GLOBALS, g, dry) | patch("types", TYPES, g, dry)
    if dry:
        return 0
    merge = ROOT / "tools" / "naming" / "merge_symbols.py"
    for b in sorted(touched):
        r = subprocess.run([sys.executable, str(merge), "--apply-batch", b], capture_output=True, text=True)
        print(b, (r.stdout + r.stderr).strip().splitlines()[-1])
        if r.returncode:
            print(r.stdout, r.stderr)
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
