"""Inventory terminals: real per-overlay R3000 controllers against typed native state machines.

Only actual callees are stubbed. Row formatters write distinctive text tokens and flags;
the list/action creations capture their consumed rows. Heap calls compare owned workspace
lifetime, not pointer widths. Unused workspace cells are deliberately dirty in the original.
"""

from __future__ import annotations

import functools
import random

from capstone import CS_ARCH_MIPS, CS_MODE_LITTLE_ENDIAN, CS_MODE_MIPS32, Cs
from differential import DEFAULT_STACK, MODULE_BASES, Fault, s32, store_problems

MODULE_BASES.update({r"MENU\MENU3.PRG": 0x80102800, r"MENU\MENUB.PRG": 0x80102800,
                     r"MENU\MAINMENU.PRG": 0x800F9800})
IDS = ("F02591", "F02592", "F02597", "F02553", "F02636")
RANGES = {
    "menu3": ((0x80104530, 0x80104788), (0x80104788, 0x80104EC0),
              (0x801053EC, 0x80106390), (0x800FAEBC, 0x800FB3C8),
              (0x80108970, 0x801090C4)),
    "menub": ((0x80104608, 0x80104860), (0x80104860, 0x80104F98),
              (0x80105454, 0x80106274)),
}
CAPACITIES = (8, 16, 16, 8, 16, 48, 64, 2)
INVENTORY, INV_BYTES = 0x80060168, 0xF00
SETTINGS, CURSOR, PRESSED, REPEAT, HELD = (
    0x80060021, 0x800F4F22, 0x8005E1D0, 0x8005DFDC, 0x8005E238)
RESTRICTIONS, FREEZE, CONTROL, USE, INVOKED, HUD = (
    0x800F4EA0, 0x801023E3, 0x800F51C0, 0x800F4E98, 0x800F5210, 0x800EB9B0)
ROW_TYPES_PTR, INFO_DONE, INTRO = 0x800F4E84, 0x800F5130, 0x80061643
HELP_PTR, NAMES_PTR, MAP, ACTIONS = 0x80102540, 0x8010229C, 0x80102214, 0x8004B9DC
CAPACITY_TABLE, ORDER_PTRS, MISC_ORDER = 0x801022A0, 0x801022A8, 0x80061A48
ANIMATION, FADE = 0x800EBBC8, 0x801022D8
SHORTCUT_ACTIONS, SHORTCUT_SELECT, SHORTCUT_STEP = 0x80102410, 0x80102450, 0x80102454
SCREEN_RESUME, SCREEN_COUNTDOWN = 0x80109649, 0x80109710
HEAP, HELP, ROWS, ORDER, ROW_TYPES, CHAR, STATS, STEP_PTR = (
    0x80A00000, 0x80A10000, 0x80A30000, 0x80A40000, 0x80A50000,
    0x80A60000, 0x80A61000, 0x80A70000)
FORMATTED = 0x80A80000
ROW_FIELDS = ((0, 1, False), (6, 1, False), (7, 1, False), (13, 1, False),
              (20, 2, True), (22, 2, True), (24, 2, True), (18, 2, False), (16, 2, False))
LAYOUT = {
    "menu3": {"sort": 0x80109668, "discard": 0x8010966A, "template": 0x80109610,
              "brightness": 0x8010961F, "nav": 0x80109674, "panels": 0x80109711,
              "preview": 0x80109568, "menu": 0x801093B8, "manage_index": 8,
              "tarnished": 0x801094FE, "mp": 0x80109534, "prohibited": 0},
    "menub": {"sort": 0x8010A608, "discard": 0x8010A60A, "template": 0x8010A5B4,
              "brightness": 0x8010A5C3, "nav": 0x8010A614, "panels": 0x8010A6B9,
              "preview": 0x8010A514, "menu": 0x8010A280, "manage_index": 18,
              "tarnished": 0, "mp": 0, "prohibited": 0x8010A4B0},
}
# IDs match the endpoint-only World fixture's Call enum.
(READY, INIT_INFO, DISCARD_INFO, DISMISS_INFO, INFORMATION, SOUND, UPDATE_ACTIONS,
 ACTION_RESULT, CREATE_ACTIONS, ACTION_INFORMATION, SORT, GET_ROW, SET_ROW, CLEAR,
 FLY_OUT, FONT, DIGIT, CURSOR_DRAW, CLEAR_ITEM, REBUILD, PARENT_WEAPON, PARENT_SHIELD,
 FIND, ACTION_FLAGS, BEGIN_ROWS, END_ROWS, POPULATE, FORMAT_MISC, CREATE_LIST,
 RESTORE, SELECTED, STORE_CURSOR, DETAILS, DISCARD, SORT_MENU, LOAD_TEXT, UNLOAD_TEXT,
 RENDER_SCREEN, BASE_STATS, ACTION_STATE, HUD_STATE, ASHLEY, NAVIGATION, EQUIPMENT,
 BACKGROUND_PRIORITY, CLOSE_BACKGROUND, CLEAR_COMMAND, COUNT, ICONS, STATUS) = range(1, 51)

