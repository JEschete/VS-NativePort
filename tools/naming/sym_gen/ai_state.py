"""Canonical names of the AI actor state (upstream func_800E78F4_t, ~0x484 bytes, T00532) by byte offset, and a
helper that names any fork view of it from the same table, so one offset gets one name in every view.

Names come from the uses gathered by files/ai_offsets.py (pinned views and upstream main). Settings bits are named
after the bit of the AI settings word they are decoded from in ApplyActorAiSettings (7CD68.c), which is what the
code establishes; their gameplay meaning is not.
"""
from __future__ import annotations

import re

L, M, H = "low", "med", "high"
# offset: (name, meaning, confidence)
TABLE: dict[int, tuple[str, str, str]] = {
    0x002: ("isStuck", "Set when the actor has not left its tile", M),
    0x007: ("movementDisabled", "Adjacent-tile search and flight movement are skipped", L),
    0x013: ("usesRoomActionDelay", "When zero the room's default action delay applies, else 50", L),
    0x016: ("linkedActor", "Actor this one follows or guards; 16 for none", M),
    0x017: ("actionCostEnabled", "Zero when actions cost nothing", L),
    0x019: ("settingsConstant19", "Set to 1 when the settings are applied", L),
    0x01B: ("isDead", "Set when the actor's HP is zero", H),
    0x020: ("settingsBit26", "Bit 26 of the AI settings word", L),
    0x021: ("settingsByte3Bit0", "Bit 0 of settings byte 3", L),
    0x022: ("settingsBit23", "Bit 23 of the AI settings word", L),
    0x023: ("settingsConstant23", "Set to 1 when the settings are applied", L),
    0x024: ("settingsBit13", "Bit 13 of the AI settings word", L),
    0x025: ("settingsBit15", "Bit 15 of the AI settings word; also lets out-of-reach targets count", L),
    0x026: ("allowFallbackAction", "Plans a fallback action when no candidate exists", M),
    0x027: ("settingsBit27", "Bit 27 of the AI settings word", L),
    0x029: ("settingsBit14", "Bit 14 of the AI settings word", L),
    0x02A: ("hasTaggedBone3", "Model has a bone tagged 3", M),
    0x02B: ("settingsModeLow", "Low bit of the settings mode field", L),
    0x02C: ("settingsBits21", "Bits 21-22 of the AI settings word; 3 excludes the actor from relations", L),
    0x02F: ("settingsBits28", "Bits 28-29 of the AI settings word", L),
    0x030: ("settingsBit25", "Bit 25 of the AI settings word", L),
    0x031: ("settingsBits30", "Bits 30-31 of the AI settings word", L),
    0x032: ("settingsBits30Value", "Table value selected by settingsBits30", L),
    0x033: ("settingsByte0Bit7", "Bit 7 of settings byte 0", L),
    0x034: ("tile", "Current tile word", H),
    0x038: ("previousTile", "Previous tile word; -1 to forget", H),
    0x042: ("options", "Options passed when the AI state is created", M),
    0x048: ("savedDestination", "Saved destination tile", M),
    0x04C: ("position", "World position", H),
    0x054: ("actor", "Battle actor", M),
    0x058: ("model", "Model record", M),
    0x05C: ("stats", "Actor statistics", H),
    0x060: ("positionSource", "Pointer to the model position", L),
    0x064: ("tileSource", "Pointer to the model tile word", M),
    0x068: ("decisionCallback", "Decision function chosen by the settings", M),
    0x070: ("historyX", "Recent X coordinates", M),
    0x078: ("historyZ", "Recent Z coordinates", M),
    0x080: ("historyIndex", "Next history slot", M),
    0x081: ("historyMask", "Filled history slots", M),
    0x082: ("pointHeight", "Height of the tagged point above the position", L),
    0x088: ("actorId", "Actor id", H),
    0x08C: ("moveAttempts", "Destination attempts; gives up at 6", M),
    0x090: ("statusFlags", "Status flags copied from the statistics", L),
    0x092: ("radius", "Actor radius", H),
    0x094: ("collisionRadius", "Collision radius", H),
    0x0A4: ("nearDistanceSquared", "Squared near range", M),
    0x0A8: ("viewCosine", "Cosine of half the view angle", M),
    0x0AC: ("farDistanceSquared", "Squared perception range", M),
    0x0B3: ("maxRange", "Reach in units of 32", H),
    0x0B4: ("moveDistance", "Distance of the current move", M),
    0x0B8: ("bodyHeight", "Model height used for occlusion and reach", L),
    0x0BA: ("facingDelay", "Frames before re-facing a target", L),
    0x0BC: ("groundClearance", "Ground clearance of flying actors", M),
    0x0BE: ("flightHeight", "Flight height above the ground", M),
    0x0C0: ("wanderPhase", "Wander phase counter", L),
    0x0C2: ("wanderTimer", "Wander timer", M),
    0x0C4: ("wanderRangeSquared", "Squared wander radius", M),
    0x0D0: ("wanderHeading", "Heading of the wander walk", M),
    0x0D2: ("wanderCooldown", "Frames before the next wander walk", M),
    0x0D4: ("restPeriod", "Wander rest period", M),
    0x0D6: ("movePeriod", "Wander move period", M),
    0x0D8: ("wanderRate", "Wander chance exponent", M),
    0x0FA: ("detourStep", "Detour attempts at the current tile", M),
    0x108: ("previousFacingTile", "Tile faced last time", L),
    0x10C: ("facingPending", "A facing change is pending", L),
    0x118: ("detourDirections", "Directions tried by the detour", H),
    0x11C: ("goal", "Wander goal tile", M),
    0x12D: ("behaviorMode", "Behavior mode (1 reacting, 2 wandering)", L),
    0x12E: ("planMode", "Plan mode of the queued action (1 approach, 2 planned tile, 7 retreat)", M),
    0x131: ("actionStatus", "Status of the selected action", L),
    0x134: ("plannedTile", "Tile the plan moves to", H),
    0x13F: ("minActionDelay", "Minimum delay before the next action", M),
    0x140: ("currentChoice", "Action choice being executed", M),
    0x158: ("targetX", "Target X", M),
    0x15A: ("targetZ", "Target Z", M),
    0x15C: ("movementTarget", "Actor the movement heads for; 16 for none", M),
    0x15E: ("movementFlags", "Movement flags (bits 1-2 flight, bit 3 planned)", L),
    0x168: ("destination", "Destination tile", H),
    0x170: ("nextTile", "Next tile on the way", H),
    0x17C: ("moveX", "Target X in 20.12 fixed point", M),
    0x180: ("moveZ", "Target Z in 20.12 fixed point", M),
    0x184: ("moveY", "Target Y in 20.12 fixed point", M),
    0x188: ("heading", "Heading in eighth turns", M),
    0x18A: ("angle", "Facing angle", M),
    0x1A0: ("nextEngaged", "Next actor in the engagement list", M),
    0x1A4: ("relations", "Relation word for each actor", H),
    0x1E4: ("distancesSquared", "Squared distance to each actor", H),
    0x228: ("stallCounter", "Frames without progress", L),
    0x22C: ("currentCandidate", "Candidate being executed", M),
    0x230: ("actionSlots", "Action slot configuration by row and column", M),
    0x2F0: ("candidates", "Action candidates", H),
    0x430: ("candidateOrder", "Candidates in priority order", M),
    0x470: ("candidateCount", "Number of candidates", H),
    0x481: ("reactionPending", "A reaction turn is pending", L),
}
PLACEHOLDER = re.compile(r"^(unk|pad|field|f|flag|flags|b|timer|target|terrain|facing|mode|bits|bytes|raw)[0-9A-Fa-f]*$"
                         r"|^(gap|rest|prefix|padding)\d*$|^unk")


