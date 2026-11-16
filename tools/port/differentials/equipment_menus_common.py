"""Shared harness for the equipment menu differentials (MENU3.PRG 16C.c:343-1233, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::Inventory core
(EquipmentMenus.cpp) in equipment_menus_driver.cpp. The inventory's weapon, shield, blade and armor tables,
their index lists, Ashley's held blade, vs_mainMenu_weaponHands, _menuText's hint and "none" entries, the
menu statics and globals and menu row 31 live at their addresses; every callee is stubbed and recorded.
The formatters write a marker per item into the row texts and the vector's flags into the row type, so the
recorded _populateMenuRows and vs_mainmenu_setMenuRows calls show which rows each item reached. The
dispatch case stubs the four slot menus at their addresses in _dispatchEquipMenu's handler table, which the
original reaches through jalr.

The typed cores refuse an index-list slot, count, highlighted item or chosen row outside its table, a body
part outside 1..7, a blade category outside vs_mainMenu_weaponHands and a _menuText offset outside the
table, where the original reads past them; vectors stay inside (EquipmentMenusTests.cpp covers the
refusals). An equipped item missing from the listed positions leaves the original's first choice row on
uninitialized stack, so vectors list it or unequip it.
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
    s32,
)

MODULE_BASES["MENU/MENU3.PRG"] = 0x80102800

MODULES = ("MENU/MENU3.PRG",)
DRIVER = "equipment_menus_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\Inventory\EquipmentMenus.cpp",)
INCLUDES = (r"Native\Content\include",)

# The slot menus' modes follow _dispatchEquipMenu's handler order (EquipmentMenu).
ARMOR, WEAPON, SHIELD, ACCESSORY, DISPATCH, TOP = range(6)
SLOT_MODES = (ARMOR, WEAPON, SHIELD, ACCESSORY)
# EquipmentTable.
TABLE_WEAPONS, TABLE_SHIELDS, TABLE_ARMOR = range(3)

# vs_main_inventory and vs_main_inventoryIndices.
WEAPONS = 0x80060168
WEAPON_BYTES = 0x20
SHIELDS = 0x80060268
SHIELD_BYTES = 0x30
BLADES = 0x800603E8
BLADE_BYTES = 0x2C
ARMOR_TABLE = 0x800607A8
ARMOR_BYTES = 0x28
INVENTORY_END = ARMOR_TABLE + 16 * ARMOR_BYTES
INDICES = 0x800619D8
WEAPON_LIST = INDICES
SHIELD_LIST = INDICES + 0x28
ARMOR_LIST = INDICES + 0x30
INDICES_BYTES = 0x40
WEAPON_SLOTS, SHIELD_SLOTS, BLADE_SLOTS, ARMOR_SLOTS = 8, 8, 16, 16
SLOTS = {ARMOR: ARMOR_SLOTS, WEAPON: WEAPON_SLOTS, SHIELD: SHIELD_SLOTS, ACCESSORY: ARMOR_SLOTS}
# Field offsets.
WEAPON_BLADE, WEAPON_EQUIPPED = 1, 3
SHIELD_EQUIPPED, SHIELD_MODEL, SHIELD_MATERIAL = 1, 6, 0x28
BLADE_MODEL, BLADE_CATEGORY, BLADE_MATERIAL = 2, 3, 0x28
ARMOR_CATEGORY, ARMOR_LIMB = 3, 0x26
ACCESSORY_LIMB = 7
BODY_PART_CATEGORIES = (0, 5, 5, 2, 3, 4, 6, 7)

MENU_TEXT = 0x801093B8
MENU_TEXT_ENTRIES = 472
FIRST_TEXT = 12  # VS_menuText_INDEX_equipWeapon; 20 and 21 are "none"
TEXTS = 10
EQUIPPED_ITEM = 0x80109767  # _equippedItem
HANDS = 0x80102140  # vs_mainMenu_weaponHands (MAINMENU.PRG data)
WEAPON_CATEGORIES = 10
CHARACTER_STATE = 0x800F19FC
CHARACTER = 0x80570000
ACTOR = 0x80571000
HELD_ID, HELD_CATEGORY = 0x54, 0x58
BUTTONS = 0x8005E1D0  # vs_main_buttonsPressed
ROWS = 0x80510000
ROW_BYTES = 0x40
ROW_SLOTS = 64
TITLE_ROW = 31
RENDER_STATE = 0x80109568  # _ashleyRenderState
ICONS = 0x80109712  # _itemCategoryIconsEnabled
STATUS = 0x80109713  # _statusCommandState
ICON_ON_TOP = 0x80109717  # _selectedItemCategoryIconOnTop

# Each menu's selection word and state bytes.
STATICS = {
    ACCESSORY: (0x80109684, (0x80109688, 0x80109689)),
    WEAPON: (0x8010968C, (0x80109690, 0x80109691, 0x80109692, 0x80109693)),
    SHIELD: (0x80109694, (0x80109698, 0x80109699, 0x8010969A)),
    ARMOR: (0x8010969C, (0x801096A0, 0x801096A1, 0x801096A2)),
    TOP: (0x801096A4, (0x801096A8,)),
}
CURRENT_MENU = 0x801096A3
HANDLERS = (0x80107B14, 0x80107148, 0x80107620, 0x80106C94)

PREVIEWS = {0x800FD700: ARMOR, 0x800FD270: WEAPON, 0x800FD5A0: SHIELD, 0x80106BB4: ACCESSORY}
FORMATTERS = {0x800FCA08: (TABLE_WEAPONS, WEAPONS, WEAPON_BYTES, WEAPON_SLOTS),
              0x800FCE40: (TABLE_SHIELDS, SHIELDS, SHIELD_BYTES, SHIELD_SLOTS),
              0x800FD084: (TABLE_ARMOR, ARMOR_TABLE, ARMOR_BYTES, ARMOR_SLOTS)}
STAT_PAGES = 0x800FBBD4
DP_PP_BARS = 0x800FC268
PRIMARY_PANEL = 0x800FBEA4
ROWS_IDLE = 0x800FA9D0
COUNT_ITEMS = 0x800FEA6C
POPULATE = 0x80106390
UPDATE_CHOICES = 0x8010659C
SELECTED_CHOICE = 0x80106574
CLEAR = 0x800FA8E0
FLY_OUT = 0x800FA810
UNEQUIP_WEAPON = 0x800FE360
UNEQUIP_SHIELD = 0x800FE3A0
EQUIP_WEAPON = 0x8006B338
EQUIP_SHIELD = 0x8006B460
EQUIP_ARMOR = 0x8006B4B8
EQUIP_ACCESSORY = 0x8006B524
RELOAD = 0x801090C4
SET_MENU_ITEM = 0x800C8E5C
CREATE_TOP_ROWS = 0x801005E0
PLAY_INVALID = 0x800C02E0
SELECTED_ROW = 0x801008C8
HIGHLIGHTED_ROW = 0x801008B0
START_TRANSITION = 0x80102B78
CONTINUE_TRANSITION = 0x80102BE4
DISPATCH_ENTRY = 0x80108010

(CALL_PREVIEW, CALL_PAGES, CALL_BARS, CALL_PANEL, CALL_IDLE, CALL_COUNT, CALL_FORMAT, CALL_POPULATE,
 CALL_CHOICES, CALL_SELECTED, CALL_CLEAR, CALL_FLY_OUT, CALL_UNEQUIP_WEAPON, CALL_UNEQUIP_SHIELD,
 CALL_EQUIP_WEAPON, CALL_EQUIP_SHIELD, CALL_EQUIP_ARMOR, CALL_EQUIP_ACCESSORY, CALL_RELOAD, CALL_SET_ITEM,
 CALL_TOP_ROWS, CALL_INVALID, CALL_SELECTED_ROW, CALL_HIGHLIGHTED_ROW, CALL_START, CALL_CONTINUE,
 CALL_DISPATCH, CALL_HANDLER) = range(1, 29)

# A formatted row's texts: MARKER + table * 0x100 + slot * 2 + (0 name, 1 description).
MARKER = 0x100000
MISALIGNED = 999
IDLE_CALLS = 4
TOP_ROWS = 8


def index_list(rng: random.Random, size: int) -> list[int]:
    shape = rng.randrange(4)
    items = list(range(1, size + 1))
    rng.shuffle(items)
    if shape == 1:
        return [item if rng.randrange(4) else 0 for item in items]
    if shape == 2:
        listed = rng.randrange(size + 1)
        return items[:listed] + [0] * (size - listed)
    if shape == 3:
        return [rng.randrange(size + 1) for _ in range(size)]
    return items


def equipped_bytes(rng: random.Random, size: int) -> list[int]:
    shape = rng.randrange(4)
    flags = [0] * size
    if shape == 1:
        flags[rng.randrange(size)] = 1
    elif shape == 2:
        for slot in rng.sample(range(size), rng.randrange(1, 4)):
            flags[slot] = rng.choice((1, rng.randrange(1, 256)))
    elif shape == 3:
        flags = [rng.choice((0, 0, 1, rng.randrange(256))) for _ in range(size)]
    return flags


def row_flags(rng: random.Random) -> int:
    flags = rng.getrandbits(32)
    if rng.randrange(2):
        flags &= (1 << 19) - 1
    return s32(flags)


def statics_of(rng: random.Random, mode: int, step: int) -> list[int]:
    selected = rng.choice((0, 1, 2, 5, -1, -2, s32(rng.getrandbits(32))))
    extra = [rng.randrange(256) for _ in range(3)]
    if mode == ACCESSORY:
        extra[0] = rng.randrange(ARMOR_SLOTS + 1)
    elif mode == ARMOR:
        extra[0] = rng.randrange(ARMOR_SLOTS + 1)
        if step in (0, 1) or rng.randrange(2):
            extra[1] = rng.randrange(1, 8)
    elif mode == TOP and step == 3:
        # The quit step returns a quit without waiting for the rows.
        selected = rng.choice((-2, selected))
    return [selected, step, *extra]


def slot_vector(rng: random.Random, mode: int) -> dict:
    if mode == TOP:
        step = rng.choice((0, 0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, rng.randrange(6, 256)))
        initialize = rng.choice((0, 0, 0, 0, 0, 1, s32(rng.getrandbits(32)) or 1))
    else:
        step = rng.choice((0, 0, 0, 1, 1, 1, 2, 2, 3, 3, rng.randrange(4, 256)))
        initialize = 0
        if rng.randrange(5) == 0:
            initialize = rng.randrange(1, 8) if mode == ARMOR else rng.choice((1, 2, 8, rng.randrange(1, 1 << 31)))
    statics = statics_of(rng, mode, step)
    slots = SLOTS.get(mode, ARMOR_SLOTS)
    part = statics[3] if mode == ARMOR else ACCESSORY_LIMB
    category = BODY_PART_CATEGORIES[part] if 0 < part < len(BODY_PART_CATEGORIES) else 7
    value = {
        "mode": mode, "initialize": initialize, "statics": statics,
        "count": rng.choice((0, slots, rng.randrange(slots + 1), rng.randrange(slots + 1))),
        "weapon_list": index_list(rng, WEAPON_SLOTS), "shield_list": index_list(rng, SHIELD_SLOTS),
        "armor_list": index_list(rng, ARMOR_SLOTS),
        "weapon_equipped": equipped_bytes(rng, WEAPON_SLOTS),
        "weapon_blade": [rng.randrange(1, BLADE_SLOTS + 1) for _ in range(WEAPON_SLOTS)],
        "blade_category": [rng.randrange(1, WEAPON_CATEGORIES + 1) for _ in range(BLADE_SLOTS)],
        "blade_model": [rng.randrange(256) for _ in range(BLADE_SLOTS)],
        "blade_material": [rng.randrange(256) for _ in range(BLADE_SLOTS)],
        "shield_equipped": equipped_bytes(rng, SHIELD_SLOTS),
        "shield_model": [rng.randrange(256) for _ in range(SHIELD_SLOTS)],
        "shield_material": [rng.randrange(256) for _ in range(SHIELD_SLOTS)],
        "armor_category": [rng.choice((7, 7, category, category, rng.randrange(1, 9), rng.randrange(256)))
                           for _ in range(ARMOR_SLOTS)],
        "armor_limb": [rng.choice((0, 0, 0, 0, part, rng.randrange(1, 8), rng.randrange(256)))
                       for _ in range(ARMOR_SLOTS)],
        "held": [rng.choice((0, 0, rng.randrange(1, 0x10000))), rng.randrange(1, WEAPON_CATEGORIES + 1)],
        "hands": [rng.choice((0, 0, 1, rng.randrange(256))) for _ in range(WEAPON_CATEGORIES)],
        "texts": [rng.randrange(MENU_TEXT_ENTRIES) for _ in range(TEXTS)],
        "choice_row": rng.choice((-3, -2, -1, -1, 0, 0, rng.randrange(1, 17), rng.randrange(1, 4),
                                  rng.randrange(1, 4))),
        "idle": [rng.choice((0, 1, 1)) for _ in range(IDLE_CALLS)],
        "flags": [[row_flags(rng) for _ in range(size)] for size in (WEAPON_SLOTS, SHIELD_SLOTS, ARMOR_SLOTS)],
        "busy": rng.randrange(2),
        "top_row": rng.choice((-1, -1, -2, -3, rng.randrange(-5, TOP_ROWS), rng.randrange(TOP_ROWS))),
        "highlighted_row": rng.randrange(TOP_ROWS),
        "buttons": s32(rng.getrandbits(32)),
        "dispatch_result": rng.choice((0, 0, -2, -1, 1, 2, 17, s32(rng.getrandbits(32)))),
        "globals": [s32(rng.getrandbits(32)), rng.randrange(256), rng.randrange(256), rng.randrange(256)],
        "row": [rng.randrange(256), rng.randrange(256), rng.randrange(-0x8000, 0x8000)],
    }
    value["before"] = rng.randrange(slots + 1)
    value["after"] = rng.randrange(1, slots + 1) if value["choice_row"] >= 1 else rng.randrange(slots + 1)
    if mode in SLOT_MODES:
        list_equipped_item(rng, value, mode, part)
    return value


def equipped_rule(value: dict, mode: int, part: int):
    """The slot menu's "equipped" test and how to clear it from a 0-based slot."""
    if mode in (ACCESSORY, ARMOR):
        limbs = value["armor_limb"]
        wanted = ACCESSORY_LIMB if mode == ACCESSORY else part
        return (lambda slot: limbs[slot] == wanted), (lambda slot: limbs.__setitem__(slot, 0)), "armor_list"
    key, listed = ("weapon_equipped", "weapon_list") if mode == WEAPON else ("shield_equipped", "shield_list")
    flags = value[key]
    return (lambda slot: flags[slot] != 0), (lambda slot: flags.__setitem__(slot, 0)), listed


