"""Shared original MENU3/MENUB inventory-detail policy comparisons."""

from __future__ import annotations

import functools
import random

from differential import DEFAULT_STACK, MODULE_BASES, Fault, s32, store_problems

MODULES = (r"MENU\MENU3.PRG", r"MENU\MENUB.PRG")
MODULE_BASES.update(dict.fromkeys(MODULES, 0x80102800))
DRIVER = "inventory_detail_helpers_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\Inventory\DetailsPolicies.cpp",)
CAPACITIES = (8, 16, 16, 8, 16, 48, 64, 2)
MEMORY, MEMORY_BYTES = 0x800F4EE8, 0xF2
ORDER_TABLE, ORDER = 0x801022A8, 0x80700000
ROWS, ROW_BYTES, ROW_COUNT = (0x80500000, 0x80510000), 64, 40
HELD, REPEAT, PRESSED = 0x8005E238, 0x8005DFDC, 0x8005E1D0
COUNT, INVALID, CLEAR, ROW, CONTINUE, PAGES, PRIMARY, LEAVE, DISMISS, BARS = range(1, 11)
CALLEES = {
    COUNT: 0x800FEA6C, INVALID: 0x800C02E0, CLEAR: 0x800FA8E0,
    ROW: 0x800C8E48, PAGES: 0x800FBBD4, PRIMARY: 0x800FBEA4,
    LEAVE: 0x800C02E8, DISMISS: 0x800FA810, BARS: 0x800FC268,
}
FIELDS = (
    (0x80109711, 0x80109712, 0x80109713, 0x80109568, 0x80109717, 0x80109718),
    (0x8010A6B9, 0x8010A6BA, 0x8010A6BB, 0x8010A50C, 0x8010A6BF, 0x8010A6C0),
)
FIELD_SIZES = (1, 1, 1, 4, 1, 4)
RANGES = {
    "F02566": ((0x8010296C, 0x80102988), (0x80104F98, 0x80104FB4)),
    "F02567": ((0x80102988, 0x80102A3C), (0x80102A60, 0x80102B14)),
    "F02569": ((0x80102B78, 0x80102BE4), (0x80102C50, 0x80102CBC)),
    "F02571": ((0x80102C44, 0x80102C94), (0x80102D1C, 0x80102D6C)),
    "F02573": ((0x80102D30, 0x80102D7C), (0x80102E08, 0x80102E54)),
    "F02574": ((0x80102D7C, 0x80102DEC), (0x80102E54, 0x80102EC4)),
}
FUNCTIONS = tuple(RANGES)
MUTATIONS = {
    "F02566": {"title-store": (1, 0), "category-store": (3, 0), "command-store": (6, 0)},
    "F02567": {
        "held-mask": (7, 0x30420008), "count-null": (12, 0x24050001),
        "left-button": (17, 0x30620001), "left-step": (20, 0x26020000),
        "right-button": (22, 0x30620002), "right-step": (25, 0x26100002),
        "wrap-branch": (27, 0x10400002), "wrap-subtract": (29, 0x02008021),
        "pressed-mask": (32, 0x30420004), "invalid-sound": (37, 0),
    },
    "F02569": {
        "clear-call": (4, 0), "clear-except": (5, 0x24040027),
        "category-row": (7, 0x2404001E), "category-state": (10, 0x24020002),
        "category-y": (12, 0x24020013), "selected-row": (9, 0x00002021),
        "selected-state": (16, 0x24020003), "selected-x": (18, 0x2402009A),
        "selected-flag": (20, 0x24020000), "subtext-clear": (22, 0),
    },
    "F02571": {
        "preview-state": (4, 0x24020002), "preview-store": (5, 0),
        "categories-store": (7, 0), "command-store": (11, 0),
        "continue-call": (10, 0), "pages-call": (12, 0),
        "pages-argument": (13, 0x00002021), "primary-open": (15, 0x24040002),
    },
    "F02573": {
        "cursor-category": (2, 0x2482001F), "cursor-stride": (3, 0x00021080),
        "row-store": (7, 0), "page-store": (8, 0),
        "order-stride": (11, 0x000420C0), "page-index": (15, 0x00401021),
        "signed-slot": (16, 0x80420000),
    },
    "F02574": {
        "leave-sound": (3, 0), "clear-call": (5, 0),
        "command-row": (8, 0x24040000), "pages-close": (10, 0x24040000),
        "primary-close": (12, 0x24040001), "header-placement": (14, 0x24020001),
        "header-store": (15, 0), "exit-mask": (19, 0x30420020),
        "optional-bars": (20, 0x16000003), "exit-store": (21, 0),
        "bars-call": (22, 0), "bars-command": (23, 0x24040000),
    },
}


