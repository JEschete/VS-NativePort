"""F01818 func_800E6178 (7CD68.c:934) actor AI state creation vs CreateActorAiState."""

from __future__ import annotations

import random

from differential import DEFAULT_STACK, MASK, Fault, Image, Machine, s16, s32

ID = "F01818"
ENTRY, END = 0x800E6178, 0x800E65DC
# The square root (0x80041B94) is in the executable.
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "actor_creation_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\ActorCreation.cpp",
    r"Native\Game\src\Battle\Ai\ActorSettings.cpp",
    r"Native\Game\src\Battle\Ai\FrameUpdate.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1818

# Host boundaries, stubbed and traced.
ALLOCATE = 0x80043EC4
ANCHOR = 0x800A190C
BONE_TAG = 0x800A152C
SETTINGS = 0x800E5A9C
RELATIONS = 0x800E7370
COMMAND = 0x800DEEFC
SAVED_RELATIONS = 0x800E72D0
REBUILD = 0x800DEC88
# Leaf routines the native core reimplements, run as original instructions.
CLEAR = 0x800E4288
LOCOMOTION = 0x800E6828
SAVED_SETTINGS = 0x800E5A74
SQUARE_ROOT = 0x80041B94

STATE_TABLE = 0x800F5878
TILE_GRID = 0x800F58B8
ROOM_STATE = 0x800F58BC
ATTRIBUTE_MASK = 0x800F590C
ACTOR, ACTOR_SIZE = 0x80500000, 0x48
MODEL, MODEL_SIZE = 0x80510000, 0x640
STATS = 0x80520000
ROOM = 0x80530000
GRID, GRID_SIDE = 0x80540000, 32
OCCUPANT = 0x80580000
STATE, STATE_SIZE = 0x80600000, 0x484
ACTOR_SLOTS = 16

MUTANTS = {
    "attribute-kinds-0x05": {0x800E61A0: 0x30420005},
    "special-object-3": {0x800E61F0: 0x24020003},
    "special-slots-3": {0x800E6220: 0x28820003},
    "radius-cap-0x40": {0x800E62C4: 0x2C620040},
    "large-body-0x80": {0x800E62E4: 0x2C620080},
    "fixed-walk-bit-2": {0x800E6374: 0x30420004},
    "reach-margin-0x20": {0x800E6414: 0x2442FFE0},
    "hover-flags-0x46": {0x800E64A4: 0x30620046},
    "hover-tile-flag-0x20": {0x800E64D4: 0x30420020},
    "motion-lock-bit-22": {0x800E64DC: 0x3C030040},
    "idle-bit-4": {0x800E6518: 0x000216C0},
    "command-flag-bit-24-kept": {0x800E6550: 0x3C03FDFF},
    "single-rebuild-pass": {0x800E659C: 0x00000000},
}

# The driver prints CreatedAiState in this order: (PS1 state offset, size).
FIELDS = [
    (0x2A, 1), (0x34, 4), (0x38, 4), (0x42, 2), (0x82, 2), (0x88, 1), (0x89, 1), (0x8A, 1), (0x92, 2),
    (0x94, 2), (0x9C, 4), (0xAC, 4), (0xB3, 1), (0xB4, 4), (0xB8, 2), (0xBC, 2), (0xBE, 2), (0x103, 1),
    (0x110, 4), (0x124, 1), (0x125, 1), (0x12D, 1), (0x13C, 2), (0x15E, 2), (0x170, 4), (0x184, 4),
    (0x194, 4), (0x224, 2), (0x226, 1), (0x227, 1),
]
# AllocateAiState links the state to the actor, model, stats and the model's position and tile.
LINKS = [(0x54, ACTOR), (0x58, MODEL), (0x5C, STATS), (0x60, MODEL + 0x1C), (0x64, MODEL + 0x5C)]
CALL_WIDTH = 3 + len(FIELDS)  # code, two arguments, the state fields
# Driver call codes.
C_ALLOCATE, C_ANCHOR, C_BONE, C_SETTINGS, C_RELATIONS, C_COMMAND, C_CLEAR, C_SAVED, C_REBUILD = range(1, 10)
STATE_CALLS = {C_SETTINGS: "settings", C_RELATIONS: "relations", C_COMMAND: "command", C_SAVED: "saved",
               C_REBUILD: "rebuild"}
REFUSED = -2


