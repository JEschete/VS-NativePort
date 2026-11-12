"""Six hit modifiers: real gem helper calls, no service stubs, read-only guarded state."""

import random
from dataclasses import dataclass
from operator import itemgetter

from differential import DEFAULT_STACK, MASK, Image, Machine, store_problems

ID = "F00339/F00340/F00342/F00343/F00347/F00348 (three disjoint spans)"
ENTRY, END = 0x8007E5E0, 0x8007E974
ENTRIES = (ENTRY, 0x8007E6A0, 0x8007E97C, 0x8007E99C, 0x8007F138, 0x8007F1A4)
ENDS = (0x8007E6A0, END, 0x8007E99C, 0x8007EA74, 0x8007F1A4, 0x8007F230)
FUNCTIONS = ("F00339", "F00340", "F00342", "F00343", "F00347", "F00348")
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "hit_modifiers_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\HitModifiers.cpp",)
VECTORS, SEED, MUTANT_VECTORS = 12288, 339340342343347348, 3072
TABLE, ACTORS, STATS = 0x800F1928, 0x80500000, 0x80600000
ACTION, SOURCE, TARGET, PREVIOUS = 0x80700000, 0x80700100, 0x80700200, 0x800F1A04
GUARD = 16
BLOCKS = ((TABLE - GUARD, 200),
          *((ACTORS + i * 0x100 - GUARD, 112) for i in range(3)),
          *((STATS + i * 0x1000 - GUARD, 2436) for i in range(2)),
          (ACTION - GUARD, 84), (SOURCE - GUARD, 100),
          (TARGET - GUARD, 100), (PREVIOUS - GUARD, 36))
OBSERVE_BYTES = itemgetter(*(address + offset for address, size in BLOCKS for offset in range(size)))
EFFECTS = (1, 2, 3, 5, 11, 15, 16, 17)
GEMS = (11, 12, 13, 14, 16, 18, 19, 21)
MUTANTS = {
    "F00339-wrong-owner-byte": {0x8007E5E4: 0x90A20956},
    "F00339-wrong-drawn-bit": {0x8007E604: 0x30420002},
    "F00339-wrong-spell-type": {0x8007E61C: 0x3C030004},
    "F00339-wrong-spell-gem": {0x8007E630: 0x24070003},
    "F00339-two-spell-slots": {0x8007E64C: 0x28620002},
    "F00339-wrong-physical-gem": {0x8007E664: 0x24070003},
    "F00339-two-physical-slots": {0x8007E680: 0x28620002},
    "F00339-bonus19": {0x8007E694: 0x24060013},
    "F00340-wrong-owner-byte": {0x8007E6A4: 0x90A20956},
    "F00340-wrong-drawn-bit": {0x8007E6C4: 0x30420002},
    "F00340-wrong-spell-type": {0x8007E6DC: 0x3C030004},
    "F00340-wrong-spell-gem": {0x8007E6EC: 0x240A0007},
    "F00340-two-spell-slots": {0x8007E70C: 0x28620002},
    "F00340-id39-excluded": {0x8007E728: 0x2C620027},
    "F00340-id53-included": {0x8007E734: 0x2462FFCB},
    "F00340-id223-excluded": {0x8007E73C: 0x2C4200A9},
    "F00340-id224-included": {0x8007E73C: 0x2C4200AB},
    "F00340-wrong-physical-gem": {0x8007E74C: 0x240A0004},
    "F00340-two-physical-slots": {0x8007E76C: 0x28620002},
    "F00340-no-base-bonus": {0x8007E780: 0},
    "F00340-wrong-hit-word": {0x8007E788: 0x8C420018},
    "F00340-wrong-effect-mask": {0x8007E790: 0x3042000F},
    "F00340-effect17-excluded": {0x8007E798: 0x2C620010},
    "F00340-effect1-wrong-gem": {0x8007E7C8: 0x2404000A},
    "F00340-effect2-wrong-gem": {0x8007E7FC: 0x2404000B},
    "F00340-effect3-wrong-gem": {0x8007E830: 0x2404000C},
    "F00340-effect5-wrong-gem": {0x8007E864: 0x2404000D},
    "F00340-effect11-wrong-gem": {0x8007E898: 0x2404000F},
    "F00340-effect15-wrong-gem": {0x8007E8CC: 0x24040011},
    "F00340-effect16-wrong-gem": {0x8007E900: 0x24040012},
    "F00340-effect17-wrong-gem": {0x8007E934: 0x24040014},
    "F00340-no-effect-bonus": {0x8007E964: 0},
    "F00342-zero-threshold254": {0x8007E98C: 0x240300FE},
    "F00342-success99": {0x8007E990: 0x24030063},
    "F00342-nonpositive-fails": {0x8007E988: 0x18400002},
    "F00343-base99": {0x8007E9BC: 0x24100063},
    "F00343-position-source-included": {0x8007E9D0: 0},
    "F00343-no-source-helper": {0x8007E9F4: 0},
    "F00343-no-attack-add": {0x8007EA04: 0},
    "F00343-position-target-included": {0x8007EA18: 0},
    "F00343-no-defense-subtract": {0x8007EA50: 0},
    "F00347-signed-risk": {0x8007F15C: 0x84430020},
    "F00347-offset149": {0x8007F164: 0x24630095},
    "F00347-wrong-multiplier": {0x8007F178: 0x00021040},
    "F00347-no-255-correction": {0x8007F198: 0},
    "F00348-wrong-limb-byte": {0x8007F1B8: 0x80C40002},
    "F00348-signed-evasion": {0x8007F1E4: 0x8063039F},
    "F00348-base254": {0x8007F1E8: 0x240400FE},
    "F00348-wrong-multiplier": {0x8007F200: 0x00021040},
}
# Check each effect's loop bound separately, not merely a shared presence helper.
for _effect, _address in zip(EFFECTS, (0x8007E7E4, 0x8007E818, 0x8007E84C, 0x8007E880,
                                      0x8007E8B4, 0x8007E8E8, 0x8007E91C, 0x8007E950),
                             strict=True):
    MUTANTS[f"F00340-effect{_effect}-two-slots"] = {_address: 0x28620002}


