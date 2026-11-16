"""A39-05: each original Analyze overlay versus its own checked native Live entry."""

from __future__ import annotations

import random
import struct
from functools import partial

from differential import (
    DEFAULT_STACK,
    DQA,
    DQB,
    MODULE_BASES,
    OFX,
    OFY,
    Fault,
    H,
    R,
    s32,
    store_problems,
)

DRIVER = "analyze_grid_driver.cpp"
SOURCES = (
    r"Native\Game\src\Effects\AnalyzeGrid.cpp",
    r"Native\Game\src\Effects\AnalyzeGridLive.cpp",
    r"Native\Game\src\Effects\Templates\Grid.cpp",
    r"Native\Game\src\Effects\Templates\SurfaceCore.cpp",
    r"Native\Game\src\Effects\Templates\RingCore.cpp",
    r"Native\Game\src\Effects\PackageExtents.cpp",
    r"Native\Game\src\Effects\PackageExtentsLive.cpp",
    r"Native\Game\src\Effects\Curves.cpp",
    r"Native\Game\src\Effects\CurvesLive.cpp",
    r"Native\Engine\src\Core\Gte.cpp",
    r"Native\Engine\src\Core\Log.cpp",
    r"Native\Engine\src\Core\Math\Interpolation.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Memory\Heap.cpp",
    r"Native\Engine\src\Core\Memory\HeapLive.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
    r"Native\Engine\src\Render\SpritesLive.cpp",
    r"Native\Engine\src\Render\UiPacketsLive.cpp",
    r"Native\Engine\src\Render\UiPackets.cpp",
)
INCLUDES = (
    r"Native\Engine", r"Native\Content", r"Native\Engine\src", r"Native\Game\src",
    r"Native\Content\include", r"Native\Content\src",
)

SCRATCH = 0x1F800000
SLOT = 0x80500000
STATE = 0x80500100
PACKAGE = 0x80501000
DEFINITIONS = 0x80510000
CURVES = 0x80520000
INSTANCE = 0x80530000
PRIMITIVES = 0x80540000
ORDERING = 0x80570000
SCENE = ORDERING + 32 * 4
ORDERING_WORDS = 0x2088 // 4
SCRATCH_BYTES = 0x18C
CURVE_COUNT, CURVE_SIZE = 16, 8
CURVE_DATA_OFFSET = 4 + CURVE_COUNT
IDENTITY = (4096, 0, 0, 0, 4096, 0, 0, 0, 4096)
STATE_BYTES = (*range(22), *range(24, 28))
PACKET_WORDS = 15
OBSERVED_PACKET_WORDS = 12

FOLLOW = {
    0x800CFCF0, 0x800CFD60, 0x800CFDD0, 0x800CFE1C, 0x800D118C,
    0x800D29B0, 0x80040F24, 0x80040F28, 0x80041578, 0x80041598,
}

