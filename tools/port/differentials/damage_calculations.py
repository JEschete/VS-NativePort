"""D5 staged signed damage; real gem helper, controlled random/growth callbacks.

Undefined healing encodings are excluded from numeric comparison, not assigned a
synthetic value. The checked-live guard driver probes those refused paths.
"""

import random
import struct
import zlib
from typing import NamedTuple

from differential import DEFAULT_STACK, MASK, Image, Machine, store_problems

ID = "D5 F00409/F00410/F00411/F00421/F00433"
ENTRIES = (0x80081148, 0x800819D0, 0x800819FC, 0x800829B8, 0x8008388C)
ENDS = (0x800819D0, 0x800819FC, 0x80082144, 0x80082FF4, 0x800838BC)
FUNCTIONS = ("F00409", "F00410", "F00411", "F00421", "F00433")
ENTRY, END = ENTRIES[0], ENDS[0]
DRIVER = "damage_calculations_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\DamageCalculations.cpp",
           r"Native\Game\src\Battle\DamagePolicies.cpp")
VECTORS, SEED, MUTANT_VECTORS = 20480, 409410411421433, 2560
TABLE, BONUS, CRITICAL = 0x800F1928, 0x800E82F4, 0x800F1A0C
ACTION, SOURCE, TARGET = 0x80500000, 0x80501000, 0x80502000
ACTORS, STATS = 0x80600000, 0x80700000
BLOCKS = ((TABLE - 16, 200), (BONUS - 16, 56), (CRITICAL - 16, 36),
          (ACTION - 16, 84), (SOURCE - 16, 100), (TARGET - 16, 100),
          (ACTORS - 16, 0x430), (STATS - 16, 0x1984))
MUTANTS = {
    "F00409-fallback-type": {0x8008120C: 0x24020002},
    "F00409-six-affinities": {0x800812A8: 0x28820006},
    "F00409-unsigned-affinity": {0x80081284: 0x94A30184},
    "F00409-target-six-affinities": {0x8008136C: 0x28820006},
    "F00409-target-class-mask": {0x800813EC: 0x30420003},
    "F00409-signed-type-value": {0x800813C8: 0x82430145},
    "F00409-unsigned-PP-division": {0x8008148C: 0x0103001B},
    "F00409-no-factor-short": {0x8008150C: 0x00021402},
    "F00409-signed-shield-type": {0x80081664: 0x806302C4},
    "F00409-unsigned-attack-stat": {0x800817F0: 0x97A40022},
    "F00409-no-defense": {0x80081874: 0x01002821},
    "F00409-four-variance": {0x800818A4: 0x24040004},
    "F00409-wrong-status": {0x800818C0: 0x30420001},
    "F00409-critical-id-five": {0x800818EC: 0x2C420005},
    "F00409-no-critical-store": {0x80081938: 0},
    "F00409-no-attacker-growth": {0x8008196C: 0},
    "F00409-no-defender-growth": {0x80081990: 0},
    "F00410-apply-statuses": {0x800819DC: 0xAFA20014},
    "F00410-no-random-forward": {0x800819E4: 0xAFA00010},
    "F00411-zero-variance": {0x800820C8: 0x24040001},
    "F00411-unsigned-attack": {0x80082014: 0x97A40022},
    "F00411-no-defense": {0x80082098: 0x01002821},
    "F00411-signed-shield-type": {0x80081E94: 0x804302C4},
    "F00411-no-growth": {0x8008210C: 0},
    "F00421-no-defense": {0x80082F20: 0x01002821},
    "F00421-ten-average": {0x80082F50: 0x2404000A},
    "F00421-offset-four": {0x80082F58: 0x2442FFFC},
    "F00421-no-second-floor": {0x80082F74: 0},
    "F00421-no-growth": {0x80082FBC: 0},
    "F00433-ignore-statuses": {0x80083894: 0x24030000},
    "F00433-no-random-forward": {0x800838A4: 0xAFA00010},
}


class Vector(NamedTuple):
    function: int
    hit: int
    execute: int
    statuses: int
    draw: int
    mutation: int
    excluded_encoding: int
    blocks: tuple[bytes, ...]


