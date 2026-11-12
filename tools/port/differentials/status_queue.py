"""F00405/F00406/F00437-F00450 status queue primitives and fourteen rule wrappers."""

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

ID = "F00405/F00406/F00437-F00450"
ENTRY, END = 0x80081020, 0x80081130
WRAPPER_START, WRAPPER_END = 0x80083998, 0x80083B78
NEXT_START, NEXT_END = WRAPPER_END, 0x80083DEC
MODULES = (r"BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "status_queue_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\StatusQueue.cpp",)
VECTORS, SEED = 16384, 405406437
MUTANT_VECTORS = 2048

FUNCTIONS = ("F00405", "F00406", *(f"F{number:05}" for number in range(437, 451)))
ENTRIES = (ENTRY, 0x800810CC, *(WRAPPER_START + index * 0x50 for index in range(11)),
           0x80083D08, 0x80083D2C, 0x80083D7C)
RULE_IDS = (5, 7, 9, 19, 4, 6, 8, 10, 20, 11, 12, 18, 17, 29)
WARD, BANISH = 13, 15
FEEDBACK_FUNCTIONS = tuple(index for index in range(2, len(FUNCTIONS)) if index not in (WARD, BANISH))
RULE_COUNT = 32
PARAMS, ACTORS = 0x8004EECC, 0x800F1928
HIT, ACTOR, STATS = 0x80500000, 0x80501000, 0x80502000
HIT_SIZE, ACTOR_SIZE, STATS_SIZE = 0x40, 0x50, 0x970
GUARD_SIZE = 16
FEEDBACK_MASK = 0x00030000
ANALYSIS_OFFSET, ANALYSIS_BIT = 0x954, 0x00020000
NAMES = ("refused", "return", "diagnostics", "native-canaries", "native-target", "native-rules")

MUTANTS = {
    "F00405-no-exemption": {0x80081060: 0},
    "F00405-exempt-127": {0x8008105C: 0x2402007F},
    "F00405-no-blocking": {0x80081078: 0},
    "F00405-no-immunity": {0x80081090: 0},
    "F00405-wrong-status-word": {0x80081084: 0x8CA40004},
    "F00405-active-additions": {0x800810A0: 0x00804821},
    "F00405-no-reapplication": {0x80081098: 0},
    "F00405-inactive-cancellations": {0x800810B8: 0x00833825},
    "F00405-replace-added": {0x800810A4: 0x01201821},
    "F00405-replace-removed": {0x800810BC: 0x00E01021},
    "F00405-return-only-added": {0x800810C8: 0x01201021},
    "F00406-no-exemption": {0x80081108: 0},
    "F00406-wrong-status-word": {0x80081110: 0x8C840008},
    "F00406-union-not-intersection": {0x8008111C: 0x00833825},
    "F00406-replace-removed": {0x80081120: 0x00E01021},
    "F00406-return-zero": {0x8008112C: 0x00001021},
}
for _function in FEEDBACK_FUNCTIONS:
    _entry, _rule_id = ENTRIES[_function], RULE_IDS[_function - 2]
    _name = FUNCTIONS[_function]
    _positive = _function in (7, 8, 9, 10, 11, 14)
    _kept_bit = 16 if _positive else 17
    MUTANTS.update({
        f"{_name}-wrong-rule": {_entry + 0x0C: 0x24040000 | ((_rule_id + 1) % RULE_COUNT)},
        f"{_name}-feedback-keeps-bit{_kept_bit}": {
            _entry + 0x24: 0x3C02FFFD if _positive else 0x3C02FFFE},
        f"{_name}-feedback-clears-neighbor": {_entry + 0x24: 0x3C02FFF8},
        f"{_name}-wrong-feedback": {_entry + 0x34: 0x3C020001 if _positive else 0x3C020002},
        f"{_name}-feedback-on-no-change": {_entry + 0x1C: 0},
    })
MUTANTS.update({
    "F00448-wrong-rule": {0x80083D0C: 0x24040013},
    "F00448-removal-not-application": {0x80083D14: 0x0C020433},
    "F00448-lost-return": {0x80083D20: 0x00001021},
    "F00450-wrong-rule": {0x80083D88: 0x2404001C},
    "F00450-removal-not-application": {0x80083D98: 0x0C020433},
    "F00450-analyze-preview": {0x80083DA0: 0},
    "F00450-analyze-only-positive": {0x80083DA0: 0x1A00000D},
    "F00450-wrong-analyzed-bit": {0x80083DCC: 0x3C030001},
    "F00450-clear-stats-neighbors": {0x80083DD0: 0x00601021},
    "F00450-no-analysis-store": {0x80083DD4: 0},
    "F00450-wrong-return-word": {0x80083DC8: 0x8C820948},
})


@dataclass(frozen=True)
class Vector:
    function: int
    rule_id: int
    actor_id: int
    active: int
    immunities: int
    scenario: int
    execute: int
    hit: bytes
    actor: bytes
    stats: bytes
    actors: bytes
    params: bytes


def put(block: bytearray, offset: int, value: int, size: int = 4) -> None:
    block[offset:offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")


def words(block: bytes) -> list[int]:
    return [int.from_bytes(block[offset:offset + 4], "little") for offset in range(0, len(block), 4)]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % len(FUNCTIONS), number // len(FUNCTIONS)
    scenario = lane % 24 if lane < 128 else rng.randrange(32)
    rule_id = lane % RULE_COUNT if function < 2 else RULE_IDS[function - 2]
    bit = 1 << ((lane // 24 + 31) % 32)
    other = ((bit << 1) | (bit >> 31)) & MASK
    actor_id = rng.choice((0, 1, 127, 128, 129, 255))
    active, immunities = rng.getrandbits(32), rng.getrandbits(32)
    statuses, cancelled, blocking = (rng.getrandbits(32) for _ in range(3))
    added, removed, flags = (rng.getrandbits(32) for _ in range(3))
    if scenario < 24:
        actor_id, active, immunities = 1, 0, 0
        statuses, cancelled, blocking = bit, 0, 0
        if scenario == 0:
            added = removed = 0
        elif scenario == 1:
            statuses, cancelled, active = 0, bit, bit
        elif scenario == 2:
            actor_id, active, cancelled = 128, bit, bit
        elif scenario == 3:
            active, cancelled, blocking = other, other, other
        elif scenario == 4:
            statuses, immunities = MASK, MASK
        elif scenario == 5:
            active = bit
        elif scenario == 6:
            active = cancelled = bit
        elif scenario == 7:
            active, cancelled, immunities = bit, bit, MASK
        elif scenario == 8:
            statuses = 0
        elif scenario == 9:
            active = cancelled = added = removed = bit
        elif scenario == 10:
            statuses, cancelled, blocking, immunities = MASK, MASK, MASK, MASK
        elif scenario == 11:
            statuses = active = immunities = cancelled = blocking = MASK
        elif scenario == 12:
            statuses, active, immunities = 0, MASK, MASK
        elif 13 <= scenario <= 16:
            flags = (flags & ~FEEDBACK_MASK) | ((scenario - 13) << 16)
            if scenario == 13:
                immunities = MASK
            elif scenario == 14:
                statuses = 0
            elif scenario == 15:
                actor_id = 128
            else:
                active = blocking = bit
        elif 17 <= scenario <= 20:
            flags = (flags & ~FEEDBACK_MASK) | ((scenario - 17) << 16)
            statuses = 0x80000000
        elif scenario == 21:
            statuses = active = immunities = blocking = bit
        elif scenario == 22:
            statuses, active = bit | other, bit
        else:
            statuses, cancelled, active, immunities = (rng.getrandbits(32) for _ in range(4))
    else:
        # Dense independent blocking masks almost always reject: deliberately reach both sides.
        if scenario % 3:
            blocking &= ~active
        if scenario == 24:
            immunities = 0
        elif scenario == 25:
            immunities = MASK
        elif scenario == 26:
            active = 0
        elif scenario == 27:
            cancelled = statuses
        elif scenario == 28:
            cancelled = 0
        elif scenario == 29:
            added = removed = MASK
        elif scenario == 30:
            actor_id = 128

    hit = bytearray(rng.randbytes(HIT_SIZE))
    actor = bytearray(rng.randbytes(ACTOR_SIZE))
    stats = bytearray(rng.randbytes(STATS_SIZE))
    actors = bytearray(rng.randbytes(256 * 4 + 2 * GUARD_SIZE))
    params = bytearray(rng.randbytes(RULE_COUNT * 16 + 2 * GUARD_SIZE))
    target_index = lane % 256
    execute = (0, 1, -1, -0x80000000, 0x7FFFFFFF)[(lane // 24) % 5]
    if lane >= 128:
        execute = rng.choice((0, 0, 1, -1, -0x80000000, rng.getrandbits(32)))
    put(hit, 0, target_index, 1)
    put(hit, 0x14, added)
    put(hit, 0x18, removed)
    put(hit, 0x1C, flags)
    put(actor, 0x3C, STATS)
    put(stats, 0x944, immunities)
    put(stats, 0x948, active)
    put(stats, 0x957, actor_id, 1)
    put(actors, GUARD_SIZE + target_index * 4, ACTOR)
    for offset, value in ((0, statuses), (8, cancelled), (12, blocking)):
        put(params, GUARD_SIZE + rule_id * 16 + offset, value)
    return Vector(function, rule_id, actor_id, active, immunities, scenario, execute,
                  bytes(hit), bytes(actor), bytes(stats), bytes(actors), bytes(params))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    readonly = ((ACTOR, vector.actor), (ACTORS - GUARD_SIZE, vector.actors),
                (PARAMS - GUARD_SIZE, vector.params))
    machine.store(HIT, vector.hit)
    machine.store(STATS, vector.stats)
    machine.put(DEFAULT_STACK + 16, vector.execute, 4)
    for address, block in readonly:
        machine.store(address, block)
    machine.follow.update(ENTRIES[:2])
    machine.r[4], machine.r[5], machine.r[6] = vector.rule_id, HIT, HIT
    machine.execute(ENTRIES[vector.function], limit=256)
    # Store inspection also rejects writes that happen to leave the read-only bytes unchanged.
    observation = {
        "result": machine.r[2],
        "hit": machine.bytes(HIT, HIT_SIZE),
        "stats": machine.bytes(STATS, STATS_SIZE),
        "stores": list(machine.stores),
        "readonly": all(machine.bytes(address, len(block)) == block for address, block in readonly),
    }
    inputs = [vector.function, vector.rule_id, vector.actor_id, vector.active, vector.immunities,
              vector.execute & MASK, *words(vector.hit), *words(vector.params), *words(vector.stats)]
    return observation, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    expected_count = len(NAMES) + (HIT_SIZE + STATS_SIZE) // 4
    if len(native) != expected_count:
        return [f"driver printed {len(native)} values, expected {expected_count}"]
    problems = field_problems(NAMES, native[:len(NAMES)], [0, observation["result"], 0, 1, 1, 1])
    hit_end = len(NAMES) + HIT_SIZE // 4
    native_hit = b"".join(value.to_bytes(4, "little") for value in native[len(NAMES):hit_end])
    native_stats = b"".join(value.to_bytes(4, "little") for value in native[hit_end:])
    if native_hit != observation["hit"]:
        offsets = [f"{offset:#x}" for offset in range(HIT_SIZE)
                   if native_hit[offset] != observation["hit"][offset]]
        problems.append("hit bytes differ at " + ", ".join(offsets))
    if native_stats != observation["stats"]:
        problems.append("stats bytes differ")
    expected_stats = bytearray(vector.stats)
    analyze = vector.function == BANISH and vector.execute != 0
    if analyze:
        word = int.from_bytes(expected_stats[ANALYSIS_OFFSET:ANALYSIS_OFFSET + 4], "little")
        put(expected_stats, ANALYSIS_OFFSET, word | ANALYSIS_BIT)
    if observation["stats"] != expected_stats:
        problems.append("original changed stats outside executed Banish analysis")
    if not observation["readonly"]:
        problems.append("original changed actor, actor table or status-rule table/guards")

    allowed = [(HIT + 0x18, HIT + 0x1C)]
    if vector.function != 1:
        allowed.append((HIT + 0x14, HIT + 0x18))
    if vector.function in FEEDBACK_FUNCTIONS:
        allowed.extend(((HIT + 0x1C, HIT + 0x20),
                        (DEFAULT_STACK - 8, DEFAULT_STACK)))
    elif vector.function == WARD:
        allowed.append((DEFAULT_STACK - 8, DEFAULT_STACK - 4))
    elif vector.function == BANISH:
        allowed.append((DEFAULT_STACK - 16, DEFAULT_STACK - 4))
        if analyze:
            allowed.append((STATS + ANALYSIS_OFFSET, STATS + ANALYSIS_OFFSET + 4))
    problems += store_problems(observation["stores"], allowed)
    if any(size != 4 for _, size in observation["stores"]):
        problems.append("original made a non-word store")

    params = words(vector.params)
    blocking = params[GUARD_SIZE // 4 + vector.rule_id * 4 + 3]
    eligible = vector.actor_id != 128 and (vector.function == 1 or not (blocking & vector.active))
    expected_stores = []
    if vector.function in FEEDBACK_FUNCTIONS:
        expected_stores.extend(((DEFAULT_STACK - 8, 4), (DEFAULT_STACK - 4, 4)))
    elif vector.function == WARD:
        expected_stores.append((DEFAULT_STACK - 8, 4))
    elif vector.function == BANISH:
        expected_stores.extend(((DEFAULT_STACK - 12, 4), (DEFAULT_STACK - 16, 4),
                                (DEFAULT_STACK - 8, 4)))
    if eligible:
        if vector.function != 1:
            expected_stores.append((HIT + 0x14, 4))
        expected_stores.append((HIT + 0x18, 4))
    if vector.function in FEEDBACK_FUNCTIONS and native[1]:
        expected_stores.append((HIT + 0x1C, 4))
    if analyze:
        expected_stores.append((STATS + ANALYSIS_OFFSET, 4))
    if observation["stores"] != expected_stores:
        problems.append(f"store sequence {observation['stores']} != {expected_stores}")
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = [
        f"F00437-F00442 {WRAPPER_START:08X}-{WRAPPER_END - 4:08X} inclusive: "
        f"{(WRAPPER_END - WRAPPER_START) // 4} original instructions; "
        f"sha256={image.sha256(WRAPPER_START, WRAPPER_END)}",
        f"F00443-F00450 {NEXT_START:08X}-{NEXT_END - 4:08X} inclusive: "
        f"{(NEXT_END - NEXT_START) // 4} original instructions; "
        f"sha256={image.sha256(NEXT_START, NEXT_END)}",
        "The two disjoint spans contain 345 instructions total; the intervening gap is not "
        "part of this case. Wrappers execute the original primitive, with no status-logic stubs.",
    ]
    for function, name in enumerate(FUNCTIONS):
        selected = [(v, obs) for v, obs, _ in results if v.function == function]
        changed = sum(obs["result"] != 0 for _, obs in selected)
        exempt = sum(v.actor_id == 128 for v, _ in selected)
        sign_bit = sum(any(word & 0x80000000 for word in words(v.params)[4:-4]) for v, _ in selected)
        active = sum(v.active != 0 for v, _ in selected)
        all_immune = sum(v.immunities == MASK for v, _ in selected)
        feedbacks = sorted({int.from_bytes(v.hit[0x1C:0x20], "little") >> 16 & 3 for v, _ in selected})
        lines.append(f"{name}: {len(selected)} vectors, {changed} nonzero returns, "
                     f"{len(selected) - changed} zero returns, {exempt} actor-128 exemptions, "
                     f"{all_immune} all-immune, {active} active-status, {sign_bit} full-width rule tables, "
                     f"initial feedback values {feedbacks}")
        if function == BANISH:
            lines.append(f"Banish execute flags: {sorted({v.execute for v, _ in selected if v.scenario < 24})}; "
                         f"{sum(v.execute != 0 for v, _ in selected)} executed, "
                         f"{sum(v.execute == 0 for v, _ in selected)} previews")
    lines.append("Compared raw v0 via driver-only projection of typed outcomes (production mediator callbacks "
                 "are void), all hit/stats bytes, exact original store order/width/address, "
                 "read-only actor/actor-table/rule-table "
                 "images and guards, native queued/target/rule canaries and native input immutability. "
                 "Only executed Banish may change stats word 0x954, and only its analysis bit. "
                 "Missing rules are outside the original domain and are not exercised here.")
    return lines
