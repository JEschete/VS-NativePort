"""F00476-F00485: selected gates, real stat dispatch and limb-recovery callees."""

import random
from dataclasses import dataclass

from differential import DEFAULT_STACK, MASK, Image, Machine, store_problems
from differentials.status_queue import put, words

ID = "F00476-F00485"
ENTRY, END = 0x80084390, 0x8008490C
ENTRIES = (ENTRY, 0x80084440, 0x800844F0, 0x80084570,
           0x800845F0, 0x80084670, 0x800846F0, 0x80084770, 0x800847F0, 0x80084870)
FUNCTIONS = tuple(f"F{number:05}" for number in range(476, 486))
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "triggered_recovery_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\BasicActionEffects.cpp",
           r"Native\Game\src\Battle\HitCalculations.cpp",
           r"Native\Game\src\Battle\DamagePolicies.cpp")
VECTORS, SEED, MUTANT_VECTORS = 20480, 476483, 2560
SOURCE, TARGET, OTHER, ACTION, STATE = (0x80500000 + i * 0x1000 for i in range(5))
ACTORS, ACTOR, STATS = 0x800F1928, 0x80600000, 0x80700000
SELECTED, DEFINITIONS = 0x800F19CC, 0x8004B9DC
CALCULATE, RECOVER, CALLBACK = 0x800838EC, 0x800801E0, 0x80800000
STAT_TABLE, LAST, DP, GUARD = 0x800E830C, 0x800F1A08, 0x800F1A10, 16
MUTANTS = {}
for _i, (_entry, _name) in enumerate(zip(ENTRIES, FUNCTIONS, strict=True)):
    _calculate = _entry + (0x60 if _i < 2 else 0x4C if _i == 9 else 0x30)
    _gate = _entry + (0x50 if _i < 2 else 0x24)
    _branch = 0x10430012 if _i == 0 else 0x14430012 if _i == 1 else (
        0x10400011 if _i == 2 else 0x10430011)
    MUTANTS.update({
        f"{_name}-inverted-gate": {_gate: _branch} if _i != 9 else {0x800848A8: 0x10620013},
        f"{_name}-wrong-target": {(_gate + 4 if _i != 9 else 0x800848A0): 0x00A08821},
        f"{_name}-missing-calculation": {_calculate: 0},
        f"{_name}-wrong-execute": {_calculate + 4: 0xAFA00010},
        f"{_name}-unsigned-amount": {_calculate + 0x14: 0x00063402},
        f"{_name}-clears-neighbor": {_calculate + 0x1C: 0x2403FFF8},
        f"{_name}-wrong-mode": {_calculate + 0x28: 0x34E70001},
        f"{_name}-no-flags-store": {_calculate + 0x2C: 0},
        f"{_name}-subtract-HP": {_calculate + 0x30: 0x00621823},
        f"{_name}-no-recovery": {_calculate + 0x34: 0},
        f"{_name}-no-HP-store": {_calculate + 0x38: 0},
    })
    if _i < 2:
        MUTANTS[f"{_name}-input-type-not-selection"] = {_entry + 0x40: 0x8E020000}
        MUTANTS[f"{_name}-wrong-type-mask"] = {_entry + 0x4C: 0x30420003}
    elif _i != 9:
        MUTANTS[f"{_name}-wrong-affinity-byte"] = {_entry + 0x1C: 0x9042000B}
    else:
        MUTANTS[f"{_name}-wrong-primary-mask"] = {0x80084894: 0x3043000C}
        MUTANTS[f"{_name}-miss-normal-damage"] = {0x8008489C: 0}


