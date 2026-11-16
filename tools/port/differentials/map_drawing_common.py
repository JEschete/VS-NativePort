"""Shared harness for the map drawing differentials (MENU5.PRG 4D8.c:1081-1510 and 1965-2470, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::map core
(MapDrawing.cpp) in map_drawing_driver.cpp. The originals' callees are stubbed and recorded:
RotTransPers answers the vector's projections in call order, DpqColor, rsin and rcos answer fixed
integer mixes both sides compute, and the marker, label, legend and compass callees only record their
arguments. Packets are written at a fixture primitive cursor and linked through a stubbed AddPrim; both
sides print the result, the globals the original writes, every linked packet's fields in link order
and the recorded calls.

The typed cores refuse a malformed room, more backdrop rows than kMaxBackdropLines, a label count past
_mapLabels and a negative frame counter, where the original reads or writes past its tables; vectors
stay inside (MapDrawingTests.cpp covers the refusals).
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
DRIVER = "map_drawing_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\MapDrawing.cpp",)

VISITED, ROOM, BACKDROP, CONTROLS, COMPASS, CORNERS, LABELS = range(7)

SCRATCH = 0x1F800000  # vs_scratch: primitive cursor and ordering table
ROWS = 0x1F800100  # _renderRoom's projected vertex rows (getScratchAddr(64))
ROW_COUNT = 16
PRIMS = 0x80500000
PRIMS_BYTES = 0x2000
OT = 0x80510000
OT_BYTES = 0x40
OT_PRESET = 0x0412345C
SCENE = 0x80520000
ROOM_DATA = 0x80530000
TEXT_BOX = 0x80540000
TEXT_BOX_BYTES = 0x20
BOX_Y, BOX_INDENT = 0x12, 0x16

CURRENT_ROOM = 0x80108D64  # currentRoomIndex
FRAMES = 0x80108D6C  # menuFramesCounter
GEOM_X, GEOM_Y = 0x80108D70, 0x80108D74  # vs_battle_geomOffset
SCENE_ROOM = 0x80108D78  # isCurrentScene
CURRENT_SCENE = 0x80108D80
HIDE_UI = 0x80108D8C  # triggerHideUi
COMMANDS_STATE = 0x80108D9C
ANIMATION_STATE = 0x80108D9E  # uiAnimationState
X_OFFSET = 0x80108D5C  # uiXOffset
CAMERA_VY, CAMERA_VZ = 0x80108DA6, 0x80108DA8  # cameraAngles.vy and vz
BOUNDS = 0x80108E54  # compassBounds
LABELS_AT = 0x80108E74  # _mapLabels
LABEL_COUNT = 0x80108EB4  # _mapLabelCount
BRIGHTNESS = 0x80102C00  # Ashley's 16 blink brightness steps
DOOR_ENTERED = 0x800F1BBE  # vs_battle_doorEntered
PALING = 0x80061598 + 0x135  # vs_main_stateFlags.mapPaling
CAPTIONS = (0x80102C78, 0x80102C80, 0x80102C88, 0x80102C94, 0x80102C9C, 0x80102CA4)

ADD_PRIM = 0x8002B4D4
ROT_TRANS_PERS = 0x80041814
DPQ_COLOR = 0x80041A00
RSIN, RCOS = 0x80040F28, 0x80040F24
GET_GEOM, SET_GEOM = 0x800994D4, 0x800994F4
DRAW_ROOM = 0x80104780  # _renderRoom
DRAW_LABELS = 0x801082A8  # renderMapLabels
SHOULD_DRAW = 0x8010839C  # func_8010839C
DOOR_STATE = 0x80107630  # func_80107630
RENDER_ICON = 0x801074AC  # _renderIcon
ADD_LABEL = 0x80108274  # _addMapLabel
DEFAULT_ICON = 0x80102D1C  # _renderDefaultIcon
GET_TEXT_BOX = 0x800CCDF4  # vs_battle_getTextBox
RENDER_BUTTON = 0x800FFC68  # vs_mainmenu_renderButton
RENDER_TEXT = 0x800C6540  # vs_battle_renderTextRawColor
BACKDROP_AT = 0x80106AF4  # _drawUIControlsBackground
COMPASS_AT = 0x80107B10  # _renderCompass
CORNERS_AT = 0x8010800C  # _computeRotatedRect
DOOR_NAME = 0x801079B8  # _renderDoorName
AREA_NAME = 0x8010815C  # _renderConnectingMapName

(CALL_ADD_PRIM, CALL_RTP, CALL_DPQ, CALL_SHOULD, CALL_DOOR_STATE, CALL_FEATURE, CALL_QUEUE, CALL_ASHLEY,
 CALL_GET_GEOM, CALL_SET_GEOM, CALL_ROOM, CALL_LABELS, CALL_TEXT_BOX, CALL_BUTTON, CALL_TEXT, CALL_BACKDROP,
 CALL_COMPASS, CALL_SIN, CALL_COS, CALL_DOOR_NAME, CALL_AREA_NAME) = range(1, 22)

MIX = 0x9E3779B1
SINE_KEY, COSINE_KEY = 0x1234567, 0x7654321
LABEL_CAPACITY = 8
LINE_LIMIT = 256
ARGUMENT_SLOTS = 0x10
INT_EDGES = (0, 1, -1, 0x7FFF, -0x8000, 0x7FFFFFFF, -0x80000000)


def trig(angle: int, key: int) -> int:
    """rsin and rcos as both sides answer them: a fixed mix into [-4096, 4096)."""
    return (((angle * MIX + key) & 0xFFFFFFFF) >> 19) - 4096


def depth_cue(r: int, g: int, b: int, p: int) -> tuple[int, int, int]:
    """DpqColor as both sides answer it."""
    return (r * 3 + p) & 0xFF, (g + (p >> 2)) & 0xFF, (b ^ p) & 0xFF


def half(rng: random.Random) -> int:
    return rng.choice((0, 1, -1, 0x7FFF, -0x8000, rng.randrange(-0x8000, 0x8000), rng.randrange(-64, 400)))


def word(rng: random.Random) -> int:
    return rng.choice((*INT_EDGES, s32(rng.getrandbits(32)), rng.randrange(-1000, 1000)))


# ------------------------------------------------------------------ vectors

def room_vector(rng: random.Random, value: dict) -> None:
    vertices = rng.choice((0, 1, 2, 3, 4, 5, 6, 8, 10))
    reach = min(ROW_COUNT, vertices + 2)
    index = lambda: rng.randrange(reach) if rng.random() < 0.9 else rng.randrange(ROW_COUNT)  # noqa: E731

    def depth() -> int:
        if rng.random() < 0.97:
            return rng.choice((0x40, 0x7FE, rng.randrange(0x40, 0x7FF)))
        return rng.choice((0x3F, 0x7FF, -1, 0, 0x10045, s32(rng.getrandbits(32))))

    def entries(count_max: int) -> tuple[int, list[list[int]]]:
        count = rng.choice((*range(count_max + 1), -1))
        return count, [[index(), index(), index(), index()] for _ in range(max(count, 0))]

    value["vertices"] = [[half(rng) for _ in range(4)] for _ in range(vertices)]
    value["projections"] = [[half(rng), half(rng), depth(), word(rng)] for _ in range(vertices)]
    value["triangles"] = entries(3)
    value["quads"] = entries(3)
    value["edges"] = entries(3)
    value["dim_edges"] = entries(3)
    count = rng.choice((0, 1, 2, 3, 4, 5, 6, -1))
    flags = (0, 0, 1, 2, 4, 8, 0x10, 0x0B, 0x1A, 0x1F, 0xFF, 6, 0x14)
    value["markers"] = (count, [[index(), rng.randrange(256), rng.choice((*flags, rng.randrange(256))),
                                 rng.choice((0, 0, 0, rng.randrange(1, 256)))] for _ in range(max(count, 0))])
    value["stale"] = [[half(rng), half(rng), half(rng), half(rng)] for _ in range(ROW_COUNT)]
    value["room"] = rng.randrange(3)
    value["current"] = rng.randrange(3)
    value["scene_room"] = rng.randrange(3)
    value["door"] = rng.randrange(-1, 6)
    value["frames"] = rng.choice((0, 0x10, 0x1F, 0x20, 0x7FFFFFFF, rng.randrange(0x10000), rng.getrandbits(31)))
    value["zone"] = half(rng)
    value["map"] = half(rng)
    value["brightness"] = [word(rng) for _ in range(16)]
    value["should"] = rng.getrandbits(256)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode}
    if mode == VISITED:
        count = rng.choice((0, 1, 2, 3, 5, 8, -1))
        value["geom"] = [word(rng), word(rng)]
        value["previous"] = [word(rng), word(rng)]
        value["rooms"] = [[rng.choice((0, 1, -1, 2, 0x100)), s32(rng.getrandbits(32)), half(rng), half(rng)]
                          for _ in range(max(count, 0))]
        value["count"] = count
    elif mode == ROOM:
        room_vector(rng, value)
    elif mode == BACKDROP:
        value["x"] = word(rng)
        value["y"] = rng.choice((18, 36, 0, -5, 0x7FFFFFF0, -0x80000000, rng.randrange(-300, 300)))
        value["w"] = word(rng)
        value["h"] = rng.choice((12, 0, 1, 2, -1, -12, rng.randrange(-20, 60), 257, 256))
        last = s32(value["y"] + value["h"] - 1)
        if last - value["y"] > LINE_LIMIT:  # the wrapped end would draw billions of rows
            value["h"] = 12
    elif mode == CONTROLS:
        value["animation"] = rng.choice((0, 1, 2, 3, -1, 0x7FFF))
        value["offset"] = rng.choice((-128, -127, -129, -112, -16, -1, 0, 1, 16, *INT_EDGES[3:],
                                      rng.randrange(-200, 50)))
        value["box"] = rng.choice((0, TEXT_BOX))
        value["box_y"], value["box_indent"] = half(rng), half(rng)
        value["hide"] = rng.choice((0, 0, 0, 1, -1))
        value["scene"] = rng.randrange(32)
        value["paling"] = rng.choice((0, 0, 0, 1, 0xFF))
        value["commands"] = rng.choice((0, 1, 2, 3, 4, 5, 6, -1, 0x7FFF, rng.randrange(8)))
        value["angle"] = half(rng)
    elif mode in (COMPASS, CORNERS):
        value["x"], value["y"] = word(rng), word(rng)
        value["angle"] = word(rng)
        value["roll"] = half(rng)
        value["w"] = rng.choice((43, 39, 15, 17, 3, 0, -1, 0x80000, word(rng)))
        value["h"] = rng.choice((43, 39, 15, 17, 3, 0, -1, 0x80000, word(rng)))
        value["sin"] = rng.choice((0, 4096, -4096, 2896, rng.randrange(-4096, 4097), word(rng)))
        value["cos"] = rng.choice((0, 4096, -4096, 2896, rng.randrange(-4096, 4097), word(rng)))
    elif mode == LABELS:
        value["commands"] = rng.choice((0, 1, 4, 5, 5, 6, -1))
        value["hide"] = rng.choice((0, 0, 0, 1, -1))
        value["count"] = rng.choice((*range(LABEL_CAPACITY + 1), -1, -5))
        value["labels"] = [[half(rng), half(rng), rng.choice((0, 0, 1, 2, -1)), rng.randrange(-4, 64)]
                           for _ in range(LABEL_CAPACITY)]
    return value


# ------------------------------------------------------------------ fixtures

def flatten(table: tuple[int, list[list[int]]]) -> list[int]:
    count, entries = table
    return [count, *(item for entry in entries for item in entry)]


def room_bytes(value: dict) -> bytes:
    data = bytearray(len(value["vertices"]).to_bytes(4, "little"))
    for vertex in value["vertices"]:
        for item in vertex:
            data += (item & 0xFFFF).to_bytes(2, "little")
    for name in ("triangles", "quads", "edges", "dim_edges", "markers"):
        count, entries = value[name]
        data += (count & 0xFFFFFFFF).to_bytes(4, "little")
        for entry in entries:
            data += bytes(entry)
    return bytes(data)


def scratch(machine: Machine) -> None:
    machine.map_zero(SCRATCH, 8)
    machine.map_zero(PRIMS, PRIMS_BYTES)
    machine.map_zero(OT, OT_BYTES)
    machine.put(SCRATCH, PRIMS)
    machine.put(SCRATCH + 4, OT)


def add_prim_recorder(machine: Machine, calls: list[list[int]], prims: list[int]) -> None:
    def add_prim(machine: Machine) -> None:
        prims.append(machine.r[5])
        calls.append([CALL_ADD_PRIM, s32(machine.r[4] - OT), s32(machine.r[5] - PRIMS)])

    machine.stub(ADD_PRIM, add_prim)


def decode(machine: Machine, at: int) -> list[int]:
    """A linked packet's tag length, code and fields, as the driver prints them."""
    byte = lambda offset: machine.load(at + offset, 1)  # noqa: E731
    xy = lambda offset: [machine.load(at + offset, 2, signed=True),  # noqa: E731
                         machine.load(at + offset + 2, 2, signed=True)]
    code = byte(7)
    values = [byte(3), code]
    if code in (0x30, 0x38):  # POLY_G3 and POLY_G4: colour and position per corner
        for corner in range(3 if code == 0x30 else 4):
            values += [byte(4 + 8 * corner), byte(5 + 8 * corner), byte(6 + 8 * corner), *xy(8 + 8 * corner)]
    elif code in (0x50, 0x52):  # LINE_G2
        values += [byte(4), byte(5), byte(6), *xy(8), byte(0xC), byte(0xD), byte(0xE), *xy(0x10)]
    elif code == 0x60:  # TILE
        values += [byte(4), byte(5), byte(6), *xy(8), *xy(0xC)]
    elif code == 0x28:  # POLY_F4
        values += [byte(4), byte(5), byte(6), *xy(8), *xy(0xC), *xy(0x10), *xy(0x14)]
    elif code in (0x2C, 0x2D, 0x2E, 0x2F):  # POLY_FT4
        for corner in range(4):
            values += [*xy(8 + 8 * corner), byte(0xC + 8 * corner), byte(0xD + 8 * corner)]
        values += [machine.load(at + 0xE, 2), machine.load(at + 0x16, 2)]
    return values


