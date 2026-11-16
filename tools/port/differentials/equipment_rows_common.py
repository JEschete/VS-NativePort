"""Shared harness for the equipment row and preview model differentials (MENU3.PRG 16C.c:39-336 and
68C4.c:12-127, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::Inventory core
(EquipmentRows.cpp) in equipment_rows_driver.cpp. The list's globals, its item text records, the menu
rows 20-25 and the accessory, actor and weapon model fixtures live at fixed addresses; every callee is
stubbed and recorded. Both sides print a refusal marker, the observed globals and fixture fields and the
recorded calls.

The typed cores refuse an empty or oversized list, a scroll position or selection outside the list and an
armor slot outside the table, where the original reads or writes past them; vectors stay inside
(EquipmentRowsTests.cpp covers the refusals).
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MODULE_BASES,
    Machine,
    mode_case,
    recorded_observation,
    s32,
)

MODULE_BASES["MENU/MENU3.PRG"] = 0x80102800

MODULES = ("MENU/MENU3.PRG",)
DRIVER = "equipment_rows_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\Inventory\EquipmentRows.cpp",)
INCLUDES = (r"Native\Content\include",)

CREATE, SELECTED, UPDATE, ACCESSORY, RELOAD, TWO_HANDED, QUEUE = range(7)

# 16C.c's globals and _processEquipSubMenu's cursor static.
ITEMS_TEXT = 0x8010975C
SELECTED_ROW = 0x80109760
STEP = 0x80109764
ROW_COUNT = 0x80109765
FIRST_ROW = 0x80109766
EQUIPPED = 0x80109767
CURSOR = 0x80109620
BUTTONS = 0x8005E1D0  # vs_main_buttonsPressed
REPEAT = 0x8005DFDC  # vs_main_buttonRepeat
# _populateUiAccessory's tables (MAINMENU.PRG data).
ARMOR_POINTER = 0x80102468  # vs_menuD_armor
STATS = 0x801024C0  # vs_mainMenu_equipmentStats: classes, affinities, types
STATUS_ITEM = 0x801024A1
SUBTYPE = 0x80102545
MAINMENU_DATA = 0x80102400
MAINMENU_DATA_BYTES = 0x200
# 68C4.c's globals.
LOAD_STATE = 0x800E8F28  # vs_battle_loadObjectDataState
ACTOR_POINTER = 0x800F453C  # vs_battle_actors[1]
WEAPON_MODELS = 0x800F4588  # D_800F4588[0..3]
CHARACTER_STATE = 0x800F19FC  # vs_battle_characterState

TEXT = 0x80500000
RECORD_BYTES = 128
ROWS = 0x80510000
ROW_BYTES = 0x40
ROW_SLOTS = 64
POINTERS = 0x80520000
TYPES = 0x80521000
NAMES = 0x80530000
DESCRIPTIONS = 0x80534000
STRING_STRIDE = 0x80
ARMOR = 0x80540000
ARMOR_BYTES = 40
ACTOR = 0x80550000
ACTOR_BYTES = 0x1300
WEAPON2 = 0x80560000
WEAPON3 = 0x80560100
CHARACTER = 0x80570000

ALLOC = 0x80043E3C
FREE = 0x80043C60
SET_MENU_ITEM = 0x800C8E5C
GET_MENU_ITEM = 0x800C8E48
INFO = 0x800FFC04
ROWS_IDLE = 0x800FA9D0
LEAVE = 0x800C02E8
INVALID = 0x800C02E0
CHANGE = 0x800C02F8
SFX = 0x80045988
DRAW_CURSOR = 0x800FFCDC
RESET_STATS = 0x800FD220
APPLY_ACCESSORY = 0x8006B194
PRIMARY_STATS = 0x800FBD28
STAT_PAGES = 0x800FBB8C
RELEASE_MODEL = 0x8009AA84
QUEUE_ANIMATIONS = 0x800FA3FC
WEAPON_DRAWN = 0x8009D934
QUEUE_BUSY = 0x8009967C
RESET_EQUIP_POSE = 0x800FA2CC
RELEASE_PREVIEW = 0x800F9E0C
CLONE_PREVIEW = 0x800F9A78
RESET_PREVIEW_POSE = 0x800F9CB0
PLAY_ANIMATION = 0x800A0204
ADVANCE_SKELETON = 0x800AFA28
MEMCPY = 0x800490B0
QUEUE_ASSET = 0x800995E8
TWO_HANDED_ENTRY = 0x80109320
QUEUE_ENTRY = 0x80109344

(CALL_ALLOC, CALL_FREE, CALL_SET_ROW, CALL_GET_ROW, CALL_INFO, CALL_IDLE, CALL_LEAVE, CALL_INVALID,
 CALL_CHANGE, CALL_SFX, CALL_CURSOR) = range(1, 12)
CALL_RESET_STATS, CALL_APPLY, CALL_PRIMARY, CALL_PAGES = range(20, 24)
(CALL_RELEASE, CALL_ANIMATIONS, CALL_DRAWN, CALL_BUSY, CALL_RESET_EQUIP, CALL_RELEASE_PREVIEW, CALL_CLONE,
 CALL_RESET_PREVIEW, CALL_PLAY, CALL_ADVANCE, CALL_COPY, CALL_QUEUE) = range(30, 42)

NAME_BYTES = 26
DESCRIPTION_BYTES = 92
VISIBLE = 6
FIRST_ROW_INDEX = 20
# The observed MenuRow fields: (offset, size, signed).
ROW_FIELDS = ((0, 1, False), (4, 1, False), (5, 1, False), (6, 1, False), (7, 1, False), (9, 1, False),
              (0xC, 1, False), (0xD, 1, False), (0x18, 2, True))
STAT_COLUMN = 16
SEQUENCE_MODEL = 0x5B1
ANIMATION = 0x5B8
FLAGS_WORD = 8
WEAPON_FLAG = 0x11


def count_of(rng: random.Random) -> int:
    return rng.choice((1, 2, 5, 6, 7, 8, rng.randrange(1, 16), rng.randrange(1, 16), rng.randrange(1, 65)))


def row_flags(rng: random.Random) -> int:
    flags = rng.getrandbits(32)
    if rng.randrange(3) == 0:
        flags &= ~(0x7F << 9)
    if rng.randrange(3) == 0:
        flags &= ~1
    return s32(flags)


def row_presets(rng: random.Random) -> list[list[int]]:
    rows = []
    for _ in range(VISIBLE):
        fields = [rng.randrange(256) for _ in range(len(ROW_FIELDS) - 1)]
        fields[4] = rng.choice((0, 0, 1, rng.randrange(256)))
        rows.append([*fields, rng.randrange(-0x8000, 0x8000)])
    return rows


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode}
    if mode == CREATE:
        count = count_of(rng)
        value["count"] = count
        value["rows"] = [{
            "name": [rng.randrange(256) for _ in range(NAME_BYTES)] if rng.randrange(4) else None,
            "description": [rng.randrange(256) for _ in range(DESCRIPTION_BYTES)] if rng.randrange(4) else None,
            "flags": row_flags(rng),
        } for _ in range(count)]
        value["presets"] = row_presets(rng)
        value["globals"] = [s32(rng.getrandbits(32)), rng.randrange(256), rng.randrange(256)]
    elif mode == SELECTED:
        value["text"] = rng.randrange(2)
        value["selected"] = rng.choice((0, 1, 5, -1, -2, -3, rng.randrange(64), s32(rng.getrandbits(32))))
    elif mode == UPDATE:
        count = count_of(rng)
        last_first = max(0, count - VISIBLE)
        first = rng.choice((0, last_first, rng.randrange(last_first + 1)))
        selected = rng.randrange(min(VISIBLE, count - first))
        if rng.randrange(4) == 0:
            selected = rng.choice((0, 1, 2, 4, 5, min(VISIBLE, count - first) - 1))
            selected = min(selected, min(VISIBLE, count - first) - 1)
        step = rng.choice((17, 17, 17, 17, 16, 16, rng.randrange(min(5, count) + 1),
                           rng.choice((6, 7, 15, 18, 0x80, 0xFF))))
        pressed = 0
        for bit, odds in ((0x10, 6), (0x20, 4), (0x40, 6)):
            if rng.randrange(odds) == 0:
                pressed |= bit
        repeat = 0
        for bit in (0x1000, 0x4000):
            if rng.randrange(3) == 0:
                repeat |= bit
        if rng.randrange(4) == 0:
            pressed |= rng.getrandbits(32) & ~0x70
            repeat |= rng.getrandbits(32) & ~0x5000
        shape = rng.randrange(8)
        if shape < 2:
            # A long list's cursor at a scrolling row, moved without a button.
            count = rng.choice((7, 8, rng.randrange(7, 65)))
            first = rng.choice((0, 1, count - VISIBLE, rng.randrange(count - VISIBLE + 1)))
            selected = rng.choice([kind for kind in (1, 2, 3, 4, 5) if kind + first < count])
            step, pressed, repeat = 17, 0, rng.choice((0x1000, 0x4000))
        elif shape == 2:
            # The last slide step of a list one row longer than the screen or shorter.
            count = rng.choice((5, 6, 7, 7, 8))
            first, selected, step = 0, 0, min(5, count)
        value.update({
            "count": count, "first": first, "selected": selected, "step": step,
            "equipped": rng.randrange(256), "cursor": rng.randrange(256), "pressed": pressed, "repeat": repeat,
            "has_sfx": rng.choice((0, 1, 2, 3)), "idle": rng.randrange(2),
            "cursor_return": s32(rng.getrandbits(32)),
            "flags": [row_flags(rng) for _ in range(count)],
            "presets": row_presets(rng),
        })
    elif mode == ACCESSORY:
        armor = rng.randrange(1, 40)
        value.update({
            "armor": armor,
            "id": rng.choice((0, 0, 1, armor, rng.randrange(armor + 1))),
            "primary": [rng.randrange(-0x8000, 0x8000) for _ in range(3)],
            "types": [rng.randrange(0x10000) for _ in range(4)],
            "classes": [rng.randrange(0x10000) for _ in range(8)],
            "affinities": [rng.randrange(0x10000) for _ in range(8)],
            "columns": [rng.randrange(-0x8000, 0x8000) for _ in range(3 * STAT_COLUMN)],
            "subtype": rng.randrange(256),
            "status": rng.randrange(256),
        })
    elif mode in (RELOAD, QUEUE, TWO_HANDED):
        value.update({
            "step": rng.choice((0, 0, 0, 1, 1, 2, 2, 3, 3, rng.randrange(4, 256))),
            "category": rng.choice((0, 0, 0, rng.randrange(1, 11), rng.randrange(-2, 13))),
            "model": rng.choice((0, rng.randrange(1, 96), rng.randrange(96, 128), 95, 96,
                                 rng.randrange(256), s32(rng.getrandbits(32)))),
            "material": rng.choice((0, 7, rng.randrange(8), s32(rng.getrandbits(32)))),
            "sequence": rng.choice((0, 3, 8, rng.randrange(11), rng.randrange(256))),
            "animation": rng.choice((1, 1, 101, 101, rng.randrange(256))),
            "flags_word": s32(rng.getrandbits(32)),
            "weapons": [rng.randrange(2), rng.randrange(2)],
            "weapon_flags": [rng.randrange(256), rng.randrange(256)],
            "busy": rng.choice((0, 0, 1, s32(rng.getrandbits(32)))),
            "slots": [s32(rng.getrandbits(32)), s32(rng.getrandbits(32))],
        })
        if mode == TWO_HANDED:
            value["category"] = rng.choice((*range(-3, 12), 0x7FFFFFFF, -0x80000000, s32(rng.getrandbits(32))))
    return value


def put_rows(machine: Machine, presets: list[list[int]]) -> None:
    machine.map_zero(ROWS, ROW_BYTES * ROW_SLOTS)
    for index, fields in enumerate(presets):
        base = ROWS + (FIRST_ROW_INDEX + index) * ROW_BYTES
        for (offset, size, _), item in zip(ROW_FIELDS, fields, strict=True):
            machine.put(base + offset, item, size)


def read_rows(machine: Machine) -> tuple[list[int], list[str]]:
    values, names = [], []
    for index in range(VISIBLE):
        base = ROWS + (FIRST_ROW_INDEX + index) * ROW_BYTES
        for field, (offset, size, signed) in enumerate(ROW_FIELDS):
            values.append(machine.load(base + offset, size, signed))
            names.append(f"row {FIRST_ROW_INDEX + index} field {field}")
    return values, names


def text_offset(address: int) -> int:
    return -1 if address == 0 else s32(address - TEXT)


def list_globals(machine: Machine) -> tuple[list[int], list[str]]:
    values = [text_offset(machine.load(ITEMS_TEXT)), machine.load(SELECTED_ROW, 4, signed=True),
              machine.load(STEP, 1), machine.load(ROW_COUNT, 1), machine.load(FIRST_ROW, 1),
              machine.load(EQUIPPED, 1), machine.load(CURSOR, 1)]
    return values, ["text", "selected", "step", "count", "first", "equipped", "cursor"]


def stub_list(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    def alloc(machine: Machine) -> None:
        calls.append([CALL_ALLOC, s32(machine.r[4])])
        machine.r[2] = TEXT

    def free(machine: Machine) -> None:
        calls.append([CALL_FREE, text_offset(machine.r[4])])

    def row_address(row: int) -> int:
        return ROWS + row * ROW_BYTES if 0 <= row < ROW_SLOTS else ROWS

    def set_row(machine: Machine) -> None:
        sp = machine.r[29]
        calls.append([CALL_SET_ROW, *(s32(machine.r[i]) for i in range(4, 8)), s32(machine.load(sp + 0x10)),
                      text_offset(machine.load(sp + 0x14))])
        machine.r[2] = row_address(s32(machine.r[4]))

    def get_row(machine: Machine) -> None:
        calls.append([CALL_GET_ROW, s32(machine.r[4])])
        machine.r[2] = row_address(s32(machine.r[4]))

    def info(machine: Machine) -> None:
        calls.append([CALL_INFO, text_offset(machine.r[4])])

    def idle(machine: Machine) -> None:
        calls.append([CALL_IDLE])
        machine.r[2] = value.get("idle", 0)

    def sfx(machine: Machine) -> None:
        calls.append([CALL_SFX, s32(machine.r[4]), s32(machine.r[5])])

    def cursor(machine: Machine) -> None:
        calls.append([CALL_CURSOR, s32(machine.r[4]), s32(machine.r[5])])
        machine.r[2] = value.get("cursor_return", 0) & 0xFFFFFFFF

    machine.stub(ALLOC, alloc)
    machine.stub(FREE, free)
    machine.stub(SET_MENU_ITEM, set_row)
    machine.stub(GET_MENU_ITEM, get_row)
    machine.stub(INFO, info)
    machine.stub(ROWS_IDLE, idle)
    machine.stub(LEAVE, lambda machine: calls.append([CALL_LEAVE]))
    machine.stub(INVALID, lambda machine: calls.append([CALL_INVALID]))
    machine.stub(CHANGE, lambda machine: calls.append([CALL_CHANGE]))
    machine.stub(SFX, sfx)
    machine.stub(DRAW_CURSOR, cursor)


def run_create(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    count = value["count"]
    machine.map_zero(TEXT, RECORD_BYTES * count)
    machine.map_zero(POINTERS, 8 * count)
    machine.map_zero(TYPES, 4 * count)
    put_rows(machine, value["presets"])
    selected, first, equipped = value["globals"]
    machine.put(ITEMS_TEXT, 0)
    machine.put(SELECTED_ROW, selected)
    machine.put(FIRST_ROW, first, 1)
    machine.put(EQUIPPED, equipped, 1)
    machine.put(STEP, 0, 1)
    machine.put(ROW_COUNT, 0, 1)
    machine.put(CURSOR, 0, 1)
    inputs = [count, selected, first, equipped]
    for index, row in enumerate(value["rows"]):
        name, description = row["name"], row["description"]
        machine.put(POINTERS + 8 * index, NAMES + STRING_STRIDE * index if name else 0)
        machine.put(POINTERS + 8 * index + 4, DESCRIPTIONS + STRING_STRIDE * index if description else 0)
        for at, item in enumerate(name or ()):
            machine.put(NAMES + STRING_STRIDE * index + at, item, 1)
        for at, item in enumerate(description or ()):
            machine.put(DESCRIPTIONS + STRING_STRIDE * index + at, item, 1)
        machine.put(TYPES + 4 * index, row["flags"])
        inputs += [row["flags"], int(name is not None), *(name or ()), int(description is not None),
                   *(description or ())]
    inputs += [item for fields in value["presets"] for item in fields]
    stub_list(machine, value, calls)
    machine.r[4], machine.r[5], machine.r[6] = count, POINTERS, TYPES
    machine.execute(entry)
    values, names = list_globals(machine)
    for index in range(count):
        for word in range(RECORD_BYTES // 4):
            values.append(machine.load(TEXT + RECORD_BYTES * index + 4 * word))
            names.append(f"record {index} word {word}")
    row_values, row_names = read_rows(machine)
    return [0, *values, *row_values], ["refused", *names, *row_names], inputs


def run_selected(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    machine.put(ITEMS_TEXT, TEXT if value["text"] else 0)
    machine.put(SELECTED_ROW, value["selected"])
    machine.execute(entry)
    return [s32(machine.r[2])], ["result"], [value["text"], value["selected"]]


def run_update(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    count = value["count"]
    machine.map_zero(TEXT, RECORD_BYTES * count)
    for index, flags in enumerate(value["flags"]):
        machine.put(TEXT + RECORD_BYTES * index + 28, flags)
    put_rows(machine, value["presets"])
    machine.put(ITEMS_TEXT, TEXT)
    machine.put(SELECTED_ROW, value["selected"])
    machine.put(STEP, value["step"], 1)
    machine.put(ROW_COUNT, count, 1)
    machine.put(FIRST_ROW, value["first"], 1)
    machine.put(EQUIPPED, value["equipped"], 1)
    machine.put(CURSOR, value["cursor"], 1)
    machine.put(BUTTONS, value["pressed"])
    machine.put(REPEAT, value["repeat"])
    stub_list(machine, value, calls)
    machine.r[4] = value["has_sfx"]
    machine.execute(entry)
    values, names = list_globals(machine)
    row_values, row_names = read_rows(machine)
    inputs = [count, value["first"], value["selected"], value["step"], value["equipped"], value["cursor"],
              s32(value["pressed"]), s32(value["repeat"]), value["has_sfx"], value["idle"],
              value["cursor_return"], *value["flags"], *(item for fields in value["presets"] for item in fields)]
    return [0, *values, *row_values], ["refused", *names, *row_names], inputs


def run_accessory(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    machine.map_zero(MAINMENU_DATA, MAINMENU_DATA_BYTES)
    machine.map_zero(ARMOR, ARMOR_BYTES * value["armor"])
    machine.put(ARMOR_POINTER, ARMOR)
    for index, item in enumerate(value["columns"]):
        machine.put(STATS + 2 * index, item, 2)
    machine.put(SUBTYPE, value["subtype"], 1)
    machine.put(STATUS_ITEM, value["status"], 1)

    def apply(machine: Machine) -> None:
        calls.append([CALL_APPLY, s32(machine.r[5] - ARMOR)])
        local = machine.r[4]
        for index, item in enumerate(value["primary"]):
            machine.put(local + 0x30 + 2 * index, item, 2)
        for base, items in ((0x38, value["types"]), (0x40, value["classes"]), (0x50, value["affinities"])):
            for index, item in enumerate(items):
                machine.put(local + base + 2 * index, item, 2)

    def primary(machine: Machine) -> None:
        calls.append([CALL_PRIMARY, *(s32(machine.r[i]) for i in range(4, 8))])

    machine.stub(RESET_STATS, lambda machine: calls.append([CALL_RESET_STATS]))
    machine.stub(APPLY_ACCESSORY, apply)
    machine.stub(PRIMARY_STATS, primary)
    machine.stub(STAT_PAGES, lambda machine: calls.append([CALL_PAGES, s32(machine.r[4])]))
    machine.r[4] = value["id"]
    machine.execute(entry)
    values = [0, *(machine.load(STATS + 2 * index, 2, signed=True) for index in range(3 * STAT_COLUMN)),
              machine.load(SUBTYPE, 1), machine.load(STATUS_ITEM, 1)]
    names = ["refused", *(f"column {index}" for index in range(3 * STAT_COLUMN)), "subtype", "status item"]
    inputs = [value["armor"], value["id"], *value["primary"], *value["types"], *value["classes"],
              *value["affinities"], *value["columns"], value["subtype"], value["status"]]
    return values, names, inputs


def stub_models(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    def queue_asset(machine: Machine) -> None:
        at = machine.r[4]
        calls.append([CALL_QUEUE, machine.load(at, 1), machine.load(at + 1, 1, signed=True),
                      machine.load(at + 2, 2, signed=True), s32(machine.load(at + 4)), machine.load(at + 0x10, 1),
                      machine.load(at + 0x11, 1), machine.load(at + 0x12, 1)])

    def busy(machine: Machine) -> None:
        calls.append([CALL_BUSY])
        machine.r[2] = value["busy"] & 0xFFFFFFFF

    def actor_offset(address: int) -> int:
        return s32(address - ACTOR)

    machine.stub(RELEASE_MODEL, lambda machine: calls.append([CALL_RELEASE, s32(machine.r[4])]))
    machine.stub(QUEUE_ANIMATIONS, lambda machine: calls.append([CALL_ANIMATIONS, s32(machine.r[4])]))
    machine.stub(WEAPON_DRAWN, lambda machine: calls.append([CALL_DRAWN, *(s32(machine.r[i]) for i in (4, 5, 6))]))
    machine.stub(QUEUE_BUSY, busy)
    machine.stub(RESET_EQUIP_POSE, lambda machine: calls.append([CALL_RESET_EQUIP]))
    machine.stub(RELEASE_PREVIEW, lambda machine: calls.append([CALL_RELEASE_PREVIEW]))
    machine.stub(CLONE_PREVIEW, lambda machine: calls.append([CALL_CLONE, s32(machine.r[4])]))
    machine.stub(RESET_PREVIEW_POSE, lambda machine: calls.append([CALL_RESET_PREVIEW]))
    machine.stub(PLAY_ANIMATION, lambda machine: calls.append([CALL_PLAY, *(s32(machine.r[i]) for i in range(4, 8))]))
    machine.stub(ADVANCE_SKELETON, lambda machine: calls.append(
        [CALL_ADVANCE, actor_offset(machine.r[4]), actor_offset(machine.r[5]), s32(machine.r[6])]))
    machine.stub(MEMCPY, lambda machine: calls.append(
        [CALL_COPY, actor_offset(machine.r[4]), actor_offset(machine.r[5]), s32(machine.r[6])]))
    machine.stub(QUEUE_ASSET, queue_asset)
    machine.follow.update((TWO_HANDED_ENTRY, QUEUE_ENTRY))


def put_models(machine: Machine, value: dict) -> None:
    machine.map_zero(ACTOR, ACTOR_BYTES)
    machine.map_zero(WEAPON2, 0x200)
    machine.map_zero(CHARACTER, 0x100)
    machine.put(LOAD_STATE, value["step"], 1)
    machine.put(ACTOR_POINTER, ACTOR)
    machine.put(ACTOR + FLAGS_WORD, value["flags_word"])
    machine.put(ACTOR + SEQUENCE_MODEL, value["sequence"], 1)
    machine.put(ACTOR + ANIMATION, value["animation"], 1)
    for index, (present, address) in enumerate(zip(value["weapons"], (WEAPON2, WEAPON3), strict=True)):
        machine.put(WEAPON_MODELS + 4 * (2 + index), address if present else 0)
        machine.put(address + WEAPON_FLAG, value["weapon_flags"][index], 1)
    machine.put(CHARACTER_STATE, CHARACTER)
    for index, slot in enumerate(value["slots"]):
        machine.put(CHARACTER + 0x48 + 4 * index, slot)


def model_inputs(value: dict) -> list[int]:
    return [value["step"], value["category"], value["model"], value["material"], value["sequence"],
            value["animation"], value["flags_word"], *value["weapons"], *value["weapon_flags"], value["busy"],
            *value["slots"]]


def run_reload(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    put_models(machine, value)
    stub_models(machine, value, calls)
    machine.r[4], machine.r[5], machine.r[6] = (value[key] & 0xFFFFFFFF for key in ("category", "model", "material"))
    machine.execute(entry)
    values = [0, s32(machine.r[2]), machine.load(LOAD_STATE, 1), machine.load(ACTOR + FLAGS_WORD, 4, signed=True),
              machine.load(WEAPON2 + WEAPON_FLAG, 1), machine.load(WEAPON3 + WEAPON_FLAG, 1)]
    names = ["refused", "result", "step", "flags word", "weapon 2 flag", "weapon 3 flag"]
    return values, names, model_inputs(value)


def run_two_handed(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    machine.r[4] = value["category"] & 0xFFFFFFFF
    machine.execute(entry)
    return [s32(machine.r[2])], ["result"], model_inputs(value)


def run_queue(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    put_models(machine, value)
    stub_models(machine, value, calls)
    machine.r[4], machine.r[5] = value["model"] & 0xFFFFFFFF, value["material"] & 0xFFFFFFFF
    machine.execute(entry)
    return [0], ["refused"], model_inputs(value)


RUNNERS = {CREATE: run_create, SELECTED: run_selected, UPDATE: run_update, ACCESSORY: run_accessory,
           RELOAD: run_reload, TWO_HANDED: run_two_handed, QUEUE: run_queue}


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    calls: list[list[int]] = []
    values, names, inputs = RUNNERS[mode](machine, value, entry, calls)
    observation = recorded_observation(machine, values, names, calls)
    return observation, [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    mode = value["mode"]
    if mode in (CREATE, UPDATE):
        allowed += [(ITEMS_TEXT, EQUIPPED + 1), (CURSOR, CURSOR + 1),
                    (ROWS, ROWS + ROW_BYTES * ROW_SLOTS), (TEXT, TEXT + RECORD_BYTES * value["count"])]
    if mode == ACCESSORY:
        allowed += [(STATS, STATS + 6 * STAT_COLUMN), (STATUS_ITEM, STATUS_ITEM + 1), (SUBTYPE, SUBTYPE + 1)]
    if mode == RELOAD:
        allowed += [(LOAD_STATE, LOAD_STATE + 1), (ACTOR + FLAGS_WORD, ACTOR + FLAGS_WORD + 4),
                    (WEAPON2 + WEAPON_FLAG, WEAPON2 + WEAPON_FLAG + 1),
                    (WEAPON3 + WEAPON_FLAG, WEAPON3 + WEAPON_FLAG + 1)]
    return allowed


def describe(image, results) -> list[str]:
    mode = results[0][0]["mode"] if results else None
    lines = []
    if mode in (CREATE, UPDATE):
        long = sum(1 for value, _, _ in results if value["count"] > VISIBLE)
        lines.append(f"{long} of {len(results)} vectors listed more than six rows")
    if mode == UPDATE:
        steps: dict[int, int] = {}
        for value, _, _ in results:
            kind = value["step"] if value["step"] in (16, 17) else (0 if value["step"] < VISIBLE else 6)
            steps[kind] = steps.get(kind, 0) + 1
        closed = sum(1 for _, observation, _ in results if observation["values"][1] == -1)
        scrolled = sum(1 for value, observation, _ in results if observation["values"][5] != value["first"])
        lines.append(f"steps (0 slide, 6 other) {dict(sorted(steps.items()))}; {closed} closed, "
                     f"{scrolled} scrolled")
    if mode == ACCESSORY:
        some = sum(1 for value, _, _ in results if value["id"])
        lines.append(f"{some} of {len(results)} vectors previewed an accessory")
    if mode == RELOAD:
        steps = {}
        for value, _, _ in results:
            steps[min(value["step"], 4)] = steps.get(min(value["step"], 4), 0) + 1
        done = sum(1 for _, observation, _ in results if observation["values"][1] == 0)
        lines.append(f"steps (4 other) {dict(sorted(steps.items()))}; {done} finished")
    if mode == QUEUE:
        queued = sum(1 for _, observation, _ in results if observation["calls"])
        lines.append(f"{queued} of {len(results)} vectors queued a model")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict, vectors: int = 1024) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return {**mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]),
            "VECTORS": vectors, "MUTANT_VECTORS": 256, "INCLUDES": INCLUDES}
