"""MENU5 terminal packet: original instructions, complete packets and controller observations.

The four rendering cases follow the resident AddPrim, SetDrawStp, sine, cosine,
matrix multiplication and ApplyMatrix instructions. Frame's other named callees
are recorded endpoints. Packet buffers and ordering heads have varying nonzero
contents; every output word, untouched RGB/CLUT, cursor, link and guard is compared.
The camera compares MAC translation, not saturated IR (#411 is not changed).

Screen/menu endpoints record original call arguments and state changes; actual
resource ownership, queue cancellation and restoration are exercised separately
by MapTerminalLiveTests. Invalid scene/row/resource refusals are not original
parity vectors. Importing metadata requires neither disc data nor optional tools.
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
    s16,
    s32,
    store_problems,
)
from manual_illustration_common import compare
from map_loading_common import VIEW

MODULE_BASES["MENU/MENU5.PRG"] = 0x80102800
MODULES = ("MENU/MENU5.PRG", "SLUS_010.40")
DRIVER = "map_terminal_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\MapTerminal.cpp",
    r"Native\Game\src\Menu\Framework\Framework.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Render\GameFont.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
    r"Native\Engine\src\Render\UiPackets.cpp",
    r"Native\Engine\src\Render\EncodedText.cpp",
)

FRAME, CAMERA, DARKEN, PALING, SCREEN, MENU = range(6)
SCRATCH = 0x1F800000
PRIMITIVES = 0x80280000
PRIMITIVE_BYTES = 0x8000
ORDERING = 0x80300000
SCENE_ENTRY = 18
ORDERING_ENTRIES = 2084
SLOTS = (8, 2046, 2047)
FRAME_BUFFER = 0x8005E210
CURRENT_SCENE = 0x80108D80
IS_PALING = 0x80108D88
PALING_FLAGS = 0x800616CD
CAPTION = 0x80108CC4
SCENE_BUFFER = 0x800F1BC0
ANGLE = 0x80108DA4
POSITION = 0x80108DAC
DISTANCE = 0x80108D7C
FRAMES = 0x80108D6C
ADD_PRIM = 0x8002B4D4
SET_MASK = 0x8002B714
DRAW_CONTROLS = 0x80106C84
DRAW_FRAME = 0x801042B0
APPLY_CAMERA = 0x80104384
DRAW_DARKEN = 0x8010625C
DRAW_PALING = 0x801066E0
DRAW_ROOMS = 0x801046B0
SET_TEXT = 0x800C6BF0
RSIN, RCOS = 0x80040F28, 0x80040F24
BUILD_YAW, BUILD_PITCH = 0x80106178, 0x801061EC
MUL_MATRIX, APPLY_MATRIX = 0x80041C68, 0x8004177C
SET_ROTATION, SET_TRANSLATION = 0x80041598, 0x80041578
STACK = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)
MASK32 = 0xFFFFFFFF


def word(rng: random.Random) -> int:
    return rng.choice((0, -1, 1, 0x7FFFFFFF, -0x80000000,
                       s32(rng.getrandbits(32))))


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value = {"mode": mode}
    if mode in (FRAME, DARKEN, PALING):
        value.update(start=rng.randrange(16), salt=rng.getrandbits(32),
                     back=rng.choice((0, 0, 1, -1, 2)), frame=word(rng))
        if mode == FRAME:
            value.update(scene=rng.randrange(32), jammed=rng.choice((0, 0, 1, 255)),
                         leaving=rng.choice((0, 0, 1, -1)),
                         caption=rng.randrange(1, 16))
    elif mode == CAMERA:
        edges = (0, 1, -1, 568, 1024, 2048, 4095, -32768, 32767)
        value.update(angles=[rng.choice((*edges, rng.randrange(-32768, 32768)))
                             for _ in range(4)],
                     position=[rng.randrange(-32768, 32768) for _ in range(4)],
                     distance=rng.choice((0, 1, -1, 512, 768, 1024, 32768, 40000,
                                          word(rng), rng.randrange(-60000, 60000))))
        if number == 0:
            value.update(angles=[568, 767, 177, 42], distance=40000)
    elif mode == SCREEN:
        value.update(screen_vector(rng, number))
    elif mode == MENU:
        value.update(menu_vector(rng, number))
    return value


def pattern(index: int, salt: int) -> int:
    return (salt ^ (index * 0x9E3779B9)) & MASK32


def sentinel(index: int, salt: int) -> int:
    return (salt & 0xFF000000) | (0x600000 + index * 4)


def packet_words(mode: int) -> int:
    return {FRAME: 3, DARKEN: 19, PALING: 3827}[mode]


def install_packets(machine: Machine, value: dict) -> int:
    count = value["start"] + packet_words(value["mode"]) + 4
    machine.map_zero(PRIMITIVES, PRIMITIVE_BYTES)
    for index in range(count):
        machine.put(PRIMITIVES + index * 4, pattern(index, value["salt"]))
    machine.map_zero(ORDERING, ORDERING_ENTRIES * 4)
    for index in range(ORDERING_ENTRIES):
        machine.put(ORDERING + index * 4, sentinel(index, value["salt"]))
    machine.put(SCRATCH, PRIMITIVES + value["start"] * 4)
    machine.put(SCRATCH + 4, ORDERING + SCENE_ENTRY * 4)
    machine.put(FRAME_BUFFER, value["back"])
    machine.put(FRAMES, value["frame"])
    machine.follow.update((ADD_PRIM, SET_MASK, RSIN))
    return count


def record(calls: list, address: int, *args: int) -> None:
    calls.append([address, len(args), *args])


def observe(machine: Machine, value: dict, values: list[int],
            inputs: list[int], calls: list | None = None) -> tuple[dict, list[int]]:
    calls = calls or []
    values = [*values, len(calls), *(word for call in calls for word in call)]
    # Do not retain millions of individual store tuples in the 1024-vector
    # scanline case. Check every store before discarding that machine.
    return {"values": values,
            "problems": store_problems(machine.stores, allowed_stores(value))}, [
                value["mode"], *inputs]


def run_packets(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    count = install_packets(machine, value)
    inputs = [value[key] for key in ("start", "salt", "back", "frame")]
    calls: list = []
    if value["mode"] == FRAME:
        machine.put(CURRENT_SCENE, value["scene"])
        machine.put(PALING_FLAGS + value["scene"], value["jammed"], 1)
        machine.put(IS_PALING, value["leaving"])
        machine.put(CAPTION, value["caption"], 2)
        machine.put(SCENE_BUFFER, OLD_SCENE)
        for callee in (DRAW_CONTROLS, DRAW_PALING, APPLY_CAMERA, DRAW_DARKEN):
            machine.stub(callee, lambda m, at=callee: record(calls, at))
        machine.stub(DRAW_ROOMS, lambda m: record(calls, DRAW_ROOMS, m.r[4]))
        machine.stub(SET_TEXT, lambda m: record(calls, SET_TEXT, m.r[4], m.r[5]))
        inputs += [value[key] for key in ("jammed", "leaving", "caption")]
    machine.execute(entry)
    values = [machine.load(SCRATCH) - PRIMITIVES]
    values += [s32(machine.load(PRIMITIVES + index * 4)) for index in range(count)]
    values += [s32(machine.load(ORDERING + (SCENE_ENTRY + slot) * 4)) for slot in SLOTS]
    return observe(machine, value, values, inputs, calls)


def run_camera(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    for index in range(4):
        machine.put(ANGLE + index * 2, value["angles"][index], 2)
        machine.put(POSITION + index * 2, value["position"][index], 2)
    machine.put(DISTANCE, value["distance"])
    machine.follow.update((RSIN, RCOS, BUILD_YAW, BUILD_PITCH, MUL_MATRIX,
                           APPLY_MATRIX, SET_ROTATION, SET_TRANSLATION))
    machine.execute(entry)
    values = [machine.load(POSITION + index * 2, 2, True) for index in range(4)]
    values += [s16(machine.gte_rotation[index // 2] >> ((index % 2) * 16))
               for index in range(9)]
    values += [s32(component) for component in machine.gte_translation]
    return observe(machine, value, values,
                   [*value["angles"], value["distance"], *value["position"]])


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    if value["mode"] in (FRAME, DARKEN, PALING):
        return run_packets(machine, value, entry)
    if value["mode"] == CAMERA:
        return run_camera(machine, value, entry)
    if value["mode"] == SCREEN:
        return run_screen(machine, value, entry)
    return run_menu(machine, value, entry)


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    mode = value["mode"]
    if mode in (FRAME, DARKEN, PALING):
        return [STACK, (SCRATCH, SCRATCH + 4),
                (PRIMITIVES + value["start"] * 4,
                 PRIMITIVES + (value["start"] + packet_words(mode)) * 4),
                *((ORDERING + (SCENE_ENTRY + slot) * 4,
                   ORDERING + (SCENE_ENTRY + slot + 1) * 4)
                  for slot in ({FRAME: (2047,), DARKEN: (2046,), PALING: (8, 2046)}[mode]))]
    if mode == CAMERA:
        return [STACK, (POSITION, POSITION + 6)]
    if mode == SCREEN:
        return [STACK, (SCREEN_STEP, SCREEN_STEP + 1), (SCREEN_STATE, SCREEN_STATE + 12),
                (BACKUP_POINTER, BACKUP_POINTER + 4), (SCREEN_RECTS, SCREEN_RECTS + 16),
                (CURRENT_ROW, CURRENT_ROW + 4), (HUD_SLIDE, HUD_SLIDE + 4),
                (DESTINATION, DESTINATION + 1), (HIDE_MENU, HIDE_MENU + 1)]
    return [STACK, *((address, address + size) for _, address, size in VIEW),
            (SELECTED_MAP, SELECTED_MAP + 4), (LABEL_Y, LABEL_Y + 4),
            (SCENE_BUFFER, SCENE_BUFFER + 4), (SCENE_SLOT, SCENE_SLOT + 4),
            (ARM_BUFFER, ARM_BUFFER + 4), (ROOM_NAMES_POINTER, ROOM_NAMES_POINTER + 4)]


def check(image, value, observation, native, reference) -> list[str]:
    return compare(native, observation["values"]) + observation["problems"]


def describe(image, results) -> list[str]:
    mode = results[0][0]["mode"]
    if mode == CAMERA:
        mac_cases = sum(any(abs(component) > 32767 for component in original["values"][13:16])
                        for _, original, _ in results)
        return [f"Original yaw/pitch, sine/cosine, MulMatrix, ApplyMatrix, setters followed; "
                f"MAC-not-IR translation vectors={mac_cases}; camera pad preserved."]
    if mode in (FRAME, DARKEN, PALING):
        return ["Complete packet words, RGB/CLUT preservation, cursor, high-byte-preserving "
                "reverse links and canaries compared; original AddPrim/SetDrawStp/rsin followed."]
    return ["Every controller output and named endpoint argument compared; out-of-domain "
            "native refusals are separate Catch2 cases, not original parity vectors."]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    return {**mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]),
            "VECTORS": 1024, "COMPILER": "clang-cl",
            "COMPILER_OPTIONS": ("/W4", "/WX", "/Gy")}


# Symbolic scene/queue addresses; these represent ownership, not host pointers.
OLD_SCENE = 0x80570000
STAGING = 0x80580000
NEW_SCENE = 0x80590000
READ_SLOT = 0x805A0000
OLD_NAMES = 0x805B0000
NEW_NAMES = 0x805C0000
SCREEN_STEP = 0x805D0000
BACKUP = 0x805E0000
OLD_BACKUP = 0x805F0000

CURRENT_ROW = 0x801083F8
SCREEN_STATE = 0x80108D30
BACKUP_POINTER = 0x80108D3C
SCREEN_RECTS = 0x80108D44
HUD_SLIDE = 0x800EB9B0
DESTINATION = 0x800F51C0
HIDE_MENU = 0x801022D6
FADE_STEP = 0x801022D8
SELECTED_MAP = 0x80108D84
LABEL_Y = 0x80108E48
SCENE_SLOT = 0x80108DBC
ARM_BUFFER = 0x80108DC0
ROOM_NAMES_POINTER = 0x80108D98
SCENE_ROWS = 0x80108DC4
ARM_FILES = 0x80102B00
MAP_NAMES = 0x801083FC
PRESSED, HELD, REPEATED = 0x8005E1D0, 0x8005E1C0, 0x8005DFDC
STICKS = 0x8005DFC0
FIELD_YAW = SCRATCH + 0x58
ALLOCATE = 0x80043E3C
FREE = 0x80043C60
LOAD_BACKGROUND = 0x80102FCC
LOAD_SCENE = 0x80103418
PRIORITY = 0x80100414
ROWS_IDLE = 0x800FA9D0
TOGGLE_FADE = 0x800FFB68
FIELD_RENDER = 0x8008A4DC
HIDE_HUD = 0x800CB654
COMMAND = 0x800FFA88
CLEAR_ROWS = 0x800FA8E0
READ_IMAGE = 0x8002895C
CLEAR_IMAGE = 0x800287D4
LOAD_CLUT = 0x80048A64
UPLOAD_IMAGE = 0x800CCDA8
MENU_ENTRY = 0x80103684
SET_FAR = 0x80041558
SET_VISIBLE = 0x80102CD8
SOUND = 0x80045988
INIT_TEXT = 0x800CCF08
DISMISS_TEXT = 0x800CD064
POLL_AREAS = 0x8010451C
AREA_NAME = 0x80102F30
ALLOCATE_READ = 0x80044B10
ENQUEUE_READ = 0x80044BC4
FREE_READ = 0x80044B80
COPY = 0x800490B0
RESOLVE_NAMES = 0x80098F4C
RESTORE_VISITS = 0x8008B4D8
SCALE = 0x801060E0
INITIAL = 0x80106570
CENTER = 0x80105B18
ADJACENT = 0x80106618
TARGET = 0x80105D3C
ANIMATE = 0x80105EC0


def screen_vector(rng: random.Random, number: int) -> dict:
    value = {
        "step": rng.choice((*range(3, 11), *range(3, 11), 0, 2, 11, 255)),
        "screen": [rng.choice((0, 0, 1, 8, 10, -1, word(rng))),
                   rng.choice((0, 0, 1, 2)), rng.choice((0, 1, 2, -1))],
        "background": rng.choice((0, 1, -1)),
        "scene_result": rng.choice((0, 1, -1)),
        "ready": rng.choice((0, 1, 2, -1, word(rng))),
        "menu_result": rng.choice((0, 0, 1, 2, -1)),
        "fade": rng.choice((0, 0, 1, -1)),
        "row": word(rng), "hud": word(rng),
        "destination": rng.randrange(256), "hide": rng.randrange(256),
        "rects": [rng.randrange(-32768, 32768) for _ in range(8)],
    }
    if number < 16:
        value["step"] = (3, 4, 4, 5, 5, 5, 6, 7, 7, 8, 9, 9, 10, 10, 0, 5)[number]
        if number in (1, 2):
            value["background"] = number - 1
        elif number in (3, 4, 5):
            value["screen"] = [0, int(number == 3), 0]
            value["scene_result"] = int(number == 5)
        elif number in (7, 8):
            value["menu_result"] = (number - 7) * 2
        elif number in (10, 11):
            value["fade"] = int(number == 10)
        elif number in (12, 13):
            value["screen"][2] = 2 if number == 12 else 1
    return value


def run_screen(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    calls: list = []
    machine.put(SCREEN_STEP, value["step"], 1)
    for index, field in enumerate(value["screen"]):
        machine.put(SCREEN_STATE + index * 4, field)
    for index, field in enumerate(value["rects"]):
        machine.put(SCREEN_RECTS + index * 2, field, 2)
    for address, field, size in (
        (BACKUP_POINTER, OLD_BACKUP, 4), (FADE_STEP, value["fade"], 4),
        (CURRENT_ROW, value["row"], 4), (HUD_SLIDE, value["hud"], 4),
        (DESTINATION, value["destination"], 1), (HIDE_MENU, value["hide"], 1),
    ):
        machine.put(address, field, size)

    def rec(address: int, *args: int) -> None:
        record(calls, address, *args, machine.load(SCREEN_STEP, 1),
               *(s32(machine.load(SCREEN_STATE + index * 4)) for index in range(3)))

    def result(address: int, count: int, answer: int, signed: tuple = ()) -> None:
        def handler(m: Machine) -> None:
            rec(address, *(s32(m.r[4 + index]) if index in signed else m.r[4 + index]
                           for index in range(count)))
            m.r[2] = answer & MASK32
        machine.stub(address, handler)

    result(ALLOCATE, 1, BACKUP)
    result(FREE, 1, 0)
    result(LOAD_BACKGROUND, 1, value["background"])
    result(LOAD_SCENE, 0, value["scene_result"])
    result(ROWS_IDLE, 0, value["ready"])
    result(MENU_ENTRY, 0, value["menu_result"])
    for address, count, signed in (
        (TOGGLE_FADE, 1, ()), (FIELD_RENDER, 1, ()), (HIDE_HUD, 1, ()),
        (PRIORITY, 2, (0, 1)), (COMMAND, 1, ()), (CLEAR_ROWS, 1, ()),
        (LOAD_CLUT, 4, ()), (UPLOAD_IMAGE, 3, ()),
    ):
        result(address, count, 0, signed)

    def rectangle(m: Machine, address: int, clear: bool) -> None:
        args = [m.load(m.r[4] + index * 2, 2, True) for index in range(4)]
        args += list(m.r[5:8]) if clear else [m.r[5]]
        rec(address, *args)
    machine.stub(READ_IMAGE, lambda m: rectangle(m, READ_IMAGE, False))
    machine.stub(CLEAR_IMAGE, lambda m: rectangle(m, CLEAR_IMAGE, True))
    machine.r[4] = SCREEN_STEP
    machine.execute(entry)
    values = [s32(machine.r[2]), machine.load(SCREEN_STEP, 1)]
    values += [s32(machine.load(SCREEN_STATE + index * 4)) for index in range(3)]
    values += [s32(machine.load(CURRENT_ROW)), machine.load(BACKUP_POINTER)]
    values += [machine.load(SCREEN_RECTS + index * 2, 2, True) for index in range(8)]
    values += [s32(machine.load(HUD_SLIDE)), machine.load(DESTINATION, 1),
               machine.load(HIDE_MENU, 1)]
    inputs = [value["step"], *value["screen"]]
    inputs += [value[key] for key in ("background", "scene_result", "ready", "menu_result",
                                      "fade", "row", "hud", "destination", "hide")]
    return observe(machine, value, values, [*inputs, *value["rects"]], calls)


def menu_vector(rng: random.Random, number: int) -> dict:
    view = [rng.randrange(-32768, 32768) if size == 2 else word(rng)
            for _, _, size in VIEW]
    view[0], view[1] = rng.randrange(2), rng.randrange(2)
    view[10] = rng.randrange(8)
    view[11] = rng.randrange(32)
    view[12] = rng.choice((view[11], rng.randrange(32)))
    view[13] = rng.choice((0, 1, 1, 1, 1, 1, 2, 2, 3, 4, 4, 5, -1, 6))
    view[14] = rng.choice((0, 16, 32, 33, 48, 112, 127, 128, 144, -1))
    view[16] = rng.choice((-384, -64, -1, 0, 1, 320, 383, 384, 448, -96, 95, 96))
    view[17] = rng.choice((-80, -79, 0, 79, 80))
    view[18] = rng.choice((1, 2))
    view[19] = rng.choice((511, 512, 513, 768, 1023, 1024, 1025))
    view[2] = rng.choice((0, 1, 568, 1023, 1024, 1025, -1, view[2]))
    value = {
        "view": view, "selected": word(rng), "label": word(rng),
        "pressed": rng.choice((0, 0, 0, 0, 0x100, 0x20, 0x10, 0x40, 0x70)),
        "held": rng.choice((0, 1, 2, 3, 4, 8, 12, 0x800, 0x1001, 0x4001,
                            0x1002, 0x4002, 0x1000, 0x2000, 0x4000, 0x8000,
                            rng.randrange(65536))),
        "repeated": rng.choice((0, 4, 8, 12, rng.randrange(65536))),
        "sticks": [rng.choice((0, 63, 64, 65, 128, 191, 192, 193, 255))
                   for _ in range(4)],
        "yaw": rng.randrange(65536), "selection": rng.choice((-1, -2, -3, rng.randrange(32))),
        "read": rng.choice((0, 1, 2, 3, 4, 4, -1)),
        "initial": rng.randrange(8), "adjacent": rng.randrange(8),
        "draw_position": [rng.randrange(-32768, 32768) for _ in range(3)],
        "draw_animation": rng.randrange(3),
        "paling": [rng.choice((0, 0, 0, 1, 255)) for _ in range(32)],
        "rows": [rng.randrange(32) for _ in range(32)],
        "sizes": [rng.choice((2048, 4096, 6144, 8192, 10240)) for _ in range(32)],
        "names": [rng.randrange(32, 282) for _ in range(32)],
    }
    # A deterministic prefix reaches each phase and each controller route
    # before the mutant sample ends, independently of pseudo-random luck.
    prefix = number % 32
    if prefix < 20:
        view[13] = 1
        value["paling"][view[11]] = 0
        value["pressed"] = 0
        value["sticks"] = [128] * 4
        if prefix == 0:
            value["pressed"] = 0x100
        elif prefix in (1, 2, 3):
            value["pressed"] = (0x20, 0x10, 0x40)[prefix - 1]
        elif prefix == 4:
            value["sticks"][0] = 255
        elif prefix == 5:
            value["sticks"][1] = 0
        elif prefix in (6, 7):
            value["held"] = 1
            value["sticks"][2 + prefix - 6] = 255
        elif prefix in (8, 9):
            value["held"] = (0x8001, 0x4001)[prefix - 8]
        elif prefix in (10, 11):
            value["held"] = 2
            value["sticks"][3] = 0 if prefix == 10 else 255
        elif prefix == 12:
            value["held"] = 0x1002
        elif prefix == 13:
            value["held"] = 0x800
        elif prefix in (14, 15):
            value["held"] = 0
            value["sticks"][2 + prefix - 14] = 0
        elif prefix in (16, 17):
            value["held"] = (0x1000, 0x2000)[prefix - 16]
        else:
            value["held"] = 0
            value["repeated"] = 8 if prefix == 18 else 4
    else:
        view[13] = (0, 0, 2, 2, 2, 2, 3, 3, 4, 4, 5, 6)[prefix - 20]
        if prefix in (20, 21):
            view[14] = 128 if prefix == 20 else 32
        elif prefix in (22, 23, 24, 25):
            value["selection"] = (-1, -2, -3, 0)[prefix - 22]
        elif prefix in (26, 27):
            view[14] = 112 if prefix == 26 else 128
        elif prefix in (28, 29):
            view[16], value["read"] = (320, 4) if prefix == 28 else (319, 4)
    return value


def run_menu(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    calls: list = []
    for (_, address, size), field in zip(VIEW, value["view"], strict=True):
        machine.put(address, field, size)
    for address, field in (
        (SELECTED_MAP, value["selected"]), (LABEL_Y, value["label"]),
        (PRESSED, value["pressed"]), (HELD, value["held"]), (REPEATED, value["repeated"]),
        (SCENE_BUFFER, OLD_SCENE), (SCENE_SLOT, READ_SLOT),
        (ARM_BUFFER, STAGING), (ROOM_NAMES_POINTER, OLD_NAMES),
    ):
        machine.put(address, field)
    machine.put(FIELD_YAW, value["yaw"], 2)
    machine.put(READ_SLOT, value["read"], 2)
    for index, axis in enumerate(value["sticks"]):
        machine.put(STICKS + index * 4, axis)
    for index in range(32):
        machine.put(PALING_FLAGS + index, value["paling"][index], 1)
        machine.put(SCENE_ROWS + index * 4, value["rows"][index])
        machine.put(ARM_FILES + index * 8 + 4, value["sizes"][index])
        machine.put(MAP_NAMES + index * 2, value["names"][index], 2)

    def rec(address: int, *args: int) -> None:
        snapshot = [machine.load(VIEW[index][1], VIEW[index][2], True)
                    for index in (20, 11, 10, 13, 15)]
        record(calls, address, *args, *snapshot)

    def result(address: int, count: int, answer: int = 0, signed: tuple = ()) -> None:
        def handler(m: Machine) -> None:
            rec(address, *(s32(m.r[4 + index]) if index in signed else m.r[4 + index]
                           for index in range(count)))
            m.r[2] = answer & MASK32
        machine.stub(address, handler)

    for address, count, signed in (
        (SET_FAR, 3, ()), (PRIORITY, 2, (0, 1)), (SET_VISIBLE, 1, ()),
        (SOUND, 2, ()), (SET_TEXT, 2, ()), (DISMISS_TEXT, 1, ()),
        (AREA_NAME, 2, ()), (ENQUEUE_READ, 2, ()), (FREE_READ, 1, ()),
        (COPY, 3, ()), (FREE, 1, ()), (RESTORE_VISITS, 1, ()),
        (SCALE, 2, ()), (CENTER, 2, (1,)), (TARGET, 2, (1,)),
        (ANIMATE, 1, ()),
    ):
        result(address, count, signed=signed)
    result(POLL_AREAS, 1, value["selection"])
    result(ALLOCATE, 1, STAGING if value["view"][13] == 2 else NEW_SCENE)
    result(ALLOCATE_READ, 1, READ_SLOT)
    result(RESOLVE_NAMES, 1, NEW_NAMES)
    result(INITIAL, 1, value["initial"])

    def initialize_text(m: Machine) -> None:
        rec(INIT_TEXT, *m.r[4:8],
            *(m.load(m.r[29] + offset) for offset in (16, 20, 24, 28)))
    machine.stub(INIT_TEXT, initialize_text)

    def adjacent(m: Machine) -> None:
        rec(ADJACENT, s32(m.r[4]))
        m.put(VIEW[10][1], value["adjacent"])
    machine.stub(ADJACENT, adjacent)

    def draw(m: Machine) -> None:
        rec(DRAW_FRAME)
        for index, component in enumerate(value["draw_position"]):
            m.put(POSITION + index * 2, component, 2)
        m.put(VIEW[21][1], value["draw_animation"], 2)
    machine.stub(DRAW_FRAME, draw)
    machine.execute(entry)
    values = [s32(machine.r[2])]
    values += [machine.load(address, size, True) for _, address, size in VIEW]
    values += [s32(machine.load(SELECTED_MAP)), s32(machine.load(LABEL_Y))]
    values += [machine.load(address) for address in
               (SCENE_BUFFER, SCENE_SLOT, ARM_BUFFER, ROOM_NAMES_POINTER)]
    inputs = [*value["view"], value["selected"], value["label"]]
    inputs += [value[key] for key in ("pressed", "held", "repeated")]
    inputs += [*value["sticks"], value["yaw"]]
    inputs += [value[key] for key in ("selection", "read", "initial", "adjacent")]
    inputs += [*value["draw_position"], value["draw_animation"]]
    inputs += [*value["paling"], *value["rows"], *value["sizes"], *value["names"]]
    return observe(machine, value, values, inputs, calls)