# Offsets and replacements were read from all four real instruction spans.
# Internal jumps differ in PLG074, but none of these mutations encodes one.
MUTATIONS = {
    "allocation-size": (0x098, 0x24040018),
    "definition-byte": (0x0A0, 0x00121243),
    "create-lifetime": (0x0C4, 0xA3C00000),
    "create-age": (0x0D0, 0x00000000),
    "page-segment-mask": (0x100, 0x30420003),
    "page-blend-mask": (0x108, 0x30630060),
    "page-base": (0x110, 0x24420019),
    "palette-row": (0x124, 0x00021140),
    "texture-origin": (0x130, 0x92E200AE),
    "position-curve": (0x190, 0x92E40009),
    "anchor-mask": (0x1DC, 0x30420006),
    "force-interpolation": (0x298, 0x00000000),
    "x-acceleration-component": (0x3B4, 0x96C2010A),
    "x-spacing-store": (0x3D4, 0x00000000),
    "z-spacing-store": (0x408, 0x00000000),
    "x-phase-store": (0x428, 0x00000000),
    "z-phase-store": (0x450, 0x00000000),
    "x-outward-force": (0x488, 0x86C20102),
    "z-outward-acceleration": (0x4F4, 0x86C30110),
    "sine-instead-of-cosine": (0x584, 0x0C0103CA),
    "wave-x-scale": (0x57C, 0x00191AC3),
    "red-saturation": (0x61C, 0x2A020080),
    "green-default": (0x66C, 0x2402007F),
    "blue-default": (0x6A8, 0x2402007F),
    "quad-code": (0x6B0, 0x2402002C),
    "u-scroll-store": (0x6E0, 0x00000000),
    "v-scroll-store": (0x744, 0x00000000),
    "u-division-rounding": (0x6FC, 0x24420000),
    "v-division-rounding": (0x760, 0x24420000),
    "effect-view-flag": (0x77C, 0x3C030004),
    "projection": (0x828, 0x00000000),
    "depth-quarter": (0x838, 0x000C6043),
    "far-clip": (0x910, 0x28C20801),
    "nearest-fixed-slot": (0x964, 0x2406FFF2),
    "middle-fixed-slot": (0x96C, 0x2406FFF4),
    "front-fixed-slot": (0x974, 0x2406FFF6),
    "farthest-fixed-slot": (0x95C, 0x240607FF),
    "nudge-distance": (0x980, 0x24C3FFF1),
    "packet-stride": (0x9A4, 0x24820038),
    "restore-mode": (0x9A0, 0x34A50100),
    "palette-store": (0xA68, 0xA482001A),
    "packet-length": (0xA7C, 0x3C030900),
    "last-quad-column": (0xABC, 0x2A620007),
    "last-quad-row": (0xAD0, 0x2AA20007),
    "age-step": (0xAE4, 0x24420002),
    "lifetime-step": (0xAF0, 0x2462FFFE),
    "expire-command": (0xB14, 0xA3C00000),
    "stop-command": (0xB18, 0xAFB105A4),
}


def _matrix(rng: random.Random, broad: bool) -> tuple[int, ...]:
    rotation = (
        tuple(rng.randrange(-32768, 32768) for _ in range(9))
        if broad else IDENTITY
    )
    translation = (
        tuple(s32(rng.getrandbits(32)) for _ in range(3))
        if broad else (rng.randrange(-50, 51), rng.randrange(-50, 51), 400)
    )
    return (*rotation, *translation)


