"""Shared harness for the SCREFF2 screen overlay differentials (GIM/SCREFF2.PRG 0.c and 5E8.c, #276).

Each case runs one original under the R3000 interpreter against its typed
VS::Effects::ScreenDistortion core (ScreenOverlays.cpp) in screen_distortion_driver.cpp.
The overlay controller (D_800EB9B4) starts with random words. The setters' texture page calls
are stubbed and record their arguments. The drawing originals follow vs_battle_setSprite
(BATTLE.PRG), rcos/rsin (SLUS_010.40) and the module's own ellipse and corner helpers.
Their packets go to a random offset in a primitive buffer, linked in front of ordering-table
entries that start random. The driver keeps the same words in a simulated PS1 memory: its
packet queues link handles as the low 24 bits of the packet address.

Native differences outside the compared domain (covered by ScreenOverlaysTests.cpp): the live
binding refuses a missing controller, refuses a vignette corner whose packets do not start at
the frame's primitive cursor, and drops packets that do not fit the frame's primitive buffer
or whose ordering-table entry lies outside the frame's table. The original wrote through all
of these.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Machine,
    mode_case,
    printed_problems,
    recorded_observation,
    s32,
)

MODULES = ("GIM/SCREFF2.PRG", "BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "screen_distortion_driver.cpp"
SOURCES = (
    r"Native\Game\src\Effects\ScreenDistortion\ScreenOverlays.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)

CONTROLLER_POINTER = 0x800EB9B4
CONTROLLER = 0x80500000
CONTROLLER_BYTES = 0x10
FRAME_BUFFER = 0x8005E210
SCRATCH = 0x1F800000
CURSOR, SCENE_TABLE, OVERLAY_TABLE = SCRATCH, SCRATCH + 4, SCRATCH + 8
PRIMITIVES = 0x80600000
PRIMITIVE_BYTES = 0x8000
MAX_START_WORDS = 16
TABLE = 0x80700000
# Room below the head for the feedback slot of layers 0-2 (slots -3 to -1).
HEAD = TABLE + 4 * 4
TABLE_BYTES = 4 * (4 + 0x10000 + 4)

RESERVE_PAGES, RETIRE_PAGES = 0x8007DFF0, 0x8007E0A8
FOLLOW = (0x800C0230, 0x80040F24, 0x80040F28, 0x800F9DF8, 0x800F9EBC)

(
    SET_LAYER,
    DRAW_TILES,
    SET_WARP,
    SET_VIGNETTE,
    ELLIPSE,
    CORNER,
    DRAW_VIGNETTE,
) = range(7)
SETTERS = (SET_LAYER, SET_WARP, SET_VIGNETTE)

# (offset, size, signed) of the controller fields the driver prints.
CONTROLLER_FIELDS = ((0, 1, False), (1, 1, False), (2, 2, False), (4, 2, False), (6, 2, True), (8, 4, True))
CONTROLLER_NAMES = ("loading", "pages reserved", "feedback layer", "warp scale", "warp growth", "vignette")

BOUNDARY_WORDS = (0, 1, -1, 2, 0xFFFF, 0x10000, 0x8000, -0x8000, 0x7FFFFFFF, -0x80000000)


def word(rng: random.Random) -> int:
    return rng.choice((*BOUNDARY_WORDS, s32(rng.getrandbits(32)), s32(rng.getrandbits(32)),
                       rng.randrange(-0x200, 0x200)))


def controller(rng: random.Random) -> list[int]:
    return [rng.randrange(256), rng.choice((0, 0, 1, 2, 0xFF, rng.randrange(256))), rng.randrange(1 << 16),
            rng.randrange(1 << 16), rng.randrange(-0x8000, 0x8000), s32(rng.getrandbits(32))]


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode, "controller": controller(rng), "start": rng.randrange(MAX_START_WORDS),
                   "head": rng.getrandbits(32), "slot_head": rng.getrandbits(32)}
    if mode == SET_LAYER:
        value["args"] = [rng.choice((0, 0, rng.randrange(1, 32), 0x10000, word(rng)))]
    elif mode == SET_WARP:
        value["args"] = [word(rng), word(rng)]
    elif mode == SET_VIGNETTE:
        value["args"] = [word(rng)]
    elif mode == ELLIPSE:
        value["args"] = [rng.choice((rng.randrange(-0x100, 0x100), rng.randrange(0x41), word(rng))),
                         rng.choice((0x80, 0x60, rng.randrange(-0x400, 0x400), word(rng)))]
    elif mode == CORNER:
        value["args"] = [word(rng), rng.choice((rng.randrange(-0x80, 0x80), 0x7FFFFFFE, 0x7FFFFFFC, 0x7FFFFFFF,
                                                -0x80000000, -2, word(rng)))]
    elif mode == DRAW_TILES:
        value["frame"] = rng.choice((0, 0, 1, 1, word(rng)))
        value["controller"][2] = rng.choice((0, 1, 2, 3, 4, 0xFFFF, rng.randrange(32), rng.randrange(1 << 16)))
    return value


def put_fields(machine: Machine, base: int, values: list[int]) -> None:
    for (offset, size, _), field in zip(CONTROLLER_FIELDS, values, strict=True):
        machine.put(base + offset, field, size)


def controller_values(machine: Machine) -> list[int]:
    return [machine.load(CONTROLLER + offset, size, signed=signed) for offset, size, signed in CONTROLLER_FIELDS]


def packet_values(machine: Machine, start: int, end: int, entries: list[int]) -> tuple[list[int], list[str]]:
    count = (end - start) // 4
    values = [s32(end - PRIMITIVES), count,
              *(s32(machine.load(start + 4 * index)) for index in range(count)),
              *(s32(machine.load(entry)) for entry in entries)]
    names = ["end", "words", *(f"word {index}" for index in range(count)),
             *(f"entry {index}" for index in range(len(entries)))]
    return values, names


def feedback_entry(layer: int) -> int:
    return (HEAD + 4 * (layer - 3)) & 0xFFFFFFFF


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    mode = value["mode"]
    calls: list[list[int]] = []
    for callee, kind in ((RESERVE_PAGES, 1), (RETIRE_PAGES, 2)):
        machine.stub(callee, lambda m, kind=kind: calls.append([kind, *(s32(m.r[4 + i]) for i in range(3))]))
    machine.follow.update(FOLLOW)
    machine.map_zero(CONTROLLER, CONTROLLER_BYTES)
    put_fields(machine, CONTROLLER, value["controller"])
    machine.put(CONTROLLER_POINTER, CONTROLLER)
    machine.map_zero(SCRATCH, 0x10)
    machine.map_zero(PRIMITIVES, PRIMITIVE_BYTES)
    machine.map_zero(TABLE, TABLE_BYTES)
    start = PRIMITIVES + 4 * value["start"]
    machine.put(CURSOR, start)
    machine.put(SCENE_TABLE, HEAD)
    machine.put(OVERLAY_TABLE, HEAD)
    machine.put(HEAD, value["head"])
    machine.put(FRAME_BUFFER, value.get("frame", 0))
    slot_entry = feedback_entry(value["controller"][2])
    if mode == DRAW_TILES:
        machine.put(slot_entry, value["slot_head"])
    for index, argument in enumerate(value.get("args", ())):
        machine.r[4 + index] = argument & 0xFFFFFFFF
    if mode == CORNER:
        machine.r[6], machine.r[7] = start, HEAD
    machine.execute(entry)

    inputs = [mode, *value["controller"], *value.get("args", ()), *([0] * (2 - len(value.get("args", ())))),
              value["start"], s32(value["head"]), s32(value["slot_head"]), s32(value.get("frame", 0))]
    if mode in SETTERS:
        values, names = controller_values(machine), list(CONTROLLER_NAMES)
    elif mode == ELLIPSE:
        values, names = [s32(machine.r[2])], ["point"]
    elif mode == CORNER:
        values, names = packet_values(machine, start, machine.r[2], [HEAD])
    elif mode == DRAW_VIGNETTE:
        values, names = packet_values(machine, start, machine.load(CURSOR), [HEAD])
    else:
        values, names = packet_values(machine, start, machine.load(CURSOR), [HEAD, slot_entry])
    return recorded_observation(machine, values, names, calls), inputs


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    called = sum(1 for _, observation, _ in results if observation["calls"])
    words = sum(observation["values"][1] for value, observation, _ in results
                if value["mode"] not in (*SETTERS, ELLIPSE))
    return [f"{called} of {len(results)} vectors called a texture page service, {words} packet words compared"]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if value["mode"] in SETTERS:
        allowed.append((CONTROLLER, CONTROLLER + CONTROLLER_BYTES))
    elif value["mode"] != ELLIPSE:
        allowed.append((PRIMITIVES, PRIMITIVES + PRIMITIVE_BYTES))
        allowed.append((HEAD, HEAD + 4))
        if value["mode"] != CORNER:
            allowed.append((CURSOR, CURSOR + 4))
        if value["mode"] == DRAW_TILES:
            slot_entry = feedback_entry(value["controller"][2])
            allowed.append((slot_entry, slot_entry + 4))
    return allowed


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
