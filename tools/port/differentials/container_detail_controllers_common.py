"""Actual MENUD controllers with ordered native-service callbacks and full byte state."""

import functools
import random

from differential import DEFAULT_STACK, MODULE_BASES, s32, store_problems

MODULES = (r"MENU\MENUD.PRG", "SLUS_010.40")
MODULE_BASES[MODULES[0]] = 0x80102800
DRIVER = "container_detail_controllers_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Container\ContainerDetailControllers.cpp",
    r"Native\Game\src\Menu\Framework\CategoryIcons.cpp",
)
IDS = ("F02944", "F02953", "F02954", "F02955",
       "F02956", "F02957", "F02958", "F02959")
RANGES = ((0x80103E24, 0x80103F64), (0x801042D0, 0x80104534),
          (0x80104534, 0x80104728), (0x80104728, 0x8010496C),
          (0x8010496C, 0x80104BDC), (0x80104BDC, 0x80104E14),
          (0x80104E14, 0x80105008), (0x80105008, 0x80105114))
CAPACITIES = (32, 64, 64, 32, 64, 192)
STATE = 0x80109A30
STATE_BYTES = 28
OFFSETS = (0, 5, 9, 14, 19, 23)
LENGTHS = (5, 4, 5, 5, 4, 4)
GLOBAL = (0x80109A7B, 0x80109A7C, 0x80109A7D, 0x80109A81,
          0x80109A84, 0x801022D5)
GLOBAL_SIZES = (1, 1, 1, 1, 4, 1)
OWNER = (0x80600000, 0x80610000)
HEADER = (0x80700000, 0x80700100)
NAME = (0x80800000, 0x80800100)
BUFFER = (0x80900000, 0x80901000)
FOOTER = 0x80910000
PACKET = 0x80A00000
SCENE = (0x80B00100, 0x80B10100)
CONTAINER, STRING_BUFFER, PRESSED = 0x80109A8C, 0x800F4E8C, 0x8005E1D0
SELECTED, CURRENT_CATEGORY, SCRATCH = 0x801024B8, 0x800F4F88, 0x1F800000
ORDER_OFFSETS = (0x3C00, 0x3C40, 0x3CC0, 0x3D40, 0x3D80, 0x3E00)
RECORD_OFFSETS = (0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300)
STRIDES = (0x20, 0x2C, 0x10, 0x30, 0x28, 0x1C)
READY, BEGIN, LAYOUT, CURRENT, STATS, BARS, DP, COMPONENT, EXIT, POLL = range(1, 11)
SELECT, FORMAT, CHANGE, SLOTS, DRAW_SLOTS, ARMOR_GATE = range(11, 17)
RESET, HEADER_CALL, COMMAND, CONTROLLER, TABS, DETAILS, ICON, FOOTER_TEXT = range(17, 25)
STATS_CALLEES = (0x800FD270, 0x800FD404, 0x800FD504,
                 0x800FD5A0, 0x800FD700, 0x800FD878)
FORMAT_CALLEES = (0x801041E0, 0x800FCAA4, 0x800FCC0C,
                  0x8010425C, 0x800FD084, 0x800FD0E0)
