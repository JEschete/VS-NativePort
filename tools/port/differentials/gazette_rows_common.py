"""Shared harness for the gazette row differentials (MENU9.PRG 238.c:54-267 and 1430-1471, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::Score core
(GazetteRows.cpp) in gazette_rows_driver.cpp.

_renderScrollableMenuItem (RENDER) draws the frame buffer word, the two animation statics at 0x8010851C,
the row's state, width, background, fade, scroll flag, position and text (always ended within its 32
bytes), the glyph widths the label stub answers with and the gradient stub's salt. The stubs record the
arrow, glyphs, gradient stops, shadow tile and reflection sprites; the arrow's UV word and each sprite's
draw mode and UV words, which the original writes into the returned packets, are read after the run. The
LINE_G2 packets are read from the primitive buffer at the scratch cursor (their words after the tag).

_setRows (SET_ROWS) draws the row count (0-256), the absolute row, the cursor memory setting, both cursor
memory bytes and the text pointer; vs_mainmenu_setMenuRows records its count, row info, text, the setting
it sees and how many row types are nonzero. _getSelectedRow (SELECTED) draws the selected row reply and the
cursor row byte. _topMenu (TOP_MENU) follows _setRows and _getSelectedRow and draws its statics, the
initialize flag, the gazette row, the first four _menuText offsets and the replies; its rows call records
the four (text, 0) pairs it built. _clearMenuExceptRow8 (CLEAR_ROW_8) and _menuReady (READY) record their
one call.

Native differences outside the compared domain (covered by GazetteRowsTests.cpp): text without an end
within the row, row counts outside 0-256 and null pointers are refused (the original reads past the text
or sizes its stack array with the count).
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

MODULE_BASES["MENU/MENU9.PRG"] = 0x80102800

MODULES = ("MENU/MENU9.PRG",)
DRIVER = "gazette_rows_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\Score\GazetteRows.cpp",)
INCLUDES: tuple[str, ...] = ()

RENDER, SET_ROWS, SELECTED, TOP_MENU, CLEAR_ROW_8, READY = range(6)

SET_ROWS_ENTRY = 0x80102E10
SELECTED_ENTRY = 0x80102F0C

FRAME_BUF = 0x8005E210
PREVIOUS_FRAME = 0x8010851C
ANIMATION_STEP = 0x8010851D
SCRATCH = 0x1F800000
CURSOR_MEMORIES = 0x800F4EE8
SETTINGS = 0x80060020
CURSOR_SETTING = SETTINGS + 1
GAZETTE_ROW = 0x800F1BF7
MENU_TEXT = 0x80108520
TOP_STEP = 0x8010A224
TOP_SELECTED = 0x8010A228

# Fixture addresses; the driver prints the same constants.
ROW = 0x80300000
ROW_BYTES = 0x40
TEXT_OFFSET = 0x1C
TEXT_BYTES = 32
ORDERING = 0x80304000
PRIMITIVES = 0x80310000
LINE_BYTES = 28
LINES = 12
SPRITES = 0x80320000
SPRITE_BYTES = 0x20
ARROW_PACKET = 0x80330000
TEXT_LIST = 0x80340000

SET_SPRITE_DEFAULT_PAGE = 0x800C0224
SET_SPRITE = 0x800C0230
RENDER_ROW_LABEL = 0x80101268
GRADIENT_STOP = 0x800C8FAC
ADD_TILE = 0x800CCCB8
SET_MENU_ROWS = 0x801005E0
GET_SELECTED_ROW = 0x801008C8
CLEAR_MENU_EXCEPT = 0x800FA8E0
FLY_OUT = 0x800FA92C
MENU_READY = 0x800FA9D0

(CALL_ARROW, CALL_GLYPH, CALL_GRADIENT, CALL_TILE, CALL_SPRITE, CALL_SHOW, CALL_SHOW_LIST, CALL_SELECT,
 CALL_CLEAR, CALL_FLY_OUT, CALL_READY) = range(1, 12)

MASK = 0xFFFFFFFF


def gradient_color(salt: int, gradient: int, color_index: int, brightness: int) -> int:
    """The gradient stub's answer; the driver computes the same."""
    return ((gradient * 0x9E37 + color_index * 0x85EB + brightness * 0xC2B3) ^ salt) & MASK


