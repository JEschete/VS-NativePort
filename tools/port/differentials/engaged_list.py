"""F01678, F01679, F01680 (73700.c:414, 463, 499) engaged list vs Engagement.cpp.

func_800DBF00 removes an actor from the room's engaged list, func_800DBFE4 advances its buildup and
func_800DC0D0 steps the engaged actors' phases. The buildup's leave runs the original
DisengageActor (func_800D82CC), which unlinks through func_800DBF00, and the awareness marker
setter (func_800D820C); only the marker display (func_800D820C's tail jump to func_80095B7C) and
the remembered target (func_800E4C8C) are stubs that record their arguments. The
native driver answers the host's unlink with the typed UnlinkEngagedActor over the same list.

The room's AI state, sixteen AI states and their battle-actor command flags hold random bytes
around the fields; the engaged list links a random subset of the states through their successor
pointers, and the states outside it keep stale successors. Vectors keep the list free of cycles
(the original walks would not end), the actor IDs within the AI state table, the battle actor list
non-empty and the engaged count positive for an actor in the engaged mask, where the native
refuses what the original would wrap or read through null.
"""

from __future__ import annotations

import random

from differential import Image, Machine, field_writer, output_problems, s32

ID = "F01678, F01679, F01680"
ENTRY, END = 0x800DBF00, 0x800DC19C
DRIVER = "engaged_list_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\Ai\Engagement.cpp",)
VECTORS, SEED = 4096, 1678
MUTANT_VECTORS = 1024

UNLINK, BUILDUP, ANIMATE = 0x800DBF00, 0x800DBFE4, 0x800DC0D0
K_UNLINK, K_BUILDUP, K_ANIMATE = range(3)
ENTRIES = {K_UNLINK: UNLINK, K_BUILDUP: BUILDUP, K_ANIMATE: ANIMATE}
MARKER = 0x800D820C
SHOW_MARKER = 0x80095B7C
DISENGAGE = 0x800D82CC
REMEMBER = 0x800E4C8C
ROOM_POINTER = 0x800F58BC
BATTLE_LIST_POINTER = 0x800F1928

DATA = 0x80500000
ROOM, ROOM_SIZE = DATA, 0x40
BATTLE_LIST, FIRST_ACTOR, SECOND_ACTOR = DATA + 0x40, DATA + 0x50, DATA + 0x60
MODELS, MODEL_SIZE = DATA + 0x100, 0x10
STATES, STATE_SIZE = DATA + 0x1000, 0x200
DATA_SIZE = 0x1000 + 16 * STATE_SIZE
ACTORS = 16

# Room AI state fields: name, offset, size.
ROOM_MASK, ROOM_COUNT, ROOM_HEAD, ROOM_TAIL = (0x10, 2), (0x12, 1), (0x1C, 4), (0x20, 4)
# AI state fields: name, offset, size, signed (the successor is compared as an actor ID).
STATE_FIELDS = (("engaged", 0x8E, 2, False), ("cooldown", 0x98, 2, False), ("grace", 0x9A, 2, False),
                ("phase", 0x12F, 1, True), ("step", 0x130, 1, True))
MODEL_FIELD, ID_FIELD, NEXT_FIELD, RELATION_FIELD = 0x54, 0x88, 0x1A0, 0x1A4
FLAGS = 8
PER_ACTOR = ("next", *(name for name, *_ in STATE_FIELDS), "flags")
C_MARKER, C_REMEMBER = range(2)
CALL_NAMES = ("marker", "remember")

