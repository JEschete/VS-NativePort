"""Four disjoint original status lifecycle routines; dependent services are explicit stubs."""

import random
import zlib
from dataclasses import dataclass

from differential import DEFAULT_STACK, MASK, Image, Machine, store_problems

ID = "F00254/F00517/F00520/F00523 (four disjoint spans)"
ENTRY, END = 0x8007C31C, 0x8007C348
ENTRIES = (ENTRY, 0x80086754, 0x80086FA8, 0x80087EF4)
ENDS = (END, 0x80086EA4, 0x80087578, 0x800882F4)
FUNCTIONS = ("F00254", "F00517", "F00520", "F00523")
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "status_lifecycle_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\StatusLifecycle.cpp",
           r"Native\Game\src\Battle\StatusHelpers.cpp",
           r"Native\Game\src\Items\EquipmentAffinity.cpp")
VECTORS, SEED, MUTANT_VECTORS = 8192, 254517520523, 1536
STATS, PLAYER, PLAYER_POINTER = 0x80500000, 0x80502000, 0x800F19FC
SIZE, GUARD = 0x964, 16
PARAMS, FLAGS = 0x8004EECC, 0x800E8204
DURATION_OFFSETS = (0x54, 0xB4, 0x134, 0xD4, 0xE6, 0x116)
# (equipment part, current rows, modifiers, number of contributing parts)
EQUIPMENT = ((0x54, 0x164, 0x1A4, 4), (0x1EC, 0x2C8, 0x308, 4),
             *((0x3B8 + i * 0xDC, 0x400 + i * 0xDC, 0x440 + i * 0xDC, 1)
               for i in range(6)))
MASKS = (0, MASK, 0x60, 0x180, 0x600, 0x7E0, 0x810, 0x1FF80000, 0x2000,
         0x8000, 0x24000, 0x10000, 0x8010, 0x800, 0x180000, 0x80000,
         0x100000, 0x20000000, 0x200000, 0x400000, 0x800000, 0x1000000,
         0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x600000, 0x6000000,
         0x10, 0x4000, 0x20000, 0x1F, 0x8810)
EDGES16 = (0, 1, 2, 9, 10, 19, 20, 49, 50, 99, 100, 32767, 32768, 65535)
MUTANTS = {
    "F00254-removes-one": {0x8007C334: 0x24040001},
    "F00254-wrong-stats-owner": {0x8007C32C: 0x8C450038},
    "F00517-does-not-clear-statuses": {0x8008679C: 0},
    "F00517-clears-shared-attribute-timer": {0x800867A4: 0},
    "F00517-wrong-equipment-timer-mask": {0x800867B8: 0x3C031FE8},
    "F00517-no-curse-restore": {0x800868B4: 0x10000008},
    "F00517-zero-mobility-dereferenced": {0x800868F0: 0},
    "F00517-restores-leg-to-one": {0x8008692C: 0x24020001},
    "F00517-missing-second-speed-call": {0x800869F4: 0},
    "F00517-five-armor-restores": {0x80086A74: 0x2A220005},
    "F00517-growth-limit-51": {0x80086B1C: 0x28420033},
    "F00517-growth-draw-16": {0x80086B2C: 0x24040010},
    "F00517-decay-boundary-99": {0x80086B74: 0x2842FF9E},
    "F00517-decay-draw-8": {0x80086B84: 0x24040008},
    "F00517-enemy-growth-labels": {0x80086BA8: 0},
    "F00517-negative-label-positive": {0x80086BB8: 0x24060000},
    "F00517-missing-weapon-modifier-clear": {0x80086BD0: 0},
    "F00517-missing-shield-rebuild": {0x80086D40: 0},
    "F00517-five-armor-rebuilds": {0x80086E5C: 0x2A220005},
    "F00517-missing-armor-flush": {0x80086E6C: 0},
    "F00520-does-not-add-statuses": {0x80086FC8: 0},
    "F00520-wrong-attribute-duration": {0x80086FD8: 0x9042EF80},
    "F00520-no-poison-precedence": {0x8008704C: 0x10000004},
    "F00520-up-reverses-to-down": {0x800870CC: 0x00431023},
    "F00520-curse-does-not-override": {0x8008718C: 0x10000018},
    "F00520-speed-from-requested": {0x800871F8: 0x02201821},
    "F00520-signed-base-speed": {0x8008722C: 0x8202002E},
    "F00520-half-speed-divide-four": {0x80087214: 0x00021082},
    "F00520-no-movement-call": {0x80087258: 0},
    "F00520-negative-gear-not-clamped": {0x800872A8: 0},
    "F00520-five-armor-boosts": {0x800873D0: 0x28A20005},
    "F00520-analyze-clears-neighbors": {0x800873F4: 0x00601021},
    "F00520-wrong-element-counterpart": {0x8008747C: 0x24060002},
    "F00520-no-armor-enchantment": {0x8008755C: 0},
    "F00523-timer-subtract-two": {0x80087F18: 0x2442FFFE},
    "F00523-loses-aggregate-expiry": {0x80087FC4: 0x00408821},
    "F00523-does-not-reset-poison": {0x80088038: 0},
    "F00523-poison-floor-zero": {0x80088078: 0x24070000},
    "F00523-poison-sign-lost": {0x8008809C: 0x24060000},
    "F00523-regen-uses-current-hp": {0x800880CC: 0x96040018},
    "F00523-regen-upper-clamp-before-wrap": {0x80088118: 0x00042402},
    "F00523-regen-negative-limb-floor": {0x80088148: 0},
    "F00523-regen-five-limbs": {0x80088180: 0x28A20005},
    "F00523-either-leg-rule-lost": {0x80088224: 0},
    "F00523-low-status-mobility-guard-lost": {0x80088284: 0},
    "F00523-low-status-wrong-limb": {0x8008828C: 0x86020398},
    "F00523-missing-expiry-label": {0x800882CC: 0},
    "F00523-missing-removal": {0x800882D8: 0},
}


