"""Shared R3000 vectors and native driver protocol for B50-01 results stats (MENUF.PRG 3B8.c:2880-2963)."""

from __future__ import annotations

import random

from differential import MASK, MODULE_BASES, Machine, s32, store_problems

MODULE = "MENU/MENUF.PRG"
MODULE_BASES[MODULE] = 0x80102800

CHARACTER_STATE = 0x800F19FC  # vs_battle_characterState
ACTOR = 0x80420000
STATS = 0x80421000
STATS_POINTER = 0x3C  # vs_battle_actor.unk3C
STATS_SIZE = 0x30
CAP = 999

# (name, offset, signed) of the ten driver fields, in protocol order.
FIELDS = (
    ("current_hp", 0x18, True),
    ("max_hp", 0x1A, True),
    ("current_mp", 0x1C, True),
    ("max_mp", 0x1E, True),
    ("strength", 0x22, False),
    ("total_strength", 0x24, False),
    ("intelligence", 0x26, False),
    ("total_intelligence", 0x28, False),
    ("agility", 0x2A, False),
    ("total_agility", 0x2C, False),
)
FIELD_INDEX = {name: index for index, (name, _, _) in enumerate(FIELDS)}

# operation, entry, end (the next function's start), permanent and current fields.
SPECS = {
    "F03066": (0, 0x80108EA8, 0x80108EC8, "total_strength", None),
    "F03067": (1, 0x80108EC8, 0x80108EE8, "total_intelligence", None),
    "F03068": (2, 0x80108EE8, 0x80108F08, "total_agility", None),
    "F03069": (3, 0x80108F08, 0x80108F98, "total_strength", "strength"),
    "F03070": (4, 0x80108F98, 0x80109028, "total_intelligence", "intelligence"),
    "F03071": (5, 0x80109028, 0x801090B8, "total_agility", "agility"),
    "F03072": (6, 0x801090B8, 0x80109148, "max_hp", "current_hp"),
    "F03073": (7, 0x80109148, 0x801091D8, "max_mp", "current_mp"),
}
NAMES = {
    "F03066": "_getTotalStrength (3B8.c:2880)",
    "F03067": "_getTotalIntelligence (3B8.c:2882)",
    "F03068": "_getTotalAgility (3B8.c:2887)",
    "F03069": "_raiseMaxStrength (3B8.c:2889)",
    "F03070": "_raiseMaxIntelligence (3B8.c:2904)",
    "F03071": "_raiseMaxAgility (3B8.c:2919)",
    "F03072": "_raiseMaxHP (3B8.c:2934)",
    "F03073": "_raiseMaxMP (3B8.c:2949)",
}