MUTANTS = {
    # func_800DBF00
    "phase-not-reset": {0x800DBF0C: 0x00000000},
    "leave-pose-rising": {0x800DBF14: 0x3C080400},
    "tail-always-moves": {0x800DBF34: 0x00000000},
    "tail-ignores-successor": {0x800DBF58: 0x00000000},
    "tail-wraps-to-itself": {0x800DBF60: 0x8CA20020},
    "head-not-advanced": {0x800DBF90: 0x00000000},
    "predecessor-cut": {0x800DBF94: 0x00001021},
    "predecessor-not-relinked": {0x800DBF9C: 0x00000000},
    "empty-list-keeps-tail": {0x800DBFD8: 0x00000000},
    # func_800DBFE4
    "buildup-steps-2": {0x800DC000: 0x24420002},
    "wrap-shows-marker": {0x800DC008: 0x00000000},
    "first-step-at-9": {0x800DC010: 0x2C620009},
    "second-step-at-17": {0x800DC01C: 0x2C620011},
    "third-step-at-25": {0x800DC028: 0x2C620019},
    "leaves-at-120": {0x800DC040: 0x2C420078},
    "marker-off-by-one": {0x800DC054: 0x24A50000},
    "buildup-falls-to-grace": {0x800DC058: 0x00000000},
    "target-not-remembered": {0x800DC060: 0x00000000},
    "never-disengages": {0x800DC068: 0x00000000},
    "grace-steps-2": {0x800DC07C: 0x2442FFFE},
    "grace-marker-3": {0x800DC08C: 0x24050003},
    "grace-end-marker-4": {0x800DC0A0: 0x00000000},
    "cooldown-not-stored": {0x800DC0BC: 0x00000000},
    # func_800DC0D0
    "skips-hostile-not-ally": {0x800DC0F0: 0x240C0003},
    "rising-step-2": {0x800DC0F4: 0x240B0002},
    "rising-pose-falling": {0x800DC100: 0x3C0A0800},
    "turn-at-15": {0x800DC104: 0x2409000F},
    "falling-step-2": {0x800DC108: 0x2408FFFE},
    "falling-pose-both": {0x800DC10C: 0x3C070C00},
    "class-from-byte-1": {0x800DC110: 0x908201A5},
    "ally-animated": {0x800DC11C: 0x00000000},
    "walk-continues-after-turn": {0x800DC17C: 0x00000000},
    "phase-not-advanced": {0x800DC180: 0x00000000},
}


def state_address(actor: int) -> int:
    return STATES + actor * STATE_SIZE


def actor_of(pointer: int) -> int:
    """The actor ID a successor, head or tail pointer names: -1 for null, the raw pointer otherwise."""
    if pointer == 0:
        return -1
    offset = pointer - STATES
    if 0 <= offset < ACTORS * STATE_SIZE and offset % STATE_SIZE == 0:
        return offset // STATE_SIZE
    return pointer


def pick(rng: random.Random, edges: tuple[int, ...], bits: int) -> int:
    return rng.choice(edges) if rng.random() < 0.6 else rng.getrandbits(bits)


def vector(rng: random.Random, number: int) -> dict:
    kind = (K_UNLINK, K_BUILDUP, K_ANIMATE)[number % 3]
    size = rng.choice((0, 1, 1, 2, 2, 3, 4, 5, 6, 8, 16))
    members = rng.sample(range(ACTORS), size)
    outside = [actor for actor in range(ACTORS) if actor not in members]
    nexts = {actor: rng.choice((None, None, rng.randrange(ACTORS))) for actor in outside}
    nexts.update({actor: (members[index + 1] if index + 1 < size else None)
                  for index, actor in enumerate(members)})
    roll = rng.random()
    tail = rng.choice(members) if members and roll < 0.7 else (None if roll < 0.85 or not outside
                                                               else rng.choice(outside))
    roll = rng.random()
    if members and roll < 0.25:
        actor = members[0]
    elif members and roll < 0.45 and tail is not None:
        actor = tail
    elif members and roll < 0.8:
        actor = rng.choice(members)
    else:
        actor = rng.randrange(ACTORS)
    if kind != K_UNLINK and rng.random() < 0.1:
        actor = 0
    mask = rng.getrandbits(16)
    if rng.random() < 0.75:
        mask |= 1 << actor
    count = rng.randrange(1, 17) if mask & 1 << actor or rng.random() < 0.8 else 0
    actors = []
    for index in range(ACTORS):
        classification = rng.choice((0, 1, 2, 3)) if rng.random() < 0.8 else 2
        actors.append({
            "next": nexts[index],
            "engaged": pick(rng, (0, 0, 1, 7, 8, 15, 16, 23, 24, 100, 119, 120, 121, 0x200, 0xFFFE, 0xFFFF), 16),
            "cooldown": pick(rng, (0, 0, 1, 2, 0x30, 0xFFFF), 16),
            "grace": pick(rng, (0, 0, 1, 2, 0x5A, 0xFFFF), 16),
            "phase": pick(rng, (0, 0, 16, 16, 1, 15, 17, 0xFF, 0x7F, 0x80), 8),
            "step": pick(rng, (1, 1, 0xFF, 0xFF, 0, 2, 0x80), 8),
            "flags": rng.getrandbits(32),
            "relation": (rng.getrandbits(32) & ~0xC0) | classification << 6,
        })
    return {"kind": kind, "actor": actor, "head": members[0] if members else None, "tail": tail,
            "mask": mask, "count": count, "single": rng.random() < 0.5, "actors": actors,
            "noise": rng.randbytes(DATA_SIZE)}


