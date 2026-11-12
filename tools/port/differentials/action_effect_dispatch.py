"""F00506/F00507 original instructions; real status policies, controlled callbacks."""

import random
from dataclasses import dataclass

from differential import DEFAULT_STACK, MASK, store_problems
from differentials.status_queue import put, words

ID = "F00506/F00507"
ENTRY, END = 0x80085008, 0x800856F8
ENTRIES = (ENTRY, 0x80085390)
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "action_effect_dispatch_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\ActionEffectDispatch.cpp",
           r"Native\Game\src\Battle\StatusQueue.cpp")
VECTORS, SEED, MUTANT_VECTORS = 8192, 506507, 2048
SOURCE, TARGET, ACTION = 0x80500000, 0x80501000, 0x80502000
ACTOR, STATS, CHARACTER = 0x80600000, 0x80700000, 0x80503000
ACTORS, CHARACTER_PTR = 0x800F1928, 0x800F19FC
RULES, IDS, LAST, EFFECTS = 0x8004EECC, 0x800E8200, 0x800F1A08, 0x800E8378
ARTS, THRESHOLDS, FLAG, CALLBACK = 0x80060040, 0x8004FDE4, 0x800BEBF4, 0x80800000
ADD, REMOVE = 0x80081020, 0x800810CC
SLOTS = (0, 1, 40)
MUTANTS = {
    "F00506-wrong-owner": {0x80085054: 0x8C530038},
    "F00506-add-mode": {0x80085050: 0x24030001},
    "F00506-subtract-mode": {0x80085064: 0x24020000},
    "F00506-upper-DP": {0x800850D8: 0x94A30022},
    "F00506-mobility-zero-index": {0x8008508C: 0x92650035},
    "F00506-cripple-mask": {0x800851A0: 0x30420020},
    "F00506-none-gate": {0x800851BC: 0},
    "F00506-health-threshold": {0x800851E4: 0x28420003},
    "F00506-no-cripple-add": {0x800851FC: 0},
    "F00506-no-cripple-remove": {0x8008525C: 0},
    "F00506-nonplayer-injuries": {0x8008526C: 0},
    "F00506-limb-mode": {0x80085294: 0x30430002},
    "F00506-limb-upper-DP": {0x800852D4: 0x96220022},
    "F00506-limb-rule": {0x800852F8: 0x90440001},
    "F00506-current-status": {0x800852FC: 0x8E63094C},
    "F00506-rule-stride": {0x80085300: 0x000410C0},
    "F00506-injury-threshold": {0x8008531C: 0x3C020002},
    "F00506-no-injury-add": {0x8008532C: 0},
    "F00506-no-injury-remove": {0x8008534C: 0},
    "F00506-only-three-limbs": {0x8008535C: 0x2A420003},
    "F00507-position-gate": {0x800853BC: 0},
    "F00507-exempt-id": {0x800853EC: 0x2402007F},
    "F00507-exempt-amount": {0x80085410: 0x244203E6},
    "F00507-exempt-mode": {0x80085408: 0x34630002},
    "F00507-outer-type": {0x80085428: 0x3C030004},
    "F00507-spell-mask": {0x8008546C: 0x3C030004},
    "F00507-suppression": {0x80085474: 0x14400003},
    "F00507-last-value": {0x80085480: 0},
    "F00507-spell-execute": {0x80085498: 0xAFA00010},
    "F00507-spell-selector": {0x80085490: 0x8C420010},
    "F00507-post-position": {0x800854C0: 0},
    "F00507-post-exempt": {0x800854F0: 0},
    "F00507-removed-mask": {0x800854F8: 0x34422000},
    "F00507-no-removals": {0x80085510: 0},
    "F00507-other-execute": {0x8008552C: 0xAFA00010},
    "F00507-other-selector": {0x80085524: 0x8C420010},
    "F00507-execute-gate": {0x8008554C: 0},
    "F00507-source-position": {0x8008555C: 0},
    "F00507-source-slot": {0x8008556C: 0},
    "F00507-target-position": {0x8008557C: 0},
    "F00507-target-exempt": {0x800855AC: 0},
    "F00507-ability-type": {0x800855C4: 0},
    "F00507-art-type": {0x800855CC: 0},
    "F00507-base-type": {0x800855D0: 0x3C02000A},
    "F00507-unsigned-hp": {0x80085600: 0x94430018},
    "F00507-unsigned-queue": {0x80085604: 0x96020004},
    "F00507-self-gate": {0x80085618: 0},
    "F00507-repeat-gate": {0x80085628: 0},
    "F00507-no-counted-store": {0x80085630: 0},
    "F00507-category-source": {0x80085640: 0x90450024},
    "F00507-flag-id": {0x80085650: 0x24040081},
    "F00507-no-low8": {0x80085674: 0},
    "F00507-flag-threshold": {0x80085678: 0x2C42000B},
    "F00507-learned-boundary": {0x80085694: 0x2C420003},
    "F00507-threshold-stride": {0x800856B0: 0x00101880},
    "F00507-kills-gate": {0x800856D0: 0},
    "F00507-no-increment": {0x800856D4: 0x24620000},
    "F00507-no-progress-store": {0x800856D8: 0},
}


