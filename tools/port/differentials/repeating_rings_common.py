"""Shared fixtures, not an interpreter, for seven original ring bodies.

Each case loads its own overlay at 800F9800 and executes its own full span.
Interpolation, trig, GTE matrix loads and every RTPS/MVMVA remain original
instructions. Declared host boundaries are allocation, curve sampling, BIOS
rand, texture initialization/advance and child emission. Their arguments/order
are observed; texture bytecode and recursively emitted children are separate
promoted cores, not simulated here.
"""

from __future__ import annotations

import random
from collections import Counter
from functools import cache, partial

from differential import (
    DEFAULT_STACK,
    DQA,
    DQB,
    MODULE_BASES,
    OFX,
    OFY,
    VXY0,
    VZ0,
    H,
    Image,
    R,
    s16,
    s32,
    store_problems,
)
from particle_emitter import IDENTITY, put, put_vector

SLOT, STATE, INSTANCE, BLOCK, PACKAGE, TEXTURES, INFO = (
    0x80500000,
    0x80510000,
    0x80520000,
    0x80530000,
    0x80540000,
    0x80550000,
    0x80560000,
)
SCRATCH_BASE, SCRATCH = 0x1F800000, 0x1F8001D0
PACKETS, ORDERING = 0x80600000, 0x80610040
ORIGIN, CURRENT_INSTANCE, PACKAGE_POINTER = 0x800F5310, 0x800F53BC, 0x800F569C
NEAR_CLIP = 0x8005E0C8
STATE_BYTES, SCRATCH_BYTES = 0x3C, 0x18C
FOLLOW = (
    0x800CFCF0,
    0x800CFD60,
    0x800CFDD0,
    0x800CFE1C,
    0x800D29B0,
    0x800D6CCC,
    0x80040F24,
    0x80040F28,
    0x80041200,
    0x80041578,
    0x80041598,
    0x8007AFA8,
)
SOURCES = (
    r"Native\Game\src\Effects\Templates\RepeatingRings.cpp",
    r"Native\Game\src\Effects\Templates\RingCore.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Interpolation.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Gte.cpp",
    r"Native\Engine\src\Core\RandomGenerator.cpp",
    r"Native\Engine\src\Core\Log.cpp",
)


def transform(rng: random.Random, number: int, view: bool) -> list[int]:
    rotation = (
        list(IDENTITY)
        if number % 3 == 0
        else [rng.randrange(-4096, 4097) for _ in range(9)]
    )
    translation = [rng.randrange(-300, 301) for _ in range(3)]
    if view:
        translation[2] = rng.choice((0, 1, 128, 512, 2048, 4096, 8192, 32768))
    return rotation + translation


@cache
def palette_bytes(module: str, address: int) -> bytes:
    image = Image.load(module)
    offset = address - image.base
    if offset < 0 or offset + 32 > len(image.data):
        raise ValueError(f"{module}: palette at {address:#x} is outside the image")
    return image.data[offset : offset + 32]


def vector(
    rng: random.Random,
    number: int,
    *,
    ring: int,
    module: str,
    palette_address: int,
) -> dict:
    command = (1, 3, 4, 0xFFFFFFFF)[number % 16] if number % 16 < 4 else 2
    definition = bytearray(rng.randbytes(0xCC))
    flags = (number % 8) | (0x20000 if number % 2 else 0)
    transparency = rng.randrange(64)
    put(definition, 4, flags | transparency << 26)
    definition[3] = rng.choice((0, 1, 2, 5, 255))
    definition[8:24] = bytes(rng.randrange(33) for _ in range(16))
    definition[0xC4:0xC7] = bytes(rng.randrange(33) for _ in range(3))
    definition[0xC8] = 3
    for offset in range(0x18, 0x94, 2):
        put(definition, offset, rng.randrange(-600, 601), 2)
    for offset in range(0x94, 0xBC, 2):
        put(definition, offset, rng.randrange(-2048, 2049), 2)
    if number % 4 == 0:
        speed = rng.randrange(-4096, 4097)
        put_vector(definition, 0xA4, (speed,) * 4, 2)
    put_vector(definition, 0x30, (rng.randrange(-4096, 8193) for _ in range(2)), 2)
    state = bytearray(rng.randbytes(STATE_BYTES))
    state[0], state[1] = rng.choice((0, 1, 2, 3, 255)), rng.randrange(4)
    for offset in range(4, 28, 4):
        put(state, offset, rng.randrange(-4096, 4097))
    put(state, 24, rng.choice((0, 1, -1, 127, 255, 32767, 0x7FFFFFFF)))
    put(state, 0x38, INFO)
    return {
        "number": number,
        "ring": ring,
        "seed": rng.getrandbits(32),
        "command": command,
        "packed": s32(
            (rng.getrandbits(16) << 16) | rng.randrange(4) << 8 | rng.randrange(256)
        ),
        "definition": definition,
        "state": state,
        "scratch": bytearray(rng.randbytes(SCRATCH_BYTES)),
        "origin": [rng.randrange(-1000, 1001) for _ in range(3)],
        "world": transform(rng, number, False),
        "effect_view": transform(rng, number + 1, True),
        "frame_view": transform(rng, number + 3, True),
        "projection": [
            rng.randrange(-300, 301),
            rng.randrange(-200, 201),
            rng.choice((64, 128, 256, 320, 512)),
            rng.randrange(-3000, 3001),
            20971520,
        ],
        "near": rng.choice((-1, 0, 1, 32, 256, 2047, 2048)),
        "samples": [0]
        + [
            rng.choice((-128, 0, 1, 64, 127, 128, 255, rng.randrange(256)))
            for _ in range(32)
        ],
        "texture": [rng.getrandbits(16), rng.getrandbits(16), *rng.randbytes(4)],
        "palette": palette_bytes(module, palette_address),
    }


