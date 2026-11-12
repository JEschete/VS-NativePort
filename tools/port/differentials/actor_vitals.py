"""Defeat/result/vitals: four disjoint MIPS spans, real sibling calls, explicit service stubs."""

import random
import zlib
from dataclasses import dataclass
from operator import itemgetter

from differential import DEFAULT_STACK, MASK, Image, Machine, store_problems

ID = "F00194/F00522/F00525/F00526 (four disjoint spans)"
ENTRY, END = 0x800792E4, 0x800793C0
ENTRIES = (ENTRY, 0x800875C8, 0x80088554, 0x80088B6C)
ENDS = (END, 0x80087EF4, 0x80088B6C, 0x80088B8C)
FUNCTIONS = ("F00194", "F00522", "F00525", "F00526")
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "actor_vitals_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\ActorVitals.cpp",
           r"Native\Game\src\Battle\ActionResults.cpp")
VECTORS, SEED, MUTANT_VECTORS = 8192, 194522525526, 1536
TABLE, ACTORS, STATS, HIT = 0x800F1928, 0x80500000, 0x80600000, 0x80700000
COUNTER, TICKS, GUARD = 0x800E8490, 0x8005E24C, 16
BLOCKS = ((TABLE - GUARD, 42 * 4 + 32),
          *((ACTORS + i * 0x100 - GUARD, 0x50 + 32) for i in range(3)),
          *((STATS + i * 0x1000 - GUARD, 0x964 + 32) for i in range(3)),
          (HIT - GUARD, 0x44 + 32), (COUNTER - GUARD, 8 + 32),
          (TICKS - GUARD, 4 + 32))
# Every observed byte is explicitly initialized in the shared machine overlay.
# Gather those bytes in C, without weakening the complete-image comparison.
OBSERVE_BYTES = itemgetter(*(address + offset for address, size in BLOCKS for offset in range(size)))
EDGES = (0, 1, 2, 34, 35, 36, 998, 999, 1000, 32767, 32768, 65534, 65535)
COUNTERS = (0, 30, 60, 120, 180, 240, 719, 720, -30, -60, -120, -240, -720,
            1, 29, 59, 0x7FFFFFFF, 0x80000000)