def pointer(actor: int | None) -> int:
    return 0 if actor is None else state_address(actor)


def data_block(v: dict) -> bytes:
    block = bytearray(v["noise"])
    put = field_writer(block, DATA)
    put(ROOM + ROOM_MASK[0], v["mask"], ROOM_MASK[1])
    put(ROOM + ROOM_COUNT[0], v["count"], ROOM_COUNT[1])
    put(ROOM + ROOM_HEAD[0], pointer(v["head"]), 4)
    put(ROOM + ROOM_TAIL[0], pointer(v["tail"]), 4)
    put(BATTLE_LIST, FIRST_ACTOR, 4)
    put(FIRST_ACTOR, 0 if v["single"] else SECOND_ACTOR, 4)
    for index, actor in enumerate(v["actors"]):
        state = state_address(index)
        model = MODELS + index * MODEL_SIZE
        put(state + MODEL_FIELD, model, 4)
        put(state + ID_FIELD, index, 1)
        put(state + NEXT_FIELD, pointer(actor["next"]), 4)
        put(state + RELATION_FIELD, actor["relation"], 4)
        for name, offset, size, _ in STATE_FIELDS:
            put(state + offset, actor[name], size)
        put(model + FLAGS, actor["flags"], 4)
    return bytes(block)


def observe(machine: Machine) -> list[int]:
    values = []
    for index in range(ACTORS):
        state = state_address(index)
        values.append(actor_of(machine.load(state + NEXT_FIELD)))
        values += [machine.load(state + offset, size, signed) for _, offset, size, signed in STATE_FIELDS]
        values.append(machine.load(MODELS + index * MODEL_SIZE + FLAGS))
    values += [actor_of(machine.load(ROOM + ROOM_HEAD[0])), actor_of(machine.load(ROOM + ROOM_TAIL[0])),
               machine.load(ROOM + ROOM_MASK[0], ROOM_MASK[1]), machine.load(ROOM + ROOM_COUNT[0], ROOM_COUNT[1])]
    return values


def run(machine: Machine, v: dict):
    machine.images.append(Image(data_block(v), DATA))
    machine.put(ROOM_POINTER, ROOM)
    machine.put(BATTLE_LIST_POINTER, BATTLE_LIST)
    machine.follow.update({MARKER, DISENGAGE, UNLINK})
    calls: list[list[int]] = []
    machine.stub(SHOW_MARKER, lambda m: calls.append([C_MARKER, s32(m.r[4]), s32(m.r[5])]), tail=True)
    machine.stub(REMEMBER, lambda m: calls.append([C_REMEMBER, actor_of(m.r[4]), 0]))
    machine.r[4] = state_address(v["actor"])
    machine.execute(ENTRIES[v["kind"]])
    observation = {"values": observe(machine), "calls": calls, "stores": machine.stores}
    inputs = [v["kind"], v["actor"], -1 if v["head"] is None else v["head"],
              -1 if v["tail"] is None else v["tail"], v["mask"], v["count"], int(v["single"])]
    for actor in v["actors"]:
        inputs += [-1 if actor["next"] is None else actor["next"], actor["engaged"], actor["cooldown"],
                   actor["grace"], s8(actor["phase"]), s8(actor["step"]), actor["flags"],
                   (actor["relation"] >> 6) & 3]
    return observation, inputs