# Encodings: lhu/lh v0,off(v1|v0); sltiu/slti v0,v0,imm; addiu v0,zero,imm; addu/subu v0,v0,a2.
MUTATIONS = {
    "F03066": {
        "F03066-reads-current-strength": {0x80108EBC: 0x94420022},
        "F03066-sign-extends": {0x80108EBC: 0x84420024},
        "F03066-reads-total-intelligence": {0x80108EBC: 0x94420028},
    },
    "F03067": {
        "F03067-reads-current-intelligence": {0x80108EDC: 0x94420026},
        "F03067-sign-extends": {0x80108EDC: 0x84420028},
        "F03067-reads-total-strength": {0x80108EDC: 0x94420024},
    },
    "F03068": {
        "F03068-reads-current-agility": {0x80108EFC: 0x9442002A},
        "F03068-sign-extends": {0x80108EFC: 0x8442002C},
        "F03068-reads-total-intelligence": {0x80108EFC: 0x94420028},
    },
    "F03069": {
        "F03069-total-cap-test-1000": {0x80108F40: 0x2C4203E9},
        "F03069-total-adds-to-current-strength": {0x80108F20: 0x94620022},
        "F03069-total-clamps-to-1000": {0x80108F48: 0x240203E8},
        "F03069-total-not-stored": {0x80108F2C: 0x00000000},
        "F03069-current-subtracts": {0x80108F68: 0x00461023},
        "F03069-current-not-clamped": {0x80108F8C: 0x00000000},
        "F03069-current-cap-test-1000": {0x80108F80: 0x2C4203E9},
    },
    "F03070": {
        "F03070-total-cap-test-1000": {0x80108FD0: 0x2C4203E9},
        "F03070-total-adds-to-current-intelligence": {0x80108FB0: 0x94620026},
        "F03070-total-clamps-to-1000": {0x80108FD8: 0x240203E8},
        "F03070-total-not-stored": {0x80108FBC: 0x00000000},
        "F03070-current-subtracts": {0x80108FF8: 0x00461023},
        "F03070-current-not-clamped": {0x8010901C: 0x00000000},
        "F03070-current-cap-test-1000": {0x80109010: 0x2C4203E9},
    },
    "F03071": {
        "F03071-total-cap-test-1000": {0x80109060: 0x2C4203E9},
        "F03071-total-adds-to-current-agility": {0x80109040: 0x9462002A},
        "F03071-total-clamps-to-1000": {0x80109068: 0x240203E8},
        "F03071-total-not-stored": {0x8010904C: 0x00000000},
        "F03071-current-subtracts": {0x80109088: 0x00461023},
        "F03071-current-not-clamped": {0x801090AC: 0x00000000},
        "F03071-current-cap-test-1000": {0x801090A0: 0x2C4203E9},
    },
    "F03072": {
        "F03072-max-cap-test-1000": {0x801090F0: 0x284203E9},
        "F03072-max-unsigned-cap-test": {0x801090F0: 0x2C4203E8},
        "F03072-max-zero-extended-reload": {0x801090E8: 0x9462001A},
        "F03072-max-clamps-to-1000": {0x801090F8: 0x240203E8},
        "F03072-current-subtracts": {0x80109118: 0x00461023},
        "F03072-current-not-clamped": {0x8010913C: 0x00000000},
        "F03072-current-unsigned-cap-test": {0x80109130: 0x2C4203E8},
    },
    "F03073": {
        "F03073-max-cap-test-1000": {0x80109180: 0x284203E9},
        "F03073-max-unsigned-cap-test": {0x80109180: 0x2C4203E8},
        "F03073-max-zero-extended-reload": {0x80109178: 0x9462001E},
        "F03073-max-clamps-to-1000": {0x80109188: 0x240203E8},
        "F03073-current-subtracts": {0x801091A8: 0x00461023},
        "F03073-current-not-clamped": {0x801091CC: 0x00000000},
        "F03073-current-unsigned-cap-test": {0x801091C0: 0x2C4203E8},
    },
}

EDGE_FIELDS = (0, 1, 2, 500, 998, 999, 1000, 1001, 0x7FFF, 0x8000, 0x8001, 0xFC18, 0xFFFE, 0xFFFF)
EDGE_AMOUNTS = (0, 1, 2, 5, 10, 20, -1, -2, 999, 1000, -999, -1000, 0x7FFF, 0x8000, 0xFFFF,
                0x10000, 0x10005, -0x10000, 0x7FFFFFFF, -0x80000000)


def field_value(rng: random.Random, number: int) -> int:
    choice = (number + rng.randrange(4)) % 4
    if choice == 0:
        return rng.choice(EDGE_FIELDS)
    if choice == 1:
        return rng.randrange(1000)
    if choice == 2:
        return rng.randrange(990, 1010)
    return rng.getrandbits(16)


