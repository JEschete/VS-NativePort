"""Inventory row materialization runs the original MENU3/MENUB items2.h bodies."""

import functools

from differential import DEFAULT_STACK, MODULE_BASES, Fault, s32, store_problems

MODULE_BASES.update(
    {
        r"MENU\MENU3.PRG": 0x80102800,
        r"MENU\MENUB.PRG": 0x80102800,
        r"MENU\MAINMENU.PRG": 0x800F9800,
    }
)
IDS = ("F02593", "F02594", "F02595", "F02596")
# Inclusive starts and exclusive ends from the overlays' disassembly. MENUB
# compiles items2.h without _ITEMMENU, so its row builder is 35 instructions
# shorter and everything after it moves by 0x68 instead of 0xF4.
RANGES = {
    "menu3": (
        (0x80104EC0, 0x80104F50),
        (0x80104F50, 0x80104F94),
        (0x80104F94, 0x80105314),
        (0x80105314, 0x801053EC),
    ),
    "menub": (
        (0x80104FB4, 0x80105044),
        (0x80105044, 0x80105088),
        (0x80105088, 0x8010537C),
        (0x8010537C, 0x80105454),
    ),
}
OVERLAYS = {"menu3": r"MENU\MENU3.PRG", "menub": r"MENU\MENUB.PRG"}
CAPACITIES = (8, 16, 16, 8, 16, 48, 64, 2)
OFFSETS = (0, 0x280, 0x540, 0x100, 0x640, 0x8C0, 0xE00)
STRIDES = (32, 44, 16, 48, 40, 28, 4)
# vs_mainMenu_draw*Row formatters by category (MAINMENU.PRG); stubbed endpoints.
FORMATTERS = (
    0x800FCA08,
    0x800FCAA4,
    0x800FCC0C,
    0x800FCE40,
    0x800FD084,
    0x800FD0E0,
    0x800FD17C,
)
INVENTORY, MEMORY_BYTES = 0x80060168, 0xF00
CAPACITY, ROUTES = 0x801022A0, 0x801022A8
MISC_MAP, MISC_MAP_ENTRIES, FIRST_MISC = 0x80102214, 134, 323
MISC_ACTIONS, ACTION_TABLE, ACTION_STRIDE, ACTIONS = 0x8010971C, 0x8004B9DC, 52, 140
ANIMATION, CURSOR_MEMORY, CURSOR_BYTES = 0x801023D0, 0x800F4EE8, 128
ADVANCE, GET_ROW = 0x80100A5C, 0x800C8E48
ORDER_AREA, MENU_TEXT, ROW_TYPES, TEXT = 0x80B00000, 0x80B10000, 0x80B20000, 0x80B30000
ROWS, ROW_STRIDE, LIST_ROWS = 0x80B40000, 0x40, 20
ROW_BUFFER = 64
SOURCES = (
    r"Native\Game\src\Menu\Inventory\ItemListRows.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
# Static original-instruction mutants per overlay: {name: (address, word)}.
MUTATIONS = {
    "menu3": (
        {
            "blade-offset": (0x80104EEC, 0x904302AB),
            "blade-stride": (0x80104EE0, 0x00451021),
            "grip-offset": (0x80104F0C, 0x9443054E),
            "gem-flag": (0x80104F38, 0x30620040),
            "gem-keep": (0x80104F3C, 0x14400002),
        },
        {
            "gem-category": (0x80104F50, 0x24020006),
            "gem-offset": (0x80104F74, 0x946308DA),
            "gem-flag": (0x80104F7C, 0x30620040),
            "clear-skip": (0x80104F80, 0x10400002),
            "slot-mask": (0x80104F90, 0x306200FF),
        },
        {
            "equipped-flag": (0x8010515C, 0x3413CB00),
            "armor-equipped": (0x80105148, 0x92020667),
            "action-base": (0x801051CC, 0x2482FF97),
            "action-store": (0x8010522C, 0x2442971D),
            "weapon-assembled-flag": (0x80105264, 0x3413CD00),
            "shield-assembled-flag": (0x8010529C, 0x3413CD00),
            "row-type-stride": (0x801052DC, 0x26520008),
        },
        {
            "animation-limit": (0x80105354, 0x2C420011),
            "slide-state": (0x80105364, 0x24130003),
            "idle-state": (0x80105368, 0x24120000),
            "cursor-base": (0x80105374, 0x2622001F),
            "target-offset": (0x8010539C, 0x9462001A),
            "selected-row": (0x801053B0, 0x24420015),
        },
    ),
    "menub": (
        {
            "blade-offset": (0x80104FE0, 0x904302AB),
            "blade-stride": (0x80104FD4, 0x00451021),
            "grip-offset": (0x80105000, 0x9443054E),
            "gem-flag": (0x8010502C, 0x30620040),
            "gem-keep": (0x80105030, 0x14400002),
        },
        {
            "gem-category": (0x80105044, 0x24020006),
            "gem-offset": (0x80105068, 0x946308DA),
            "gem-flag": (0x80105070, 0x30620040),
            "clear-skip": (0x80105074, 0x10400002),
            "slot-mask": (0x80105084, 0x306200FF),
        },
        {
            "equipped-flag": (0x80105248, 0x3413CB00),
            "armor-equipped": (0x80105234, 0x92020667),
            "weapon-assembled-flag": (0x801052C8, 0x3413CD00),
            "shield-assembled-flag": (0x80105300, 0x3413CD00),
            "count-step": (0x80105310, 0x25080002),
            "row-type-stride": (0x80105344, 0x26520008),
        },
        {
            "animation-limit": (0x801053BC, 0x2C420011),
            "slide-state": (0x801053CC, 0x24130003),
            "idle-state": (0x801053D0, 0x24120000),
            "cursor-base": (0x801053DC, 0x2622001F),
            "target-offset": (0x80105404, 0x9462001A),
            "selected-row": (0x80105418, 0x24420015),
        },
    ),
}


def configuration(kind, overlay):
    """Publish static metadata only; importing and listing never reads game data."""
    entry, end = RANGES[overlay][kind]
    return {
        "ID": IDS[kind],
        "ENTRY": entry,
        "END": end,
        "MODULES": (OVERLAYS[overlay], r"MENU\MAINMENU.PRG"),
        "DRIVER": "inventory_rows_driver.cpp",
        "SOURCES": SOURCES,
        "CXX_STANDARD": "c++latest",
        "VECTORS": 384,
        "MUTANT_VECTORS": 96,
        "SEED": int(IDS[kind][1:]) + (1000 if overlay == "menub" else 0),
        "MUTANTS": {
            name: {address: word}
            for name, (address, word) in MUTATIONS[overlay][kind].items()
        },
        "vector": functools.partial(vector, kind, overlay),
        "run": functools.partial(run, kind, overlay),
        "check": check,
        "describe": functools.partial(describe, kind, overlay),
    }


def put16(block, offset, value):
    block[offset : offset + 2] = value.to_bytes(2, "little")


def inventory(rng, overlay, linked):
    """Random records; linked lists keep parents and misc ids in the defined domain."""
    block = bytearray(rng.randbytes(MEMORY_BYTES))
    for row in range(8):
        block[row * 32 + 3] = rng.choice((0, 0, 1, 0x80))
        block[0x100 + row * 48 + 1] = rng.choice((0, 0, 1, 0x40))
    for row in range(16):
        block[0x640 + row * 40 + 0x26] = rng.choice((0, 0, 1, 4))
    if not linked:
        return block
    for row in range(16):
        block[0x280 + row * 44 + 0x2A] = rng.choice((0, 0, *range(1, 9)))
        put16(block, 0x540 + row * 16 + 0xC, rng.choice((0, 0, *range(1, 9))))
    for row in range(48):
        slot = rng.randrange(9)
        put16(block, 0x8C0 + row * 28 + 0x18, rng.choice((slot, 0x80 | slot, 0x8080 | slot)))
    if overlay == "menu3":
        for row in range(64):
            put16(block, 0xE00 + row * 4, rng.randrange(FIRST_MISC, FIRST_MISC + MISC_MAP_ENTRIES))
    return block


def parent_category(rng, number):
    """Every category, plus foreign and wrapped negative ones the bodies treat as other."""
    if number % 4 == 3:
        return rng.choice((-1, 7, 8, 0x7FFFFFFF, -0x80000000, 0x10000005))
    return (1, 2, 5, 0, 3, 4, 6)[number % 7]


def rows_order(rng, category, number):
    size = CAPACITIES[category]
    if category == 7:
        entries = [rng.randrange(1, 256) for _ in range(size)]
    elif number % 3:
        entries = rng.sample(range(1, size + 1), size)
    else:
        entries = [rng.randrange(1, size + 1) for _ in range(size)]
    if number % 5 == 1:
        entries[rng.randrange(size)] = 0
    capacity = size if number % 4 else rng.randrange(size + 1)
    return entries, capacity


def vector(kind, overlay, rng, number):
    """Cover each category and branch inside the original bodies' defined domain."""
    if kind < 2:
        category = parent_category(rng, number)
        return {
            "category": category,
            "index": rng.randrange(CAPACITIES[category]) if category in (1, 2, 5) else rng.randrange(-4, 64),
            "inventory": bytes(inventory(rng, overlay, False)),
        }
    if kind == 2:
        category = number % 8
        order, capacity = rows_order(rng, category, number)
        unlocked = [rng.randrange(2) for _ in range(ACTIONS)]
        return {
            "category": category,
            "order": order,
            "capacity": capacity,
            "unlocked": unlocked,
            "words": [rng.getrandbits(32) & ~0x8000 | bit << 15 for bit in unlocked],
            "formatted": [rng.getrandbits(32) for _ in range(ROW_BUFFER)],
            "row_types": [rng.getrandbits(32) for _ in range(ROW_BUFFER)],
            "actions": list(rng.randbytes(ROW_BUFFER)),
            "inventory": bytes(inventory(rng, overlay, True)),
        }
    category = rng.randrange(-30, 34) if number % 6 else 0x7FFFFFE2 + rng.randrange(34)
    category = s32(category)
    index = (category + 30) * 2 & 0xFFFFFFFF
    memory = bytearray(rng.randbytes(CURSOR_BYTES))
    memory[index] = rng.randrange(24)
    identity = number % 4 != 3
    rows = []
    for _ in range(LIST_ROWS):
        rows.append(
            [rng.choice((0, 1, 2, 2, 3, 5)), rng.randrange(256), rng.getrandbits(16), rng.getrandbits(16)]
        )
    return {
        "category": category,
        "index": index,
        "animation": rng.choice((16, 17, 255, *range(0, 16))),
        "step": rng.randrange(1, 4),
        "mutate_at": rng.randrange(26),
        "mutate_value": rng.randrange(256),
        "memory": list(memory),
        "row_map": list(range(LIST_ROWS)) if identity else [rng.randrange(LIST_ROWS) for _ in range(LIST_ROWS)],
        "rows": rows,
        "filler": rng.randbytes(LIST_ROWS * ROW_STRIDE),
    }


def run_parent(machine, item, entry):
    machine.store(INVENTORY, item["inventory"])
    machine.r[4:6] = [item["category"] & 0xFFFFFFFF, item["index"] & 0xFFFFFFFF]
    machine.execute(entry)
    inputs = [item["category"], item["index"], *item["inventory"]]
    allowed = [(DEFAULT_STACK - 0x1000, DEFAULT_STACK + 0x10)]
    return {"values": [machine.r[2]], "stores": machine.stores, "allowed": allowed, "problems": []}, inputs


def run_rows(machine, item, entry, overlay):
    category = item["category"]
    machine.store(INVENTORY, item["inventory"])
    machine.store(ORDER_AREA, bytes(item["order"]))
    machine.put(CAPACITY + category, item["capacity"], 1)
    machine.put(ROUTES + category * 4, ORDER_AREA)
    machine.map_zero(ACTION_TABLE, ACTIONS * ACTION_STRIDE)
    for action, word in enumerate(item["words"]):
        machine.put(ACTION_TABLE + action * ACTION_STRIDE + 0xC, word)
    for row, value in enumerate(item["row_types"]):
        machine.put(ROW_TYPES + row * 4, value)
    machine.store(MISC_ACTIONS, bytes(item["actions"]))
    calls, problems = [], []

    def formatter(table):
        def handler(m):
            offset, row_offset = m.r[4] - INVENTORY - OFFSETS[table], m.r[6] - ROW_TYPES
            row = row_offset // 4
            slot = offset // STRIDES[table]
            if (
                offset % STRIDES[table]
                or row_offset % 4
                or m.r[5] != MENU_TEXT + row * 8
                or m.r[7] != TEXT + row * 96
            ):
                problems.append(f"formatter {table} arguments {m.r[4:8]}")
            calls.append([table, slot, row, 1])
            if not 0 <= row < ROW_BUFFER:
                raise Fault(f"formatter row {row} outside the row type buffer")
            m.put(m.r[6], item["formatted"][row])

        return handler

    for table, address in enumerate(FORMATTERS):
        machine.stub(address, formatter(table))
    machine.follow.update((RANGES[overlay][0][0], RANGES[overlay][1][0]))
    machine.r[4:8] = [category, MENU_TEXT, ROW_TYPES, TEXT]
    machine.execute(entry)
    misc_map = list(machine.bytes(MISC_MAP, MISC_MAP_ENTRIES))
    values = [
        s32(machine.r[2]),
        *(machine.load(ROW_TYPES + row * 4) for row in range(ROW_BUFFER)),
        *machine.bytes(MISC_ACTIONS, ROW_BUFFER),
        len(calls),
        *(value for call in calls for value in call),
    ]
    allowed = [
        (DEFAULT_STACK - 0x1000, DEFAULT_STACK + 0x10),
        (ROW_TYPES, ROW_TYPES + ROW_BUFFER * 4),
        (MISC_ACTIONS, MISC_ACTIONS + ROW_BUFFER),
    ]
    inputs = [
        0 if overlay == "menu3" else 1,
        category,
        item["capacity"],
        len(item["order"]),
        *item["order"],
        *misc_map,
        *item["unlocked"],
        *item["formatted"],
        *item["row_types"],
        *item["actions"],
        *item["inventory"],
    ]
    return {"values": values, "stores": machine.stores, "allowed": allowed, "problems": problems}, inputs


def run_restore(machine, item, entry):
    machine.store(CURSOR_MEMORY, bytes(item["memory"]))
    machine.put(ANIMATION, item["animation"], 1)
    machine.store(ROWS, item["filler"])
    for row, (state, selected, x, target) in enumerate(item["rows"]):
        base = ROWS + row * ROW_STRIDE
        machine.put(base, state, 1)
        machine.put(base + 6, selected, 1)
        machine.put(base + 0x14, x, 2)
        machine.put(base + 0x18, target, 2)
    advances, lookups, problems = [0], [0], []

    def advance(m):
        m.put(ANIMATION, m.load(ANIMATION, 1) + item["step"], 1)
        advances[0] += 1

    def get_row(m):
        if m.r[4] != LIST_ROWS + lookups[0]:
            problems.append(f"row lookup {m.r[4]} at call {lookups[0]}")
        if lookups[0] == item["mutate_at"]:
            m.put(CURSOR_MEMORY + item["index"], item["mutate_value"], 1)
        lookups[0] += 1
        m.r[2] = ROWS + item["row_map"][m.r[4] - LIST_ROWS] * ROW_STRIDE

    machine.stub(ADVANCE, advance)
    machine.stub(GET_ROW, get_row)
    machine.r[4] = item["category"] & 0xFFFFFFFF
    machine.execute(entry)
    values = [0, machine.load(ANIMATION, 1), advances[0], *machine.bytes(CURSOR_MEMORY, CURSOR_BYTES)]
    allowed = [(DEFAULT_STACK - 0x1000, DEFAULT_STACK + 0x10)]
    for row in range(LIST_ROWS):
        base = ROWS + row * ROW_STRIDE
        values += [
            machine.load(base, 1),
            machine.load(base + 6, 1),
            machine.load(base + 0x14, 2),
            machine.load(base + 0x18, 2),
        ]
        allowed += [(base, base + 1), (base + 6, base + 7), (base + 0x14, base + 0x16)]
    inputs = [
        item["category"],
        item["animation"],
        item["step"],
        item["mutate_at"],
        item["mutate_value"],
        item["index"],
        *item["memory"],
        *item["row_map"],
        *(value for row in item["rows"] for value in row),
    ]
    return {"values": values, "stores": machine.stores, "allowed": allowed, "problems": problems}, inputs


def run(kind, overlay, machine, item):
    """Execute the real body; formatters, row lookup and animation are bounded stubs."""
    entry = RANGES[overlay][kind][0]
    if kind < 2:
        observation, inputs = run_parent(machine, item, entry)
    elif kind == 2:
        observation, inputs = run_rows(machine, item, entry, overlay)
    else:
        observation, inputs = run_restore(machine, item, entry)
    return observation, [kind, *inputs]


def check(image, item, observation, native, reference):
    """Compare every returned value, row type, action, call, cursor byte and row field."""
    problems = list(observation["problems"])
    if native != observation["values"]:
        first = next(
            (n for n, pair in enumerate(zip(native, observation["values"], strict=False)) if pair[0] != pair[1]),
            None,
        )
        problems.append(
            f"values differ at {first}; lengths {len(native)}/{len(observation['values'])}"
        )
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(kind, overlay, image, results):
    """State the original/native domain, endpoint doubles and limits."""
    lines = [
        f"Original {OVERLAYS[overlay]} items2.h body executes in the shared R3000 interpreter "
        f"against the typed VS::Menu::Inventory core.",
    ]
    if kind < 2:
        lines.append(
            "Random 0xF00-byte inventories; every category plus foreign ids; in-table slots for "
            "the categories that read a table, so the full u8/u16 link words are compared."
        )
    elif kind == 2:
        lines.append(
            "Categories 0..7 (manage has no formatter), complete/short/zero-gapped one-based "
            "orders, full and partial capacities, real parents 0..8 with 0x80 and 0x8080 shield "
            "gems, equipped bytes, row types, formatter calls and arguments compared."
        )
        lines.append(
            "MENU3 misc ids 323..456 read the real MAINMENU action map; action words carry "
            "random unlock bits. MENUB records no actions; its untouched action bytes compare."
            if overlay == "menu3"
            else "MENUB compiles without _ITEMMENU: misc rows only format; the action bytes "
            "stay untouched, which the store check and the compared bytes confirm."
        )
        lines.append(
            "Formatters are endpoint stubs that write a vector row type. The native core's "
            "plan-then-apply refusals are outside this defined domain."
        )
    else:
        lines.append(
            "Category offsets including 32-bit wrapped ones, animation rows below and past 16, "
            "aliased row lookups and a cursor byte changed during a lookup; only state, x and "
            "selected may be written."
        )
        lines.append(
            "The animation advance and row lookup are endpoint stubs. The native up-front row "
            "check and stall refusal are outside this domain."
        )
    return lines