def encode(value: dict) -> list[int]:
    return [
        value["ring"],
        value["seed"],
        value["command"],
        value["packed"],
        *value["definition"],
        *value["state"],
        *value["scratch"],
        *value["origin"],
        *value["world"],
        *value["effect_view"],
        *value["frame_view"],
        *value["projection"],
        value["near"],
        *value["samples"],
        *value["texture"],
        *value["palette"],
    ]


def put_bytes(machine, address: int, data) -> None:
    for index, byte in enumerate(data):
        machine.put(address + index, byte, 1)


def put_transform(machine, address: int, values: list[int]) -> None:
    for index, value in enumerate(values[:9]):
        machine.put(address + index * 2, value, 2)
    for index, value in enumerate(values[9:]):
        machine.put(address + 20 + index * 4, value)


def run(machine, value: dict, *, entry: int):
    for address, size in (
        (SLOT, 0x20),
        (STATE, STATE_BYTES),
        (INSTANCE, 0x1800),
        (BLOCK, 4 + 4 * 0xCC),
        (PACKAGE, 0x200),
        (TEXTURES, 16),
        (INFO, 32),
        (SCRATCH_BASE, 0x400),
        (PACKETS, 0x1000),
        (ORDERING - 64, 2064 * 4),
    ):
        machine.map_zero(address, size)
    put_bytes(machine, STATE, value["state"])
    put_bytes(machine, SCRATCH, value["scratch"])
    for index in range(4):
        put_bytes(machine, BLOCK + 4 + index * 0xCC, value["definition"])
    machine.put(SLOT + 8, STATE)
    machine.put(CURRENT_INSTANCE, INSTANCE)
    machine.put(PACKAGE_POINTER, PACKAGE)
    machine.put(PACKAGE + 8, BLOCK)
    machine.put(PACKAGE + 0xA8, TEXTURES)
    machine.put(INFO + 2, 0, 2)
    clut, tpage, u, v, w, h = value["texture"]
    for offset, datum, size in (
        (0, clut, 2),
        (2, tpage, 2),
        (4, u, 1),
        (5, v, 1),
        (6, w, 1),
        (7, h, 1),
    ):
        machine.put(TEXTURES + offset, datum, size)
    for axis, coordinate in enumerate(value["origin"]):
        machine.put(ORIGIN + axis * 2, coordinate, 2)
    world = INSTANCE + 0x54 + value["definition"][0xC8] * 0xD0
    put_transform(machine, world, value["world"])
    put_transform(machine, world + 0x20, value["effect_view"])
    put_transform(machine, SCRATCH_BASE + 0x14, value["frame_view"])
    machine.put(SCRATCH_BASE, PACKETS)
    machine.put(SCRATCH_BASE + 4, ORDERING)
    machine.put(NEAR_CLIP, value["near"])
    for index in range(2064):
        machine.put(ORDERING - 64 + index * 4, 0xAB000000 | (index * 13 + 5))
    ofx, ofy, screen, dqa, dqb = value["projection"]
    machine.gte_projection.update(
        {OFX: s32(ofx << 16), OFY: s32(ofy << 16), H: screen, DQA: dqa, DQB: dqb}
    )
    machine.follow.update(FOLLOW)
    sample_calls = [-1, value["state"][1]] if value["command"] == 2 else []
    texture_calls, projections, events = [], [], []
    allocations, advances, random_state = [0], [0], [value["seed"]]

    def allocate(m):
        if m.r[4] != STATE_BYTES:
            raise AssertionError(f"unexpected ring allocation {m.r[4]}")
        allocations[0] += 1
        m.r[2] = STATE

    def sample(m):
        sample_calls.extend((m.r[4], s32(m.r[5])))
        m.r[2] = value["samples"][m.r[4]] & 0xFFFFFFFF

    def random15(m):
        random_state[0] = (random_state[0] * 0x41C64E6D + 0x3039) & 0xFFFFFFFF
        m.r[2] = (random_state[0] >> 16) & 0x7FFF

    def initialize(m):
        texture_calls.extend(m.r[4:7])

    def advance(m):
        if m.r[4] != STATE + 0x1C:
            raise AssertionError(f"unexpected ring texture {m.r[4]:#x}")
        advances[0] += 1

    def emit(m):
        child = m.load(m.r[R["sp"]] + 16)
        events.extend(
            (
                *m.r[4:8],
                *(m.load(child + 12 + axis * 4, 4, True) for axis in range(3)),
                m.load(child + 0x60, 1),
            )
        )

    original_rtps = machine.gte_rtps

    def observe_projection():
        packed_xy = machine.gte_read(VXY0)
        point = (s16(packed_xy), s16(packed_xy >> 16), s16(machine.gte_read(VZ0)))
        original_rtps()
        projections.extend((*point, machine.gte_read(14), machine.gte_read(19) >> 2))

    machine.gte_rtps = observe_projection
    for address, callback in (
        (0x80043E3C, allocate),
        (0x800D118C, sample),
        (0x80026EC4, random15),
        (0x800D6CF0, initialize),
        (0x800D6D24, advance),
        (0x800D2ADC, emit),
    ):
        machine.stub(address, callback)
    machine.r[4:7] = [SLOT, value["command"], value["packed"] & 0xFFFFFFFF]
    machine.execute(entry)
    depths = [
        (address - ORDERING) // 4
        for address, size in machine.stores
        if size == 4 and ORDERING - 64 <= address < ORDERING + 2048 * 4
    ]
    packets = []
    for index, depth in enumerate(depths):
        packets.extend(
            (
                depth,
                *(machine.load(PACKETS + index * 60 + word * 4) for word in range(15)),
            )
        )
    output = [
        machine.r[2],
        random_state[0],
        allocations[0],
        advances[0],
        *machine.bytes(STATE, STATE_BYTES),
        *machine.bytes(SCRATCH, SCRATCH_BYTES),
        len(sample_calls) // 2,
        *sample_calls,
        len(projections) // 5,
        *projections,
        len(texture_calls) // 3,
        *texture_calls,
        len(packets) // 16,
        *packets,
        len(events) // 8,
        *events,
    ]
    return {
        "output": output,
        "stores": list(machine.stores),
        "quads": len(depths),
        "projections": len(projections) // 5,
        "events": len(events) // 8,
    }, encode(value)


