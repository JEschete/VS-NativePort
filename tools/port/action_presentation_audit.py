"""Audit the presentation forwarding aliases and their migrated callers."""

import hashlib
import json
import re
import struct
from pathlib import Path

from differential import DISC, ROOT, Image, Machine

ALIASES = {
    "F00110": (0x800719DC, 0x800719FC, 0x80070F28, 0xA698),
    "F00191": (0x80079030, 0x80079050, 0x8007138C, 0x11D50),
}
OLD = ("Battle_CommitEnvironmentAction", "Battle_StartFollowupActionEffects")


def configured_modules():
    root = ROOT / "resources" / "rood-reverseWin" / "config"
    for path in sorted(root.rglob("splat.yaml")):
        configuration = path.read_text()
        target = re.search(r"target_path:\s*data/([^\s]+)", configuration)
        if re.search(r"type:\s*code\b", configuration):
            if target is None:
                raise ValueError(f"code image without target_path: {path}")
            yield target[1]


def images():
    result = []
    for name in configured_modules():
        data = (DISC / Path(name)).read_bytes()
        words = struct.unpack(f"<{len(data)//4}I", data[:len(data)//4*4])
        refs = {}
        for identifier, (start, _, _, _) in ALIASES.items():
            refs[identifier] = {
                "pointers": [i * 4 for i, word in enumerate(words) if word == start],
                "jumps": [i * 4 for i, word in enumerate(words)
                          if word >> 26 in (2, 3) and
                          ((word & 0x3FFFFFF) << 2) == start & 0xFFFFFFF],
            }
        result.append({"name": name, "size": len(data),
                       "sha256": hashlib.sha256(data).hexdigest(), "references": refs})
    assert len(result) == 89, len(result)
    return result


def main():
    scanned = images()
    image = Image.load("BATTLE/BATTLE.PRG")
    controls = {}
    for identifier, (start, end, callee, offset) in ALIASES.items():
        pointers = [(i["name"], p) for i in scanned for p in i["references"][identifier]["pointers"]]
        jumps = [(i["name"], p) for i in scanned for p in i["references"][identifier]["jumps"]]
        assert pointers == [] and jumps == [("BATTLE/BATTLE.PRG", offset)], (pointers, jumps)
        observations = []
        for argument in (0, 6, 255, 0xFFFFFFFF):
            machine = Machine([image])
            calls = []
            machine.r[4] = argument
            machine.stubs[callee] = lambda m, calls=calls, identifier=identifier: calls.append(
                m.r[4] if identifier == "F00110" else "VOID0")
            machine.execute(start)
            assert calls == ([0] if identifier == "F00110" else ["VOID0"]), calls
            observations.append({"input": argument, "calls": calls})
        controls[identifier] = {
            "entry": f"{start:08X}", "end_exclusive": f"{end:08X}",
            "instructions": (end-start)//4, "sha256": image.sha256(start, end),
            "callee": f"{callee:08X}", "absolute_pointers": pointers,
            "sole_jal": jumps, "original_call_controls": observations,
            "disposition": "retired forwarding alias, not C++ promotion or numeric-return parity",
        }
    remaining = []
    for path in (ROOT / "Native").rglob("*"):
        if path.suffix not in (".cpp", ".h", ".hpp"):
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if any(re.search(rf"\b{name}\b", line) for name in OLD):
                remaining.append((str(path.relative_to(ROOT)), number, line))
    assert not remaining, remaining
    targeting = (ROOT / r"Native\Game\src\Battle\TargetingLive.cpp").read_text()
    gameplay = (ROOT / r"Native\Game\src\Boot\GameplayLoopLive.cpp").read_text()
    pattern = r"void CommitEnvironmentAction\(int /\*mode\*/\) override \{.*?CommitLiveSelectedAction\(0\);"
    assert re.search(pattern, targeting, re.S)
    assert not re.search(pattern, targeting.replace("CommitLiveSelectedAction(0);",
                                                   "CommitLiveSelectedAction(mode);"), re.S)
    pattern = r"case BattleActionFlow::kFollowupEffects:\s*VS::Battle::StartLiveActionPresentation\(\);"
    assert re.search(pattern, gameplay)
    assert not re.search(pattern, gameplay.replace("StartLiveActionPresentation();", "StartLiveActionAnimation();"))
    result = {"configured_code_images": len(scanned), "aliases": controls,
              "native_old_symbol_references": remaining,
              "caller_source_controls": "constant0 environment mode and direct followup; wrong-mode/wrong-target replacements rejected",
              "images": scanned}
    output = ROOT / r"build\scratch\action-presentation\alias-audit.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({k: v for k, v in result.items() if k != "images"}, indent=2))


if __name__ == "__main__":
    main()