def put(blocks: list[bytearray], address: int, value: int, size: int = 4) -> None:
    for (start, length), block in zip(BLOCKS, blocks, strict=True):
        if start <= address and address + size <= start + length:
            struct.pack_into({1: "<B", 2: "<H", 4: "<I"}[size], block,
                             address - start, value % (1 << (8 * size)))
            return
    raise AssertionError(f"unmapped {address:x}")


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 5, number // 5
    blocks = [bytearray(rng.randbytes(size)) for _, size in BLOCKS]
    narrow = (-32768, -1, 0, 1, 100, 32767)
    small = (lane // 32) % 4 != 0
    for owner in range(2):
        actor, stats = ACTORS + owner * 0x100, STATS + owner * 0x1000
        put(blocks, TABLE + owner * 4, actor)
        put(blocks, actor + 0x3C, stats)
        put(blocks, TABLE + (40 + owner) * 4, ACTORS + 0x200 + owner * 0x100)
        put(blocks, ACTORS + 0x220 + owner * 0x100, (lane >> (3 + owner)) & 3)
        put(blocks, stats + 0x957, 40 + owner, 1)
        put(blocks, stats + 0x37, (rng.randrange(32) << 3) | ((lane + owner) % 8), 1)
        put(blocks, stats + 0x948, (0, 2, MASK)[lane % 3])
        for offset in (0x22, 0x26):
            put(blocks, stats + offset, (100 if owner == 0 else 20) if small
                else rng.choice((0, 1, 32767, 32768, 65535, rng.randrange(65536))), 2)
        put(blocks, stats + 0x20, (0, 1, 9, 10, 100, 65535)[lane % 6], 2)
        put(blocks, stats + 0x54, lane % 3, 2)
        put(blocks, stats + 0x58, (lane // 4) % 12, 1)
        put(blocks, stats + 0x145, (0, 128, 255)[lane % 3], 1)
        put(blocks, stats + 0x14A, lane % 4, 1)
        for offset in (0x154, 0x158, 0x2B8, 0x2BC, 0x368, 0x36A):
            put(blocks, stats + offset, rng.randrange(-30, 31) if small else rng.choice(narrow), 2)
        for base in (0x164, 0x184, 0x2C8, 0x2E8, 0x370, 0x378, 0x388):
            count = 4 if base == 0x370 else 8
            for i in range(count):
                value = rng.randrange(-50, 101) if small else rng.choice(narrow)
                put(blocks, stats + base + 2 * i, value, 2)
        for i in range(4):
            put(blocks, stats + 0x2C4 + i, (0, 1, 128, 255)[(lane + i) % 4], 1)
        for offset in (0x14C, 0x150, 0x2B0, 0x2B4):
            current, maximum = ((0, 0), (100, 100), (101, 100), (65535, 1),
                                (65535, 65535), (1, 65535))[(lane + offset) % 6]
            put(blocks, stats + offset, current, 2)
            put(blocks, stats + offset + 2, maximum, 2)
        for limb in range(6):
            start = stats + 0xDC * limb
            for base, count in ((0x3A0, 4), (0x3A8, 8), (0x3F8, 4),
                                (0x400, 8), (0x420, 8)):
                for i in range(count):
                    put(blocks, start + base + i * 2,
                        rng.randrange(-30, 61) if small else rng.choice(narrow), 2)
            for offset in (0x3EC, 0x3F0):
                put(blocks, start + offset, rng.randrange(-20, 21) if small else rng.choice(narrow), 2)
            put(blocks, start + 0x3E8, (0, 100, 102, 65535)[lane % 4], 2)
            put(blocks, start + 0x3EA, (0, 100, 100, 1)[lane % 4], 2)
        for slot in range(3):
            put(blocks, stats + 0xCA + slot * 0x30, (0, 3, 4, 9)[(lane + slot) % 4], 1)
            put(blocks, stats + 0x232 + slot * 0x30, (0, 7, 8, 9)[(lane + slot) % 4], 1)
    if lane % 13 == 0:
        put(blocks, ACTORS + 0x13C, STATS)
    for category in range(12):
        put(blocks, BONUS + 2 * category, (0, 100, 32768, 65535)[(lane + category) % 4], 2)
    put(blocks, SOURCE, 0, 1)
    put(blocks, TARGET, 1, 1)
    put(blocks, TARGET + 1, (lane // 8) % 6, 1)
    put(blocks, SOURCE + 0x40, lane % 3)
    put(blocks, TARGET + 0x40, int(lane % 5 == 0))
    put(blocks, ACTION, (rng.getrandbits(32) & ~0xE00FF) | ((lane % 8) << 17) | (lane % 8))
    hit = (lane // 4) % 2
    excluded = -1
    for index in range(2):
        affinity = (lane + index) % 8
        # Excluded trials are counted separately, then replaced by a defined
        # encoded-six vector. They are never numerically compared as valid.
        source_drawn = bool((lane >> 3) & 1)
        if function == 3 and (affinity == 0 or (affinity == 1 and source_drawn)):
            if index == hit:
                excluded = affinity
            affinity = 6
        put(blocks, ACTION + 0x14 + index * 4,
            (affinity << 29) | ((lane % 4) << 27) | (((lane + index) % 32) << 22))
    return Vector(function, hit, (0, 1, MASK, 2)[lane % 4], (0, 1, MASK)[lane % 3],
                  lane % 11, (lane // 3) % 32, excluded, tuple(bytes(b) for b in blocks))


def mutate(machine: Machine, vector: Vector, stage: int, source: int, target: int) -> None:
    mode = vector.mutation
    if stage in (1, 2) and mode & 1:
        machine.write(source + 0x948, 2, 4)
        machine.write(source + 0x20, 65535 if stage == 2 else 0, 2)
        machine.write(ACTION, (machine.load(ACTION) & ~255) | (6 if mode & 16 else 5), 4)
    if stage == 1 and mode & 2:
        machine.write(ACTORS + 0x3C, STATS + 0x1000, 4)
        machine.write(ACTORS + 0x13C, STATS, 4)
        machine.write(SOURCE, 255, 1)
        machine.write(TARGET, 255, 1)
        machine.write(TARGET + 1, 128, 1)
    if stage == 1 and mode & 8:
        machine.write(TARGET + 0x40, 1, 4)
    if stage == 3 and mode & 4:
        machine.write(source + 0x37, 7, 1)
        machine.write(source + 0x368, 12345, 2)
    if stage == 4:
        machine.write(target + 0x36A, 23456, 2)


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    for (address, _), block in zip(BLOCKS, vector.blocks, strict=True):
        machine.store(address, block)
    source, target = (machine.load(ACTORS + offset) for offset in (0x3C, 0x13C))
    events = []

    def random_call(m: Machine, averaged: bool = False) -> None:
        bound = m.r[4]
        events.append([1 if not averaged else 2, bound, m.load(CRITICAL), 0, 0, 0])
        mutate(m, vector, 2 if bound == 100 else 1, source, target)
        m.r[2] = vector.draw % max(bound, 1)

    def growth(m: Machine, defender: bool) -> None:
        limb = m.load(m.r[29] + 16) if defender else 0
        events.append([4 if defender else 3, *m.r[4:8], limb])
        mutate(m, vector, 4 if defender else 3, source, target)
        m.r[2] = 0xDEADBEEF

    machine.stub(0x800427A8, random_call)
    machine.stub(0x80042860, lambda m: random_call(m, True))
    machine.stub(0x80080F78, lambda m: growth(m, False))
    machine.stub(0x80080C9C, lambda m: growth(m, True))
    machine.follow.update((ENTRIES[0], 0x8007FE5C))
    machine.r[4:8] = [ACTION, SOURCE, TARGET, vector.hit]
    machine.write(DEFAULT_STACK + 16, vector.execute, 4)
    machine.write(DEFAULT_STACK + 20, vector.statuses, 4)
    machine.stores.clear()
    machine.execute(ENTRIES[vector.function], limit=4000)
    data = b"".join(machine.bytes(address, size) for address, size in BLOCKS)
    inputs = [vector.function, vector.hit, vector.execute, vector.statuses,
              vector.draw, vector.mutation,
              *struct.unpack(f"<{sum(size for _, size in BLOCKS)//4}I", b"".join(vector.blocks))]
    return {"result": machine.r[2], "crc": zlib.crc32(data), "events": events,
            "stores": machine.stores}, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int],
          reference: dict) -> list[str]:
    del image, vector, reference
    expected = [observation["result"], observation["crc"],
                *(value for event in observation["events"] for value in event)]
    problems = [] if native == expected else [f"native {native} != {expected}"]
    allowed = [(DEFAULT_STACK - 0x88, DEFAULT_STACK),
               (CRITICAL, CRITICAL + 4), (ACTION, ACTION + 4),
               (SOURCE, SOURCE + 1), (TARGET, TARGET + 2), (TARGET + 0x40, TARGET + 0x44),
               (ACTORS + 0x3C, ACTORS + 0x40), (ACTORS + 0x13C, ACTORS + 0x140)]
    allowed += [(STATS + owner * 0x1000 + offset, STATS + owner * 0x1000 + offset + size)
                for owner in range(2) for offset, size in
                ((0x948, 4), (0x20, 2), (0x37, 1), (0x368, 2), (0x36A, 2))]
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    lines = [f"{name} [{start:08X},{end:08X}) {(end-start)//4} instructions "
             f"sha256={image.sha256(start, end)}; "
             f"{sum(v.function == i for v, _, _ in results)} defined vectors; "
             f"{sum(bool(check(image, v, o, n, o)) for v, o, n in results if v.function == i)} mismatches"
             for i, (name, start, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True))]
    for encoding in (0, 1):
        lines.append(f"#351 unsupported encoded {encoding}: "
                     f"{sum(v.excluded_encoding == encoding for v, _, _ in results)} excluded trials; "
                     "checked-live refusal evidence required separately")
    return lines