def offsets(g, tid: str) -> list[tuple[int, int]]:
    """(offset, size) of each member of a pinned type, from the decomp index."""
    t = g.types[tid]
    key = t["key"].partition("#")[0]
    ix = g.trees[""].ix
    lay = next(x for x in ix["types"].values() if x["key"] == key and len(x["fields"]) == len(g.members[tid]))
    offs = [f["offset"] // 8 if f["offset"] is not None and f["offset"] >= 0 else -1 for f in lay["fields"]]
    sizes = [(offs[i + 1] if i + 1 < len(offs) else lay["size"]) - offs[i] for i in range(len(offs))]
    return list(zip(offs, sizes))


_REFS: set[str] | None = None


def used(g, tid: str, member: str) -> bool:
    global _REFS
    if _REFS is None:
        _REFS = {k for fn in g.trees[""].ix["functions"].values() for k in (fn.get("refs") or {}).get("field", {})}
    names = {g.types[tid]["name"]}
    names |= {f"{k} {n}" for n in list(names) if n.startswith("(unnamed") for k in ("struct", "union")}
    return any(f"{n}.{member}" in _REFS for n in names)


def camel(name: str) -> str:
    return name[0].lower() + name[1:]


_CANON: dict[int, int] | None = None


def canonical_sizes(g) -> dict[int, int]:
    """Byte size of each upstream func_800E78F4_t member by offset."""
    global _CANON
    if _CANON is None:
        t = next(x for x in g.trees["upstream:"].ix["types"].values() if x["key"] == "struct:func_800E78F4_t")
        offs = [f["offset"] // 8 for f in t["fields"]]
        # untyped char[] fillers upstream say nothing about the member's size
        _CANON = {o: (offs[i + 1] if i + 1 < len(offs) else t["size"]) - o for i, o in enumerate(offs)
                  if not re.fullmatch(r"(signed )?char\[\d+\]", t["fields"][i]["type"])}
    return _CANON


def view_specs(g, tid: str, overrides: dict | None = None) -> list[tuple]:
    """Specs for g.T(): override > canonical table at the member's offset (same size as upstream's member there, or
    no upstream member there) > meaningful fork name > opaque/reserved."""
    overrides = overrides or {}
    canon = canonical_sizes(g)
    out, seen = [], set()
    for m, (off, size) in zip(g.members[tid], offsets(g, tid)):
        name = m["name"]
        filler = PLACEHOLDER.search(name) and re.fullmatch(r"(unsigned |signed )?char\[\d+\]", m["type"])
        fits = off in TABLE and canon.get(off, size) == size and not filler
        if name in overrides:
            spec = (name, *overrides[name])
        elif fits and not re.fullmatch(r"pad\w*", name):
            nm, meaning, conf = TABLE[off]
            spec = (name, nm, meaning, conf)
        elif not PLACEHOLDER.search(name):
            spec = (name, camel(name), "Named in the fork view; see cited use", M)
        elif used(g, tid, name):
            spec = (name, f"opaque{off:X}", "Used, but its meaning is not established", L)
        else:
            spec = (name, f"reserved{off:X}", "Not read through this view", L)
        if spec[1] in seen:
            spec = (spec[0], f"{spec[1]}{off:X}", *spec[2:])
        seen.add(spec[1])
        out.append(spec)
    return out