@dataclass(frozen=True)
class Vector:
    function: int
    lane: int
    execute: int
    mutation: int
    flag: int
    category: int
    blocks: tuple[bytes, ...]


def vector(rng: random.Random, number: int) -> Vector:
    lane = number // 2
    source, target = (bytearray(rng.randbytes(100)) for _ in range(2))
    for b, slot in ((source, 0), (target, SLOTS[(lane // 4) % 3])):
        put(b, 16, slot, 1)
        put(b, 80, 0 if lane % 7 else 1)
    if lane % 11 == 0:
        put(source, 16, 1, 1)
    put(target, 20, (0, 1, 2, 32767, 32768, 65535)[lane % 6], 2)
    put(target, 44, (rng.getrandbits(32) & ~3) | lane % 4)
    action = bytearray(52)
    put(action, 0, (1, 2, 3, 6, 0, 7)[(lane // 3) % 6] << 17)
    put(action, 20, 1)
    put(action, 24, 2)
    blocks = [source, target, action]
    for owner in range(3):
        stats = bytearray(0x960)
        put(stats, 0x18, (0, 1, 2, 32767, 32768, 65535)[(lane // 6) % 6], 2)
        put(stats, 0x34, (lane + owner) % 6, 1)
        put(stats, 0x35, (lane // 6 + owner) % 6, 1)
        put(stats, 0x957, 128 if lane % 13 == 0 else SLOTS[owner], 1)
        put(stats, 0x948, (0, 0x10, 0x61000, 0x80000000, 0x40000, MASK)[(lane + owner) % 6])
        put(stats, 0x944, rng.getrandbits(32) if lane % 5 == 0 else 0)
        for limb in range(6):
            put(stats, 0x398 + limb * 0xDC, (0, 1, 2, 3, 32767, 32768, 65535)[(lane + limb) % 7], 2)
            if lane % 3:
                put(target, 48 + limb * 4, (lane + limb) % 3, 2)
        blocks.append(stats)
    rules = bytearray(512)
    for i in range(32):
        for j in range(4):
            put(rules, i * 16 + j * 4, (1 << (i % 32)) if j == 0 else 0)
    ids = bytearray((1, 2, 3, 5))
    arts = bytearray(32)
    thresholds = bytearray(80)
    for i in range(10):
        put(arts, i, (lane // 2 + i) % 5, 1)
        put(arts, 12 + 2 * i, (0, 1, 2, 65534, 65535)[lane % 5], 2)
        for j in range(4):
            put(thresholds, i * 8 + j * 2, (0, 1, 2, 65535)[(lane // 5 + j) % 4], 2)
    mutation = lane % 4
    execute = (0, 1, -1, -2147483648)[(lane // 2) % 4]
    bias = lane % 64
    if number % 2 and bias < 24:
        mutation, execute = 0, -1
        put(source, 16, 0, 1)
        put(source, 80, int(bias == 1))
        put(target, 16, 1, 1)
        put(target, 80, int(bias == 2))
        put(target, 20, 2, 2)
        put(action, 0, (3 if bias == 5 else 6 if bias == 6 else 1 if 7 <= bias <= 10 else 2) << 17)
        for i, stats in enumerate(blocks[3:6]):
            put(stats, 0x957, SLOTS[i], 1)
            put(stats, 0x948, 0)
            put(stats, 0x18, 1, 2)
        if bias == 3:
            put(blocks[4], 0x957, 128, 1)
        if bias == 4:
            put(source, 16, 1, 1)
        if bias in (7, 8):
            put(blocks[4], 0x948, 0x80000000 if bias == 7 else 0x40000)
        if bias == 9:
            put(target, 16, 40, 1)
            put(blocks[4], 0x957, 128, 1)
            put(blocks[4], 0x948, 0x61000)
            mutation = 2
        if bias == 10:
            mutation = 3
        for i in range(10):
            put(arts, i, 0, 1)
            put(arts, 12 + i * 2, i + 2, 2)
            for j in range(4):
                put(thresholds, i * 8 + j * 2, i + 3, 2)
    blocks.extend((rules, ids, arts, thresholds))
    return Vector(number % 2, lane, execute, mutation,
                  (0, 9, 10, 255, 0x10000, 0x1000A)[(lane // 4) % 6],
                  (lane * 7) % 256, tuple(bytes(b) for b in blocks))


def run(machine, value):
    v = value
    locations = (SOURCE - 16, TARGET - 16, ACTION, STATS, STATS + 0x1000, STATS + 0x2000,
                 RULES, IDS, ARTS, THRESHOLDS)
    for address, block in zip(locations, v.blocks, strict=True):
        machine.store(address, block)
    for i, slot in enumerate(SLOTS):
        machine.put(ACTORS + slot * 4, ACTOR + i * 0x100)
        machine.map_zero(ACTOR + i * 0x100, 0x44)
        machine.put(ACTOR + i * 0x100 + 0x3C, STATS + i * 0x1000)
        machine.put(ACTOR + i * 0x100 + 0x38, STATS + (2 - i) * 0x1000)
        machine.put(ACTOR + i * 0x100 + 0x29, 1 if v.lane % 17 == 0 else 0, 1)
    machine.map_zero(CHARACTER, 0x40)
    machine.put(CHARACTER + 0x25, v.category, 1)
    machine.put(CHARACTER_PTR, CHARACTER)
    machine.put(LAST, 0xFEED1234)
    for i in range(3):
        machine.put(EFFECTS + i * 4, CALLBACK + i * 4)
    events = []
    calls = 0
    original_call = machine.call

    def mutation():
        nonlocal calls
        calls += 1
        if not v.mutation:
            return
        machine.put(TARGET, SLOTS[v.mutation - 1], 1)
        machine.put(ACTORS, ACTOR + 0x200)
        machine.put(ACTION, (v.mutation + 1) << 17)
        machine.put(STATS + 0x948, 0xFFFFFFFF if calls % 2 else 0)
        machine.put(TARGET + 0x1C, (machine.load(TARGET + 0x1C) & ~3) | calls % 4)
        machine.put(IDS + calls % 4, (calls + 5) % 32, 1)
        machine.put(STATS + 0x398 + (calls % 4) * 0xDC, 2, 2)
        machine.put(RULES + 16 * ((calls + 5) % 32), 1 << calls)
        if v.mutation == 3:
            machine.put(TARGET + 0x40, calls % 2)

    def observe(address):
        if address in (ADD, REMOVE):
            events.extend([3 if address == ADD else 4, machine.r[4], machine.r[5]])
            mutation()
        return original_call(address)

    def effect(m, selector):
        events.extend([1, *m.r[4:8], m.load(m.r[29] + 16), selector])
        mutation()
        m.put(TARGET + 4, m.load(TARGET + 4, 2) + 3, 2)
        m.r[4:8] = [0xBAD00000] * 4

    def flag(m):
        events.extend([2, m.r[4], *(m.load(ACTOR + i * 0x100 + 0x29, 1) for i in range(3))])
        if v.mutation:
            for i in range(10):
                m.put(ARTS + i, (v.lane + i) % 5, 1)
                m.put(ARTS + 12 + i * 2, (v.lane + i) % 3, 2)
            m.put(CHARACTER_PTR, 0)
        m.r[2] = v.flag

    machine.call = observe
    machine.follow.update((ADD, REMOVE))
    for i in range(3):
        machine.stub(CALLBACK + 4 * i, lambda m, i=i: effect(m, i))
    machine.stub(FLAG, flag)
    machine.r[4:8] = [TARGET, 0, 0, 0] if v.function == 0 else [ACTION, SOURCE, TARGET, v.execute & MASK]
    machine.put(DEFAULT_STACK + 16, v.lane % 2)
    machine.execute(ENTRIES[v.function], limit=4000)
    memory = [*words(machine.bytes(TARGET - 16, 100)), machine.load(LAST),
              *(machine.load(ACTOR + i * 0x100 + 0x29, 1) for i in range(3)),
              *words(machine.bytes(ARTS, 32))]
    inputs = [v.function, v.lane, v.execute & MASK, v.mutation, v.flag, v.category,
              *(w for block in v.blocks for w in words(block))]
    return {"memory": memory, "events": events, "stores": machine.stores}, inputs


def check(image, vector, observation, native, reference):
    del image, vector, reference
    expected = [*observation["memory"], len(observation["events"]), *observation["events"]]
    allowed = [(DEFAULT_STACK - 0x100, DEFAULT_STACK), (TARGET + 4, TARGET + 6),
               (TARGET + 0x14, TARGET + 0x20), (LAST, LAST + 4), (ARTS + 12, ARTS + 32),
               *((ACTOR + i * 0x100 + 0x29, ACTOR + i * 0x100 + 0x2A) for i in range(3))]
    return ([] if expected == native else ["hit, progress or callback sequence differs"]) + \
        store_problems(observation["stores"], allowed)


def describe(image, results):
    return [f"F{506+i:05}: {sum(v.function == i for v, _, _ in results)} vectors; "
            f"[{start:08X},{end:08X}) {(end-start)//4} instructions; "
            f"sha256={image.sha256(start, end)}"
            for i, (start, end) in enumerate(zip(ENTRIES, (*ENTRIES[1:], END), strict=True))]