# Stable original instruction replacements; the proof run must detect every one.
MUTATIONS = {
    ("menu3", 0): {
        "open-category": (0x80104548, 0x24840000),
        "sort-state": (0x8010465C, 0x24020000),
        "menu-id": (0x80104654, 0x24A5002E),
        "criterion-name": (0x801045EC, 0x9442036E),
        "criterion-description": (0x80104610, 0x9442036C),
        "weapon-dispatch": (0x801046E4, 0),
        "shield-dispatch": (0x80104704, 0),
        "misc-dispatch": (0x80104724, 0),
        "equipment-dispatch": (0x80104754, 0),
        "cancel-sound": (0x80104764, 0),
    },
    ("menu3", 1): {
        "kind-mask": (0x80104870, 0x30420000),
        "single-menu": (0x801048F8, 0x24050005),
        "maximum-digit": (0x80104960, 0xA2020007),
        "quantity-start": (0x80104980, 0xA0420000),
        "single-clear": (0x80104A20, 0),
        "confirm-mask": (0x80104A9C, 0x30620050),
        "select-sound": (0x80104B58, 0),
        "digit-font": (0x80104CDC, 0x24040004),
        "cursor": (0x80104D74, 0),
        "quantity-clear": (0x80104E78, 0),
    },
    ("menu3", 2): {
        "initial-title-threshold": (0x80105430, 0x28A200B4),
        "initial-title-target": (0x80105454, 0x240200B5),
        "cursor-reset": (0x8010549C, 0),
        "global-restriction": (0x8010561C, 0x30420000),
        "manage-threshold": (0x8010558C, 0x2C420004),
        "manage-tarnish-mask": (0x801055B8, 0x3C030004),
        "restore-cursor": (0x801056E0, 0),
        "details": (0x8010580C, 0),
        "selection-flag": (0x8010589C, 0x34420004),
        "misc-clear": (0x80105B58, 0),
        "parent-search": (0x801062E4, 0),
        "action-menu": (0x80106068, 0x26850027),
        "discard-entry": (0x801061D4, 0),
        "sort-entry": (0x80106224, 0),
        "discard-poll": (0x80106308, 0),
        "sort-poll": (0x8010632C, 0),
    },
    ("menu3", 3): {
        "reentry-latch": (0x800FAEF8, 0x32020000),
        "load-request": (0x800FAFB8, 0),
        "list-menu": (0x800FB208, 0x34A59225),
        "key-threshold": (0x800FB0A8, 0x2C4201C8),
        "upgrade-count": (0x800FB0DC, 0x24050001),
        "upgrade-bit": (0x800FB104, 0x00021382),
        "restriction-mask": (0x800FB144, 0x30420000),
        "disabled-flag": (0x800FB18C, 0x34420002),
        "close-menu": (0x800FB378, 0x2402003E),
        "selection-kind": (0x800FB36C, 0x24020007),
    },
    ("menu3", 4): {
        "comparison-actor": (0x801089C4, 0x2404000F),
        "navigation-open": (0x80108A40, 0x24840000),
        "equipment-open": (0x80108AB4, 0),
        "use-type": (0x80108B20, 0x2402000C),
        "container-restriction": (0x80108C18, 0x34420100),
        "quit-countdown": (0x80108D44, 0x24020009),
        "quit-unload": (0x80108D9C, 0),
        "use-unload": (0x80108DEC, 0),
        "resume-latch": (0x80108E28, 0x24020000),
        "secondary-cap": (0x80108E54, 0x2C620009),
        "icons-cap": (0x80108F78, 0x2C620003),
        "status-cap": (0x80108FD8, 0x2C620009),
        "status-button": (0x8010906C, 0),
    },
}
# Sort/discard bodies have a constant instruction displacement in MENUB,
# independently verified by disassembly; navigation does not.
for _kind, _shift in ((0, 0xD8), (1, 0xD8)):
    MUTATIONS[("menub", _kind)] = {
        name: (address + _shift, word)
        for name, (address, word) in MUTATIONS[("menu3", _kind)].items()
    }
MUTATIONS[("menub", 2)] = {
    "initial-title-target": (0x801054BC, 0x240200B5),
    "cursor-reset": (0x80105504, 0),
    "manage-disabled": (0x801055B8, 0x24070000),
    "restore-cursor": (0x801056A8, 0),
    "details": (0x801057BC, 0),
    "misc-clear": (0x80105B0C, 0),
    "use-disabled": (0x80105C20, 0),
    "parent-search": (0x801061C4, 0),
    "action-menu": (0x80105F94, 0x26850027),
    "discard-entry": (0x801060B0, 0),
    "sort-entry": (0x801060FC, 0),
    "discard-poll": (0x801061E8, 0),
    "sort-poll": (0x8010620C, 0),
}


def configuration(kind: int, overlay: str) -> dict:
    entry, end = RANGES[overlay][kind]
    modules = ((r"MENU\MAINMENU.PRG", r"MENU\MENU3.PRG") if kind == 3 else
               (rf"MENU\{overlay.upper()}.PRG", r"MENU\MAINMENU.PRG"))
    return {
        "ID": IDS[kind], "ENTRY": entry, "END": end, "MODULES": modules,
        "DRIVER": "inventory_terminal_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\Inventory\InventoryTerminal.cpp",
                    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
                    r"Native\Engine\src\Core\Math\Distance.cpp"),
        "INCLUDES": (r"Native\Tests\include",), "CXX_STANDARD": "c++latest",
        "VECTORS": 1024, "MUTANT_VECTORS": 256, "SEED": 2591 + kind * 73,
        "MUTANTS": {name: {address: word}
                    for name, (address, word) in MUTATIONS[(overlay, kind)].items()},
        "vector": functools.partial(vector, kind, overlay),
        "run": functools.partial(run, kind, overlay), "check": check,
        "describe": functools.partial(describe, kind, overlay),
    }