def vector(rng: random.Random, number: int) -> dict:
    actor_id = 0 if rng.random() < 0.15 else rng.randrange(1, ACTOR_SLOTS)
    kind = rng.choice([0, 1, 4, 5, 0x10, 0x11, 0x14, 0x15]) if rng.random() < 0.4 else rng.getrandbits(16)
    allocation = 0 if rng.random() < 0.5 else rng.choice([1, 2, -1, s32(rng.getrandbits(32))])
    object_id = rng.choice([2, 4, 2, 4, 0, 3, 6, rng.randrange(8)])
    occupied = [rng.random() < 0.6 for _ in range(4)]
    if number < 4:  # Every slot but `number` taken, so each slot is chosen once.
        allocation, kind, object_id = 0, 0x15, 4
        occupied = [slot != number for slot in range(4)]
    radius = (rng.choice([0, 0x28, 0x3E, 0x3F, 0x40, 0x80, 0x81, 0xFFFF]) if rng.random() < 0.5
              else rng.getrandbits(16))
    tile = rng.randrange(GRID_SIDE) | rng.getrandbits(8) << 8 | rng.randrange(GRID_SIDE) << 16
    tile |= rng.getrandbits(8) << 24
    far_roll = rng.random()
    far = (rng.randrange(0x1000) if far_roll < 0.2 else rng.getrandbits(30) if far_roll < 0.8
           else rng.getrandbits(32))
    saved = (rng.choice([-1, -0x8000, 0, 0x20, 0x7FFF, -0x21]) if rng.random() < 0.3
             else s16(rng.getrandbits(16)))
    return {
        "id": actor_id, "kind": kind, "pose": rng.choice([0, 0, rng.randrange(1, 256)]),
        "flags34": rng.getrandbits(32), "settings": rng.getrandbits(32), "allocation": allocation,
        "object": rng.getrandbits(29) << 3 | object_id, "model_word8": rng.getrandbits(32),
        "position": [rng.getrandbits(16) for _ in range(3)], "tile": tile, "radius": radius,
        "body_height": rng.getrandbits(16), "stats": [rng.getrandbits(8) for _ in range(4)],
        "anchor": [rng.getrandbits(16) for _ in range(3)],
        "bone": rng.choice([-1, -2, 0, 1, 7, 0x7FFFFFFF, -0x80000000]),
        "mask": rng.getrandbits(32), "occupied": occupied, "grid": rng.randbytes(GRID_SIDE * GRID_SIDE * 2),
        "far": far, "perturb_flags": rng.getrandbits(16) if rng.random() < 0.25 else -1,
        "perturb_move": rng.randrange(0x8000) if rng.random() < 0.25 else -1,
        "rebuilds": rng.choice([0, 0, 1, 2, 3]), "saved": saved,
        "actor": rng.randbytes(ACTOR_SIZE), "garbage": rng.randbytes(STATE_SIZE),
    }


def tile_flag(v: dict) -> int:
    cell = (v["tile"] >> 16 & 0xFF) * GRID_SIDE + (v["tile"] & 0xFF)
    return int.from_bytes(v["grid"][cell * 2:cell * 2 + 2], "little")


