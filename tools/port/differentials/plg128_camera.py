"""F02064-F02068 PLG128.BIN effect camera (F8.c:66-249) vs Effects::Plg128 camera cores."""

from __future__ import annotations

import random
from collections import Counter

from differential import DEFAULT_STACK, Fault, Image, Machine, s16, s32

ID = "F02064-F02068"
ENTRY, END = 0x800FD9A4, 0x800FE3D0
MODULES = ("EFFECT/PLG128.BIN", "BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "plg128_camera_driver.cpp"
SOURCES = (
    r"Native\Game\src\Effects\Plugins\Plg128Camera.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Interpolation.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 5120, 2064

BUILD_OFFSET = 0x800FD9A4
RESOLVE_ENDPOINTS = 0x800FDA04
RESOLVE_FACING = 0x800FDC80
STORE_ENDPOINTS = 0x800FDE78
UPDATE = 0x800FDEC4
ENTRIES = (BUILD_OFFSET, RESOLVE_ENDPOINTS, RESOLVE_FACING, STORE_ENDPOINTS, UPDATE)
KIND_NAMES = ("BuildCameraOffsetTransform", "ResolveCameraEndpoints", "ResolveActorFacingCamera",
              "StoreCameraEndpoints", "UpdateEffectCamera")

LOOK_AT = 0x8007AFA8
ALLOCATE = 0x80043E3C
CAPTURE = 0x800D1B18
TRANSITION = 0x800D1E20
SAMPLE_CURVE = 0x800D118C
FOLLOW = (
    0x800D29B0,  # vs_battle_addVecToSvec
    0x800CFCF0, 0x800CFD60, 0x800CFDD0, 0x800CFE1C, 0x800CFB68,  # seven-bit lerps
    0x80040F24, 0x80040F28,  # rcos, rsin
    0x80041578, 0x80041598,  # SetTransMatrix, SetRotMatrix
    RESOLVE_ENDPOINTS, RESOLVE_FACING, STORE_ENDPOINTS, BUILD_OFFSET,
)

CURRENT_INSTANCE = 0x800F53BC
PACKAGE_POINTER = 0x800F569C
EFFECT_ORIGIN = 0x800F5310
MOTION_FRAME = 0x800F54D0
MOTION_DURATION = 0x800F55A0
MOTION_ENDS = 0x800F5520

SLOT = 0x80600000
STATE, STATE_SIZE = 0x80600100, 0xD0
TRANS = 0x80600200
PACKAGE = 0x80610000
BLOCK = 0x80620000
DEFINITION_SIZE = 0xCC
INSTANCE = 0x80640000
TRANSFORM_STRIDE = 0xD0
INSTANCE_SIZE = 0x100 + 256 * TRANSFORM_STRIDE
SCRATCH, SCRATCH_SIZE = 0x1F8001D0, 0x180

# Detected per function: argument swaps, dropped stores, mask and shift changes, a skipped negation.
MUTANTS = {
    "D9A4-look-at-arguments-swapped": {0x800FD9B8: 0x26040088, 0x800FD9BC: 0x26050098},
    "D9A4-translation-y-dropped": {0x800FD9E0: 0x00000000},
    "DA04-position-mask-3": {0x800FDA30: 0x30430003},
    "DA04-transform-x-reads-y": {0x800FDA98: 0x8C42006C},
    "DA04-origin-target-z-dropped": {0x800FDC3C: 0x00000000},
    "DC80-jitter-not-negated": {0x800FDDE4: 0x00000000},
    "DC80-cosine-uses-sine": {0x800FDE24: 0x0C0103CA},
    "DC80-y-from-x": {0x800FDE50: 0x8E020098},
    "DE78-position-x-shift-11": {0x800FDE80: 0x000212C0},
    "DE78-look-at-z-dropped": {0x800FDEC0: 0x00000000},
    "DEC4-sine-not-negated": {0x800FE17C: 0x00000000},
    "DEC4-lifetime-not-decremented": {0x800FE380: 0x24630000},
    "DEC4-transition-skipped": {0x800FDFA4: 0x00000000},
    "DEC4-end-near-not-published": {0x800FE2F8: 0x00000000},
}

POSE = [(0x00, 4, True), (0x04, 4, True), (0x08, 4, True), (0x10, 4, True), (0x14, 4, True),
        (0x18, 4, True), *((offset, 4, True) for offset in range(0x20, 0x40, 4))]


def words(base: int, count: int, size: int = 4, signed: bool = True) -> list[tuple[int, int, bool]]:
    return [(base + index * size, size, signed) for index in range(count)]


# (address, size, signed) of every value the driver prints after its return value.
OUTPUT_FIELDS = [
    (STATE, 1, False), (STATE + 1, 1, False), (STATE + 2, 2, True), (STATE + 4, 4, True),
    *((STATE + 0x08 + offset, size, signed) for offset, size, signed in POSE),
    *((STATE + 0x48 + offset, size, signed) for offset, size, signed in POSE),
    *words(STATE + 0x88, 3), *words(STATE + 0x98, 3), *words(STATE + 0xA8, 9, 2), *words(STATE + 0xBC, 3),
    (STATE + 0xC8, 4, True),
    (SCRATCH + 0x24, 4, False), *words(SCRATCH + 0x2C, 3, 2), *words(SCRATCH + 0x98, 3),
    *words(SCRATCH + 0xA8, 3), *words(SCRATCH + 0xE8, 3), *words(SCRATCH + 0x118, 2),
    *words(SCRATCH + 0x13C, 3, 2), *words(SCRATCH + 0x144, 3, 2), *words(SCRATCH + 0x170, 4),
    (MOTION_FRAME, 2, True), (MOTION_DURATION, 2, True),
    *((MOTION_ENDS + offset, size, signed) for offset, size, signed in POSE),
    *((MOTION_ENDS + 0x40 + offset, size, signed) for offset, size, signed in POSE),
]
STATE_FIELDS = 51  # lifetime through nearClip
GLOBAL_FIELDS = 30  # the motion frame, duration and both published poses
# sinAngle0, sinAngle1, cosAngle0, cosAngle1.
SCRATCH_ANGLE_ORDER = (0x170, 0x174, 0x178, 0x17C)


def moderate(rng: random.Random, bits: int = 26) -> int:
    return rng.randrange(-(1 << bits), 1 << bits)


def word(rng: random.Random) -> int:
    return s32(rng.getrandbits(32))


def flags(rng: random.Random) -> int:
    value = rng.getrandbits(32)
    value = (value & ~0x7) | rng.choice([0, 1, 2, 1, 2, 3, 5, 6, 7])
    value = (value & ~0x1C0) | rng.choice([0, 0x40, 0x80, 0xC0, 0x80, 0xC0, 0x100, 0x1C0])
    return (value & ~0x38) | rng.choice([0, 0x20, 0, 0x20, 0x08, 0x18, 0x38])


def weight(rng: random.Random) -> int:
    return rng.randrange(0, 129) if rng.random() < 0.7 else s16(rng.getrandbits(16))


def vector(rng: random.Random, number: int) -> dict:
    kind = number % 5
    tick = kind == 4
    command = rng.choices([1, 2, 3, 4, 0, 5, rng.getrandbits(32)], [25, 50, 10, 5, 4, 3, 3])[0]
    mode = rng.choice([0, 1, 0, 1, 2, -1, 0x7FFF, s16(rng.getrandbits(16))])
    packed = rng.getrandbits(32)
    if rng.random() < 0.7:
        packed = (packed & 0xFFFF) | ((mode & 0xFFFF) << 16)
    state = bytearray(rng.randbytes(STATE_SIZE))
    state[0] = rng.choice([0, 1, 2, rng.getrandbits(8)])
    state[2:4] = (mode & 0xFFFF).to_bytes(2, "little")
    if rng.random() < 0.1:
        state[4:8] = (0x7FFFFFFF).to_bytes(4, "little")
    bound = 26 if tick else 31
    for offset in range(0x88, 0xA4, 4):  # position, pad, target
        state[offset:offset + 4] = (moderate(rng, bound) & 0xFFFFFFFF).to_bytes(4, "little")
    for offset in (0xBC, 0xC0, 0xC4):
        state[offset:offset + 4] = (moderate(rng, bound) & 0xFFFFFFFF).to_bytes(4, "little")
    scratch = bytearray(rng.randbytes(SCRATCH_SIZE))
    scratch[0x24:0x28] = flags(rng).to_bytes(4, "little")
    for offset in range(0x98, 0xA4, 4):
        scratch[offset:offset + 4] = (moderate(rng, bound) & 0xFFFFFFFF).to_bytes(4, "little")
    definition = bytearray(rng.randbytes(DEFINITION_SIZE))
    definition[4:8] = flags(rng).to_bytes(4, "little")
    transform = rng.getrandbits(8)
    attraction = transform if rng.random() < 0.15 else rng.getrandbits(8)
    definition[0xC8], definition[0xC9] = transform, attraction
    translations = {index: [moderate(rng, 26 if tick else 31) for _ in range(3)]
                    for index in (transform, attraction)}
    return {
        "kind": kind,
        "command": command if tick else 0,
        "packed": packed,
        "state": state,
        "scratch": scratch,
        "index": state[1],
        "definition": definition,
        "translations": translations,
        "facing": s16(rng.getrandbits(16)),
        "origin": [s16(rng.getrandbits(16)) for _ in range(3)],
        "look_at": [s16(rng.getrandbits(16)) for _ in range(9)],
        "trans": [word(rng) for _ in range(3)],
        "captures": [[word(rng) for _ in range(10)] for _ in range(2)],
        "samples": [weight(rng) for _ in range(7)],
        "motion": [s16(rng.getrandbits(16)) for _ in range(2)],
        "ends": rng.randbytes(0x80),
    }


def put_words(machine: Machine, address: int, values: list[int], size: int = 4) -> None:
    for index, value in enumerate(values):
        machine.put(address + index * size, value, size)


def prepare(machine: Machine, value: dict) -> None:
    calls: dict[str, list] = {"look_at": [], "transition": [], "capture": [], "curve": []}
    machine.calls = calls
    samples = list(value["samples"])
    captures = list(value["captures"])

    def look_at(m: Machine) -> None:
        source, target = m.r[4], m.r[5]
        calls["look_at"].append([m.load(source + 4 * i, 4, True) for i in range(3)]
                                + [m.load(target + 4 * i, 4, True) for i in range(3)])
        for index, element in enumerate(value["look_at"]):
            m.write(m.r[6] + 2 * index, element, 2)

    def allocate(m: Machine) -> None:
        if m.r[4] != STATE_SIZE:
            raise Fault(f"allocation of {m.r[4]:#x} bytes")
        m.r[2] = STATE

    def capture(m: Machine) -> None:
        if not captures:
            raise Fault("a third camera capture")
        pose = captures.pop(0)
        calls["capture"].append(m.r[4])
        for offset, element in zip((0, 4, 8, 0x10, 0x14, 0x18, 0x30, 0x34, 0x38, 0x3C), pose, strict=True):
            m.write(m.r[4] + offset, element, 4)

    def sample(m: Machine) -> None:
        if not samples:
            raise Fault("an eighth curve sample")
        calls["curve"].append([m.r[4], s32(m.r[5])])
        m.r[2] = samples.pop(0) & 0xFFFFFFFF

    machine.stub(LOOK_AT, look_at)
    machine.stub(ALLOCATE, allocate)
    machine.stub(CAPTURE, capture)
    machine.stub(TRANSITION, lambda m: calls["transition"].append(s32(m.r[4])))
    machine.stub(SAMPLE_CURVE, sample)
    machine.follow.update(FOLLOW)

    machine.map_zero(SLOT, 0x10)
    machine.map_zero(PACKAGE, 0x10)
    machine.map_zero(BLOCK, 4 + 256 * DEFINITION_SIZE)
    machine.map_zero(INSTANCE, INSTANCE_SIZE)
    machine.store(STATE, bytes(value["state"]))
    machine.store(SCRATCH, bytes(value["scratch"]))
    machine.put(SLOT + 8, STATE)
    put_words(machine, TRANS, value["trans"])
    machine.put(CURRENT_INSTANCE, INSTANCE)
    machine.put(PACKAGE_POINTER, PACKAGE)
    machine.put(PACKAGE + 8, BLOCK)
    definition = BLOCK + 4 + value["index"] * DEFINITION_SIZE
    machine.store(definition, bytes(value["definition"]))
    for index, translation in value["translations"].items():
        put_words(machine, INSTANCE + index * TRANSFORM_STRIDE + 0x68, translation)
    machine.put(INSTANCE + value["definition"][0xC8] * TRANSFORM_STRIDE + 0xDA, value["facing"], 2)
    put_words(machine, EFFECT_ORIGIN, value["origin"], 2)
    put_words(machine, MOTION_FRAME, value["motion"][:1], 2)
    put_words(machine, MOTION_DURATION, value["motion"][1:], 2)
    machine.store(MOTION_ENDS, value["ends"])


def load_all(machine: Machine, fields: list[tuple[int, int, bool]]) -> list[int]:
    return [machine.load(address, size, signed) for address, size, signed in fields]


def driver_inputs(machine: Machine, value: dict) -> list[int]:
    definition = BLOCK + 4 + value["index"] * DEFINITION_SIZE
    state = load_all(machine, OUTPUT_FIELDS[:STATE_FIELDS])
    scratch = load_all(machine, [(SCRATCH + 0x24, 4, False), *words(SCRATCH + 0x2C, 3, 2),
                                 *words(SCRATCH + 0x98, 3), *words(SCRATCH + 0xA8, 3),
                                 *words(SCRATCH + 0xE8, 3), *words(SCRATCH + 0x118, 2),
                                 *words(SCRATCH + 0x13C, 3, 2), *words(SCRATCH + 0x144, 3, 2),
                                 *((SCRATCH + offset, 4, True) for offset in SCRATCH_ANGLE_ORDER)])
    record = load_all(machine, [(definition + 4, 4, False),
                                *((definition + offset, 1, False) for offset in (8, 0xA, 0xC, 0xF, 0x10, 0x17)),
                                *words(definition + 0x18, 6, 2), *words(definition + 0x24, 6, 2),
                                *words(definition + 0x34, 6, 2), *words(definition + 0xA4, 4, 2),
                                *words(definition + 0x7C, 6, 2), *words(definition + 0x88, 6, 2),
                                *words(definition + 0x30, 2, 2),
                                (definition + 0xC8, 1, False), (definition + 0xC9, 1, False)])
    transform = INSTANCE + value["definition"][0xC8] * TRANSFORM_STRIDE
    attraction = INSTANCE + value["definition"][0xC9] * TRANSFORM_STRIDE
    anchors = load_all(machine, [*words(EFFECT_ORIGIN, 3, 2), *words(transform + 0x68, 3),
                                 *words(attraction + 0x68, 3), (transform + 0xDA, 2, True)])
    captures = [element for pose in value["captures"] for element in pose]
    globals_ = load_all(machine, OUTPUT_FIELDS[-GLOBAL_FIELDS:])
    return [value["kind"], value["command"], s32(value["packed"]), *state, *scratch, *record, *anchors,
            *value["look_at"], *value["trans"], *captures, *value["samples"], *globals_]


def allowed(address: int, size: int) -> bool:
    regions = ((STATE, STATE + STATE_SIZE), (SCRATCH, SCRATCH + SCRATCH_SIZE), (SLOT + 8, SLOT + 12),
               (MOTION_FRAME, MOTION_FRAME + 2), (MOTION_DURATION, MOTION_DURATION + 2),
               (MOTION_ENDS, MOTION_ENDS + 0x80), (DEFAULT_STACK - 0x200, DEFAULT_STACK))
    return any(start <= address and address + size <= end for start, end in regions)


def run(machine: Machine, value: dict) -> tuple:
    prepare(machine, value)
    inputs = driver_inputs(machine, value)
    kind = value["kind"]
    machine.r[4] = SLOT if kind == 4 else STATE
    machine.r[5] = (TRANS if kind == 0 else value["command"] if kind == 4 else SCRATCH) & 0xFFFFFFFF
    machine.r[6] = value["packed"] if kind == 4 else INSTANCE
    machine.r[7] = BLOCK + 4 + value["index"] * DEFINITION_SIZE
    machine.execute(ENTRIES[kind])
    for address, size in machine.stores:
        if not allowed(address, size):
            raise Fault(f"store outside the camera's state, scratch and globals {address:#x}")
    result = s32(machine.r[2]) if kind == 4 else 0
    calls = machine.calls
    records = [len(calls["look_at"]), *(element for call in calls["look_at"] for element in call),
               len(calls["transition"]), *calls["transition"], len(calls["capture"]),
               len(calls["curve"]), *(element for call in calls["curve"] for element in call)]
    gte = [*machine.gte_rotation, *machine.gte_translation]
    observation = {
        "values": [result, *load_all(machine, OUTPUT_FIELDS), *records],
        "captures": list(calls["capture"]),
        "slot": machine.load(SLOT + 8),
        "gte": gte,
        "tick": kind == 4 and value["command"] == 2,
    }
    return observation, inputs


def packed_rotation(native: list[int]) -> list[int]:
    rotation = native[1 + 4 + 28 + 6:1 + 4 + 28 + 6 + 9]
    pairs = [(rotation[i] & 0xFFFF) | (rotation[i + 1] & 0xFFFF) << 16 for i in range(0, 8, 2)]
    return [*pairs, rotation[8]]


def check(image: Image, vector: dict, observation: dict, native: list[int], reference: dict) -> list[str]:
    problems = []
    original = observation["values"]
    if len(native) != len(original):
        return [f"{len(native)} native values, {len(original)} original"]
    for index, (mine, theirs) in enumerate(zip(native, original, strict=True)):
        if mine != theirs:
            problems.append(f"value {index}: native {mine} original {theirs}")
            if len(problems) > 4:
                break
    if vector["kind"] == 4 and vector["command"] == 1:
        if observation["slot"] != STATE:
            problems.append(f"slot state {observation['slot']:#x}")
        if observation["captures"] not in ([STATE + 8, STATE + 0x48],):
            problems.append(f"captures {observation['captures']}")
    if observation["tick"]:
        view = native[1 + 4 + 28 + 6:1 + 4 + 28 + 6 + 12]
        want = [value & 0xFFFFFFFF for value in packed_rotation(native)[:4]] + [view[8]]
        rotation = observation["gte"][:5]
        translation = observation["gte"][5:]
        if rotation != want or translation != view[9:12]:
            problems.append(f"GTE rotation/translation {observation['gte']} vs view {view}")
    return problems


def describe(image: Image, results: list) -> list[str]:
    kinds = Counter(KIND_NAMES[vector["kind"]] for vector, _, _ in results)
    commands = Counter(vector["command"] if vector["command"] <= 5 else "other"
                       for vector, _, _ in results if vector["kind"] == 4)
    returns = Counter(native[0] for vector, _, native in results if vector["kind"] == 4)
    shapes = Counter(int.from_bytes(vector["definition"][4:8], "little") & 0x38
                     for vector, _, _ in results if vector["kind"] == 4 and vector["command"] == 2)
    lifetimes = Counter(vector["state"][0] for vector, _, _ in results
                        if vector["kind"] == 4 and vector["command"] == 2 and vector["state"][0] < 3)
    return [
        "Vectors per function: " + ", ".join(f"{name} {count}" for name, count in sorted(kinds.items())),
        "UpdateEffectCamera commands: " + ", ".join(f"{k} {v}" for k, v in sorted(commands.items(), key=str)),
        f"UpdateEffectCamera returns: {dict(sorted(returns.items()))}",
        f"Tick offset shapes (flags & 0x38): {dict(sorted(shapes.items()))}",
        f"Tick lifetimes 0/1/2: {dict(sorted(lifetimes.items()))}",
    ]
