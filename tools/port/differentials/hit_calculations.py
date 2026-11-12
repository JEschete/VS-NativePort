"""Five active hit/stat calculations; sentinels have no invented return model."""

import random
from dataclasses import dataclass

from differential import DEFAULT_STACK, MASK, Image, Machine, load_case, store_problems

BASE = load_case("hit_modifiers")
ID = "F00344/F00345/F00346/F00349/F00435 (five disjoint spans, 603 instructions)"
ENTRY, END = 0x8007EA74, 0x8007F138
ENTRIES = (ENTRY, 0x8007ECA8, 0x8007EEFC, 0x8007F230, 0x800838EC)
ENDS = (0x8007ECA8, 0x8007EEFC, END, 0x8007F434, 0x80083990)
FUNCTIONS = ("F00344", "F00345", "F00346", "F00349", "F00435")
MODULES = BASE.MODULES
DRIVER = "hit_calculations_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\HitCalculations.cpp",
           r"Native\Game\src\Battle\HitModifiers.cpp")
VECTORS, SEED, MUTANT_VECTORS = 15360, 344345346349435, 5120
BLOCKS, GUARD = BASE.BLOCKS, BASE.GUARD
ACTION, SOURCE, TARGET, STATS, TABLE, ACTORS = (
    BASE.ACTION, BASE.SOURCE, BASE.TARGET, BASE.STATS, BASE.TABLE, BASE.ACTORS)
PREREQUISITE, LAST = 0x800F1A00, 0x800F1A08
HIT_TABLE, STAT_TABLE, CALLBACK = 0x800E8234, 0x800E830C, 0x80800000
MUTANTS = {
    "F00344-five-armor": {0x8007EB0C: 0x29220005},
    "F00344-wrong-drawn-bit": {0x8007EB3C: 0x30420002},
    "F00344-no-source-weapon": {0x8007EB58: 0},
    "F00344-no-target-weapon": {0x8007EB98: 0},
    "F00344-unsigned-defense": {0x8007EBC4: 0x9042039C},
    "F00344-signed-risk": {0x8007EB9C: 0x86440020},
    "F00344-base99": {0x8007EC20: 0x26100063},
    "F00344-variance10": {0x8007EC28: 0x2404000A},
    "F00344-no-attack-gem": {0x8007EC48: 0},
    "F00344-no-defense-gem": {0x8007EC64: 0},
    "F00344-no-negative-floor": {0x8007EC74: 0},
    "F00344-no-255-correction": {0x8007EC80: 0},
    "F00345-signed-base": {0x8007ED10: 0x86450026},
    "F00345-five-armor": {0x8007ED40: 0x29220005},
    "F00345-wrong-drawn-bit": {0x8007ED70: 0x30420002},
    "F00345-no-source-weapon": {0x8007ED8C: 0},
    "F00345-no-target-weapon": {0x8007EDCC: 0},
    "F00345-unsigned-defense": {0x8007EDE8: 0x9042039C},
    "F00345-unsigned-hp": {0x8007EE14: 0x96240018},
    "F00345-unsigned-maxhp": {0x8007EE30: 0x9622001A},
    "F00345-wrong-risk": {0x8007EDF8: 0x24030063},
    "F00345-variance10": {0x8007EE7C: 0x2404000A},
    "F00345-no-attack-gem": {0x8007EE9C: 0},
    "F00345-no-defense-gem": {0x8007EEB8: 0},
    "F00345-no-negative-floor": {0x8007EEC8: 0},
    "F00345-no-255-correction": {0x8007EED4: 0},
    "F00346-five-armor": {0x8007EF94: 0x29220005},
    "F00346-wrong-drawn-bit": {0x8007EFC4: 0x30420002},
    "F00346-no-source-weapon": {0x8007EFE0: 0},
    "F00346-no-target-weapon": {0x8007F020: 0},
    "F00346-unsigned-defense": {0x8007F04C: 0x9042039C},
    "F00346-signed-cost": {0x8007F0A4: 0x82630003},
    "F00346-base129": {0x8007F0A8: 0x26020081},
    "F00346-no-cost": {0x8007F0B0: 0x00408021},
    "F00346-variance10": {0x8007F0B8: 0x2404000A},
    "F00346-no-attack-gem": {0x8007F0D8: 0},
    "F00346-no-defense-gem": {0x8007F0F4: 0},
    "F00346-no-negative-floor": {0x8007F104: 0},
    "F00346-no-255-correction": {0x8007F110: 0},
    "F00349-wrong-exemption": {0x8007F290: 0x2402007F},
    "F00349-exempt99": {0x8007F2A0: 0x24100063},
    "F00349-wrong-hit": {0x8007F2B4: 0x8C420018},
    "F00349-wrong-selector-mask": {0x8007F2D0: 0x3042000C},
    "F00349-wrong-spell": {0x8007F2F8: 0x3C030004},
    "F00349-no-spell-block": {0x8007F338: 0},
    "F00349-no-prerequisite": {0x8007F350: 0},
    "F00349-wrong-double-status": {0x8007F398: 0x30420008},
    "F00349-no-double": {0x8007F3A4: 0},
    "F00349-cap99": {0x8007F3B4: 0x24100063},
    "F00349-wrong-threshold-sentinel": {0x8007F3BC: 0x240200FE},
    "F00349-no-threshold-store": {0x8007F3D0: 0},
    "F00349-random99": {0x8007F3D8: 0x24040063},
    "F00349-player-bonus9": {0x8007F40C: 0x26100009},
    "F00349-unsigned-compare": {0x8007F410: 0x0070102B},
    "F00435-wrong-exemption": {0x8008392C: 0x2402007F},
    "F00435-exempt998": {0x8008393C: 0x240203E6},
    "F00435-wrong-hit": {0x80083948: 0x9442001A},
    "F00435-wrong-selector-mask": {0x80083958: 0x3042000F},
    "F00435-unsigned-return": {0x80083980: 0x00021402},
    "F00435-no-last-value": {0x80083984: 0},
}