SHIFTS = (0, 2, 4, 6, 8, 10, 12, 14, 18, 20, 22, 24)
AMOUNTS = (4, 6, 8, 10, 12, 14, 16, 18, 0x38, 0x3A, 0x3C, 0x3E)
MUTANTS = {
    "F00194-negative-hp-defeated": {0x8007932C: 0x1C40001B},
    "F00194-no-enemy-defeat": {0x80079334: 0},
    "F00194-count-kill-cleared": {0x80079338: 0x00003021},
    "F00194-player-trigger-63": {0x8007934C: 0x2404003F},
    "F00194-player-no-cutscene": {0x80079360: 0},
    "F00194-player-return-zero": {0x80079364: 0x24120000},
    "F00194-wrong-retain-byte": {0x80079378: 0x90420027},
    "F00194-no-idle": {0x8007939C: 0},
    "F00522-no-initial-commit": {0x800875E4: 0},
    "F00522-wrong-target": {0x800875F4: 0x92740001},
    "F00522-impact-label-one": {0x80087624: 0x24070001},
    "F00522-unsigned-limb-label": {0x80087670: 0x92650001},
    "F00522-no-hp-add": {0x80087690: 0},
    "F00522-unsigned-hp-clamp": {0x8008769C: 0x00021402},
    "F00522-five-limbs": {0x800876F8: 0x2E220005},
    "F00522-no-hp-reduction-label": {0x80087720: 0x00403021},
    "F00522-hp-floor-one": {0x80087748: 0xA6440018},
    "F00522-signed-armor-dp": {0x80087780: 0x84A403E8},
    "F00522-no-mp-store": {0x800877F8: 0},
    "F00522-no-risk-store": {0x800878BC: 0},
    "F00522-strength-clamp-998": {0x80087914: 0x240203E6},
    "F00522-strength-no-halfword-wrap": {0x80087908: 0},
    "F00522-intelligence-no-total": {0x800879F0: 0},
    "F00522-agility-no-actual": {0x80087B04: 0},
    "F00522-unsigned-max-hp": {0x80087BD0: 0x00021402},
    "F00522-max-mp-clamp-998": {0x80087C3C: 0x240203E6},
    "F00522-weapon-dp-no-wrap": {0x80087C80: 0},
    "F00522-no-weapon-pp-add": {0x80087CCC: 0},
    "F00522-no-weapon-pp-subtract": {0x80087D04: 0},
    "F00522-no-shield-dp-subtract": {0x80087D40: 0},
    "F00522-no-limb-armor-subtract": {0x80087D84: 0},
    "F00522-no-shield-pp-subtract": {0x80087DBC: 0},
    "F00522-dead-enemy-label": {0x80087E10: 0},
    "F00522-remove-31-bits": {0x80087E3C: 0x2E22001F},
    "F00522-no-removal": {0x80087E30: 0},
    "F00522-add-31-bits": {0x80087EC8: 0x2E22001F},
    "F00522-no-infliction": {0x80087EBC: 0},
    "F00525-fifteen-slots": {0x80088A60: 0x2A42000F},
    "F00525-spawn-127": {0x800885B8: 0x2402007F},
    "F00525-no-player-spawn-exception": {0x800885C4: 0x1000011C},
    "F00525-no-busy-call": {0x800885E8: 0},
    "F00525-busy-zero-not-one": {0x800885F0: 0x10400111},
    "F00525-wrong-normal-mask": {0x80088600: 0x34422000},
    "F00525-recovery-step-34": {0x80088634: 0x2482FFDE},
    "F00525-no-recovery-floor": {0x80088648: 0},
    "F00525-no-drawn-risk-decay": {0x800886CC: 0},
    "F00525-no-drawn-mp": {0x8008872C: 0},
    "F00525-no-drawn-weapon-pp": {0x80088754: 0},
    "F00525-no-drawn-shield-pp": {0x80088768: 0},
    "F00525-no-drawn-hp": {0x800887C0: 0},
    "F00525-wrong-slow-risk-mask": {0x8008880C: 0x30420004},
    "F00525-no-rest-mp": {0x80088890: 0},
    "F00525-no-rest-hp": {0x80088924: 0},
    "F00525-no-limb-hp": {0x80088974: 0},
    "F00525-no-status-call": {0x800889BC: 0},
    "F00525-no-defeat-animation": {0x800889E4: 0},
    "F00525-event-result-highhalf": {0x80088A24: 0},
    "F00525-no-one-precedence": {0x80088A58: 0},
    "F00525-no-counter-store": {0x80088AE8: 0},
    "F00525-no-limb-advance": {0x80088B20: 0},
    "F00525-no-timer-override": {0x80088B38: 0},
    "F00526-no-real-callee": {0x80088B74: 0},
    "F00526-zero-return": {0x80088B80: 0x00001021},
}


@dataclass(frozen=True)
class Vector:
    function: int
    source: int
    target: int
    count: int
    mutations: int
    trigger: int
    timer: int
    flags: tuple[int, ...]
    busy: tuple[int, ...]
    blocks: tuple[bytes, ...]