MUTATIONS = (
    {
        "category-wrap": (0x80103E58, 0x30540003),
        "tabs-step": (0x80103E5C, 0x24020003),
        "tabs-call": (0x80103E68, 0),
        "animation-scale": (0x80103E70, 0x00131080),
        "icon-x": (0x80103E98, 0x24500021),
        "merge-inset": (0x80103EBC, 0x2442FFF5),
        "merged-icon": (0x80103ECC, 0x26220019),
        "merge-store": (0x80103ED4, 0),
        "overlay-render": (0x80103EE0, 0),
        "scene-layer": (0x80103F0C, 0x24E70000),
        "selected-palette": (0x80103F20, 0x3C0237FE),
        "texture-store": (0x80103F28, 0),
        "eight-icons": (0x80103F30, 0x2A220007),
        "uv-pitch": (0x80103F38, 0x26520008),
    },
    {
        "entry-row": (0x801042F4, 0x00051243),
        "entry-page": (0x80104304, 0x24A20000),
        "begin-call": (0x80104308, 0),
        "unused-reset": (0x80104318, 0),
        "ready-call": (0x80104374, 0),
        "stat-pages": (0x8010438C, 0x24050003),
        "halfword-token": (0x801043A8, 0x90643C00),
        "stats-call": (0x801043AC, 0),
        "both-bars": (0x801043B8, 0x24040001),
        "animation-end": (0x801043D0, 0x2C620009),
        "five-components": (0x801043E4, 0x2C420005),
        "exit-priority": (0x80104434, 0x30420010),
        "paging-call": (0x8010445C, 0),
        "format-call": (0x801044A8, 0),
        "refresh-components": (0x801044E8, 0),
        "return-ready": (0x80104504, 0),
    },
    {
        "entry-row": (0x80104550, 0x00051243),
        "entry-page": (0x80104560, 0x24A20000),
        "countdown-start": (0x80104574, 0x24030009),
        "ready-call": (0x801045CC, 0),
        "stat-pages": (0x801045E4, 0x24050004),
        "halfword-token": (0x80104600, 0x90643C40),
        "stats-call": (0x80104604, 0),
        "countdown-step": (0x8010462C, 0x2442FFFE),
        "exit-priority": (0x80104650, 0x30420010),
        "paging-category": (0x8010467C, 0x24040000),
        "format-stride": (0x801046C4, 0x248409D8),
        "page-call": (0x801046F0, 0),
        "return-ready": (0x80104700, 0),
    },
    {
        "entry-page": (0x80104754, 0x24A20000),
        "unused-reset": (0x80104768, 0),
        "ready-call": (0x801047C4, 0),
        "stat-pages": (0x801047DC, 0x24050003),
        "halfword-token": (0x801047F8, 0x90643CC0),
        "stats-call": (0x801047FC, 0),
        "countup-end": (0x80104818, 0x2C620009),
        "exit-priority": (0x80104844, 0x30420010),
        "paging-category": (0x80104870, 0x24040001),
        "format-call": (0x801048B4, 0),
        "footer-icon": (0x801048DC, 0x24040115),
        "footer-slot-field": (0x80104914, 0x90621505),
        "footer-digit": (0x8010491C, 0x24420031),
        "footer-text": (0x80104934, 0),
        "return-ready": (0x80104944, 0),
    },
    {
        "entry-page": (0x801049A0, 0x24A20000),
        "unused-reset": (0x801049B4, 0),
        "ready-call": (0x80104A10, 0),
        "stat-pages": (0x80104A28, 0x24050003),
        "halfword-token": (0x80104A44, 0x90643D40),
        "stats-call": (0x80104A48, 0),
        "animation-end": (0x80104A6C, 0x2C620009),
        "three-components": (0x80104A80, 0x2C420003),
        "finish-ready": (0x80104AB8, 0),
        "exit-priority": (0x80104ADC, 0x30420010),
        "paging-category": (0x80104B08, 0x24040002),
        "format-call": (0x80104B50, 0),
        "refresh-components": (0x80104B90, 0),
        "return-ready": (0x80104BAC, 0),
    },
    {
        "entry-page": (0x80104C10, 0x24A20000),
        "countdown-start": (0x80104C24, 0x24030009),
        "ready-call": (0x80104C7C, 0),
        "stat-pages": (0x80104C94, 0x24050003),
        "halfword-token": (0x80104CB0, 0x90643D80),
        "stats-call": (0x80104CB4, 0),
        "inventory-category-field": (0x80104CEC, 0x90430642),
        "accessory-gate": (0x80104CF0, 0x24020006),
        "dp-only": (0x80104D00, 0x24040003),
        "countdown-step": (0x80104D1C, 0x2442FFFE),
        "exit-priority": (0x80104D40, 0x30420010),
        "format-stride": (0x80104DA8, 0x248418DC),
        "return-ready": (0x80104DE4, 0),
    },
    {
        "entry-page": (0x80104E40, 0x24A20000),
        "countdown-start": (0x80104E54, 0x24030009),
        "ready-call": (0x80104EAC, 0),
        "stat-pages": (0x80104EC4, 0x24050004),
        "halfword-token": (0x80104EE0, 0x90643E00),
        "stats-call": (0x80104EE4, 0),
        "countdown-step": (0x80104F04, 0x2442FFFE),
        "exit-priority": (0x80104F28, 0x30420010),
        "triangle-reread-mask": (0x80104F4C, 0x30420040),
        "triangle-store": (0x80104F54, 0),
        "paging-category": (0x80104F64, 0x24040004),
        "format-stride": (0x80104FA4, 0x248422E8),
        "return-ready": (0x80104FE0, 0),
    },
    {
        "category-mask": (0x80105024, 0x30820003),
        "active-phase": (0x80105030, 0x24020002),
        "close-clear": (0x80105040, 0),
        "reset-cursor": (0x8010503C, 0),
        "header-row": (0x80105048, 0x2404001E),
        "header-page-mask": (0x80105050, 0x3203007F),
        "header-page-store": (0x80105058, 0xA043000E),
        "dispatch-stride": (0x8010506C, 0x000210C0),
        "controller-call": (0x8010507C, 0),
        "close-result": (0x8010509C, 0x2402FFFF),
        "icons-restore": (0x801050AC, 0),
        "phase-restore": (0x801050B4, 0),
        "controls-restore": (0x801050C0, 0),
        "menu-command": (0x801050A0, 0x24040001),
        "ready-call": (0x801050CC, 0),
        "cursor-element": (0x801050E8, 0x38420008),
        "tab-mode": (0x801050D8, 0x24040000),
        "detail-draw": (0x801050F8, 0),
    },
)