@dataclass(frozen=True)
class Vector:
    function: int
    action: int
    source: int
    target: int
    hit: int
    execute: int
    result: int
    draw: int
    mutation: int
    blocks: tuple[bytes, ...]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 5, number // 5
    seed = BASE.vector(rng, number * 6 + 3)
    blocks = [bytearray(block) for block in seed.blocks]
    put = BASE.put
    for index in range(2):
        stats = blocks[4 + index]
        for offset in (0x26, 0x2A):
            put(stats, GUARD + offset, rng.choice((0, 100, 65535, rng.randrange(65536))), 2)
        for offset in (0x36C, 0x36A, 0x15C, 0x158, 0x2C0, 0x2BC,
                       *(0x3F4 + 0xDC * i for i in range(6)),
                       *(0x3F0 + 0xDC * i for i in range(6))):
            put(stats, GUARD + offset, rng.choice((0, -32768, 32767, rng.randrange(-300, 301))), 2)
        for limb in range(6):
            put(stats, GUARD + 0x39C + 0xDC * limb, rng.randrange(256), 1)
        put(stats, GUARD + 0x18, rng.choice((0, 1, -1, -32768, 32767)), 2)
        put(stats, GUARD + 0x1A, rng.choice((0, 1, -1, -32768, 32767)), 2)
        put(stats, GUARD + 0x948, rng.choice((0, 4, 8, MASK, 0x80000000)))
        put(blocks[7 + index], GUARD + 0x40, rng.choice((0, 0, 1, MASK)))
        put(blocks[7 + index], GUARD + 2, rng.choice((0, 254, 255, 33)), 1)
    action, source, hit = ACTION, SOURCE, lane % 2
    put(blocks[9], GUARD - 4, rng.choice((0, 1, MASK)))
    for i in range(2):
        old = int.from_bytes(blocks[6][GUARD + 0x14 + 4*i:GUARD + 0x18 + 4*i], "little")
        put(blocks[6], GUARD + 0x14 + 4*i,
            (old & ~0x3FE000) | (rng.randrange(1, 8) << 13) | (rng.randrange(1, 27) << 16))
    result = rng.choice((0, 1, 45, 50, 99, 100, 101, 254, 255, 256, MASK,
                         0x80000000, 0x40000000, 0x40000001, 0xFFFF8000, rng.getrandbits(32)))
    draw = rng.choice((0, 1, 9, 10, 49, 50, 59, 99, 100, MASK, 0x80000000))
    execute, mutation = rng.choice((0, 1, MASK)), lane % 8
    # Exact 255 and adjacent values must survive the final correction independently.
    if function < 3 and lane % 16 < 4:
        for index in range(2):
            stats = blocks[4 + index]
            for offset in (0x20, 0x26, 0x2A, 0x36C, 0x36A, 0x15C, 0x158, 0x2C0, 0x2BC,
                           *(0x3F4 + 0xDC * i for i in range(6)),
                           *(0x3F0 + 0xDC * i for i in range(6))):
                put(stats, GUARD + offset, 0, 2)
            for limb in range(6):
                put(stats, GUARD + 0x39C + 0xDC * limb, 0, 1)
            put(stats, GUARD + 0x1A, 1, 2)
            for gem in range(3):
                put(stats, GUARD + 0xCA + 0x30 * gem, 0, 1)
                put(stats, GUARD + 0x232 + 0x30 * gem, 0, 1)
        goal = (0, 254, 255, 256)[lane % 4]
        offset = (100, 0, 130)[function]
        if goal >= offset:
            put(blocks[4], GUARD + (0x26 if function == 1 else 0x2A), goal-offset, 2)
        else:
            put(blocks[5], GUARD + 0x2A, offset-goal, 2)
        put(blocks[6], GUARD + 3, 0, 1)
        execute = mutation = 0
    if function >= 3 and lane % 12 == 0:
        put(blocks[8], GUARD + 0x40, 0)
        put(blocks[5], GUARD + 0x957, 128, 1)
        action, hit = 0, MASK
    if function == 3 and lane % 12 == 1:
        result, source = 255, 0
        mutation = 0
        # Nonspell leaves the callback's failure threshold untouched.
        old = int.from_bytes(blocks[6][GUARD:GUARD+4], "little")
        put(blocks[6], GUARD, old & ~0xE0000)
    return Vector(function, action, source, TARGET, hit, execute, result, draw,
                  mutation, tuple(bytes(block) for block in blocks))