def configuration(identifier: str, overlay: int) -> dict:
    entry, end = RANGES[identifier][overlay]
    return {
        "ID": identifier, "ENTRY": entry, "END": end, "MODULES": (MODULES[overlay],),
        "DRIVER": DRIVER, "SOURCES": SOURCES, "VECTORS": 1024,
        "SEED": int(identifier[1:]) + overlay * 100000,
        "MUTANTS": {name: {entry + index * 4: word}
                    for name, (index, word) in MUTATIONS[identifier].items()},
        "vector": functools.partial(vector, FUNCTIONS.index(identifier)),
        "run": functools.partial(run, identifier, overlay), "check": check,
        "describe": functools.partial(describe, identifier, overlay),
    }


def vector(kind: int, rng: random.Random, number: int) -> dict:
    count = rng.choice((0, 1, 2, 8, 16, 48, 64))
    category = number % 7
    page = rng.choice((0, count - 1, count, count * 2, -1, 0x7FFFFFFF, -0x80000000,
                       s32(rng.getrandbits(32))))
    row = (0, 31, 39, 20, rng.randrange(40))[number % 5]
    held = rng.getrandbits(32) & ~12 | (number % 4) << 2
    repeat = rng.getrandbits(32)
    pressed = rng.getrandbits(32)
    after_repeat = rng.getrandbits(32) & ~12 | (number // 4 % 4) << 2
    after_pressed = rng.getrandbits(32) & ~12 | (number // 16 % 4) << 2
    if kind == 1 and number % 10 == 0:
        category, held = -1, held | 12
    first, second = (
        (rng.choice((0, 1, 2, 128, 255, -1, s32(rng.getrandbits(32)))), 0),
        (category, page),
        (row, 0),
        (row, (3, 4, 7, -1, 0, 255)[number % 6]),
        (category, rng.randrange(CAPACITIES[category])),
        ((0, 1, -1, -2)[number % 4], 0),
    )[kind]
    state = [rng.randrange(256), rng.randrange(256), rng.randrange(256),
             s32(rng.getrandbits(32)), rng.randrange(256), s32(rng.getrandbits(32))]
    memory = rng.randbytes(MEMORY_BYTES) if kind == 4 else b""
    order = rng.randbytes(64) if kind == 4 else b""
    rows = rng.randbytes(ROW_BYTES * ROW_COUNT * 2) if kind == 2 else b""
    if kind == 4 and number % 4 == 0:
        second = (0, CAPACITIES[category] - 1)[number // 4 % 2]
    return {"kind": kind, "args": [first, second], "count": count,
            "held": held, "repeat": repeat, "pressed": pressed,
            "after_repeat": after_repeat, "after_pressed": after_pressed,
            "mode": number % 3, "state": state, "memory": memory, "order": order, "rows": rows}


def run(identifier: str, overlay: int, machine, item: dict) -> tuple:
    fields = FIELDS[overlay]
    for address, value, size in zip(fields, item["state"], FIELD_SIZES, strict=True):
        machine.put(address, value, size)
    for address, value in ((HELD, item["held"]), (REPEAT, item["repeat"]), (PRESSED, item["pressed"])):
        machine.put(address, value)
    kind, owner, row_reads = item["kind"], 0, 0
    if kind == 4:
        machine.store(MEMORY, item["memory"])
        for category in range(7):
            machine.put(ORDER_TABLE + 4 * category, ORDER + 0x100 * category)
            machine.store(ORDER + 0x100 * category, item["order"])
    if kind == 2:
        for n, address in enumerate(ROWS):
            machine.store(address, item["rows"][n * ROW_BYTES * ROW_COUNT:(n + 1) * ROW_BYTES * ROW_COUNT])

    def state() -> list[int]:
        return [machine.load(address, size, size == 4)
                for address, size in zip(fields, FIELD_SIZES, strict=True)]

    def callback(m, operation: int) -> None:
        nonlocal owner, row_reads
        argument = 0 if operation in (INVALID, LEAVE) else s32(m.r[4])
        extra = s32(m.r[5]) if operation == COUNT else 0
        m.trace.append([operation, argument, extra, *state()])
        if operation == COUNT:
            m.r[2] = item["count"] & 0xFFFFFFFF
            m.put(REPEAT, item["after_repeat"])
            m.put(PRESSED, item["after_pressed"])
        elif operation == CLEAR and kind == 2 and item["mode"] == 1:
            owner = 1
        elif operation == ROW:
            if not 0 <= argument < ROW_COUNT:
                raise Fault(f"menu row {argument} outside the stub's real row table")
            m.r[2] = ROWS[owner] + argument * ROW_BYTES
            row_reads += 1
            if item["mode"] == 2 and row_reads == 1:
                owner = 1
        elif operation == PRIMARY:
            if item["mode"] == 1:
                m.put(PRESSED, item["after_pressed"])
            elif item["mode"] == 2:
                m.put(fields[1], 0x93, 1)
                m.put(fields[2], 0xA7, 1)

    for operation, address in CALLEES.items():
        machine.stub(address, functools.partial(callback, operation=operation))
    machine.stub((0x80102BE4, 0x80102CBC)[overlay], functools.partial(callback, operation=CONTINUE))
    machine.r[4], machine.r[5] = (value & 0xFFFFFFFF for value in item["args"])
    machine.execute(RANGES[identifier][overlay][0])
    memory = machine.bytes(MEMORY, MEMORY_BYTES) if kind == 4 else b""
    rows = b"".join(machine.bytes(address, ROW_BYTES * ROW_COUNT) for address in ROWS) if kind == 2 else b""
    result = s32(machine.r[2]) if kind in (1, 4) else 0
    allowed = [(DEFAULT_STACK - 0x40, DEFAULT_STACK),
               *((address, address + size) for address, size in zip(fields, FIELD_SIZES, strict=True))]
    if kind == 4:
        allowed.append((MEMORY, MEMORY + MEMORY_BYTES))
    if kind == 2:
        allowed.extend((address, address + ROW_BYTES * ROW_COUNT) for address in ROWS)
    observation = {"values": [result, *state()], "memory": memory, "rows": rows,
                   "calls": machine.trace, "stores": machine.stores, "allowed": allowed}
    inputs = [kind, *item["args"], item["count"], item["held"], item["repeat"], item["pressed"],
              item["after_repeat"], item["after_pressed"], item["mode"], *item["state"],
              *item["memory"], *item["order"], *item["rows"]]
    return observation, inputs


def check(image, item: dict, observation: dict, native: list[int], reference: dict) -> list[str]:
    values, memory_end = native[:7], 7 + len(observation["memory"])
    memory = bytes(native[7:memory_end])
    rows_end = memory_end + len(observation["rows"])
    rows = bytes(native[memory_end:rows_end])
    calls = [native[rows_end + 1 + n * 9:rows_end + 1 + (n + 1) * 9] for n in range(native[rows_end])]
    problems = []
    if values != observation["values"]:
        problems.append(f"values {values} != {observation['values']}")
    if memory != observation["memory"]:
        problems.append("complete cursor-memory image differs")
    if rows != observation["rows"]:
        problems.append("complete row-owner images differ")
    if calls != observation["calls"]:
        problems.append(f"callback trace/snapshots {calls[:3]} != {observation['calls'][:3]}")
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(identifier: str, overlay: int, image, results: list) -> list[str]:
    items = [item for item, _, _ in results]
    return [
        f"{MODULES[overlay]} independent instantiation; source and relocation equivalence audited separately.",
        "Bounded original callbacks stubbed: inventory count, menu sounds, clear rows, get row, continue transition, "
        "stat pages, primary stats, command flyout and DP/PP bars. Their used arguments, order and live-field snapshots compared.",
        "Compared all six scalar fields; F02573 the entire 0xF2-byte memory record; F02569 both complete forty-row "
        "64-byte PS1 images, including retained subtext handles. Native row pointer alignment is not treated as disk layout.",
        f"Biased vectors: {sum(item['mode'] == 1 for item in items)} clear/primary owner-input callbacks; "
        f"{sum(item['mode'] == 2 for item in items)} row-owner/callback-write cases; "
        f"{sum(item['held'] & 12 == 12 for item in items)} simultaneous-shoulder inputs.",
    ]
