"""Five MENU0 originals: full state, calls, rows and normalized rank packets.

Font and sprite callees execute their original BATTLE instructions. Framework,
action restrictions, sounds and the terminal's independently proven children
are recorded boundaries. Image reads occur only in run(), never in metadata.
"""

from __future__ import annotations

import random

import differential
from differential import Machine, s32, store_problems
from native_compile_support import COMPILER, COMPILER_OPTIONS, CXX_STANDARD

MODULE = "MENU/MENU0.PRG"
MODULES = (MODULE, "BATTLE/BATTLE.PRG")
DRIVER = "magic_menus_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\MagicMenus.cpp",
    r"Native\Game\src\Menu\MagicTerminal.cpp",
    r"Native\Game\src\Menu\MagicSpellMenu.cpp",
    r"Native\Engine\src\Render\GameFont.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
    r"Native\Engine\src\Render\UiPackets.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
INCLUDES = (
    r"Native\Engine\include",
    r"Native\Game\include",
    r"Native\Tests\include",
)
RANGES = (
    (0x80102B08, 0x80103464),
    (0x80103464, 0x801037A8),
    (0x801037A8, 0x80103AEC),
    (0x80103AEC, 0x80103E30),
    (0x801042C8, 0x80104A58),
)
HASHES = (
    "4b28901850cfdc49b990059b3f03eb8f6856949a021947005bf516922b6cb0ba",
    "d9317307296960cffc48540afb61e0643b27e8217f2c4dc7cffebe8bcebeb6c5",
    "77facd9687012e98b124cd69699987ccf612198abb446da62977256c1648d043",
    "ab0e7fbc747372b42df1206f6ed00113026527b663502f3f5b5572f5e2e781b6",
    "323c1bd43b1215007a7911d5ebf347cb1a8eeb7b3983b42e29f495c841aecdf1",
)
RENDER_SPANS = (
    (0x800C6540, 0x800C6828, "8e1dddace50549e82832613938ebc8b11e234b9ec869b0966ad97dcf419821a9"),
    (0x800C6828, 0x800C6850, "d188327d6085605429968d8d57555f5e8e774741db2a6f2816c4375440091eea"),
    (0x800C0224, 0x800C02A8, "888186cbe90bc41e25d3a12abd34b7f2f7aa2ffabc9b788305d2dad10ee53b76"),
)
COUNTS = (18, 6, 18, 8)
STEPS = (0x801068E8, 0x801068F4, 0x80106900, 0x8010690C)
AVAILABLE = (0x80106938, 0x80106950, 0x80106958, 0x80106970)
IDS = (0x800EBD70, 0x800EBD84, 0x800EBD8C, 0x800EBDA0)
RANKED = (106, 110, 114, 118, 122, 126, 130)
ACTIONS = 0x8004B9DC
MAGIC_STRINGS = 0x80104A58
TELEPORT_STRINGS = 0x80105D94
SHORTCUT = 0x800F4E6A
RESTRICTIONS = 0x800F4EA0
MEMORY = 0x800F4EE8
CURSOR_SETTING = 0x80060021
DISPLAY = 0x801022D4
BLINK = 0x8010694A
CONTROL = 0x800F51C0
ACTION = 0x800F4E98
TELEPORT_AVAILABLE = 0x8006164D
SAVE_POINTS = 0x800616EE
STATE = 0x80320000
TITLE = 0x80330000
PRIMITIVES = 0x80300000
TEXT_CHAIN = 0x80310000
ICON_CHAIN = 0x8031007C
SCRATCH = 0x1F800000
WIDTHS = 0x800EB7B4
TRIMS = 0x800EB708
GLYPHS = 0x800EB7D4
BRIGHTNESS = 0x800EC270
STOP_LINK = 0xFFFFFF

differential.MODULE_BASES[MODULE] = 0x80102800

