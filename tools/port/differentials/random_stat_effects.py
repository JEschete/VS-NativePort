"""F00486-F00493: random queued stat amounts and direction-only decreases."""

import random
from dataclasses import dataclass

from differential import (
    DEFAULT_STACK,
    MASK,
    Image,
    Machine,
    field_problems,
    store_problems,
)
from differentials.status_queue import put, words

ID = "F00486-F00493"
ENTRY, END = 0x8008490C, 0x80084B70
ENTRIES = (ENTRY, 0x80084958, 0x800849A4, 0x800849F0,
           0x80084A3C, 0x80084A8C, 0x80084AD8, 0x80084B24)
FUNCTIONS = tuple(f"F{number:05}" for number in range(486, 494))
OFFSETS = (0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x0A, 0x0C, 0x0E)
SHIFTS = (6, 8, 10, 12, 14, 6, 8, 10)
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "random_stat_effects_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\BasicActionEffects.cpp",)
VECTORS, SEED, MUTANT_VECTORS = 16384, 486493, 2048
SOURCE, TARGET, OTHER, ACTORS = 0x80500000, 0x80501000, 0x80502000, 0x800F1928
RANDOM, HIT_SIZE, GUARD = 0x800427A8, 0x44, 16
NAMES = ("native-canaries", "trace-count", "random-bound", "random-return", "draw-before-write")

MUTANTS = {}
for _index, (_entry, _name, _offset, _shift) in enumerate(
        zip(ENTRIES, FUNCTIONS, OFFSETS, SHIFTS, strict=True)):
    _extra = 4 if _index == 4 else 0
    _mode = 2 if _index < 5 else 1
    _mask = (~(3 << _shift) & 0xFFFF) & ~4
    MUTANTS.update({
        f"{_name}-wrong-cached-target": {_entry + 8: 0x00A08021},
        f"{_name}-missing-random": {_entry + 0x10: 0},
        f"{_name}-bound-five": {_entry + 0x14: 0x24040005},
        f"{_name}-clears-other-flag": {
            _entry + 0x18 + _extra: (0x34630000 if _index == 4 else 0x24030000) | _mask},
        f"{_name}-zero-based": {_entry + 0x20 + _extra: 0x24420000},
        f"{_name}-reads-next-field": {_entry + 0x28 + _extra: 0x96030000 | (_offset + 2)},
        f"{_name}-wrong-direction": {_entry + 0x2C + _extra: 0x34840000 | ((3 - _mode) << _shift)},
        f"{_name}-no-flags-store": {_entry + 0x30 + _extra: 0},
        f"{_name}-subtracts-amount": {_entry + 0x34 + _extra: 0x00621823},
        f"{_name}-no-amount-store": {_entry + 0x38 + _extra: 0},
        f"{_name}-stores-next-field": {_entry + 0x38 + _extra: 0xA6030000 | (_offset + 2)},
    })


