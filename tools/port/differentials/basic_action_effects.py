"""F00469-F00475: source outcome, instant death, armor wear and script flag mediators."""

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

ID = "F00469-F00475"
ENTRY, END = 0x80084228, 0x80084390
ENTRIES = (ENTRY, 0x80084248, 0x800842AC, 0x800842F0, 0x80084310, 0x80084340, 0x80084370)
FUNCTIONS = tuple(f"F{number:05}" for number in range(469, 476))
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "basic_action_effects_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\BasicActionEffects.cpp",
           r"Native\Game\src\Battle\StatusQueue.cpp")
VECTORS, SEED, MUTANT_VECTORS = 14336, 469475, 1792
SOURCE, TARGET, ACTOR, STATS = 0x80500000, 0x80501000, 0x80502000, 0x80503000
ACTORS, PARAMS = 0x800F1928, 0x8004EECC
HIT_SIZE, GUARD = 0x44, 16
REMOVAL, FLAG_SERVICE = 0x800810CC, 0x800BEC14
NAMES = ("refused", "native-canaries", "native-inputs", "flag-count", "flag-index", "flag-value")

MUTANTS = {}
for _function in (0, 3, 6):
    _entry, _name = ENTRIES[_function], FUNCTIONS[_function]
    MUTANTS.update({
        f"{_name}-keeps-old-bit26": {_entry: 0x3C03F7FF},
        f"{_name}-wrong-outcome": {_entry + 0x10: 0x3C030400},
        f"{_name}-no-store": {_entry + 0x1C: 0},
    })
MUTANTS.update({
    "F00470-rule29": {0x80084254: 0x2404001D},
    "F00470-no-removal": {0x8008425C: 0},
    "F00470-currentHP-upper-half": {0x8008428C: 0x9463001A},
    "F00470-clears-neighbor": {0x80084270: 0x2404FFF8},
    "F00470-wrong-hp-mode": {0x80084290: 0x34420002},
    "F00470-no-HP-store": {0x80084298: 0},
    "F00471-undoubled-global": {0x800842BC: 0x00021000},
    "F00471-undoubled-limb": {0x800842D0: 0x00021000},
    "F00471-wrong-limb-stride": {0x800842C0: 0x00031840},
    "F00471-overwrites-limbHP": {0x800842D4: 0xA4620020},
    "F00471-keeps-old-mode-bit": {0x800842AC: 0x3C04FFBF},
    "F00471-wrong-mode": {0x800842DC: 0x3C030080},
})
for _function, _flag in ((4, 64), (5, 32)):
    _entry, _name = ENTRIES[_function], FUNCTIONS[_function]
    MUTANTS.update({
        f"{_name}-executes-preview": {_entry + 0x0C: 0},
        f"{_name}-only-positive": {_entry + 0x0C: 0x18400004},
        f"{_name}-wrong-index": {_entry + 0x14: 0x24040000 | (_flag + 1)},
        f"{_name}-wrong-value": {_entry + 0x1C: 0x24050002},
        f"{_name}-missing-call": {_entry + 0x18: 0},
    })


