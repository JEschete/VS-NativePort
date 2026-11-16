"""Shared harness for the loot screen policy differentials (MENUB.PRG 260.c:1015-1099 and 1192-1619, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::Loot core
(LootPolicies.cpp) in loot_policies_driver.cpp. The loot list, the staged inventory, the item names, the
menu rows and the description text live at fixed addresses; every callee is stubbed and recorded. Both
sides print a refusal marker, the observed globals and fixture fields and the recorded calls.

The typed cores refuse a previewed row or a window outside the loot list, a row outside the item
categories and a misc slot outside its table, where the original reads past them; vectors stay inside
(LootPoliciesTests.cpp covers the refusals).
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Machine,
    mode_case,
    recorded_observation,
    s32,
)

MODULES = ("MENU/MENUB.PRG",)
DRIVER = "loot_policies_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\Loot\LootPolicies.cpp",)
INCLUDES = (
    r"Native\Engine", r"Native\Content", r"Native\Engine\include", r"Native\Engine\src",
    r"Native\Content\include", r"Native\Content\src", r"Native\Game\include", r"Native\Game\src",
)

CAPACITIES, ROWS, DESCRIPTION, LEAVE, ORGANIZE = range(5)
MASK = 0xFFFFFFFF

# 260.c's globals and the prompts' statics.
LOOT_LIST_POINTER = 0x8010A6A0  # _lootList
INVENTORY_POINTER = 0x8010A6A8  # _inventory
BACKUP_POINTER = 0x8010A6AC  # _inventoryBackup
LIST_OFFSET = 0x8010A6B5  # _lootListOffset
LIST_COUNT = 0x8010A6B6  # _lootListCount
PREVIEWED = 0x8010A6B7  # D_8010A6B7
CLAIMED = 0x8010A6B8  # _lootClaimed
GRADIENT_INDEX = 0x8010A504  # _gradientStateIndex
CONTROLS = 0x8010A505  # _controlsEnabled
HEADERS = 0x8010A5CC  # _itemCategoryHeaders
MENU_TEXT = 0x8010A280  # _menuText
LEAVE_STATE = 0x8010A691
LEAVE_CHOICE = 0x8010A692
LEAVE_NAME = 0x8010A694
LEAVE_CURSOR = 0x8010A5E8
ORGANIZE_STATE = 0x8010A698
ORGANIZE_CATEGORY = 0x8010A699
ORGANIZE_SELECTED = 0x8010A69A
ORGANIZE_ACTION = 0x8010A69B
ORGANIZE_CURSOR = 0x8010A5E9
CAPACITY_TABLE = 0x801022A0  # vs_mainMenu_inventoryItemCapacities
NAMES_POINTER = 0x8010229C  # vs_mainMenu_itemNames
LOOT_ENTRY = 0x800EB9C8  # vs_menuB_lootEntry
STRING0 = 0x800F5190  # vs_battle_stringContext.strings[0]
STRING_BUFFER = 0x800F4E8C  # vs_battle_stringBuf
INFORMATION = 0x80060028  # vs_main_settings.information
MAIN_INVENTORY = 0x80060168  # vs_main_inventory
BUTTONS = 0x8005E1D0  # vs_main_buttonsPressed
REPEAT = 0x8005DFDC  # vs_main_buttonRepeat

# Fixtures.
INVENTORY = 0x80500000
INVENTORY_BYTES = 0xF00
LIST = 0x80510000
LIST_SLOTS = 64
NAMES = 0x80530000
NAME_BYTES = 24
NAME_COUNT = 512
NAMES_TEXT_ID = 0x10000
BACKUP = 0x80540000
MENU_ROWS = 0x80560000
MENU_ROW_BYTES = 0x40
MENU_ROW_SLOTS = 40
TEXT = 0x80570000
HEADER_TEXT = 0x80580000
ROW_TYPE = 0x55
# _menuText[10], [12] and [14], the action texts' offsets.
ACTION_TEXTS = (0x150, 0x160, 0x170)

# Callees.
RENDER_TEXT = 0x800C6828
RENDER_TEXT_COLOR = 0x800C6540
ITEM_COUNT = 0x800FEA6C
CONTAINED = 0x80108264
RENDER_INT = 0x800FFD64
BUTTON_BACKGROUND = 0x80100344
BUTTON = 0x800FFC68
MEMZERO = 0x800CCC1C
GET_MENU_ITEM = 0x800C8E48
SET_MENU_ITEM = 0x800C8E5C
COPY = 0x800C01E0
FORMATTERS = (0x800FCA08, 0x800FCAA4, 0x800FCC0C, 0x800FCE40, 0x800FD084, 0x800FD0E0, 0x800FD17C)
INFO = 0x800FFC04
INIT_TEXT_BOX = 0x800C8E04
INIT_INFO_BOX = 0x800FFBC8
DISMISS = 0x800FFBA8
FLY_OUT = 0x800FA8A0
ROWS_IDLE = 0x800FA9D0
LEAVE_SOUND = 0x800C02E8
SELECT_SOUND = 0x800C02F0
CHANGE_SOUND = 0x800C02F8
SFX = 0x80045988
DRAW_CURSOR = 0x800CCD40

# The panel's strings in the module.
LABELS = {0x801029A4: 0, 0x801029B0: 1, 0x801029B4: 3, 0x801029B8: 4, 0x801029BC: 5}
CATEGORY_LABEL = 2
PROMPT_TEXTS = {69: 0, 71: 1, 73: 2, 97: 3, 109: 4, ACTION_TEXTS[0]: 5, ACTION_TEXTS[1]: 6, 190: 7, 125: 8,
                155: 9, 200: 10}

CALL_LABEL, CALL_COLORED, CALL_COUNT, CALL_CONTAINED, CALL_INTEGER, CALL_BACKGROUND, CALL_BUTTON = range(1, 8)
CALL_GET_ROW, CALL_SET_ROW = 10, 11
CALL_COPY, CALL_FORMAT, CALL_INFO = 20, 21, 22
(CALL_INIT_TEXT, CALL_INIT_BOX, CALL_IDLE, CALL_FLY_OUT, CALL_DISMISS, CALL_LEAVE, CALL_CHANGE, CALL_SELECT,
 CALL_SFX, CALL_CURSOR) = range(30, 40)

CATEGORY_CAPACITIES = (8, 16, 16, 8, 16, 48, 64)
LAYOUTS = ((0x000, 0x20), (0x280, 0x2C), (0x540, 0x10), (0x100, 0x30), (0x640, 0x28), (0x8C0, 0x1C),
           (0xE00, 0x04))
MISC = 6
FIRST_KEY_ITEM = 458
# MenuRow fields the cases compare: (offset, size, signed).
ROW_FIELDS = ((0x00, 1, False), (0x02, 1, False), (0x04, 1, False), (0x05, 1, False), (0x06, 1, False),
              (0x07, 1, False), (0x09, 1, False), (0x0A, 1, False), (0x0C, 1, False), (0x10, 2, False),
              (0x14, 2, True), (0x18, 2, True))
ROW_FIELD_NAMES = ("state", "background", "gradient", "scrollable", "selected", "unselectable", "icon", "font",
                   "material", "count", "x", "target0")
LOOT_ROWS = range(32, 40)
PROMPT_ROWS = range(30, 32)


def set16(data: bytearray, offset: int, value: int) -> None:
    data[offset:offset + 2] = (value & 0xFFFF).to_bytes(2, "little")


def misc_id(rng: random.Random) -> int:
    pick = rng.random()
    if pick < 0.2:
        return rng.choice((FIRST_KEY_ITEM - 1, FIRST_KEY_ITEM))
    if pick < 0.6:
        return rng.randrange(440, NAME_COUNT)
    return rng.randrange(NAME_COUNT)


def inventory_bytes(rng: random.Random) -> bytes:
    """A staged inventory of random bytes whose blade links and 16-bit item ids stay inside their tables."""
    data = bytearray(rng.randbytes(INVENTORY_BYTES))
    for weapon in range(CATEGORY_CAPACITIES[0]):
        data[weapon * 0x20 + 1] = rng.randrange(1, CATEGORY_CAPACITIES[1] + 1)
    for grip in range(CATEGORY_CAPACITIES[2]):
        set16(data, 0x540 + grip * 0x10, rng.randrange(NAME_COUNT))
    for gem in range(CATEGORY_CAPACITIES[5]):
        set16(data, 0x8C0 + gem * 0x1C, rng.randrange(NAME_COUNT))
    for misc in range(CATEGORY_CAPACITIES[MISC]):
        set16(data, 0xE00 + misc * 4, misc_id(rng))
    return bytes(data)


def inventory_inputs(data: bytes) -> list[int]:
    return [int.from_bytes(data[at:at + 4], "little") for at in range(0, INVENTORY_BYTES, 4)]


def loot_row(rng: random.Random, invalid: bool = False) -> list[int]:
    category = rng.randrange(7, 10) if invalid and rng.random() < 0.1 else rng.randrange(7)
    index = rng.randrange(CATEGORY_CAPACITIES[category]) if category < 7 else rng.randrange(256)
    slide = rng.randrange(1, 6) if rng.random() < 0.3 else 0
    return [category, index, slide, rng.randrange(6)]


def loot_list(rng: random.Random, invalid: bool = False) -> list[list[int]]:
    return [loot_row(rng, invalid) for _ in range(LIST_SLOTS)]


def row_presets(rng: random.Random, rows: range) -> list[list[int]]:
    return [[rng.randrange(6), rng.randrange(256), rng.randrange(256), rng.randrange(3), rng.randrange(2),
             rng.randrange(2), rng.randrange(256), rng.randrange(2), rng.randrange(256), rng.randrange(1000),
             rng.randrange(-300, 300), rng.randrange(-300, 300)] for _ in rows]


def buttons(rng: random.Random) -> int:
    if rng.random() < 0.3:
        return 0
    bits = (0x10, 0x20, 0x40, 0x80, 0x01, 0x100, 0x1000, 0x4000, 0x8000, 0x10000)
    value = 0
    for _ in range(rng.randrange(1, 3)):
        value |= rng.choice(bits)
    return value


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode}
    if mode == CAPACITIES:
        count = rng.randrange(25)
        offset = rng.randrange(count + 1)
        pick = rng.random()
        if pick < 0.2:
            previewed = 0
        elif pick < 0.35:
            previewed = rng.randrange(1, 16) << 4
        else:
            # The whole byte indexes the list; the high nibble stays inside it.
            low = rng.randrange(1, 9)
            previewed = rng.randrange((LIST_SLOTS - offset - low) // 16 + 1) << 4 | low
        value.update(edge=rng.randrange(-64, 256), previewed=previewed, controls=rng.randrange(4), count=count,
                     offset=offset,
                     capacities=[rng.randrange(65) for _ in range(7)],
                     counts=[rng.randrange(70) for _ in range(7)],
                     contained=[rng.choice((0, 0, rng.randrange(1, 6))) for _ in range(7)],
                     rows=loot_list(rng))
    elif mode == ROWS:
        count = rng.randrange(25)
        offset = rng.randrange(max(1, count - 7)) if rng.random() < 0.8 else rng.randrange(count + 1)
        value.update(x=rng.randrange(-64, 200), count=count, offset=offset, gradient=rng.randrange(201),
                     rows=loot_list(rng, invalid=True), inventory=inventory_bytes(rng),
                     presets=row_presets(rng, LOOT_ROWS))
    elif mode == DESCRIPTION:
        value.update(index=rng.randrange(LIST_SLOTS), rows=loot_list(rng))
    elif mode == LEAVE:
        value["init"] = int(rng.random() < 0.35)
        if value["init"]:
            rows = loot_list(rng)
            for row in rows:
                if rng.random() < 0.15:
                    row[0], row[1] = MISC, rng.randrange(CATEGORY_CAPACITIES[MISC])
            value.update(count=rng.choice((0, rng.randrange(1, 25))), claimed=rng.randrange(2),
                         entry=rng.randrange(2), state=rng.randrange(8), choice=rng.randrange(2),
                         kept=rng.randrange(NAME_COUNT), rows=rows, inventory=inventory_bytes(rng))
        else:
            value.update(state=rng.choice((0, 1, 2, 3, 3, 3, 4, 5, 6, 7, 8, 9)), choice=rng.randrange(2),
                         kept=rng.randrange(NAME_COUNT), claimed=rng.randrange(2), pressed=buttons(rng),
                         repeat=buttons(rng), information=rng.randrange(2), ready=rng.randrange(2),
                         cursor=rng.randrange(256), cursor_return=rng.randrange(256),
                         presets=row_presets(rng, PROMPT_ROWS))
    else:
        value["init"] = int(rng.random() < 0.35)
        if value["init"]:
            category = rng.randrange(9)
            index = rng.randrange(CATEGORY_CAPACITIES[MISC]) if category == MISC else rng.randrange(256)
            value.update(category=category, index=index, state=rng.randrange(4), menu_category=rng.randrange(8),
                         selected=rng.randrange(2), action=rng.randrange(3), cursor=rng.randrange(256),
                         inventory=inventory_bytes(rng), presets=row_presets(rng, PROMPT_ROWS))
        else:
            value.update(state=rng.choice((0, 1, 2, 2, 2, 3)), menu_category=rng.randrange(8),
                         selected=rng.randrange(2), action=rng.randrange(3), cursor=rng.randrange(256),
                         pressed=buttons(rng), repeat=buttons(rng), ready=rng.randrange(2),
                         cursor_return=rng.randrange(256), presets=row_presets(rng, PROMPT_ROWS))
    return value


# ------------------------------------------------------------------ fixtures
def put_list(machine: Machine, rows: list[list[int]]) -> list[int]:
    machine.put(LOOT_LIST_POINTER, LIST)
    for index, row in enumerate(rows):
        machine.store(LIST + index * 4, bytes(row))
    return [item for row in rows for item in row]


def put_inventory(machine: Machine, data: bytes) -> list[int]:
    machine.put(INVENTORY_POINTER, INVENTORY)
    machine.store(INVENTORY, data)
    machine.put(NAMES_POINTER, NAMES)
    return inventory_inputs(data)


def put_rows(machine: Machine, presets: list[list[int]], rows: range) -> list[int]:
    machine.map_zero(MENU_ROWS, MENU_ROW_BYTES * MENU_ROW_SLOTS)
    for row, fields in zip(rows, presets, strict=True):
        for (offset, size, _), item in zip(ROW_FIELDS, fields, strict=True):
            machine.put(MENU_ROWS + row * MENU_ROW_BYTES + offset, item, size)
    return [item for fields in presets for item in fields]


def read_rows(machine: Machine, rows: range) -> tuple[list[int], list[str]]:
    values, names = [], []
    for row in rows:
        for (offset, size, signed), name in zip(ROW_FIELDS, ROW_FIELD_NAMES, strict=True):
            values.append(machine.load(MENU_ROWS + row * MENU_ROW_BYTES + offset, size, signed))
            names.append(f"row {row} {name}")
    return values, names


def text_id(address: int) -> int:
    if INVENTORY <= address < INVENTORY + INVENTORY_BYTES:
        return address - INVENTORY
    if NAMES <= address < NAMES + NAME_BYTES * NAME_COUNT:
        return NAMES_TEXT_ID + address - NAMES
    return -1


def prompt_text(address: int) -> int:
    index, odd = divmod(address - MENU_TEXT, 2)
    if odd:
        return -1
    return PROMPT_TEXTS.get(index, 1000 + index)


def stub_rows(machine: Machine, calls: list[list[int]], prompt: bool) -> None:
    def row_address(row: int) -> int:
        return MENU_ROWS + row * MENU_ROW_BYTES if 0 <= row < MENU_ROW_SLOTS else MENU_ROWS

    def set_row(machine: Machine) -> None:
        sp = machine.r[29]
        text = machine.load(sp + 0x14)
        calls.append([CALL_SET_ROW, *(s32(machine.r[i]) for i in range(4, 8)), s32(machine.load(sp + 0x10)),
                      prompt_text(text) if prompt else text_id(text)])
        machine.r[2] = row_address(s32(machine.r[4]))

    def get_row(machine: Machine) -> None:
        calls.append([CALL_GET_ROW, s32(machine.r[4])])
        machine.r[2] = row_address(s32(machine.r[4]))

    machine.stub(SET_MENU_ITEM, set_row)
    machine.stub(GET_MENU_ITEM, get_row)


def stub_prompt(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    stub_rows(machine, calls, prompt=True)

    def idle(machine: Machine) -> None:
        calls.append([CALL_IDLE])
        machine.r[2] = value.get("ready", 0)

    def cursor(machine: Machine) -> None:
        calls.append([CALL_CURSOR, s32(machine.r[4]), s32(machine.r[5])])
        machine.r[2] = value.get("cursor_return", 0)

    def recorder(call: int, arguments: int = 0):
        return lambda machine: calls.append([call, *(s32(machine.r[4 + i]) for i in range(arguments))])

    machine.stub(INFO, lambda machine: calls.append([CALL_INFO, prompt_text(machine.r[4])]))
    machine.stub(INIT_TEXT_BOX, recorder(CALL_INIT_TEXT, 1))
    machine.stub(INIT_INFO_BOX, recorder(CALL_INIT_BOX))
    machine.stub(ROWS_IDLE, idle)
    machine.stub(FLY_OUT, recorder(CALL_FLY_OUT, 1))
    machine.stub(DISMISS, recorder(CALL_DISMISS))
    machine.stub(LEAVE_SOUND, recorder(CALL_LEAVE))
    machine.stub(CHANGE_SOUND, recorder(CALL_CHANGE))
    machine.stub(SELECT_SOUND, recorder(CALL_SELECT))
    machine.stub(SFX, recorder(CALL_SFX, 2))
    machine.stub(DRAW_CURSOR, cursor)
    machine.put(MENU_TEXT + 20, ACTION_TEXTS[0], 2)
    machine.put(MENU_TEXT + 24, ACTION_TEXTS[1], 2)
    machine.put(MENU_TEXT + 28, ACTION_TEXTS[2], 2)


def put_input(machine: Machine, value: dict) -> list[int]:
    machine.put(BUTTONS, value["pressed"])
    machine.put(REPEAT, value["repeat"])
    return [value["pressed"], value["repeat"]]


# ------------------------------------------------------------------ runners
def run_capacities(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    rows = put_list(machine, value["rows"])
    machine.put(LIST_OFFSET, value["offset"], 1)
    machine.put(LIST_COUNT, value["count"], 1)
    machine.put(PREVIEWED, value["previewed"], 1)
    machine.put(CONTROLS, value["controls"], 1)
    machine.store(CAPACITY_TABLE, bytes(value["capacities"]))
    for category in range(7):
        machine.put(HEADERS + 4 * category, HEADER_TEXT + 0x10 * category)

    def label(address: int) -> tuple[int, int]:
        if address in LABELS:
            return LABELS[address], 0
        category, rest = divmod(address - HEADER_TEXT, 0x10)
        if rest == 0 and 0 <= category < 7:
            return CATEGORY_LABEL, category
        return 99, s32(address)

    def text(machine: Machine) -> None:
        calls.append([CALL_LABEL, *label(machine.r[4]), machine.r[5] & MASK])

    def colored(machine: Machine) -> None:
        calls.append([CALL_COLORED, *label(machine.r[4]), machine.r[5] & MASK, machine.r[6] & MASK])

    def count(machine: Machine) -> None:
        calls.append([CALL_COUNT, s32(machine.r[4])])
        machine.r[2] = value["counts"][machine.r[4] % 7]

    def contained(machine: Machine) -> None:
        row, rest = divmod(machine.r[5] - LIST, 4)
        calls.append([CALL_CONTAINED, s32(machine.r[4]), row if rest == 0 else -1])
        machine.r[2] = value["contained"][machine.r[4] % 7]

    def integer(machine: Machine) -> None:
        calls.append([CALL_INTEGER, s32(machine.r[4]), machine.r[5] & MASK, machine.r[6] & MASK])

    machine.stub(RENDER_TEXT, text)
    machine.stub(RENDER_TEXT_COLOR, colored)
    machine.stub(ITEM_COUNT, count)
    machine.stub(CONTAINED, contained)
    machine.stub(RENDER_INT, integer)
    machine.stub(BUTTON_BACKGROUND,
                 lambda machine: calls.append([CALL_BACKGROUND, *(s32(machine.r[i]) for i in range(4, 8))]))
    machine.stub(BUTTON, lambda machine: calls.append([CALL_BUTTON, *(s32(machine.r[i]) for i in range(4, 7))]))
    machine.r[4] = value["edge"] & MASK
    machine.execute(entry)
    inputs = [value["edge"], value["previewed"], value["controls"], value["count"], value["offset"],
              *value["capacities"], *value["counts"], *value["contained"], *rows]
    return [0], ["refused"], inputs


def run_rows(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    rows = put_list(machine, value["rows"])
    inventory = put_inventory(machine, value["inventory"])
    presets = put_rows(machine, value["presets"], LOOT_ROWS)
    machine.put(LIST_OFFSET, value["offset"], 1)
    machine.put(LIST_COUNT, value["count"], 1)
    machine.put(GRADIENT_INDEX, value["gradient"], 1)
    stub_rows(machine, calls, prompt=False)

    def memzero(machine: Machine) -> None:
        machine.store(machine.r[4], bytes(machine.r[5] & MASK))

    machine.stub(MEMZERO, memzero)
    machine.r[4] = value["x"] & MASK
    machine.execute(entry)
    values = [0, machine.load(GRADIENT_INDEX, 1)]
    names = ["refused", "gradient index"]
    for index in range(LIST_SLOTS):
        values.append(machine.load(LIST + index * 4 + 2, 1))
        names.append(f"loot row {index} slide")
    row_values, row_names = read_rows(machine, LOOT_ROWS)
    inputs = [value["x"], value["count"], value["offset"], value["gradient"], *rows, *inventory, *presets]
    return values + row_values, names + row_names, inputs


def run_description(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    rows = put_list(machine, value["rows"])
    machine.put(INVENTORY_POINTER, INVENTORY)
    machine.put(BACKUP_POINTER, BACKUP)
    machine.put(STRING_BUFFER, TEXT + 0x1000)
    inventories = {MAIN_INVENTORY: 0, BACKUP: 1, INVENTORY: 2}
    index = value["rows"][value["index"]][1]

    def copy(machine: Machine) -> None:
        calls.append([CALL_COPY, inventories.get(machine.r[4], -1), inventories.get(machine.r[5], -1),
                      s32(machine.r[6])])

    def formatter(category: int):
        def format_row(machine: Machine) -> None:
            calls.append([CALL_FORMAT, category, s32(machine.r[4] - MAIN_INVENTORY)])
            machine.put(machine.r[5] + 4, TEXT + category * 0x100 + index)
            machine.put(machine.r[6], ROW_TYPE)
        return format_row

    machine.stub(COPY, copy)
    for category, address in enumerate(FORMATTERS):
        machine.stub(address, formatter(category))
    machine.stub(INFO, lambda machine: calls.append([CALL_INFO, s32(machine.r[4] - TEXT)]))
    machine.r[4] = value["index"]
    machine.execute(entry)
    return [0], ["refused"], [value["index"], *rows]


def run_leave(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    machine.put(LEAVE_STATE, value["state"], 1)
    machine.put(LEAVE_CHOICE, value["choice"], 1)
    machine.put(LEAVE_NAME, NAMES + value["kept"] * NAME_BYTES)
    machine.put(NAMES_POINTER, NAMES)
    machine.put(CLAIMED, value["claimed"], 1)
    if value["init"]:
        rows = put_list(machine, value["rows"])
        inventory = put_inventory(machine, value["inventory"])
        machine.put(LIST_COUNT, value["count"], 1)
        machine.put(LOOT_ENTRY, 0x80123450 if value["entry"] else 0)
        machine.r[4] = 1
        machine.execute(entry)
        values = [0, s32(machine.r[2]), machine.load(LEAVE_STATE, 1), machine.load(LEAVE_CHOICE, 1),
                  s32(machine.load(LEAVE_NAME) - NAMES)]
        names = ["refused", "result", "state", "choice", "kept"]
        inputs = [1, value["count"], value["claimed"], value["entry"], value["state"], value["choice"],
                  value["kept"], *rows, *inventory]
        return values, names, inputs
    presets = put_rows(machine, value["presets"], PROMPT_ROWS)
    pressed = put_input(machine, value)
    machine.put(INFORMATION, value["information"], 1)
    machine.put(LEAVE_CURSOR, value["cursor"], 1)
    machine.put(STRING0, 0)
    stub_prompt(machine, value, calls)
    machine.r[4] = 0
    machine.execute(entry)
    string = machine.load(STRING0)
    values = [0, s32(machine.r[2]), machine.load(LEAVE_STATE, 1), machine.load(LEAVE_CHOICE, 1),
              machine.load(LEAVE_CURSOR, 1), s32(string - NAMES) if string else -1]
    names = ["refused", "result", "state", "choice", "cursor", "string"]
    row_values, row_names = read_rows(machine, PROMPT_ROWS)
    inputs = [0, value["state"], value["choice"], value["kept"], value["claimed"], *pressed, value["information"],
              value["ready"], value["cursor"], value["cursor_return"], *presets]
    return values + row_values, names + row_names, inputs


def run_organize(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    machine.put(ORGANIZE_STATE, value["state"], 1)
    machine.put(ORGANIZE_CATEGORY, value["menu_category"], 1)
    machine.put(ORGANIZE_SELECTED, value["selected"], 1)
    machine.put(ORGANIZE_ACTION, value["action"], 1)
    machine.put(ORGANIZE_CURSOR, value["cursor"], 1)
    presets = put_rows(machine, value["presets"], PROMPT_ROWS)
    stub_prompt(machine, value, calls)
    statics = [value["state"], value["menu_category"], value["selected"], value["action"], value["cursor"]]
    if value["init"]:
        inventory = put_inventory(machine, value["inventory"])
        machine.store(LIST, bytes((value["category"], value["index"], 0, 0)))
        machine.r[4] = LIST
        inputs = [1, value["category"], value["index"], *statics, *inventory, *presets]
    else:
        pressed = put_input(machine, value)
        machine.r[4] = 0
        inputs = [0, *statics, *pressed, value["ready"], value["cursor_return"], *presets]
    machine.execute(entry)
    values = [0, s32(machine.r[2]), machine.load(ORGANIZE_STATE, 1), machine.load(ORGANIZE_CATEGORY, 1),
              machine.load(ORGANIZE_SELECTED, 1), machine.load(ORGANIZE_ACTION, 1),
              machine.load(ORGANIZE_CURSOR, 1)]
    names = ["refused", "result", "state", "category", "selected", "action", "cursor"]
    row_values, row_names = read_rows(machine, PROMPT_ROWS)
    return values + row_values, names + row_names, inputs


RUNNERS = {CAPACITIES: run_capacities, ROWS: run_rows, DESCRIPTION: run_description, LEAVE: run_leave,
           ORGANIZE: run_organize}


def run(machine: Machine, value: dict, entry: int) -> tuple:
    calls: list[list[int]] = []
    runner = RUNNERS[value["mode"]]
    values, names, inputs = runner(machine, value, entry, calls)
    return recorded_observation(machine, values, names, calls), [value["mode"], *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    # The originals spill their arguments to the caller's home slots.
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK + 0x10)]
    mode = value["mode"]
    if mode == ROWS:
        allowed += [(LIST, LIST + 4 * LIST_SLOTS), (GRADIENT_INDEX, GRADIENT_INDEX + 1),
                    (MENU_ROWS, MENU_ROWS + MENU_ROW_BYTES * MENU_ROW_SLOTS)]
    if mode == LEAVE:
        allowed += [(LEAVE_STATE, LEAVE_NAME + 4), (LEAVE_CURSOR, LEAVE_CURSOR + 1), (STRING0, STRING0 + 4),
                    (MENU_ROWS, MENU_ROWS + MENU_ROW_BYTES * MENU_ROW_SLOTS)]
    if mode == ORGANIZE:
        allowed += [(ORGANIZE_STATE, ORGANIZE_ACTION + 1), (ORGANIZE_CURSOR, ORGANIZE_CURSOR + 1),
                    (MENU_ROWS, MENU_ROWS + MENU_ROW_BYTES * MENU_ROW_SLOTS)]
    return allowed


def describe(image, results) -> list[str]:
    mode = results[0][0]["mode"] if results else None
    lines = []
    if mode == CAPACITIES:
        previewed = sum(1 for value, _, _ in results if value["previewed"] & 0xF)
        lines.append(f"{previewed} of {len(results)} vectors previewed one row")
    if mode == ROWS:
        long = sum(1 for value, _, _ in results if value["count"] > 8)
        lines.append(f"{long} of {len(results)} vectors listed more than eight rows")
    if mode == DESCRIPTION:
        kinds: dict[int, int] = {}
        for value, _, _ in results:
            category = value["rows"][value["index"]][0]
            kinds[category] = kinds.get(category, 0) + 1
        lines.append(f"categories {dict(sorted(kinds.items()))}")
    if mode in (LEAVE, ORGANIZE):
        opened = sum(1 for value, _, _ in results if value["init"])
        left = sum(1 for _, observation, _ in results if observation["values"][1] != 0)
        lines.append(f"{opened} of {len(results)} vectors opened the prompt; {left} returned a choice")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict, vectors: int = 1024) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return {**mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]),
            "VECTORS": vectors, "MUTANT_VECTORS": 256, "INCLUDES": INCLUDES}
