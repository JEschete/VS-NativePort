"""Original MENU3/MENUB inventory equipment controllers with the five prepared policies followed."""

from __future__ import annotations

import functools
import random

from differential import DEFAULT_STACK, MODULE_BASES, Fault, s32, store_problems

MODULES = (r"MENU\MENU3.PRG", r"MENU\MENUB.PRG")
MODULE_BASES.update(dict.fromkeys(MODULES, 0x80102800))
DRIVER = "equipment_detail_b_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Inventory\EquipmentDetailControllers.cpp",
    r"Native\Game\src\Menu\Inventory\DetailsPolicies.cpp",
)
RANGES = (
    ((0x80102DEC, 0x80103034), (0x80102EC4, 0x8010310C)),
    ((0x80103034, 0x80103220), (0x8010310C, 0x801032F8)),
    ((0x80103220, 0x80103460), (0x801032F8, 0x80103538)),
    ((0x80103460, 0x801036BC), (0x80103538, 0x80103794)),
)
STATE = (
    (0x8010964C, 0x80109651, 0x80109655, 0x8010965A),
    (0x8010A5EC, 0x8010A5F1, 0x8010A5F5, 0x8010A5FA),
)
FIELDS = (
    (0x80109711, 0x80109712, 0x80109713, 0x80109568, 0x80109717, 0x80109718),
    (0x8010A6B9, 0x8010A6BA, 0x8010A6BB, 0x8010A50C, 0x8010A6BF, 0x8010A6C0),
)
SIZES = (1, 1, 1, 4, 1, 4)
CAPACITIES = (8, 16, 16, 8)
DIRECT, DIRECT_OFFSETS = 0x800619D8, (0, 8, 24, 40)
MEMORY, MEMORY_BYTES = 0x800F4EE8, 0xF2
ORDER_TABLE, ORDER = 0x801022A8, 0x80700000
ROWS, ROW_BYTES, ROW_COUNT = 0x80500000, 64, 40
HELD, REPEAT, PRESSED = 0x8005E238, 0x8005DFDC, 0x8005E1D0
RECORD_BASES, RECORD_STRIDES = (
    (0x80060168, 0x800603E8, 0x800606A8, 0x80060268),
    (32, 44, 16, 48),
)
BUFFER_OWNER, BUFFERS = 0x800F4E8C, (0x80600000, 0x80610000)
NAME, DESCRIPTION, GEM_TEXT = 0x80620000, 0x80620100, 0x80620200
GEM_OWNER = (0x8010956C, 0x8010A510)
FOLLOW = (
    (0x80102B78, 0x80102C44, 0x80102D7C, 0x80102988, 0x80102D30),
    (0x80102C50, 0x80102D1C, 0x80102E54, 0x80102A60, 0x80102E08),
)
(
    READY,
    COUNT,
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
) = range(1, 18)
CALLEES = {
    READY: 0x800FA9D0,
    COUNT: 0x800FEA6C,
    CLEAR: 0x800FA8E0,
    ROW: 0x800C8E48,
    PAGES: 0x800FBBD4,
    PRIMARY: 0x800FBEA4,
    LEAVE: 0x800C02E8,
    INVALID: 0x800C02E0,
    DISMISS: 0x800FA810,
    BARS: 0x800FC268,
}
STATS_CALLEES = (0x800FD270, 0x800FD404, 0x800FD504, 0x800FD5A0)
FORMAT_CALLEES = (0x800FCA08, 0x800FCAA4, 0x800FCC0C, 0x800FCE40)
MUTATIONS = (
    {
        "entry-row-store": (0x80102E0C, 0),
        "entry-page-decode": (0x80102E18, 0x24A20000),
        "entry-stage-reset": (0x80102E40, 0),
        "all-stat-pages": (0x80102EA0, 0x24050003),
        "initialize-stage": (0x80102ED0, 0),
        "animation-ten": (0x80102EE0, 0x2C620009),
        "five-component-rows": (0x80102EF4, 0x2C420005),
        "exit-buttons": (0x80102F40, 0x30420010),
        "exit-dp-pp": (0x80102F50, 0x24040000),
        "selected-page-store": (0x80102F88, 0),
        "page-row-slide": (0x80102FEC, 0x24060001),
        "page-component-end": (0x80102FF8, 0x2A020005),
        "return-ready": (0x8010300C, 0),
    },
    {
        "entry-row-store": (0x80103054, 0),
        "entry-page-decode": (0x80103060, 0x24A20000),
        "initial-timer-ten": (0x80103074, 0x24030009),
        "class-affinity-only": (0x801030E4, 0x24050007),
        "countdown": (0x80103128, 0x24420001),
        "zero-timer-gate": (0x80103124, 0x14400003),
        "exit-buttons": (0x8010314C, 0x30420040),
        "exit-dp-pp": (0x8010315C, 0x24040000),
        "selection-category": (0x80103188, 0x24040002),
        "selected-page-store": (0x80103194, 0),
        "return-ready": (0x801031F8, 0),
    },
    {
        "entry-row-store": (0x80103240, 0),
        "entry-page-decode": (0x8010324C, 0x24A20000),
        "entry-timer-reset": (0x80103268, 0),
        "type-pages-only": (0x801032D4, 0x24050003),
        "animation-ten": (0x8010330C, 0x2C620009),
        "count-up": (0x80103314, 0x24620002),
        "exit-buttons": (0x80103338, 0x30420040),
        "no-exit-dp-pp": (0x80103348, 0x24040001),
        "selected-page-store": (0x80103380, 0),
        "gem-digit-zero": (0x80103410, 0x24420031),
        "gem-digit-store": (0x80103418, 0),
        "gem-icon-highlight": (0x801033CC, 0x24040016),
        "gem-icon-x": (0x801033D0, 0x240500FF),
        "gem-text-x": (0x80103420, 0x34A50119),
        "gem-text-call": (0x80103428, 0),
    },
    {
        "entry-row-store": (0x80103480, 0),
        "entry-page-decode": (0x8010348C, 0x24A20000),
        "entry-stage-reset": (0x801034B4, 0),
        "all-stat-pages": (0x80103514, 0x24050003),
        "animation-ten": (0x80103554, 0x2C620009),
        "three-gem-rows": (0x80103568, 0x2C420006),
        "initial-gem-slide": (0x80103590, 0x24060000),
        "post-animation-ready": (0x8010359C, 0),
        "exit-buttons": (0x801035C0, 0x30420010),
        "paging-category": (0x801035EC, 0x24040002),
        "selected-page-store": (0x80103608, 0),
        "page-gem-end": (0x80103680, 0x2A020006),
    },
)


