"""F01813 func_800E5A9C (7CD68.c:689) actor AI settings decode vs ApplyActorAiSettings."""

from __future__ import annotations

import random

from differential import DEFAULT_STACK, Fault, Image, Machine, s32

ID = "F01813"
ENTRY, END = 0x800E5A9C, 0x800E5EC0
DRIVER = "actor_settings_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\ActorSettings.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 8192, 1813

RCOS = 0x80040F24
ALLOCATE_ORBIT = 0x800E7660
RESOLVE_LINKS = 0x800E5998
BUILD_METADATA = 0x800E5710
ORBIT_CALLBACK = 0x800E7F8C
DECISION_CALLBACKS = 0x800F1790
DECISION_COUNT = 9
ROOM_AUXILIARY = 0x800F58C0
ROOM_WAYPOINTS = 0x800F58C8
STATE, STATE_SIZE = 0x80600000, 0x180
ACTOR, ACTOR_SIZE = 0x80500000, 0x40
SETTINGS = 0x80700000
AUXILIARY = 0x80710000
WAYPOINTS = AUXILIARY + 0x40
# Uncapped near range, radius threshold 0x80, no 0xC0 terrain guard, wander preset shift 5,
# three orbiting sub-types, waypoint bit 14, no angle halving.
MUTANTS = {
    "near-uncapped": {0x800E5AE8: 0x00000000},
    "radius-threshold-0x80": {0x800E5C1C: 0x2C420080},
    "terrain-guard-missing": {0x800E5D7C: 0x00000000},
    "wander-shift-5": {0x800E5CDC: 0x00031942},
    "orbit-sub-types-3": {0x800E5E48: 0x2C420003},
    "waypoint-bit-14": {0x800E5BA8: 0x00021382},
    "angle-not-halved": {0x800E5B1C: 0x00000000},
}
# (driver output index, PS1 state offset, size) for every decoded field the driver prints.
FIELDS = [
    (0, 0xAC, 4), (1, 0xA4, 4), (2, 0x128, 2), (4, 0x25, 1), (5, 0x29, 1), (6, 0x24, 1), (7, 0x2D, 1),
    (8, 0xB2, 1), (9, 0xB0, 2), (10, 0xBA, 2), (11, 0x21, 1), (12, 0x22, 1), (13, 0x2C, 1), (14, 0x2F, 1),
    (15, 0x31, 1), (16, 0x20, 1), (17, 0xD6, 2), (18, 0xD4, 2), (19, 0xD8, 1), (20, 0x27, 1), (21, 0x30, 1),
    (23, 0x33, 1), (24, 0x26, 1), (25, 0x2B, 1), (26, 0x102, 1), (27, 0x101, 1), (28, 0xC8, 4), (29, 0xCC, 1),
    (30, 0xCE, 1),
]


def vector(rng: random.Random, number: int) -> list:
    settings, group, flags = rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32)
    if number < 34:  # Each single settings bit, then none and all.
        settings = 1 << number if number < 32 else 0 if number == 32 else 0xFFFFFFFF
    index = rng.randrange(DECISION_COUNT) if rng.random() < 0.85 else rng.randrange(DECISION_COUNT, 32)
    group = (group & ~0x1F0) | index << 4
    sub_type = rng.choice([0xAB, 0xAC, 0xAD, 0xAE, 0, 0xFFFF]) if rng.random() < 0.3 else rng.getrandbits(16)
    radius = rng.choice([0, 0x80, 0x81, 0xFFFF]) if rng.random() < 0.4 else rng.getrandbits(16)
    movement = rng.choice([0, 0x40, 0x80, 0xC0, 0xFF3F]) if rng.random() < 0.5 else rng.getrandbits(16)
    roll = rng.random()
    waypoints = None
    if roll >= 0.3:
        waypoints = [rng.getrandbits(15) for _ in range(8)]
        if roll < 0.75:
            for slot in rng.sample(range(8), rng.randrange(1, 4)):
                waypoints[slot] |= 0x8000
    state = bytearray(rng.randbytes(STATE_SIZE))
    return [settings, group, flags, sub_type, radius, movement, waypoints, state]


