"""Shared harness for the loot row differentials (MENUB.PRG _lootList, #276).

Each case runs one MENUB.PRG function under the R3000 interpreter against the typed
VS::Menu::Loot core in loot_rows_driver.cpp. The original's copyItem room check is a
deterministic predicate of the row and the vector's salt on both sides, so the claim
states, the cursor, the staged inventory and the check order compare exactly.

Native differences outside the compared domain (covered by LootRowsTests.cpp): a drop
that gets no row or bag room is deferred instead of being added as row index 0xFF, the
row table refuses a 177th row, and building a batch also clears the stale selection the
original's caller left (vectors for _applyAllLootLists start with the selection at 0).
The original's staging loops never fill the last slot of a category, which native
staging uses (#128), so pre-filled inventories leave that slot and the record's needs free.
"""

from __future__ import annotations

import random

from differential import Machine, s32
from loot_staging_differential import (
    ARMOR,
    BLADE,
    CAPACITIES,
    GEM,
    GRIP,
    INVENTORY_BYTES,
    LAYOUTS,
    SHIELD,
    WEAPON,
    fill_category,
    pattern,
    set_integer,
    source_bytes,
)
from loot_staging_differential import (
    COMPILER as COMPILER,
)
from loot_staging_differential import (
    COMPILER_OPTIONS as COMPILER_OPTIONS,
)
from loot_staging_differential import (
    CXX_STANDARD as CXX_STANDARD,
)
from loot_staging_differential import (
    INCLUDES as INCLUDES,
)

MODULES = ("MENU/MENUB.PRG",)
DRIVER = "loot_rows_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Loot\LootRows.cpp",
    r"Native\Game\src\Menu\Loot\LootStaging.cpp",
    r"Native\Game\src\Items\EquipmentStats.cpp",
    r"Native\Game\src\Items\Inventory.cpp",
)

INVENTORY = 0x80500000
ROWS = 0x80520000
RECORDS = 0x80530000
ROWS_POINTER = 0x8010A6A0
INVENTORY_POINTER = 0x8010A6A8
SELECTED, OFFSET, COUNT = 0x8010A6B4, 0x8010A6B5, 0x8010A6B6
DIFFICULTY = 0x80061599
MAIN_INVENTORY = 0x80060168
FALLBACK_LOOT = 0x800EB9C8
COPY_ITEM = 0x800FEB94
SET_FLAG = 0x800BEC14
MASK = 0xFFFFFFFF

ROW_CAPACITY = 176
ROW_BYTES = ROW_CAPACITY * 4
RECORD_BYTES = 0x220
NODE_STRIDE = 0x230
LOOT_IN_NODE = 8
MAX_RECORDS = 5

REFRESH, APPEND, COUNT_CATEGORY, DIFFICULTY_MATCH, STAGE, BUILD, KEY_FLAG = range(7)
LISTING = (REFRESH, STAGE, BUILD)

# _updateLootList, _addLootToList, _droppableUnderCurrentDifficulty, _applyLootList and
# the staging functions they reach.
FOLLOWED = (
    0x801080F0, 0x8010822C, 0x801086DC, 0x801086F4,
    0x80107C54, 0x80107CE8, 0x80107D7C, 0x80107E10, 0x80107F1C, 0x80107F9C, 0x8010808C,
)

# Free room the staging of up to MAX_RECORDS records needs per category (weapon, blade,
# grip, shield, armor, gem, misc).
RECORD_NEEDS = (1, 2, 2, 1, 3, 7, 3)


def claim(salt: int, category: int, index: int) -> int:
    """The copyItem result both sides use: 0 for no room, else a slot-like value."""
    return (category * 37 + index * 11 + salt) % 5


def flags(rng: random.Random) -> int:
    return rng.choice((0, 1, 2, 3, 4, 0xFF, rng.randrange(256)))