def flatten(items):
    return [value for item in items for value in item]


def random_inventory(rng: random.Random) -> list[int]:
    block = list(rng.randbytes(INV_BYTES))
    for slot in range(64):
        item = rng.choice((323, 324, 428, 456, 458, 459, 500))
        block[0xE00 + slot * 4:0xE02 + slot * 4] = (item & 255, item >> 8)
        block[0xE02 + slot * 4] = rng.randrange(1, 101)
    return block


def vector(kind: int, overlay: str, rng: random.Random, number: int) -> dict:
    category = number % 8
    step = number % (2, 8, 13, 5, 14)[kind]
    if kind == 2 and step in (6, 8, 9, 10):
        category = number % 7
    if kind < 2:
        category = number % 7
    count = rng.randrange(1, CAPACITIES[category] + 1)
    chosen = rng.randrange(1, count + 1)
    selected = rng.choice((-3, -2, -1, -1, 0, chosen - 1))
    actions = ([1, 3, 4, 5] if category == 6 else
               [2, 3, 5] if category in (1, 2, 5) else [3, 5])
    inventory = random_inventory(rng)
    # Deliberate same-id/remainder cases exercise both actual merge variants.
    if number % 3 == 0:
        for slot in range(64):
            inventory[0xE00 + slot * 4:0xE02 + slot * 4] = (67, 1)
    ready = [rng.choice((0, 1, 1, 1)) for _ in range(8)]
    cursor = [(category + 1) & 7, rng.randrange(256), *list(rng.randbytes(16))]
    panels = [rng.randrange(2), rng.randrange(2), rng.randrange(3),
              rng.randrange(11), rng.randrange(5), rng.randrange(11),
              rng.randrange(2), rng.randrange(4)]
    runtime = [*cursor, *panels, *rng.choices(range(64), k=4),
               rng.randrange(10), rng.randrange(10), rng.randrange(256),
               rng.randrange(500), rng.randrange(64), rng.randrange(2),
               rng.randrange(2), rng.choice((0, 0, 0, 1, 2, 4, 0x200)),
               s32(rng.getrandbits(32))]
    init = 0
    if number % 11 == 0:
        init = category + 1 if kind == 0 else rng.choice((1, 2, 3))
    discard_slot = rng.randrange(1, CAPACITIES[category] + 1)
    discard_kind = rng.randrange(3) if category == 6 else 0
    if kind == 1:
        if step in (2, 4, 5, 6, 7):
            category, discard_kind = 6, 1
            discard_slot = rng.randrange(1, 65)
        if init:
            init = discard_slot | (category << 8) | (discard_kind << 16)
    action_result = rng.choice((-3, -2, -1, -1, 0, 1))
    if kind == 0:
        sizes = (24, 24, 7, 24, 23, 16, 2)
        action_result = rng.choice((-3, -2, -1, rng.randrange(sizes[category])))
    elif kind == 2:
        action_result = rng.choice((-3, -2, -1, *range(len(actions))))
    orders = []
    for capacity in CAPACITIES[:7]:
        items = rng.sample(range(1, capacity + 1), capacity)
        orders.append(items + [0] * (64 - capacity))
    if kind == 3:
        listed = (number * 13) % 65
        if listed < 64:
            orders[6][listed:] = [0] * (64 - listed)
        selected = rng.choice((-3, -2, -1, -1, rng.randrange(64)))
    rows = [[rng.randrange(6), rng.randrange(2), rng.randrange(2), rng.randrange(256),
             rng.choice((180, 181, 204, 205, 320, rng.randrange(-300, 321))),
             rng.randrange(-300, 321), rng.randrange(-300, 321),
             rng.randrange(101), rng.randrange(101)] for _ in range(40)]
    # Input-positive rows must name a real item even when the listing is short.
    if kind == 3 and selected >= 0:
        selected = rng.randrange(listed) if listed else -1
    data = {
        "initialize": init, "menu_step": step if kind == 4 else rng.randrange(14),
        "sort": [step if kind == 0 else rng.randrange(2), category],
        "discard": [step if kind == 1 else rng.randrange(8), category, discard_slot,
                    discard_kind, rng.randrange(3), rng.randrange(101), rng.randrange(101)],
        "presentation": [0xFA, 0x30, *rng.choices(range(10), k=3), 0xA1,
                         *rng.choices(range(10), k=3), 0xE7, 0, 0, rng.randrange(256)],
        "navigation": [0, *actions, *([0] * (7 - len(actions))),
                       step if kind == 2 else rng.randrange(13),
                       1 if number % 9 == 0 else 0, rng.randrange(2), chosen],
        "shortcut": [*rng.randbytes(64), selected + 1,
                     step if kind == 3 else rng.randrange(5), number % 9 == 0, rng.randrange(2)],
        "screen": [rng.randrange(2), rng.choice((0, 0, 1, 10, rng.randrange(256)))],
        "runtime": runtime, "buttons": [rng.getrandbits(16) if number % 4 else 0,
                                       rng.getrandbits(16), rng.getrandbits(16)],
        "ready": ready,
        "services": [rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(2),
                     rng.randrange(2), rng.choice((2, 3, 4)), action_result, selected,
                     (rng.randrange(20, 40) << 8) | rng.randrange(16),
                     rng.choice((-2, -1, 0, 1)), rng.choice((-2, -1, 0, 1, 2)),
                     rng.choice((-2, -1, 0, 1, 2)),
                     rng.choice((-2, -1, 0, 1, 2, 3)), rng.choice((-2, -1, 0, 1)),
                     rng.randrange(9), rng.randrange(9), rng.randrange(9),
                     count if number % 10 else 0, rng.getrandbits(32),
                     rng.choice((0, 0, 1)), rng.randrange(65), number % 5 == 0],
        "animation": list(rng.randbytes(11)), "inventory": inventory, "orders": orders,
        "misc_actions": list(rng.randbytes(64)),
        "map": [rng.choice((0, 105, 106, 107, 130, 133, 134)) for _ in range(134)],
        "unlocked": rng.choices((0, 1), k=256),
        "action_flags": [rng.randrange(4) for _ in range(256)],
        "formatted_flags": [s32(rng.getrandbits(32)) for _ in range(64)], "rows": rows,
    }
    if kind == 2 and step == 2 and selected >= 0:
        data["services"][17] = max(count, selected + 1)
    # Arrival at rearrangement is only legal for an item category.
    if kind == 2 and step == 6:
        data["services"][7] = rng.choice((-3, -2, -1, chosen - 1, count - 1))
    if kind == 3:
        data["runtime"][0] = 7
        data["shortcut"][66] = (listed == 0)
    if kind == 1 and number % 4 == 0:
        choice = (number // 4) % 7
        data["initialize"] = 0
        data["discard"][:4] = [(0, 2, 4, 5, 6, 7, 7)[choice], 6, discard_slot, 1]
        data["ready"] = [1] * 8
        data["services"][0] = 1
        data["services"][6] = 0
        data["buttons"] = [0, rng.choice((0, 16, 32, 64, 128)), 0]
        count = inventory[0xE02 + (discard_slot - 1) * 4]
        data["discard"][5] = count if choice == 5 else max(0, count - 1)
        data["discard"][6] = count
    if kind == 2 and number % 3 == 0:
        choice = (number // 3) % 10
        data["initialize"] = 0
        data["ready"] = [1] * 8
        data["buttons"] = [0, 0, 0]
        data["runtime"][37] = 0
        data["navigation"][9] = 0
        data["services"][7] = -1
        if choice == 0:
            data["runtime"][0] = 0
            data["navigation"][8] = 0
            data["services"][4:6] = [0, 3]
        elif choice == 1:
            data["runtime"][0] = 1
            data["runtime"][37] = 4
            data["navigation"][8] = 1
            data["services"][17] = 8
            data["formatted_flags"] = [value & ~1 for value in data["formatted_flags"]]
        elif choice in (2, 3):
            data["runtime"][0] = 2
            data["navigation"][8] = 2
            data["services"][17] = 16
            if choice == 2:
                data["buttons"][0] = 128
            else:
                data["services"][7] = number % 16
                data["formatted_flags"] = [value & ~8 for value in data["formatted_flags"]]
        elif choice in (4, 5):
            data["runtime"][0] = 7
            data["navigation"][8] = 6
            data["navigation"][11] = 1
            data["services"][7] = 1
            for slot in range(64):
                data["inventory"][0xE00 + slot * 4:0xE02 + slot * 4] = [67, 1]
                data["inventory"][0xE02 + slot * 4] = rng.randrange(
                    1 if choice == 4 else 60, 41 if choice == 4 else 101)
        elif choice in (6, 7, 8):
            data["runtime"][0] = 7
            data["navigation"][8] = 9 if choice == 8 else 8
            data["navigation"][:8] = [0, 1, 3, 4, 5, 0, 0, 0]
            data["services"][14:16] = [0, 0]
            data["services"][6] = 0
            data["action_flags"] = [0] * 256
            for slot in range(64):
                value = 458 if choice == 7 else 323
                data["inventory"][0xE00 + slot * 4:0xE02 + slot * 4] = [
                    value & 255, value >> 8]
        else:
            data["runtime"][0] = 0
            data["navigation"][8] = 1
            data["services"][4:6] = [1, 4]
    return data


def inputs(kind, overlay, item):
    values = [kind, int(overlay == "menub"), item["initialize"], item["menu_step"],
            *item["sort"], *item["discard"], *item["presentation"], *item["navigation"],
            *item["shortcut"], *item["screen"], *item["runtime"], *item["buttons"],
            *item["ready"], *item["services"], *item["animation"], *item["inventory"],
            *flatten(item["orders"]), *item["misc_actions"], *item["map"],
            *item["unlocked"], *item["action_flags"], *item["formatted_flags"],
            *flatten(item["rows"])]
    return [int(value) for value in values]


def put_fields(machine, address, values, width=1):
    for index, value in enumerate(values):
        machine.put(address + index * width, int(value), width)


def prepare_runtime(machine, item, layout):
    values = item["runtime"]
    put_fields(machine, CURSOR, values[:18])
    put_fields(machine, layout["panels"], values[18:25])
    machine.put(layout["preview"], values[25])
    put_fields(machine, CONTROL, values[26:30])
    put_fields(machine, USE, values[30:34], 2)
    for address, value in ((INVOKED, values[34]), (SETTINGS, values[35]), (FREEZE, values[36])):
        machine.put(address, value, 1)
    machine.put(RESTRICTIONS, values[37])
    machine.put(HUD, values[38])


def runtime_values(machine, layout):
    return [*machine.bytes(CURSOR, 18), *machine.bytes(layout["panels"], 7),
            s32(machine.load(layout["preview"])), *machine.bytes(CONTROL, 4),
            machine.load(USE, 2, True), machine.load(USE + 2, 2, True),
            machine.load(USE + 4, 2), machine.load(USE + 6, 2),
            machine.load(INVOKED, 1), machine.load(SETTINGS, 1), machine.load(FREEZE, 1),
            s32(machine.load(RESTRICTIONS)), s32(machine.load(HUD))]


def prepare_statics(machine, item, layout):
    put_fields(machine, layout["sort"], item["sort"])
    put_fields(machine, layout["discard"], item["discard"])
    put_fields(machine, layout["template"], item["presentation"][:12])
    machine.put(layout["brightness"], item["presentation"][12], 1)
    put_fields(machine, layout["nav"], item["navigation"][:11])
    machine.put(layout["nav"] + 12, item["navigation"][11])
    put_fields(machine, SHORTCUT_ACTIONS, item["shortcut"][:64])
    machine.put(SHORTCUT_SELECT, item["shortcut"][64])
    put_fields(machine, SHORTCUT_STEP, item["shortcut"][65:])
    machine.put(SCREEN_RESUME, item["screen"][0], 1)
    machine.put(SCREEN_COUNTDOWN, item["screen"][1], 1)
    machine.put(STEP_PTR, item["menu_step"], 1)


def static_values(machine, layout):
    return [machine.load(STEP_PTR, 1), *machine.bytes(layout["sort"], 2),
            *machine.bytes(layout["discard"], 7), *machine.bytes(layout["template"], 12),
            machine.load(layout["brightness"], 1),
            *machine.bytes(layout["nav"], 11), s32(machine.load(layout["nav"] + 12)),
            *machine.bytes(SHORTCUT_ACTIONS, 64), s32(machine.load(SHORTCUT_SELECT)),
            *machine.bytes(SHORTCUT_STEP, 3), machine.load(SCREEN_RESUME, 1),
            machine.load(SCREEN_COUNTDOWN, 1)]


def text_tokens(machine, layout):
    """Normalize actual source text references; no text bytes or disc data are emitted."""
    tokens = {}

    def direct(kind, address, index=0):
        tokens[address] = kind * 256 + index

    def help_entry(kind, entry, index=0):
        offset = 0x4000 + entry * 2
        machine.put(HELP + entry * 2, offset, 2)
        direct(kind, HELP + offset * 2, index)

    for stat in range(57):
        help_entry(0, 438 + stat * 2, stat)
        help_entry(1, 439 + stat * 2, stat)
    for index in range(2):
        help_entry(2, 552 + index, index)
    for index in range(3):
        help_entry(3, 554 + index, index)
    direct(4, HELP + 0x5C92, 1)
    direct(5, HELP + 0x67C6)
    for index in range(4):
        entry = layout["manage_index"] + index
        direct(6, layout["menu"] + machine.load(layout["menu"] + entry * 2, 2) * 2, index)
    if layout["tarnished"]:
        direct(7, layout["tarnished"])
        direct(11, layout["mp"])
    if layout["prohibited"]:
        direct(12, layout["prohibited"])
    for kind, offset in ((8, 0x6A3A), (9, 0x6A40), (10, 0x6BA2),
                         (13, 0x6A4E), (14, 0x6A58), (15, 0x6B64),
                         (16, 0x6A94), (17, 0x6A9E), (19, 0x6ADA),
                         (20, 0x6AE8), (21, 0x6A1C), (22, 0x6B0C), (23, 0x6B14)):
        direct(kind, HELP + offset)
    for restriction in range(1, 4):
        help_entry(18, 663 + restriction, restriction)
    machine.put(0x800EB9DC + 6, 32, 2)
    direct(24, 0x800EB9DC + 64)
    direct(25, 0x80102060)
    for index in range(8):
        direct(26, layout["menu"] + machine.load(layout["menu"] + index * 2, 2) * 2, index)
    direct(-1, layout["template"])
    tokens[layout["template"]] = -1
    for row in range(64):
        tokens[FORMATTED + row * 2] = 100000 + row * 2
        tokens[FORMATTED + row * 2 + 1] = 100000 + row * 2 + 1
    return tokens


def setup(machine, item, layout):
    prepare_runtime(machine, item, layout)
    prepare_statics(machine, item, layout)
    machine.map_zero(HEAP, 0x2000)
    machine.store(HEAP, bytes([0xA6]) * 0x2000)
    machine.map_zero(HELP, 0x10000)
    machine.put(HELP_PTR, HELP)
    machine.put(NAMES_PTR, HELP if item["services"][1] else 0)
    machine.put(PRESSED, item["buttons"][0])
    machine.put(REPEAT, item["buttons"][1])
    machine.put(HELD, item["buttons"][2])
    machine.put(INFO_DONE, item["services"][0] << 30)
    machine.put(INTRO, item["services"][5], 1)
    machine.put(0x800F19FC, CHAR)
    machine.put(CHAR + 0x3C, STATS)
    machine.put(STATS + 0x948, item["services"][4] << 19)
    machine.put(ROW_TYPES_PTR, ROW_TYPES)
    machine.map_zero(ROW_TYPES, 256)
    machine.store(INVENTORY, bytes(item["inventory"]))
    for category, order in enumerate(item["orders"]):
        put_fields(machine, ORDER + category * 64, order)
        machine.put(ORDER_PTRS + category * 4, ORDER + category * 64)
    machine.put(ORDER_PTRS + 28, ORDER)
    put_fields(machine, MISC_ORDER, item["orders"][6])
    put_fields(machine, 0x8010971C, item["misc_actions"])
    put_fields(machine, MAP, item["map"])
    machine.map_zero(ACTIONS, 256 * 52)
    for action, unlocked in enumerate(item["unlocked"]):
        machine.put(ACTIONS + action * 52 + 12, unlocked << 15)
    put_fields(machine, CAPACITY_TABLE, CAPACITIES)
    put_fields(machine, ANIMATION, item["animation"])
    machine.put(FADE, item["services"][19])
    for row, fields in enumerate(item["rows"]):
        for (offset, width, _), value in zip(ROW_FIELDS, fields, strict=True):
            machine.put(ROWS + row * 0x40 + offset, value, width)
    machine.put(0x1F800008, ROW_TYPES + 12)


def run(kind, overlay, machine, item):
    layout = LAYOUT[overlay]
    setup(machine, item, layout)
    tokens = text_tokens(machine, layout)
    calls, problems = [], []
    ready_position, workspace = [0], [False]
    services = item["services"]

    def record(op, *values):
        calls.extend((op, len(values), *values))

    def token(address):
        if address not in tokens:
            raise Fault(f"unknown text reference {address:08X}")
        return tokens[address]

    def fixed(address, op, argument_count=0, result=0):
        def callback(m):
            record(op, *(s32(m.r[4 + index]) for index in range(argument_count)))
            m.r[2] = result & 0xFFFFFFFF
        machine.stub(address, callback)

    def ready(m):
        record(READY)
        m.r[2] = item["ready"][min(ready_position[0], 7)]
        ready_position[0] += 1

    def allocate(m):
        expected = 0x1800 if overlay == "menub" else 0x1B00
        if workspace[0] or m.r[4] != expected:
            problems.append(f"row workspace acquisition {m.r[4]:X}, active={workspace[0]}")
        workspace[0] = True
        record(BEGIN_ROWS)
        m.r[2] = HEAP

    def free(m):
        if not workspace[0] or m.r[4] != HEAP:
            problems.append("row workspace release without its acquired owner")
        workspace[0] = False
        record(END_ROWS)

    def create_rows(op):
        def callback(m):
            count, menu, texts, flags = m.r[4:8]
            if count > 64:
                raise Fault("row count exceeded caller buffers")
            record(op, menu, count, m.load(SETTINGS, 1))
            for row in range(count):
                calls.extend((token(m.load(texts + row * 8)),
                              token(m.load(texts + row * 8 + 4)),
                              s32(m.load(flags + row * 4))))
        return callback

    def format_into(m, row, texts, flags):
        m.put(texts, FORMATTED + row * 2)
        m.put(texts + 4, FORMATTED + row * 2 + 1)
        m.put(flags, item["formatted_flags"][row])

    def populate(m):
        record(POPULATE, int(overlay == "menub"), m.r[4])
        count = services[17]
        for row in range(count):
            format_into(m, row, m.r[5] + row * 8, m.r[6] + row * 4)
        m.r[2] = count

    def format_misc(m):
        slot = (m.r[4] - INVENTORY - 0xE00) // 4
        row = (m.r[7] - HEAP) // 96
        if not 0 <= slot < 64 or not 0 <= row < 64:
            raise Fault("misc formatter exceeded a real record or row")
        record(FORMAT_MISC, slot, row)
        format_into(m, row, m.r[5], m.r[6])

    def row(m):
        if m.r[4] >= 40:
            raise Fault("row lookup exceeded table")
        record(GET_ROW, m.r[4])
        m.r[2] = ROWS + m.r[4] * 0x40

    def set_row(m):
        index, x, y, width = m.r[4:8]
        background = m.load(m.r[29] + 16)
        text = m.load(m.r[29] + 20)
        text_token = token(text)
        record(SET_ROW, index, s32(x), s32(y), width, background, text_token)
        if text_token == -1:
            calls.extend(m.bytes(text, 12))
        m.r[2] = ROWS + index * 0x40
        m.put(m.r[2] + 20, x, 2)
        m.put(m.r[2] + 22, y, 2)

    def sound(sound_id):
        def callback(m):
            record(SOUND, sound_id)
        return callback

    def info(m):
        record(INFORMATION, token(m.r[4]))

    def toggle(m):
        put_fields(m, layout["panels"], [m.r[4]] * 3)

    def memset(m):
        m.store(m.r[4], bytes([m.r[5] & 255]) * m.r[6])

    def memzero(m):
        m.store(m.r[4], bytes(m.r[5]))

    def action_flags(m):
        if m.r[4] != 0:
            problems.append("action flags actor was not Ashley")
        record(ACTION_FLAGS, m.r[5])
        m.r[2] = item["action_flags"][m.r[5]]

    def details(m):
        packed = m.r[4]
        record(DETAILS, (packed & 15) if packed else -1, (packed >> 4) - 1 if packed else 0)
        m.r[2] = services[9] & 0xFFFFFFFF

    def discard(m):
        packed = m.r[4]
        record(DISCARD, (packed >> 8) & 255 if packed else -1,
               packed & 255 if packed else 0, (packed >> 16) & 255 if packed else 0)
        m.r[2] = services[10] & 0xFFFFFFFF

    def sort_menu(m):
        record(SORT_MENU, m.r[4] - 1 if m.r[4] else -1)
        m.r[2] = services[11] & 0xFFFFFFFF

    def navigation(m):
        record(NAVIGATION, m.r[4])
        if services[21]:
            m.put(layout["panels"] + 2, 2, 1)
        m.r[2] = services[12] & 0xFFFFFFFF

    def digit(m):
        record(DIGIT, m.r[4], s32(m.r[5]))
        if m.r[6] != 130 or m.r[7] != ROW_TYPES:
            problems.append("quantity glyph y/foreground target differs")

    def font(m):
        record(FONT)
        if m.r[4] != 5:
            problems.append("quantity font selector differs")

    def decimal(m):
        value = m.r[4]
        m.r[2] = (value // 10) * 16 + value % 10

    def cursor(m):
        record(CURSOR_DRAW, m.r[4], m.r[5] & 0xFFFF, m.r[5] >> 16)
        m.r[2] = services[18] & 0xFFFFFFFF

    def sort_call(category, equipment=False):
        def callback(m):
            record(SORT, m.r[4] if equipment else category, m.r[5] if equipment else m.r[4])
        return callback

    def action_information(m):
        record(ACTION_INFORMATION)

    pending_status = []

    def status_background(m):
        pending_status[:] = [s32(16 - m.r[4])]
        if m.r[5:8] != [38, 88, 10]:
            problems.append("status background extent differs")

    def status_button(m):
        if not pending_status or m.r[4] != 1 or s32(m.r[5]) != 8 - pending_status[0] \
                or m.r[6:8] != [36, 0]:
            problems.append("status button request differs")
        pending_status.append(1)

    def status_text(m):
        offset = pending_status[0] if pending_status else 0
        status = m.load(layout["panels"] + 2, 1)
        expected_xy = ((28 - offset) & 65535) | (38 << 16)
        if len(pending_status) != 2 or m.r[4] != 0x80102938 or m.r[5] != expected_xy \
                or m.r[6] != (0x202020 << (status & 31)) & 0xFFFFFFFF or m.r[7] != 0:
            problems.append("status text/color/button sequence differs")
        record(STATUS, offset, status)

    machine.stub(0x800FA9D0, ready)
    machine.stub(0x80043E3C, allocate)
    machine.stub(0x80043C60, free)
    machine.stub(0x800FF0EC, create_rows(CREATE_ACTIONS))
    machine.stub(0x801005E0, create_rows(CREATE_LIST))
    machine.stub(0x800C8E48, row)
    machine.stub(0x800C8E5C, set_row)
    machine.stub(0x800FFC04, info)
    machine.stub(0x800FD17C, format_misc)
    machine.stub(0x80026EB4, memset)
    machine.stub(0x800CCC1C, memzero)
    machine.stub(0x8008A5D0, action_flags)
    machine.stub(0x800CCC54, decimal)
    machine.stub(0x800C70F8, digit)
    machine.stub(0x800C7210, font)
    machine.stub(0x800FFCDC, cursor)
    for address, sound_id in ((0x800C02F0, 0), (0x800C02E8, 1), (0x800C02F8, 2),
                              (0x800C02E0, 3), (0x80045988, 4)):
        machine.stub(address, sound(sound_id))
    for address, op, argc, result in (
        (0x800FFBC8, INIT_INFO, 0, 0), (0x800C8E04, DISCARD_INFO, 0, 0),
        (0x800FFBA8, DISMISS_INFO, 0, 0), (0x800FF43C, UPDATE_ACTIONS, 0, 0),
        (0x800FF360, ACTION_RESULT, 0, services[6]), (0x800FA8E0, CLEAR, 1, 0),
        (0x800FA854, FLY_OUT, 1, 0),
        (0x800FE3E0, CLEAR_ITEM, 2, 0), (0x800FE8B0, REBUILD, 1, 0),
        (0x800FE7E0, FIND, 2, services[16]), (0x801008C8, SELECTED, 0, services[7]),
        (0x80100814, STORE_CURSOR, 0, services[8]), (0x800FE5CC, LOAD_TEXT, 1, services[2]),
        (0x800FE694, UNLOAD_TEXT, 0, services[3]), (0x80101F38, RENDER_SCREEN, 0, 0),
        (0x800FBD80, BASE_STATS, 1, 0), (0x8008A4DC, ACTION_STATE, 1, 0),
        (0x800CB654, HUD_STATE, 1, 0), (0x80108088, EQUIPMENT, 1, services[13]),
        (0x80100414, BACKGROUND_PRIORITY, 2, 0), (0x800FFB68, CLOSE_BACKGROUND, 0, 0),
        (0x800FFA88, CLEAR_COMMAND, 0, 0), (0x800FEA6C, COUNT, 1, services[20]),
        (0x80102A3C, ICONS, 2, 0), (0x80108518, ASHLEY, 1, 0)):
        fixed(address, op, argc, result)
    fixed(0x800FF348, 999, 0, 0)
    # The getter feeds the real information call; only the combined typed endpoint is logged.
    machine.stub(0x800FF348, lambda m: m.r.__setitem__(2, 0))
    machine.put(0x801023A3, 0, 1)
    machine.stub(0x800FF9E4, action_information)
    machine.stub(0x80100344, status_background)
    machine.stub(0x800FFC68, status_button)
    machine.stub(0x800C6540, status_text)
    if overlay == "menu3":
        addresses = (0x8010296C, 0x80104F94, 0x80105314, 0x80104EC0, 0x80104F50, 0x80103AD0)
        sorts = (0x80103CC8, 0x80103F00, 0x8010408C, 0x8010439C)
    else:
        addresses = (0x80104F98, 0x80105088, 0x8010537C, 0x80104FB4, 0x80105044, 0x80103BA8)
        sorts = (0x80103DA0, 0x80103FD8, 0x80104164, 0x80104474)
    machine.stub(addresses[0], toggle)
    machine.stub(addresses[1], populate)
    fixed(addresses[2], RESTORE, 1)
    fixed(addresses[3], PARENT_WEAPON, 2, services[14])
    fixed(addresses[4], PARENT_SHIELD, 2, services[15])
    machine.stub(addresses[5], details)
    for address, category in zip(sorts[:3], (0, 3, 6), strict=True):
        machine.stub(address, sort_call(category))
    machine.stub(sorts[3], sort_call(0, True))
    if kind != 0:
        machine.stub(RANGES[overlay][0][0], sort_menu)
    if kind != 1:
        machine.stub(RANGES[overlay][1][0], discard)
    if kind != 2:
        machine.stub(RANGES[overlay][2][0], navigation)
    else:
        machine.follow.add(RANGES[overlay][2][0])
    entry = RANGES[overlay][kind][0]
    machine.r[4] = STEP_PTR if kind == 4 else item["initialize"] & 0xFFFFFFFF
    machine.execute(entry)
    result = 0 if kind == 3 else s32(machine.r[2])
    row_values = [machine.load(ROWS + row * 0x40 + offset, width, signed)
                  for row in range(40) for offset, width, signed in ROW_FIELDS]
    values = [result, *static_values(machine, layout), *runtime_values(machine, layout),
              *machine.bytes(INVENTORY, INV_BYTES), *machine.bytes(ORDER, 7 * 64),
              *machine.bytes(0x8010971C, 64), *row_values, workspace[0], len(calls), *calls]
    allowed = [(DEFAULT_STACK - 0x2000, DEFAULT_STACK + 16),
               (HEAP, HEAP + 0x2000), (ROW_TYPES, ROW_TYPES + 256),
               (INVENTORY, INVENTORY + INV_BYTES), (ORDER, ORDER + 7 * 64),
               (CURSOR, CURSOR + 18), (layout["panels"], layout["panels"] + 7),
               (layout["preview"], layout["preview"] + 4), (CONTROL, CONTROL + 4),
               (USE, USE + 8), (INVOKED, INVOKED + 1), (HUD, HUD + 4),
               (RESTRICTIONS, RESTRICTIONS + 4), (FREEZE, FREEZE + 1),
               (SETTINGS, SETTINGS + 1), (ROWS, ROWS + 40 * 0x40),
               (layout["sort"], layout["sort"] + 2),
               (layout["discard"], layout["discard"] + 7),
               (layout["template"], layout["template"] + 12),
               (layout["brightness"], layout["brightness"] + 1),
               (layout["nav"], layout["nav"] + 16),
               (SHORTCUT_ACTIONS, SHORTCUT_STEP + 3),
               (SCREEN_RESUME, SCREEN_RESUME + 1),
               (SCREEN_COUNTDOWN, SCREEN_COUNTDOWN + 1), (STEP_PTR, STEP_PTR + 1)]
    return {"values": values, "stores": machine.stores, "allowed": allowed,
            "problems": problems}, inputs(kind, overlay, item)


def check(image, item, observation, native, reference):
    problems = list(observation["problems"])
    if native != observation["values"]:
        first = next((index for index, (left, right) in enumerate(
            zip(native, observation["values"], strict=False)) if left != right), None)
        problems.append(f"state/request differs at {first}; lengths {len(native)}/{len(observation['values'])}; "
                        f"native={native[first:first+12] if first is not None else []}, "
                        f"original={observation['values'][first:first+12] if first is not None else []}")
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(kind, overlay, image, results):
    entry, end = RANGES[overlay][kind]
    body = bytes(image.read(address, 1) for address in range(entry, end))
    decoder = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_LITTLE_ENDIAN)
    instructions = list(decoder.disasm(body, entry))
    if len(instructions) != (end - entry) // 4 or instructions[0].mnemonic != "addiu" \
            or image.read(end - 8) != 0x03E00008:
        raise AssertionError("terminal jr ra no longer agrees with the verified disassembly")
    module = "MAINMENU.PRG" if kind == 3 else f"{overlay.upper()}.PRG"
    return [
        f"Actual module-relative {module} span +{entry-image.base:X}..+{end-image.base:X}; "
        "Capstone-verified boundaries, original tables and instructions, full controller state.",
        "Every state, initialization/reentry, input cancellations, list requests, item effects, "
        "cleanup and presentation calls compare; unrelated inventory/order/row bytes must remain unchanged.",
        "Callees are endpoint doubles, not controller models. Workspace pointer/unused padding and "
        "rendering packet internals are outside comparison. Native refusal tests cover invalid borrows.",
    ]
