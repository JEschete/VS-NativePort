"""F01814-F01816 special terrain heights (7CD68.c:760-813) vs SpecialTerrain.cpp.

func_800E5EC0 floods a height through connected special terrain tiles, func_800E5FDC finds the
floor height beside a tile, and func_800E6098 scans the grid and fills each unassigned special
region. Each vector runs one of the three entries on random tile flag and height maps; the floor
callee is a deterministic hash of its world point, so both sides see the same floor.
"""

from __future__ import annotations

import random

from differential import DEFAULT_STACK, MASK, Fault, Image, Machine, s32

ID = "F01814-F01816"
ENTRY, END = 0x800E5EC0, 0x800E6158
DRIVER = "special_terrain_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\Ai\SpecialTerrain.cpp",)
VECTORS, SEED = 2048, 1814
# 32 builds, 96 fills and 128 finds per mutant.
MUTANT_VECTORS = 256

FILL, FIND, BUILD = 0x800E5EC0, 0x800E5FDC, 0x800E6098
FLOOR = 0x8008DC7C
FLAG_MAP, HEIGHT_MAP = 0x800F58B8, 0x800F58D0
FLAGS, HEIGHTS = 0x80500000, 0x80510000
SIDE, CELLS = 32, 1024
# A full-grid fill recurses up to 1024 frames of 0x28 bytes.
STACK_LIMIT = DEFAULT_STACK - 0x10000
KINDS = ("build", "fill", "find")
MUTANTS = {
    "fill-special-flag-0x10": {0x800E5F74: 0x30420010},
    "find-blocking-flags-0x30": {0x800E6048: 0x30420030},
    "scan-assigned-bit-0x1000": {0x800E60F8: 0x30421000},
    "fill-value-mask-0x1FF": {0x800E5F10: 0x328401FF},
    "fill-kept-mask-0xF800": {0x800E5F14: 0x3042F800},
    "find-height-shift-23": {0x800E6070: 0x000215C3},
    "find-unsigned-height": {0x800E6070: 0x00021582},
    "find-keeps-32-bit-floor": {0x800E6068: 0x00000000},
    "scan-fills-negative-heights": {0x800E610C: 0x00000000},
    "find-floor-at-tile-corner": {0x800E605C: 0x34840000},
    "fill-three-directions": {0x800E5FB0: 0x2A220003},
    "find-three-directions": {0x800E6078: 0x29020003},
    "fill-odd-direction-codes": {0x800E5FB8: 0x26100001},
    "scan-31-columns": {0x800E6124: 0x2A02001F},
    "scan-row-stride-32-bytes": {0x800E60B4: 0x00119140},
    "fill-no-x-bound": {0x800E5F48: 0x00000000},
}


def floor_height(seed: int, mode: int, x: int, z: int) -> int:
    """The stubbed floor callee; the driver's FloorHeight computes the same value."""
    value = (seed ^ (x * 0x9E3779B1) ^ (z * 0x85EBCA77)) & MASK
    value ^= value >> 16
    value = value * 0x7FEB352D & MASK
    value ^= value >> 15
    value = value * 0x846CA68B & MASK
    value ^= value >> 16
    if mode == 1:
        return value & 0x7FFF
    if mode == 2:
        return (value & 0xFFFF) - 0x10000 & MASK if value & 0x8000 else value & 0xFFFF
    return value


def maps(rng: random.Random) -> tuple[list[int], list[int]]:
    special = rng.choice([0.0, 0.1, 0.3, 0.5, 0.65, 0.8, 1.0])
    blocked = rng.choice([0.0, 0.2, 0.5, 0.9, 1.0])
    assigned = rng.choice([0.0, 0.05, 0.3])
    flags, heights = [], []
    for _ in range(CELLS):
        flag = rng.getrandbits(16) & ~0x70
        if rng.random() < special:
            flag |= 0x20
        if rng.random() < blocked:
            flag |= rng.choice([0x10, 0x40, 0x50])
        height = rng.getrandbits(16) & ~0x2000
        if rng.random() < assigned:
            height |= 0x2000
        flags.append(flag)
        heights.append(height)
    return flags, heights


