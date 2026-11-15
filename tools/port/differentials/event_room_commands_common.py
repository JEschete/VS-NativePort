"""Shared harness for the event-script room opcode differentials (BATTLE.PRG 4A0A8.c:1966-2268, #276).

Each case runs one original opcode handler under the R3000 interpreter against its typed
VS::Event core (ScriptRoomCommands.cpp) in event_room_commands_driver.cpp. The original's
instruction lies at every alignment between random neighbour bytes, so a handler reading the
wrong operand, or reading one with the wrong width or signedness, differs. The stubs record the
room callees' argument registers as signed words; the backdrop byte (D_800F4C6A) and the draw mode
word (D_800F19C8) start at random sentinels, and the driver's host stores them as LiveVmHost does.

Native differences outside the compared domain (covered by EventScriptVmTests.cpp): an instruction
shorter than its opcode's operands is refused without reading them.
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

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "event_room_commands_driver.cpp"
SOURCES = (r"Native\Game\src\Event\ScriptRoomCommands.cpp",)

SCRIPT = 0x80500000
SCRIPT_BYTES = 0x40
NEIGHBOURS = 6
BACKDROP = 0x800F4C6A
DRAW_MODE = 0x800F19C8
GET_SHORT = 0x800BFE00

PLATFORM_ADDRESS = 0x8008E224
LOD_DEPTH_ADDRESS = 0x80098194
SHOW_ADDRESS = 0x80091998
HIDE_ADDRESS = 0x800919D8
SWAP_ADDRESS = 0x80091A1C
COLLISION_ADDRESS = 0x8008B808
DEPTH_BIAS_ADDRESS = 0x80091A78

(
    GIMMICK,
    DEPTH_CUE,
    LIGHTING,
    SHOW,
    HIDE,
    SWAP,
    COLLISION,
    DEPTH_BIAS,
) = range(8)

# Each mode's opcode byte and length (the BATTLE.PRG size table at 0x800E9C30).
OPCODES = {
    GIMMICK: (0x60, 3),
    DEPTH_CUE: (0x61, 3),
    LIGHTING: (0x62, 2),
    SHOW: (0x64, 2),
    HIDE: (0x65, 2),
    SWAP: (0x66, 3),
    COLLISION: (0x67, 5),
    DEPTH_BIAS: (0x7D, 3),
}

# The kinds the driver prints for each call, and the argument registers each callee takes.
CALLS = {
    PLATFORM_ADDRESS: (1, 2),
    LOD_DEPTH_ADDRESS: (2, 1),
    SHOW_ADDRESS: (3, 1),
    HIDE_ADDRESS: (4, 1),
    SWAP_ADDRESS: (5, 2),
    COLLISION_ADDRESS: (6, 4),
    DEPTH_BIAS_ADDRESS: (7, 2),
}

BOUNDARY_BYTES = (0, 1, 0x7F, 0x80, 0xFF)
# Sign bit and depth mask edges of opcode 0x61's halfword.
DEPTH_CUE_HALVES = (0, 1, 0x7FF, 0x800, 0x7FFF, 0x8000, 0x8001, 0x87FF, 0x8800, 0xFBFF, 0xFFFF)


def operand_byte(rng: random.Random) -> int:
    return rng.choice((*BOUNDARY_BYTES, rng.randrange(256), rng.randrange(256)))


def vector(rng: random.Random, number: int, mode: int) -> dict:
    opcode, length = OPCODES[mode]
    operands = [operand_byte(rng) for _ in range(length - 1)]
    if mode == DEPTH_CUE:
        half = rng.choice((*DEPTH_CUE_HALVES, rng.randrange(1 << 16), rng.randrange(1 << 16)))
        operands = [half & 0xFF, half >> 8]
    return {
        "mode": mode,
        "alignment": number % 4,
        "bytes": [opcode, *operands],
        "before": [rng.randrange(256) for _ in range(NEIGHBOURS)],
        "after": [rng.randrange(256) for _ in range(NEIGHBOURS)],
        "backdrop": rng.choice((0, 0xFF, 0x5A, rng.randrange(256))),
        "draw_mode": s32(rng.choice((0, 0xFFFFFFFF, 0x12345678, rng.randrange(1 << 32)))),
    }


def recorder(calls: list[list[int]]):
    def record(kind: int, count: int):
        def stub(machine: Machine) -> None:
            calls.append([kind, *(s32(machine.r[4 + index]) for index in range(count))])

        return stub

    return {address: record(kind, count) for address, (kind, count) in CALLS.items()}


def run(machine: Machine, value: dict, entry: int) -> tuple:
    machine.map_zero(SCRIPT, SCRIPT_BYTES)
    address = SCRIPT + NEIGHBOURS + value["alignment"]
    machine.store(address - NEIGHBOURS, bytes(value["before"]))
    machine.store(address, bytes(value["bytes"]))
    machine.store(address + len(value["bytes"]), bytes(value["after"]))
    machine.put(BACKDROP, value["backdrop"], 1)
    machine.put(DRAW_MODE, value["draw_mode"], 4)
    calls: list[list[int]] = []
    for callee, stub in recorder(calls).items():
        machine.stub(callee, stub)
    machine.follow.add(GET_SHORT)
    machine.r[4] = address
    machine.execute(entry)

    values = [
        s32(machine.r[2]),
        0,
        machine.load(BACKDROP, 1),
        machine.load(DRAW_MODE, 4, signed=True),
    ]
    names = ["flow", "refusals", "backdrop", "draw mode"]
    inputs = [
        value["mode"],
        value["backdrop"],
        value["draw_mode"],
        len(value["bytes"]),
        *value["bytes"],
    ]
    return recorded_observation(machine, values, names, calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if value["mode"] == DEPTH_CUE:
        allowed.append((BACKDROP, BACKDROP + 1))
    elif value["mode"] == LIGHTING:
        allowed.append((DRAW_MODE, DRAW_MODE + 4))
    return allowed


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    if not results:
        return []
    called = sum(1 for _, observation, _ in results if observation["calls"])
    lines = [f"{called} of {len(results)} vectors made a room call"]
    mode = results[0][0]["mode"]
    if mode == DEPTH_CUE:
        kept = sum(1 for value, _, _ in results if value["bytes"][2] & 0x80)
        lines.append(f"{kept} of {len(results)} vectors set the depth cue sign bit")
    elif mode == LIGHTING:
        changed = sum(1 for value, observation, _ in results if observation["values"][3] != value["draw_mode"])
        lines.append(f"{changed} of {len(results)} vectors changed the draw mode word")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