def list_equipped_item(rng: random.Random, value: dict, mode: int, part: int) -> None:
    """Lists an equipped item the listed positions miss, or unequips it."""
    is_equipped, clear, key = equipped_rule(value, mode, part)
    listed = value[key][:value["count"]]
    slots = SLOTS[mode]
    equipped = [slot for slot in range(slots) if is_equipped(slot)]
    if not equipped or any(slot and is_equipped(slot - 1) for slot in listed):
        return
    if value["count"] and rng.randrange(2):
        value[key][rng.randrange(value["count"])] = rng.choice(equipped) + 1
        return
    for slot in equipped:
        clear(slot)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    if mode == DISPATCH:
        row = rng.randrange(9)
        return {"mode": mode, "row": row, "current": rng.randrange(4),
                "result": rng.choice((0, 0, -2, -1, 1, 2, 9, s32(rng.getrandbits(32))))}
    return slot_vector(rng, mode)


def text_word(word: int) -> int:
    """A row text: a _menuText halfword offset, or the formatter's marker."""
    offset = word - MENU_TEXT
    if 0 <= offset < 2 * MENU_TEXT_ENTRIES and offset % 2 == 0:
        return offset // 2
    return s32(word)


def index_of(address: int, base: int, stride: int) -> int:
    offset = s32(address - base)
    return offset // stride if offset % stride == 0 else MISALIGNED


