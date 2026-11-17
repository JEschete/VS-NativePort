"""F01515: complete original emitter instructions against the typed particle core.

The pool allocator, vector interpolation, trigonometry, square roots, Euler
rotations, normalization and look-at body execute from BATTLE.PRG/SLUS. BIOS
randomness, texture initialization and actor-bone lookup are host boundaries.
OuterProduct0 is the Q12 cross-product boundary (the shared interpreter lacks
OP); ApplyRotMatrix uses the shared GTE model and saturated IR outputs (its SWC2
IR stores are unsupported). No private instruction interpreter is used.

Original zero/full-count emission dereferences an uninitialized node (#73).
Those previously repaired cases, refused package data and allocation rollback
are native Catch2 controls, not fabricated original parity. Padding and the
unconsumed RotTrans FLAG scratch word are outside the semantic observation.
Filled polygon vectors keep the inner fraction within Q12 [0,4096]; negative
SquareRoot12 inputs address data before the SDK root table and are not a
mathematical input contract. That malformed-data boundary remains explicit.
"""

from __future__ import annotations

import random
from collections import Counter

from differential import DEFAULT_STACK, IR, VXY0, VZ0, R, s16, s32, store_problems

ID = "F01515"
ENTRY, END = 0x800D2ADC, 0x800D46DC
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "particle_emitter_driver.cpp"
SOURCES = (
    r"Native\Game\src\Effects\ParticleEmitter.cpp",
    r"Native\Game\src\Effects\ParticleMotion.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Interpolation.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\RandomGenerator.cpp",
    r"Native\Engine\src\Core\Log.cpp",
)
CXX_STANDARD = "c++latest"
VECTORS, SEED, MUTANT_VECTORS = 2048, 1515, 512

INSTANCE, BLOCK, POOL, PACKAGE, PARENT = (
    0x80500000,
    0x80510000,
    0x80520000,
    0x80530000,
    0x80540000,
)
SCRATCH, SCRATCH_BYTES = 0x1F8001D0, 0x18C
RECORDS, STRIDE = 8, 0x78
SAMPLES, ORIGIN, COUNTER = 0x800F5330, 0x800F5310, 0x800EC328
CAPACITY, FREE, USED, PACKAGE_POINTER = 0x800F55F0, 0x800F55F4, 0x800F55F8, 0x800F569C
IDENTITY = (4096, 0, 0, 0, 4096, 0, 0, 0, 4096)
FOLLOW = (
    0x800CFB80,
    0x800CFCF0,
    0x800CFD60,
    0x800CFDD0,
    0x800CFE1C,
    0x800D27F0,
    0x800D6CCC,
    0x800D29B0,
    0x800D29F0,
    0x800D2A14,
    0x800D2A38,
    0x80040F24,
    0x80040F28,
    0x80041200,
    0x80041508,
    0x80041598,
    0x800417EC,
    0x80041A94,
    0x80041AF4,
    0x80041B94,
    0x80041BF0,
    0x8007AFA8,
)
MUTANTS = {
    "transparency-default": {0x800D2B9C: 0x24020003},
    "parent-variant-mask": {0x800D2BD4: 0x30420007},
    "interval-ignored": {0x800D2C10: 0},
    "count-uses-interval-range": {0x800D2C24: 0x26E400C0},
    "capacity-clamp-dropped": {0x800D2C88: 0},
    "transform-store-dropped": {0x800D2CA8: 0},
    "attraction-transform-store-dropped": {0x800D2CB4: 0},
    "texture-arguments-swapped": {0x800D2CB8: 0x92E50000, 0x800D2CBC: 0x92E60001},
    "position-q11": {0x800D2DE8: 0x000212C0},
    "shape-mask": {0x800D2E60: 0x30430030},
    "sphere-placement-dropped": {0x800D3020: 0},
    "box-corner-counter-mask": {0x800D32A0: 0x30C20003},
    "box-placement-dropped": {0x800D32E8: 0},
    "cone-placement-dropped": {0x800D350C: 0},
    "cylinder-placement-dropped": {0x800D379C: 0},
    "disc-placement-dropped": {0x800D3A20: 0},
    "polygon-fill-range": {0x800D3B18: 0x24021000},
    "polygon-placement-dropped": {0x800D3C88: 0},
    "speed-scale": {0x800D3CD0: 0x00021100},
    "direction-mode-mask": {0x800D3D20: 0x3043C000},
    "euler-build-dropped": {0x800D3EBC: 0},
    "inward-fallback-y-dropped": {0x800D423C: 0},
    "force-z-store-dropped": {0x800D4308: 0},
    "acceleration-z-store-dropped": {0x800D4344: 0},
    "mass-store-dropped": {0x800D43D4: 0},
    "gravity-store-dropped": {0x800D43E8: 0},
    "lifetime-store-dropped": {0x800D43FC: 0},
    "attraction-mode-mask": {0x800D4454: 0x30430180},
    "target-x-negative-bound-changed": {0x800D44E8: 0x86450144},
    "target-clear-z-dropped": {0x800D45E0: 0},
    "color-age-store-dropped": {0x800D4634: 0},
    "emit-flags-store-dropped": {0x800D4650: 0},
    "attraction-store-dropped": {0x800D4668: 0},
    "color-store-dropped": {0x800D466C: 0},
    "events-store-dropped": {0x800D4678: 0},
    "transparency-store-dropped": {0x800D4684: 0},
    "variant-store-dropped": {0x800D4698: 0},
}


