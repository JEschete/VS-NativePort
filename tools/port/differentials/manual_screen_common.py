"""Shared harness for the manual screen differentials (MENUE.PRG 494.c:132-448, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::manual core
(ManualScreen.cpp) in manual_screen_driver.cpp. Every callee is stubbed at its call boundary and recorded
with its arguments; the driver's recording hosts print the same words.

func_80102E9C (CLEAR), _menuReady (READY) and vs_menuE_exec (SCREEN) draw the step byte, the open delay,
init flag and help result statics, the replies of _initMenuState, _showMenu and vs_mainmenu_ready, the
background fade step, the current menu, the menu hide flag and the HUD slide. The first vectors visit every
step 3-8 and an idle one.

_initMenuState (INIT) and _showMenu (PAGES) draw the help statics (_showMenuState, _scrollPosition,
_lineCount, _pageArrowAnimState, _helpPageLoadCounter, _helpText, _helpAssets and both disc request
pointers), both requests' states, the _topMenu reply, both cursor memory bytes, the asset header, the page
text box state, the _getRoundedLineCount reply, the pressed, previous and repeated buttons, both settings
bytes and vs_battle_manualDisplayState. The page drawing and text callees record the help statics they see
when called (step, scroll, line count, arrow frame, load counter and _helpText); sprintf formats into its
buffer and vs_battle_renderTextRawColor records the text it reads. The first vectors visit every step 0-7
and an idle one; most of the rest page through the text.

Native differences outside the compared domain (covered by ManualScreenTests.cpp): a topic row past the
help files (the original reads past _helpFileCdFiles), a shown topic past the scroll memories, loaded assets
without a buffer or a request without a slot (null reads) and a null step pointer are refused rather than
run.
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

MODULE_BASES["MENU/MENUE.PRG"] = 0x80102800

MODULES = ("MENU/MENUE.PRG",)
DRIVER = "manual_screen_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\ManualScreen.cpp",)
INCLUDES: tuple[str, ...] = ()

CLEAR, READY, SCREEN, INIT, PAGES = range(5)

# The originals' statics.
MENU_RESULT = 0x80105228
IS_INIT = 0x8010522C
OPEN_DELAY = 0x8010522D
TEXT_QUEUE = 0x80105238
ASSETS_QUEUE = 0x8010523C
HELP_STEP = 0x80105240
SCROLL = 0x80105244
LINE_COUNT = 0x80105248
ARROW = 0x8010524C
LOAD = 0x80105250
HELP_TEXT = 0x80105254
HELP_ASSETS = 0x80105258
HELP_STATICS_END = 0x8010525C

# Globals outside MENUE.PRG.
HUD_SLIDE = 0x800EB9B0
FADE_STEP = 0x801022D8
HIDE_MENU = 0x801022D6
CURRENT_MENU = 0x800F51C0
DISPLAY = 0x800F1BC8
DISPLAY_HALFWORDS = 18
CURSOR_MEMORIES = 0x800F4EE8
SETTINGS = 0x80060020
CURSOR_SETTING = SETTINGS + 1
PRESSED = 0x8005E1D0
PREVIOUS = 0x8005E1C0
REPEATED = 0x8005DFDC
SCRATCH = 0x1F800000

# Fixture addresses; the driver numbers its allocations and slots the same way.
STEP = 0x80300000
ORDERING = 0x80310000
TEXT_BOX = 0x80320000
TEXT_BOX_STATE = TEXT_BOX + 0x1C
TEXT_BUF = 0x80500000
ASSETS_BUF = 0x80510000
TEXT_SLOT = 0x80520000
ASSETS_SLOT = 0x80520010
NEW_SLOTS = 0x80530000
SLOT_STRIDE = 0x10
ALLOCATIONS = 0x80540000
ALLOCATION_STRIDE = 0x1000

# Callees.
CLEAR_MENU_EXCEPT = 0x800FA8E0
MENU_READY = 0x800FA9D0
TOGGLE_FADE = 0x800FFB68
INIT_MENU_STATE = 0x801030A4
SHOW_MENU = 0x80103110
FIELD_RENDERING = 0x8008A4DC
HIDE_HUD = 0x800CB654
SET_PRIORITY = 0x80100414
SET_MENU_COMMAND = 0x800FFA88
BZERO = 0x80049110
RESERVE_PAGES = 0x8007DFF0
RETIRE_PAGES = 0x8007E0A8
INIT_INFORMATION = 0x800FFBC8
DISMISS_INFORMATION = 0x800FFBA8
TOP_MENU = 0x8010391C
FREE_HEAP = 0x80043C60
ALLOC_HEAP = 0x80043E3C
ALLOC_SLOT = 0x80044B10
CD_ENQUEUE = 0x80044BC4
FREE_SLOT = 0x80044B80
COPY_SPRITES = 0x80103A24
COPY_CLUTS = 0x80103AD8
INIT_TEXT_BOX = 0x800CCF08
GET_TEXT_BOX = 0x800CCDF4
ROUNDED_LINE_COUNT = 0x80103B6C
PLAY_SFX = 0x80045988
RENDER_BUTTON = 0x800FFC68
RENDER_TEXT = 0x800C6540
SPRINTF = 0x80026EF4
CONTROLS_BACKGROUND = 0x80104CC4
PAGE_BACKGROUND = 0x80104204
FADE_UPPER = 0x8010435C
FADE_LOWER = 0x80104620
DRAW_CONTENT = 0x80103CF0
COPY_PAGE_TEXT = 0x80103BC8

# Call kinds, shared with manual_screen_driver.cpp.
(CALL_CLEAR, CALL_READY, CALL_FADE, CALL_INIT_HELP, CALL_PAGES, CALL_FIELD, CALL_HUD, CALL_PRIORITY,
 CALL_COMMAND, CALL_BZERO, CALL_RESERVE, CALL_RETIRE, CALL_INFO_INIT, CALL_DISMISS, CALL_TOPICS, CALL_FREE,
 CALL_ALLOC, CALL_SLOT, CALL_ENQUEUE, CALL_FREE_SLOT, CALL_SPRITES, CALL_CLUTS, CALL_TEXT_BOX, CALL_LINES,
 CALL_SFX, CALL_BUTTON, CALL_TEXT, CALL_CONTROLS_BACKGROUND, CALL_HIDE, CALL_PAGE_BACKGROUND, CALL_FADE_UP,
 CALL_FADE_DOWN, CALL_CONTENT, CALL_COPY) = range(1, 35)

MASK = 0xFFFFFFFF
TOPICS = 14
TEXT_LIMIT = 64

# The pad bits _showMenu reads.
TRIANGLE, CIRCLE, CROSS, SQUARE, START, UP, DOWN = 0x10, 0x20, 0x40, 0x80, 0x800, 0x1000, 0x4000


def word(rng: random.Random) -> int:
    return rng.randrange(-0x80000000, 0x80000000)


def screen_vector(rng: random.Random, number: int, mode: int) -> dict:
    steps = (3, 4, 5, 6, 7, 8, 0, rng.randrange(256))
    step = steps[number % len(steps)] if number < 2 * len(steps) else rng.choice(
        (3, 4, 4, 4, 5, 5, 6, 6, 6, 7, 8, 8, 8, 0, 1, 2, 9, rng.randrange(256)))
    return {
        "mode": mode,
        "step": step,
        "open_delay": rng.choice((0, 0, 0, 1, 2, 8, rng.randrange(256))),
        "initialized": rng.choice((0, 0, 1, 1, rng.randrange(256))),
        "help_result": rng.choice((0, 1, 2, 2, -1, word(rng))),
        "init_reply": rng.choice((0, 1, 1, 0x100, 0x101, -1, word(rng))),
        "pages_reply": rng.choice((0, 0, 1, 2, 2, -1, word(rng))),
        "ready_reply": rng.choice((0, 1, 1, -1, 2, 0xFF, word(rng))),
        "fade_step": rng.choice((0, 0, 0, 1, -1, word(rng))),
        "current_menu": rng.randrange(256),
        "hide_menu": rng.choice((0, 1, rng.randrange(256))),
        "hud_slide": word(rng),
    }


def pad(rng: random.Random) -> tuple[int, int, int]:
    """The pressed, previous and repeated button words: the paging and leaving combinations, mostly, with
    Square held for half of them."""
    pressed = rng.choice((0, UP, UP, UP, UP, UP, DOWN, DOWN, DOWN, DOWN, DOWN, START, CIRCLE, CROSS, TRIANGLE,
                          CIRCLE | TRIANGLE, START | UP, SQUARE, rng.randrange(0x10000), rng.getrandbits(32)))
    if rng.random() < 0.5:
        previous = rng.choice((SQUARE, SQUARE, SQUARE | UP, SQUARE | rng.getrandbits(32)))
    else:
        previous = rng.choice((0, 0, UP, rng.getrandbits(32) & ~SQUARE))
    repeated = rng.choice((0, 0, UP, UP, DOWN, DOWN, UP | DOWN, rng.getrandbits(32)))
    return pressed, previous, repeated


def page_position(rng: random.Random, number: int) -> tuple[int, int]:
    """The scroll position and line count: a third on the first or last page of a short text, a third inside a
    longer text, the rest at page edges, mostly, then any word."""
    if number % 3 == 0:
        lines = rng.choice((0, 0, 0, 5, 9, 10, 19, 20, 25, -5, -1, -1))
        tens = lines - lines % 10 if lines > 0 else 0
        return rng.choice((0, 0, lines, lines, rng.randrange(10), tens, 10, 1)), lines
    if number % 3 == 1:
        lines = rng.randrange(20, 80)
        tens = lines - lines % 10
        return rng.choice((10, 20, tens, tens - 10, lines, rng.randrange(lines + 1), 0)), lines
    lines = rng.choice((0, 0, 1, 9, 10, 11, 20, 25, 30, rng.randrange(80), rng.randrange(-30, 0), word(rng)))
    tens = lines - lines % 10 if lines > 0 else 0
    scroll = rng.choice((0, 0, lines, lines, lines - 1, 1, 9, 10, tens, tens - 10, rng.randrange(max(lines, 1) + 1),
                         rng.randrange(0x10000, 0x20000), rng.randrange(-20, 0), word(rng)))
    return s32(scroll & MASK), lines


def help_vector(rng: random.Random, number: int, mode: int) -> dict:
    steps = (0, 1, 2, 3, 4, 5, 6, 7, rng.choice((8, 9, -1, word(rng))))
    if number < 2 * len(steps):
        step = steps[number % len(steps)]
    else:
        step = rng.choice((0, 1, 2, 2, 2, 3, 3, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 7, 7, 8, -1))
    scroll, lines = page_position(rng, number)
    pressed, previous, repeated = pad(rng)
    # The steps that read a disc request or the asset header always have them (null reads otherwise).
    loading = step in (3, 4)
    text_buf = rng.choice((0, TEXT_BUF)) if step in (6, 7) else rng.choice((0, TEXT_BUF, TEXT_BUF))
    assets_buf = ASSETS_BUF if step == 3 else rng.choice((0, ASSETS_BUF))
    # The topic row stays within _helpFileCdFiles; the shown topic within the scroll memories while paging.
    display = [rng.randrange(0x10000) for _ in range(DISPLAY_HALFWORDS)]
    if step in (4, 5):
        display[0] = rng.choice((0, 13, rng.randrange(TOPICS)))
    elif rng.random() < 0.5:
        display[0] = rng.randrange(TOPICS)
    tiles = rng.choice((0, 1, 4, rng.randrange(64), rng.getrandbits(32)))
    return {
        "mode": mode,
        "step": step,
        "scroll": scroll,
        "lines": lines,
        "arrow": rng.choice((0, 7, 14, 15, 16, rng.randrange(256), word(rng))),
        "load": rng.choice((0, 0, 7, 7, 8, 8, 9, -1, rng.randrange(16), word(rng))),
        "assets_buf": assets_buf,
        "text_buf": text_buf,
        "assets_queue": ASSETS_SLOT if loading else rng.choice((0, ASSETS_SLOT)),
        "text_queue": TEXT_SLOT if loading else rng.choice((0, TEXT_SLOT)),
        "assets_state": rng.choice((4, 4, 4, 3, 0, 5, -1, rng.randrange(-0x8000, 0x8000))),
        "text_state": rng.choice((4, 4, 4, 3, 0, 5, -1, rng.randrange(-0x8000, 0x8000))),
        "topic_reply": rng.choice((-1, -1, -2, -3, -3, -4, 0, 1, 13, rng.randrange(TOPICS), rng.randrange(TOPICS),
                                   rng.randrange(-0x80000000, -1))),
        "cursor": [rng.randrange(256), rng.randrange(256)],
        "header": [tiles, rng.choice((0, 1, 16, rng.randrange(64), rng.getrandbits(32)))],
        "text_box_state": rng.choice((0, 1, 2, rng.randrange(0x10000))),
        "line_reply": rng.choice((0, 10, 23, rng.randrange(100), word(rng))),
        "pressed": pressed,
        "previous": previous,
        "repeated": repeated,
        "setting": rng.choice((0, 0, 1, rng.randrange(256))),
        "other_setting": rng.choice((0, 1, rng.randrange(256))),
        "display": display,
    }


def vector(rng: random.Random, number: int, mode: int) -> dict:
    if mode in (INIT, PAGES):
        return help_vector(rng, number, mode)
    return screen_vector(rng, number, mode)


class Recorder:
    def __init__(self, machine: Machine, value: dict) -> None:
        self.value = value
        self.calls: list[list[int]] = []
        self.slots = 0
        self.allocations = 0

        def record(kind: int, registers: int = 0):
            return lambda m: self.calls.append([kind, *(m.r[4 + index] for index in range(registers))])

        stubs = {
            CLEAR_MENU_EXCEPT: record(CALL_CLEAR, 1),
            MENU_READY: lambda m: self.reply(m, [CALL_READY], value.get("ready_reply", 0)),
            TOGGLE_FADE: record(CALL_FADE, 1),
            INIT_MENU_STATE: lambda m: self.reply(m, [CALL_INIT_HELP], value.get("init_reply", 0)),
            SHOW_MENU: lambda m: self.reply(m, [CALL_PAGES], value.get("pages_reply", 0)),
            FIELD_RENDERING: record(CALL_FIELD, 1),
            HIDE_HUD: record(CALL_HUD, 1),
            SET_PRIORITY: record(CALL_PRIORITY, 2),
            SET_MENU_COMMAND: record(CALL_COMMAND, 1),
            BZERO: self.bzero,
            RESERVE_PAGES: record(CALL_RESERVE, 3),
            RETIRE_PAGES: record(CALL_RETIRE, 3),
            INIT_INFORMATION: record(CALL_INFO_INIT),
            DISMISS_INFORMATION: record(CALL_DISMISS),
            TOP_MENU: lambda m: self.reply(m, [CALL_TOPICS, m.r[4]], value.get("topic_reply", 0)),
            FREE_HEAP: record(CALL_FREE, 1),
            ALLOC_HEAP: self.allocate,
            ALLOC_SLOT: self.allocate_slot,
            CD_ENQUEUE: record(CALL_ENQUEUE, 2),
            FREE_SLOT: record(CALL_FREE_SLOT, 1),
            COPY_SPRITES: lambda m: self.calls.append([CALL_SPRITES, (m.r[4] - ASSETS_BUF) & MASK, m.r[5]]),
            COPY_CLUTS: lambda m: self.calls.append([CALL_CLUTS, (m.r[4] - ASSETS_BUF) & MASK, m.r[5]]),
            INIT_TEXT_BOX: self.init_text_box,
            GET_TEXT_BOX: lambda m: self.reply(m, [CALL_HIDE, m.r[4]], TEXT_BOX),
            ROUNDED_LINE_COUNT: lambda m: self.reply(m, self.snapshot(m, CALL_LINES), value.get("line_reply", 0)),
            PLAY_SFX: record(CALL_SFX, 2),
            RENDER_BUTTON: lambda m: self.calls.append([CALL_BUTTON, m.r[4], m.r[5], m.r[6],
                                                        (m.r[7] - ORDERING) & MASK]),
            RENDER_TEXT: self.render_text,
            SPRINTF: self.sprintf,
            CONTROLS_BACKGROUND: record(CALL_CONTROLS_BACKGROUND, 4),
            PAGE_BACKGROUND: lambda m: self.calls.append([CALL_PAGE_BACKGROUND, *(m.r[r] for r in (4, 5, 6, 7)),
                                                          m.load(m.r[29] + 0x10)]),
            FADE_UPPER: lambda m: self.calls.append(self.snapshot(m, CALL_FADE_UP)),
            FADE_LOWER: lambda m: self.calls.append(self.snapshot(m, CALL_FADE_DOWN)),
            DRAW_CONTENT: lambda m: self.calls.append(self.snapshot(m, CALL_CONTENT)),
            COPY_PAGE_TEXT: lambda m: self.calls.append(self.snapshot(m, CALL_COPY)),
        }
        for address, stub in stubs.items():
            machine.stub(address, stub)

    def reply(self, machine: Machine, call: list[int], result: int) -> None:
        self.calls.append(call)
        machine.r[2] = result & MASK

    @staticmethod
    def snapshot(machine: Machine, kind: int) -> list[int]:
        """The help statics a page drawing or text callee reads."""
        return [kind, *(machine.load(address) for address in (HELP_STEP, SCROLL, LINE_COUNT, ARROW, LOAD, HELP_TEXT))]

    def bzero(self, machine: Machine) -> None:
        self.calls.append([CALL_BZERO, machine.r[4], machine.r[5]])
        machine.store(machine.r[4], bytes(min(machine.r[5], 0x100)))

    def allocate(self, machine: Machine) -> None:
        self.reply(machine, [CALL_ALLOC, machine.r[4]], ALLOCATIONS + ALLOCATION_STRIDE * self.allocations)
        self.allocations += 1

    def allocate_slot(self, machine: Machine) -> None:
        file = machine.r[4]
        self.reply(machine, [CALL_SLOT, machine.load(file), machine.load(file + 4)],
                   NEW_SLOTS + SLOT_STRIDE * self.slots)
        self.slots += 1

    def init_text_box(self, machine: Machine) -> None:
        stack = [machine.load(machine.r[29] + offset) for offset in (0x10, 0x14, 0x18, 0x1C)]
        self.calls.append([CALL_TEXT_BOX, *(machine.r[r] for r in (4, 5, 6, 7)), *stack])

    @staticmethod
    def string(machine: Machine, address: int) -> bytes:
        text = bytearray()
        while len(text) < TEXT_LIMIT and (byte := machine.byte(address + len(text))):
            text.append(byte)
        return bytes(text)

    def sprintf(self, machine: Machine) -> None:
        formatted = self.string(machine, machine.r[5]).replace(b"%d", str(s32(machine.r[6])).encode())
        machine.store(machine.r[4], formatted + b"\0")

    def render_text(self, machine: Machine) -> None:
        text = self.string(machine, machine.r[4])
        self.calls.append([CALL_TEXT, len(text), *text, machine.r[5], machine.r[6], (machine.r[7] - ORDERING) & MASK])


def run_screen(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    recorder = Recorder(machine, value)
    machine.put(STEP, value["step"], 1)
    machine.put(OPEN_DELAY, value["open_delay"], 1)
    machine.put(IS_INIT, value["initialized"], 1)
    machine.put(MENU_RESULT, value["help_result"])
    machine.put(FADE_STEP, value["fade_step"])
    machine.put(CURRENT_MENU, value["current_menu"], 1)
    machine.put(HIDE_MENU, value["hide_menu"], 1)
    machine.put(HUD_SLIDE, value["hud_slide"])
    machine.r[4] = STEP
    machine.execute(entry)
    result = machine.r[2] if mode == SCREEN else 0
    values = [result, machine.load(STEP, 1), machine.load(OPEN_DELAY, 1), machine.load(IS_INIT, 1),
              machine.load(MENU_RESULT), machine.load(CURRENT_MENU, 1), machine.load(HIDE_MENU, 1),
              machine.load(HUD_SLIDE)]
    names = ["return", "step", "open delay", "init flag", "help result", "current menu", "hide menu", "HUD slide"]
    inputs = [value[key] for key in ("step", "open_delay", "initialized", "help_result", "init_reply", "pages_reply",
                                     "ready_reply", "fade_step", "current_menu", "hide_menu", "hud_slide")]
    return recorded_observation(machine, values, names, recorder.calls), [mode, *inputs]


HELP_STATICS = (("step", HELP_STEP), ("scroll", SCROLL), ("lines", LINE_COUNT), ("arrow", ARROW), ("load", LOAD),
                ("text_buf", HELP_TEXT), ("assets_buf", HELP_ASSETS), ("text_queue", TEXT_QUEUE),
                ("assets_queue", ASSETS_QUEUE))


def run_help(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    recorder = Recorder(machine, value)
    for key, address in HELP_STATICS:
        machine.put(address, value[key])
    machine.map_zero(TEXT_SLOT, 0x20)
    machine.put(TEXT_SLOT, value["text_state"], 2)
    machine.put(ASSETS_SLOT, value["assets_state"], 2)
    machine.map_zero(ASSETS_BUF, 0x10)
    machine.put(ASSETS_BUF, value["header"][0])
    machine.put(ASSETS_BUF + 4, value["header"][1])
    machine.map_zero(TEXT_BOX, 0x40)
    machine.put(TEXT_BOX_STATE, value["text_box_state"], 2)
    machine.put(CURSOR_MEMORIES + 4, value["cursor"][0], 1)
    machine.put(CURSOR_MEMORIES + 5, value["cursor"][1], 1)
    machine.put(SETTINGS, value["other_setting"], 1)
    machine.put(CURSOR_SETTING, value["setting"], 1)
    machine.put(PRESSED, value["pressed"])
    machine.put(PREVIOUS, value["previous"])
    machine.put(REPEATED, value["repeated"])
    machine.map_zero(SCRATCH, 0x10)
    machine.put(SCRATCH + 4, ORDERING)
    for index, halfword in enumerate(value["display"]):
        machine.put(DISPLAY + 2 * index, halfword, 2)
    machine.execute(entry)
    values = [machine.r[2], *(machine.load(address) for _, address in HELP_STATICS), machine.load(TEXT_BOX_STATE, 2),
              *(machine.load(DISPLAY + 2 * index, 2) for index in range(DISPLAY_HALFWORDS))]
    names = ["return", *(key for key, _ in HELP_STATICS), "text box state",
             *(f"display halfword {index}" for index in range(DISPLAY_HALFWORDS))]
    inputs = [value[key] for key in ("step", "scroll", "lines", "arrow", "load", "assets_buf", "text_buf",
                                     "assets_queue", "text_queue", "assets_state", "text_state", "topic_reply")]
    inputs += [*value["cursor"], *value["header"]]
    inputs += [value[key] for key in ("text_box_state", "line_reply", "pressed", "previous", "repeated", "setting",
                                      "other_setting")]
    inputs += value["display"]
    return recorded_observation(machine, values, names, recorder.calls), [mode, *inputs]


def run(machine: Machine, value: dict, entry: int) -> tuple:
    if value["mode"] in (INIT, PAGES):
        return run_help(machine, value, entry)
    return run_screen(machine, value, entry)


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    stack = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)
    if value["mode"] == INIT:
        return [stack, (HELP_STEP, HELP_STATICS_END)]
    if value["mode"] == PAGES:
        return [stack, (TEXT_QUEUE, HELP_STATICS_END), (DISPLAY, DISPLAY + 2 * DISPLAY_HALFWORDS),
                (TEXT_BOX_STATE, TEXT_BOX_STATE + 2)]
    return [stack, (STEP, STEP + 1), (MENU_RESULT, OPEN_DELAY + 1), (HUD_SLIDE, HUD_SLIDE + 4),
            (CURRENT_MENU, CURRENT_MENU + 1), (HIDE_MENU, HIDE_MENU + 1)]


CALL_NAMES = {
    CALL_CLEAR: "clears", CALL_READY: "ready polls", CALL_FADE: "fades", CALL_INIT_HELP: "help inits",
    CALL_PAGES: "page updates", CALL_FIELD: "field toggles", CALL_HUD: "HUD toggles", CALL_PRIORITY: "priorities",
    CALL_COMMAND: "commands", CALL_BZERO: "display clears", CALL_RESERVE: "page loans",
    CALL_RETIRE: "page retirements", CALL_INFO_INIT: "information boxes", CALL_DISMISS: "dismissals",
    CALL_TOPICS: "topic menus", CALL_FREE: "frees", CALL_ALLOC: "allocations", CALL_SLOT: "slots",
    CALL_ENQUEUE: "requests", CALL_FREE_SLOT: "slot releases", CALL_SPRITES: "sprite uploads",
    CALL_CLUTS: "CLUT uploads", CALL_TEXT_BOX: "text boxes", CALL_LINES: "line counts", CALL_SFX: "sounds",
    CALL_BUTTON: "buttons", CALL_TEXT: "texts", CALL_CONTROLS_BACKGROUND: "control backgrounds",
    CALL_HIDE: "text box hides", CALL_PAGE_BACKGROUND: "page backgrounds", CALL_FADE_UP: "upper fades",
    CALL_FADE_DOWN: "lower fades", CALL_CONTENT: "contents", CALL_COPY: "page copies",
}


def describe(image, results) -> list[str]:
    kinds: dict[int, int] = {}
    steps: dict[int, int] = {}
    for value, observation, _ in results:
        steps[s32(value["step"])] = steps.get(s32(value["step"]), 0) + 1
        for call in observation["calls"]:
            kinds[call[0]] = kinds.get(call[0], 0) + 1
    shown = sorted((step, count) for step, count in steps.items() if -2 <= step <= 9)
    return ["steps: " + ", ".join(f"{step}: {count}" for step, count in shown),
            ", ".join(f"{count} {CALL_NAMES[kind]}" for kind, count in sorted(kinds.items())) or "no calls"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]) | {
        "INCLUDES": INCLUDES, "VECTORS": 1024, "MUTANT_VECTORS": 256}
