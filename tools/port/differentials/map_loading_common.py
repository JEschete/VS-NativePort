"""Shared harness for the map area list, background and scene-map loading differentials (MENU5.PRG
4D8.c:129-238, 448-520 and 979-1045, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::map core
(MapLoading.cpp) in map_loading_driver.cpp. The callees (the menu rows, the disc queue, the heap, the
image uploads and the scene preparation) are stubs that record their arguments; the driver's hosts
record the same words, computing the pointers the Live bindings pass (the menu text's area-name
pointers, the CD file record, the upload source) from the symbolic slot, image and table addresses
the vector supplies. Every global the original reads is drawn and every global it may write is
prefilled with a value it would not write, then printed.

Vectors keep the remembered area row at -1 or above (a row below -1 makes the original write before
its row types; the typed core marks none) and the current scene id non-negative (the original reads
before _sceneArmFiles and mapPaling; the typed core refuses). MapLoadingTests.cpp covers both.
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

MODULE_BASES["MENU/MENU5.PRG"] = 0x80102800

MODULES = ("MENU/MENU5.PRG",)
DRIVER = "map_loading_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\MapLoading.cpp",)

CONFIGURE, POLL, NAME, READY, BACKGROUND, SCENE, SELECT = range(7)

# Symbolic addresses the stubs return and the driver prints.
MENU_TEXT = 0x80590000
ROW = 0x805A0000
ROW_BYTES = 0x20
SLOT = 0x80560000
OLD_SLOT = 0x80561000
BUFFER = 0x80570000
OLD_BUFFER = 0x80571000
ROOM_NAMES = 0x80580000
OLD_ROOM_NAMES = 0x80581000

# Globals.
CURRENT_MAP_ROW = 0x801083F8  # _currentMapRow
CURSOR = 0x800F4EE8  # D_800F4EE8.cursorMemories[0..1]
SETTING = 0x80060021  # vs_main_settings.cursorMemory
AREA_FLAGS = 0x80060018  # vs_main_mapStatus.areaFlags[0]
ROOM_FLAGS = 0x8005FFDC  # vs_main_mapStatus.roomFlags[1]
MAP_NAMES = 0x801083FC  # _mapNames
ARM_FILES = 0x80102B00  # _sceneArmFiles
PALING = 0x800616CD  # vs_main_stateFlags.mapPaling
SCENE_BUFFER = 0x800F1BC0  # vs_battle_sceneBuffer
YAW = 0x1F800058  # scratch camera yaw
PROJECTION = 0x8005E248  # the projection distance SetFogNear reads
SELECTION_STEP = 0x80108D54
SELECTION_SELECTED = 0x80108D58
SCENE_ROWS = 0x80108DC4  # _sceneRowIdMap
BACKGROUND_SLOT = 0x80108D24
BACKGROUND_IMAGE = 0x80108D28
BACKGROUND_STEP = 0x80108D2C
SCENE_SLOT = 0x80108DBC
ROOM_NAMES_POINTER = 0x80108D98  # _roomNamesTable
CURRENT_SCENE = 0x80108D80
CURRENT_SCENE_ROW = 0x80108D90

# (name, address, size) of the map view globals _loadArmFile resets, in SceneMapView's order.
VIEW = (
    ("trigger hide ui", 0x80108D8C, 4),
    ("hide ui", 0x80108D94, 4),
    ("camera angle x", 0x80108DA4, 2),
    ("camera angle y", 0x80108DA6, 2),
    ("camera angle z", 0x80108DA8, 2),
    ("camera angle pad", 0x80108DAA, 2),
    ("camera position x", 0x80108DAC, 2),
    ("camera position y", 0x80108DAE, 2),
    ("camera position z", 0x80108DB0, 2),
    ("camera position pad", 0x80108DB2, 2),
    ("current room", 0x80108D64, 4),
    ("current scene", CURRENT_SCENE, 4),
    ("current scene row", CURRENT_SCENE_ROW, 4),
    ("menu state", 0x80108D60, 4),
    ("background brightness", 0x80108D68, 4),
    ("menu frames", 0x80108D6C, 4),
    ("geometry x", 0x80108D70, 4),
    ("geometry y", 0x80108D74, 4),
    ("quit state", 0x80108E44, 4),
    ("camera distance", 0x80108D7C, 4),
    ("menu commands state", 0x80108D9C, 2),
    ("ui animation", 0x80108D9E, 2),
    ("is paling", 0x80108D88, 4),
)

# Callees.
SET_MENU_ROWS = 0x801005E0
SELECTED_ROW = 0x801008C8
CLEAR_MENU = 0x800FA8E0
FLY_OUT = 0x800FA92C
SET_MENU_ITEM = 0x800C8E5C
MENU_READY = 0x800FA9D0
ALLOCATE_SLOT = 0x80044B10
ALLOCATE_HEAP = 0x80043E3C
ENQUEUE = 0x80044BC4
FREE_SLOT = 0x80044B80
LOAD_CLUT = 0x80048A64
RENDER_IMAGE = 0x800CCDA8
BACKGROUND_PRIORITY = 0x80100414
FREE_HEAP = 0x80043C60
BZERO = 0x80049110
CURRENT_SCENE_ID = 0x8008D400
ROOM_NAMES_OF = 0x80098F4C
RESTORE_VISITS = 0x8008B4D8
SCALE_ROOMS = 0x801060E0
INITIAL_ROOM = 0x80106570
SNAP_TO_ROOM = 0x80105B18
SET_FOG_NEAR = 0x80041954
SET_FAR_COLOR = 0x80041558
SHOW_AREA_NAME = 0x80102F30
CONFIGURE_ENTRY = 0x80102D90
POLL_ENTRY = 0x80102ED8

SCENES = 32
STACK = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)


def word(rng: random.Random) -> int:
    return rng.choice((0, 1, -1, 2, 0x7FFFFFFF, -0x80000000, s32(rng.randrange(1 << 32))))


def area_value(rng: random.Random, count: int) -> int:
    """An area argument: a row (with or without the no-current flag) or a wider positive word."""
    row = rng.choice((rng.randrange(max(count, 1) + 2), rng.randrange(0x80), 0, 7, 8, 9))
    return rng.choice((row, row, row | 0x80, rng.randrange(0x100), rng.randrange(0x1000) & ~0x80))


def remembered_row(rng: random.Random, count: int) -> int:
    return rng.choice((-1, -1, rng.randrange(max(count, 1) + 2), rng.randrange(0x200)))


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode}
    if mode == CONFIGURE:
        count = rng.choice((*range(-1, 21), 0, 8, 9, 9, 9, 10))
        value.update(count=count, area=area_value(rng, count), row=remembered_row(rng, count),
                     cursor=[rng.randrange(256), rng.randrange(256)], setting=rng.randrange(256))
    elif mode == POLL:
        value.update(selected=rng.choice((-1, -1, -2, -3, 0, 1, rng.randrange(20), word(rng))),
                     cursor=rng.randrange(256))
    elif mode == NAME:
        value.update(name=word(rng), color=rng.choice((0, 1, 2, 7, 0x1FF, word(rng))))
    elif mode == READY:
        value.update(ready=word(rng))
    elif mode == BACKGROUND:
        value.update(begin=rng.choice((0, 0, 1, word(rng))), step=rng.choice((0, 0, 1, 1, 2, -1, word(rng))),
                     state=rng.choice((4, 4, 0, 1, 3, 5, -4, rng.randrange(-0x8000, 0x8000))))
    elif mode == SCENE:
        current = rng.randrange(SCENES)
        value.update(
            buffer=rng.choice((0, OLD_BUFFER)),
            room_flags=rng.choice((0, 0x800000, 0xC00000, 0x400000, s32(rng.randrange(1 << 32)))),
            yaw=rng.choice((0, 0x800, 0xFFFF, 0x7FFF, 0x8000, 0x1000, rng.randrange(0x10000))),
            scene=rng.choice((*range(SCENES), 31, 31, 32, 33, 0x7FFFFFFF, rng.randrange(SCENES))),
            state=rng.choice((4, 4, 0, 1, 3, -4)),
            initial=rng.choice((0, 1, 5, -1, word(rng))),
            projection=word(rng),
            view=[rng.randrange(-0x8000, 0x8000) if size == 2 else word(rng) for _, _, size in VIEW],
            paling=[rng.randrange(256) for _ in range(SCENES)],
            sizes=[rng.choice((0, 0x1000, rng.randrange(0x40000))) for _ in range(SCENES)],
            names=[rng.randrange(0x400) for _ in range(SCENES)],
        )
        value["view"][11] = current
        if number % 8 == 3:
            value.update(buffer=0, scene=SCENES - 1)
    elif mode == SELECT:
        visited = rng.choice((0, 1, 1 << rng.randrange(32), rng.randrange(1 << 32),
                              rng.randrange(1 << 32) & rng.randrange(1 << 32), 0xFFFFFFFF, 0xFFFFFFFE))
        listed = [scene for scene in range(1, SCENES) if visited >> scene & 1]
        current = rng.choice((*listed, *listed, 0, rng.randrange(34))) if listed else rng.randrange(34)
        value.update(
            initialize=rng.choice((0, 0, 1, -1, 2)),
            step=rng.choice((0, 0, 1, 1, 2, -1)),
            selected=word(rng),
            visited=visited,
            current_scene=current,
            current_scene_row=rng.choice((0, current, 5, word(rng))),
            row=remembered_row(rng, len(listed)),
            cursor=[rng.randrange(256), rng.randrange(256)],
            setting=rng.randrange(256),
            selected_row=rng.choice((-1, -1, -2, -3, 0, 1, rng.randrange(20), word(rng))),
            scene_rows=[word(rng) for _ in range(SCENES)],
            names=[rng.randrange(0x400) for _ in range(SCENES)],
        )
    return value


def record(calls: list[list[int]], *words: int) -> None:
    calls.append([s32(item) for item in words])


def menu_stubs(machine: Machine, value: dict, calls: list[list[int]], text_words: bool) -> None:
    """The menu row callees; setMenuRows records the row types and, with `text_words`, the menu text."""

    def rows(machine: Machine) -> None:
        count = s32(machine.r[4])
        types = [machine.load(machine.r[7] + 4 * index, 4, signed=True) for index in range(max(count, 0))]
        text = ([machine.load(machine.r[6] + 4 * index, 4, signed=True) for index in range(2 * max(count, 0))]
                if text_words else [s32(machine.r[6])])
        record(calls, 1, count, machine.r[5], *text, machine.byte(SETTING), *types)

    def selected(machine: Machine) -> None:
        record(calls, 0)
        machine.r[2] = value["selected_row"] & 0xFFFFFFFF

    machine.stub(SET_MENU_ROWS, rows)
    machine.stub(SELECTED_ROW, selected)
    machine.stub(CLEAR_MENU, lambda machine: record(calls, 2, machine.r[4]))
    machine.stub(FLY_OUT, lambda machine: record(calls, 3, machine.r[4], machine.r[5]))


def put_menu(machine: Machine, value: dict) -> None:
    machine.put(CURRENT_MAP_ROW, value["row"])
    machine.put(CURSOR, value["cursor"][0], 1)
    machine.put(CURSOR + 1, value["cursor"][1], 1)
    machine.put(SETTING, value["setting"], 1)


def menu_values(machine: Machine) -> tuple[list[int], list[str]]:
    return ([machine.load(CURRENT_MAP_ROW, 4, signed=True), machine.byte(CURSOR), machine.byte(CURSOR + 1),
             machine.byte(SETTING)],
            ["current map row", "cursor row", "cursor first row", "cursor memory setting"])


def run_configure(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    put_menu(machine, value)
    menu_stubs(machine, value, calls, text_words=False)
    machine.r[4] = value["count"] & 0xFFFFFFFF
    machine.r[5] = value["area"] & 0xFFFFFFFF
    machine.r[6] = MENU_TEXT
    machine.execute(entry)
    values, names = menu_values(machine)
    return values, names, [value["count"], value["area"], value["row"], *value["cursor"], value["setting"]]


def run_poll(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    machine.put(CURSOR, value["cursor"], 1)
    value = {**value, "selected_row": value["selected"]}
    menu_stubs(machine, value, calls, text_words=False)
    machine.execute(entry)
    return [s32(machine.r[2])], ["result"], [value["selected"], value["cursor"]]


def run_name(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    machine.store(ROW, bytes(range(0x40, 0x40 + ROW_BYTES)))

    def item(machine: Machine) -> None:
        sp = machine.r[29]
        record(calls, 1, machine.r[4], machine.r[5], machine.r[6], machine.r[7],
               machine.load(sp + 0x10), machine.load(sp + 0x14))
        machine.r[2] = ROW

    machine.stub(SET_MENU_ITEM, item)
    machine.r[4] = value["name"] & 0xFFFFFFFF
    machine.r[5] = value["color"] & 0xFFFFFFFF
    machine.execute(entry)
    values = [machine.byte(ROW), machine.load(ROW + 0x18, 2, signed=True), machine.byte(ROW + 6),
              machine.byte(ROW + 0xA)]
    return values, ["state", "target x", "selected", "font color"], [value["name"], value["color"]]


def run_ready(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    def ready(machine: Machine) -> None:
        record(calls, 1)
        machine.r[2] = value["ready"] & 0xFFFFFFFF

    machine.stub(MENU_READY, ready)
    machine.execute(entry)
    return [], [], [value["ready"]]


def run_background(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    machine.put(BACKGROUND_SLOT, OLD_SLOT)
    machine.put(BACKGROUND_IMAGE, OLD_BUFFER)
    machine.put(BACKGROUND_STEP, value["step"])
    machine.put(OLD_SLOT, value["state"], 2)

    def allocate_read(machine: Machine) -> None:
        record(calls, 1, machine.load(machine.r[4]), machine.load(machine.r[4] + 4))
        machine.r[2] = SLOT

    def allocate_image(machine: Machine) -> None:
        record(calls, 2, machine.r[4])
        machine.r[2] = BUFFER

    machine.stub(ALLOCATE_SLOT, allocate_read)
    machine.stub(ALLOCATE_HEAP, allocate_image)
    machine.stub(ENQUEUE, lambda machine: record(calls, 3, machine.r[4], machine.r[5]))
    machine.stub(FREE_SLOT, lambda machine: record(calls, 4, machine.r[4]))
    machine.stub(LOAD_CLUT, lambda machine: record(calls, 5, machine.r[4], machine.r[5], machine.r[6], machine.r[7]))
    machine.stub(RENDER_IMAGE, lambda machine: record(calls, 6, machine.r[4], machine.r[5], machine.r[6]))
    machine.stub(BACKGROUND_PRIORITY, lambda machine: record(calls, 7, machine.r[4], machine.r[5]))
    machine.stub(FREE_HEAP, lambda machine: record(calls, 8, machine.r[4]))
    machine.r[4] = value["begin"] & 0xFFFFFFFF
    machine.execute(entry)
    values = [s32(machine.r[2]), *(machine.load(address, 4, signed=True)
                                   for address in (BACKGROUND_STEP, BACKGROUND_SLOT, BACKGROUND_IMAGE))]
    return (values, ["result", "step", "read", "image"],
            [value["begin"], value["step"], value["state"], s32(OLD_SLOT), s32(OLD_BUFFER)])


def run_scene(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    machine.put(SCENE_BUFFER, value["buffer"])
    machine.put(ROOM_FLAGS, value["room_flags"])
    machine.put(YAW, value["yaw"], 2)
    machine.put(SCENE_SLOT, OLD_SLOT)
    machine.put(OLD_SLOT, value["state"], 2)
    machine.put(PROJECTION, value["projection"])
    machine.put(ROOM_NAMES_POINTER, OLD_ROOM_NAMES)
    for (_, address, size), item in zip(VIEW, value["view"], strict=True):
        machine.put(address, item, size)
    for scene in range(SCENES):
        machine.put(PALING + scene, value["paling"][scene], 1)
        machine.put(ARM_FILES + 8 * scene, 0x1000 + scene)
        machine.put(ARM_FILES + 8 * scene + 4, value["sizes"][scene])
        machine.put(MAP_NAMES + 2 * scene, value["names"][scene], 2)

    def bzero(machine: Machine) -> None:
        for offset in range(machine.r[5]):
            machine.write(machine.r[4] + offset, 0, 1)

    def scene_id(machine: Machine) -> None:
        record(calls, 1)
        machine.r[2] = value["scene"] & 0xFFFFFFFF

    def allocate_buffer(machine: Machine) -> None:
        record(calls, 2, machine.r[4])
        machine.r[2] = BUFFER

    def allocate_read(machine: Machine) -> None:
        record(calls, 3, machine.r[4])
        machine.r[2] = SLOT

    def room_names(machine: Machine) -> None:
        record(calls, 6, machine.r[4])
        machine.r[2] = ROOM_NAMES

    def initial(machine: Machine) -> None:
        record(calls, 9, machine.r[4])
        machine.r[2] = value["initial"] & 0xFFFFFFFF

    machine.stub(BZERO, bzero)
    machine.stub(CURRENT_SCENE_ID, scene_id)
    machine.stub(ALLOCATE_HEAP, allocate_buffer)
    machine.stub(ALLOCATE_SLOT, allocate_read)
    machine.stub(ENQUEUE, lambda machine: record(calls, 4, machine.r[4], machine.r[5]))
    machine.stub(FREE_SLOT, lambda machine: record(calls, 5, machine.r[4]))
    machine.stub(ROOM_NAMES_OF, room_names)
    machine.stub(RESTORE_VISITS, lambda machine: record(calls, 7, machine.r[4]))
    machine.stub(SCALE_ROOMS, lambda machine: record(calls, 8, machine.r[4], machine.r[5]))
    machine.stub(INITIAL_ROOM, initial)
    machine.stub(SNAP_TO_ROOM, lambda machine: record(calls, 10, machine.r[4], machine.r[5]))
    machine.stub(SET_FOG_NEAR, lambda machine: record(calls, 11, machine.r[4], machine.r[5]))
    machine.stub(SET_FAR_COLOR, lambda machine: record(calls, 12, machine.r[4], machine.r[5], machine.r[6]))
    machine.stub(SHOW_AREA_NAME, lambda machine: record(calls, 13, machine.r[4], machine.r[5]))
    machine.execute(entry)

    values = [s32(machine.r[2]), *(machine.load(address, 4, signed=True) for address in
                                   (SCENE_BUFFER, SCENE_SLOT, ROOM_NAMES_POINTER, ROOM_FLAGS))]
    names = ["result", "scene buffer", "scene read", "room names", "room flags"]
    for name, address, size in VIEW:
        values.append(machine.load(address, size, signed=True))
        names.append(name)
    for scene in range(SCENES):
        values.append(machine.byte(PALING + scene))
        names.append(f"paling {scene}")
    inputs = [value["buffer"], value["room_flags"], value["yaw"], value["scene"], s32(OLD_SLOT), value["state"],
              value["initial"], value["projection"], s32(OLD_ROOM_NAMES), *value["view"], *value["paling"],
              *value["sizes"], *value["names"]]
    return values, names, inputs


def run_select(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    put_menu(machine, value)
    machine.put(SELECTION_STEP, value["step"])
    machine.put(SELECTION_SELECTED, value["selected"])
    machine.put(AREA_FLAGS, value["visited"])
    machine.put(CURRENT_SCENE, value["current_scene"])
    machine.put(CURRENT_SCENE_ROW, value["current_scene_row"])
    for scene in range(SCENES):
        machine.put(SCENE_ROWS + 4 * scene, value["scene_rows"][scene])
        machine.put(MAP_NAMES + 2 * scene, value["names"][scene], 2)
    menu_stubs(machine, value, calls, text_words=True)
    machine.follow.update((CONFIGURE_ENTRY, POLL_ENTRY))
    machine.r[4] = value["initialize"] & 0xFFFFFFFF
    machine.execute(entry)
    menu, menu_names = menu_values(machine)
    values = [s32(machine.r[2]), machine.load(SELECTION_STEP, 4, signed=True),
              machine.load(SELECTION_SELECTED, 4, signed=True), *menu,
              *(machine.load(SCENE_ROWS + 4 * scene, 4, signed=True) for scene in range(SCENES))]
    names = ["result", "step", "selected", *menu_names, *(f"scene row {scene}" for scene in range(SCENES))]
    inputs = [value["initialize"], value["step"], value["selected"], s32(value["visited"]), value["current_scene"],
              value["current_scene_row"], value["row"], *value["cursor"], value["setting"], value["selected_row"],
              *value["scene_rows"], *value["names"]]
    return values, names, inputs


RUNNERS = {
    CONFIGURE: run_configure,
    POLL: run_poll,
    NAME: run_name,
    READY: run_ready,
    BACKGROUND: run_background,
    SCENE: run_scene,
    SELECT: run_select,
}

MENU_STORES = [(CURRENT_MAP_ROW, CURRENT_MAP_ROW + 4), (CURSOR, CURSOR + 2), (SETTING, SETTING + 1)]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [STACK]
    mode = value["mode"]
    if mode == CONFIGURE:
        allowed += MENU_STORES
    elif mode == NAME:
        allowed.append((ROW, ROW + ROW_BYTES))
    elif mode == BACKGROUND:
        allowed.append((BACKGROUND_SLOT, BACKGROUND_STEP + 4))
    elif mode == SCENE:
        allowed += [(address, address + size) for _, address, size in VIEW]
        allowed += [(PALING, PALING + 1), (ROOM_FLAGS, ROOM_FLAGS + 4), (SCENE_BUFFER, SCENE_BUFFER + 4),
                    (SCENE_SLOT, SCENE_SLOT + 4), (ROOM_NAMES_POINTER, ROOM_NAMES_POINTER + 4)]
    elif mode == SELECT:
        allowed += MENU_STORES + [(SELECTION_STEP, SELECTION_SELECTED + 4), (SCENE_ROWS, SCENE_ROWS + 4 * SCENES)]
    return allowed


def describe(image, results) -> list[str]:
    called = sum(1 for _, observation, _ in results if observation["calls"])
    return [f"{called} of {len(results)} vectors made a recorded call"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