def configuration(kind):
    start, end = RANGES[kind]
    return {
        "ID": IDS[kind], "ENTRY": start, "END": end, "MODULES": MODULES,
        "DRIVER": DRIVER, "SOURCES": SOURCES, "VECTORS": 1024,
        "MUTANT_VECTORS": 256, "SEED": int(IDS[kind][1:]),
        "MUTANTS": {name: {address: word}
                    for name, (address, word) in MUTATIONS[kind].items()},
        "vector": functools.partial(vector, kind), "run": functools.partial(run, kind),
        "check": check, "describe": describe,
    }


def vector(kind, rng: random.Random, number):
    state = bytearray(rng.randbytes(STATE_BYTES))
    category = number % 6
    table_category = max(0, kind - 1) % 6
    capacity = CAPACITIES[table_category]
    page, after_page = rng.randrange(capacity), rng.randrange(capacity)
    if 1 <= kind <= 6:
        at = OFFSETS[kind - 1]
        state[at] = rng.choice((0, 0, 1, 1, 2, 2, 3, 255))
        state[at + 1] = rng.choice((0, 1, 2, 3, 4, 5, 9, 10, 11, 255))
        state[at + 2], state[at + 3] = rng.randrange(40), page
    state[27] = category
    command = 0
    if 1 <= kind <= 6 and number % 7 == 0:
        command = rng.choice((1, 0x10000, -1,
                              rng.randrange(40) << 8 | (page + 1)))
    elif kind == 0:
        command = rng.choice((0, 1, 2, 3, 4, 5, -1, 0x7FFFFFFF, -0x80000000))
    elif kind == 7 and number % 3 != 0:
        command = (rng.randrange(40) << 8 | rng.choice((0, 1, 128, 192, 255))) << 4 | category
        if command == 0:
            command = 1
    orders = [[rng.choice((1, capacity, 256, 32768, 65535))
               for _ in range(192)] for _ in range(2)]
    if kind in (3, 5):
        bound = 64 if kind == 3 else 16
        orders = [[rng.randrange(1, bound + 1) for _ in range(192)] for _ in range(2)]
    item = {
        "kind": kind, "args": [command, rng.choice((0, 1, 1, 2, 255, -1))],
        "mode": rng.randrange(10), "ready": rng.choice((0, 1, -7, -0x80000000)),
        "next_page": rng.choice((page, (page + 1) % capacity, capacity - 1)),
        "token": rng.choice((1, capacity, 256, 32768, 65535)),
        "after_page": after_page, "after_row": rng.randrange(40),
        "after_pressed": rng.getrandbits(16),
        "controller_result": rng.choice((0, 0, 1, -7, -0x80000000)),
        "category": rng.choice((0, 1, 2, 6, 8, 9, 128, 255)),
        "selected": rng.choice((0, 8, 9, 10, 20, 255)),
        "inventory_category": rng.choice((0, 1, 6, 7, 255)),
        "gem_slots": [rng.choice((0, 1, 2, 3, 255)) for _ in range(2)],
        "pressed": rng.choice((0, 4, 8, 12, 16, 64, 80, 0xFFFF)),
        "state": bytes(state),
        "globals": [rng.randrange(256), rng.randrange(256), rng.randrange(256),
                    rng.randrange(256), s32(rng.getrandbits(32)), rng.randrange(256)],
        "headers": rng.randbytes(128), "orders": orders,
        "names": [ord("a"), ord("b")], "descriptions": [ord("c"), ord("d")],
    }
    if kind == 3 and command == 0 and state[OFFSETS[2]] == 0:
        item["orders"] = [
            [rng.choice((256, 32768, 65535)) for _ in range(192)]
            for _ in range(2)
        ]
    if (kind == 5 and command == 0 and state[OFFSETS[4]] == 0
            and item["ready"] != 0 and item["mode"] == 3):
        item["orders"][0] = [rng.choice((256, 32768, 65535)) for _ in range(192)]
    if kind == 0:
        selected = (item["category"] - 1) & 7
        x = (((selected + 1) & 7) * 16) + 32
        headers = bytearray(item["headers"])
        for owner in range(2):
            position = rng.choice((x + 12, x + 13, -32768, 32767))
            headers[owner * 64 + 20:owner * 64 + 22] = (
                position & 0xFFFF).to_bytes(2, "little")
        item["headers"] = bytes(headers)
    if kind == 6 and number == 1:
        state[OFFSETS[5]] = 2
        item["state"] = bytes(state)
        item["args"][0] = 0
        item["pressed"] = 64
        item["after_pressed"] = 16
        item["mode"] = 8
    return item


