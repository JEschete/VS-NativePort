"""Actual final inventory details instructions, including real counts and child controllers."""

from __future__ import annotations

import functools
import random

from differential import DEFAULT_STACK, MODULE_BASES, Fault, s32, store_problems

OVERLAYS = (r"MENU\MENU3.PRG", r"MENU\MENUB.PRG")
MAINMENU, BATTLE = r"MENU\MAINMENU.PRG", r"BATTLE\BATTLE.PRG"
MODULE_BASES.update(
    {
        **dict.fromkeys(OVERLAYS, 0x80102800),
        MAINMENU: 0x800F9800,
        BATTLE: 0x80068800,
    }
)
DRIVER = "item_detail_dispatch_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Inventory\ItemDetailDispatch.cpp",
    r"Native\Game\src\Menu\Framework\CategoryIcons.cpp",
    r"Native\Game\src\Menu\Inventory\EquipmentDetailControllers.cpp",
    r"Native\Game\src\Menu\Inventory\DetailsPolicies.cpp",
    r"Native\Game\src\Items\Inventory.cpp",
    r"Native\Game\src\Items\EquipmentStats.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
)
IDS = ("F02568", "F02579", "F02580", "F02581")
RANGES = (
    ((0x80102A3C, 0x80102B78), (0x80102B14, 0x80102C50)),
    ((0x801036BC, 0x801038E4), (0x80103794, 0x801039BC)),
    ((0x801038E4, 0x80103AD0), (0x801039BC, 0x80103BA8)),
    ((0x80103AD0, 0x80103BE4), (0x80103BA8, 0x80103CBC)),
)
CONTROLLERS = (
    (0x80102DEC, 0x80103034, 0x80103220, 0x80103460, 0x801036BC, 0x801038E4),
    (0x80102EC4, 0x8010310C, 0x801032F8, 0x80103538, 0x80103794, 0x801039BC),
)
STATE = (0x8010964C, 0x8010A5EC)
STATE_OFFSETS = (0, 5, 9, 14, 19, 23)
STATE_BYTES = 28
FIELDS = (
    (0x80109711, 0x80109712, 0x80109713, 0x80109568, 0x80109717, 0x80109718),
    (0x8010A6B9, 0x8010A6BA, 0x8010A6BB, 0x8010A50C, 0x8010A6BF, 0x8010A6C0),
)
FIELD_SIZES = (1, 1, 1, 4, 1, 4)
CAPACITIES = (8, 16, 16, 8, 16, 48)
DIRECT, DIRECT_OFFSETS = 0x800619D8, (0, 8, 24, 40, 48, 64)
INVENTORY = 0x80060168
RECORD_BASES = (0x80060168, 0x800603E8, 0x800606A8, 0x80060268, 0x800607A8, 0x80060A28)
RECORD_STRIDES = (32, 44, 16, 48, 40, 28)
OCCUPANCY_OFFSETS, OCCUPANCY_SIZES = (1, 0, 0, 4, 0, 0), (1, 1, 2, 1, 1, 2)
ORDER_TABLE, ORDER = 0x801022A8, 0x80700000
MEMORY, MEMORY_BYTES = 0x800F4EE8, 0xF2
ROW_BASES, ROW_BYTES, ROW_COUNT = (0x80500000, 0x80510000), 64, 40
HELD, REPEAT, PRESSED = 0x8005E238, 0x8005DFDC, 0x8005E1D0
SELECTED, CURSOR = 0x801024B8, 0x801022D5
BUFFER_OWNER, BUFFERS = 0x800F4E8C, (0x80600000, 0x80610000)
NAME, DESCRIPTION, GEM_TEXT = 0x80620000, 0x80620100, 0x80620200
GEM_OWNER = (0x8010956C, 0x8010A510)
PRIMITIVE, SCENE_CHAIN, OVERLAY_CHAIN, SCRATCH = (
    0x80900000,
    0x80B00000,
    0x80B00004,
    0x1F800000,
)
COUNT, OCCUPANCY = 0x800FEA6C, 0x800FE6EC
FOLLOW = (
    (0x80102B78, 0x80102C44, 0x80102D7C, 0x80102988, 0x80102D30),
    (0x80102C50, 0x80102D1C, 0x80102E54, 0x80102A60, 0x80102E08),
)
(
    READY,
    CLEAR,
    ROW,
    CONTINUE,
    PAGES,
    PRIMARY,
    LEAVE,
    INVALID,
    DISMISS,
    BARS,
    STATS,
    COMPONENT,
    FORMAT,
    CHANGE,
    ICON,
    TEXT,
    RESET_CURSOR,
    COMMAND,
    TABS,
    DRAW_DETAILS,
) = range(1, 21)
CALLEES = {
    READY: 0x800FA9D0,
    CLEAR: 0x800FA8E0,
    ROW: 0x800C8E48,
    PAGES: 0x800FBBD4,
    PRIMARY: 0x800FBEA4,
    LEAVE: 0x800C02E8,
    INVALID: 0x800C02E0,
    DISMISS: 0x800FA810,
    BARS: 0x800FC268,
    RESET_CURSOR: 0x800FDD78,
    COMMAND: 0x800FFA88,
    TABS: 0x801013F8,
    DRAW_DETAILS: 0x800FDEBC,
}
STATS_CALLEES = (0x800FD270, 0x800FD404, 0x800FD504, 0x800FD5A0, 0x800FD700, 0x800FD878)
FORMAT_CALLEES = (
    0x800FCA08,
    0x800FCAA4,
    0x800FCC0C,
    0x800FCE40,
    0x800FD084,
    0x800FD0E0,
)
MUTATIONS = (
    {
        "category-mask": (0x80102A70, 0x30540003),
        "tabs-complete-step": (0x80102A74, 0x24020003),
        "tabs-call": (0x80102A80, 0),
        "animation-scale": (0x80102A88, 0x00131080),
        "animation-offset": (0x80102A8C, 0x2453FFF1),
        "category-cycle": (0x80102AA4, 0x30420003),
        "icon-pitch": (0x80102AA8, 0x000210C0),
        "icon-start-x": (0x80102AB0, 0x24500021),
        "highlight-exact-one": (0x80102AB4, 0x24020002),
        "header-row": (0x80102AC4, 0x2404001E),
        "merge-inset": (0x80102AD4, 0x2442FFF5),
        "merge-equality": (0x80102AD8, 0x0040102A),
        "merged-icon-index": (0x80102AE4, 0x26220019),
        "merged-icon-store": (0x80102AEC, 0),
        "overlay-y": (0x80102AF4, 0x3C050011),
        "animated-y": (0x80102B0C, 0x001328C0),
        "selected-palette": (0x80102B34, 0x3C0237FE),
        "texture-store": (0x80102B3C, 0),
        "eight-icons": (0x80102B44, 0x2A220007),
    },
    {
        "entry-row-store": (0x801036E4, 0),
        "entry-page-decode": (0x801036F0, 0x24A20000),
        "entry-ten": (0x80103704, 0x24030009),
        "entry-stage-reset": (0x80103714, 0),
        "initial-ready": (0x8010375C, 0),
        "all-stat-pages": (0x80103774, 0x24050003),
        "stats-direct-order": (0x8010378C, 0x90440031),
        "applicability-reloads-order": (0x801037A4, 0x90430031),
        "accessory-category": (0x801037C4, 0x24020006),
        "dp-only": (0x801037D4, 0x24040003),
        "countdown": (0x801037F0, 0x24420001),
        "exit-buttons": (0x80103814, 0x30420010),
        "exit-dp-pp": (0x80103824, 0x24040000),
        "paging-category": (0x8010383C, 0x24040005),
        "selected-page-store": (0x80103858, 0),
        "format-record-stride": (0x80103868, 0x00021080),
        "page-selection": (0x801038A4, 0),
        "return-ready": (0x801038B4, 0),
    },
    {
        "entry-row-store": (0x80103904, 0),
        "entry-page-decode": (0x80103910, 0x24A20000),
        "entry-ten": (0x80103924, 0x24030009),
        "entry-stage-reset": (0x80103934, 0),
        "initial-ready": (0x8010397C, 0),
        "class-affinity-pages": (0x80103994, 0x24050007),
        "stats-direct-order": (0x801039AC, 0x90440041),
        "countdown": (0x801039D0, 0x24420001),
        "exit-buttons": (0x801039F4, 0x30420040),
        "no-exit-dp-pp": (0x80103A04, 0x24040001),
        "battle-intent-mask": (0x80103A18, 0x30420040),
        "battle-intent-store": (0x80103A20, 0xAC609718),
        "paging-category": (0x80103A30, 0x24040004),
        "selected-page-store": (0x80103A4C, 0),
        "format-record-stride": (0x80103A5C, 0x000210C0),
        "page-selection": (0x80103A98, 0),
        "return-ready": (0x80103AA8, 0),
    },
    {
        "entry-page-shift": (0x80103AE4, 0x000480C3),
        "category-mask": (0x80103AEC, 0x30820003),
        "category-store": (0x80103AF0, 0),
        "header-placement": (0x80103AF8, 0x24020002),
        "clear-battle-intent": (0x80103B08, 0),
        "cursor-reset": (0x80103B04, 0),
        "header-row": (0x80103B10, 0x2404001E),
        "page-byte-mask": (0x80103B18, 0x3203007F),
        "header-page-store": (0x80103B20, 0),
        "controller-call": (0x80103B44, 0),
        "return-battle-value": (0x80103B64, 0x2402FFFD),
        "restore-categories": (0x80103B74, 0),
        "restore-header": (0x80103B7C, 0),
        "restore-commands": (0x80103B84, 0),
        "restore-preview": (0x80103B90, 0),
        "menu-command": (0x80103B68, 0x24040001),
        "readiness-gate": (0x80103B9C, 0),
        "selected-element": (0x80103BB8, 0x38420008),
        "cursor-store": (0x80103BC4, 0),
        "detail-tabs-mode": (0x80103BA8, 0x24040000),
        "detail-draw": (0x80103BC8, 0),
    },
)