def record_bytes(rng: random.Random) -> bytes:
    data = bytearray(RECORD_BYTES)

    def place(offset: int, kind: int, header: bool) -> None:
        salt = rng.randrange(1, 256)
        if header:
            data[offset:offset + 4] = pattern(4, salt)
            offset += 4
        record = source_bytes(kind, salt, rng.randrange(8))
        data[offset:offset + len(record)] = record

    place(0x000, WEAPON, False)
    data[0x000] = flags(rng)
    place(0x0AC, BLADE, True)
    data[0x0AC] = flags(rng)
    place(0x0DC, GRIP, True)
    data[0x0DC] = flags(rng)
    place(0x0F0, SHIELD, False)
    data[0x0F0] = flags(rng)
    for base in (0x170, 0x19C, 0x1C8):
        place(base, ARMOR, True)
        data[base] = flags(rng)
    place(0x1F4, GEM, True)
    data[0x1F4] = flags(rng)
    for item in range(3):
        base = 0x214 + item * 4
        set_integer(data, base, rng.choice((0, rng.randrange(1, 0x10000), rng.randrange(1, 0x200))), 2)
        data[base + 2] = flags(rng)
        data[base + 3] = rng.choice((0, rng.randrange(1, 256), rng.randrange(1, 100)))
    return bytes(data)


def random_rows(rng: random.Random, count: int) -> bytearray:
    rows = bytearray(rng.randbytes(ROW_BYTES))
    for row in range(count):
        rows[row * 4] = rng.choice((rng.randrange(7), rng.randrange(256)))
    return rows


def cursor_byte(rng: random.Random) -> int:
    return rng.choice((0, rng.randrange(8), rng.randrange(8), rng.randrange(256)))


def prefilled_inventory(rng: random.Random, salt: int, room: tuple[int, ...]) -> bytearray:
    inventory = bytearray(INVENTORY_BYTES)
    for category, layout in enumerate(LAYOUTS):
        # The original stops one slot short of each table (#128).
        free = CAPACITIES[category] - 1 - room[category]
        fill_category(inventory, layout, rng.randrange(free + 1), salt, weapon=category == WEAPON)
    return inventory


def count_inventory(rng: random.Random) -> bytearray:
    """Weapons, grips and shields whose gem references and socket counts stay in range."""
    inventory = bytearray(rng.randbytes(INVENTORY_BYTES))
    for weapon in range(CAPACITIES[WEAPON]):
        base = weapon * 0x20
        inventory[base + 2] = rng.randrange(1, CAPACITIES[GRIP] + 1)
        for gem in range(4):
            if rng.randrange(2):
                inventory[base + 4 + gem] = 0
    for grip in range(CAPACITIES[GRIP]):
        inventory[0x540 + grip * 0x10 + 4] = rng.randrange(5)
    for shield in range(CAPACITIES[SHIELD]):
        base = 0x100 + shield * 0x30
        inventory[base + 0x10] = rng.randrange(5)
        for gem in range(4):
            if rng.randrange(2):
                inventory[base + 0x2C + gem] = 0
    return inventory


