"""C6 damage policies: explicit original spans, real SLUS category jump table."""

import random
import struct
import zlib
from typing import NamedTuple

from differential import DEFAULT_STACK, MASK, Image, Machine, store_problems

ID = "C6 F00394/F00396/F00397/F00408/F00432/F00434"
ENTRY, END = 0x8007FE5C, 0x8007FF8C
ENTRIES = (ENTRY, 0x80080000, 0x800801E0, 0x80081138, 0x8008384C, 0x800838BC)
ENDS = (END, 0x800801E0, 0x800802C4, 0x80081148, 0x8008388C, 0x800838EC)
FUNCTIONS = ("F00394", "F00396", "F00397", "F00408", "F00432", "F00434")
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "damage_policies_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\DamagePolicies.cpp",)
VECTORS, SEED, MUTANT_VECTORS = 24576, 394396397408432434, 8192
GUARD = 16
TABLE, LAST, DP = 0x800F1928, 0x800F1A08, 0x800F1A10
ACTION, HIT, SOURCE = 0x80500000, 0x80501000, 0x80502000
ACTOR, STATS, OTHER = 0x80600000, 0x80700000, 0x80701000
MUTANTS = {
    "F00394-wrong-type": {0x8007FE68: 0x3C030004},
    "F00394-spell-source": {0x8007FE7C: 0x24090003},
    "F00394-physical-source": {0x8007FEEC: 0x24090004},
    "F00394-spell-shield": {0x8007FEB4: 0x24060007},
    "F00394-physical-shield": {0x8007FF24: 0x24060008},
    "F00394-two-spell-slots": {0x8007FE98: 0x28620002},
    "F00394-two-physical-slots": {0x8007FF08: 0x28620002},
    "F00394-spell-bonus": {0x8007FEAC: 0x25080014},
    "F00394-physical-bonus": {0x8007FF1C: 0x25080014},
    "F00394-shield-bonus": {0x8007FF54: 0x25080005},
    "F00394-unsigned-damage": {0x8007FF58: 0x00031C02},
    "F00394-unsigned-return": {0x8007FF88: 0x00021402},
    "F00396-clear-five": {0x80080004: 0x24040004},
    "F00396-clear-tail": {0x80080010: 0x2442000E},
    "F00396-category-mask": {0x80080040: 0x30440007},
    "F00396-negative-max-inactive": {0x80080088: 0x18400002},
    "F00396-even-full-amount": {0x800800D0: 0x01201821},
    "F00396-even-no-add": {0x800800E4: 0x00601021},
    "F00396-signed-splash": {0x80080148: 0x80420920},
    "F00396-wrong-splash-row": {0x8008013C: 0x00001021},
    "F00396-splash-half": {0x80080170: 0x00031983},
    "F00396-selected-dp-neighbor": {0x800801B4: 0xA4460002},
    "F00396-selected-hp-neighbor": {0x800801D4: 0xA4620022},
    "F00397-all-limbs-active": {0x80080210: 0},
    "F00397-total-sum": {0x80080220: 0x00621021},
    "F00397-unsigned-maximum": {0x80080208: 0x9483039A},
    "F00397-unsigned-current": {0x80080218: 0x94820398},
    "F00397-inactive-write": {0x8008025C: 0},
    "F00397-no-product": {0x80080274: 0x01401021},
    "F00397-no-doubling": {0x80080298: 0x00021403},
    "F00397-unsigned-division": {0x80080280: 0x0048001B},
    "F00408-unsigned-return": {0x8008113C: 0x94421A08},
    "F00408-high-half": {0x8008113C: 0x84421A0A},
    "F00432-wrong-hit": {0x80083858: 0x8C820018},
    "F00432-low-half-only": {0x8008385C: 0x94631A08},
    "F00432-factor-mask": {0x80083864: 0x3042000F},
    "F00432-no-product": {0x8008386C: 0},
    "F00432-signed-division": {0x80083878: 0x00620018},
    "F00432-unsigned-return": {0x80083888: 0x00021402},
    "F00434-source-owner": {0x800838C0: 0x90A20000},
    "F00434-unsigned-risk": {0x800838E0: 0x94420020},
    "F00434-byte-risk": {0x800838E0: 0x90420020},
}


class Vector(NamedTuple):
    function: int
    arguments: tuple[int, ...]
    blocks: tuple[tuple[int, bytes], ...]