@dataclass(frozen=True)
class Vector:
    function: int
    selected_id: int
    selected_type: int
    affinity: int
    execute: int
    hit: int
    amount: int
    mutate: bool
    alias: bool
    null_unused: bool
    blocks: tuple[bytes, ...]
    vitals: tuple[int, ...]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 10, number // 10
    selected_type = lane % 8
    affinity = (lane // 2) % 256 if lane % 2 else max(0, function - 2)
    matches = ((selected_type == 1) == (function == 0) if function < 2 else
               (selected_type & 3) in (1, 3) if function == 9 else affinity == function - 2)
    blocks = [bytearray(rng.randbytes(100)) for _ in range(4)]
    for block in blocks:
        put(block, GUARD, 0, 1)
        put(block, GUARD + 0x40, lane % 2)
    pending = (0, 19, -17, 32767, 32768, 65535)[(lane // 3) % 6]
    put(blocks[1], GUARD + 4, pending, 2)
    put(blocks[3], GUARD + 4, pending + 7, 2)
    blocks[3][:GUARD], blocks[3][-GUARD:] = blocks[1][:GUARD], blocks[1][-GUARD:]
    amount = (-32768, -101, -1, 0, 1, 101, 32767, rng.randrange(-32768, 32768))[(lane // 5) % 8]
    vitals = []
    for i in range(12):
        maximum = (0, 100, -1, 32767)[(lane + i) % 4]
        current = (-32768, -1, 0, 50, 32767)[(lane // 7 + i) % 5]
        vitals.extend((current, maximum))
    return Vector(function, (0, 255, 128, lane % 256)[lane % 4], selected_type,
                  affinity, (0, 1, -1, -2147483648, 2147483647)[lane % 5],
                  lane % 2, amount, lane % 3 != 0, lane % 7 == 0,
                  not matches and lane % 3 == 0, tuple(bytes(b) for b in blocks),
                  tuple(vitals))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    for address, block in zip((SOURCE, TARGET, OTHER), vector.blocks, strict=False):
        machine.store(address - GUARD, block)
    machine.map_zero(STATE, 44)
    machine.map_zero(ACTION, 52)
    machine.put(SELECTED, STATE)
    machine.put(STATE + 8, vector.selected_id, 2)
    machine.put(STATE + 10, vector.affinity, 1)
    machine.put(STATE + 11, vector.affinity ^ 1, 1)
    machine.put(STATE + 0x28, 0xABCDEF00 | (vector.selected_type & 3))
    machine.put(DEFINITIONS + 52 * vector.selected_id, vector.selected_type << 17)
    machine.put(ACTION, ((vector.selected_type ^ 1) & 7) << 17)
    for hit in range(2):
        machine.put(ACTION + 0x14 + 4 * hit, 1 << 16)
    for owner in range(2):
        machine.put(ACTORS + owner * 4, ACTOR + owner * 0x100)
        machine.map_zero(ACTOR + owner * 0x100, 0x44)
        machine.put(ACTOR + owner * 0x100 + 0x3C, STATS + owner * 0x1000)
        machine.map_zero(STATS + owner * 0x1000, 0x960)
        machine.put(STATS + owner * 0x1000 + 0x957, 255, 1)
        for limb in range(6):
            for field in range(2):
                machine.put(STATS + owner * 0x1000 + 0x398 + limb * 0xDC + field * 2,
                            vector.vitals[owner * 12 + limb * 2 + field], 2)
    machine.put(LAST, 0xAABBCCDD)
    machine.store(DP, b"\x71\x00" * 8)
    machine.put(STAT_TABLE + 4, CALLBACK)
    args = [ACTION, TARGET if vector.alias else SOURCE, TARGET,
            vector.hit, vector.execute & MASK]
    if vector.null_unused:
        args[:4] = [0, 0, 0, MASK]
    events = []
    original_call = machine.call

    def observe(address: int) -> bool:
        if address in (CALCULATE, CALLBACK):
            events.append([1 if address == CALCULATE else 2, *machine.r[4:8],
                           machine.load(machine.r[29] + 16)])
        if address == RECOVER:
            events.append([3, *machine.r[4:7], machine.load(TARGET + 4, 2),
                           machine.load(TARGET + 0x1C)])
        return original_call(address)

    def calculate(m: Machine) -> None:
        if vector.mutate:
            m.store(TARGET - GUARD, vector.blocks[3])
            m.put(ACTORS, ACTOR + 0x100)
            m.put(STATE + 8, 65535, 2)
            m.put(STATE + 10, 127, 1)
        m.r[2] = vector.amount & MASK
        m.r[4:8] = [OTHER] * 4

    machine.call = observe
    machine.stub(CALLBACK, calculate)
    machine.follow.update((CALCULATE, RECOVER))
    machine.r[4:8] = args[:4]
    machine.put(DEFAULT_STACK + 16, args[4])
    machine.execute(ENTRIES[vector.function], limit=1000)
    memory = [*words(machine.bytes(SOURCE - GUARD, 100)),
              *words(machine.bytes(TARGET - GUARD, 100)),
              *words(machine.bytes(OTHER - GUARD, 100)),
              *words(machine.bytes(DP, 16)), machine.load(LAST)]
    inputs = [vector.function, vector.selected_id, vector.selected_type, vector.affinity,
              *args, vector.amount & MASK, int(vector.mutate),
              *(word for block in vector.blocks for word in words(block)),
              *(value & 0xFFFF for value in vector.vitals)]
    return {"memory": memory, "events": events, "stores": list(machine.stores)}, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, vector, reference
    expected = [1, *observation["memory"], len(observation["events"]),
                *(value for event in observation["events"] for value in event)]
    problems = [] if native == expected else ["guarded results/call identity/ordered pending observations differ"]
    allowed = [(DEFAULT_STACK - 0x60, DEFAULT_STACK), (TARGET + 4, TARGET + 6),
               (TARGET + 0x1C, TARGET + 0x20), (LAST, LAST + 4), (DP, DP + 12)]
    allowed += [(TARGET + 0x20 + 4*i, TARGET + 0x22 + 4*i) for i in range(6)]
    problems += store_problems(observation["stores"], allowed)
    writes = [(a, s) for a, s in observation["stores"] if a in (TARGET + 0x1C, TARGET + 4)]
    if observation["events"] and writes != [(TARGET + 0x1C, 4), (TARGET + 4, 2)]:
        problems.append("flags must be stored before queued HP")
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = [f"Explicit original [{ENTRY:08X},{END:08X}) 351 instructions; sha256={image.sha256(ENTRY, END)}"]
    for function, name in enumerate(FUNCTIONS):
        selected = [(v, obs, native) for v, obs, native in results if v.function == function]
        end = ENTRIES[function + 1] if function + 1 < len(ENTRIES) else END
        lines.append(f"{name}: {len(selected)} vectors; "
                     f"{sum(bool(check(image, v, obs, native, {})) for v, obs, native in selected)} mismatches; "
                     f"[{ENTRIES[function]:08X},{end:08X}) {(end-ENTRIES[function])//4} instructions; "
                     f"sha256={image.sha256(ENTRIES[function], end)}; "
                     f"gated-in={sum(bool(obs['events']) for _, obs, _ in selected)}")
    lines.append("Real original F00435 stat dispatcher and F00397 limb recovery followed; controlled SHORT "
                 "selected stat callback only. Native uses both owned typed helper algorithms. Exact VOID5 "
                 "arguments, callback mutation/clobbers, cached hit versus current actor identity, full guarded "
                 "hit bytes, signed/zero/wrapped HP, recovery entry observation and flags-before-HP stores; "
                 "no incidental V0 contract, no selected stat algorithm claim.")
    return lines
