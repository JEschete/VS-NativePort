"""F00510/511/512/548: resource cost, real callee previews, recovery and restrictions.

Only original-defined weapon resource types 1..4 are compared numerically.
Empty/natural 0/0 and unsupported weapon combinations are native policy tests:
the original consumes unassigned a3 there (#352), not a fourth argument.
"""

import collections
import random
import struct
from dataclasses import dataclass

from differential import DEFAULT_STACK, DISC, MASK, Image, Machine, store_problems

ID = "F00510-F00512/F00548"
ENTRY, END = 0x8008574C, 0x80085A34
ENTRIES = (ENTRY, 0x80085978, 0x800859B4, 0x8008A5D0)
ENDS = (0x80085978, 0x800859B4, END, 0x8008A6FC)
FUNCTIONS = ("F00510", "F00511", "F00512", "F00548")
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "action_costs_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\ActionCosts.cpp",)
VECTORS, MUTANT_VECTORS, SEED = 16384, 2048, 510548
STATS, ACTOR, OTHER, STATE = 0x80500000, 0x80600000, 0x80600100, 0x80700000
ACTORS, SELECTED, DEFINITIONS = 0x800F1928, 0x800F19CC, 0x8004B9DC
FIELDS = ((0x18, 2), (0x1C, 2), (0x20, 2), (0x14C, 2), (0x954, 2), (0x948, 4))
MUTANTS = {
    "F00510-MP-unsigned": {0x800857A8: 0x94A7001C},
    "F00510-HP-no-reserve": {0x800857CC: 0},
    "F00510-PP-signed": {0x800857D0: 0x84A7014C},
    "F00510-weapon-wrong-cost": {0x800857E4: 0x90A90147},
    "F00510-weapon-HP-no-reserve": {0x80085848: 0},
    "F00510-chain-threshold-11": {0x80085880: 0x2C62000B},
    "F00510-chain-first-23": {0x8008588C: 0x2482FFE9},
    "F00510-chain-count-17": {0x80085890: 0x2C420011},
    "F00510-chain-wide-product": {0x800858A0: 0x00001810},
    "F00510-chain-divisor-shift": {0x800858B4: 0x00031082},
    "F00510-no-HP-reserve-release": {0x800858D4: 0},
    "F00510-risk-no-saturation": {0x800858EC: 0},
    "F00510-risk-not-allowed": {0x800858F0: 0},
    "F00510-preview-writes": {0x800858F4: 0},
    "F00510-no-MP-store": {0x80085948: 0},
    "F00510-no-risk-store": {0x80085954: 0},
    "F00510-no-HP-store": {0x8008595C: 0},
    "F00510-no-PP-store": {0x80085960: 0},
    "F00510-packed-OR": {0x80085974: 0x00491025},
    "F00511-no-cost-call": {0x8008599C: 0},
    "F00511-preview-updates": {0x800859A0: 0x24060001},
    "F00512-ignore-execute": {0x800859D0: 0},
    "F00512-exclusion-31": {0x800859DC: 0x2C42001F},
    "F00512-fallback-254": {0x800859E4: 0x240200FE},
    "F00512-bonus-mask-31": {0x80085A14: 0x3042001F},
    "F00512-shift-7": {0x80085A20: 0x000319C0},
    "F00512-no-store": {0x80085A28: 0},
    "F00548-no-cost-call": {0x8008A63C: 0},
    "F00548-no-recovery-flag": {0x8008A624: 0x0040802B},
    "F00548-no-cost-flag": {0x8008A654: 0},
    "F00548-no-unlock-flag": {0x8008A674: 0},
    "F00548-edge-140": {0x8008A670: 0x2A42008C},
    "F00548-edge-223": {0x8008A68C: 0x2A4200DF},
    "F00548-wrong-silence": {0x8008A688: 0x30422000},
    "F00548-no-drawn-flag": {0x8008A6BC: 0},
    "F00548-wrong-numbness": {0x8008A6C8: 0x30421000},
}