def mutate(machine: Machine, vector: Vector, random_call: bool) -> None:
    mode = vector.mutation
    if vector.function < 3:
        if mode & 1:
            for owner in range(2):
                address = STATS + owner * 0x1000
                machine.write(address + 0x2A, 60000, 2)
                machine.write(address + 0xCA + 0x60, 2, 1)
                machine.write(address + 0x232 + 0x60, 6, 1)
                machine.write(address + 0x957, 41, 1)
            machine.write(ACTORS + 0x200 + 0x20, 1, 4)
            machine.write(ACTION, (machine.load(ACTION) & ~0xE0000) | 0x20000, 4)
        if mode & 2:
            machine.write(ACTORS + 0x3C, STATS + 0x1000, 4)
            machine.write(ACTORS + 0x100 + 0x3C, STATS, 4)
            machine.write(SOURCE, 1, 1)
            machine.write(TARGET, 0, 1)
        return
    if not random_call:
        if mode & 1:
            machine.write(TARGET, 0, 1)
            machine.write(TARGET + 0x40, 1, 4)
            machine.write(STATS + 0x948, MASK, 4)
            machine.write(ACTION, (machine.load(ACTION) & ~0xE0000) | 0x20000, 4)
        if mode & 2:
            machine.write(PREREQUISITE, 0, 4)
        machine.write(LAST, 0x12345678, 4)
    elif mode & 4:
        machine.write(SOURCE, 0, 1)
        machine.write(SOURCE + 0x40, 0, 4)
        machine.write(TARGET + 2, 77, 1)
        machine.write(PREREQUISITE, 0, 4)


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    for (address, _), block in zip(BLOCKS, vector.blocks, strict=True):
        machine.store(address, block)
    events = []

    def random_call(m: Machine, kind: int) -> None:
        events.append([kind, m.r[4], m.load(TARGET + 2, 1), 0, 0])
        mutate(m, vector, True)
        m.r[2] = vector.draw

    def calculator(m: Machine, selector: int, kind: int) -> None:
        events.append([kind, selector, m.r[7], m.load(m.r[29] + 16), m.load(TARGET + 2, 1)])
        if m.r[4:7] != [vector.action, vector.source, vector.target]:
            raise AssertionError("callback arguments changed")
        mutate(m, vector, False)
        m.r[2] = vector.result

    machine.stub(0x80042860, lambda m: random_call(m, 1))
    machine.stub(0x800427A8, lambda m: random_call(m, 2))
    for table, count, kind in ((HIT_TABLE, 8, 3), (STAT_TABLE, 27, 4)):
        for selector in range(count):
            address = CALLBACK + kind * 0x100 + selector * 4
            machine.write(table + selector * 4, address, 4)
            machine.stub(address, lambda m, s=selector, k=kind: calculator(m, s, k))
    machine.r[4:8] = [vector.action, vector.source, vector.target, vector.hit]
    machine.write(DEFAULT_STACK + 16, vector.execute, 4)
    machine.stores.clear()
    machine.follow.update(BASE.ENTRIES[:2])
    machine.execute(ENTRIES[vector.function], limit=3000)
    data = b"".join(vector.blocks)
    inputs = [vector.function, vector.action, vector.source, vector.target,
              vector.hit, vector.execute, vector.result, vector.draw, vector.mutation,
              *(int.from_bytes(data[i:i+4], "little") for i in range(0, len(data), 4))]
    return {"result": machine.r[2], "memory": bytes(BASE.OBSERVE_BYTES(machine.overlay)),
            "events": events, "stores": list(machine.stores)}, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, vector, reference
    count = sum(size for _, size in BLOCKS) // 4
    if len(native) < count + 3:
        return ["truncated output"]
    problems = [] if native[0] == 1 else ["native refused"]
    if native[1] != observation["result"]:
        problems.append(f"result {native[1]:08X} != {observation['result']:08X}")
    memory = b"".join(word.to_bytes(4, "little") for word in native[2:2+count])
    if memory != observation["memory"]:
        problems.append("guarded memory differs")
    calls = native[3+count:]
    if calls != [value for event in observation["events"] for value in event]:
        problems.append(f"events {calls} != {observation['events']}")
    allowed = [(DEFAULT_STACK-0x60, DEFAULT_STACK), (PREREQUISITE, PREREQUISITE+4),
               (LAST, LAST+4), (SOURCE, SOURCE+1), (SOURCE+0x40, SOURCE+0x44),
               (TARGET, TARGET+3), (TARGET+0x40, TARGET+0x44), (ACTION, ACTION+4),
               (ACTORS+0x3C, ACTORS+0x40), (ACTORS+0x13C, ACTORS+0x140),
               (ACTORS+0x220, ACTORS+0x224), (STATS+0x948, STATS+0x94C)]
    allowed += [(STATS+owner*0x1000+offset, STATS+owner*0x1000+offset+size)
                for owner in range(2) for offset, size in ((0x2A, 2), (0x12A, 1),
                                                          (0x292, 1), (0x957, 1))]
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    return [f"{name} [{start:08X},{end:08X}): {(end-start)//4} instructions; "
            f"sha256={image.sha256(start, end)}; "
            f"{sum(v.function == i for v, _, _ in results)} vectors"
            for i, (name, start, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True))]
