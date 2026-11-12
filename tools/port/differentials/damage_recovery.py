"""F00494-F00499 original queues with real stat, limb and equipment helpers."""

import random
from dataclasses import dataclass

from differential import DEFAULT_STACK, MASK, Image, Machine, store_problems
from differentials.status_queue import put, words

ID = "F00494-F00499"
ENTRY, END = 0x80084B70, 0x80084EEC
ENTRIES = (ENTRY, 0x80084CAC, 0x80084D44, 0x80084DA8, 0x80084E28, 0x80084EA0)
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "damage_recovery_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\DamageRecoveryEffects.cpp",
           r"Native\Game\src\Battle\DamagePolicies.cpp",
           r"Native\Game\src\Battle\HitCalculations.cpp",
           r"Native\Game\src\Items\EquipmentWear.cpp")
VECTORS, SEED, MUTANT_VECTORS = 6144, 494499, 1536
SOURCE, TARGET, ACTION, STATE = (0x80500000 + i * 0x1000 for i in range(4))
ACTORS, ACTOR, STATS = 0x800F1928, 0x80600000, 0x80700000
SELECTED, STAT_TABLE, LAST, CRITICAL, DP = 0x800F19CC, 0x800E830C, 0x800F1A08, 0x800F1A0C, 0x800F1A10
CALCULATE, CALLBACK, DAMAGE, RECOVER, WEAR, PP = (
    0x800838EC, 0x80800000, 0x80080000, 0x800801E0, 0x800802C4, 0x800803A4)
SLOTS = (0, 1, 40)
RESOURCE_OFFSETS = (0x1C, 0x14C, 0x150, 0x2B0, 0x2B4, *(0x3E8 + i * 0xDC for i in range(6)))
MUTANTS = {
    "F00494-missing-critical-reset": {0x80084BD0: 0},
    "F00494-source-owner-is-target": {0x80084BC4: 0x8C54003C},
    "F00494-target-owner-is-source": {0x80084BC8: 0x8C72003C},
    "F00494-no-calculation": {0x80084BD4: 0},
    "F00494-wrong-execute": {0x80084BD8: 0xAFA00010},
    "F00494-subtract-HP": {0x80084BE8: 0x00461023},
    "F00494-invert-critical": {0x80084BEC: 0x14600004},
    "F00494-missing-HP": {0x80084BF0: 0},
    "F00494-wrong-mode": {0x80084C0C: 0x34420002},
    "F00494-neighbor-mode-clear": {0x80084C04: 0x2403FFF0},
    "F00494-invert-position": {0x80084C1C: 0x1040001A},
    "F00494-no-limbs": {0x80084C30: 0},
    "F00494-invert-exempt": {0x80084C40: 0x14620006},
    "F00494-wrong-status-mask": {0x80084C50: 0x30424000},
    "F00494-no-status-write": {0x80084C58: 0},
    "F00494-invert-execute": {0x80084C5C: 0x16A0000A},
    "F00494-no-DP": {0x80084C6C: 0},
    "F00494-no-PP": {0x80084C80: 0},
    "F00494-PP-before-DP": {0x80084C6C: 0x0C0200E9},
    "F00495-no-calculation": {0x80084CE0: 0},
    "F00495-unsigned-MP": {0x80084CF4: 0x9605001C},
    "F00495-unsigned-amount": {0x80084CF0: 0x00021402},
    "F00495-invert-cap": {0x80084D00: 0x14400003},
    "F00495-no-cap": {0x80084D08: 0},
    "F00495-no-last-value": {0x80084D0C: 0},
    "F00495-zero-extend-last": {0x80084D0C: 0xAC431A08},
    "F00495-wrong-mode": {0x80084D20: 0x34630008},
    "F00495-missing-MP": {0x80084D2C: 0},
    "F00496-invert-blade": {0x80084D70: 0x1440000B},
    "F00496-wrong-PP-field": {0x80084D84: 0x94A50150},
    "F00496-subtract-PP": {0x80084D98: 0x00451023},
    "F00496-wrong-mode": {0x80084D74: 0x3C040020},
    "F00496-missing-store": {0x80084D9C: 0},
    "F00497-wrong-gate-mask": {0x80084DD0: 0x3084000C},
    "F00497-miss-normal": {0x80084DD8: 0},
    "F00497-invert-critical-gate": {0x80084DE0: 0x1082000F},
    "F00497-invert-shield": {0x80084DF0: 0x1440000B},
    "F00497-wrong-PP-field": {0x80084E04: 0x94A502B4},
    "F00497-subtract-PP": {0x80084E18: 0x00451023},
    "F00497-wrong-mode": {0x80084DF4: 0x3C040200},
    "F00497-missing-store": {0x80084E1C: 0},
    "F00498-no-calculation": {0x80084E44: 0},
    "F00498-wrong-execute": {0x80084E48: 0xAFA00010},
    "F00498-wrong-mode": {0x80084E60: 0x34840001},
    "F00498-subtract-HP": {0x80084E68: 0x00461023},
    "F00498-missing-store": {0x80084E6C: 0},
    "F00498-invert-position": {0x80084E70: 0x10600006},
    "F00498-no-recovery": {0x80084E84: 0},
    "F00499-no-calculation": {0x80084EB4: 0},
    "F00499-wrong-execute": {0x80084EB8: 0xAFA00010},
    "F00499-wrong-mode": {0x80084ECC: 0x34840004},
    "F00499-neighbor-mask": {0x80084EC0: 0x2403FFF0},
    "F00499-subtract-MP": {0x80084ED4: 0x00621823},
    "F00499-missing-store": {0x80084ED8: 0},
}


