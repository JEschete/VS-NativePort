"""Original Gazette terminal instructions against the typed production cores.

The six encyclopedia cases observe 79 complete 44-byte records (including an
untouched guard), framework-row bytes, signed selection/highlight state, cursor,
ordered text/model/projection endpoints and complete NUL-terminated ASCII text.
Number/navigation cases compare real UiTexturedQuad packets, both ordering heads,
primitive cursor, untouched UV/RGB fields and surrounding guards.

The controller/screen cases add every owned scalar, persistent cursor bytes,
textbox bytes, raw file aliases and endpoint-time state snapshots. External
framework, disc, actor, progress and renderer services are explicit doubles.
Native malformed-input/ownership refusals are separate from original parity.
Metadata imports neither optional disassemblers nor extracted data.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MODULE_BASES,
    Fault,
    Machine,
    mode_case,
    recorded_observation,
    s32,
)
from gazette_statistics_common import Recorder as GazetteRowRecorder
from gazette_times_common import c_string, install_formatters
from manual_illustration_common import add_prim

MODULE_BASES["MENU/MENU9.PRG"] = 0x80102800
MODULES = ("MENU/MENU9.PRG",)
DRIVER = "gazette_terminal_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Score\GazetteEncyclopedia.cpp",
    r"Native\Game\src\Menu\Score\GazetteTerminal.cpp",
    r"Native\Engine\src\Core\String\GameText.cpp",
    r"Native\Engine\src\Render\UiTexturedQuads.cpp",
)
INCLUDES = (r"Native\Tests\include",)
DESCRIPTION, NUMBER, INITIALIZE, LIST, VIEWER, NAVIGATION, CONTROLLER, SCREEN = range(8)
MASK = 0xFFFFFFFF
ENEMIES, ENTRY_BYTES, ENEMY_COUNT = 0x80310000, 44, 79
DESCRIPTIONS = ENEMIES + 150 * ENTRY_BYTES
MON_DATA, DESCRIPTION_POINTER = 0x8010A470, 0x8010A474
SELECTED, HIDDEN, NAV_STEP = 0x8010A450, 0x8010A45C, 0x8010A438
ROTATION_COLOR, ZOOM_COLOR = 0x8010A458, 0x8010A454
CURSOR, CURSOR_MEMORY, SCORE = 0x80109899, 0x800F1BF0, 0x8005FEA0
MISC = 0x801097DC
MENU_ROW, MENU_BYTES = 0x80302000, 60
PRIMITIVES, ORDERING, SCRATCH = 0x80303000, 0x80304000, 0x1F800000
SCENE_ORDERING, GUARD_BYTES = ORDERING + 0x100, 16
GET_ROW, SET_ROW, SET_INFO = 0x800C8E48, 0x800C8E5C, 0x800FFC04
DRAW_CURSOR, DRAW_NUMBER = 0x800FFCDC, 0x80105BCC
DRAW_ROW, DRAW_SCROLL, ADD_PRIM = 0x800C9078, 0x80102A7C, 0x8002B4D4
GET_PROJECTION, SET_PROJECTION = 0x800994D4, 0x800994F4
BUILD_CAMERA, DRAW_PREVIEW, EDGE_FADE = 0x80104CBC, 0x800F9EB8, 0x80106528
DRAW_BUTTON, DRAW_TEXT = 0x800FFC68, 0x800C6540
BANNER, PANEL = 0x80104F74, 0x801051AC


def word(rng: random.Random, number: int) -> int:
    edges = (0, 1, 7, 8, 9, 63, 64, 65, 127, 128, 255, 32767, 32768,
             65535, -1, -32768, -0x80000000, 0x7FFFFFFF)
    return edges[number % len(edges)] if number % 3 else s32(rng.getrandbits(32))


def enemies(rng: random.Random, number: int) -> list[list[int]]:
    result = []
    for index in range(ENEMY_COUNT):
        first = rng.randrange(832)
        count = rng.choice((-32768, -1, 0, 1, 2, 4, 8))
        if count > 0:
            first = min(first, 832 - count)
        fields = [rng.randrange(-32768, 32768), index % 6, first, count,
                  rng.choice((0, 0, 1, -1, -32768)),
                  rng.choice((-32768, -1, 0, 1, 7, 8, 127, 255, 32767))]
        raw = list(b"".join((value & 0xFFFF).to_bytes(2, "little") for value in fields))
        raw += [0 if (number + index) % 3 == 0 else rng.randrange(1, 256),
                (index - 1) % 78, (index + 1) % 78, rng.randrange(256)]
        name = [0xFA, 8] + [rng.randrange(0xE7) for _ in range(26)]
        name[2 + (number + index) % 26] = 0xE7
        raw += name
        result.append(raw)
    return result


def vector(rng: random.Random, number: int, mode: int) -> dict:
    if mode in (CONTROLLER, SCREEN):
        from gazette_terminal_control import vector as control_vector
        return control_vector(rng, number, mode)
    if mode in (NUMBER, NAVIGATION):
        count = 3 if mode == NUMBER else 1
        return {
            "mode": mode, "x": word(rng, number), "y": word(rng, number + 5),
            "number": (number * 37) % 1000,
            "dimmed": 0 if number % 2 == 0 else rng.choice((-1, 1, 256)),
            "direction": number % 4, "step": word(rng, number + 1),
            "hidden": 0 if number % 5 else rng.choice((1, -1, 256)),
            "ordering": rng.getrandbits(32), "other_ordering": rng.getrandbits(32),
            "primitives": [rng.getrandbits(32) for _ in range(count * 10 + 8)],
        }
    flags = [rng.getrandbits(32) for _ in range(26)]
    if number % 4 == 0:
        flags = [0] * 26
    elif number % 4 == 1:
        flags = [MASK] * 26
    elif number % 4 == 2:
        flags = [0] * 26
        flags[number % 26] = 1 << (number % 32)
    rows = enemies(rng, number)
    if mode == INITIALIZE and number % 4 == 2:
        # A known one-entry ring as well as sparse randomly intersecting ranges.
        for row in rows[:78]:
            row[6:8] = [0, 0]
        chosen = number % 78
        bit = (number % 26) * 32 + number % 32
        rows[chosen][4:8] = [bit & 255, bit >> 8, 1, 0]
    return {
        "mode": mode, "rows": rows, "flags": flags, "selected": number % 78,
        "step": 8 if number % 3 == 0 else word(rng, number + 4),
        "page": number % 71, "row": 255 if number % 17 == 0 else (number // 71) % 8,
        "cursor": rng.randrange(256), "cursor_reply": s32(rng.getrandbits(32)),
        "menu": [[rng.randrange(256) for _ in range(MENU_BYTES)] for _ in range(8)],
        "transition": (0, 1, 2, 3, -1, 4, -0x80000000, 0x7FFFFFFF)[number % 8],
        "hidden": 0 if number % 3 else rng.choice((1, -1, 256)),
        "rotation": rng.getrandbits(32), "zoom": rng.getrandbits(32),
        "projection": [word(rng, number + 7), word(rng, number + 9)],
        "camera": [rng.randrange(-32768, 32768) for _ in range(9)] +
                  [s32(rng.getrandbits(32)) for _ in range(3)],
    }


def text(machine: Machine, address: int) -> list[int]:
    result = []
    for index in range(256):
        byte = machine.load(address + index, 1)
        result.append(byte)
        if byte == 0xE7:
            return result
    raise Fault("unterminated game text")


def setup_enemies(machine: Machine, value: dict) -> None:
    for index, row in enumerate(value["rows"]):
        machine.store(ENEMIES + index * ENTRY_BYTES, bytes(row))
        offset = 256 + index * 4
        machine.put(DESCRIPTIONS + index * 2, offset, 2)
        machine.store(DESCRIPTIONS + offset * 2, bytes((index, 0x40, 0xE7)))
    machine.put(MON_DATA, ENEMIES)
    machine.put(DESCRIPTION_POINTER, DESCRIPTIONS)
    machine.put(MISC + 4, 8, 2)
    machine.put(MISC + 6, 12, 2)
    machine.store(MISC + 16, bytes((0x20, 0x21, 0xE7)))
    machine.store(MISC + 24, bytes((0xA7, 0xA7, 0xE7)))


def enemy_observation(machine: Machine) -> list[int]:
    return list(machine.bytes(ENEMIES, ENEMY_COUNT * ENTRY_BYTES))


def flatten(rows) -> list[int]:
    return [item for row in rows for item in row]


class Recorder(GazetteRowRecorder):
    def __init__(self, machine: Machine, value: dict) -> None:
        self.calls: list[list[int]] = []
        self.value = value
        self.created = 0
        self.projection = list(value.get("projection", (0, 0)))
        machine.stub(GET_ROW, self.get_row)
        machine.stub(SET_ROW, self.create_row)
        machine.stub(SET_INFO, self.information)
        machine.stub(DRAW_CURSOR, self.cursor)
        machine.stub(DRAW_NUMBER, self.number)
        machine.stub(DRAW_ROW, lambda m: self.draw(m, 6))
        machine.stub(DRAW_SCROLL, lambda m: self.draw(m, 7))
        machine.stub(GET_PROJECTION, self.get_projection)
        machine.stub(SET_PROJECTION, self.set_projection)
        machine.stub(BUILD_CAMERA, self.build_camera)
        machine.stub(DRAW_PREVIEW, self.preview)
        machine.stub(EDGE_FADE, lambda m: self.calls.append([12]))
        machine.stub(DRAW_BUTTON, self.button)
        machine.stub(DRAW_TEXT, self.draw_text)
        machine.stub(BANNER, lambda m: self.panel(m, 15))
        machine.stub(PANEL, lambda m: self.panel(m, 16))

    def information(self, machine: Machine) -> None:
        content = text(machine, machine.r[4])
        self.calls.append([3, len(content), *content])

    def get_projection(self, machine: Machine) -> None:
        self.calls.append([8])
        machine.put(machine.r[4], self.projection[0])
        machine.put(machine.r[4] + 4, self.projection[1])

    def set_projection(self, machine: Machine) -> None:
        self.projection = [machine.load(machine.r[4] + offset, signed=True) for offset in (0, 4)]
        self.calls.append([9, *self.projection])

    def build_camera(self, machine: Machine) -> None:
        self.calls.append([10])
        for index, value in enumerate(self.value["camera"][:9]):
            machine.put(machine.r[4] + index * 2, value, 2)
        machine.put(machine.r[4] + 18, 0xAA55, 2)
        for index, value in enumerate(self.value["camera"][9:]):
            machine.put(machine.r[4] + 20 + index * 4, value)

    def preview(self, machine: Machine) -> None:
        matrix = [machine.load(machine.r[4] + index * 2, 2, signed=True) for index in range(9)]
        matrix += [machine.load(machine.r[4] + 20 + index * 4, signed=True) for index in range(3)]
        self.calls.append([11, *matrix])

    def button(self, machine: Machine) -> None:
        self.calls.append([13, *(s32(machine.r[i]) for i in range(4, 7)),
                           (machine.r[7] - SCENE_ORDERING) // 4])

    def draw_text(self, machine: Machine) -> None:
        content = c_string(machine, machine.r[4]) + b"\0"
        layer = 0 if machine.r[7] == 0 else (machine.r[7] - SCENE_ORDERING) // 4
        self.calls.append([14, machine.r[5], machine.r[6], layer, len(content), *content])

    def panel(self, machine: Machine, kind: int) -> None:
        self.calls.append([kind, *(s32(machine.r[i]) for i in range(4, 8)),
                           s32(machine.load(machine.r[29] + 16))])


def observe(machine: Machine, values: list[int], calls: list[list[int]], prefix: str = "state") -> dict:
    return recorded_observation(machine, values,
                                [f"{prefix} word/byte {index}" for index in range(len(values))], calls)


def run_primitives(machine: Machine, value: dict, entry: int) -> tuple:
    install_formatters(machine)
    machine.stub(ADD_PRIM, add_prim)
    machine.map_zero(SCRATCH, 12)
    machine.put(SCRATCH, PRIMITIVES + GUARD_BYTES)
    machine.put(SCRATCH + 4, SCENE_ORDERING)
    machine.put(SCRATCH + 8, ORDERING)
    number = value["mode"] == NUMBER
    primary, other = (ORDERING, SCENE_ORDERING) if number else (SCENE_ORDERING, ORDERING)
    machine.put(primary, value["ordering"])
    machine.put(other, value["other_ordering"])
    for index, item in enumerate(value["primitives"]):
        machine.put(PRIMITIVES + index * 4, item)
    calls = []
    if number:
        args = [value["x"], value["y"], value["number"], value["dimmed"]]
        for register, item in zip(range(4, 8), args, strict=True):
            machine.r[register] = item & MASK
    else:
        args = [value["direction"], value["step"], value["hidden"]]
        machine.r[4] = value["direction"]
        machine.put(NAV_STEP, value["step"])
        machine.put(HIDDEN, value["hidden"])
        calls = Recorder(machine, value).calls
    machine.execute(entry)
    values = [machine.load(SCRATCH), machine.load(primary), machine.load(other)]
    values += [machine.load(PRIMITIVES + index * 4) for index in range(len(value["primitives"]))]
    inputs = [value["mode"], *args, value["ordering"], value["other_ordering"],
              *value["primitives"]]
    return observe(machine, values, calls, "packet"), inputs


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    if mode in (CONTROLLER, SCREEN):
        from gazette_terminal_control import run as control_run
        return control_run(machine, value, entry)
    if mode in (NUMBER, NAVIGATION):
        return run_primitives(machine, value, entry)
    setup_enemies(machine, value)
    machine.put(SELECTED, value["selected"], 2)
    install_formatters(machine)
    recorder = Recorder(machine, value)
    values = []
    if mode in (DESCRIPTION, INITIALIZE):
        inputs = [mode, value["selected"], *flatten(value["rows"]), *value["flags"]]
        for index, flag in enumerate(value["flags"]):
            machine.put(SCORE + 0x9C + index * 4, flag)
    elif mode == LIST:
        inputs = [mode, value["step"], value["page"], value["row"],
                  value["cursor"], value["cursor_reply"], *flatten(value["rows"]),
                  *flatten(value["menu"])]
        machine.put(CURSOR_MEMORY + 2, value["page"], 1)
        machine.put(CURSOR_MEMORY + 3, value["row"], 1)
        machine.put(CURSOR, value["cursor"], 1)
        machine.r[4] = value["step"] & MASK
    else:
        inputs = [mode, value["selected"], value["step"], value["transition"], value["hidden"],
                  value["rotation"], value["zoom"], *value["projection"], *value["camera"],
                  *flatten(value["rows"]), *flatten(value["menu"])]
        machine.put(HIDDEN, value["hidden"])
        machine.put(ROTATION_COLOR, value["rotation"])
        machine.put(ZOOM_COLOR, value["zoom"])
        machine.map_zero(SCRATCH, 12)
        machine.put(SCRATCH + 4, SCENE_ORDERING)
        machine.r[4] = value["step"] & MASK
        machine.r[5] = value["transition"] & MASK
    machine.store(MENU_ROW, bytes(value["menu"][0]))
    machine.execute(entry)
    if mode == LIST:
        values.append(machine.load(CURSOR, 1))
    elif mode == VIEWER:
        values.extend(recorder.projection)
    values += enemy_observation(machine)
    if mode in (LIST, VIEWER):
        values += list(machine.bytes(MENU_ROW, MENU_BYTES))
    return observe(machine, values, recorder.calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    mode = value["mode"]
    stack = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK + 4)
    if mode in (CONTROLLER, SCREEN):
        from gazette_terminal_control import allowed_stores as control_stores
        return control_stores(value)
    if mode in (NUMBER, NAVIGATION):
        count = 3 if mode == NUMBER else 1
        return [stack, (SCRATCH, SCRATCH + 4),
                (PRIMITIVES + GUARD_BYTES, PRIMITIVES + GUARD_BYTES + count * 40)]
    stores = [stack]
    if mode in (INITIALIZE, LIST):
        stores.append((ENEMIES, ENEMIES + 78 * ENTRY_BYTES))
    if mode in (LIST, VIEWER):
        stores.append((MENU_ROW, MENU_ROW + MENU_BYTES))
    if mode == LIST:
        stores.append((CURSOR, CURSOR + 1))
    return stores


def describe(image, results) -> list[str]:
    calls = {}
    phases = {}
    for value, observation, _ in results:
        for call in observation["calls"]:
            calls[call[0]] = calls.get(call[0], 0) + 1
        if "state" in value:
            phase = value["state"][0]
            phases[phase] = phases.get(phase, 0) + 1
    return [f"ordered endpoint coverage by kind: {calls}",
            f"controller/screen entry phase coverage: {phases}",
            "full packet/record/state and text payload comparison; bounded native refusals separate"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]) | {
        "INCLUDES": INCLUDES,
        "VECTORS": 4096 if mode == CONTROLLER else 1024,
        "MUTANT_VECTORS": 1024 if mode == CONTROLLER else 128,
        "COMPILER": "clang-cl",
        "COMPILER_OPTIONS": ("/W4", "/WX", "/clang:-Wall", "/clang:-Wextra",
                             "/clang:-Wconversion", "/clang:-Wsign-conversion"),
    }