def amount_for(rng: random.Random, number: int, value: int) -> int:
    pattern = number % 6
    if pattern == 0:
        return EDGE_AMOUNTS[number // 6 % len(EDGE_AMOUNTS)]
    if pattern == 1:
        return 1000 - value + rng.choice((-1, 0, 0, 1))  # Lands on the cap boundary.
    if pattern == 2:
        return rng.randrange(-60, 61)
    if pattern == 3:
        return 0x8000 - value + rng.choice((-1, 0, 1))  # Lands on the sign boundary.
    if pattern == 4:
        return s32(rng.getrandbits(32))
    return rng.randrange(0, 1000)


def vector_for(identifier: str, rng: random.Random, number: int) -> list[int]:
    operation, _, _, permanent, current = SPECS[identifier]
    fields = [field_value(rng, number + index) for index in range(len(FIELDS))]
    amount = 0
    if current is not None:
        target = FIELD_INDEX[permanent if number % 2 else current]
        amount = s32(amount_for(rng, number // 2, fields[target]))
    available = 0 if number % 16 == 15 else 1
    return [operation, amount, available, *fields]


def signed16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def run_original(machine: Machine, values: list[int]) -> tuple[dict, list[int]]:
    operation, amount, available, *fields = values
    inputs = [
        operation,
        amount,
        available,
        *(signed16(value) if signed else value & 0xFFFF
          for value, (_, _, signed) in zip(fields, FIELDS, strict=True)),
    ]
    if not available:
        # The native host refuses a missing character/stat record; the original would dereference it.
        return {"refused": True}, inputs
    entry = next(spec[1] for spec in SPECS.values() if spec[0] == operation)
    rng = random.Random(hash(tuple(values)) & MASK)
    machine.store(STATS, bytes(rng.getrandbits(8) for _ in range(STATS_SIZE)))
    for value, (_, offset, _) in zip(fields, FIELDS, strict=True):
        machine.put(STATS + offset, value, 2)
    machine.put(CHARACTER_STATE, ACTOR)
    machine.map_zero(ACTOR, 0x100)
    machine.put(ACTOR + STATS_POINTER, STATS)
    machine.r[4] = amount & MASK
    machine.r[2] = 0xDEADBEEF
    machine.execute(entry)
    return {
        "refused": False,
        "return": machine.r[2] & MASK,
        "fields": [machine.load(STATS + offset, 2, signed=signed) for _, offset, signed in FIELDS],
        "stores": list(machine.stores),
    }, inputs


def check_original(_image, values: list[int], observation: dict, native: list[int], _reference: dict) -> list[str]:
    if len(native) != 2 + len(FIELDS):
        return [f"invalid native protocol {native}"]
    if observation["refused"]:
        return [] if native[0] == 1 else ["native accepted a missing stat record"]
    if native[0] != 0:
        return ["native refused a valid stat record"]
    operation = values[0]
    identifier = next(key for key, spec in SPECS.items() if spec[0] == operation)
    _, _, _, permanent, current = SPECS[identifier]
    problems = []
    if current is None and native[1] & MASK != observation["return"]:
        problems.append(f"return native {native[1]:#x} original {observation['return']:#x}")
    for (name, _, _), mine, theirs in zip(FIELDS, native[2:], observation["fields"], strict=True):
        if mine != theirs:
            problems.append(f"{name}: native {mine} original {theirs}")
    allowed = []
    if current is not None:
        for name in (permanent, current):
            offset = FIELDS[FIELD_INDEX[name]][1]
            allowed.append((STATS + offset, STATS + offset + 2))
    return problems + store_problems(observation["stores"], allowed)


def build_case(identifier: str) -> dict:
    _, entry, end, permanent, current = SPECS[identifier]

    def vector(rng: random.Random, number: int) -> list[int]:
        return vector_for(identifier, rng, number)

    def describe(_image, results) -> list[str]:
        refused = sum(bool(observation["refused"]) for _, observation, _ in results)
        lines = [
            f"{NAMES[identifier]}: executed {(end - entry) // 4} original instructions including the return delay slot.",
            "Compared the ten HP/MP/STR/INT/AGI halfwords of the stat record, the return of the reads, and every "
            "original store (reads may store nothing; bonuses only their two fields).",
            f"{refused} vectors are documented refusals (missing character or stat record) the original would "
            "dereference; native must refuse exactly those.",
        ]
        if current is not None:
            executed = [(v, o) for v, o, _ in results if not o["refused"]]
            index = (FIELD_INDEX[permanent], FIELD_INDEX[current])
            capped = [sum(o["fields"][i] == CAP for _, o in executed) for i in index]
            lines.append(f"{permanent} ended at the cap in {capped[0]} vectors, {current} in {capped[1]}.")
        return lines

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "MODULES": (MODULE,),
        "DRIVER": "results_stats_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\Results\ResultsStats.cpp",),
        "VECTORS": 1024,
        "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 1024,
        "MUTANTS": MUTATIONS[identifier],
        "vector": vector,
        "run": run_original,
        "check": check_original,
        "describe": describe,
    }