def put(raw: bytearray, at: int, value: int, width: int = 4) -> None:
    raw[at : at + width] = (value & ((1 << (width * 8)) - 1)).to_bytes(width, "little")


def put_vector(raw: bytearray, at: int, values, width: int = 4) -> None:
    for index, value in enumerate(values):
        put(raw, at + index * width, value, width)


def record(index: int) -> int:
    return POOL + index * STRIDE


def vector(rng: random.Random, number: int) -> dict:
    shape = (number % 8) * 8
    distribution = ((number // 8) % 4) * 0x800
    direction = ((number // 32) % 8) * 0x2000
    attraction = ((number // 64) % 8) * 0x40
    flags = shape | distribution | direction | attraction | rng.randrange(8)
    flags |= rng.choice((0, 0x20000, 0x40000, 0x60000))
    definition = bytearray(rng.randbytes(0xCC))
    transparency = rng.choice((0, 0, 1, 4, 7, 32, 63))
    put(definition, 4, flags | transparency << 26)
    definition[8:24] = bytes(rng.randrange(33) for _ in range(16))
    # Exercise signed/extrapolated interpolation, but not negative root indices.
    samples = [0] + [
        rng.choice((0, 1, 64, 127, 128, 255, -128, rng.randrange(129)))
        for _ in range(32)
    ]
    for offset in range(0x18, 0x30, 2):
        put(definition, offset, rng.randrange(-700, 701), 2)
    for offset in range(0x34, 0x94, 2):
        put(definition, offset, rng.randrange(-2048, 2049), 2)
    for offset in range(0x94, 0xBC, 2):
        put(
            definition,
            offset,
            rng.choice((0, -1, 1, 32, -256, 32767, -32768, rng.randrange(-512, 513))),
            2,
        )
    put_vector(definition, 0x30, (rng.randrange(4097), rng.randrange(4097)), 2)
    if shape in (40, 48) and distribution != 0x1000:
        definition[23] = 32
        samples[32] = rng.randrange(129)
    count = rng.choice((1, 2, 3, 4, 8, 12))
    put_vector(definition, 0xBC, (count, count), 2)
    interval = rng.choice((0, 1, 1, 1, 2, 3, 4, -1, -2))
    put_vector(definition, 0xC0, (interval, interval), 2)
    definition[0xC8], definition[0xC9] = 2, 5
    definition[0xCA], definition[0xCB] = (
        rng.choice((0, 1, 3, 4, 7, 16, 255)),
        rng.choice((0, 1, 2, 4, 9, 255)),
    )
    frame = 0 if interval == 0 else rng.randrange(0x10000)
    if number % 11 != 0 and interval != 0:
        frame -= frame % abs(interval)
    # Bias the zero-height cone and the inward zero-vector fallback.
    if shape == 16 and number % 3 == 0:
        put_vector(definition, 0x28, (0, 0), 2)
    if number % 97 == 0:
        flags = (flags & ~(0xE038 | 0x1800)) | 0x2000
        put(definition, 4, flags | transparency << 26)
        put_vector(definition, 0x24, (0,) * 6, 2)
    scratch = bytearray(rng.randbytes(SCRATCH_BYTES))
    put_vector(scratch, 0x2C, (rng.randrange(-1000, 1001) for _ in range(3)), 2)
    used = rng.randrange(3)
    order = rng.sample(range(RECORDS), RECORDS)
    occupied, free = order[:used], order[used:]
    particles = [bytearray(rng.randbytes(STRIDE)) for _ in range(RECORDS)]
    for raw in particles:
        put_vector(raw, 0xC, (rng.randrange(-0x400000, 0x400001) for _ in range(3)))
        put(raw, 0, 0)
        put(raw, 4, 0)
    for chain in (occupied, free):
        for at, index in enumerate(chain):
            put(particles[index], 0, record(chain[at - 1]) if at else 0)
            put(
                particles[index], 4, record(chain[at + 1]) if at + 1 < len(chain) else 0
            )
    return {
        "number": number,
        "seed": rng.getrandbits(32),
        "counter": rng.randrange(512),
        "frame": frame,
        "capacity": RECORDS,
        "used": used,
        "override": rng.choice((0, 0, 1, 255, 256, -1)),
        "variant": rng.choice((0, 0, 1, 15, 16, 31, -1)),
        "parent": number % 3 != 0,
        "parent_position": [rng.randrange(-0x400000, 0x400001) for _ in range(3)],
        "parent_transparency": rng.randrange(256),
        "parent_variant": rng.randrange(16),
        "origin": [rng.randrange(-500, 501) for _ in range(3)],
        "rotation": list(IDENTITY)
        if number % 3 == 0
        else [rng.randrange(-4096, 4097) for _ in range(9)],
        "translation": [rng.randrange(-500, 501) for _ in range(3)],
        "attraction_translation": [s32(rng.getrandbits(32)) for _ in range(3)],
        "aim": [rng.randrange(-2000, 2001) for _ in range(3)],
        "actor_aim": number % 2 == 0,
        "first": record(occupied[0]) if occupied else 0,
        "free": record(free[0]),
        "definition": definition,
        "samples": samples,
        "scratch": scratch,
        "particles": particles,
        "flags": flags,
    }


def encode(value: dict) -> list[int]:
    return [
        value["seed"],
        value["counter"],
        value["frame"],
        value["capacity"],
        value["used"],
        value["override"],
        value["variant"],
        int(value["parent"]),
        *value["parent_position"],
        value["parent_transparency"],
        value["parent_variant"],
        *value["origin"],
        *value["rotation"],
        *value["translation"],
        *value["attraction_translation"],
        *value["aim"],
        value["first"],
        value["free"],
        value["used"],
        *value["definition"],
        *value["samples"],
        *value["scratch"],
        *(byte for raw in value["particles"] for byte in raw),
    ]


def run(machine, value: dict):
    random_state = [value["seed"]]
    texture_calls = []

    def rand(m):
        random_state[0] = (random_state[0] * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
        m.r[R["v0"]] = (random_state[0] >> 16) & 0x7FFF

    def initialize(m):
        texture_calls.append(((m.r[4] - POOL - 0x3C) // STRIDE, m.r[5], m.r[6]))

    def actor_aim(m):
        for axis, component in enumerate(value["aim"]):
            m.write(m.r[6] + axis * 2, component, 2)
        m.r[2] = 0

    def cross_product(m):
        left = [s16(m.load(m.r[4] + axis * 4)) for axis in range(3)]
        right = [s16(m.load(m.r[5] + axis * 4)) for axis in range(3)]
        for axis in range(3):
            first, second = (axis + 1) % 3, (axis + 2) % 3
            result = (left[first] * right[second] - left[second] * right[first]) >> 12
            m.write(m.r[6] + axis * 4, result, 4)

    def rotate_saturated(m):
        m.gte_write(VXY0, m.load(m.r[4]))
        m.gte_write(VZ0, m.load(m.r[4] + 4))
        m.gte_transform_v0((0, 0, 0))
        for axis, register in enumerate(IR):
            m.write(m.r[5] + axis * 4, m.gte_read(register), 4)
        m.r[2] = m.r[5]

    machine.map_zero(INSTANCE, 0x1000)
    machine.map_zero(PACKAGE, 0x100)
    machine.map_zero(PARENT, STRIDE)
    machine.map_zero(DEFAULT_STACK, 32)
    machine.store(BLOCK, bytes(4) + value["definition"])
    machine.store(SCRATCH, value["scratch"])
    for index, raw in enumerate(value["particles"]):
        machine.store(record(index), raw)
    machine.put(PACKAGE_POINTER, PACKAGE)
    machine.put(PACKAGE + 8, BLOCK)
    for address, item in (
        (COUNTER, value["counter"]),
        (CAPACITY, RECORDS),
        (FREE, value["free"]),
        (USED, value["used"]),
        (INSTANCE + 0x18, value["first"]),
    ):
        machine.put(address, item)
    machine.put(INSTANCE + 8, value["used"], 1)
    machine.put(INSTANCE + 0x16, value["frame"], 2)
    for index, sample in enumerate(value["samples"]):
        machine.put(SAMPLES + 4 * index, sample)
    for index, component in enumerate(value["origin"]):
        machine.put(ORIGIN + 2 * index, component, 2)
    matrix = INSTANCE + 0x54 + value["definition"][0xC8] * 0xD0
    for index, component in enumerate(value["rotation"]):
        machine.put(matrix + 2 * index, component, 2)
    for index, component in enumerate(value["translation"]):
        machine.put(matrix + 0x14 + 4 * index, component)
    for index, component in enumerate(value["attraction_translation"]):
        machine.put(
            INSTANCE + 0x68 + value["definition"][0xC9] * 0xD0 + 4 * index, component
        )
    for endpoint in (INSTANCE + 0xD1C + 0xC, INSTANCE + 0xD1C + 0x24):
        machine.put(endpoint, 4 if value["actor_aim"] else 1, 2)
        for axis, component in enumerate(value["aim"]):
            machine.put(endpoint + 4 + 2 * axis, component, 2)
    for axis, component in enumerate(value["parent_position"]):
        machine.put(PARENT + 0xC + axis * 4, component)
    machine.put(
        PARENT + 0x60, value["parent_transparency"] | value["parent_variant"] << 8
    )
    machine.put(DEFAULT_STACK + 16, PARENT if value["parent"] else 0)
    machine.r[4:8] = [
        INSTANCE,
        0,
        value["override"] & 0xFFFFFFFF,
        value["variant"] & 0xFFFFFFFF,
    ]
    machine.follow.update(FOLLOW)
    machine.stub(0x80026EC4, rand)
    machine.stub(0x800D6CF0, initialize)
    machine.stub(0x800A1AF8, actor_aim)
    machine.stub(0x800419A8, cross_product)
    machine.stub(0x80041508, rotate_saturated)
    machine.execute(ENTRY)
    output = [
        0,
        random_state[0],
        machine.load(COUNTER, 4, True),
        machine.load(USED),
        machine.load(INSTANCE + 8, 1),
        machine.load(INSTANCE + 0x18),
        machine.load(FREE),
        0,
        *machine.bytes(SCRATCH, SCRATCH_BYTES),
        *machine.bytes(POOL, RECORDS * STRIDE),
        len(texture_calls),
        *(item for call in texture_calls for item in call),
    ]
    return {"output": output, "stores": list(machine.stores)}, encode(value)


def check(image, value, observation, native, reference):
    expected = observation["output"]
    if len(native) != len(expected):
        return [f"output length {len(native)} != {len(expected)}"]
    # Actor/position SVECTOR copies include an unconsumed padding halfword.
    # RotTrans writes FLAG into the otherwise unused scratch.direction.pad.
    ignored = {8 + 0x32, 8 + 0x33, *range(8 + 0xC4, 8 + 0xC8)}
    problems = [
        f"word {index}: native {got} original {want}"
        for index, (got, want) in enumerate(zip(native, expected, strict=True))
        if index not in ignored and got != want
    ]
    allowed = [
        (DEFAULT_STACK - 0x4000, DEFAULT_STACK + 32),
        (SCRATCH, SCRATCH + SCRATCH_BYTES),
        (POOL, POOL + RECORDS * STRIDE),
        (INSTANCE + 8, INSTANCE + 9),
        (INSTANCE + 0x18, INSTANCE + 0x1C),
        (COUNTER, COUNTER + 4),
        (FREE, FREE + 8),
    ]
    return problems[:8] + store_problems(observation["stores"], allowed)


def describe(image, results):
    shapes = Counter((value["flags"] & 0x38) for value, _, _ in results)
    directions = Counter((value["flags"] & 0xE000) for value, _, _ in results)
    spawned = sum(
        observation["output"][3] > value["used"] for value, observation, _ in results
    )
    return [
        f"{spawned} actual emitting vectors; shapes {dict(sorted(shapes.items()))}; "
        f"directions {dict(sorted(directions.items()))}.",
        "Compared complete particle records/list links, allocation/texture-call order, RNG state, "
        "emission counter and named scratch values; preserved stale fields in unhandled switch modes.",
        "Zero/full count (#73), invalid package extents, invalid links and allocation rollback are "
        "separate native controls; no gameplay/compatibility-retirement claim.",
    ]