def flatten(item):
    return [
        item["kind"], *item["args"], item["mode"], item["ready"], item["next_page"],
        item["token"], item["after_page"], item["after_row"], item["after_pressed"],
        item["controller_result"], item["category"], item["selected"],
        item["inventory_category"], *item["gem_slots"], item["pressed"],
        *item["state"], *item["globals"], *item["headers"],
        *item["orders"][0], *item["orders"][1], *item["names"], *item["descriptions"],
    ]


def run(kind, machine, item):
    category = max(0, kind - 1) % 6
    owner = 0
    packets = []
    machine.store(STATE, item["state"])
    for address, value, size in zip(GLOBAL, item["globals"], GLOBAL_SIZES, strict=True):
        machine.put(address, value, size)
    machine.put(CONTAINER, OWNER[0])
    machine.put(STRING_BUFFER, BUFFER[0])
    machine.put(PRESSED, item["pressed"])
    machine.put(SELECTED, item["selected"], 1)
    machine.put(CURRENT_CATEGORY, item["category"], 1)
    machine.put(SCRATCH + 4, SCENE[0])
    machine.put(0x80109954, FOOTER)
    machine.store(FOOTER, b"X     0\0")
    for index in range(2):
        machine.map_zero(OWNER[index], 0x4180)
        machine.store(HEADER[index], item["headers"][index * 64:(index + 1) * 64])
        machine.store(NAME[index], bytes((item["names"][index],)) + bytes(31))
        machine.store(BUFFER[index], bytes((item["descriptions"][index],)) + bytes(95))
        for row, token in enumerate(item["orders"][index][:CAPACITIES[category]]):
            machine.put(OWNER[index] + ORDER_OFFSETS[category] + row * 2, token, 2)
        for row in range(64):
            machine.put(OWNER[index] + 0x1500 + row * 16 + 4,
                        item["gem_slots"][index], 1)
    for row in range(16):
        machine.put(0x800607AB + row * 40, item["inventory_category"], 1)
        machine.put(0x800607AA + row * 40, item["inventory_category"] ^ 7, 1)

    def snapshot():
        return [*machine.bytes(STATE, STATE_BYTES), *(
            machine.load(address, size, size == 4)
            for address, size in zip(GLOBAL, GLOBAL_SIZES, strict=True)), owner]

    def record(event, args=(0, 0, 0), extra=(0, 0)):
        machine.trace.append([event, *args, *extra, *snapshot()])

    def mutate(event):
        nonlocal owner
        target = {
            1: READY, 2: LAYOUT, 3: STATS, 4: SELECT, 5: CHANGE,
            6: POLL, 7: BEGIN, 8: EXIT, 9: CONTROLLER,
        }.get(item["mode"])
        if event != target:
            return
        owner = 1
        machine.put(CONTAINER, OWNER[1])
        machine.put(STRING_BUFFER, BUFFER[1])
        machine.put(SCRATCH + 4, SCENE[1])
        machine.put(PRESSED, item["after_pressed"])
        machine.put(SELECTED, item["selected"] ^ 9, 1)
        if 1 <= kind <= 6:
            offset = OFFSETS[kind - 1]
            machine.put(STATE + offset + 2, item["after_row"], 1)
            machine.put(STATE + offset + 3,
                        item["next_page"] if event == POLL else item["after_page"], 1)
        if event == STATS:
            machine.put(NAME[0], ord("s"), 1)
            machine.put(BUFFER[0], ord("t"), 1)
        if event == CONTROLLER:
            machine.put(GLOBAL[4], 0x10000)

    def callback(m, event, selected_category=None):
        if event == READY:
            record(event)
            mutate(event)
            m.r[2] = item["ready"] & 0xFFFFFFFF
        elif event in (BEGIN, LAYOUT):
            record(event, (s32(m.r[4]), s32(m.r[5]) if event == LAYOUT else 0, 0))
            mutate(event)
        elif event == STATS:
            record(event, (s32(m.r[4]), 0, 0))
            mutate(event)
        elif event == BARS:
            record(DP if m.r[4] == 1 else BARS)
        elif event == COMPONENT:
            record(event, (s32(m.r[4]), s32(m.r[5]), s32(m.r[6])))
            mutate(event)
        elif event == EXIT:
            record(event, (s32(m.r[4]), 0, 0))
            m.put(GLOBAL[4], m.load(PRESSED) & 16)
            mutate(event)
        elif event == POLL:
            record(event, (s32(m.r[5]), s32(m.r[4]), 0))
            mutate(event)
            m.r[2] = item["next_page"]
        elif event == SELECT:
            record(event, (s32(m.r[5]), s32(m.r[4]), 0))
            mutate(event)
            m.r[2] = item["token"]
        elif event == FORMAT:
            source = m.r[4]
            remainder = (source - OWNER[owner] if category in (0, 3) else
                         (source - OWNER[owner] - RECORD_OFFSETS[category]) % STRIDES[category])
            slot = (s32(m.load(m.r[29] + 16)) + 1 if category in (0, 3)
                    else (source - OWNER[owner] - RECORD_OFFSETS[category]) // STRIDES[category] + 1)
            m.put(m.r[5], NAME[owner])
            m.put(m.r[5] + 4, m.r[7])
            m.put(m.r[6], 0x7C050000)
            record(event, (slot, category, 0), (remainder, 0))
            mutate(event)
        elif event == CHANGE:
            first, second = m.load(m.r[5]), m.load(m.r[5] + 4)
            record(event, (s32(m.r[4]), s32(m.r[7]), s32(m.r[6])),
                   (m.load(first, 1), m.load(second, 1)))
            mutate(event)
        elif event == DRAW_SLOTS:
            record(event, (s32(m.r[4]), s32(m.r[5]), s32(m.r[6])))
        elif event == FOOTER_TEXT:
            record(event, (m.load(m.r[4] + 6, 1), s32(m.r[5]), s32(m.r[6])))
        elif event == HEADER_CALL:
            record(event, (s32(m.r[4]), 0, 0))
            returned = HEADER[owner]
            mutate(event)
            m.r[2] = returned
        elif event in (RESET, COMMAND, TABS, DETAILS):
            args = (s32(m.r[4]), 0, 0) if event in (COMMAND, TABS) else (0, 0, 0)
            record(event, args)
            mutate(event)
        elif event == CONTROLLER:
            record(event, (selected_category, s32(m.r[4]), 0))
            mutate(event)
            m.r[2] = item["controller_result"] & 0xFFFFFFFF
        else:
            raise AssertionError(event)

    callees = {
        0x800FA9D0: READY, 0x80103F64: BEGIN, 0x80104034: LAYOUT,
        0x800FC268: BARS, 0x800FC510: COMPONENT, 0x800FC704: COMPONENT,
        0x80104170: EXIT, 0x80103D6C: POLL, 0x80104114: SELECT,
        0x80104078: CHANGE, 0x80100004: DRAW_SLOTS, 0x800C6828: FOOTER_TEXT,
        0x800FDD78: RESET, 0x800C8E48: HEADER_CALL, 0x800FFA88: COMMAND,
        0x801013F8: TABS, 0x800FDEBC: DETAILS,
        **dict.fromkeys(STATS_CALLEES, STATS),
        **dict.fromkeys(FORMAT_CALLEES, FORMAT),
    }
    for address, event in callees.items():
        machine.stub(address, functools.partial(callback, event=event))
    if kind == 7:
        for index, (address, _) in enumerate(RANGES[1:7]):
            machine.stub(address, functools.partial(
                callback, event=CONTROLLER, selected_category=index))

    def sprite(m, overlay):
        xy, wh = (m.r[5], m.r[4]) if overlay else (m.r[5], m.r[6])
        record(ICON, (m.r[17], s32(xy), s32(wh)),
               (1 if overlay else 0, 0 if overlay else s32(m.r[7] - SCENE[owner])))
        pointer = PACKET + len(packets) * 32
        packets.append(pointer)
        m.map_zero(pointer, 32)
        m.r[2] = pointer

    machine.stub(0x800C0214, functools.partial(sprite, overlay=True))
    machine.stub(0x800C0224, functools.partial(sprite, overlay=False))
    machine.r[4:6] = [value & 0xFFFFFFFF for value in item["args"]]
    machine.execute(RANGES[kind][0])
    result = 0 if kind == 0 else s32(machine.r[2])
    values = [
        result, *machine.bytes(STATE, STATE_BYTES), *(
            machine.load(address, size, size == 4)
            for address, size in zip(GLOBAL, GLOBAL_SIZES, strict=True)),
        owner, machine.load(PRESSED), machine.load(SELECTED, 1),
        *machine.bytes(HEADER[0], 64), *machine.bytes(HEADER[1], 64),
        *machine.bytes(FOOTER, 8),
        len(packets), *(machine.load(pointer + 16) for pointer in packets),
        len(machine.trace), *(value for call in machine.trace for value in call),
    ]
    allowed = [
        (DEFAULT_STACK - 0x80, DEFAULT_STACK), (STATE, STATE + STATE_BYTES),
        *((address, address + size) for address, size in zip(GLOBAL, GLOBAL_SIZES, strict=True)),
        *((address, address + 64) for address in HEADER), (FOOTER, FOOTER + 8),
        *((pointer, pointer + 32) for pointer in packets),
    ]
    return {"values": values, "stores": machine.stores, "allowed": allowed}, flatten(item)


def check(image, item, observation, native, reference):
    problems = []
    if native != observation["values"]:
        first = next((index for index, (left, right) in enumerate(
            zip(native, observation["values"], strict=False)) if left != right),
            min(len(native), len(observation["values"])))
        problems.append(
            f"complete result/state/header/footer/texture/ordered callback stream differs "
            f"at {first}: native {native[first:first+8]}, original {observation['values'][first:first+8]}")
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(image, results):
    return [
        "Actual single defining MENUD body with audited terminal jr ra and delay slot; "
        "repository R3000 interpreter, Image, comparison and mutant execution unchanged.",
        "Compared complete 28-byte controller BSS, six full-width/byte globals, two 64-byte "
        "PS1 header images, owned grip text, original texture stores and ordered service arguments "
        "with pre-callback BSS/global/owner snapshots.",
        "Native services bounded at their existing ABI: ready/transition/layout/stats/bars/"
        "component/exit/paging/selection/format/change/font/sprites/cursor/command/dispatcher. "
        "No original controller branch is stubbed or replaced.",
        "Includes retained/entry/invalid stages, byte animation boundaries, exit-over-paging, "
        "fullword ready/results/close masks, source halfwords, owner/page/row changes at callbacks "
        "and borrowed formatter text changed before selection consumes it.",
        "Raw out-of-record halfwords test controller service widths, not valid equipment loading. "
        "Actual checked Live bounds and missing-owner refusal probes are separate. "
        "F02957 holds the injected armor-gate service result at the original inventory "
        "category for controller-order parity only. Corrected Live ownership intentionally "
        "differs under #366; the separate original-MIPS/actual-Live owner-delta proof retains "
        "both expectations. Zero controller-boundary mismatches do NOT assert Live owner parity.",
    ]