def configuration(kind: int, overlay: int) -> dict:
    entry, end = RANGES[kind][overlay]
    return {
        "ID": f"F{2575 + kind:05}",
        "ENTRY": entry,
        "END": end,
        "MODULES": (MODULES[overlay],),
        "DRIVER": DRIVER,
        "SOURCES": SOURCES,
        "VECTORS": 1024,
        "SEED": 2575 + kind + overlay * 100000,
        "MUTANTS": {
            name: {entry + address - RANGES[kind][0][0]: word}
            for name, (address, word) in MUTATIONS[kind].items()
        },
        "vector": functools.partial(vector, kind),
        "run": functools.partial(run, kind, overlay),
        "check": check,
        "describe": functools.partial(describe, kind, overlay),
    }


def vector(kind: int, rng: random.Random, number: int) -> dict:
    capacity = CAPACITIES[kind]
    count = rng.choice((1, 2, capacity - 1, capacity))
    page = rng.choice((0, count - 1, rng.randrange(count)))
    row = (0, 20, 31, 39)[number % 4]
    stage = (0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 255)[number % 13]
    timer = (0, 1, 2, 3, 4, 5, 6, 9, 10, 11, 255)[number // 13 % 11]
    initial = [stage, timer, row, page, rng.randrange(256)]
    command = (row << 8 | page + 1) if number % 13 == 0 else 0
    if command and number % 5 == 0:
        command |= rng.getrandbits(16) << 16
    if command:
        initial[0] = rng.choice((1, 2, 3, 255))
        initial[2] = (row + 7) % 40
        initial[3] = rng.randrange(256)
    held = rng.getrandbits(32) & ~12 | (number // 4 % 4) << 2
    repeat = rng.getrandbits(32) & ~12 | (number % 4) << 2
    pressed = rng.getrandbits(32) & ~0x5C | (0, 0x10, 0x40, 0x50, 4, 8)[number // 4 % 6]
    mode = number % 8
    order = list(range(1, capacity + 1))
    rng.shuffle(order)
    selected_order = order.copy()
    rng.shuffle(selected_order)
    slots = [rng.choice((0, 1, 2, 3, 255)) for _ in range(16)]
    fields = [
        rng.randrange(256),
        rng.randrange(256),
        rng.randrange(256),
        s32(rng.getrandbits(32)),
        rng.randrange(256),
        s32(rng.getrandbits(32)),
    ]
    return {
        "kind": kind,
        "command": s32(command),
        "initial": initial,
        "count": count,
        "held": held,
        "repeat": repeat,
        "pressed": pressed,
        "after_repeat": (0, 4, 8, 12)[number // 8 % 4],
        "after_pressed": (0, 4, 8, 0x10, 0x40)[number // 16 % 5],
        "ready": (0, 1, 2, -7, -0x80000000)[number // 3 % 5],
        "mode": mode,
        "fields": fields,
        "order": order,
        "selected_order": selected_order,
        "slots": slots,
        "row_type": rng.getrandbits(32),
        "name": rng.randrange(1, 96),
        "description": rng.randrange(1, 96),
        "glyph": rng.randrange(256),
        "memory": rng.randbytes(MEMORY_BYTES),
    }


def run(kind: int, overlay: int, machine, item: dict) -> tuple:
    state, fields = STATE[overlay][kind], FIELDS[overlay]
    machine.store(state, bytes(item["initial"]))
    for address, value, size in zip(fields, item["fields"], SIZES, strict=True):
        machine.put(address, value, size)
    for address, value in (
        (HELD, item["held"]),
        (REPEAT, item["repeat"]),
        (PRESSED, item["pressed"]),
    ):
        machine.put(address, value)
    machine.store(DIRECT + DIRECT_OFFSETS[kind], bytes(item["order"]))
    # All categories exist for malformed original-mutant arguments, but the
    # unpatched controller uses only its own capacity and table.
    for category in range(4):
        machine.put(ORDER_TABLE + category * 4, ORDER + category * 64)
        machine.store(
            ORDER + category * 64,
            bytes(item["selected_order"] + [1] * (64 - len(item["selected_order"]))),
        )
    for grip, slots in enumerate(item["slots"]):
        machine.put(RECORD_BASES[2] + grip * RECORD_STRIDES[2] + 4, slots, 1)
    machine.store(MEMORY, item["memory"])
    machine.store(ROWS, bytes([0xA5]) * ROW_BYTES * ROW_COUNT)
    machine.put(BUFFER_OWNER, BUFFERS[0])
    machine.store(NAME, bytes((item["name"], 0)))
    machine.store(DESCRIPTION, bytes((item["description"], 0)))
    machine.store(GEM_TEXT, b"X     " + bytes((item["glyph"], 0)))
    machine.put(GEM_OWNER[overlay], GEM_TEXT)
    machine.follow.update(FOLLOW[overlay])

    def values() -> list:
        return [
            *machine.bytes(state, 5),
            *(
                machine.load(at, size, size == 4)
                for at, size in zip(fields, SIZES, strict=True)
            ),
        ]

    def record(operation, first=0, second=0, third=0, fourth=0):
        machine.trace.append([operation, first, second, third, fourth, *values()])

    def callback(m, operation):
        a, b, c, d = (s32(m.r[reg]) for reg in (4, 5, 6, 7))
        if operation == READY:
            record(READY)
            m.r[2] = item["ready"] & 0xFFFFFFFF
            if item["mode"] == 2:
                m.put(state + 2, 20, 1)
                m.put(state + 3, (item["initial"][3] + 1) % item["count"], 1)
        elif operation == COUNT:
            record(COUNT, a, b)
            m.r[2] = item["count"]
            if item["mode"] == 1:
                m.put(REPEAT, item["after_repeat"])
                m.put(PRESSED, item["after_pressed"])
        elif operation == ROW:
            if not 0 <= a < ROW_COUNT:
                raise Fault("row outside the real forty-row workspace")
            record(ROW, a)
            m.r[2] = ROWS + a * ROW_BYTES
        elif operation == CONTINUE:
            record(CONTINUE, a)
            if item["mode"] == 3:
                at = DIRECT + DIRECT_OFFSETS[kind] + m.load(state + 3, 1)
                m.put(at, m.load(at, 1) % CAPACITIES[kind] + 1, 1)
        elif operation == PRIMARY:
            record(PRIMARY, a)
            if item["mode"] == 6:
                m.put(PRESSED, item["after_pressed"])
                m.put(state + 3, (item["initial"][3] + 1) % item["count"], 1)
        elif operation == STATS:
            record(STATS, a)
            if item["mode"] == 4:
                m.put(state + 2, 31, 1)
            if item["mode"] == 7:
                m.put(NAME, (item["name"] + 2) & 0xFF, 1)
                m.put(DESCRIPTION, (item["description"] + 3) & 0xFF, 1)
                m.put(BUFFER_OWNER, BUFFERS[1])
        elif operation == COMPONENT:
            record(COMPONENT, a, b, c)
            if item["mode"] == 5:
                at = DIRECT + DIRECT_OFFSETS[kind] + item["initial"][3]
                # Paging's full new page, not a callback-rewritten state byte.
                if m.trace and any(call[0] == CHANGE for call in m.trace):
                    at = (
                        DIRECT
                        + DIRECT_OFFSETS[kind]
                        + next(
                            call[3] for call in reversed(m.trace) if call[0] == CHANGE
                        )
                    )
                m.put(at, m.load(at, 1) % CAPACITIES[kind] + 1, 1)
        elif operation == FORMAT:
            slot = (m.r[4] - RECORD_BASES[kind]) // RECORD_STRIDES[kind] + 1
            if (
                not 1 <= slot <= CAPACITIES[kind]
                or (m.r[4] - RECORD_BASES[kind]) % RECORD_STRIDES[kind]
            ):
                raise Fault(
                    "formatter source outside its actual inventory record stride"
                )
            owner = BUFFERS.index(m.r[7]) if m.r[7] in BUFFERS else -1
            record(FORMAT, slot, owner)
            m.put(m.r[5], NAME)
            m.put(m.r[5] + 4, DESCRIPTION)
            m.put(m.r[6], item["row_type"])
        elif operation == CHANGE:
            name = m.load(m.load(m.r[5]), 1)
            desc = m.load(m.load(m.r[5] + 4), 1)
            record(CHANGE, a, c & 0xFFFFFFFF, d, name << 8 | desc)
        elif operation == ICON:
            record(ICON, a, b, c, m.load(GEM_TEXT + 6, 1))
        elif operation == TEXT:
            record(TEXT, m.load(m.r[4] + 6, 1), b & 0xFFFFFFFF, c)
        elif operation in (LEAVE, INVALID):
            record(operation)
        else:
            record(operation, a)

    for operation, address in CALLEES.items():
        machine.stub(address, functools.partial(callback, operation=operation))
    for operation, address in (
        (CONTINUE, (0x80102BE4, 0x80102CBC)[overlay]),
        (STATS, STATS_CALLEES[kind]),
        (FORMAT, FORMAT_CALLEES[kind]),
        (CHANGE, (0x80102C94, 0x80102D6C)[overlay]),
        (COMPONENT, 0x800FC510 if kind == 0 else 0x800FC704),
        (ICON, 0x80100004),
        (TEXT, 0x800C6828),
    ):
        machine.stub(address, functools.partial(callback, operation=operation))
    machine.r[4] = item["command"] & 0xFFFFFFFF
    machine.execute(RANGES[kind][overlay][0])
    allowed = [
        (DEFAULT_STACK - 0x80, DEFAULT_STACK),
        (state, state + 5),
        (MEMORY, MEMORY + MEMORY_BYTES),
        (ROWS, ROWS + ROW_BYTES * ROW_COUNT),
        (GEM_TEXT, GEM_TEXT + 8),
    ]
    allowed += [
        (address, address + size) for address, size in zip(fields, SIZES, strict=True)
    ]
    # Callback stores use Machine.put and are not mistaken for controller instructions.
    observation = {
        "values": [s32(machine.r[2]), *values(), machine.load(GEM_TEXT + 6, 1)],
        "memory": machine.bytes(MEMORY, MEMORY_BYTES),
        "rows": machine.bytes(ROWS, ROW_BYTES * ROW_COUNT),
        "calls": machine.trace,
        "stores": machine.stores,
        "allowed": allowed,
    }
    inputs = [
        kind,
        item["command"],
        *item["initial"],
        item["count"],
        item["held"],
        item["repeat"],
        item["pressed"],
        item["after_repeat"],
        item["after_pressed"],
        item["ready"],
        item["mode"],
        *item["fields"],
        *item["order"],
        *item["selected_order"],
        *item["slots"],
        item["row_type"],
        item["name"],
        item["description"],
        item["glyph"],
        *item["memory"],
    ]
    return observation, inputs


def check(image, item, observation, native, reference):
    fields_end = 13
    memory_end = fields_end + MEMORY_BYTES
    rows_end = memory_end + ROW_BYTES * ROW_COUNT
    problems = []
    if native[:fields_end] != observation["values"]:
        problems.append(
            f"controller/panel/count fields {native[:fields_end]} != {observation['values']}"
        )
    if bytes(native[fields_end:memory_end]) != observation["memory"]:
        problems.append("full cursor memory differs")
    if bytes(native[memory_end:rows_end]) != observation["rows"]:
        problems.append(
            "typed row fields differ from the complete original forty-row image"
        )
    calls = [
        native[rows_end + 1 + n * 16 : rows_end + 1 + (n + 1) * 16]
        for n in range(native[rows_end])
    ]
    if calls != observation["calls"]:
        first = next(
            (
                i
                for i, (a, b) in enumerate(
                    zip(calls, observation["calls"], strict=False)
                )
                if a != b
            ),
            min(len(calls), len(observation["calls"])),
        )
        problems.append(
            f"callback arguments/order/current snapshots at {first}: "
            f"{calls[first : first + 1]} != {observation['calls'][first : first + 1]}"
        )
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(kind, overlay, image, results):
    return [
        f"{MODULES[overlay]} independently audited real F{2575 + kind:05} controller instructions.",
        "Followed original _startMenuTransition, _initMenuLayout, _exitStatusView, _itemPaging and "
        "_getItemIndex. Native driver executes the frozen prepared typed policies for the same five services.",
        "Bounded native external callbacks: row readiness/clear/get/continue, equipment stats and bars, count/sounds, "
        "item formatter/selection/component rows, compact text/icon presentation. Original ABIs, typed values, "
        "borrowed-text bytes, exact order and current controller/panel snapshots compared.",
        "Compared all controller bytes, six panel fields, mutable gem digit, entire 0xF2 cursor memory, "
        "all forty typed menu rows re-encoded to the original record layout, and allowed instruction-store spans.",
        f"Biased modes: {[(mode, sum(v['mode'] == mode for v, _, _ in results)) for mode in range(8)]}; "
        "entry/initialize, timer/component boundaries, both-shoulder paging, exit priority, callback changes "
        "to readiness/item mapping/current row/input, per-component reloads and borrowed text/buffer owners.",
    ]