def vector(rng: random.Random, number: int) -> list:
    kind = 0 if number % 8 == 0 else 1 if number % 8 < 4 else 2
    flags, heights = maps(rng)
    if kind == 2:
        x, z = rng.randrange(-2, SIDE + 2), rng.randrange(-2, SIDE + 2)
    else:
        x, z = rng.randrange(SIDE), rng.randrange(SIDE)
    height = s32(rng.getrandbits(32)) if rng.random() < 0.5 else rng.randrange(-0x400, 0x800)
    mode = rng.choice([0, 1, 2])
    return [kind, x, z, height, rng.getrandbits(32), mode, flags, heights]


def run(machine: Machine, vector: list) -> tuple:
    kind, x, z, height, seed, mode, flags, heights = vector

    def floor(m: Machine) -> None:
        world_x, world_z = s32(m.r[4]), s32(m.r[5])
        m.trace.append((world_x, world_z))
        m.r[2] = floor_height(seed, mode, world_x, world_z)

    machine.stub(FLOOR, floor)
    machine.follow = {FILL, FIND}
    machine.store(FLAGS, b"".join(flag.to_bytes(2, "little") for flag in flags))
    machine.store(HEIGHTS, b"".join(cell.to_bytes(2, "little") for cell in heights))
    machine.put(FLAG_MAP, FLAGS)
    machine.put(HEIGHT_MAP, HEIGHTS)
    machine.r[4], machine.r[5], machine.r[6] = x & MASK, z & MASK, height & MASK
    machine.execute((BUILD, FILL, FIND)[kind], limit=400_000)
    for address, size in machine.stores:
        if not (STACK_LIMIT <= address < DEFAULT_STACK
                or address >= HEIGHTS and address + size <= HEIGHTS + 2 * CELLS):
            raise Fault(f"store outside the height map and stack {address:#x}")
    result = s32(machine.r[2]) if kind == 2 else 1 if kind == 1 else 0
    after = tuple(machine.load(HEIGHTS + 2 * cell, 2) for cell in range(CELLS))
    inputs = [kind, x, z, height, seed, mode, *flags, *heights]
    return (result, tuple(machine.trace), after), inputs


def check(image: Image, vector: list, observation: tuple, native: list[int], reference: tuple) -> list[str]:
    result, trace, after = observation
    count = native[1]
    queries = tuple(zip(native[2:2 + 2 * count:2], native[3:2 + 2 * count:2], strict=True))
    heights = tuple(native[2 + 2 * count:])
    problems = []
    if native[0] != result:
        problems.append(f"result {native[0]} != {result}")
    if queries != trace:
        problems.append(f"floor queries {queries[:4]} != {trace[:4]} ({count} vs {len(trace)})")
    if heights != after:
        cells = [cell for cell in range(CELLS) if heights[cell] != after[cell]]
        problems.append(f"{len(cells)} heights differ from cell {cells[0]}")
    return problems


def describe(image: Image, results: list) -> list[str]:
    counts = dict.fromkeys(("build", "build-filled", "fill", "fill-cells", "find", "find-ground",
                            "find-negative", "find-none", "queries"), 0)
    for vector, (result, trace, after), _ in results:
        kind = KINDS[vector[0]]
        counts[kind] += 1
        counts["queries"] += len(trace)
        changed = sum(before != now for before, now in zip(vector[7], after, strict=True))
        if kind == "build":
            counts["build-filled"] += changed > 0
        elif kind == "fill":
            counts["fill-cells"] += changed
        elif not trace:
            counts["find-none"] += 1
        else:
            counts["find-ground" if result >= 0 else "find-negative"] += 1
    return ["Coverage: " + ", ".join(f"{name} {value}" for name, value in counts.items())]