def check(image, value, observation, native, reference):
    expected = observation["output"]
    if len(native) != len(expected):
        return [f"native/original output sizes {len(native)}/{len(expected)}"]
    problems = [
        f"word {index}: native {got} original {want}"
        for index, (got, want) in enumerate(zip(native, expected, strict=True))
        if got != want
    ]
    allowed = [
        (DEFAULT_STACK - 0x4000, DEFAULT_STACK + 32),
        (SCRATCH_BASE, SCRATCH_BASE + 4),
        (SCRATCH, SCRATCH + SCRATCH_BYTES),
        (STATE, STATE + STATE_BYTES),
        (SLOT + 8, SLOT + 12),
        (PACKETS, PACKETS + 32 * 60),
        (ORDERING - 64, ORDERING + 2048 * 4),
    ]
    return problems[:8] + store_problems(observation["stores"], allowed)


def describe(image, results):
    commands = Counter(value["command"] for value, _, _ in results)
    quads = sum(observation["quads"] for _, observation, _ in results)
    children = sum(observation["events"] for _, observation, _ in results)
    return [
        f"Commands {dict(sorted(commands.items()))}; {quads} complete submitted quads; "
        f"{children} child events; {commands[2]} full 48-point projection/motion ticks.",
        "Compared state/texture clear, named scratch, exact curve calls, RNG, every projected "
        "point, packet/link/corner/UV/color, signed ordering indices and child-event arguments.",
        "Original texture programs and recursive children are explicit host boundaries. "
        "Refused native extents/allocation and cleanup are Catch2 controls, not original parity.",
    ]


def original_mutants(ring: int, entry: int) -> dict:
    repeating = ring in (0, 1, 5)
    child_offset = 0xEE8 if repeating else 0xEAC if ring == 6 else 0xEB8
    table_word = 0x930500CB if repeating else 0x91E500CB
    return {
        "create-texture-table-swapped": {entry + 0x118: table_word},
        "inner-curve-call-dropped": {entry + 0x1D4: 0},
        "texture-advance-dropped": {entry + 0x97C: 0},
        "child-event-dropped": {entry + child_offset: 0},
        "event-index-not-decremented": {entry + child_offset + 4: 0x24A50000},
        "age-increment-doubled": {entry + child_offset + 24: 0x24420002},
    }


def configure(
    namespace: dict,
    *,
    ring: int,
    ledger_id: str,
    module: str,
    entry: int,
    end: int,
    palette_address: int,
) -> None:
    MODULE_BASES[module] = 0x800F9800
    namespace.update(
        ID=ledger_id,
        ENTRY=entry,
        END=end,
        MODULES=(module, "BATTLE/BATTLE.PRG", "SLUS_010.40"),
        DRIVER="repeating_rings_driver.cpp",
        SOURCES=SOURCES,
        CXX_STANDARD="c++latest",
        VECTORS=2048,
        SEED=2763904 + ring,
        MUTANT_VECTORS=64,
        MUTANTS=original_mutants(ring, entry),
        vector=partial(
            vector, ring=ring, module=module, palette_address=palette_address
        ),
        run=partial(run, entry=entry),
        check=check,
        describe=describe,
    )