@dataclass(frozen=True)
class Vector:
    function: int
    execute: int
    index: int
    draw: int
    mutate: bool
    source: bytes
    target: bytes
    other: bytes
    changed: bytes


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 8, number // 8
    source, target, other, changed = (
        bytearray(rng.randbytes(HIT_SIZE + 2 * GUARD)) for _ in range(4))
    draw, mode = lane % 4, (lane // 4) % 4
    pending = (0, 19, -17, 32767, 32768, 65535, rng.getrandbits(16))[(lane // 16) % 7]
    for block, amount in ((target, pending), (changed, pending + 17)):
        put(block, GUARD + OFFSETS[function], amount, 2)
        flags = int.from_bytes(block[GUARD + 0x1C:GUARD + 0x20], "little")
        put(block, GUARD + 0x1C, (flags & ~(3 << SHIFTS[function])) | (mode << SHIFTS[function]))
    # A callback can edit the record, not its allocation's guards.
    changed[:GUARD], changed[-GUARD:] = target[:GUARD], target[-GUARD:]
    unused = (0, 1, -1, -0x80000000, 0x7FFFFFFF)
    return Vector(function, unused[(lane // 7) % 5], unused[(lane // 11) % 5],
                  draw, bool((lane // 112) % 2), bytes(source), bytes(target),
                  bytes(other), bytes(changed))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    for address, block in ((SOURCE, vector.source), (TARGET, vector.target), (OTHER, vector.other)):
        machine.store(address - GUARD, block)
    machine.put(ACTORS, TARGET)
    machine.put(DEFAULT_STACK + 16, vector.execute)
    before_draw = []

    def random_service(m: Machine) -> None:
        before_draw.append(m.bytes(TARGET - GUARD, len(vector.target)) == vector.target)
        m.trace.append((m.r[4], vector.draw))
        if vector.mutate:
            m.store(TARGET - GUARD, vector.changed)
            m.put(ACTORS, OTHER)
        m.r[5:7] = [OTHER, OTHER]  # Caller-saved pointers no longer identify the target.
        m.r[2] = vector.draw

    machine.stub(RANDOM, random_service)
    machine.r[4:8] = [0, SOURCE, TARGET, vector.index & MASK]
    machine.execute(ENTRIES[vector.function], limit=80)
    observation = {
        "source": machine.bytes(SOURCE - GUARD, len(vector.source)),
        "target": machine.bytes(TARGET - GUARD, len(vector.target)),
        "other": machine.bytes(OTHER - GUARD, len(vector.other)),
        "trace": list(machine.trace),
        "before_draw": all(before_draw),
        "stores": list(machine.stores),
    }
    inputs = [vector.function, vector.execute & MASK, vector.index & MASK, vector.draw,
              int(vector.mutate), *words(vector.source), *words(vector.target),
              *words(vector.other), *words(vector.changed)]
    return observation, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    width = (HIT_SIZE + 2 * GUARD) // 4
    if len(native) != len(NAMES) + 3 * width:
        return [f"driver printed {len(native)} values"]
    trace = observation["trace"]
    problems = field_problems(NAMES, native[:len(NAMES)],
                              [1, len(trace), *(trace[0] if trace else (0, 0)),
                               int(observation["before_draw"])])
    for index, name in enumerate(("source", "target", "other")):
        start = len(NAMES) + index * width
        if words(observation[name]) != native[start:start + width]:
            problems.append(f"{name} bytes/guards differ")
    expected_stores = [(DEFAULT_STACK - 8, 4), (DEFAULT_STACK - 4, 4),
                       (TARGET + 0x1C, 4), (TARGET + OFFSETS[vector.function], 2)]
    problems += store_problems(observation["stores"], [(address, address + size)
                                                      for address, size in expected_stores])
    if observation["stores"] != expected_stores:
        problems.append(f"original store sequence {observation['stores']} != {expected_stores}")
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = [f"Eight explicit entries [{ENTRY:08X},{END:08X}):153 instructions; "
             f"sha256={image.sha256(ENTRY, END)}"]
    for function, name in enumerate(FUNCTIONS):
        selected = [(v, obs, native) for v, obs, native in results if v.function == function]
        mismatches = sum(bool(check(image, v, obs, native, {})) for v, obs, native in selected)
        end = ENTRIES[function + 1] if function < 7 else END
        lines.append(f"{name}: {len(selected)} vectors; {mismatches} mismatches; "
                     f"[{ENTRIES[function]:08X},{end:08X}) "
                     f"{(end - ENTRIES[function]) // 4} instructions; "
                     f"sha256={image.sha256(ENTRIES[function], end)}; "
                     f"callback-mutations={sum(v.mutate for v, _, _ in selected)}; "
                     f"draws={sorted({v.draw for v, _, _ in selected})}; "
                     f"execute zero/positive/negative="
                     f"{sum(v.execute == 0 for v, _, _ in selected)}/"
                     f"{sum(v.execute > 0 for v, _, _ in selected)}/"
                     f"{sum(v.execute < 0 for v, _, _ in selected)}")
    lines.append("Controlled original 800427A8 RNG: exact bound, return and call count, target "
                 "callback mutations with clobbered caller-saved pointers, actor table retargeting; "
                 "all guarded source/target/other bytes and native typed canaries; original flags "
                 "store before halfword; no incidental V0 comparison, no real RNG algorithm claim.")
    return lines