@dataclass
class Vector:
    function: int
    action: int
    kind: int
    cost: int
    weapon_type: int
    weapon_cost: int
    hp: int
    mp: int
    risk: int
    pp: int
    chain: int
    execute: int
    timer: int
    bonus: int
    delay: int
    weapon_delay: int
    statuses: int
    drawn: int
    slot: int
    unlocked: int
    mutate: int
    null_stats: int
    block: bytes

    def inputs(self) -> list[int]:
        return [value & MASK for value in vars(self).values() if isinstance(value, int)]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 4, number // 4
    action = (0, 1, 21, 22, 39, 40, 53, 54, 140, 141, 223, 224, 255)[lane % 13]
    kind = (lane // 13) % 8
    cost = (0, 1, 10, 255)[lane % 4]
    execute = (0, 1, MASK, 0x80000000)[(lane // 3) % 4]
    if function == 2 and lane % 17 == 0:
        action, execute = MASK, 0  # unused invalid definition and owner
    resource = (-32768, -1, 0, 1, 9, 10, 11, 255, 256, 32767, rng.randrange(-32768, 32768))
    result = Vector(
        function, action, kind, cost, 1 + (lane // 7) % 4,
        (0, 1, 10, 255)[(lane // 5) % 4], resource[lane % len(resource)],
        resource[(lane // 2) % len(resource)],
        (0, 89, 90, 91, 100, 101, 65535)[(lane // 3) % 7],
        (0, 9, 10, 11, 32767, 32768, 65535)[(lane // 5) % 7],
        (0, 9, 10, 65535, 65536, MASK, 0x80000000, rng.getrandbits(32))[(lane // 2) % 8],
        execute, (0, 1, 65535)[lane % 3], (0, 1, 63)[(lane // 4) % 3],
        (0, 1, 254, 255)[(lane // 3) % 4], (0, 7, 255)[(lane // 5) % 3],
        (0, 0x1000, 0x2000, 0x3000)[(lane // 7) % 4],
        (0, 1, 2, MASK)[(lane // 11) % 4], (0, 1, 40)[(lane // 2) % 3],
        lane % 2, int(function == 3 and lane % 3 == 1),
        int(function in (0, 1) and kind in (0, 5, 7) and cost % 2 == 0),
        rng.randbytes(0x960))
    if function == 3 and action == 140:
        result.slot, result.unlocked, result.mutate = 0, 1, 0
        result.statuses, result.drawn = 0x1000, 2
    return result


def run(machine: Machine, v: Vector) -> tuple[dict, list[int]]:
    machine.store(STATS, v.block)
    for (offset, size), value in zip(FIELDS, (v.hp, v.mp, v.risk, v.pp, v.timer, v.statuses), strict=True):
        machine.put(STATS + offset, value, size)
    for offset, value in ((0x146, v.weapon_cost), (0x147, v.weapon_delay), (0x14B, v.weapon_type)):
        machine.put(STATS + offset, value, 1)
    machine.put(STATS + 0x956, v.bonus << 2, 1)
    machine.map_zero(ACTOR, 0x200)
    machine.put(ACTOR + 0x3C, 0 if v.null_stats else STATS)
    machine.put(ACTOR + 0x20, v.drawn)
    machine.put(OTHER + 0x20, v.drawn ^ 2)
    machine.put(OTHER + 0x3C, STATS + 0x1000)
    machine.put(ACTORS + v.slot * 4, ACTOR)
    machine.put(SELECTED, STATE)
    machine.put(STATE + 4, v.chain)
    definition = DEFINITIONS + v.action * 52
    if v.action < 256:
        machine.put(definition, v.kind << 17 | v.cost << 24)
        machine.put(definition + 12, v.delay | v.unlocked << 15)
    before = machine.bytes(STATS, 0x960)
    calls = []
    original_call, original_step = machine.call, machine.step

    def observe_call(address: int) -> bool:
        if address == ENTRY:
            calls.append([machine.r[4], machine.r[5], machine.r[6]])
        return original_call(address)

    def observe_step(pc: int):
        if pc == 0x8008A644 and v.mutate:
            machine.put(STATS + 0x954, v.timer ^ 1, 2)
            machine.put(STATS + 0x948, v.statuses ^ 0x3000)
            machine.put(definition + 12, v.delay | (v.unlocked ^ 1) << 15)
            machine.put(ACTORS + v.slot * 4, OTHER)
        return original_step(pc)

    machine.call, machine.step = observe_call, observe_step
    machine.follow.add(ENTRY)
    actor = 0 if v.action == MASK or v.null_stats else STATS
    args = ((v.action, actor, v.execute), (v.action, v.slot, 0),
            (v.action, actor, v.execute), (v.slot, v.action, 0))[v.function]
    machine.r[4:7] = args
    machine.r[7] = 0xA3A3A3A3  # never a formal cost argument
    machine.execute(ENTRIES[v.function], limit=1000)
    fields = [machine.load(STATS + off, size) for off, size in FIELDS]
    result = machine.r[2] if v.function in (0, 3) else 0
    stores = [(a, s) for a, s in machine.stores if not DEFAULT_STACK - 0x40 <= a < DEFAULT_STACK]
    return {"result": result, "before": before, "after": machine.bytes(STATS, 0x960),
            "fields": fields, "calls": calls, "stores": stores}, v.inputs()


def check(image: Image, v: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    expected = [observation["result"], *observation["fields"], len(observation["calls"]),
                *(field for call in observation["calls"] for field in call)]
    problems = [] if native == expected else [f"result/resources/recovery/calls differ: {native} != {expected}"]
    wanted = bytearray(observation["before"])
    for (offset, size), value in zip(FIELDS, native[1:7], strict=True):
        wanted[offset:offset + size] = value.to_bytes(size, "little")
    if wanted != observation["after"]:
        problems.append("original altered unrelated stat bytes")
    allowed = [(STATS + off, STATS + off + size) for off, size in FIELDS[:5]]
    problems += store_problems(observation["stores"], allowed)
    effective = v.weapon_type if v.kind == 6 else v.kind
    expected_store = []
    if v.function == 0 and v.execute and effective in (1, 2, 3, 4):
        expected_store = [(STATS + {1: 0x1C, 2: 0x20, 3: 0x18, 4: 0x14C}[effective], 2)]
    if v.function == 2 and v.execute and ((v.action - 22) & MASK) >= 32:
        expected_store = [(STATS + 0x954, 2)]
    if observation["stores"] != expected_store:
        problems.append("wrong stores, including unchanged insufficient-resource writes")
    return problems


def audit_data() -> list[str]:
    counts, natural_actions = collections.Counter(), collections.Counter()
    main = Image.load("SLUS_010.40")
    zones = 0
    for path in sorted((DISC / "MAP").glob("ZONE*.ZND")):
        zones += 1
        data = path.read_bytes()
        offset, size = struct.unpack_from("<II", data, 8)
        if not size:
            continue
        count = struct.unpack_from("<I", data, offset)[0]
        start = offset + 4 + count * 8
        if start + count * 0x464 > offset + size or offset + size > len(data):
            raise ValueError(f"{path.name}: invalid CharacterInit bounds")
        for index in range(count):
            record = data[start + index * 0x464:start + (index + 1) * 0x464]
            kind, cost = record[0x45:0x47]
            counts[(kind, cost)] += 1
            if (kind, cost) == (0, 0):
                if struct.unpack_from("<H", record, 0x34)[0] or record[0x37]:
                    raise ValueError("zero-cost natural weapon unexpectedly has a blade/model")
                actions = [struct.unpack_from("<H", record, 0x238 + limb * 0x5C + 16 + slot * 4)[0]
                           for limb in range(6) for slot in range(4)]
                base = any(0 < a < 256 and main.read(DEFINITIONS + a * 52) >> 17 & 7 == 6 for a in actions)
                natural_actions[(struct.unpack_from("<h", record, 28)[0] > 0, base)] += 1
    base_ids = [a for a in range(256) if main.read(DEFINITIONS + a * 52) >> 17 & 7 == 6]
    return [f"Read-only {zones} ZNDs, {sum(counts.values())} CharacterInit records: {sorted(counts.items())}",
            f"Zero-cost/type0 records all blade/model0; (positiveHP,baseAttack) counts {dict(natural_actions)}; "
            f"256x52 SLUS definitions at {DEFINITIONS:08X}: base-attack IDs {base_ids}",
            "Producer: original146C.c:968/1257/6286, Items EquipmentStats.cpp BuildWeaponStatsFromParts/"
            "LoadInventoryWeaponStats and ActorSpawningLive::BuildTemplateEquipment; no cost normalization. "
            "Natural and unequipped 0/0 explicitly require no resource and cause no payment. "
            "This is a native policy, not a numeric original-register match or an attack-eligibility claim."]


def describe(image: Image, results: list) -> list[str]:
    lines = []
    for index, (name, start, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, obs, native) for v, obs, native in results if v.function == index]
        lines.append(f"{name}: {len(selected)} vectors; "
                     f"{sum(bool(check(image, v, obs, native, {})) for v, obs, native in selected)} mismatches; "
                     f"[{start:08X},{end:08X}) {(end-start)//4} instructions SHA256={image.sha256(start, end)}")
    return lines + audit_data()