def run(machine: Machine, vector: list) -> tuple:
    settings, group, flags, sub_type, radius, movement, waypoints, state = vector

    def rcos(m: Machine) -> None:
        m.trace.append(("rcos", s32(m.r[4])))
        m.r[2] = m.r[4]  # Echo the argument; the native cosine is compared separately.

    machine.stub(RCOS, rcos)
    machine.stub(ALLOCATE_ORBIT, lambda m: m.trace.append(("orbit", m.r[4])))
    machine.stub(RESOLVE_LINKS, lambda m: m.trace.append(("links",)))
    machine.stub(BUILD_METADATA, lambda m: m.trace.append(("metadata", m.r[4])))
    machine.store(STATE, bytes(state))
    machine.put(STATE + 0x54, ACTOR)
    machine.put(STATE + 0x94, radius, 2)
    machine.put(STATE + 0x15E, movement, 2)
    machine.store(ACTOR, random.Random(settings ^ group).randbytes(ACTOR_SIZE))
    machine.put(ACTOR + 0x1A, sub_type, 2)
    machine.put(ACTOR + 0x30, group)
    machine.put(ACTOR + 0x34, flags)
    machine.put(SETTINGS, settings)
    machine.put(ROOM_AUXILIARY, AUXILIARY if waypoints is not None else 0)
    machine.put(ROOM_WAYPOINTS, WAYPOINTS)
    for slot, waypoint in enumerate(waypoints or []):
        machine.put(WAYPOINTS + slot * 2, waypoint, 2)
    before = machine.bytes(STATE, STATE_SIZE)
    machine.r[4], machine.r[5] = STATE, SETTINGS
    machine.execute(ENTRY)
    for address, size in machine.stores:
        if not (DEFAULT_STACK - 0x100 <= address < DEFAULT_STACK
                or address >= STATE and address + size <= STATE + STATE_SIZE):
            raise Fault(f"store outside the state and stack frame {address:#x}")
    trace = tuple(machine.trace)
    rcos_argument = trace[0][1] if trace else 0
    inputs = [settings, group, flags, sub_type, radius, movement, int(waypoints is not None),
              *(waypoints or [0] * 8), rcos_argument]
    return (before, machine.bytes(STATE, STATE_SIZE), trace), inputs


def expected_image(image: Image, before: bytes, fields: list[int], rcos_argument: int) -> bytes:
    result = bytearray(before)

    def put(offset: int, value: int, size: int) -> None:
        result[offset:offset + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")

    for index, offset, size in FIELDS:
        put(offset, fields[index], size)
    put(0xA8, rcos_argument, 4)  # The echoed rcos result.
    if fields[22] >= 0:
        put(0x32, fields[22], 1)
    put(0x23, 1, 1)
    put(0x16, 0x10, 1)
    if fields[31]:
        put(0x68, ORBIT_CALLBACK, 4)
        put(0x19, 1, 1)
    else:
        put(0x68, image.read(DECISION_CALLBACKS + 4 * fields[33]), 4)
    return bytes(result)


def check(image: Image, vector: list, observation: tuple, native: list[int], reference: tuple) -> list[str]:
    _, after, trace = observation
    before, _, reference_trace = reference
    rcos_argument = reference_trace[0][1]
    want = expected_image(image, before, native, rcos_argument)
    calls = [("rcos", rcos_argument)] + ([("orbit", STATE)] if native[31] else []) + \
        [("links",), ("metadata", STATE)]
    problems = []
    if after != want:
        problems.append("image " + ",".join(hex(i) for i in range(STATE_SIZE) if after[i] != want[i]))
    if list(trace) != calls:
        problems.append(f"calls {list(trace)} != {calls}")
    if native[3] != native[34]:
        problems.append(f"cosine {native[3]} != Cosine({rcos_argument}) {native[34]}")
    if native[32] != (native[33] if native[33] < DECISION_COUNT else -1) or native[33] != vector[1] >> 4 & 31:
        problems.append(f"decision slot {native[32]} index {native[33]}")
    return problems


def describe(image: Image, results: list) -> list[str]:
    counts = dict.fromkeys(("orbit", "no-room", "waypoint-kept", "waypoint-cleared", "terrain-kept",
                            "near-capped", "radius-large", "out-of-table"), 0)
    for vector, _, fields in results:
        waypoints = vector[6]
        counts["orbit"] += fields[31]
        counts["no-room"] += waypoints is None
        if waypoints is not None:
            counts["waypoint-kept" if any(w & 0x8000 for w in waypoints) else "waypoint-cleared"] += 1
        counts["terrain-kept"] += fields[22] < 0
        counts["near-capped"] += fields[1] != fields[0]
        counts["radius-large"] += vector[4] > 0x80
        counts["out-of-table"] += not fields[31] and fields[33] >= DECISION_COUNT
    return [
        f"{counts['orbit']} orbiting sub-types, {counts['no-room']} rooms without waypoints, "
        f"{counts['waypoint-kept']} with an active waypoint, {counts['waypoint-cleared']} with none active, "
        f"{counts['terrain-kept']} kept terrain masks (0xC0), {counts['near-capped']} capped near ranges, "
        f"{counts['radius-large']} radii over 0x80, {counts['out-of-table']} decision indexes past the table "
        "(#272: native stores no callback)",
        "Compared the full 0x180-byte state image against the decoded fields at PS1 offsets, the call order "
        "and arguments, the native cosine of the original rcos argument and the decision slot.",
    ]