def put_fixture(machine: Machine, value: dict) -> None:
    machine.map_zero(WEAPONS, INVENTORY_END - WEAPONS)
    machine.map_zero(INDICES, INDICES_BYTES)
    for base, items in ((WEAPON_LIST, value["weapon_list"]), (SHIELD_LIST, value["shield_list"]),
                        (ARMOR_LIST, value["armor_list"])):
        for index, item in enumerate(items):
            machine.put(base + index, item, 1)
    for slot in range(WEAPON_SLOTS):
        machine.put(WEAPONS + slot * WEAPON_BYTES + WEAPON_BLADE, value["weapon_blade"][slot], 1)
        machine.put(WEAPONS + slot * WEAPON_BYTES + WEAPON_EQUIPPED, value["weapon_equipped"][slot], 1)
    for slot in range(BLADE_SLOTS):
        at = BLADES + slot * BLADE_BYTES
        machine.put(at + BLADE_MODEL, value["blade_model"][slot], 1)
        machine.put(at + BLADE_CATEGORY, value["blade_category"][slot], 1)
        machine.put(at + BLADE_MATERIAL, value["blade_material"][slot], 1)
    for slot in range(SHIELD_SLOTS):
        at = SHIELDS + slot * SHIELD_BYTES
        machine.put(at + SHIELD_EQUIPPED, value["shield_equipped"][slot], 1)
        machine.put(at + SHIELD_MODEL, value["shield_model"][slot], 1)
        machine.put(at + SHIELD_MATERIAL, value["shield_material"][slot], 1)
    for slot in range(ARMOR_SLOTS):
        at = ARMOR_TABLE + slot * ARMOR_BYTES
        machine.put(at + ARMOR_CATEGORY, value["armor_category"][slot], 1)
        machine.put(at + ARMOR_LIMB, value["armor_limb"][slot], 1)
    machine.map_zero(CHARACTER_STATE, 4)
    machine.map_zero(CHARACTER, 0x100)
    machine.map_zero(ACTOR, 0x100)
    machine.put(CHARACTER_STATE, CHARACTER)
    machine.put(CHARACTER + 0x3C, ACTOR)
    machine.put(ACTOR + HELD_ID, value["held"][0], 2)
    machine.put(ACTOR + HELD_CATEGORY, value["held"][1], 1)
    machine.map_zero(HANDS, WEAPON_CATEGORIES)
    for index, item in enumerate(value["hands"]):
        machine.put(HANDS + index, item, 1)
    for index, item in enumerate(value["texts"]):
        machine.put(MENU_TEXT + 2 * (FIRST_TEXT + index), item, 2)
    machine.put(EQUIPPED_ITEM, value["before"], 1)
    machine.map_zero(BUTTONS, 4)
    machine.put(BUTTONS, value["buttons"])
    machine.map_zero(ROWS, ROW_BYTES * ROW_SLOTS)
    row = ROWS + TITLE_ROW * ROW_BYTES
    machine.put(row, value["row"][0], 1)
    machine.put(row + 6, value["row"][1], 1)
    machine.put(row + 0x18, value["row"][2], 2)
    render, icons, status, icon_on_top = value["globals"]
    machine.put(RENDER_STATE, render)
    machine.put(ICONS, icons, 1)
    machine.put(STATUS, status, 1)
    machine.put(ICON_ON_TOP, icon_on_top, 1)
    mode = value["mode"]
    selected_at, state_at = STATICS[mode]
    machine.put(selected_at, value["statics"][0])
    for address, item in zip(state_at, value["statics"][1:], strict=False):
        machine.put(address, item, 1)