def row_text(rng: random.Random) -> list[int]:
    text: list[int] = []
    for _ in range(rng.choice((0, 1, 3, rng.randrange(16), rng.randrange(28)))):
        if rng.random() < 0.15:
            text += [0xFA, rng.choice((0, 2, 0xFF, rng.randrange(256)))]
        else:
            text.append(rng.choice((0, 0x80, 0xE7, 0xF9, 0xFB, 0xFE, rng.randrange(0xFA))))
    text = text[:TEXT_BYTES - 1]
    if text and text[-1] == 0xFA:
        text[-1] = 0
    text.append(0xFF)
    return text + [rng.randrange(256) for _ in range(TEXT_BYTES - len(text))]


def vector(rng: random.Random, number: int, mode: int) -> dict:
    if mode == RENDER:
        return {
            "mode": mode,
            "frame": rng.choice((0, 1, 0, 1, 2, 0x100)),
            "previous": rng.choice((0, 1, rng.randrange(256))),
            "step": rng.choice((0, 3, 4, 7, 8, 10, 11, 12, rng.randrange(256))),
            "state": rng.choice((1, 1, 1, 0, 2, rng.randrange(256))),
            "w": rng.choice((0, 0x7F, 0xFF, rng.randrange(256), rng.randrange(256))),
            "background": rng.randrange(256),
            "fade": rng.choice((0, 1, 8, rng.randrange(9), rng.randrange(256))),
            "scroll": rng.choice((0, 1, 1, 2, 2, rng.randrange(256))),
            "x": rng.choice((0, 13, 14, 127, 128, rng.randrange(128), rng.randrange(-0x8000, 0x8000))),
            "y": rng.choice((0, 18, rng.randrange(240), rng.randrange(-0x8000, 0x8000))),
            "text": row_text(rng),
            "widths": [rng.choice((0, 6, rng.randrange(-8, 16))) for _ in range(4)],
            "salt": rng.randrange(1 << 32),
            "ordering": rng.randrange(1 << 32),
        }
    small = number % 2 == 0
    count = rng.choice((0, 1, 4, 8, 9, 10, 16, 256, rng.randrange(20))) if small else rng.randrange(257)
    row = rng.choice((0, 3, 4, 7, 8, rng.randrange(-4, 24), rng.randrange(-300, 300)))
    if number % 4 == 0:
        # The page boundaries: a list just over a page, with rows about the last page.
        count = rng.choice((8, 9, 16, 17))
        row = rng.choice((count - 9, count - 8, count - 1, count, count + 1, 7, 8))
    return {
        "mode": mode,
        "count": count,
        "row": row,
        "setting": rng.choice((0, 1, rng.randrange(256))),
        "cursor": [rng.randrange(256), rng.randrange(256)],
        "text": TEXT_LIST,
        "selected": rng.choice((-1, -1, -2, -3, 0, 1, 3, rng.randrange(-0x80000000, 0x80000000))),
        "initialize": rng.choice((0, 0, 1, rng.randrange(-5, 5))),
        "top_step": rng.choice((0, 1, 1, 2, -1, rng.randrange(-0x80000000, 0x80000000))),
        "top_selected": rng.randrange(-0x80000000, 0x80000000),
        "gazette_row": rng.choice((0, 3, 8, rng.randrange(256))),
        "menu_text": [rng.choice((4, 0x20, rng.randrange(0x10000))) for _ in range(4)],
        "ready": rng.randrange(-4, 4),
    }