def s8(value: int) -> int:
    return value - 0x100 if value & 0x80 else value


def allowed_stores() -> list[tuple[int, int]]:
    spans = [(ROOM + offset, ROOM + offset + size) for offset, size in (ROOM_MASK, ROOM_COUNT, ROOM_HEAD, ROOM_TAIL)]
    for index in range(ACTORS):
        state = state_address(index)
        spans += [(state + offset, state + offset + size) for _, offset, size, _ in STATE_FIELDS]
        spans.append((state + NEXT_FIELD, state + NEXT_FIELD + 4))
        model = MODELS + index * MODEL_SIZE
        spans.append((model + FLAGS, model + FLAGS + 4))
    return [*spans, (0x80400000 - 0x4000, 0x80400000)]


ALLOWED = allowed_stores()
NAMES = [f"{name}[{index}]" for index in range(ACTORS) for name in PER_ACTOR] + ["head", "tail", "mask", "count"]


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    return output_problems(NAMES, native, obs, ALLOWED)


def describe(image: Image, results: list) -> list[str]:
    paths: list[dict[str, int]] = [{}, {}, {}]
    for v, obs, _native in results:
        before = {name: value for name, value in zip(NAMES, [*initial(v)], strict=True)}
        after = dict(zip(NAMES, obs["values"], strict=True))
        actor = v["actor"]
        if v["kind"] == K_UNLINK:
            signature = "head" if v["head"] == actor else "inside" if actor in members(v) else "outside"
            signature += "+tail" if v["tail"] == actor else ""
            signature += "+emptied" if after["head"] == -1 and before["head"] != -1 else ""
        elif v["kind"] == K_BUILDUP:
            engaged = v["actors"][actor]["engaged"]
            signature = ("idle" if engaged == 0 else "wrapped" if engaged == 0xFFFF
                         else "built up" if engaged + 1 <= 120 else "left" if obs["values"][
                             PER_ACTOR.index("engaged") + actor * len(PER_ACTOR)] == 0 else "stayed")
            signature += "+".join([""] + [CALL_NAMES[call[0]] for call in obs["calls"]])
        else:
            turned = [index for index in range(ACTORS)
                      if after[f"flags[{index}]"] != before[f"flags[{index}]"]]
            signature = f"turned {len(turned)}" if members(v) else "empty"
        paths[v["kind"]][signature] = paths[v["kind"]].get(signature, 0) + 1
    lines = []
    for kind, name in ((K_UNLINK, "F01678 unlink"), (K_BUILDUP, "F01679 buildup"), (K_ANIMATE, "F01680 phase")):
        ranked = sorted(paths[kind].items(), key=lambda item: -item[1])
        lines.append(f"{name} paths: " + ", ".join(f"{path} {count}" for path, count in ranked))
    return lines


def members(v: dict) -> list[int]:
    found, node = [], v["head"]
    while node is not None:
        found.append(node)
        node = v["actors"][node]["next"]
    return found


def initial(v: dict) -> list[int]:
    values = []
    for actor in v["actors"]:
        values.append(-1 if actor["next"] is None else actor["next"])
        values += [actor["engaged"], actor["cooldown"], actor["grace"], s8(actor["phase"]), s8(actor["step"]),
                   actor["flags"]]
    return [*values, -1 if v["head"] is None else v["head"], -1 if v["tail"] is None else v["tail"],
            v["mask"], v["count"]]
