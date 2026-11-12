"""F00405/F00406/F00437-F00442 status queue primitives and six stat-rule wrappers."""

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

ID = "F00405/F00406/F00437-F00442"
ENTRY, END = 0x80081020, 0x80081130
WRAPPER_START, WRAPPER_END = 0x80083998, 0x80083B78
MODULES = (r"BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "status_queue_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\StatusQueue.cpp",)
VECTORS, SEED = 8192, 405406437
MUTANT_VECTORS = 1024

FUNCTIONS = ("F00405", "F00406", "F00437", "F00438", "F00439", "F00440", "F00441", "F00442")
ENTRIES = (ENTRY, 0x800810CC, *(WRAPPER_START + index * 0x50 for index in range(6)))
RULE_IDS = (5, 7, 9, 19, 4, 6)
RULE_COUNT = 32
PARAMS, ACTORS = 0x8004EECC, 0x800F1928
HIT, ACTOR, STATS = 0x80500000, 0x80501000, 0x80502000
HIT_SIZE, ACTOR_SIZE, STATS_SIZE = 0x40, 0x50, 0x970
GUARD_SIZE = 16
FEEDBACK_MASK = 0x00030000
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
for _index, (_entry, _rule_id) in enumerate(zip(ENTRIES[2:], RULE_IDS, strict=True)):
    _name = FUNCTIONS[_index + 2]
    _kept_bit = 16 if _index == 5 else 17
    MUTANTS.update({
        f"{_name}-wrong-rule": {_entry + 0x0C: 0x24040000 | ((_rule_id + 1) % RULE_COUNT)},
        f"{_name}-feedback-keeps-bit{_kept_bit}": {
            _entry + 0x24: 0x3C02FFFD if _index == 5 else 0x3C02FFFE},
        f"{_name}-feedback-clears-neighbor": {_entry + 0x24: 0x3C02FFF8},
        f"{_name}-wrong-feedback": {_entry + 0x34: 0x3C020001 if _index == 5 else 0x3C020002},
        f"{_name}-feedback-on-no-change": {_entry + 0x1C: 0},
    })


@dataclass(frozen=True)
class Vector:
    function: int
    rule_id: int
    actor_id: int
    active: int
    immunities: int
    scenario: int
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
    return Vector(function, rule_id, actor_id, active, immunities, scenario,
                  bytes(hit), bytes(actor), bytes(stats), bytes(actors), bytes(params))


def run(machine: Machine, vector: Vector) -> tuple[dict, list[int]]:
    readonly = ((ACTOR, vector.actor), (STATS, vector.stats),
                (ACTORS - GUARD_SIZE, vector.actors), (PARAMS - GUARD_SIZE, vector.params))
    machine.store(HIT, vector.hit)
    for address, block in readonly:
        machine.store(address, block)
    machine.follow.update(ENTRIES[:2])
    machine.r[4], machine.r[5], machine.r[6] = vector.rule_id, HIT, HIT
    machine.execute(ENTRIES[vector.function], limit=256)
    # Store inspection also rejects writes that happen to leave the read-only bytes unchanged.
    observation = {
        "result": machine.r[2],
        "hit": machine.bytes(HIT, HIT_SIZE),
        "stores": list(machine.stores),
        "readonly": all(machine.bytes(address, len(block)) == block for address, block in readonly),
    }
    inputs = [vector.function, vector.rule_id, vector.actor_id, vector.active, vector.immunities,
              *words(vector.hit), *words(vector.params)]
    return observation, inputs


def check(image: Image, vector: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    expected_count = len(NAMES) + HIT_SIZE // 4
    if len(native) != expected_count:
        return [f"driver printed {len(native)} values, expected {expected_count}"]
    problems = field_problems(NAMES, native[:len(NAMES)], [0, observation["result"], 0, 1, 1, 1])
    native_hit = b"".join(value.to_bytes(4, "little") for value in native[len(NAMES):])
    if native_hit != observation["hit"]:
        offsets = [f"{offset:#x}" for offset in range(HIT_SIZE)
                   if native_hit[offset] != observation["hit"][offset]]
        problems.append("hit bytes differ at " + ", ".join(offsets))
    if not observation["readonly"]:
        problems.append("original changed actor, stats, actor table or status-rule table/guards")

    allowed = [(HIT + 0x18, HIT + 0x1C)]
    if vector.function != 1:
        allowed.append((HIT + 0x14, HIT + 0x18))
    if vector.function >= 2:
        allowed.extend(((HIT + 0x1C, HIT + 0x20),
                        (DEFAULT_STACK - 8, DEFAULT_STACK)))
    problems += store_problems(observation["stores"], allowed)
    if any(size != 4 for _, size in observation["stores"]):
        problems.append("original made a non-word store")

    params = words(vector.params)
    blocking = params[GUARD_SIZE // 4 + vector.rule_id * 4 + 3]
    eligible = vector.actor_id != 128 and (vector.function == 1 or not (blocking & vector.active))
    expected_stores = []
    if vector.function >= 2:
        expected_stores.extend(((DEFAULT_STACK - 8, 4), (DEFAULT_STACK - 4, 4)))
    if eligible:
        if vector.function != 1:
            expected_stores.append((HIT + 0x14, 4))
        expected_stores.append((HIT + 0x18, 4))
    if vector.function >= 2 and native[1]:
        expected_stores.append((HIT + 0x1C, 4))
    if observation["stores"] != expected_stores:
        problems.append(f"store sequence {observation['stores']} != {expected_stores}")
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = [
        f"F00437-F00442 {WRAPPER_START:08X}-{WRAPPER_END - 4:08X} inclusive: "
        f"{(WRAPPER_END - WRAPPER_START) // 4} original instructions; "
        f"sha256={image.sha256(WRAPPER_START, WRAPPER_END)}",
        "The two disjoint spans contain 188 instructions total; the intervening gap is not "
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
    lines.append("Compared raw v0 (wrappers flatten optional feedback << 16), all hit bytes, "
                 "exact original store order/width/address, read-only actor/stats/actor-table/rule-table "
                 "images and guards, native queued/target/rule canaries and native input immutability. "
                 "Missing rules are outside the original domain and are not exercised here.")
    return lines