class Recorder:
    def __init__(self, machine: Machine, value: dict) -> None:
        self.value = value
        self.calls: list[list[int]] = []
        self.sprites: list[tuple[list[int], int]] = []
        self.arrow: list[int] | None = None
        self.glyphs = 0
        stubs = {
            SET_SPRITE_DEFAULT_PAGE: self.draw_arrow,
            SET_SPRITE: self.draw_sprite,
            RENDER_ROW_LABEL: self.draw_glyph,
            GRADIENT_STOP: self.gradient,
            ADD_TILE: lambda m: self.calls.append([CALL_TILE, *(m.r[r] for r in (4, 5, 6, 7))]),
            SET_MENU_ROWS: self.show,
            GET_SELECTED_ROW: lambda m: self.reply(m, [CALL_SELECT], value["selected"]),
            CLEAR_MENU_EXCEPT: lambda m: self.calls.append([CALL_CLEAR, s32(m.r[4])]),
            FLY_OUT: lambda m: self.calls.append([CALL_FLY_OUT, s32(m.r[4]), s32(m.r[5])]),
            MENU_READY: lambda m: self.reply(m, [CALL_READY], value["ready"]),
        }
        for address, stub in stubs.items():
            machine.stub(address, stub)

    def reply(self, machine: Machine, call: list[int], result: int) -> None:
        self.calls.append(call)
        machine.r[2] = result & MASK

    def draw_arrow(self, machine: Machine) -> None:
        self.arrow = [CALL_ARROW, *(machine.r[r] for r in (4, 5, 6, 7))]
        self.calls.append(self.arrow)
        machine.r[2] = ARROW_PACKET

    def draw_sprite(self, machine: Machine) -> None:
        packet = SPRITES + SPRITE_BYTES * len(self.sprites)
        call = [CALL_SPRITE, *(machine.r[r] for r in (4, 5, 6, 7))]
        self.sprites.append((call, packet))
        self.calls.append(call)
        machine.r[2] = packet

    def draw_glyph(self, machine: Machine) -> None:
        self.calls.append([CALL_GLYPH, *(machine.r[r] for r in (4, 5, 6, 7))])
        machine.r[2] = (machine.r[5] + self.value["widths"][self.glyphs % 4]) & MASK
        self.glyphs += 1

    def gradient(self, machine: Machine) -> None:
        arguments = [machine.r[r] for r in (4, 5, 6)]
        self.calls.append([CALL_GRADIENT, *arguments])
        machine.r[2] = gradient_color(self.value["salt"], *arguments)

    def show(self, machine: Machine) -> None:
        count = s32(machine.r[4])
        types = sum(1 for index in range(max(count, 0)) if machine.load(machine.r[7] + 4 * index))
        setting = machine.load(CURSOR_SETTING, 1)
        if self.value["mode"] == TOP_MENU:
            pairs = [machine.load(machine.r[6] + 4 * index) for index in range(8)]
            self.calls.append([CALL_SHOW_LIST, count, machine.r[5], *pairs, setting, types])
        else:
            self.calls.append([CALL_SHOW, count, machine.r[5], machine.r[6], setting, types])

    def finish(self, machine: Machine) -> None:
        """Appends the words the original wrote into the returned packets."""
        if self.arrow is not None:
            self.arrow.append(machine.load(ARROW_PACKET + 0x10))
        for call, packet in self.sprites:
            call += [machine.load(packet + 4), machine.load(packet + 0x10)]


def setup_render(machine: Machine, value: dict) -> None:
    machine.map_zero(SCRATCH, 0x10)
    machine.map_zero(ORDERING, 0x10)
    machine.map_zero(PRIMITIVES, LINE_BYTES * LINES)
    machine.map_zero(SPRITES, SPRITE_BYTES * LINES)
    machine.map_zero(ARROW_PACKET, 0x20)
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 8, ORDERING)
    machine.put(ORDERING + 8, value["ordering"])
    machine.put(FRAME_BUF, value["frame"])
    machine.put(PREVIOUS_FRAME, value["previous"], 1)
    machine.put(ANIMATION_STEP, value["step"], 1)
    row = bytearray(ROW_BYTES)
    row[0:6] = bytes((value["state"], value["w"], value["background"], 0, value["fade"], value["scroll"]))
    row[0x14:0x16] = (value["x"] & 0xFFFF).to_bytes(2, "little")
    row[0x16:0x18] = (value["y"] & 0xFFFF).to_bytes(2, "little")
    row[TEXT_OFFSET:TEXT_OFFSET + TEXT_BYTES] = bytes(value["text"])
    machine.store(ROW, bytes(row))