def actor_image(v: dict) -> bytes:
    actor = bytearray(v["actor"])
    for offset, value, size in ((4, v["id"], 4), (0x1C, v["kind"], 2), (0x1F, v["pose"], 1),
                                (0x34, v["flags34"], 4), (0x38, v["settings"], 4), (0x3C, STATS, 4),
                                (0x40, v["allocation"], 4), (0x44, MODEL, 4)):
        actor[offset:offset + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")
    return bytes(actor)


def run(machine: Machine, v: dict) -> tuple:
    rebuild_calls = []

    def traced(name: str, *args: int) -> None:
        machine.trace.append((name, *args, machine.bytes(STATE, STATE_SIZE), machine.load(ACTOR + 8)))

    def allocate(m: Machine) -> None:
        m.trace.append(("allocate", m.r[4]))
        m.r[2] = STATE

    def anchor(m: Machine) -> None:
        m.trace.append(("anchor", m.r[4], m.r[5], m.r[7]))
        for index, value in enumerate(v["anchor"]):
            m.put(m.r[6] + 2 * index, value, 2)

    def bone(m: Machine) -> None:
        m.trace.append(("bone", m.r[4], m.r[5], m.r[6]))
        m.r[2] = v["bone"] & MASK

    def settings(m: Machine) -> None:
        traced("settings", m.r[4], m.load(m.r[5]))
        m.put(STATE + 0xAC, v["far"])
        if v["perturb_flags"] >= 0:
            m.put(STATE + 0x15E, v["perturb_flags"], 2)

    def relations(m: Machine) -> None:
        traced("relations", m.r[4])
        if v["perturb_move"] >= 0:
            m.put(STATE + 0xB4, v["perturb_move"])

    def rebuild(m: Machine) -> None:
        traced("rebuild", m.r[4])
        rebuild_calls.append(1)
        m.r[2] = int(len(rebuild_calls) <= v["rebuilds"])

    machine.stub(ALLOCATE, allocate)
    machine.stub(ANCHOR, anchor)
    machine.stub(BONE_TAG, bone)
    machine.stub(SETTINGS, settings)
    machine.stub(RELATIONS, relations)
    machine.stub(COMMAND, lambda m: traced("command", m.r[4], m.r[5]))
    machine.stub(SAVED_RELATIONS, lambda m: traced("saved"))
    machine.stub(REBUILD, rebuild)
    machine.follow.update((CLEAR, LOCOMOTION, SAVED_SETTINGS, SQUARE_ROOT))

    machine.store(ACTOR, actor_image(v))
    machine.map_zero(MODEL, MODEL_SIZE)
    machine.put(MODEL + 8, v["model_word8"])
    for index, value in enumerate(v["position"]):
        machine.put(MODEL + 0x1C + 2 * index, value, 2)
    machine.put(MODEL + 0x5C, v["tile"])
    machine.put(MODEL + 0x16C, v["object"])
    machine.put(MODEL + 0x63C, v["radius"], 2)
    machine.put(MODEL + 0x63E, v["body_height"], 2)
    machine.map_zero(STATS, 0x40)
    machine.store(STATS + 0x30, bytes(v["stats"]))
    machine.put(ROOM_STATE, ROOM)
    machine.map_zero(ROOM, 0x40)
    for slot, taken in enumerate(v["occupied"]):
        machine.put(ROOM + 0x28 + 4 * slot, OCCUPANT + 0x100 * slot if taken else 0)
    machine.put(TILE_GRID, GRID)
    machine.store(GRID, v["grid"])
    machine.put(ATTRIBUTE_MASK, v["mask"])
    for slot in range(ACTOR_SLOTS):
        machine.put(STATE_TABLE + 4 * slot, 0)
    machine.store(STATE, v["garbage"])  # The heap does not zero; the clear routine does.

    machine.r[4], machine.r[5] = ACTOR, v["saved"] & MASK
    machine.execute(ENTRY)
    allowed = [(DEFAULT_STACK - 0x100, DEFAULT_STACK), (STATE, STATE + STATE_SIZE), (ACTOR + 8, ACTOR + 12),
               (ATTRIBUTE_MASK, ATTRIBUTE_MASK + 4), (ROOM + 0x28, ROOM + 0x38),
               (STATE_TABLE, STATE_TABLE + 4 * ACTOR_SLOTS)]
    for address, size in machine.stores:
        if not any(start <= address and address + size <= end for start, end in allowed):
            raise Fault(f"store outside the creation's records {address:#x}")
    observation = (
        machine.r[2], machine.load(ATTRIBUTE_MASK), [machine.load(ROOM + 0x28 + 4 * i) for i in range(4)],
        [machine.load(STATE_TABLE + 4 * i) for i in range(ACTOR_SLOTS)], machine.bytes(ACTOR, ACTOR_SIZE),
        machine.bytes(STATE, STATE_SIZE), tuple(machine.trace),
    )
    inputs = [
        v["id"], v["kind"], v["pose"], v["flags34"] & 0xFFFF, v["settings"], v["allocation"], v["object"],
        v["model_word8"], s16(v["position"][1]), v["tile"], v["radius"], v["body_height"], *v["stats"],
        s16(v["anchor"][1]), v["bone"], v["mask"], *map(int, v["occupied"]), tile_flag(v), v["far"],
        v["perturb_flags"], v["perturb_move"], v["rebuilds"], v["saved"],
    ]
    return observation, inputs


def state_image(fields: list[int]) -> bytes:
    result = bytearray(STATE_SIZE)
    for (offset, size), value in zip([(offset, 4) for offset, _ in LINKS] + FIELDS,
                                     [address for _, address in LINKS] + fields, strict=True):
        result[offset:offset + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")
    return bytes(result)


def cleared(word: int) -> int:
    return word & 0xF0FF00FF  # Opcode byte 9 and the low four bits of flags byte 11.


def check(image: Image, v: dict, observation: tuple, native: list[int], reference: tuple) -> list[str]:
    result, mask, slots, table, actor, state, trace = observation
    native_result, stored_mask, slot, count = native[:4]
    calls = [native[4 + CALL_WIDTH * i:4 + CALL_WIDTH * (i + 1)] for i in range(count)]
    if native_result == REFUSED:
        return ["native refused a vector the original accepts"]
    problems = []
    if result != native_result:
        problems.append(f"result {result} != {native_result}")
    if mask != (stored_mask if stored_mask >= 0 else v["mask"]):
        problems.append(f"attribute mask {mask:#x} != {stored_mask:#x}")
    want_slots = [OCCUPANT + 0x100 * i if taken else 0 for i, taken in enumerate(v["occupied"])]
    if slot >= 0:
        want_slots[slot] = ACTOR
    if slots != want_slots or slot < -1:
        problems.append(f"special slots {[hex(s) for s in slots]} (native slot {slot})")
    word8 = int.from_bytes(actor_image(v)[8:12], "little")
    expected_trace, snapshots, allocated, last_state = [], [], False, None
    for code, first, second, *fields in calls:
        if code == C_ALLOCATE:
            allocated = True
            expected_trace.append(("allocate", STATE_SIZE))
            if (first, second) != (v["id"], 1):
                problems.append(f"allocated slot {first} for actor match {second}")
        elif code == C_ANCHOR:
            expected_trace.append(("anchor", first & 0xFF, 0xFA, 0))
        elif code == C_BONE:
            expected_trace.append(("bone", first, second, 3))
        elif code == C_CLEAR:
            word8 = cleared(word8) if first == 1 else -1
        else:
            name = STATE_CALLS[code]
            args = {"settings": (STATE, second), "relations": (STATE,), "command": (STATE, second),
                    "saved": (), "rebuild": (0,)}[name]
            expected_trace.append((name, *(arg & MASK for arg in args)))
            last_state = state_image(fields)
            snapshots.append((name, last_state, word8))
    heads = [entry if entry[0] in ("allocate", "anchor", "bone") else entry[:-2] for entry in trace]
    if heads != expected_trace:
        problems.append(f"calls {heads} != {expected_trace}")
    else:
        for entry, (name, want, want_word8) in zip((e for e in trace if e[0] in STATE_CALLS.values()),
                                                   snapshots, strict=True):
            if entry[-2] != want:
                problems.append(f"state at {name} differs at " + ",".join(
                    hex(i) for i in range(STATE_SIZE) if entry[-2][i] != want[i]))
            if entry[-1] != want_word8:
                problems.append(f"actor word 8 at {name} {entry[-1]:#x} != {want_word8:#x}")
    want_state = last_state if last_state is not None else v["garbage"]
    if state != want_state:
        problems.append("final state differs at " + ",".join(
            hex(i) for i in range(STATE_SIZE) if state[i] != want_state[i]))
    want_table = [STATE if allocated and i == v["id"] else 0 for i in range(ACTOR_SLOTS)]
    if table != want_table:
        problems.append("AI state table " + ",".join(hex(t) for t in table))
    want_actor = bytearray(actor_image(v))
    want_actor[8:12] = (word8 & MASK).to_bytes(4, "little")
    if actor != bytes(want_actor):
        problems.append(f"actor bytes {actor.hex()} != {bytes(want_actor).hex()}")
    return problems


def move_distance(state: bytes) -> int:
    return int.from_bytes(state[0xB4:0xB8], "little")


def describe(image: Image, results: list) -> list[str]:
    counts = dict.fromkeys(("created", "ashley", "special", "sqrt", "hover", "stop", "idle", "rebuild"), 0)
    for v, observation, native in results:
        trace = observation[6]
        names = [entry[0] for entry in trace]
        commands = [entry[2] for entry in trace if entry[0] == "command"]
        relations = next((entry for entry in trace if entry[0] == "relations"), None)
        if relations is not None:
            counts["created"] += 1
            counts["ashley"] += v["id"] == 0
            reach = v["perturb_move"] if v["perturb_move"] >= 0 else move_distance(relations[-2])
            counts["sqrt"] += move_distance(observation[5]) != reach
        counts["special"] += native[2] >= 0
        counts["hover"] += 12 in commands
        counts["stop"] += 1 in commands
        counts["idle"] += observation[0] == 1
        counts["rebuild"] += names.count("rebuild") > 1
    return [
        f"{counts['created']} states created ({counts['ashley']} Ashley), {counts['special']} special actor "
        f"registrations, {counts['sqrt']} reaches clamped by the far range, {counts['hover']} hover starts, "
        f"{counts['stop']} stop starts, {counts['idle']} remembered idle, {counts['rebuild']} multi-pass "
        "rebuilds",
    ]