def vector(rng: random.Random, number: int, mode: int) -> dict:
    salt = rng.randrange(256)
    value = {
        "mode": mode,
        "salt": salt,
        "difficulty": rng.choice((0, 1, 2, 3, rng.randrange(256))),
        "count": 0,
        "offset": cursor_byte(rng),
        "selected": cursor_byte(rng),
        "arg0": 0,
        "arg1": 0,
        "records": [],
        "rows": bytearray(rng.randbytes(ROW_BYTES)),
        "inventory": bytearray(INVENTORY_BYTES),
        "number": number,
    }
    if mode == REFRESH:
        value["count"] = rng.choice((0, rng.randrange(1, 9), rng.randrange(9, 41), rng.randrange(ROW_CAPACITY + 1)))
        value["rows"] = random_rows(rng, value["count"])
    elif mode == APPEND:
        value["count"] = rng.choice((0, rng.randrange(ROW_CAPACITY), ROW_CAPACITY - 1))
        value["arg0"] = rng.randrange(7)
        value["arg1"] = rng.choice((0, 1, rng.randrange(1, 65), rng.randrange(-0x80000000, 0x80000000)))
    elif mode == COUNT_CATEGORY:
        value["arg0"] = rng.choice((GEM, GEM, BLADE, GRIP, rng.randrange(8)))
        category = rng.choice((WEAPON, WEAPON, SHIELD, SHIELD, GEM, rng.randrange(8)))
        index = rng.randrange(8) if category in (WEAPON, SHIELD) else rng.randrange(256)
        value["rows"][0:2] = bytes((category, index))
        value["inventory"] = count_inventory(rng)
    elif mode == DIFFICULTY_MATCH:
        value["arg0"] = rng.choice((rng.randrange(256), rng.randrange(-0x80000000, 0x80000000)))
    elif mode == STAGE:
        value["count"] = rng.randrange(25)
        value["rows"] = random_rows(rng, value["count"])
        value["inventory"] = prefilled_inventory(rng, salt, RECORD_NEEDS)
        value["records"] = [record_bytes(rng)]
    elif mode == BUILD:
        value["offset"] = value["selected"] = 0
        value["arg0"] = rng.choice((0, 1, 1, 1))
        count = rng.randrange(1, MAX_RECORDS + 1) if value["arg0"] else 1
        value["records"] = [record_bytes(rng) for _ in range(count)]
        value["inventory"] = bytearray(rng.randbytes(INVENTORY_BYTES))
    else:
        value["arg0"] = rng.choice((
            457, 458, 459, rng.randrange(1024), rng.randrange(32000, 33000),
            rng.randrange(-0x80000000, 0x80000000),
        ))
    return value


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    calls: list[tuple[int, int, int, int]] = []
    flag_calls: list[tuple[int, int]] = []

    def copy_item(core: Machine) -> None:
        calls.append((core.r[4], core.r[6], core.r[5], core.r[7]))
        core.r[2] = claim(value["salt"], core.r[4], core.r[6])

    def set_flag(core: Machine) -> None:
        flag_calls.append((s32(core.r[4]), core.r[5]))

    def copy_bytes(core: Machine) -> None:
        core.store(core.r[4], core.bytes(core.r[5], core.r[6] & MASK))

    def zero_bytes(core: Machine) -> None:
        core.store(core.r[4], bytes(core.r[5] & MASK))

    machine.put(ROWS_POINTER, ROWS)
    machine.put(INVENTORY_POINTER, INVENTORY)
    machine.store(ROWS, bytes(value["rows"]))
    machine.store(INVENTORY, bytes(value["inventory"]))
    machine.put(SELECTED, value["selected"], 1)
    machine.put(OFFSET, value["offset"], 1)
    machine.put(COUNT, value["count"], 1)
    machine.put(DIFFICULTY, value["difficulty"], 1)
    machine.stub(COPY_ITEM, copy_item)
    machine.stub(SET_FLAG, set_flag)
    machine.stub(0x800C01C0, copy_bytes)
    machine.stub(0x800CCC1C, zero_bytes)
    machine.stub(0x800CCC34, copy_bytes)
    machine.follow.update(FOLLOWED)

    records = value["records"]
    if mode in (APPEND, DIFFICULTY_MATCH, KEY_FLAG):
        machine.r[4] = value["arg0"] & MASK
        machine.r[5] = value["arg1"] & MASK
    elif mode == COUNT_CATEGORY:
        machine.r[4] = value["arg0"]
        machine.r[5] = ROWS
    elif mode == STAGE:
        machine.store(RECORDS, records[0])
        machine.r[4] = RECORDS
    elif mode == BUILD:
        for index, record in enumerate(records):
            node = RECORDS + index * NODE_STRIDE
            following = node + NODE_STRIDE if index + 1 < len(records) else 0
            machine.put(node, following if value["arg0"] else 0)
            machine.store(node + 4, pattern(4, index))
            machine.store(node + LOOT_IN_NODE, record)
        machine.put(FALLBACK_LOOT, RECORDS + LOOT_IN_NODE)
        machine.r[4] = RECORDS if value["arg0"] else 0
    machine.execute(entry)

    if mode in LISTING:
        observation = (
            machine.byte(COUNT), machine.byte(OFFSET), machine.byte(SELECTED),
            machine.bytes(ROWS, ROW_BYTES), machine.bytes(INVENTORY, INVENTORY_BYTES), tuple(calls),
        )
    elif mode == APPEND:
        observation = (machine.byte(COUNT), machine.byte(OFFSET), machine.byte(SELECTED),
                       machine.bytes(ROWS, ROW_BYTES))
    elif mode == KEY_FLAG:
        observation = tuple(flag_calls)
    else:
        observation = s32(machine.r[2])
    inputs = [
        mode, value["salt"], value["difficulty"], value["count"], value["offset"], value["selected"],
        value["arg0"], value["arg1"], len(records),
        *value["rows"], *value["inventory"], *(byte for record in records for byte in record),
    ]
    return observation, inputs