@dataclass(frozen=True)
class Vector:
    function: int
    execute: int
    source: bytes
    target: bytes
    actor: bytes
    stats: bytes
    actors: bytes
    rules: bytes


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 7, number // 7
    source, target = (bytearray(rng.randbytes(HIT_SIZE + 2 * GUARD)) for _ in range(2))
    actor = bytearray(rng.randbytes(0x50 + 2 * GUARD))
    stats = bytearray(rng.randbytes(0x970 + 2 * GUARD))
    actors = bytearray(rng.randbytes(42 * 4 + 2 * GUARD))
    rules = bytearray(rng.randbytes(32 * 16 + 2 * GUARD))
    slot, limb = lane % 42, (lane // 8) % 6
    hp = (-32768, -16385, -1, 0, 1, 16384, 32767, rng.randrange(-32768, 32768))[lane % 8]
    current_hp = (0, 1, 32767, 32768, 65535, 65536, -1, rng.getrandbits(32))[lane % 8]
    identity = (0, 128, 255)[(lane // 8) % 3]
    active = (0, MASK, 0x80000001, rng.getrandbits(32))[lane % 4]
    immunities = (0, MASK, rng.getrandbits(32))[lane % 3]
    statuses = (MASK, 0, 0x80000001, rng.getrandbits(32))[(lane // 4) % 4]
    put(source, GUARD + 0x1C, (words(source)[11] & ~0x0C000000) | ((lane % 4) << 26))
    put(target, GUARD, slot, 1)
    put(target, GUARD + 1, limb, 1)
    put(target, GUARD + 4, hp, 2)
    put(stats, GUARD + 0x18, current_hp)
    put(stats, GUARD + 0x957, identity, 1)
    put(stats, GUARD + 0x948, active)
    put(stats, GUARD + 0x944, immunities)
    put(actor, GUARD + 0x3C, STATS)
    put(actors, GUARD + slot * 4, ACTOR)
    put(rules, GUARD + 30 * 16, statuses)
    execute = (0, 1, -1, -0x80000000, 0x7FFFFFFF)[lane % 5]
    return Vector(function, execute, bytes(source), bytes(target), bytes(actor),
                  bytes(stats), bytes(actors), bytes(rules))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    blocks = ((SOURCE - GUARD, vector.source), (TARGET - GUARD, vector.target),
              (ACTOR - GUARD, vector.actor), (STATS - GUARD, vector.stats),
              (ACTORS - GUARD, vector.actors), (PARAMS - GUARD, vector.rules))
    for address, block in blocks:
        machine.store(address, block)
    machine.put(DEFAULT_STACK + 16, vector.execute)
    machine.follow.add(REMOVAL)

    def flag_service(m: Machine) -> None:
        m.trace.append((m.r[4], m.r[5]))
        m.r[2] = 0xD00DFEED  # No mediator return-value contract.

    machine.stub(FLAG_SERVICE, flag_service)
    machine.r[4:8] = [0, SOURCE, TARGET, 0xDEADBEEF]
    machine.execute(ENTRIES[vector.function], limit=192)
    observation = {
        "source": machine.bytes(SOURCE - GUARD, len(vector.source)),
        "target": machine.bytes(TARGET - GUARD, len(vector.target)),
        "trace": list(machine.trace),
        "stores": list(machine.stores),
        "readonly": all(machine.bytes(address, len(block)) == block for address, block in blocks[2:]),
    }
    inputs = [vector.function, vector.execute & MASK, *words(vector.source), *words(vector.target),
              *words(vector.stats), *words(vector.rules)]
    return observation, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    count = len(NAMES) + (HIT_SIZE + 2 * GUARD) // 2
    if len(native) != count:
        return [f"driver printed {len(native)} values, expected {count}"]
    trace = observation["trace"]
    expected = [0, 1, 1, len(trace), *(trace[0] if trace else (0, 0))]
    problems = field_problems(NAMES, native[:len(NAMES)], expected)
    split = len(NAMES) + len(vector.source) // 4
    for name, output in (("source", native[len(NAMES):split]), ("target", native[split:])):
        if words(observation[name]) != output:
            problems.append(f"{name} bytes/guards differ")
    if not observation["readonly"]:
        problems.append("original changed actor/stats/actor-table/rules or guards")
    expected_stores = []
    if vector.function in (0, 3, 6):
        expected_stores = [(SOURCE + 0x1C, 4)]
    elif vector.function == 1:
        expected_stores = [(DEFAULT_STACK - 8, 4), (DEFAULT_STACK - 4, 4)]
        if vector.stats[GUARD + 0x957] != 128:
            expected_stores.append((TARGET + 0x18, 4))
        expected_stores.extend(((TARGET + 0x1C, 4), (TARGET + 4, 2)))
    elif vector.function == 2:
        limb = vector.target[GUARD + 1]
        expected_stores = [(TARGET + 0x3C, 2), (TARGET + 0x22 + limb * 4, 2), (TARGET + 0x1C, 4)]
    else:
        expected_stores = [(DEFAULT_STACK - 8, 4)]
    problems += store_problems(observation["stores"], [(address, address + size)
                                                      for address, size in expected_stores])
    if observation["stores"] != expected_stores:
        problems.append(f"store sequence {observation['stores']} != {expected_stores}")
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = [f"Seven original entries [{ENTRY:08X},{END:08X}): 90 instructions; "
             f"sha256={image.sha256(ENTRY, END)}",
             f"Real status-removal helper [{REMOVAL:08X},80081130):25 instructions; "
             f"sha256={image.sha256(REMOVAL, 0x80081130)}"]
    for function, name in enumerate(FUNCTIONS):
        selected_results = [(v, obs, native) for v, obs, native in results if v.function == function]
        selected = [v for v, _, _ in selected_results]
        mismatches = sum(bool(check(image, v, obs, native, {})) for v, obs, native in selected_results)
        end = ENTRIES[function + 1] if function < 6 else END
        lines.append(f"{name}: {len(selected)} vectors; {mismatches} mismatches; "
                     f"[{ENTRIES[function]:08X},{end:08X}) "
                     f"{(end - ENTRIES[function]) // 4} instructions; "
                     f"sha256={image.sha256(ENTRIES[function], end)}; "
                     f"execute zero/positive/negative="
                     f"{sum(v.execute == 0 for v in selected)}/"
                     f"{sum(v.execute > 0 for v in selected)}/"
                     f"{sum(v.execute < 0 for v in selected)}")
    lines.append("Compared all guarded source/target bytes, exact original store order/address/width, "
                 "read-only actor/stats/table/rule images, native typed canaries and immutable inputs. "
                 "Instant death follows the actual original status-removal MIPS with rule30; flags "
                 "use the controlled original service with exact index/value/call count. No incidental "
                 "v0 comparison. Signed limb indices outside0..5 are checked-native refusals only.")
    return lines