def vector(rng: random.Random, number: int) -> dict:
    command = (1, 3, 4, 0xFFFFFFFF)[number % 16] if number % 16 < 4 else 2
    definition = bytearray(rng.randbytes(0xCC))
    broad = number >= 64 and number % 5 == 0
    limit = 32768 if broad else 40
    for at in range(0x18, 0xC4, 2):
        struct.pack_into("<h", definition, at, rng.randrange(-limit, limit))
    flags = (number % 4) | (0x20000 if (number // 4) % 2 else 0)
    flags |= rng.getrandbits(26) & ~0x20007
    struct.pack_into("<I", definition, 4, flags | (rng.randrange(64) << 26))
    for at in (*range(8, 0x18), 0xC4, 0xC5, 0xC6):
        definition[at] = rng.randrange(CURVE_COUNT + 1)
    if number % 16 == 4:
        definition[0xC4:0xC7] = bytes(3)
    definition[0xC8] = rng.randrange(16)
    mode = number % 8 - 1
    struct.pack_into("<2h", definition, 0x30, mode, mode)
    width = (0, -1, 1, -64, 64, 32767, -32768, 63)[number % 8]
    height = (0, 63, -64, -1, 64, 32767, -32768, 1)[number % 8]
    struct.pack_into("<h", definition, 0x94, width)
    struct.pack_into("<h", definition, 0x98, height)
    age = rng.randrange(0x80000000) if broad else number % 17
    if number % 31 == 0:
        age = 0x7FFFFFFF
    state = bytearray(rng.randbytes(28))
    # Offset each command cycle so create and tick both exercise all lifetimes.
    state[0] = (0, 1, 2, 255)[(number // 4 + number // 16) % 4]
    state[1] = rng.randrange(256)
    struct.pack_into("<i", state, 24, age)
    if not broad:
        for at in range(10, 22, 2):
            struct.pack_into("<h", state, at, rng.randrange(-16, 17))
    samples = bytes(
        rng.choice((0, 1, 63, 64, 127, 128, 129, 255))
        for _ in range(CURVE_COUNT * CURVE_SIZE)
    )
    frame_view = _matrix(rng, broad)
    effect_view = _matrix(rng, broad)
    near_clip = rng.choice((0, 12, 32))
    # Whole flat grids at exact near, far and nudge boundaries.
    if command == 2 and number % 9 == 0:
        frame_view = (*IDENTITY, 0, 0, 0)
        effect_view = frame_view
        depth = (12, 13, 28, 29, 2047, 2048)[(number // 9) % 6]
        for at in range(0x18, 0x94, 2):
            struct.pack_into("<h", definition, at, 0)
        struct.pack_into("<2h", definition, 0x20, depth * 4, depth * 4)
        struct.pack_into("<2h", definition, 0x30, mode, mode)
        struct.pack_into("<I", definition, 4, flags & ~7)
        near_clip = 12
        state[10:22] = bytes(12)
    packed = s32((rng.getrandbits(16) << 16) | (state[1] << 8) | state[0])
    return {
        "command": command, "packed": packed, "definition": bytes(definition),
        "state": bytes(state), "curves": samples,
        "origin": tuple(rng.randrange(-100, 101) for _ in range(3)),
        "world_translation": tuple(rng.randrange(-50, 51) for _ in range(3)),
        "frame_view": frame_view, "effect_view": effect_view,
        "near_clip": near_clip, "h": rng.choice((64, 256, 512, 65535)),
        "offset": (160, 120), "dqa": -128, "dqb": 20971520,
    }


def _store_matrix(machine, address: int, values: tuple[int, ...]) -> None:
    machine.store(address, struct.pack("<9h2x3i", *values))


def run(mode: int, entry: int, machine, value: dict):
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(SLOT, 0x80000)
    machine.put(SLOT + 8, STATE)
    machine.put(0x800F53BC, INSTANCE)
    machine.put(0x800F569C, PACKAGE)
    machine.put(PACKAGE + 8, DEFINITIONS)
    machine.put(PACKAGE + 0x8C, CURVES)
    curve_bytes = struct.pack("<HH", CURVE_COUNT, CURVE_DATA_OFFSET)
    curve_bytes += bytes([CURVE_SIZE] * CURVE_COUNT) + value["curves"]
    machine.store(CURVES, curve_bytes)
    for index in range(CURVE_COUNT):
        machine.put(PACKAGE + 0xC + 4 * index,
                    CURVES + CURVE_DATA_OFFSET + index * CURVE_SIZE)
    definition_id = value["state"][1]
    machine.store(DEFINITIONS + 4 + definition_id * 0xCC, value["definition"])
    machine.store(STATE, value["state"])
    machine.store(SCRATCH + 0x1D0, bytes([0xA5] * SCRATCH_BYTES))
    machine.store(0x800F5310, struct.pack("<3h2x", *value["origin"]))
    transform = value["definition"][0xC8]
    _store_matrix(machine, INSTANCE + 0x54 + transform * 0xD0,
                  (*IDENTITY, *value["world_translation"]))
    _store_matrix(machine, INSTANCE + 0x74 + transform * 0xD0,
                  value["effect_view"])
    _store_matrix(machine, SCRATCH + 0x14, value["frame_view"])
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, SCENE)
    machine.put(0x8005E0C8, value["near_clip"])
    for index in range(ORDERING_WORDS):
        machine.put(ORDERING + index * 4, 0xA5000000 | (index * 20 + 7))
    for name, sample in (
        (OFX, value["offset"][0] << 16), (OFY, value["offset"][1] << 16),
        (H, value["h"]), (DQA, value["dqa"]), (DQB, value["dqb"]),
    ):
        machine.gte_projection[name] = sample
    allocation: list[int] = []

    def allocate(current) -> None:
        allocation.append(current.r[R["a0"]])
        current.r[R["v0"]] = STATE

    machine.stub(0x80043E3C, allocate)
    machine.follow.update(FOLLOW)
    machine.r[R["a0"]] = SLOT
    machine.r[R["a1"]] = value["command"]
    machine.r[R["a2"]] = value["packed"] & 0xFFFFFFFF
    machine.execute(entry)
    cursor = machine.load(SCRATCH) - PRIMITIVES
    if not 0 <= cursor <= 0x20000 or cursor % (PACKET_WORDS * 4):
        raise Fault(f"invalid original grid packet cursor {cursor}")
    packet_count = cursor // (PACKET_WORDS * 4)
    output = [
        s32(machine.r[R["v0"]]), sum(allocation),
        *(machine.byte(STATE + index) for index in STATE_BYTES),
        *machine.bytes(SCRATCH + 0x1D0, SCRATCH_BYTES), cursor,
        *(machine.load(ORDERING + index * 4) for index in range(ORDERING_WORDS)),
    ]
    for index in range(packet_count):
        words = [
            machine.load(PRIMITIVES + index * PACKET_WORDS * 4 + word * 4)
            for word in range(OBSERVED_PACKET_WORDS)
        ]
        words[8] &= 0xFFFF
        words[10] &= 0xFFFF
        output.extend(words)
    inputs = [
        mode, value["command"], value["packed"], *value["state"],
        *value["definition"], *value["curves"], *value["origin"],
        *value["world_translation"], *value["frame_view"], *value["effect_view"],
        value["near_clip"], value["h"], *value["offset"], value["dqa"], value["dqb"],
    ]
    return {"values": output, "stores": machine.stores, "packets": packet_count}, inputs


def check(_image, _value, observation: dict, native: list[int], _reference) -> list[str]:
    expected = observation["values"]
    problems = []
    if native != expected:
        if len(native) != len(expected):
            problems.append(f"output length native {len(native)} original {len(expected)}")
        else:
            differences = [
                f"word {index}: native {a:#x} original {b:#x}"
                for index, (a, b) in enumerate(zip(native, expected, strict=True))
                if a != b
            ]
            problems.append("; ".join(differences[:6]))
    allowed = [
        (DEFAULT_STACK - 0x4000, DEFAULT_STACK), (SLOT + 8, SLOT + 12),
        (STATE, STATE + 28), (SCRATCH, SCRATCH + 4),
        (SCRATCH + 0x1F4, SCRATCH + 0x1F8),
        (SCRATCH + 0x268, SCRATCH + 0x274),
        (SCRATCH + 0x278, SCRATCH + 0x284),
        (SCRATCH + 0x298, SCRATCH + 0x2A4),
        (SCRATCH + 0x2B8, SCRATCH + 0x2C4),
        *((SCRATCH + at, SCRATCH + at + 6) for at in (0x2C8, 0x2D0, 0x2D8, 0x2E0)),
        (SCRATCH + 0x2E8, SCRATCH + 0x2F0),
        (SCRATCH + 0x2F4, SCRATCH + 0x304),
        (PRIMITIVES, PRIMITIVES + 64 * PACKET_WORDS * 4),
        (ORDERING, ORDERING + ORDERING_WORDS * 4),
    ]
    return problems + store_problems(observation["stores"], allowed)


def describe(_image, results: list) -> list[str]:
    counts = {
        command: sum(value["command"] == command for value, _, _ in results)
        for command in (1, 2, 3, 4, 0xFFFFFFFF)
    }
    packets = sum(observation["packets"] for _, observation, _ in results)
    return [
        f"commands={counts}; compared {packets} complete grid quads and every ordering entry",
        "Native executes the overlay-specific Live entry, owned Grid/Surface/Ring, heap, "
        "curve, interpolation, projection, package-extent and bounded frame helpers. "
        "Only storage/diagnostics and GPU pointer normalization are fixture services.",
        "Unrelated linked UI and segmented-curve services fail closed if invoked.",
        "Original follows the real resident sampler, interpolation, trig and matrix-loader "
        "instructions; only heap allocation is stubbed. Full meaningful state, scratch, "
        "packet payloads and links are compared. Reserved state/quad padding is excluded.",
    ]


def case(function_id: int, mode: int, module: str, entry: int, end: int) -> dict:
    MODULE_BASES[module] = 0x800F9800
    return {
        "ID": f"F{function_id:05d}", "ENTRY": entry, "END": end,
        "MODULES": (module, "BATTLE/BATTLE.PRG", "SLUS_010.40"),
        "DRIVER": DRIVER, "SOURCES": SOURCES, "INCLUDES": INCLUDES,
        "COMPILER": "clang-cl", "CXX_STANDARD": "c++latest",
        "COMPILER_OPTIONS": (
            "/Gy", "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX",
            "/D_CRT_SECURE_NO_WARNINGS",
        ),
        "VECTORS": 1024, "SEED": function_id, "MUTANT_VECTORS": 64,
        "MUTANTS": {
            name: {entry + offset: word}
            for name, (offset, word) in MUTATIONS.items()
        },
        "vector": vector, "run": partial(run, mode, entry),
        "check": check, "describe": describe,
    }
