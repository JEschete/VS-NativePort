"""Shared harness for the map marker and label differentials (MENU5.PRG 4D8.c:106-117, 2158-2270 and
2403-2479, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::map core
(MapMarkers.cpp) in map_markers_driver.cpp. Every vector draws the screen position (biased to the 16-bit
wrap and to the leader-line pivots at x = 170 and 186), the icon RECT tables, a small self-indexed name
table, the area visit flags, _mapLabelY, the label queue and its count (0 to 7), the door state flag and
the sigil-door answer. The original writes its packets at a fixture primitive cursor and links them
through a stubbed AddPrim; the menu-row, sprite and sigil callees are stubbed and recorded. Both sides
print the result, _mapLabelY, the label queue and count, the sprite's texture and colour words, the
menu row's background width and row 0's state, the primitive cursor's advance, every linked packet's
fields and the recorded calls.

The typed cores refuse an icon, name or area flag word outside its table and a full label queue, where
the original reads or writes past them; vectors stay inside (MapMarkersTests.cpp covers refusals).
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

MODULE_BASES["MENU/MENU5.PRG"] = 0x80102800

MODULES = ("MENU/MENU5.PRG",)
DRIVER = "map_markers_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\MapMarkers.cpp",)

SCRATCH = 0x1F800000  # vs_scratch: primitive cursor, ordering table, overlay table
PRIMS = 0x80500000
PRIMS_BYTES = 0x100
OT = 0x80510000
OVERLAY = 0x80520000
CALLER_PRIM = 0x80528000
SPRITE = 0x80530000
SPRITE_BYTES = 24
MENU_ITEM = 0x80540000
OTHER_ITEM = 0x80540100
ITEM_BYTES = 0x40
ICON_RECTS = 0x80108CE4  # _iconRects
UPPER_RECTS = 0x80108D04  # D_80108D04
LOWER_RECTS = 0x80108D14  # D_80108D14
AREA_NAMES = 0x80108630  # _connectingMaps
DOOR_NAMES = 0x801088B0  # _doors
LABEL_Y = 0x80108E48  # _mapLabelY
LABELS = 0x80108E74  # _mapLabels
LABEL_COUNT = 0x80108EB4  # _mapLabelCount
STATE_FLAGS = 0x80061598 + 0x33F  # vs_main_stateFlags.unk33F
AREA_FLAGS = 0x8005FFD8 + 0x40  # vs_main_mapStatus.areaFlags

ADD_PRIM = 0x8002B4D4
SET_SPRITE = 0x800C0224  # vs_battle_setSpriteDefaultTexPage
SET_MENU_ITEM = 0x800C8E5C  # vs_battle_setMenuItem
RENDER_MENU_ITEM = 0x800C9078  # vs_battle_renderMenuItem
GET_MENU_ITEM = 0x800C8E48  # vs_battle_getMenuItem
SIGIL_UNLOCKED = 0x8008E7BC  # func_8008E7BC
INSERT_LINE = 0x80107A9C  # _insertLine
ADD_LABEL = 0x80108274  # _addMapLabel

ASHLEY, FEATURE, DOOR_STATE, DOOR_LABEL, LINE, AREA_LABEL, QUEUE, SHOULD = range(8)

QUAD_SLOT = 28
LINE_SLOT = 32
QUAD_BYTES = 40
LINE_BYTES = 16
LABEL_CAPACITY = 8
NAME_ENTRIES = 40
AREA_FLAG_WORDS = 2
UNVISITED = 32
ITEM_PRESET = 0xEE
STATE_PRESET = 0xFF
CALL_ADD_PRIM, CALL_SPRITE, CALL_SET_ITEM, CALL_RENDER_ITEM, CALL_GET_ITEM, CALL_SIGIL = range(1, 7)
TABLE_DOORS, TABLE_AREAS, TABLE_OTHER = range(3)
LIST_OVERLAY, LIST_CALLER = range(2)

EDGE_COORDINATES = (0, 1, -1, 7, 8, 9, 0x7FFF, -0x8000, 0x8000, 0xFFFF, 0x12345, -0x12345, 0x7FFFFFFF,
                    -0x80000000, 160, 120)
PIVOT_EDGES = (166, 167, 168, 169, 170, 171, 172, 173, 182, 183, 184, 185, 186, 187, 188, 189, 190)


def coordinate(rng: random.Random) -> int:
    return rng.choice((*EDGE_COORDINATES, *PIVOT_EDGES, rng.randrange(-400, 600), s32(rng.getrandbits(32))))


def half(rng: random.Random) -> int:
    return rng.choice((0, 1, -1, 7, 8, 9, 17, 0xFF, 0x100, 0x7FFF, -0x8000, rng.randrange(-0x8000, 0x8000),
                       rng.randrange(-64, 300)))


def rect(rng: random.Random) -> list[int]:
    return [half(rng) for _ in range(4)]


def vector(rng: random.Random, number: int, mode: int) -> dict:
    count = rng.randrange(LABEL_CAPACITY)
    value = {
        "mode": mode,
        "x": coordinate(rng),
        "y": coordinate(rng),
        "x1": coordinate(rng),
        "y1": coordinate(rng),
        "id": 0,
        "kind": rng.choice((0, 1, -1, 2, 0x10000, s32(rng.getrandbits(32)))),
        "icons": [rect(rng) for _ in range(4)],
        "upper": [rect(rng) for _ in range(2)],
        "lower": [rect(rng) for _ in range(2)],
        "names": [rng.randrange(NAME_ENTRIES) for _ in range(NAME_ENTRIES)],
        "area_flags": [rng.choice((0, 0xFFFFFFFF, rng.getrandbits(32))) for _ in range(AREA_FLAG_WORDS)],
        "label_y": rng.choice((188, 172, 0, -8, 0x7FFFFFF8, -0x80000000, s32(rng.getrandbits(32)))),
        "labels": [[half(rng) for _ in range(4)] for _ in range(LABEL_CAPACITY)],
        "count": count,
        "flag": rng.choice((0, 1, 2, 3, 0x7F, 0x80, 0xFE, 0xFF, rng.randrange(256))),
        "sigil": rng.choice((0, 1, 2, -1)),
        "zone": rng.randrange(-4, 40),
        "map": rng.randrange(-4, 40),
        "caller": rng.choice((0, CALLER_PRIM)),
    }
    if mode == ASHLEY:
        value["id"] = rng.choice((0, 1, -1, 3, 4, 0xF, 0x10000000, s32(rng.getrandbits(32))))
        value["x"] = s32(rng.getrandbits(32))
        value["y"] = rng.choice((0, 0x80, 0xFF, -1, s32(rng.getrandbits(32))))
    elif mode == FEATURE:
        value["id"] = number % 4
    elif mode in (DOOR_STATE, SHOULD):
        value["id"] = rng.choice((*range(0x3F, 0x49), *range(0, 4), rng.randrange(0x41)))
    elif mode in (DOOR_LABEL, AREA_LABEL):
        value["id"] = rng.randrange(NAME_ENTRIES)
    elif mode == QUEUE:
        value["id"] = rng.choice((0, 1, -1, 0x7F, 0x8000, 0x12345, s32(rng.getrandbits(32))))
    return value


def put_rects(machine: Machine, address: int, rects: list[list[int]]) -> None:
    for index, fields in enumerate(rects):
        for field, item in enumerate(fields):
            machine.put(address + 8 * index + 2 * field, item, 2)


def decode_quad(machine: Machine, at: int) -> list[int]:
    values = [machine.load(at + 3, 1), machine.load(at + 7, 1)]
    values += [machine.load(at + offset, 2, signed=True) for offset in (8, 10, 0x10, 0x12, 0x18, 0x1A, 0x20, 0x22)]
    values += [machine.load(at + offset, 1) for offset in (0xC, 0xD, 0x14, 0x15, 0x1C, 0x1D, 0x24, 0x25)]
    return values + [machine.load(at + 0xE, 2), machine.load(at + 0x16, 2)]


def decode_line(machine: Machine, at: int) -> list[int]:
    values = [machine.load(at + offset, 1) for offset in (3, 4, 5, 6, 7)]
    return values + [machine.load(at + offset, 2, signed=True) for offset in (8, 10, 12, 14)]


def table_of(address: int) -> tuple[int, int]:
    for code, base in ((TABLE_DOORS, DOOR_NAMES), (TABLE_AREAS, AREA_NAMES)):
        if base <= address < base + 2 * NAME_ENTRIES and (address - base) % 2 == 0:
            return code, (address - base) // 2
    return TABLE_OTHER, address


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    machine.map_zero(PRIMS, PRIMS_BYTES)
    machine.map_zero(SPRITE, SPRITE_BYTES)
    machine.map_zero(MENU_ITEM, ITEM_BYTES)
    machine.map_zero(OTHER_ITEM, ITEM_BYTES)
    machine.put(MENU_ITEM + 2, ITEM_PRESET, 1)
    machine.put(OTHER_ITEM, STATE_PRESET, 1)
    machine.put(SCRATCH, PRIMS)
    machine.put(SCRATCH + 4, OT)
    machine.put(SCRATCH + 8, OVERLAY)
    put_rects(machine, ICON_RECTS, value["icons"])
    put_rects(machine, UPPER_RECTS, value["upper"])
    put_rects(machine, LOWER_RECTS, value["lower"])
    for index, entry_offset in enumerate(value["names"]):
        machine.put(DOOR_NAMES + 2 * index, entry_offset, 2)
        machine.put(AREA_NAMES + 2 * index, entry_offset, 2)
    for index, flags in enumerate(value["area_flags"]):
        machine.put(AREA_FLAGS + 4 * index, flags)
    machine.put(LABEL_Y, value["label_y"])
    for index, label in enumerate(value["labels"]):
        for field, item in enumerate(label):
            machine.put(LABELS + 8 * index + 2 * field, item, 2)
    machine.put(LABEL_COUNT, value["count"])
    if mode in (DOOR_STATE, SHOULD):
        machine.put(STATE_FLAGS + value["id"], value["flag"], 1)
    machine.follow.update((INSERT_LINE, ADD_LABEL))
    calls: list[list[int]] = []
    prims: list[tuple[int, int]] = []

    def add_prim(machine: Machine) -> None:
        slot, prim = machine.r[4] - OT, machine.r[5]
        prims.append((slot, prim))
        calls.append([CALL_ADD_PRIM, s32(slot), s32(prim - PRIMS)])

    def set_sprite(machine: Machine) -> None:
        target = machine.r[7]
        listed = {OVERLAY: LIST_OVERLAY, CALLER_PRIM: LIST_CALLER}.get(target, s32(target))
        calls.append([CALL_SPRITE, s32(machine.r[4]), s32(machine.r[5]), s32(machine.r[6]), listed])
        machine.put(SCRATCH, SPRITE + SPRITE_BYTES)
        machine.r[2] = SPRITE

    def set_item(machine: Machine) -> None:
        sp = machine.r[29]
        table, offset = table_of(machine.load(sp + 0x14))
        calls.append([CALL_SET_ITEM, *(s32(machine.r[i]) for i in range(4, 8)),
                      s32(machine.load(sp + 0x10)), table, s32(offset)])
        machine.r[2] = MENU_ITEM

    def render_item(machine: Machine) -> None:
        calls.append([CALL_RENDER_ITEM, int(machine.r[4] == MENU_ITEM)])

    def get_item(machine: Machine) -> None:
        calls.append([CALL_GET_ITEM, s32(machine.r[4])])
        machine.r[2] = OTHER_ITEM

    def sigil(machine: Machine) -> None:
        calls.append([CALL_SIGIL, s32(machine.r[4]), s32(machine.r[5]), s32(machine.r[6])])
        machine.r[2] = value["sigil"] & 0xFFFFFFFF

    machine.stub(ADD_PRIM, add_prim)
    machine.stub(SET_SPRITE, set_sprite)
    machine.stub(SET_MENU_ITEM, set_item)
    machine.stub(RENDER_MENU_ITEM, render_item)
    machine.stub(GET_MENU_ITEM, get_item)
    machine.stub(SIGIL_UNLOCKED, sigil)
    arguments = {
        ASHLEY: (value["id"], value["x"], value["y"], value["caller"]),
        FEATURE: (value["id"], value["x"], value["y"]),
        DOOR_STATE: (value["x"], value["y"], value["id"]),
        DOOR_LABEL: (value["x"], value["y"], value["id"]),
        LINE: (value["x"], value["y"], value["x1"], value["y1"]),
        AREA_LABEL: (value["x"], value["y"], value["id"]),
        QUEUE: (value["x"], value["y"], value["kind"], value["id"]),
        SHOULD: (value["id"], value["zone"], value["map"]),
    }[mode]
    for register, item in enumerate(arguments, start=4):
        machine.r[register] = item & 0xFFFFFFFF
    machine.r[2] = 0
    machine.execute(entry)

    cursor = machine.load(SCRATCH)
    values = [
        s32(machine.r[2]) if mode == SHOULD else 0,
        machine.load(LABEL_Y, 4, signed=True),
        machine.load(LABEL_COUNT, 4, signed=True),
        *(machine.load(LABELS + 2 * field, 2, signed=True) for field in range(4 * LABEL_CAPACITY)),
        machine.load(SPRITE + 0x10),
        machine.load(SPRITE + 0x8),
        machine.load(MENU_ITEM + 2, 1),
        machine.load(OTHER_ITEM, 1),
        s32(cursor - (SPRITE if cursor >= SPRITE else PRIMS)),
    ]
    names = ["result", "label y", "label count", *(f"label {i // 4} field {i % 4}" for i in range(32)),
             "sprite texture", "sprite colour", "row background", "row 0 state", "cursor advance"]
    for number, (slot, prim) in enumerate(prims):
        decoded = decode_quad(machine, prim) if slot == QUAD_SLOT else decode_line(machine, prim)
        values += decoded
        names += [f"packet {number} field {field}" for field in range(len(decoded))]
    inputs = [value["x"], value["y"], value["x1"], value["y1"], value["id"], value["kind"],
              value["label_y"], value["count"], value["flag"], value["sigil"], value["zone"], value["map"],
              value["caller"], *(item for icon in value["icons"] for item in icon),
              *(item for icon in value["upper"] for item in icon),
              *(item for icon in value["lower"] for item in icon),
              *value["names"], *(s32(flags) for flags in value["area_flags"]),
              *(item for label in value["labels"] for item in label)]
    return recorded_observation(machine, values, names, calls), [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    mode = value["mode"]
    if mode in (FEATURE, DOOR_STATE, DOOR_LABEL, LINE, AREA_LABEL):
        allowed += [(SCRATCH, SCRATCH + 4), (PRIMS, PRIMS + PRIMS_BYTES)]
    if mode == ASHLEY:
        allowed += [(SPRITE, SPRITE + SPRITE_BYTES)]
    if mode in (DOOR_STATE, QUEUE):
        count = value["count"]
        allowed += [(LABELS + 8 * count, LABELS + 8 * count + 8), (LABEL_COUNT, LABEL_COUNT + 4)]
    if mode in (DOOR_LABEL, AREA_LABEL):
        allowed += [(LABEL_Y, LABEL_Y + 4), (MENU_ITEM + 2, MENU_ITEM + 3), (OTHER_ITEM, OTHER_ITEM + 1)]
    return allowed


def describe(image, results) -> list[str]:
    mode = results[0][0]["mode"] if results else None
    lines = []
    if mode in (DOOR_LABEL, AREA_LABEL):
        bent = sum(1 for value, _, _ in results if value["x"] in PIVOT_EDGES)
        lines.append(f"{bent} of {len(results)} vectors started at a pivot edge")
    if mode == AREA_LABEL:
        hidden = sum(1 for value, _, _ in results
                     if not value["area_flags"][value["id"] >> 5] >> (value["id"] & 31) & 1)
        lines.append(f"{hidden} vectors showed the unvisited name")
    if mode == SHOULD:
        drawn = sum(1 for _, observation, _ in results if observation["values"][0])
        asked = sum(1 for _, observation, _ in results if observation["calls"])
        lines.append(f"{drawn} of {len(results)} vectors drew the marker, {asked} asked the sigil door")
    if mode in (DOOR_STATE, ASHLEY):
        odd = sum(1 for value, _, _ in results if value["flag"] & 1)
        callers = sum(1 for value, _, _ in results if value["caller"])
        lines.append(f"{odd} vectors had an odd state flag, {callers} a caller primitive")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