@dataclass(frozen=True)
class Vector:
    function: int
    lane: int
    amount: int
    execute: int
    mode: int
    critical: int
    mutate: int
    category: int
    action_type: int
    blocks: tuple[bytes, ...]
    stats: tuple[bytes, ...]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 6, number // 6
    amount = (-32768, -101, -1, 0, 1, 101, 32767, rng.randrange(-32768, 32768))[lane % 8]
    execute = (0, 1, -1, -2147483648)[(lane // 3) % 4]
    blocks = [bytearray(rng.randbytes(100)) for _ in range(3)]
    for i, block in enumerate(blocks):
        put(block, 16, (1 if lane % 5 == 0 else 0) if i == 0 else 1 if i == 1 else 40, 1)
        put(block, 17, lane % 6, 1)
        put(block, 80, lane % 2 if i == 1 else (lane // 2) % 2)
    blocks[2][:16], blocks[2][-16:] = blocks[1][:16], blocks[1][-16:]
    stats = []
    for owner in range(3):
        block = bytearray(0x960)
        for offset in RESOURCE_OFFSETS:
            put(block, offset, (0, 1, 19, 32767, 32768, 65535)[(lane + owner + offset) % 6], 2)
        put(block, 0x14E, 60000, 2)
        put(block, 0x2B2, 60000, 2)
        put(block, 0x54, (0, 1, 65535)[(lane + owner) % 3], 2)
        put(block, 0x1EC, (0, 1, 65535)[(lane // 2 + owner) % 3], 2)
        put(block, 0x957, SLOTS[owner], 1)
        put(block, 0x948, 0x8000 if lane % 2 else 0x4000)
        for limb in range(6):
            put(block, 0x398 + limb * 0xDC, limb * 3, 2)
            put(block, 0x39A + limb * 0xDC, 100 if (lane + owner + limb) % 3 else 0, 2)
            for hit_limb in range(6):
                put(block, 0x920 + hit_limb * 6 + limb, (lane + owner + limb * 17) % 256, 1)
        for gem in range(3):
            put(block, 0xCA + gem * 0x30, 9 if (lane + owner + gem) % 4 == 0 else 0, 1)
            put(block, 0x232 + gem * 0x30, 9 if (lane + owner + gem) % 3 == 0 else 0, 1)
        stats.append(bytes(block))
    return Vector(function, lane, amount, execute, lane % 4, (0, -1, 2)[lane % 3],
                  lane % 3, lane % 16, (1, 6, 7)[lane % 3], tuple(bytes(b) for b in blocks),
                  tuple(stats))


def run(machine: Machine, vector: Vector, entries=ENTRIES) -> tuple[dict, list[int]]:
    v = vector
    machine.store(SOURCE - 16, v.blocks[0])
    machine.store(TARGET - 16, v.blocks[1])
    machine.map_zero(ACTION, 52)
    machine.put(ACTION, (v.action_type << 17) | (v.category << 20))
    machine.put(ACTION + 0x18, 1 << 16)
    machine.map_zero(STATE, 44)
    machine.put(SELECTED, STATE)
    machine.put(STATE + 0x28, 0xABCDEF00 | v.mode)
    for owner, slot in enumerate(SLOTS):
        machine.put(ACTORS + slot * 4, ACTOR + owner * 0x100)
        machine.map_zero(ACTOR + owner * 0x100, 0x44)
        machine.put(ACTOR + owner * 0x100 + 0x20, (v.lane + owner) & 3)
        machine.put(ACTOR + owner * 0x100 + 0x3C, STATS + owner * 0x1000)
        machine.store(STATS + owner * 0x1000, v.stats[owner])
    machine.put(LAST, 0xAABBCCDD)
    machine.put(CRITICAL, 99)
    machine.store(DP, b"\x71\x00" * 8)
    machine.put(STAT_TABLE + 4, CALLBACK)
    source = TARGET if v.lane % 7 == 0 else SOURCE
    events = []
    original_call = machine.call

    def observe(address: int) -> bool:
        if address in (CALCULATE, CALLBACK):
            events.extend([1 if address == CALCULATE else 2, *machine.r[4:8],
                           machine.load(machine.r[29] + 16), machine.load(CRITICAL)])
        if address in (DAMAGE, RECOVER):
            events.extend([3 if address == DAMAGE else 4, *machine.r[4:7],
                           machine.load(TARGET + 4, 2), machine.load(TARGET + 0x1C)])
        if address in (WEAR, PP):
            events.extend([5 if address == WEAR else 6, *machine.r[4:8],
                           machine.load(machine.r[5] + 0x150, 2),
                           machine.load(machine.r[6] + 0x2B4, 2)])
        return original_call(address)

    def calculate(m: Machine) -> None:
        if v.mutate:
            m.store(TARGET - 16, v.blocks[2])
            m.put(ACTOR + 0x3C, STATS + 0x2000)
            m.put(ACTOR + 0x100 + 0x3C, STATS + 0x2000)
            m.put(STATS + 0x1000 + 0x1C, -19, 2)
            m.put(STATS + 0x1000 + 0x948, 0x8000 if v.mutate == 1 else 0x4000)
            if v.execute == 0:
                m.put(STATS + 0x1000 + 0x957, 128 if v.mutate == 2 else 1, 1)
        m.put(CRITICAL, v.critical)
        m.r[2] = v.amount & MASK
        m.r[4:8] = [0x80900000] * 4

    machine.call = observe
    machine.stub(CALLBACK, calculate)
    machine.follow.update((CALCULATE, DAMAGE, RECOVER, WEAR, PP))
    machine.r[4:8] = [ACTION, source, TARGET, 1]
    machine.put(DEFAULT_STACK + 16, v.execute)
    machine.execute(entries[v.function], limit=3000)
    memory = [*words(machine.bytes(SOURCE - 16, 100)),
              *words(machine.bytes(TARGET - 16, 100)),
              *words(machine.bytes(DP, 16)), machine.load(LAST), machine.load(CRITICAL)]
    for owner in range(3):
        memory.extend(machine.load(STATS + owner * 0x1000 + offset, 2) for offset in RESOURCE_OFFSETS)
    inputs = [v.function, v.lane, v.amount & MASK, v.execute & MASK, v.mode, v.critical & MASK,
              v.mutate, v.category, v.action_type,
              *(word for block in (*v.blocks, *v.stats) for word in words(block))]
    return {"memory": memory, "events": events, "stores": list(machine.stores)}, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, vector, reference
    expected = [*observation["memory"], len(observation["events"]), *observation["events"]]
    problems = [] if native == expected else ["queued result/resources/ordered helper observation differs"]
    allowed = [(DEFAULT_STACK - 0x100, DEFAULT_STACK), (LAST, LAST + 4),
               (CRITICAL, CRITICAL + 4), (DP, DP + 12), (TARGET + 4, TARGET + 8),
               (TARGET + 0x18, TARGET + 0x20), (TARGET + 0x3A, TARGET + 0x3C),
               (TARGET + 0x3E, TARGET + 0x40)]
    allowed += [(TARGET + 0x20 + i * 4, TARGET + 0x22 + i * 4) for i in range(6)]
    allowed += [(STATS + owner * 0x1000 + offset, STATS + owner * 0x1000 + offset + 2)
                for owner in range(3) for offset in RESOURCE_OFFSETS]
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    lines = []
    for i, start in enumerate(ENTRIES):
        end = ENTRIES[i + 1] if i < 5 else END
        selected = [(v, o, n) for v, o, n in results if v.function == i]
        lines.append(f"F{494+i:05}: {len(selected)} vectors; "
                     f"{sum(bool(check(image, v, o, n, {})) for v, o, n in selected)} mismatches; "
                     f"[{start:08X},{end:08X}) {(end-start)//4} instructions; "
                     f"sha256={image.sha256(start, end)}")
    lines.append("Real original and owned stat dispatch, limb damage/recovery and equipment DP/PP "
                 "algorithms; controlled selected SHORT calculator mutates current hit, captured "
                 "owner fields and actor stats bindings. Full guarded hit and resource comparisons; "
                 "exact VOID5 arguments/ordered helper calls, critical reset, current flags, cached "
                 "owners, low16 signed recovery, negative MP caps, all category distributions and "
                 "DP-before-PP observations. Original slots0/1/40 avoid selection-pointer overlap. "
                 "No arbitrary selected-calculator/gameplay or EC retirement claim.")
    return lines
