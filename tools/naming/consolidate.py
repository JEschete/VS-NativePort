"""Consolidate scattered namespaces/files in the naming outputs (one-time review step, idempotent).

Each rule maps (namespace, current file) -> (new namespace, new file). Function leaf names never change, so
the only collision risk is two leaves meeting in the same namespace; that is checked before anything is written.

  python tools/naming/consolidate.py            dry run: print moves and collisions
  python tools/naming/consolidate.py --write    apply to tools/naming/out/*.csv
"""
from __future__ import annotations

import argparse
import collections
import csv
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from merge_names import HEADER, OUTS, c_spelling, dup_target, read_csv  # noqa: E402

G = "Native\\Game\\src\\"


def core_math_file(leaf: str) -> str:
    if leaf.startswith("Interpolate"):
        return "Interpolation"
    if re.search(r"Sine|Cosine|Atan|Trig", leaf):
        return "Trig"
    if re.search(r"Matrix|Rotation|Orientation|LookAt", leaf):
        return "Matrix"
    if re.search(r"Fixed|Clamp|Digit|Multiply", leaf):
        return "FixedPoint"
    return "Vector"


# (namespace, file-or-None for any) -> (new namespace, new location; may be a callable of the leaf name)
RULES: list[tuple[str, str | None, str, object]] = [
    # Audio: one sequencer namespace; mixer commands live with the mixer
    ("Audio::Sequence", None, "Audio::Seq", "Native\\Engine\\src\\Audio\\Seq\\MusicSequence.c"),
    ("Audio::Driver", "Native\\Engine\\src\\Audio\\Driver\\Sequencer.c", "Audio::Seq", "Native\\Engine\\src\\Audio\\Seq\\Sequencer.c"),
    ("Audio::Commands", None, "Audio::Mixer", "Native\\Engine\\src\\Audio\\Mixer\\MixControl.c"),
    ("Audio", "Native\\Engine\\src\\Audio\\ActorSounds.c", "Audio", "Native\\Engine\\src\\Audio\\ActorSound.c"),
    # Core: math, memory, random each get one namespace
    ("Core", "Native\\Engine\\src\\Core\\Math.c", "Core::Math", lambda leaf: f"Native\\Engine\\src\\Core\\Math\\{core_math_file(leaf)}.c"),
    ("Core", "Native\\Engine\\src\\Core\\Matrix.c", "Core::Math", "Native\\Engine\\src\\Core\\Math\\Matrix.c"),
    ("Core", "Native\\Engine\\src\\Core\\Numbers.c", "Core::Math", "Native\\Engine\\src\\Core\\Math\\FixedPoint.c"),
    ("Core::FixedMath", None, "Core::Math", "Native\\Engine\\src\\Core\\Math\\Interpolation.c"),
    ("Core", "Native\\Engine\\src\\Core\\Memory.c", "Core::Memory", "Native\\Engine\\src\\Core\\Memory\\Copy.c"),
    ("Core::Memory", "Native\\Engine\\src\\Core\\Memory\\WordBuffer.c", "Core::Memory", "Native\\Engine\\src\\Core\\Memory\\Copy.c"),
    ("Core", "Native\\Engine\\src\\Core\\Random.c", "Core::Random", "Native\\Engine\\src\\Core\\Random.c"),
    ("Core::Random", None, "Core::Random", "Native\\Engine\\src\\Core\\Random.c"),
    # Graphics: texture-management singletons into one file
    ("Render", "Native\\Engine\\src\\Render\\TexturePages.c", "Render", "Native\\Engine\\src\\Render\\Textures.c"),
    ("Render", "Native\\Engine\\src\\Render\\TextureTransfer.c", "Render", "Native\\Engine\\src\\Render\\Textures.c"),
    ("Render", "Native\\Engine\\src\\Render\\TextureSlots.c", "Render", "Native\\Engine\\src\\Render\\Textures.c"),
    ("Render", "Native\\Engine\\src\\Render\\Primitives.c", "Render", "Native\\Engine\\src\\Render\\UiPrimitives.c"),
    ("Render", "Native\\Engine\\src\\Render\\ModelRendering.c", "Render", "Native\\Engine\\src\\Render\\Model.c"),
    ("Render", "Native\\Engine\\src\\Render\\ScreenTransition.c", "Render", "Native\\Engine\\src\\Render\\FramebufferEffects.c"),
    ("Input", "Native\\Engine\\src\\Input\\Pad.c", "Input", "Native\\Engine\\src\\Input\\Controller.c"),
    # Game modules
    ("Actors", G + "Actors\\Models.c", "Actors", G + "Actors\\ModelRendering.c"),
    ("Battle", G + "Battle\\BattleAbilities.c", "Battle", G + "Battle\\Abilities.c"),
    ("Battle", G + "Battle\\Actions.c", "Battle", G + "Battle\\ActionFlow.c"),
    ("Battle", G + "Battle\\TargetSelection.c", "Battle", G + "Battle\\Targeting.c"),
    ("Boot", G + "Boot\\Gameplay.c", "Boot", G + "Boot\\GameLoop.c"),
    ("Effects", G + "Effects\\ResourceSetup.c", "Effects", G + "Effects\\EffectResources.c"),
    ("Field", G + "Field\\Room.c", "Field", G + "Field\\RoomState.c"),
    ("Field::Gimmick", None, "Field", G + "Field\\Gimmicks.c"),
    ("Items::Inventory", None, "Items::Inventory", G + "Items\\Inventory.c"),
    # Menu: one-screen folders become one file named after the sub-namespace
    ("Menu::BattleAbilities", None, "Menu::BattleAbilities", G + "Menu\\BattleAbilities.c"),
    ("Menu::BreakArts", None, "Menu::BreakArts", G + "Menu\\BreakArts.c"),
    ("Menu::Files", None, "Menu::Files", G + "Menu\\Files.c"),
    ("Menu::Magic", None, "Menu::Magic", G + "Menu\\Magic.c"),
    ("Menu::Manual", None, "Menu::Manual", G + "Menu\\Manual.c"),
    ("Menu::Map", None, "Menu::Map", G + "Menu\\Map.c"),
    ("Menu::Options", None, "Menu::Options", G + "Menu\\Options.c"),
    ("Menu::Rename", None, "Menu::Rename", G + "Menu\\Rename.c"),
    # Menu framework: shared menu machinery gets its own folder
    ("Menu", G + "Menu\\Framework.c", "Menu::Framework", G + "Menu\\Framework\\Framework.c"),
    ("Menu::Framework", G + "Menu\\Framework.c", "Menu::Framework", G + "Menu\\Framework\\Framework.c"),
    ("Menu::Framework", G + "Menu\\EquipmentDetails.c", "Menu::Framework", G + "Menu\\Framework\\EquipmentDetails.c"),
    ("Menu::Framework", G + "Menu\\ItemRows.c", "Menu::Framework", G + "Menu\\Framework\\ItemRows.c"),
    ("Menu::Framework", G + "Menu\\ActorPreview.c", "Menu::Framework", G + "Menu\\Framework\\ActorPreview.c"),
]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()

    files = {p: read_csv(p) for p in sorted(OUTS.glob("*.csv"))}
    moves = collections.Counter()
    renamed: dict[str, str] = {}
    for rows in files.values():
        for r in rows:
            parts = r["final_name"].strip().split("::")
            ns, leaf, loc = "::".join(parts[:-1]), parts[-1], r["final_location"].strip()
            for rns, rfile, nns, nloc in RULES:
                if ns == rns and (rfile is None or loc == rfile):
                    new_loc = nloc(leaf) if callable(nloc) else nloc
                    new_name = f"{nns}::{leaf}"
                    if (new_name, new_loc) != (r["final_name"].strip(), loc):
                        moves[(f"{ns} @ {loc}", f"{nns} @ {new_loc}")] += 1
                        renamed[r["final_name"].strip()] = new_name
                        r["final_name"], r["final_location"] = new_name, new_loc
                    break

    # dup-of rows must follow their target's new name
    by_id = {r["id"]: r for rows in files.values() for r in rows}
    for r in by_id.values():
        t = dup_target(r.get("notes", ""))
        if t and t in by_id:
            r["final_name"], r["final_location"] = by_id[t]["final_name"], by_id[t]["final_location"]

    names = collections.defaultdict(list)
    for r in by_id.values():
        if not dup_target(r.get("notes", "")):
            names[c_spelling(r["final_name"])].append(r["id"])
    coll = {n: ids for n, ids in names.items() if len(ids) > 1}

    for (src, dst), n in sorted(moves.items()):
        print(f"{n:4}  {src}  ->  {dst}")
    print(f"{sum(moves.values())} rows moved; {len(coll)} collisions")
    for n, ids in coll.items():
        print("  COLLISION", n, ids)
    if coll:
        return 1
    if a.write:
        for p, rows in files.items():
            with p.open("w", newline="", encoding="utf-8") as fh:
                w = csv.DictWriter(fh, fieldnames=HEADER, extrasaction="ignore")
                w.writeheader()
                w.writerows(rows)
        print("written")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