def setup_menu(machine: Machine, value: dict) -> None:
    machine.put(CURSOR_MEMORIES + 2, value["cursor"][0], 1)
    machine.put(CURSOR_MEMORIES + 3, value["cursor"][1], 1)
    machine.put(CURSOR_SETTING, value["setting"], 1)
    machine.put(GAZETTE_ROW, value["gazette_row"], 1)
    machine.put(TOP_STEP, value["top_step"])
    machine.put(TOP_SELECTED, value["top_selected"])
    for index, offset in enumerate(value["menu_text"]):
        machine.put(MENU_TEXT + 2 * index, offset, 2)


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    recorder = Recorder(machine, value)
    if mode == RENDER:
        setup_render(machine, value)
        machine.r[4] = ROW
        machine.execute(entry)
        recorder.finish(machine)
        lines = (machine.load(SCRATCH) - PRIMITIVES) // LINE_BYTES
        words = [machine.load(PRIMITIVES + LINE_BYTES * line + 4 * word)
                 for line in range(lines) for word in range(1, 7)]
        values = [machine.load(PREVIOUS_FRAME, 1), machine.load(ANIMATION_STEP, 1), machine.load(ROW + 4, 1),
                  lines, *words]
        names = ["previous frame", "step", "fade", "lines", *(f"line {i // 6} word {i % 6 + 1}" for i in
                                                              range(len(words)))]
        inputs = [value[key] for key in ("frame", "previous", "step", "state", "w", "background", "fade",
                                         "scroll", "x", "y")]
        inputs += [*value["text"], *value["widths"], value["salt"]]
        return recorded_observation(machine, values, names, recorder.calls), [mode, *inputs]

    setup_menu(machine, value)
    if mode == TOP_MENU:
        machine.follow.update((SET_ROWS_ENTRY, SELECTED_ENTRY))
    machine.r[4] = {SET_ROWS: value["count"], TOP_MENU: value["initialize"]}.get(mode, 0) & MASK
    machine.r[5] = value["row"] & MASK
    machine.r[6] = value["text"]
    machine.execute(entry)
    result = s32(machine.r[2]) if mode in (SELECTED, TOP_MENU) else 0
    values = [result, s32(machine.load(TOP_STEP)), s32(machine.load(TOP_SELECTED)),
              machine.load(CURSOR_MEMORIES + 2, 1), machine.load(CURSOR_MEMORIES + 3, 1),
              machine.load(CURSOR_SETTING, 1)]
    names = ["return", "top step", "top selected", "cursor row", "first row", "setting"]
    inputs = [value["count"], value["row"], value["setting"], *value["cursor"], value["text"], value["selected"],
              value["initialize"], value["top_step"], value["top_selected"], value["gazette_row"],
              *value["menu_text"], value["ready"]]
    return recorded_observation(machine, values, names, recorder.calls), [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    stack = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)
    if value["mode"] == RENDER:
        return [stack, (PREVIOUS_FRAME, PREVIOUS_FRAME + 2), (ROW + 4, ROW + 5), (SCRATCH, SCRATCH + 4),
                (ORDERING + 8, ORDERING + 12), (PRIMITIVES, PRIMITIVES + LINE_BYTES * LINES),
                (SPRITES, SPRITES + SPRITE_BYTES * LINES), (ARROW_PACKET, ARROW_PACKET + 0x20)]
    return [stack, (CURSOR_MEMORIES + 2, CURSOR_MEMORIES + 4), (CURSOR_SETTING, CURSOR_SETTING + 1),
            (TOP_STEP, TOP_SELECTED + 4)]


def describe(image, results) -> list[str]:
    kinds = {}
    for _, observation, _ in results:
        for call in observation["calls"]:
            kinds[call[0]] = kinds.get(call[0], 0) + 1
    names = {CALL_ARROW: "arrows", CALL_GLYPH: "glyphs", CALL_GRADIENT: "gradient stops", CALL_TILE: "tiles",
             CALL_SPRITE: "reflections", CALL_SHOW: "row lists", CALL_SHOW_LIST: "choice lists",
             CALL_SELECT: "selection polls", CALL_CLEAR: "clears", CALL_FLY_OUT: "fly-outs", CALL_READY: "polls"}
    return [", ".join(f"{count} {names[kind]}" for kind, count in sorted(kinds.items())) or "no calls"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]) | {
        "INCLUDES": INCLUDES, "VECTORS": 512}