def put(data: bytearray, offset: int, value: int, size: int = 4) -> None:
    data[offset:offset + size] = (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little")


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 4, number // 4
    blocks = [bytearray(rng.randbytes(size)) for _, size in BLOCKS]
    table = blocks[0]
    for slot in range(42):
        put(table, GUARD + slot * 4, 0)
    for slot, actor in ((0, 0), (1, 1), (2, 2), (15, 2), (16, 1), (41, 2)):
        put(table, GUARD + slot * 4, ACTORS + actor * 0x100)
    for actor in range(3):
        record, stats = blocks[1 + actor], blocks[4 + actor]
        put(record, GUARD + 0x3C, STATS + actor * 0x1000)
        put(record, GUARD + 0x20, rng.choice((0, 1, 2, 3, MASK)))
        put(record, GUARD + 0x26, rng.choice((0, 1, 255)), 1)
        put(record, GUARD + 0x27, rng.choice((0, 1, 127, 128)), 1)
        for offset in range(0x18, 0x2E, 2):
            put(stats, GUARD + offset, rng.choice(EDGES), 2)
        put(stats, GUARD + 0x18, rng.choice((0, 0, 1, 10, 32767, 32768, 65535)), 2)
        for offset in (0x14C, 0x14E, 0x150, 0x152, 0x2B0, 0x2B2, 0x2B4):
            put(stats, GUARD + offset, rng.choice(EDGES), 2)
        for limb in range(6):
            for offset in (0x398, 0x39A, 0x3E8):
                put(stats, GUARD + offset + limb * 0xDC, rng.choice(EDGES), 2)
        put(stats, GUARD + 0x948, rng.choice((0, 1, 3, 8, 0x80000000, MASK)))
        put(stats, GUARD + 0x954, rng.choice(EDGES), 2)
    hit = blocks[7]
    put(hit, GUARD, lane % 3, 1)
    put(hit, GUARD + 1, lane % 6, 1)
    put(hit, GUARD + 3, rng.choice((0, 1, 3, 4, 5)), 1)
    effects = 0
    for index, (shift, offset) in enumerate(zip(SHIFTS, AMOUNTS, strict=True)):
        mode = lane % 4 if lane < 64 else rng.randrange(4)
        effects |= mode << shift
        put(hit, GUARD + offset, rng.choice(EDGES), 2)
        if lane % 16 == 0:
            put(hit, GUARD + offset, 1 if index % 2 else 65535, 2)
    put(hit, GUARD + 0x1C, effects)
    if ((effects >> 22) & 3) != 1:
        put(hit, GUARD + 1, rng.choice((0, 1, 5, 6, 127, 128, 255)), 1)
    for limb in range(6):
        put(hit, GUARD + 0x20 + limb * 4, rng.choice(EDGES), 2)
        put(hit, GUARD + 0x22 + limb * 4, rng.choice(EDGES), 2)
    for offset in (0x14, 0x18):
        put(hit, GUARD + offset, rng.choice((0, 1, 3, 0x80000000, MASK)))
    counter = COUNTERS[lane % len(COUNTERS)]
    put(blocks[8], GUARD, counter)
    put(blocks[8], GUARD + 4, (lane + lane // len(COUNTERS)) % 6)
    put(blocks[9], GUARD, rng.choice((0, 1, 30, 60, 720, MASK, 0x7FFFFFFF)))
    flags = [rng.choice((0, 1, 1, 1, 0x2001, 0x1000000, 0x1002001)) for _ in range(42)]
    busy = [rng.choice((0, 1, 2, 65537, MASK)) for _ in range(42)]
    mutations = lane % 512 if lane % 3 else 0
    # Biased cadence lanes: all used limbs, drawn/rest, low/max and signed HP.
    if function >= 2 and lane % 4 == 0:
        flags = [1] * 42
        mutations = 0
        for actor in range(3):
            put(blocks[actor + 1], GUARD + 0x27, 0, 1)
            put(blocks[actor + 1], GUARD + 0x20, (lane // 4) % 2)
            put(blocks[actor + 4], GUARD + 0x18, 10, 2)
            put(blocks[actor + 4], GUARD + 0x1A, 20, 2)
            put(blocks[actor + 4], GUARD + 0x1C, 10, 2)
            put(blocks[actor + 4], GUARD + 0x1E, 20, 2)
            for offset in (0x14C, 0x2B0):
                put(blocks[actor + 4], GUARD + offset, 2, 2)
            for limb in range(6):
                put(blocks[actor + 4], GUARD + 0x398 + limb * 0xDC, 1, 2)
                put(blocks[actor + 4], GUARD + 0x39A + limb * 0xDC, 10, 2)
    if function >= 2 and lane % 32 == 5:
        mutations = 256
        flags = [0x01000000] * 42
        busy = [0] * 42
        put(table, GUARD, 0)
        for actor in range(3):
            put(blocks[actor + 1], GUARD + 0x27, 0, 1)
            put(blocks[actor + 1], GUARD + 0x26, 1, 1)
            put(blocks[actor + 4], GUARD + 0x18, 0, 2)
    if function >= 2 and lane % 32 == 6:
        for slot in range(42):
            put(table, GUARD + slot * 4, 0)
        put(blocks[8], GUARD, 0)
        put(blocks[8], GUARD + 4, (0, -1, -6, 0x7FFFFFFF, 0x80000000)[lane // 32 % 5])
        put(blocks[9], GUARD, 0)
    target = (0, 1, 2, 3, 15, 41)[lane % 6]
    return Vector(function, rng.getrandbits(32), target, rng.getrandbits(32),
                  mutations, rng.choice((0, 1, 65537, 0xFFFF0001, MASK)),
                  rng.choice((0, 0, 0, 1, 2, 7, MASK)), tuple(flags), tuple(busy),
                  tuple(bytes(block) for block in blocks))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    for (address, _), block in zip(BLOCKS, vector.blocks, strict=True):
        machine.store(address, block)
    result_owner = STATS
    current_slot = 0

    def crc(m: Machine) -> int:
        return zlib.crc32(bytes(OBSERVE_BYTES(m.overlay)))

    def observe(m: Machine, tag: int, args: tuple[int, ...] = ()) -> None:
        nonlocal result_owner
        m.trace.append((tag, *args, *([0] * (4 - len(args))), crc(m)))
        mutate = vector.mutations
        if tag == 0:
            if mutate & 1:
                m.write(HIT, (m.load(HIT, 1) + 1) % 3, 1)
                m.write(HIT + 3, 4, 1)
            actor = m.load(TABLE + m.load(HIT, 1) * 4)
            result_owner = m.load(actor + 0x3C)
        if tag == 1 and mutate & 2:
            m.write(result_owner + 0x18, m.load(result_owner + 0x18, 2) + 1, 2)
            m.write(HIT + 4, m.load(HIT + 4, 2) + 3, 2)
            m.write(HIT + 0x1C, m.load(HIT + 0x1C) ^ 3, 4)
        if tag in (1, 2) and mutate & 4:
            m.write(HIT, 2, 1)
            m.write(TABLE, ACTORS + 0x100, 4)
            m.write(ACTORS + 0x3C, STATS + 0x2000, 4)
        if tag == 2 and mutate & 8:
            offset = 0x18 if args[3] else 0x14
            m.write(HIT + offset, m.load(HIT + offset) ^ args[2], 4)
            m.write(result_owner + 0x18, 0, 2)
        if tag in (3, 4):
            mask, owner = args
            active = m.load(owner + 0x948)
            m.write(owner + 0x948, active & ~mask if tag == 3 else active | mask, 4)
            if mutate & 16:
                m.write(owner + 0x18, 3, 2)
                m.write(HIT + 0x14, m.load(HIT + 0x14) | 0x80000000, 4)
        if tag == 5 and mutate & 32:
            slot = args[0]
            actor = m.load(TABLE + slot * 4)
            m.write(actor + 0x3C, STATS + 0x2000, 4)
            m.write(TABLE + slot * 4, ACTORS + 0x100, 4)
        if tag == 7 and mutate & 64:
            m.write(TABLE + args[1] * 4, ACTORS + 0x200, 4)
        if tag == 9:
            m.write(TABLE + args[0] * 4, 0, 4)
        if tag == 14:
            owner = args[0]
            if mutate & 128:
                m.write(owner + 0x18, 0 if mutate & 1 else 1, 2)
                m.write(ACTORS + 0x26, 1 if mutate & 2 else 0, 1)
            if mutate & 64:
                m.write(TABLE + 4, ACTORS + 0x200, 4)
                m.write(COUNTER, 31, 4)
                m.write(TICKS, 9, 4)
        m.r[2] = 0xD00DFEED

    def service(m: Machine, tag: int, count: int) -> None:
        observe(m, tag, tuple(m.r[4:4 + count]))

    def flags(m: Machine) -> None:
        nonlocal current_slot
        slot = m.r[4]
        current_slot = slot
        service(m, 5, 1)
        m.r[2] = vector.flags[slot]

    def busy(m: Machine) -> None:
        slot = m.r[4]
        service(m, 6, 1)
        m.r[2] = vector.busy[slot]

    def trigger(m: Machine) -> None:
        event = m.r[4]
        service(m, 11, 4)
        m.r[2] = ((1 if current_slot == 15 else 2)
                  if event == 12 and vector.mutations & 256 else vector.trigger)

    def timer(m: Machine) -> None:
        observe(m, 15)
        m.r[2] = vector.timer

    for address, tag, count in ((0x800971D4, 0, 0), (0x80087578, 1, 4),
                                (0x80093FEC, 2, 4), (0x80086754, 3, 2),
                                (0x80086FA8, 4, 2), (0x8006C1CC, 7, 3),
                                (0x8009E5C4, 8, 1), (0x8006A228, 9, 2),
                                (0x8009722C, 10, 0), (0x80073898, 12, 0),
                                (0x80074A20, 13, 1), (0x80087EF4, 14, 1)):
        machine.stub(address, lambda m, tag=tag, count=count: service(m, tag, count))
    machine.stub(0x800A0BE0, flags)
    machine.stub(0x8009E4B0, busy)
    machine.stub(0x800BEC58, trigger)
    machine.stub(0x800882F4, timer)
    machine.follow.update((ENTRIES[0], ENTRIES[2]))
    machine.r[4:7] = [vector.source, vector.target, vector.count]
    if vector.function == 1:
        machine.r[4] = HIT
    machine.execute(ENTRIES[vector.function], limit=20000)
    data = b"".join(vector.blocks)
    inputs = [vector.function, vector.source, vector.target, vector.count, vector.mutations,
              vector.trigger, vector.timer, *vector.flags, *vector.busy,
              *(int.from_bytes(data[i:i + 4], "little") for i in range(0, len(data), 4))]
    return {"memory": bytes(OBSERVE_BYTES(machine.overlay)),
            "result": machine.r[2] if vector.function != 1 else 0,
            "trace": tuple(machine.trace), "stores": list(machine.stores)}, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    count = sum(size for _, size in BLOCKS) // 4
    if len(native) < count + 3:
        return ["truncated native output"]
    problems = [] if native[0] == 1 else ["native refused"]
    if native[1] != observation["result"]:
        problems.append(f"result {native[1]:08X} != {observation['result']:08X}")
    actual = b"".join(word.to_bytes(4, "little") for word in native[2:2 + count])
    if actual != observation["memory"]:
        bad = [hex(i) for i, pair in enumerate(zip(actual, observation["memory"], strict=True))
               if pair[0] != pair[1]]
        problems.append("guarded memory differs at " + ",".join(bad[:18]))
    calls = tuple(tuple(native[i:i + 6]) for i in range(3 + count, len(native), 6))
    if native[2 + count] != len(calls) or calls != observation["trace"]:
        first = next((i for i, pair in enumerate(zip(calls, observation["trace"], strict=False))
                      if pair[0] != pair[1]), min(len(calls), len(observation["trace"])))
        problems.append(f"calls/snapshots at {first}: {calls[first:first + 1]} != "
                        f"{observation['trace'][first:first + 1]}")
    allowed = [(DEFAULT_STACK - 0x100, DEFAULT_STACK), (TABLE, TABLE + 42 * 4),
               (HIT, HIT + 1), (HIT + 3, HIT + 6), (HIT + 0x14, HIT + 0x20),
               (COUNTER, COUNTER + 8), (TICKS, TICKS + 4)]
    for index in range(3):
        actor, stats = ACTORS + index * 0x100, STATS + index * 0x1000
        allowed.extend(((actor + 0x26, actor + 0x27), (actor + 0x3C, actor + 0x40),
                        (stats + 0x18, stats + 0x2E), (stats + 0x14C, stats + 0x154),
                        (stats + 0x2B0, stats + 0x2B6), (stats + 0x948, stats + 0x94C),
                        (stats + 0x954, stats + 0x956)))
        for limb in range(6):
            allowed.extend(((stats + 0x398 + limb * 0xDC, stats + 0x39A + limb * 0xDC),
                            (stats + 0x3E8 + limb * 0xDC, stats + 0x3EA + limb * 0xDC)))
    problems += store_problems(observation["stores"], allowed)
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = [f"{name} [{start:08X},{end:08X}): {(end-start)//4} instructions; "
             f"sha256={image.sha256(start, end)}"
             for name, start, end in zip(FUNCTIONS, ENTRIES, ENDS, strict=True)]
    lines.append("Four disjoint spans: 55+587+390+8=1040 instructions, no intervening gaps. "
                 "F00526 executes F00525; both vitals entries execute F00194. "
                 "Full guarded actor/stats/hit/table/counter images and pre-service CRC32 snapshots; "
                 "signed label arguments, cached ownership, callback-mutated tables/masks/HP/counters, "
                 "allowed store spans. Lifecycle/animation/flags/event/label services are explicit stubs.")
    for function, name in enumerate(FUNCTIONS):
        selected = [(v, obs) for v, obs, _ in results if v.function == function]
        lines.append(f"{name}: {len(selected)} vectors; "
                     f"{sum(bool(v.mutations) for v, _ in selected)} mutation vectors; "
                     f"{sum(len(obs['trace']) for _, obs in selected)} service calls.")
    return lines