def put(block: bytearray, offset: int, value: int, size: int = 4) -> None:
    struct.pack_into({1: "<B", 2: "<H", 4: "<I"}[size],
                     block, GUARD + offset, value % (1 << (8 * size)))


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 6, number // 6
    locations = ((TABLE, 42 * 4), (LAST, 24), (ACTION, 0x1C), (HIT, 0x44),
                 (SOURCE, 4), (ACTOR, 0x40), (STATS, 0x944), (OTHER, 0x2A0))
    blocks = [bytearray(rng.randbytes(size + 2 * GUARD)) for _, size in locations]
    category, mask, limb = lane % 16, (lane // 16) % 64, (lane // 64) % 6
    amount = rng.choice((-32768, -101, -1, 0, 1, 101, 32767, rng.randrange(-32768, 32768)))
    slot = (0, 40)[lane % 2]
    put(blocks[0], slot * 4, ACTOR)
    put(blocks[3], 0, slot, 1)
    put(blocks[4], 0, 255, 1)
    unused_stats = function == 1 and category in (6, 8, 9, 10, 11, 12, 13, 15)
    put(blocks[5], 0x3C, 0 if unused_stats else STATS)
    put(blocks[3], 1, limb if category not in (9, 10, 13, 15) else -128, 1)
    action_type = lane % 8
    put(blocks[2], 0, (rng.getrandbits(32) & ~0xFE0000) |
        (category << 20) | (action_type << 17))
    for i in range(3):
        source_effect = (3, 4, 9, 0)[(lane // 8) % 4]
        target_effect = (7, 8, 9, 0)[(lane // 32) % 4]
        put(blocks[6], 0xCA + 0x30 * i,
            source_effect if lane % 3 == i or lane % 4 == 0 else 0, 1)
        put(blocks[7], 0x232 + 0x30 * i,
            target_effect if lane % 3 == i or lane % 4 == 0 else 0, 1)
    for i in range(6):
        maximum = rng.choice((-32768, -1, 1, 100, 32767)) if mask & (1 << i) else 0
        current = rng.choice((-32768, -1, 0, 1, 100, 32767))
        if function == 2 and lane % 8 == 0:
            maximum, current = ((100, 0), (100, 200))[i % 2]
        elif function == 2 and lane % 8 == 1:
            maximum, current = ((32767, -32768), (-32767, 32767), (0, 1))[min(i, 2)]
        put(blocks[6], 0x398 + 0xDC * i, current, 2)
        put(blocks[6], 0x39A + 0xDC * i, maximum, 2)
        for row in range(6):
            put(blocks[6], 0x920 + row * 6 + i,
                (0, 100, 128, 255, 1, rng.randrange(256))[(row + i + lane) % 6], 1)
    last = (0, 1, 32767, 32768, 65535, 0x80000001, MASK,
            rng.getrandbits(32))[lane % 8]
    put(blocks[1], 0, last)
    put(blocks[6], 0x20, (0, 255, 256, 32768, 65535, rng.randrange(65536))[lane % 6], 2)
    for hit in range(2):
        factor = (lane + hit * 13) % 32
        put(blocks[2], 0x14 + 4 * hit,
            (rng.getrandbits(32) & ~(31 << 22)) | (factor << 22))
    if function == 0:
        args = (ACTION, amount & MASK, STATS, OTHER, MASK)
    elif function in (1, 2):
        args = (ACTION if function == 1 else 0, HIT, amount & MASK, MASK, MASK)
    elif function == 3:
        args = (0, 0, 0, MASK, MASK)
    elif function == 4:
        args = (ACTION, 0, 0, lane % 2, MASK)
    else:
        args = (0, SOURCE if lane % 2 else 0, HIT, MASK, MASK)
    return Vector(function, args, tuple(
        (address - GUARD, bytes(block)) for (address, _), block in zip(locations, blocks, strict=True)))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    machine.r[4:8] = vector.arguments[:4]
    machine.write(DEFAULT_STACK + 16, vector.arguments[4], 4)
    inputs = [vector.function, *vector.arguments, len(vector.blocks)]
    for address, block in vector.blocks:
        machine.store(address, block)
        words = struct.unpack(f"<{len(block) // 4}I", block)
        inputs += [address, len(words), *words]
    machine.stores.clear()
    machine.execute(ENTRIES[vector.function], limit=1000)
    memory = b"".join(machine.bytes(address, len(block)) for address, block in vector.blocks)
    return {"result": machine.r[2] if vector.function not in (1, 2) else 0,
            "crc": zlib.crc32(memory), "stores": machine.stores, "calls": machine.trace}, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int],
          reference: dict) -> list[str]:
    del image, reference
    expected = [observation["result"], observation["crc"]]
    problems = [] if native == expected else [f"native {native} != {expected}"]
    allowed = ([(DP, DP + 12)] +
               [(HIT + 0x20 + 4 * i, HIT + 0x22 + 4 * i) for i in range(6)]
               if vector.function in (1, 2) else [])
    problems.extend(store_problems(observation["stores"], allowed))
    if observation["calls"]:
        problems.append("unexpected callback")
    return problems


def describe(image: Image, results: list) -> list[str]:
    counts = [0] * len(FUNCTIONS)
    errors = [0] * len(FUNCTIONS)
    for vector, observation, native in results:
        counts[vector.function] += 1
        errors[vector.function] += bool(check(image, vector, observation, native, observation))
    return [f"{name} [{start:08X},{stop:08X}) {(stop-start)//4} instructions "
            f"sha256={image.sha256(start, stop)}; {counts[i]} vectors; mismatches={errors[i]}"
            for i, (name, start, stop) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True))]