SCALARS = (
    "init", "step", "selection", "remember", "blink", "shortcut", "idle",
    "cursor_setting", "rank_selected", "rank_count", "rank_scroll",
    "selected_row", "highlighted_row", "pressed", "repeat", "display",
    "terminal_state", "teleport_available", "child_result", "teleport_result",
    "action", "kind", "ability_type", "current_menu", "return_menu", "hook",
    "hook_state", "hook_current", "hook_action",
)
BLOCKS = (
    ("available", 18), ("ids", 18), ("unlocked", 256), ("flags", 256),
    ("strings", 128), ("schools", 4), ("points", 48), ("at_point", 3),
    ("widths", 28), ("trims", 172), ("glyphs", 60), ("brightness", 16),
)


def vector(rng: random.Random, number: int, school: int) -> dict:
    mode = number % 24
    ids = rng.sample(range(1, 256), 18)
    value = dict.fromkeys(SCALARS, 0)
    value.update(
        school=school, mode=mode, ids=ids, available=rng.sample(range(1, 256), 18),
        unlocked=[rng.randrange(2) for _ in range(256)],
        flags=[rng.choice((0, 0, 0, 1, 2, 0x80000000)) for _ in range(256)],
        strings=[rng.randrange(80, 2400) for _ in range(128)],
        schools=[rng.randrange(2) for _ in range(4)],
        points=[0] * 48, at_point=[rng.randrange(2) for _ in range(3)],
        widths=[rng.randrange(1, 10) | (rng.randrange(1, 10) << 4) for _ in range(28)],
        trims=[rng.randrange(256) for _ in range(172)],
        glyphs=[rng.randrange(256) for _ in range(60)],
        brightness=[rng.randrange(256) for _ in range(16)],
        idle=rng.randrange(2), cursor_setting=rng.choice((0, 1, 128, 255)),
        shortcut=rng.choice((0, 0, 1, 255)), remember=rng.randrange(2),
        selected_row=-1, display=rng.randrange(2), blink=rng.randrange(16),
        selection=rng.choice((-2, -1, 1, 255, -2147483648)),
        rank_count=rng.randrange(2, 5), rank_scroll=rng.randrange(256),
        action=rng.randrange(-2147483648, 2147483648),
        kind=rng.randrange(-32768, 32768), ability_type=rng.randrange(256),
        current_menu=rng.randrange(256), return_menu=rng.randrange(256),
        restrictions=rng.choice((0, 0x48, 1, 2, 4, 0x10, 0x20, 0x80, 0xB7)),
        hook=-1, hook_state=rng.randrange(256), hook_current=rng.randrange(256),
        hook_action=rng.randrange(-2147483648, 2147483648),
    )
    value["rank_selected"] = rng.randrange(value["rank_count"])
    if school == 4:
        return terminal_vector(rng, number, value)
    count = COUNTS[school]
    value["highlighted_row"] = rng.randrange(count)
    if mode < 3:
        value["init"] = (1, 2, 0xFFFFFFFF)[mode]
        value["step"] = rng.randrange(5 if school == 0 else 3)
    elif mode <= 10:
        value["step"] = 0
        value["idle"] = mode != 3
        value["shortcut"] = 0 if mode == 3 else value["shortcut"]
        if mode == 4:
            value.update(idle=0, shortcut=255)
        if mode in (5, 6):
            value["unlocked"] = [mode == 6] * 256
        if mode in (7, 8):
            value["unlocked"] = [0] * 256
            value["unlocked"][ids[0 if mode == 7 else count - 1]] = 1
    elif mode <= 16:
        value["step"] = 1
        value["selected_row"] = {
            11: -1, 12: rng.randrange(count), 13: -2, 14: -2, 15: -3,
            16: 0x7FFFFFFF,
        }[mode]
        if mode in (13, 14):
            value["shortcut"] = mode - 13
        if school == 0 and mode in (11, 12):
            row = value["highlighted_row"] if mode == 11 else value["selected_row"]
            action = (130, 255, *RANKED)[(number // 24) % 9]
            value["available"][row] = action
            if action == 130:
                value["unlocked"][131] = 1
    elif mode == 17 or school != 0:
        value["step"] = 2
    elif mode in (18, 19):
        value.update(step=3, selection=rng.choice(RANKED),
                     rank_selected=rng.choice((0, 1, 2, 3, 128, 255)),
                     idle=mode == 19)
    else:
        value.update(step=4, selection=rng.choice(RANKED))
        if mode == 20:
            value["repeat"] = rng.choice((0, 0x2000, 0x8000, 0xA000))
        elif mode == 21:
            value.update(pressed=0x20, repeat=rng.choice((0, 0x2000, 0x8000, 0xA000)))
            value["flags"][value["selection"] + value["rank_selected"]] = 1
        elif mode == 22:
            value["pressed"] = rng.choice((0x10, 0x40, 0x70))
        else:
            value["pressed"] = 0x20
            value["flags"][value["selection"] + value["rank_selected"]] = 0
    return value


def terminal_vector(rng: random.Random, number: int, value: dict) -> dict:
    value["terminal_state"] = (0, 1, 2, 255, *range(3, 18))[number % 19]
    value["shortcut"] = rng.randrange(5)
    value["teleport_available"] = rng.choice((0, 1, 2, 255))
    if number % 3:
        value["points"][rng.choice((0, 47, rng.randrange(48)))] = rng.choice((1, 2, 255))
    value["selected_row"] = rng.choice((-1, -2, -3, 0, 1, 2, 3, 4, 0x7FFFFFFF))
    value["child_result"] = rng.choice((0, 0, -1, -2, 1, 106, 255, 0x7FFFFFFF, -2147483648))
    value["teleport_result"] = rng.choice((0, -1, -2, 1, 0x7FFFFFFF, 0x07FA5612))
    value["hook"] = rng.choice((-1, -1, 0, 17, 18))
    return value


def inputs(value: dict) -> list[int]:
    return [
        value["school"], *(int(value[key]) for key in SCALARS), value["restrictions"],
        *(int(item) for key, _ in BLOCKS for item in value[key]),
    ]


def text_ref(address: int) -> list[int]:
    if MAGIC_STRINGS <= address < TELEPORT_STRINGS and address % 2 == 0:
        return [0, (address - MAGIC_STRINGS) // 2]
    if TELEPORT_STRINGS <= address < TELEPORT_STRINGS + 3198 and address % 2 == 0:
        return [1, (address - TELEPORT_STRINGS) // 2]
    if (ACTIONS + 28 <= address < ACTIONS + 52 * 256
            and (address - ACTIONS - 28) % 52 == 0):
        return [2, (address - ACTIONS - 28) // 52]
    return [-1, address]


def setup(machine: Machine, value: dict) -> None:
    for address, size in (
        (ACTIONS, 52 * 256), (CURSOR_SETTING, 1), (SHORTCUT, 1),
        (RESTRICTIONS, 4), (MEMORY, 256), (DISPLAY, 1), (CONTROL, 4),
        (ACTION, 8), (SAVE_POINTS, 48), (TELEPORT_AVAILABLE, 1), (STATE, 4),
        (TITLE, 64), (PRIMITIVES, 0x10000), (TEXT_CHAIN, 0x100), (SCRATCH, 16),
    ):
        machine.map_zero(address, size)
    for key, address, size in (
        ("shortcut", SHORTCUT, 1), ("restrictions", RESTRICTIONS, 4),
        ("cursor_setting", CURSOR_SETTING, 1), ("display", DISPLAY, 1),
        ("terminal_state", STATE, 1), ("action", ACTION + 4, 4),
        ("kind", ACTION + 2, 2), ("ability_type", CONTROL + 2, 1),
        ("current_menu", CONTROL, 1), ("return_menu", CONTROL + 1, 1),
        ("teleport_available", TELEPORT_AVAILABLE, 1),
        ("rank_selected", MEMORY + 12, 1), ("rank_count", MEMORY + 13, 1),
        ("rank_scroll", MEMORY + 14, 1), ("blink", BLINK, 1),
        ("pressed", 0x8005E1D0, 4), ("repeat", 0x8005DFDC, 4),
    ):
        machine.put(address, value[key], size)
    for i, unlocked in enumerate(value["unlocked"]):
        machine.put(ACTIONS + i * 52 + 12, (int(unlocked) << 15) | 0x51A54012)
    for i, offset in enumerate(value["strings"]):
        machine.put(MAGIC_STRINGS + i * 2, offset, 2)
    machine.store(SAVE_POINTS, bytes(value["points"]))
    school = value["school"]
    if school < 4:
        base = STEPS[school]
        for at, key in enumerate(("step", "selection", "remember")):
            machine.put(base + at * 4, value[key])
        machine.store(AVAILABLE[school], bytes(value["available"][:COUNTS[school]]))
        machine.store(IDS[school], bytes(value["ids"][:COUNTS[school]]))
    for key, address in (("widths", WIDTHS), ("trims", TRIMS), ("glyphs", GLYPHS),
                         ("brightness", BRIGHTNESS)):
        machine.store(address, bytes(value[key]))
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, ICON_CHAIN + 4)
    machine.put(SCRATCH + 8, TEXT_CHAIN)
    for at in range(TEXT_CHAIN, TEXT_CHAIN + 0x100, 4):
        machine.put(at, STOP_LINK)


def install_boundaries(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    point_index = 0

    def hook(code: int) -> None:
        if code == value["hook"]:
            machine.put(STATE, value["hook_state"], 1)
            machine.put(CONTROL, value["hook_current"], 1)
            machine.put(ACTION + 4, value["hook_action"], 4)

    def record(code: int, *args: int) -> None:
        calls.append([code, *args])
        hook(code)

    def constant(code: int, result: int, argc: int = 0):
        def call(m: Machine) -> None:
            record(code, *(s32(m.r[4 + i]) for i in range(argc)))
            m.r[2] = result & 0xFFFFFFFF
        return call

    def rows(m: Machine) -> None:
        count, style, texts, types = m.r[4:8]
        event = [3, style, m.load(CURSOR_SETTING, 1), count]
        for i in range(count):
            event.extend(text_ref(m.load(texts + i * 8)))
            event.extend(text_ref(m.load(texts + i * 8 + 4)))
            event.append(s32(m.load(types + i * 4)))
        calls.append(event)
        hook(3)

    def flags(m: Machine) -> None:
        action = m.r[5]
        record(2, action)
        m.r[2] = value["flags"][action]

    def unlocked_school(m: Machine) -> None:
        record(13, m.r[4])
        m.r[2] = value["schools"][m.r[4]]

    def at_point(m: Machine) -> None:
        nonlocal point_index
        record(14)
        m.r[2] = value["at_point"][min(point_index, 2)]
        point_index += 1

    def title(m: Machine) -> None:
        text = m.load(m.r[29] + 20)
        record(15, m.r[4] - 10, *text_ref(text), m.r[7])
        m.r[2] = TITLE

    def child(index: int):
        def call(m: Machine) -> None:
            record(17, index, m.r[4], m.load(STATE, 1))
            m.r[2] = value["child_result"] & 0xFFFFFFFF
        return call

    def teleport(m: Machine) -> None:
        record(18, m.r[4], m.load(STATE, 1))
        m.r[2] = value["teleport_result"] & 0xFFFFFFFF

    for address, handler in (
        (0x800FA9D0, constant(0, value["idle"])),
        (0x800FA92C, constant(1, 0, 2)), (0x8008A5D0, flags),
        (0x801005E0, rows), (0x801008C8, constant(4, value["selected_row"])),
        (0x801008B0, constant(5, value["highlighted_row"])),
        (0x80102884, constant(6, 0, 1)),
        (0x800FA8E0, constant(7, 0, 1)), (0x800FFBA8, constant(8, 0)),
        (0x800FFA88, constant(9, 0, 1)), (0x800FA8A0, constant(11, 0, 1)),
        (0x800FFBC8, constant(12, 0)), (0x800CAEAC, unlocked_school),
        (0x8008A4FC, at_point), (0x800C8E5C, title),
        (0x80104254, constant(16, 0)),
    ):
        machine.stub(address, handler)
    for code, address in enumerate((0x800C02E8, 0x800C02F0, 0x800C02E0, 0x800C02F8)):
        machine.stub(address, lambda _m, code=code: record(10, code))
    if value["school"] == 4:
        for index, (entry, _end) in enumerate(RANGES[:4]):
            machine.stub(entry, child(index))
        machine.stub(0x80103E30, teleport)
    machine.follow.update((0x800C6828, 0x800C6540, 0x800C0224))


def chain(machine: Machine, head: int) -> tuple[list[int], int]:
    result: list[int] = []
    packets = 0
    empty_nodes = 0
    address = machine.load(head) & 0xFFFFFF
    seen: set[int] = set()
    while address != STOP_LINK:
        address |= 0x80000000
        if address in seen or not PRIMITIVES <= address < PRIMITIVES + 0x10000:
            raise ValueError(f"invalid rank chain at {address:#x}")
        seen.add(address)
        tag = machine.load(address)
        count = tag >> 24
        if count:
            packets += 1
            result.extend((count, *(machine.load(address + i * 4)
                                     for i in range(1, count + 1))))
        else:
            empty_nodes += 1
        address = tag & 0xFFFFFF
    return [packets, *result], empty_nodes


def flattened(calls: list[list[int]]) -> list[int]:
    return [len(calls), *(word for call in calls for word in (len(call), *call))]


def allowed_stores(school: int) -> list[tuple[int, int]]:
    result = [
        (0x803FC000, 0x80400010), (MEMORY, MEMORY + 256),
        (CURSOR_SETTING, CURSOR_SETTING + 1), (DISPLAY, DISPLAY + 1),
        (BLINK, BLINK + 1), (STATE, STATE + 4), (TITLE, TITLE + 64),
        (CONTROL, CONTROL + 4), (ACTION, ACTION + 8),
        (PRIMITIVES, PRIMITIVES + 0x10000), (SCRATCH, SCRATCH + 12),
        (TEXT_CHAIN, TEXT_CHAIN + 4), (ICON_CHAIN, ICON_CHAIN + 4),
    ]
    if school < 4:
        result.extend(((STEPS[school], STEPS[school] + 12),
                       (AVAILABLE[school], AVAILABLE[school] + COUNTS[school])))
    return result


def run(machine: Machine, value: dict) -> tuple[dict, list[int]]:
    school = value["school"]
    start, end = RANGES[school]
    if machine.images[0].sha256(start, end) != HASHES[school]:
        function_id = 2600 if school == 4 else 2604 + school
        raise ValueError(f"F{function_id:05} original span hash differs")
    if school == 0:
        for first, last, expected in RENDER_SPANS:
            if machine.images[1].sha256(first, last) != expected:
                raise ValueError(f"original rendering support at {first:#x} differs")
    setup(machine, value)
    calls: list[list[int]] = []
    install_boundaries(machine, value, calls)
    machine.r[4] = STATE if school == 4 else value["init"]
    machine.execute(start)
    output = [0, s32(machine.r[2])]
    if school < 4:
        output.extend((machine.load(STEPS[school]), s32(machine.load(STEPS[school] + 4)),
                       machine.load(STEPS[school] + 8), machine.load(BLINK, 1)))
        output.extend(machine.load(MEMORY + i, 1) for i in (12, 13, 14))
        output.extend((machine.load(CURSOR_SETTING, 1), machine.load(DISPLAY, 1)))
        output.extend(machine.bytes(AVAILABLE[school], COUNTS[school]))
        text, text_empty = chain(machine, TEXT_CHAIN)
        icons, icon_empty = chain(machine, ICON_CHAIN)
        output.extend(text)
        output.extend(icons)
        output.append((machine.load(SCRATCH) - PRIMITIVES) // 4 - text_empty - icon_empty)
    else:
        output.extend((machine.load(STATE, 1), s32(machine.load(ACTION + 4)),
                       machine.load(ACTION + 2, 2, signed=True),
                       machine.load(CONTROL + 2, 1), machine.load(CONTROL, 1),
                       machine.load(CONTROL + 1, 1), machine.load(CURSOR_SETTING, 1),
                       machine.load(TITLE + 6, 1)))
    output.extend(flattened(calls))
    return {"values": output, "stores": machine.stores, "mode": value["mode"]}, inputs(value)


def check(_image, value: dict, observation: dict, native: list[int], _reference) -> list[str]:
    expected = observation["values"]
    problems = store_problems(observation["stores"], allowed_stores(value["school"]))
    if native != expected:
        at = next((i for i, pair in enumerate(zip(native, expected, strict=False))
                   if pair[0] != pair[1]), min(len(native), len(expected)))
        problems.append(f"mode {value['mode']} output {at}: native {native[at:at+12]} "
                        f"original {expected[at:at+12]} (lengths {len(native)}/{len(expected)})")
    return problems


def original_mutants(school: int) -> dict[str, dict[int, int]]:
    if school == 4:
        return terminal_mutants()
    # Shaman, Sorcerer and Enchanter share identical instruction offsets.
    if school:
        shift = RANGES[school][0] - RANGES[1][0]
        changes = {
            "init-school": (0x80103498, 0x24040004),
            "init-shortcut-value": (0x801034A4, 0x38630001),
            "init-state-not-reset": (0x801034C0, 0),
            "unlock-bit": (0x80103578, 0x00021382),
            "action-byte-signed": (0x80103550, 0x80510000),
            "name-offset": (0x80103590, 0x2508B9FC),
            "restricted-row-bit": (0x801035D8, 0x34420002),
            "available-store": (0x801035EC, 0),
            "global-mask": (0x80103618, 0x30420048),
            "global-row-bit": (0x80103630, 0x34420002),
            "cursor-setting": (0x8010365C, 0x24020002),
            "row-style": (0x80103668, 0x2405020F),
            "cursor-restore": (0x80103680, 0),
            "selected-increment": (0x80103694, 0x24440002),
            "hide-cost": (0x801036B0, 0),
            "shortcut-cancel": (0x801036C0, 0x2402FFFD),
            "back-keeps-title": (0x801036DC, 0x24040001),
            "selected-action-signed": (0x80103700, 0x8042FFFF),
            "clear-keeps-row": (0x80103710, 0x24040000),
            "dismiss-call": (0x80103714, 0),
            "command-call": (0x8010371C, 0),
            "return-state": (0x80103728, 0x24020001),
            "preview-action-signed": (0x80103744, 0x80440000),
            "cost-call": (0x80103748, 0),
            "return-load": (0x80103768, 0x00001021),
        }
        return {name: {address + shift: word} for name, (address, word) in changes.items()}
    changes = {
        "init-school": (0x80102B3C, 0x24040001),
        "init-shortcut-value": (0x80102B48, 0x38630001),
        "init-state-not-reset": (0x80102B64, 0),
        "action-byte-signed": (0x80102BF4, 0x80520000),
        "unlock-bit": (0x80102C1C, 0x00021382),
        "name-index": (0x80102BD0, 0x2416007E),
        "restricted-row-bit": (0x80102C84, 0x34420002),
        "available-store": (0x80102C98, 0),
        "global-mask": (0x80102CC8, 0x30420048),
        "global-row-bit": (0x80102CE0, 0x34420002),
        "cursor-restore": (0x80102D30, 0),
        "row-style": (0x80102D18, 0x24050208),
        "selected-increment": (0x80102D44, 0x24440002),
        "hide-cost": (0x80102D60, 0),
        "shortcut-cancel": (0x80102D70, 0x2402FFFD),
        "selected-action-signed": (0x80102DCC, 0x8042FFFF),
        "ranked-action-signed": (0x80102DDC, 0x80420000),
        "hoist-rank-mode": (0x80102E28, 0x24050001),
        "preview-action-signed": (0x80102E98, 0x80510000),
        "variable-cost-sign": (0x80102EFC, 0),
        "return-load": (0x80102F2C, 0x00001021),
        "third-rank-offset": (0x80102F54, 0x24830001),
        "fourth-rank-count": (0x80102FBC, 0x24110003),
        "cursor-clamp": (0x80102FE0, 0),
        "blink-reset": (0x80102FEC, 0),
        "quit-button": (0x80103014, 0x30620080),
        "back-row-end": (0x80103080, 0x2A22001D),
        "confirm-restriction": (0x801030AC, 0x10400012),
        "rank-addition": (0x801030EC, 0x00401021),
        "right-bound": (0x80103114, 0x26620000),
        "rank-cursor-store": (0x8010316C, 0),
        "rank-ordering-bucket": (0x801031A0, 0x2456FFF0),
        "rank-brightness-signed": (0x801031E8, 0x80440000),
        "blink-wrap": (0x80103218, 0x30420007),
        "dim-brightness": (0x8010324C, 0x24040041),
        "locked-icon-mode": (0x801032B8, 0x34632001),
        "glyph-clut": (0x801032CC, 0x3C0C37F6),
        "glyph-transparency": (0x801032DC, 0x3C0C0100),
        "rank-font-page": (0x80103324, 0x3442000C),
        "arrow-left-x": (0x80103370, 0x34A50051),
        "arrow-mirrored-u": (0x801033B4, 0x34E7305E),
        "arrow-right-x": (0x801033C0, 0x3C04000D),
        "arrow-link": (0x80103424, 0),
        "primitive-cursor": (0x80103428, 0x25620024),
    }
    return {name: {address: word} for name, (address, word) in changes.items()}


def terminal_mutants() -> dict[str, dict[int, int]]:
    changes = {
        "state-byte-offset": (0x801042F8, 0x90820001),
        "shortcut-info-init": (0x8010433C, 0),
        "shortcut-label-index": (0x8010437C, 0x2442FFFE),
        "shortcut-width": (0x80104388, 0x2407008D),
        "title-selected": (0x801043A0, 0),
        "unlocked-school-result": (0x80104450, 0x1040000B),
        "locked-description-index": (0x801043E8, 0x241E0006),
        "teleport-discovered-last": (0x801044D4, 0x2A02002F),
        "teleport-disabled-description": (0x80104560, 0x24425EB6),
        "teleport-type": (0x801045A8, 0x24120002),
        "submenu-cursor": (0x801045D0, 0),
        "submenu-style": (0x801045D8, 0x24050106),
        "cursor-restore": (0x801045E8, 0),
        "submenu-state": (0x801045F0, 0x24020004),
        "selected-increment": (0x80104608, 0x24710002),
        "warlock-dispatch-init": (0x80104648, 0x24040002),
        "shaman-dispatch-state": (0x8010465C, 0x24020008),
        "sorcerer-dispatch-state": (0x80104678, 0x2402000A),
        "enchanter-dispatch-init": (0x8010469C, 0x24040002),
        "teleport-dispatch-state": (0x801046B0, 0x2402000D),
        "cancel-back-state": (0x801046F4, 0x24020011),
        "resumed-width": (0x80104714, 0x2407007F),
        "resumed-init": (0x80104730, 0x24040001),
        "warlock-action-type": (0x8010477C, 0x24020007),
        "shaman-action-type": (0x80104810, 0x24020009),
        "sorcerer-action-type": (0x80104890, 0x2402000B),
        "enchanter-action-type": (0x8010490C, 0x2402000D),
        "teleport-selection-kind": (0x80104940, 0x2403000B),
        "teleport-packed-result": (0x80104948, 0x26230000),
        "back-submenu-state": (0x80104974, 0x24020005),
        "execute-selection-kind": (0x801049C0, 0x24030008),
        "execute-return-state": (0x801049DC, 0),
        "exit-current-menu": (0x80104A18, 0),
        "exit-state-store": (0x80104A20, 0),
    }
    return {name: {address: word} for name, (address, word) in changes.items()}


def describe(_image, results) -> list[str]:
    return [
        f"Modes observed: {sorted({value['mode'] for value, _, _ in results})}",
        "Every visible row and complete normalized font/icon chain compared; "
        "original font/sprite callees run, zero-length chain nodes omitted.",
        "Unpopulated original stack row types are outside the observable row contract (#373).",
    ]


def configure(school: int) -> dict:
    entry, end = RANGES[school]
    return {
        "ID": "F02600" if school == 4 else f"F{2604 + school:05}",
        "MODULES": MODULES, "SOURCES": SOURCES, "DRIVER": DRIVER,
        "COMPILER": COMPILER, "CXX_STANDARD": CXX_STANDARD,
        "COMPILER_OPTIONS": COMPILER_OPTIONS, "INCLUDES": INCLUDES,
        "ENTRY": entry, "END": end, "VECTORS": 1024, "SEED": 2604 + school,
        "MUTANT_VECTORS": 256,
        "vector": lambda rng, number: vector(rng, number, school),
        "run": run, "check": check, "describe": describe,
        "MUTANTS": original_mutants(school),
    }