def stub_menus(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    mode = value["mode"]
    idle_calls = [0]

    def preview(menu: int):
        return lambda machine: calls.append([CALL_PREVIEW, menu, s32(machine.r[4])])

    def idle(machine: Machine) -> None:
        calls.append([CALL_IDLE])
        at = idle_calls[0]
        idle_calls[0] += 1
        machine.r[2] = value["idle"][at] if at < IDLE_CALLS else 0

    def count(machine: Machine) -> None:
        calls.append([CALL_COUNT, s32(machine.r[4]), s32(machine.r[5])])
        machine.r[2] = value["count"]

    def row_bases(machine: Machine) -> tuple[int, int, int]:
        if mode == TOP:
            sp = machine.r[29]
            return sp + 0x18, sp + 0x58, sp + 0x78
        # The slot menus' variable-length arrays below their frame pointer.
        rows = value["count"] + 1
        text = machine.load(machine.r[30] + 0x10)
        return text, text - ((rows * 4 + 7) & ~7), text + rows * 8

    def formatter(table: int, base: int, stride: int, size: int):
        def format_row(machine: Machine) -> None:
            slot = index_of(machine.r[4], base, stride)
            text, types, buffer = row_bases(machine)
            calls.append([CALL_FORMAT, table, slot, index_of(machine.r[5], text, 8),
                          index_of(machine.r[6], types, 4), index_of(machine.r[7], buffer, 96)])
            machine.put(machine.r[5], MARKER + table * 0x100 + slot * 2)
            machine.put(machine.r[5] + 4, MARKER + table * 0x100 + slot * 2 + 1)
            machine.put(machine.r[6], value["flags"][table][slot] if 0 <= slot < size else 0)
        return format_row

    def populate(machine: Machine) -> None:
        rows, text, types = s32(machine.r[4]), machine.r[5], machine.r[6]
        call = [CALL_POPULATE, rows]
        for row in range(max(0, min(rows, 32))):
            call += [machine.load(types + 4 * row, 4, signed=True), text_word(machine.load(text + 8 * row)),
                     text_word(machine.load(text + 8 * row + 4))]
        calls.append(call)

    def update_choices(machine: Machine) -> None:
        calls.append([CALL_CHOICES, s32(machine.r[4])])
        machine.put(EQUIPPED_ITEM, value["after"], 1)

    def selected_choice(machine: Machine) -> None:
        calls.append([CALL_SELECTED])
        machine.r[2] = value["choice_row"] & 0xFFFFFFFF

    def item(base: int, stride: int, address: int) -> int:
        return -1 if address == 0 else index_of(address, base, stride)

    def reload(machine: Machine) -> None:
        calls.append([CALL_RELOAD, *(s32(machine.r[i]) for i in (4, 5, 6))])
        machine.r[2] = value["busy"]

    def set_menu_item(machine: Machine) -> None:
        sp = machine.r[29]
        row = s32(machine.r[4])
        calls.append([CALL_SET_ITEM, *(s32(machine.r[i]) for i in range(4, 8)), s32(machine.load(sp + 0x10)),
                      text_word(machine.load(sp + 0x14))])
        machine.r[2] = ROWS + (row if 0 <= row < ROW_SLOTS else 0) * ROW_BYTES

    def top_rows(machine: Machine) -> None:
        text, types = machine.r[6], machine.r[7]
        calls.append([CALL_TOP_ROWS, s32(machine.r[4]), s32(machine.r[5]),
                      *(text_word(machine.load(text + 4 * index)) for index in range(2 * TOP_ROWS)),
                      *(machine.load(types + 4 * index, 4, signed=True) for index in range(TOP_ROWS))])

    def returning(call: int, result: int):
        def stub(machine: Machine) -> None:
            calls.append([call])
            machine.r[2] = result & 0xFFFFFFFF
        return stub

    def dispatch(machine: Machine) -> None:
        calls.append([CALL_DISPATCH, s32(machine.r[4])])
        machine.r[2] = value["dispatch_result"] & 0xFFFFFFFF

    for address, menu in PREVIEWS.items():
        machine.stub(address, preview(menu))
    for address, (table, base, stride, size) in FORMATTERS.items():
        machine.stub(address, formatter(table, base, stride, size))
    one_argument = {STAT_PAGES: CALL_PAGES, DP_PP_BARS: CALL_BARS, PRIMARY_PANEL: CALL_PANEL, CLEAR: CALL_CLEAR,
                    FLY_OUT: CALL_FLY_OUT, START_TRANSITION: CALL_START, CONTINUE_TRANSITION: CALL_CONTINUE}
    for address, call in one_argument.items():
        machine.stub(address, lambda machine, call=call: calls.append([call, s32(machine.r[4])]))
    machine.stub(ROWS_IDLE, idle)
    machine.stub(COUNT_ITEMS, count)
    machine.stub(POPULATE, populate)
    machine.stub(UPDATE_CHOICES, update_choices)
    machine.stub(SELECTED_CHOICE, selected_choice)
    machine.stub(UNEQUIP_WEAPON, lambda machine: calls.append([CALL_UNEQUIP_WEAPON]))
    machine.stub(UNEQUIP_SHIELD, lambda machine: calls.append([CALL_UNEQUIP_SHIELD]))
    machine.stub(EQUIP_WEAPON, lambda machine: calls.append(
        [CALL_EQUIP_WEAPON, item(WEAPONS, WEAPON_BYTES, machine.r[4])]))
    machine.stub(EQUIP_SHIELD, lambda machine: calls.append(
        [CALL_EQUIP_SHIELD, item(SHIELDS, SHIELD_BYTES, machine.r[4])]))
    machine.stub(EQUIP_ARMOR, lambda machine: calls.append(
        [CALL_EQUIP_ARMOR, s32(machine.r[4]), item(ARMOR_TABLE, ARMOR_BYTES, machine.r[5])]))
    machine.stub(EQUIP_ACCESSORY, lambda machine: calls.append(
        [CALL_EQUIP_ACCESSORY, item(ARMOR_TABLE, ARMOR_BYTES, machine.r[4])]))
    machine.stub(RELOAD, reload)
    machine.stub(SET_MENU_ITEM, set_menu_item)
    machine.stub(CREATE_TOP_ROWS, top_rows)
    machine.stub(PLAY_INVALID, lambda machine: calls.append([CALL_INVALID]))
    machine.stub(SELECTED_ROW, returning(CALL_SELECTED_ROW, value["top_row"]))
    machine.stub(HIGHLIGHTED_ROW, returning(CALL_HIGHLIGHTED_ROW, value["highlighted_row"]))
    machine.stub(DISPATCH_ENTRY, dispatch)


def slot_inputs(value: dict) -> list[int]:
    return [value["initialize"], *value["statics"], value["count"], *value["weapon_list"], *value["shield_list"],
            *value["armor_list"], *value["weapon_equipped"], *value["weapon_blade"], *value["blade_category"],
            *value["blade_model"], *value["blade_material"], *value["shield_equipped"], *value["shield_model"],
            *value["shield_material"], *value["armor_category"], *value["armor_limb"], *value["held"],
            *value["hands"], *value["texts"], value["before"], value["after"], value["choice_row"],
            *value["idle"], *(flag for table in value["flags"] for flag in table), value["busy"],
            value["top_row"], value["highlighted_row"], value["buttons"], value["dispatch_result"],
            *value["globals"], *value["row"]]


def run_menu(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    put_fixture(machine, value)
    stub_menus(machine, value, calls)
    machine.r[4] = value["initialize"] & 0xFFFFFFFF
    machine.execute(entry)
    mode = value["mode"]
    selected_at, state_at = STATICS[mode]
    values = [0, s32(machine.r[2]), machine.load(selected_at, 4, signed=True),
              *(machine.load(address, 1) for address in state_at)]
    names = ["refused", "result", "selected", *(f"static {index}" for index in range(len(state_at)))]
    for slot in range(WEAPON_SLOTS):
        values.append(machine.load(WEAPONS + slot * WEAPON_BYTES + WEAPON_EQUIPPED, 1))
        names.append(f"weapon {slot} equipped")
    for slot in range(SHIELD_SLOTS):
        values.append(machine.load(SHIELDS + slot * SHIELD_BYTES + SHIELD_EQUIPPED, 1))
        names.append(f"shield {slot} equipped")
    for slot in range(ARMOR_SLOTS):
        values.append(machine.load(ARMOR_TABLE + slot * ARMOR_BYTES + ARMOR_LIMB, 1))
        names.append(f"armor {slot} limb")
    row = ROWS + TITLE_ROW * ROW_BYTES
    values += [machine.load(RENDER_STATE, 4, signed=True), machine.load(ICONS, 1), machine.load(STATUS, 1),
               machine.load(ICON_ON_TOP, 1), machine.load(row, 1), machine.load(row + 6, 1),
               machine.load(row + 0x18, 2, signed=True)]
    names += ["render state", "icons enabled", "status command", "icon on top", "row state", "row selected",
              "row target"]
    return values, names, slot_inputs(value)


def run_dispatch(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    machine.put(CURRENT_MENU, value["current"], 1)

    def handler(which: int):
        def stub(machine: Machine) -> None:
            calls.append([CALL_HANDLER, which, s32(machine.r[4])])
            machine.r[2] = value["result"] & 0xFFFFFFFF
        return stub

    for which, address in enumerate(HANDLERS):
        machine.stub(address, handler(which))
    machine.r[4] = value["row"]
    machine.execute(entry)
    values = [0, s32(machine.r[2]), machine.load(CURRENT_MENU, 1)]
    return values, ["refused", "result", "current"], [value["row"], value["current"], value["result"]]


RUNNERS = {ARMOR: run_menu, WEAPON: run_menu, SHIELD: run_menu, ACCESSORY: run_menu, DISPATCH: run_dispatch,
           TOP: run_menu}


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    mode = value["mode"]
    if mode == DISPATCH:
        return [*allowed, (CURRENT_MENU, CURRENT_MENU + 1)]
    selected_at, state_at = STATICS[mode]
    allowed += [(selected_at, selected_at + 4), *((address, address + 1) for address in state_at)]
    if mode in (ACCESSORY, ARMOR):
        allowed += [(at + ARMOR_LIMB, at + ARMOR_LIMB + 1)
                    for at in range(ARMOR_TABLE, INVENTORY_END, ARMOR_BYTES)]
    elif mode == WEAPON:
        allowed += [(at + WEAPON_EQUIPPED, at + WEAPON_EQUIPPED + 1)
                    for at in range(WEAPONS, WEAPONS + WEAPON_SLOTS * WEAPON_BYTES, WEAPON_BYTES)]
    elif mode == SHIELD:
        allowed += [(at + SHIELD_EQUIPPED, at + SHIELD_EQUIPPED + 1)
                    for at in range(SHIELDS, SHIELDS + SHIELD_SLOTS * SHIELD_BYTES, SHIELD_BYTES)]
    elif mode == TOP:
        row = ROWS + TITLE_ROW * ROW_BYTES
        allowed += [(RENDER_STATE, RENDER_STATE + 4), (ICONS, STATUS + 1), (ICON_ON_TOP, ICON_ON_TOP + 1),
                    (row, row + ROW_BYTES)]
    return allowed


def describe(image, results) -> list[str]:
    if not results:
        return []
    mode = results[0][0]["mode"]
    if mode == DISPATCH:
        handlers: dict[int, int] = {}
        for _, observation, _ in results:
            which = observation["calls"][0][1] if observation["calls"] else -1
            handlers[which] = handlers.get(which, 0) + 1
        return [f"handlers called (0 armor, 1 weapon, 2 shield, 3 accessory) {dict(sorted(handlers.items()))}"]
    steps: dict[int, int] = {}
    for value, _, _ in results:
        step = -1 if value["initialize"] else min(value["statics"][1], 6)
        steps[step] = steps.get(step, 0) + 1
    closed = sum(1 for _, observation, _ in results if observation["values"][1] != 0)
    built = sum(1 for _, observation, _ in results
                if any(call[0] in (CALL_POPULATE, CALL_TOP_ROWS) for call in observation["calls"]))
    equipped = sum(1 for _, observation, _ in results
                   if any(CALL_EQUIP_WEAPON <= call[0] <= CALL_EQUIP_ACCESSORY for call in observation["calls"]))
    return [f"steps (-1 initialize, 6 other) {dict(sorted(steps.items()))}",
            f"{built} built rows, {equipped} equipped, {closed} returned nonzero"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict, vectors: int = 1024) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return {**mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]),
            "VECTORS": vectors, "MUTANT_VECTORS": 256, "INCLUDES": INCLUDES}