def packet_values(machine: Machine, prims: list[int]) -> tuple[list[int], list[str]]:
    values: list[int] = []
    names: list[str] = []
    for number, prim in enumerate(prims):
        decoded = decode(machine, prim)
        values += decoded
        names += [f"packet {number} field {field}" for field in range(len(decoded))]
    return values, names


def cursor_advance(machine: Machine) -> int:
    return s32(machine.load(SCRATCH) - PRIMS)


def set_arguments(machine: Machine, *arguments: int) -> None:
    for register, item in enumerate(arguments, start=4):
        machine.r[register] = item & 0xFFFFFFFF


# ------------------------------------------------------------------ runners

def run_visited(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    machine.map_zero(SCENE, 4 + 12 * max(len(value["rooms"]), 1))
    machine.put(SCENE, value["count"])
    for index, (visited, data, zone, map_id) in enumerate(value["rooms"]):
        at = SCENE + 4 + 12 * index
        machine.put(at, visited)
        machine.put(at + 4, data)
        machine.put(at + 8, zone, 2)
        machine.put(at + 10, map_id, 2)
    machine.put(GEOM_X, value["geom"][0])
    machine.put(GEOM_Y, value["geom"][1])
    machine.put(LABEL_COUNT, 5)

    def get_geom(machine: Machine) -> None:
        calls.append([CALL_GET_GEOM])
        machine.put(machine.r[4], value["previous"][0])
        machine.put(machine.r[4] + 4, value["previous"][1])

    def set_geom(machine: Machine) -> None:
        calls.append([CALL_SET_GEOM, machine.load(machine.r[4], 4, signed=True),
                      machine.load(machine.r[4] + 4, 4, signed=True)])

    machine.stub(GET_GEOM, get_geom)
    machine.stub(SET_GEOM, set_geom)
    machine.stub(DRAW_ROOM, lambda m: calls.append([CALL_ROOM, *(s32(m.r[i]) for i in range(4, 8))]))
    machine.stub(DRAW_LABELS, lambda m: calls.append([CALL_LABELS]))
    set_arguments(machine, SCENE)
    machine.execute(entry)
    inputs = [*value["geom"], *value["previous"], value["count"], len(value["rooms"]),
              *(item for room in value["rooms"] for item in room)]
    return [machine.load(LABEL_COUNT, 4, signed=True)], ["label count"], inputs


def run_room(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    scratch(machine)
    data = room_bytes(value)
    machine.store(ROOM_DATA, data)
    machine.map_zero(ROWS, 8 * 256)
    for index, row in enumerate(value["stale"]):
        for field, item in enumerate(row):
            machine.put(ROWS + 8 * index + 2 * field, item, 2)
    machine.put(CURRENT_ROOM, value["current"])
    machine.put(SCENE_ROOM, value["scene_room"])
    machine.put(FRAMES, value["frames"])
    machine.put(DOOR_ENTERED, value["door"], 2)
    for index, step in enumerate(value["brightness"]):
        machine.put(BRIGHTNESS + 4 * index, step)
    prims: list[int] = []
    add_prim_recorder(machine, calls, prims)
    projections = iter(value["projections"])

    def rot_trans_pers(machine: Machine) -> None:
        at = machine.r[4]
        calls.append([CALL_RTP, *(machine.load(at + 2 * axis, 2, signed=True) for axis in range(3))])
        sx, sy, otz, p = next(projections)
        machine.put(machine.r[5], (sx & 0xFFFF) | ((sy & 0xFFFF) << 16))
        machine.put(machine.r[6], p)
        machine.put(machine.r[7], 0)
        machine.r[2] = otz & 0xFFFFFFFF

    def dpq_color(machine: Machine) -> None:
        r, g, b = (machine.load(machine.r[4] + offset, 1) for offset in range(3))
        p = s32(machine.r[5])
        calls.append([CALL_DPQ, r, g, b, p])
        for offset, item in enumerate((*depth_cue(r, g, b, p), 0)):
            machine.put(machine.r[6] + offset, item, 1)

    def should(machine: Machine) -> None:
        state = s32(machine.r[4])
        calls.append([CALL_SHOULD, state, s32(machine.r[5]), s32(machine.r[6])])
        machine.r[2] = value["should"] >> (state & 0xFF) & 1

    machine.stub(ROT_TRANS_PERS, rot_trans_pers)
    machine.stub(DPQ_COLOR, dpq_color)
    machine.stub(SHOULD_DRAW, should)
    machine.stub(DOOR_STATE, lambda m: calls.append([CALL_DOOR_STATE, *(s32(m.r[i]) for i in range(4, 7))]))
    machine.stub(RENDER_ICON, lambda m: calls.append([CALL_FEATURE, *(s32(m.r[i]) for i in range(4, 7))]))
    machine.stub(ADD_LABEL, lambda m: calls.append([CALL_QUEUE, *(s32(m.r[i]) for i in range(4, 8))]))
    machine.stub(DEFAULT_ICON, lambda m: calls.append(
        [CALL_ASHLEY, s32(m.r[4]), s32(m.r[5]), s32(m.r[6]), s32(m.r[7] - OT)]))
    set_arguments(machine, value["room"], ROOM_DATA, value["zone"], value["map"])
    machine.execute(entry)
    values = [s32(machine.r[2]), cursor_advance(machine)]
    names = ["result", "cursor advance"]
    for index in range(ROW_COUNT):
        values += [machine.load(ROWS + 8 * index + 2 * field, 2, signed=True) for field in range(4)]
        names += [f"row {index} field {field}" for field in range(4)]
    packets, packet_names = packet_values(machine, prims)
    inputs = [value["room"], value["current"], value["scene_room"], value["door"], value["frames"],
              value["zone"], value["map"], *value["brightness"], value["should"] & 0xFFFFFFFF,
              value["should"] >> 32 & 0xFFFFFFFF, value["should"] >> 64 & 0xFFFFFFFF,
              value["should"] >> 96 & 0xFFFFFFFF, value["should"] >> 128 & 0xFFFFFFFF,
              value["should"] >> 160 & 0xFFFFFFFF, value["should"] >> 192 & 0xFFFFFFFF,
              value["should"] >> 224 & 0xFFFFFFFF,
              *(item for row in value["stale"] for item in row),
              len(value["projections"]), *(item for row in value["projections"] for item in row),
              len(data), *data]
    return values + packets, names + packet_names, inputs


def run_backdrop(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    scratch(machine)
    prims: list[int] = []
    add_prim_recorder(machine, calls, prims)
    set_arguments(machine, value["x"], value["y"], value["w"], value["h"])
    machine.execute(entry)
    packets, names = packet_values(machine, prims)
    return [cursor_advance(machine), *packets], ["cursor advance", *names], [
        value["x"], value["y"], value["w"], value["h"]]


def run_controls(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    scratch(machine)
    machine.put(OT + 0x1C, OT_PRESET)
    machine.map_zero(TEXT_BOX, TEXT_BOX_BYTES)
    machine.put(TEXT_BOX + BOX_Y, value["box_y"], 2)
    machine.put(TEXT_BOX + BOX_INDENT, value["box_indent"], 2)
    machine.put(ANIMATION_STATE, value["animation"], 2)
    machine.put(X_OFFSET, value["offset"])
    machine.put(HIDE_UI, value["hide"])
    machine.put(CURRENT_SCENE, value["scene"])
    machine.put(PALING + value["scene"], value["paling"], 1)
    machine.put(COMMANDS_STATE, value["commands"], 2)
    machine.put(CAMERA_VY, value["angle"], 2)

    def text_box(machine: Machine) -> None:
        calls.append([CALL_TEXT_BOX, s32(machine.r[4])])
        machine.r[2] = value["box"]

    def text(machine: Machine) -> None:
        caption = CAPTIONS.index(machine.r[4]) if machine.r[4] in CAPTIONS else s32(machine.r[4])
        calls.append([CALL_TEXT, caption, s32(machine.r[5]), s32(machine.r[6]), s32(machine.r[7] - OT)])

    machine.stub(GET_TEXT_BOX, text_box)
    machine.stub(RENDER_TEXT, text)
    machine.stub(RENDER_BUTTON, lambda m: calls.append(
        [CALL_BUTTON, s32(m.r[4]), s32(m.r[5]), s32(m.r[6]), s32(m.r[7] - OT)]))
    machine.stub(BACKDROP_AT, lambda m: calls.append([CALL_BACKDROP, *(s32(m.r[i]) for i in range(4, 8))]))
    machine.stub(COMPASS_AT, lambda m: calls.append([CALL_COMPASS, *(s32(m.r[i]) for i in range(4, 7))]))
    machine.execute(entry)
    values = [machine.load(ANIMATION_STATE, 2, signed=True), machine.load(X_OFFSET, 4, signed=True),
              machine.load(TEXT_BOX + BOX_Y, 2, signed=True), machine.load(TEXT_BOX + BOX_INDENT, 2, signed=True),
              cursor_advance(machine), s32(machine.load(OT + 0x1C)),
              *(s32(machine.load(PRIMS + 4 * index)) for index in range(3))]
    names = ["animation state", "x offset", "box y", "box indent", "cursor advance", "ot 7",
             "tpage word 0", "tpage word 1", "tpage word 2"]
    inputs = [value["animation"], value["offset"], value["box"], value["box_y"], value["box_indent"],
              value["hide"], value["paling"], value["commands"], value["angle"]]
    return values, names, inputs


def trig_stubs(machine: Machine, calls: list[list[int]], value: dict | None) -> None:
    def sine(machine: Machine) -> None:
        angle = s32(machine.r[4])
        calls.append([CALL_SIN, angle])
        machine.r[2] = (value["sin"] if value else trig(angle, SINE_KEY)) & 0xFFFFFFFF

    def cosine(machine: Machine) -> None:
        angle = s32(machine.r[4])
        calls.append([CALL_COS, angle])
        machine.r[2] = (value["cos"] if value else trig(angle, COSINE_KEY)) & 0xFFFFFFFF

    machine.stub(RSIN, sine)
    machine.stub(RCOS, cosine)


def bounds(machine: Machine) -> tuple[list[int], list[str]]:
    return ([machine.load(BOUNDS + 4 * index, 4, signed=True) for index in range(8)],
            [f"bound {index // 2} {'xy'[index % 2]}" for index in range(8)])


def run_compass(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    scratch(machine)
    machine.put(CAMERA_VZ, value["roll"], 2)
    prims: list[int] = []
    add_prim_recorder(machine, calls, prims)
    trig_stubs(machine, calls, None)
    machine.follow.add(CORNERS_AT)
    set_arguments(machine, value["x"], value["y"], value["angle"])
    machine.execute(entry)
    corners, names = bounds(machine)
    packets, packet_names = packet_values(machine, prims)
    return ([cursor_advance(machine), *corners, *packets], ["cursor advance", *names, *packet_names],
            [value["x"], value["y"], value["angle"], value["roll"]])


def run_corners(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    trig_stubs(machine, calls, value)
    machine.put(machine.r[29] + 0x10, value["angle"])
    set_arguments(machine, value["x"], value["y"], value["w"], value["h"])
    machine.execute(entry)
    corners, names = bounds(machine)
    return corners, names, [value["x"], value["y"], value["w"], value["h"], value["angle"], value["sin"],
                            value["cos"]]


def run_labels(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    machine.put(COMMANDS_STATE, value["commands"], 2)
    machine.put(HIDE_UI, value["hide"])
    machine.put(LABEL_COUNT, value["count"])
    for index, label in enumerate(value["labels"]):
        for field, item in enumerate(label):
            machine.put(LABELS_AT + 8 * index + 2 * field, item, 2)
    machine.stub(DOOR_NAME, lambda m: calls.append([CALL_DOOR_NAME, *(s32(m.r[i]) for i in range(4, 7))]))
    machine.stub(AREA_NAME, lambda m: calls.append([CALL_AREA_NAME, *(s32(m.r[i]) for i in range(4, 7))]))
    machine.execute(entry)
    return [], [], [value["commands"], value["hide"], value["count"],
                    *(item for label in value["labels"] for item in label)]


RUNNERS = {VISITED: run_visited, ROOM: run_room, BACKDROP: run_backdrop, CONTROLS: run_controls,
           COMPASS: run_compass, CORNERS: run_corners, LABELS: run_labels}


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    # The originals home their arguments in the caller's four argument slots above the stack.
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK + ARGUMENT_SLOTS)]
    mode = value["mode"]
    if mode == VISITED:
        allowed.append((LABEL_COUNT, LABEL_COUNT + 4))
    if mode in (ROOM, BACKDROP, CONTROLS, COMPASS):
        allowed += [(SCRATCH, SCRATCH + 4), (PRIMS, PRIMS + PRIMS_BYTES)]
    if mode == ROOM:
        allowed.append((ROWS, ROWS + 8 * ROW_COUNT))
    if mode == CONTROLS:
        allowed += [(ANIMATION_STATE, ANIMATION_STATE + 2), (X_OFFSET, X_OFFSET + 4),
                    (TEXT_BOX + BOX_Y, TEXT_BOX + BOX_Y + 2), (TEXT_BOX + BOX_INDENT, TEXT_BOX + BOX_INDENT + 2),
                    (OT + 0x1C, OT + 0x20)]
    if mode in (COMPASS, CORNERS):
        allowed.append((BOUNDS, BOUNDS + 0x20))
    return allowed


def describe(image, results) -> list[str]:
    mode = results[0][0]["mode"] if results else None
    lines = []
    if mode == ROOM:
        drawn = sum(1 for _, observation, _ in results if observation["values"][0] == 1)
        packets = sum(sum(1 for call in observation["calls"] if call[0] == CALL_ADD_PRIM)
                      for _, observation, _ in results)
        markers = sum(sum(1 for call in observation["calls"] if call[0] in (CALL_FEATURE, CALL_ASHLEY,
                                                                            CALL_DOOR_STATE))
                      for _, observation, _ in results)
        lines.append(f"{drawn} of {len(results)} rooms drawn ({len(results) - drawn} hit the depth range), "
                     f"{packets} packets, {markers} marker icons")
    if mode == CONTROLS:
        jammed = sum(1 for _, observation, _ in results
                     if any(call[0] == CALL_TEXT and call[1] == 5 for call in observation["calls"]))
        legends = sum(1 for _, observation, _ in results
                      if any(call[0] == CALL_COMPASS for call in observation["calls"]))
        lines.append(f"{legends} legends drawn, {jammed} jammed")
    if mode in (VISITED, LABELS):
        calls = sum(len(observation["calls"]) for _, observation, _ in results)
        lines.append(f"{calls} recorded calls")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