def configuration(kind: int, overlay: int) -> dict:
    entry, end = RANGES[kind][overlay]
    return {
        "ID": IDS[kind],
        "ENTRY": entry,
        "END": end,
        "MODULES": (OVERLAYS[overlay], MAINMENU, BATTLE),
        "DRIVER": DRIVER,
        "SOURCES": SOURCES,
        "VECTORS": 1024,
        "MUTANT_VECTORS": 192,
        "SEED": 2568 + kind * 17 + overlay * 100000,
        "MUTANTS": {
            name: {
                entry + at - RANGES[kind][0][0]: (
                    (word & 0xFFFF0000) | (0x9718 if overlay == 0 else 0xA6C0)
                )
                if name == "battle-intent-store"
                else word
            }
            for name, (at, word) in MUTATIONS[kind].items()
        },
        "vector": functools.partial(vector, kind),
        "run": functools.partial(run, kind, overlay),
        "check": check,
        "describe": functools.partial(describe, kind, overlay),
    }


def vector(kind: int, rng: random.Random, number: int) -> dict:
    category = (4 if kind == 1 else 5) if kind in (1, 2) else number % 6
    counts = [rng.choice((1, 2, size - 1, size)) for size in CAPACITIES]
    state = [rng.randrange(256) for _ in range(STATE_BYTES)]
    stage = (0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 255)[number % 11]
    timer = (0, 1, 9, 10, 11, 255)[number // 11 % 6]
    page = (0, counts[category] - 1)[number % 2]
    row = (0, 20, 31, 39)[number // 2 % 4]
    state[STATE_OFFSETS[category] : STATE_OFFSETS[category] + 4] = [
        stage,
        timer,
        row,
        page,
    ]
    state[27] = category
    child_command = row << 8 | page + 1
    command = child_command if number % 11 == 0 else 0
    if command:
        command |= rng.getrandbits(16) << 16
    if kind == 3:
        command = ((child_command << 4) | category) if number % 11 == 0 else 0
        if command:
            command |= rng.getrandbits(12) << 20
        if number % 29 == 1:
            command = category
        # Valid callers can store page >= 128 without accessing that page:
        # its high byte is a row and its low byte is the index-plus-one.
        if number % 47 == 2:
            command = ((row << 8 | 0x81) << 4) | category
        if number % 53 == 3:
            command = (0x10000 << 4) | category
    elif kind in (1, 2) and number % 53 == 3:
        command = 0x10000
    if kind != 0 and command:
        offset = STATE_OFFSETS[category]
        state[offset] = rng.choice((1, 2, 3, 255))
        state[offset + 2] = (row + 7) % 40
        state[offset + 3] = (page + 1) % CAPACITIES[category]
        if kind == 3:
            state[27] = (category + 1) % 6
    held = rng.getrandbits(32) & ~12 | (number // 4 % 4) << 2
    repeat = rng.getrandbits(32) & ~12 | (number % 4) << 2
    pressed = rng.getrandbits(32) & ~0x5C | (0, 0x10, 0x40, 0x50, 4, 8)[number // 4 % 6]
    direct, bound = [], []
    for size in CAPACITIES:
        order = list(range(1, size + 1))
        rng.shuffle(order)
        direct.append(order)
        order = order.copy()
        rng.shuffle(order)
        bound.append(order)
    memory = bytearray(rng.randbytes(MEMORY_BYTES))
    category_byte = (0, 1, 8, 255)[number % 4]
    memory[0x3A] = category_byte
    selected_icon = (category_byte - 1) & 7
    icon_x = (((selected_icon + 1) & 7) * 16) + 32
    return {
        "kind": kind,
        "category": category,
        "command": s32(command),
        "state": state,
        "counts": counts,
        "held": held,
        "repeat": repeat,
        "pressed": pressed,
        "after_pressed": (0, 0x10, 0x40, 0x50)[number // 8 % 4],
        "ready": (0, 1, 2, -7, -0x80000000)[number // 3 % 5],
        "mode": number % 8,
        "fields": [
            rng.randrange(256),
            rng.randrange(256),
            rng.randrange(256),
            s32(rng.getrandbits(32)),
            rng.randrange(256),
            (0, 16, -7)[number % 3],
        ],
        "direct": direct,
        "bound": bound,
        "armor_categories": [rng.choice((1, 6, 7, 255)) for _ in range(16)],
        "slots": [rng.choice((0, 1, 2, 3, 255)) for _ in range(16)],
        "row_type": rng.getrandbits(32),
        "name": rng.randrange(1, 96),
        "description": rng.randrange(1, 96),
        "glyph": rng.randrange(256),
        "memory": bytes(memory),
        "selected": (9, 8, 20, 255)[number % 4],
        "cursor": rng.randrange(256),
        "command_value": rng.randrange(256),
        "animation": (0, 1, 2, 3, 4, -1, 0x7FFFFFFF, -0x80000000)[number % 8],
        "highlight": (0, 1, 1, 2)[number // 8 % 4],
        "header_x": icon_x + (11, 12, 13, -32000, 32000)[number // 4 % 5],
        "scene_chain": rng.getrandbits(32),
        "overlay_chain": rng.getrandbits(32),
    }


def run(kind: int, overlay: int, machine, item: dict) -> tuple:
    state, fields = STATE[overlay], FIELDS[overlay]
    machine.store(state, bytes(item["state"]))
    for address, value, size in zip(fields, item["fields"], FIELD_SIZES, strict=True):
        machine.put(address, value, size)
    machine.map_zero(INVENTORY, 0x1800)
    machine.map_zero(PRIMITIVE, 0x400)
    for category, size in enumerate(CAPACITIES):
        machine.store(
            DIRECT + DIRECT_OFFSETS[category], bytes(item["direct"][category])
        )
        machine.put(ORDER_TABLE + category * 4, ORDER + category * 64)
        machine.store(
            ORDER + category * 64, bytes(item["bound"][category] + [1] * (64 - size))
        )
        for slot in range(item["counts"][category]):
            machine.put(
                RECORD_BASES[category]
                + slot * RECORD_STRIDES[category]
                + OCCUPANCY_OFFSETS[category],
                1,
                OCCUPANCY_SIZES[category],
            )
    for slot, category in enumerate(item["armor_categories"]):
        machine.put(RECORD_BASES[4] + slot * RECORD_STRIDES[4] + 3, category, 1)
    for grip, slots in enumerate(item["slots"]):
        machine.put(RECORD_BASES[2] + grip * RECORD_STRIDES[2] + 4, slots, 1)
    for address, value in (
        (HELD, item["held"]),
        (REPEAT, item["repeat"]),
        (PRESSED, item["pressed"]),
    ):
        machine.put(address, value)
    machine.put(SELECTED, item["selected"], 1)
    machine.put(CURSOR, item["cursor"], 1)
    machine.store(MEMORY, item["memory"])
    for rows in ROW_BASES:
        machine.store(rows, bytes([0xA5]) * ROW_BYTES * ROW_COUNT)
        machine.put(rows + 31 * ROW_BYTES + 20, item["header_x"], 2)
    machine.put(BUFFER_OWNER, BUFFERS[0])
    machine.store(NAME, bytes((item["name"], 0)))
    machine.store(DESCRIPTION, bytes((item["description"], 0)))
    machine.store(GEM_TEXT, b"X     " + bytes((item["glyph"], 0)))
    machine.put(GEM_OWNER[overlay], GEM_TEXT)
    machine.put(SCRATCH, PRIMITIVE)
    machine.put(SCRATCH + 4, SCENE_CHAIN)
    machine.put(SCRATCH + 8, OVERLAY_CHAIN)
    machine.put(SCENE_CHAIN, item["scene_chain"])
    machine.put(OVERLAY_CHAIN, item["overlay_chain"])
    machine.follow.update(
        (
            *FOLLOW[overlay],
            *CONTROLLERS[overlay],
            COUNT,
            OCCUPANCY,
            0x800C0214,
            0x800C0224,
        )
    )
    bank, command_value = 0, item["command_value"]

    def active():
        category = machine.load(state + 27, 1) if kind == 3 else item["category"]
        if category >= 6:
            raise Fault("used category outside the actual six controllers")
        return category

    def values():
        return [
            *machine.bytes(state, STATE_BYTES),
            *(
                machine.load(at, size, size == 4)
                for at, size in zip(fields, FIELD_SIZES, strict=True)
            ),
            machine.load(SELECTED, 1),
            machine.load(CURSOR, 1),
            command_value,
        ]

    def record(operation, first=0, second=0, third=0, fourth=0):
        machine.trace.append([operation, first, second, third, fourth, *values()])

    def callback(m, operation, callee_category=-1):
        nonlocal bank, command_value
        a, b, c, d = (s32(m.r[reg]) for reg in (4, 5, 6, 7))
        category = active()
        selected_state = state + STATE_OFFSETS[category]
        if operation == READY:
            record(READY)
            m.r[2] = item["ready"] & 0xFFFFFFFF
            if item["mode"] == 1:
                m.put(selected_state + 2, 20, 1)
                m.put(
                    selected_state + 3,
                    (m.load(selected_state + 3, 1) + 1) % item["counts"][category],
                    1,
                )
        elif operation == ROW:
            if not 0 <= a < ROW_COUNT:
                raise Fault("row outside the actual forty-row workspace")
            record(ROW, a, bank)
            m.r[2] = ROW_BASES[bank] + a * ROW_BYTES
        elif operation == CLEAR:
            record(CLEAR, a)
            if item["mode"] == 7:
                bank = 1
        elif operation == CONTINUE:
            record(CONTINUE, a)
            if item["mode"] == 2:
                at = DIRECT + DIRECT_OFFSETS[category] + m.load(selected_state + 3, 1)
                m.put(at, m.load(at, 1) % CAPACITIES[category] + 1, 1)
        elif operation == PRIMARY:
            record(PRIMARY, a)
            if item["mode"] == 5:
                m.put(PRESSED, item["after_pressed"])
                m.put(
                    selected_state + 3,
                    (m.load(selected_state + 3, 1) + 1) % item["counts"][category],
                    1,
                )
        elif operation == STATS:
            record(STATS, a, callee_category)
            if item["mode"] == 3:
                m.put(selected_state + 2, 31, 1)
                at = DIRECT + DIRECT_OFFSETS[category] + m.load(selected_state + 3, 1)
                m.put(at, m.load(at, 1) % CAPACITIES[category] + 1, 1)
            if item["mode"] == 4:
                m.put(NAME, item["name"] + 2, 1)
                m.put(DESCRIPTION, item["description"] + 3, 1)
                m.put(BUFFER_OWNER, BUFFERS[1])
        elif operation == FORMAT:
            base, stride = (
                RECORD_BASES[callee_category],
                RECORD_STRIDES[callee_category],
            )
            slot = (m.r[4] - base) // stride + 1
            if not 1 <= slot <= CAPACITIES[callee_category] or (m.r[4] - base) % stride:
                raise Fault("formatter source outside its actual inventory stride")
            owner = BUFFERS.index(m.r[7]) if m.r[7] in BUFFERS else -1
            record(FORMAT, slot, callee_category, owner)
            m.put(m.r[5], NAME)
            m.put(m.r[5] + 4, DESCRIPTION)
            m.put(m.r[6], item["row_type"])
            if item["mode"] == 6:
                page = m.load(selected_state + 3, 1)
                m.put(ORDER + category * 64 + page, 1, 1)
        elif operation == CHANGE:
            name, description = m.load(m.load(m.r[5]), 1), m.load(m.load(m.r[5] + 4), 1)
            record(CHANGE, a, c & 0xFFFFFFFF, d, name << 8 | description)
        elif operation == COMPONENT:
            record(COMPONENT, a, b, c)
        elif operation == ICON:
            record(ICON, a, b, c, m.load(GEM_TEXT + 6, 1))
        elif operation == TEXT:
            record(TEXT, m.load(m.r[4] + 6, 1), b & 0xFFFFFFFF, c)
        elif operation == RESET_CURSOR:
            record(RESET_CURSOR)
            m.put(SELECTED, 9, 1)
        elif operation == COMMAND:
            record(COMMAND, a)
            command_value = a
        elif operation == TABS:
            record(TABS, a)
            if item["mode"] == 7:
                bank = 1
                m.put(MEMORY + 0x3A, (item["memory"][0x3A] + 3) & 0xFF, 1)
        elif operation in (LEAVE, INVALID, DRAW_DETAILS):
            record(operation)
        else:
            record(operation, a)

    for operation, address in CALLEES.items():
        machine.stub(address, functools.partial(callback, operation=operation))
    machine.stub(
        (0x80102BE4, 0x80102CBC)[overlay],
        functools.partial(callback, operation=CONTINUE),
    )
    machine.stub(
        (0x80102C94, 0x80102D6C)[overlay], functools.partial(callback, operation=CHANGE)
    )
    for category in range(6):
        machine.stub(
            STATS_CALLEES[category],
            functools.partial(callback, operation=STATS, callee_category=category),
        )
        machine.stub(
            FORMAT_CALLEES[category],
            functools.partial(callback, operation=FORMAT, callee_category=category),
        )
    for operation, address in (
        (COMPONENT, 0x800FC510),
        (COMPONENT, 0x800FC704),
        (ICON, 0x80100004),
        (TEXT, 0x800C6828),
    ):
        machine.stub(address, functools.partial(callback, operation=operation))
    machine.r[4] = (item["animation"] if kind == 0 else item["command"]) & 0xFFFFFFFF
    machine.r[5] = item["highlight"] & 0xFFFFFFFF
    machine.execute(RANGES[kind][overlay][0])
    packet_count = (machine.load(SCRATCH) - PRIMITIVE) // 24
    allowed = [
        (DEFAULT_STACK - 0x100, DEFAULT_STACK),
        (state, state + STATE_BYTES),
        (MEMORY, MEMORY + MEMORY_BYTES),
        (GEM_TEXT, GEM_TEXT + 8),
        (SELECTED, SELECTED + 1),
        (CURSOR, CURSOR + 1),
        (PRIMITIVE, PRIMITIVE + 0x400),
        (SCRATCH, SCRATCH + 4),
        (SCENE_CHAIN, OVERLAY_CHAIN + 4),
        *((rows, rows + ROW_BYTES * ROW_COUNT) for rows in ROW_BASES),
        *((at, at + size) for at, size in zip(fields, FIELD_SIZES, strict=True)),
    ]
    observation = {
        "values": [
            0 if kind == 0 else s32(machine.r[2]),
            *values(),
            machine.load(GEM_TEXT + 6, 1),
        ],
        "memory": machine.bytes(MEMORY, MEMORY_BYTES),
        "rows": b"".join(
            machine.bytes(rows, ROW_BYTES * ROW_COUNT) for rows in ROW_BASES
        ),
        "render": [
            machine.load(SCRATCH),
            machine.load(SCENE_CHAIN),
            machine.load(OVERLAY_CHAIN),
            packet_count,
            *(machine.load(PRIMITIVE + index * 4) for index in range(packet_count * 6)),
        ],
        "calls": machine.trace,
        "stores": machine.stores,
        "allowed": allowed,
    }
    inputs = [
        kind,
        item["category"],
        item["command"],
        *item["state"],
        *item["counts"],
        item["held"],
        item["repeat"],
        item["pressed"],
        item["after_pressed"],
        item["ready"],
        item["mode"],
        *item["fields"],
        *(slot for order in item["direct"] for slot in order),
        *(slot for order in item["bound"] for slot in order),
        *item["armor_categories"],
        *item["slots"],
        item["row_type"],
        item["name"],
        item["description"],
        item["glyph"],
        *item["memory"],
        item["selected"],
        item["cursor"],
        item["command_value"],
        item["animation"],
        item["highlight"],
        item["header_x"],
        item["scene_chain"],
        item["overlay_chain"],
    ]
    return observation, inputs


def check(image, item, observation, native, reference):
    fields_end = 1 + STATE_BYTES + 6 + 3 + 1
    memory_end = fields_end + MEMORY_BYTES
    rows_end = memory_end + ROW_BYTES * ROW_COUNT * 2
    render_end = rows_end + 4 + native[rows_end + 3] * 6
    calls = [
        native[render_end + 1 + index * 42 : render_end + 1 + (index + 1) * 42]
        for index in range(native[render_end])
    ]
    problems = []
    for label, theirs, ours in (
        ("controller/panel/cursor fields", native[:fields_end], observation["values"]),
        (
            "entire cursor memory",
            bytes(native[fields_end:memory_end]),
            observation["memory"],
        ),
        (
            "both forty-row owners",
            bytes(native[memory_end:rows_end]),
            observation["rows"],
        ),
        (
            "actual native sprite helpers",
            native[rows_end:render_end],
            observation["render"],
        ),
        ("ordered callbacks and current snapshots", calls, observation["calls"]),
    ):
        if theirs != ours:
            problems.append(f"{label} differ: {str(theirs)[:800]} != {str(ours)[:800]}")
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(kind, overlay, image, results):
    return [
        f"{IDS[kind]} {OVERLAYS[overlay]} independently executed, not assumed equivalent by header name.",
        "Followed the five original policies, actual MAINMENU item count and occupancy selector, "
        "all six original child controllers for dispatch, and actual BATTLE sprite instructions for icons.",
        "Native driver executes new typed cores, frozen B4+B6 cores, real Items::Inventory::CountItems "
        "and real Render sprite builders/queue helpers. No new math stub or private interpreter.",
        "Only bounded external presentation callbacks stubbed: readiness/row layout, statistics/bars, "
        "row formatting/selection, commands/cursor, tabs/help rendering, sounds and grip text/icon draws.",
        "Compared all 28 controller/category bytes, six UI fields, cursor/command state, both complete "
        "forty-row owners, full 0xF2 cursor memory, actual sprite words/queues and callback order/snapshots.",
        f"Modes: {[(mode, sum(vector['mode'] == mode for vector, _, _ in results)) for mode in range(8)]}; "
        "entry/return, exact merge equality, wrapping, both shoulders, countdown edges, direct-vs-bound "
        "orders, post-layout/stat order reload, cached slot, borrowed texts and callback-switched rows.",
    ]