def first_difference(label: str, native: bytes, original: bytes) -> list[str]:
    if native == original:
        return []
    first = next(i for i, (left, right) in enumerate(zip(native, original, strict=True)) if left != right)
    return [f"{label} byte {first:#x} native {native[first]:#x} original {original[first]:#x}"]


def cursor_problems(native: list[int], original: tuple) -> list[str]:
    names = ("count", "offset", "selected")
    return [f"{name} native {left} original {right}"
            for name, left, right in zip(names, native, original, strict=True) if left != right]


def check(image, value: dict, observation, native: list[int], reference) -> list[str]:
    mode = value["mode"]
    if mode in LISTING:
        count, offset, selected, rows, inventory, calls = observation
        fixed = 3 + ROW_BYTES + INVENTORY_BYTES + 1
        if len(native) < fixed or len(native) != fixed + 2 * native[fixed - 1]:
            return [f"native printed {len(native)} values"]
        problems = cursor_problems(native[:3], (count, offset, selected))
        problems += first_difference("row", bytes(native[3:3 + ROW_BYTES]), rows)
        problems += first_difference(
            "inventory", bytes(native[3 + ROW_BYTES:fixed - 1]), inventory)
        native_calls = [tuple(native[fixed + 2 * i:fixed + 2 * i + 2]) for i in range(native[fixed - 1])]
        original_calls = [(category, index) for category, index, _, _ in calls]
        if native_calls != original_calls:
            problems.append(f"room checks native {native_calls[:6]} original {original_calls[:6]}")
        if any(main != MAIN_INVENTORY or staged != INVENTORY for _, _, main, staged in calls):
            problems.append("original copyItem was not given Ashley's and the working inventories")
        return problems
    if mode == APPEND:
        if len(native) != 4 + ROW_BYTES:
            return [f"native printed {len(native)} values"]
        problems = [] if native[0] == 1 else ["native refused the row"]
        problems += cursor_problems(native[1:4], observation[:3])
        return problems + first_difference("row", bytes(native[4:]), observation[3])
    if mode == COUNT_CATEGORY:
        if native != [observation, 0]:
            return [f"count native {native} original {observation}"]
        return []
    if mode == DIFFICULTY_MATCH:
        return [] if native == [observation] else [f"match native {native} original {observation}"]
    expected = [1, observation[0][0]] if observation else [0, 0]
    if len(observation) > 1 or (observation and observation[0][1] != 1):
        return [f"original raised {observation}"]
    return [] if native == expected else [f"key flag native {native} original {expected}"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one LootRows function compared in `mode`."""
    return {
        "ID": f"F{function:05}", "ENTRY": entry, "END": end,
        "DRIVER": DRIVER, "INCLUDES": INCLUDES, "SOURCES": SOURCES, "MODULES": MODULES,
        "COMPILER": COMPILER, "COMPILER_OPTIONS": COMPILER_OPTIONS,
        "CXX_STANDARD": CXX_STANDARD,
        "VECTORS": 256, "SEED": function, "MUTANT_VECTORS": 64, "MUTANTS": mutants,
        "vector": lambda rng, number: vector(rng, number, mode),
        "run": lambda machine, value: run(machine, value, entry),
        "check": check,
    }