@dataclass(frozen=True)
class Vector:
    function: int
    requested: int
    mutations: int
    durations: tuple[int, ...]
    flags: tuple[int, ...]
    guards: tuple[int, ...]
    draws: tuple[int, ...]
    stats: bytes
    globals: tuple[tuple[int, bytes], ...]


def put(block: bytearray, offset: int, value: int, size: int = 4) -> None:
    block[offset:offset + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")


def words(block: bytes) -> list[int]:
    return [int.from_bytes(block[i:i + 4], "little") for i in range(0, len(block), 4)]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 4, number // 4
    stats = bytearray(rng.randbytes(SIZE + 2 * GUARD))
    requested = MASKS[lane % len(MASKS)] if lane < 256 or rng.random() < 0.6 else rng.getrandbits(32)
    active = rng.choice((0, requested, MASK, *MASKS)) if rng.random() < 0.8 else rng.getrandbits(32)
    if function == 3:
        active = MASKS[lane % len(MASKS)] if lane < 256 else active
    for offset in (0x18, 0x1A, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C):
        put(stats, GUARD + offset, rng.choice(EDGES16) if lane < 256 else rng.getrandbits(16), 2)
    for offset in (0x2E, 0x2F):
        put(stats, GUARD + offset, rng.choice((0, 1, 3, 4, 127, 128, 204, 205, 255)), 1)
    for offset in (0x34, 0x35):
        put(stats, GUARD + offset, rng.randrange(6), 1)
    put(stats, GUARD + 0x957, rng.choice((0, 0, 0, 1, 41, 128, 255)), 1)
    put(stats, GUARD + 0x948, active)
    for index in range(6):
        put(stats, GUARD + 0x94C + index, rng.choice((0, 1, 1, 2, 255)), 1)
        put(stats, GUARD + 0x398 + index * 0xDC, rng.choice(EDGES16), 2)
        put(stats, GUARD + 0x39A + index * 0xDC, rng.choice(EDGES16), 2)
    for part, _, _, _ in EQUIPMENT:
        put(stats, GUARD + part, rng.choice((0, 0, 1, 256, 65535)), 2)
        for affinity in range(8):
            put(stats, GUARD + part + 0x28 + affinity, rng.choice((-128, -101, -100, -99, 0, 49, 50, 51, 127)), 1)
    # Deterministic edge lanes ensure shared-timer masks and signed store/clamp edges are reached.
    if function == 3 and lane % 8 in (0, 1):
        put(stats, GUARD + 0x948, 0x20000 if lane % 8 == 0 else 0x24000)
        put(stats, GUARD + 0x94C + 5, 1, 1)
        put(stats, GUARD + 0x18, 32767 if lane % 8 == 0 else 0, 2)
        put(stats, GUARD + 0x1A, 32767 if lane % 16 == 0 else 0, 2)
    draws = tuple(0 if lane % 3 == 0 else rng.getrandbits(32) for _ in range(32))
    durations = tuple(rng.choice((0, 1, 2, 15, 255, 256, 257, 65535)) for _ in range(6))
    params = bytearray(rng.randbytes(0x200 + 2 * GUARD))
    for offset, duration in zip(DURATION_OFFSETS, durations, strict=True):
        put(params, GUARD + offset, duration, 2)
    flags = (1, 2, 4, 8) if lane % 2 == 0 else tuple(1 << rng.randrange(4) for _ in range(4))
    limb_flags = bytearray(rng.randbytes(16 + 2 * GUARD))
    for i, flag in enumerate(flags):
        put(limb_flags, GUARD + i * 4, flag)
    owner = bytearray(rng.randbytes(0x50 + 2 * GUARD))
    put(owner, GUARD + 0x3C, STATS)
    pointer = bytearray(rng.randbytes(4 + 2 * GUARD))
    put(pointer, GUARD, PLAYER)
    blocks = ((PARAMS - GUARD, bytes(params)), (FLAGS - GUARD, bytes(limb_flags)),
              (PLAYER - GUARD, bytes(owner)), (PLAYER_POINTER - GUARD, bytes(pointer)))
    return Vector(function, requested, lane % 8, durations, flags,
                  tuple(rng.getrandbits(32) for _ in range(4)), draws, bytes(stats), blocks)


def rebuild(machine: Machine, equipment: int) -> None:
    part, rows, modifiers, count = EQUIPMENT[equipment]
    # Weapon skips the grip; shield's four parts are contiguous. These are service stubs,
    # not additional executed ranges or a second interpreter.
    parts = (part, part + 0x60, part + 0x90, part + 0xC0) if equipment == 0 else tuple(
        part + i * 0x30 for i in range(count))
    for size, offset, row_offset in ((6, 0x20, 0), (7, 0x28, 0x20)):
        for index in range(size):
            total = sum(machine.load(STATS + p + offset + index, 1, True) for p in parts)
            for row in range(2):
                value = total
                if row == 0 and row_offset:
                    value += machine.load(STATS + modifiers + index * 2, 2, True)
                machine.write(STATS + rows + row_offset + row * 16 + index * 2, value, 2)


def enchant(machine: Machine, equipment: int, increased: int, decreased: int, amount: int) -> None:
    part, rows, modifiers, _ = EQUIPMENT[equipment]
    if machine.load(STATS + part, 2) == 0:
        return
    for index, delta in ((increased, amount), (decreased, -amount)):
        at = STATS + rows + 0x20 + index * 2
        machine.write(at, machine.load(at, 2) + delta, 2)
    machine.write(STATS + modifiers + increased * 2, amount, 2)
    machine.write(STATS + modifiers + decreased * 2, -amount, 2)


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    machine.store(STATS - GUARD, vector.stats)
    for address, data in vector.globals:
        machine.store(address, data)
    next_draw = 0

    def observe(m: Machine, tag: int, args: tuple = ()) -> None:
        m.trace.append((tag, *args, *([0] * (4 - len(args))),
                        zlib.crc32(m.bytes(STATS - GUARD, len(vector.stats)))))
        if vector.mutations & 1 and tag in (0, 1, 3):
            m.write(STATS + 0x18, m.load(STATS + 0x18, 2) + 1, 2)
            for limb in range(6):
                at = STATS + 0x39E + limb * 0xDC
                m.write(at, m.load(at, 1) + 1, 1)
        if vector.mutations & 2 and tag == 2:
            for offset, mask in ((0x26, 0xAAAA), (0x158, 0x5555)):
                m.write(STATS + offset, m.load(STATS + offset, 2) ^ mask, 2)
        if vector.mutations & 4 and tag == 5:
            m.write(STATS + 0x24, m.load(STATS + 0x24, 2) ^ 0x1234, 2)
            m.write(STATS + 0x948, m.load(STATS + 0x948) ^ 0x40000000, 4)
        m.r[2] = 0xD00DFEED  # Discarded service return values are not lifecycle outputs.

    def random_draw(m: Machine, tag: int) -> None:
        nonlocal next_draw
        bound = m.r[4]
        observe(m, tag, (bound,))
        m.r[2] = vector.draws[next_draw % len(vector.draws)] % bound
        next_draw += 1

    def rebuild_call(m: Machine, equipment: int) -> None:
        if equipment == 2:
            equipment += m.r[5]
        observe(m, 6 + min(equipment, 2), (equipment - 2,) if equipment >= 2 else ())
        rebuild(m, equipment)

    def enchant_call(m: Machine, armor: bool) -> None:
        increase, decrease, amount = m.r[5:8]
        observe(m, 10 if armor else 9, (increase, decrease, amount))
        for equipment in range(1, 8) if armor else (0,):
            enchant(m, equipment, increase, decrease, amount)

    machine.stub(0x800427A8, lambda m: random_draw(m, 0))
    machine.stub(0x80042860, lambda m: random_draw(m, 1))
    machine.stub(0x800E685C, lambda m: observe(m, 2, tuple(m.r[4:7])))
    machine.stub(0x80096768, lambda m: observe(m, 3, tuple(m.r[4:7])))
    machine.stub(0x80093B68, lambda m: observe(m, 4, tuple(m.r[4:8])))
    machine.stub(0x80093FEC, lambda m: observe(m, 5, tuple(m.r[4:8])))
    machine.stub(0x8007E380, lambda m: rebuild_call(m, 0))
    machine.stub(0x8007E2AC, lambda m: rebuild_call(m, 1))
    machine.stub(0x8007E200, lambda m: rebuild_call(m, 2))
    machine.stub(0x80086EA4, lambda m: enchant_call(m, False))
    machine.stub(0x80086EF4, lambda m: enchant_call(m, True))
    machine.stub(0x800971D4, lambda m: observe(m, 11))
    machine.follow.add(ENTRIES[1])
    if vector.function in (1, 2):
        machine.r[4:6] = [vector.requested, STATS]
    elif vector.function == 3:
        machine.r[4] = STATS
    machine.execute(ENTRIES[vector.function], limit=5000)
    inputs = [vector.function, vector.requested, vector.mutations, *vector.durations, *vector.flags,
              *vector.guards, *vector.draws, *words(vector.stats)]
    return {"stats": machine.bytes(STATS - GUARD, len(vector.stats)), "trace": tuple(machine.trace),
            "stores": list(machine.stores),
            "globals": tuple((at, machine.bytes(at, len(data))) for at, data in vector.globals)}, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    count = len(vector.stats) // 4
    if len(native) < 3 + count:
        return ["truncated native output"]
    problems = [] if native[:2] == [0, 1] else ["native refusal or damaged state guards"]
    actual = b"".join(word.to_bytes(4, "little") for word in native[2:2 + count])
    if actual != observation["stats"]:
        offsets = [hex(i - GUARD) for i in range(len(actual)) if actual[i] != observation["stats"][i]]
        problems.append("actor bytes differ: " + ",".join(offsets[:18]))
    calls = tuple(tuple(native[i:i + 6]) for i in range(3 + count, len(native), 6))
    if len(calls) != native[2 + count] or calls != observation["trace"]:
        first = next((i for i, pair in enumerate(zip(calls, observation["trace"], strict=False))
                      if pair[0] != pair[1]), min(len(calls), len(observation["trace"])))
        problems.append(f"calls/snapshots at {first}: {calls[first:first + 1]} != "
                        f"{observation['trace'][first:first + 1]}")
    if observation["globals"] != vector.globals:
        problems.append("BSS/rule/owner bytes changed")
    allowed = [(DEFAULT_STACK - 0x80, DEFAULT_STACK), (STATS + 0x18, STATS + 0x1C),
               (STATS + 0x22, STATS + 0x2E), (STATS + 0x31, STATS + 0x32),
               (STATS + 0x33, STATS + 0x34), (STATS + 0x948, STATS + 0x952),
               (STATS + 0x954, STATS + 0x958)]
    for equipment, (part, rows, modifiers, _) in enumerate(EQUIPMENT):
        attributes = rows - (0x14 if equipment >= 2 else 0x10)
        allowed.extend(((STATS + part + 0x28, STATS + part + 0x30),
                        (STATS + attributes, STATS + attributes + 6),
                        (STATS + rows, STATS + modifiers + 0x10)))
    allowed.extend((STATS + 0x398 + i * 0xDC, STATS + 0x39A + i * 0xDC) for i in range(6))
    allowed.extend((STATS + 0x39E + i * 0xDC, STATS + 0x39F + i * 0xDC) for i in range(6))
    problems += store_problems(observation["stores"], allowed)
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = [f"{name} [{start:08X},{end:08X}): {(end-start)//4} instructions; "
             f"sha256={image.sha256(start, end)}"
             for name, start, end in zip(FUNCTIONS, ENTRIES, ENDS, strict=True)]
    lines.append("Four disjoint spans: 11 + 468 + 372 + 256 = 1107 instructions; no gaps counted. "
                 "Full guarded actor/rule/BSS images; callback argument/order and pre-call state CRC32; "
                 "callback mutations, bounded draw tape, allowed store spans; repeated driver reuse. "
                 "Equipment rebuild/enchantment services are explicit stubs, not claimed executed ranges.")
    for function, name in enumerate(FUNCTIONS):
        selected = [(v, obs) for v, obs, _ in results if v.function == function]
        lines.append(f"{name}: {len(selected)} vectors; "
                     f"{sum(bool(v.mutations) for v, _ in selected)} callback-mutation vectors; "
                     f"{sum(len(obs['trace']) for _, obs in selected)} service calls.")
    return lines