@dataclass(frozen=True)
class Vector:
    function: int
    action: int
    source: int
    target: int
    stats: int
    hit: int
    execute: int
    blocks: tuple[bytes, ...]


def put(data: bytearray, offset: int, value: int, size: int = 4) -> None:
    data[offset:offset + size] = (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 6, number // 6
    blocks = [bytearray(rng.randbytes(size)) for _, size in BLOCKS]
    table = blocks[0]
    for slot in range(42):
        put(table, GUARD + slot * 4, 0)
    for slot, actor in ((0, 0), (1, 1), (41, 2)):
        put(table, GUARD + slot * 4, ACTORS + actor * 0x100)
    drawn = [rng.choice((0, 1, 2, 3, MASK, MASK - 1)) for _ in range(3)]
    if lane < 256:
        drawn = [1, 1, 1]
    owners = [rng.choice((0, 1, 41)) for _ in range(2)]
    for actor in range(3):
        put(blocks[1 + actor], GUARD + 0x20, drawn[actor])
        put(blocks[1 + actor], GUARD + 0x3C, STATS + (actor % 2) * 0x1000)
    for index in range(2):
        stats = blocks[4 + index]
        put(stats, GUARD + 0x957, owners[index], 1)
        put(stats, GUARD + 0x956, 1 - index, 1)
        put(stats, GUARD + 0x20, rng.choice((0, 1, 502, 503, 504, 505, 506,
                                          32767, 32768, 65535, rng.randrange(65536))), 2)
        for limb in range(6):
            put(stats, GUARD + 0x39F + 0xDC * limb, rng.randrange(256), 1)
        for slot in range(3):
            put(stats, GUARD + 0xCA + 0x30 * slot, rng.choice((0, 1, 2, 3, 255)), 1)
        for slot in range(4):
            put(stats, GUARD + 0x232 + 0x30 * slot, rng.choice((0, 5, 6, 255, *GEMS)), 1)
    action = blocks[6]
    type_id = lane % 8
    action_id = (0, 39, 40, 53, 54, 223, 224, 255)[lane // 8 % 8]
    put(action, GUARD, (rng.getrandbits(32) & ~0xE00FF) | (type_id << 17) | action_id)
    hit = lane % 2
    effect = EFFECTS[lane // 2 % 8] if lane < 256 else rng.randrange(128)
    for index in range(2):
        put(action, GUARD + 0x14 + index * 4,
            (rng.getrandbits(32) & ~127) | (effect if index == hit else 0))
    # Presence in only the final slot detects every independent three-slot loop.
    if lane < 256:
        for index in range(2):
            stats = blocks[4 + index]
            for slot in range(3):
                put(stats, GUARD + 0xCA + 0x30 * slot, 0, 1)
                put(stats, GUARD + 0x232 + 0x30 * slot, 0, 1)
            put(stats, GUARD + 0xCA + 0x60, 2 if type_id == 1 else 1, 1)
            if lane // 16 % 2 == 0:
                put(stats, GUARD + 0x232 + 0x60, GEMS[lane // 2 % 8], 1)
                put(stats, GUARD + 0x232, 6 if type_id == 1 else 5, 1)
            else:
                put(stats, GUARD + 0x232 + 0x60, 6 if type_id == 1 else 5, 1)
                put(stats, GUARD + 0x232, GEMS[lane // 2 % 8], 1)
    for index in range(2):
        block = blocks[7 + index]
        put(block, GUARD, index, 1)
        put(block, GUARD + 1, rng.randrange(6), 1)
        put(block, GUARD + 2, rng.randrange(6), 1)
        put(block, GUARD + 0x40, 0 if lane < 256 else rng.choice((0, 0, 1, MASK)))
    previous = (0, 1, MASK, 0x80000000, 0x10000, rng.getrandbits(32))[lane % 6]
    put(blocks[9], GUARD, previous)
    source = SOURCE if lane < 256 or function == 4 else rng.choice((0, SOURCE, TARGET))
    target = TARGET if lane < 256 or function == 5 else rng.choice((0, SOURCE, TARGET))
    action_pointer = ACTION
    if lane >= 256 and lane % 8 == 0:
        # Both owner actors are undrawn: even a null action and invalid hit are unused.
        for actor in range(3):
            put(blocks[1 + actor], GUARD + 0x20, 2)
        action_pointer, hit = 0, MASK
    if function == 2:
        action_pointer = source = target = 0
        hit = rng.getrandbits(32)
    return Vector(function, action_pointer, source, target, STATS + (lane % 2) * 0x1000,
                  hit, rng.getrandbits(32), tuple(bytes(block) for block in blocks))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    for (address, _), block in zip(BLOCKS, vector.blocks, strict=True):
        machine.store(address, block)
    machine.r[4:8] = [vector.action, vector.source, vector.target, vector.hit]
    if vector.function < 2:
        machine.r[5:7] = [vector.stats, vector.hit]
    machine.write(DEFAULT_STACK + 16, vector.execute, 4)
    machine.stores.clear()
    machine.follow.update(ENTRIES[:2])
    machine.execute(ENTRIES[vector.function], limit=2000)
    data = b"".join(vector.blocks)
    inputs = [vector.function, vector.action, vector.source, vector.target, vector.stats,
              vector.hit, vector.execute,
              *(int.from_bytes(data[i:i + 4], "little") for i in range(0, len(data), 4))]
    return {"result": machine.r[2], "memory": bytes(OBSERVE_BYTES(machine.overlay)),
            "stores": list(machine.stores)}, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, vector, reference
    count = sum(size for _, size in BLOCKS) // 4
    if len(native) != count + 2:
        return ["truncated native output"]
    problems = [] if native[0] == 1 else ["native refused"]
    if native[1] != observation["result"]:
        problems.append(f"result {native[1]:08X} != {observation['result']:08X}")
    actual = b"".join(word.to_bytes(4, "little") for word in native[2:])
    if actual != observation["memory"]:
        problems.append("guarded input memory changed")
    return problems + store_problems(observation["stores"], [(DEFAULT_STACK - 0x28, DEFAULT_STACK)])


def describe(image: Image, results: list) -> list[str]:
    lines = [f"{name} [{start:08X},{end:08X}): {(end-start)//4} instructions; "
             f"sha256={image.sha256(start, end)}"
             for name, start, end in zip(FUNCTIONS, ENTRIES, ENDS, strict=True)]
    lines.append("Three disjoint spans 229+62+62=353 instructions; no gaps or F00349 claimed. "
                 "SLUS supplies original F00340 jump table at 80068A48. F00343 executes real "
                 "F00339/F00340; no helper stubs. Full guarded read-only actor/stats/action/hit/"
                 "global/table images; only F00343 stack stores permitted. Signed helper returns "
                 "are original instructions; unreachable U8 chain rate==255 is not a claimed mutant.")
    for function, name in enumerate(FUNCTIONS):
        lines.append(f"{name}: {sum(v.function == function for v, _, _ in results)} vectors.")
    return lines
