"""Shared harness for the event-script fade, display and palette opcode differentials (BATTLE.PRG
4A0A8.c:2049-2149, #276).

Each case runs one original opcode handler under the R3000 interpreter against its typed VS::Event
core (ScriptScreenCommands.cpp) in event_screen_commands_driver.cpp. The original's instruction lies at
every alignment between random neighbour bytes, so a handler reading the wrong operand, or reading one
with the wrong width or signedness, differs. The stubs record each screen callee in call order with its
argument registers (and the palette transforms' stacked arguments) as signed words; the transition phase
poll returns the vector's phase, and the actor selection returns the vector's ids, with random upper
halves, until its end sentinel 1.

Native differences outside the compared domain (covered by EventScriptVmTests.cpp): an instruction
shorter than its opcode's operands is refused without reading them, and the host returns the selected
actor id already narrowed to the 16 bits the original masks.
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
DRIVER = "event_screen_commands_driver.cpp"
SOURCES = (r"Native\Game\src\Event\ScriptScreenCommands.cpp",)
# The core reads the actor-selection sentinel from ScriptActorCommands.hpp, whose actor headers need Content.
INCLUDES = (r"Native\Content\include",)

SCRIPT = 0x80500000
SCRIPT_BYTES = 0x40
NEIGHBOURS = 8
GET_SHORT = 0x800BFE00
SELECTION_END = 1

(
    FADE_OUT,
    FADE_IN,
    WIPE_ANGLE,
    CONFIGURE,
    DISPLAY_FLAGS,
    WAIT_TRANSITION,
    PALETTE_HOOK,
    TRANSFORM_PALETTES,
) = range(8)

# Each mode's opcode byte and length (the BATTLE.PRG size table at 0x800E9C30).
OPCODES = {
    FADE_OUT: (0x69, 4),
    FADE_IN: (0x6A, 4),
    WIPE_ANGLE: (0x6B, 2),
    CONFIGURE: (0x6C, 4),
    DISPLAY_FLAGS: (0x6D, 2),
    WAIT_TRANSITION: (0x6E, 1),
    PALETTE_HOOK: (0x6F, 2),
    TRANSFORM_PALETTES: (0x70, 8),
}

PHASE_ADDRESS = 0x8007C398
NEXT_ACTOR_ADDRESS = 0x800BB788
# The kinds the driver prints for each call, the argument registers each callee takes and how many of
# its further arguments lie on the caller's stack from sp+0x10.
CALLS = {
    0x8007C46C: (1, 3, 0),  # fade out
    0x8007C4AC: (2, 3, 0),  # fade in
    0x8007C460: (3, 1, 0),  # wipe angle
    0x8007C444: (4, 3, 0),  # transition style
    0x8007C424: (5, 0, 0),  # display off
    0x8007C404: (6, 0, 0),  # display on
    0x8007BF4C: (7, 0, 0),  # primary room geometry hidden
    0x8007BF2C: (8, 0, 0),  # primary room geometry shown
    0x8007C050: (9, 0, 0),  # actor models off
    0x8007BFC0: (10, 0, 0),  # actor models on
    0x8007B104: (11, 0, 0),  # nonzero palette hook
    0x8007B0FC: (12, 0, 0),  # zero palette hook
    0x800BB68C: (13, 1, 0),  # actor selection
    0x8007B29C: (14, 4, 2),  # actor colours
    0x8007B1B8: (15, 4, 1),  # scene colours
    0x8007B344: (16, 4, 1),  # background colours
}
PHASE_CALL = 17
NEXT_ACTOR_CALL = 18

BOUNDARY_BYTES = (0, 1, 0x7F, 0x80, 0xFF)
# The transition phases: idle, the fade-out's covered screen, others and words with upper bits.
PHASES = (0, 0, 5, 5, 1, 4, 6, 7, -1, 0x10000, 0x10005)
# Shift amounts around the five-bit duration mask and the sllv modulo.
SHIFTS = (0, 1, 4, 5, 5, 31, 32, 33, 36, 37, 0x80, 0xFF)
# Target nibbles: actors, scene, background, and unused targets.
TARGETS = (0, 0, 0, 1, 1, 2, 2, 3, 0xF)


def operand_byte(rng: random.Random) -> int:
    return rng.choice((*BOUNDARY_BYTES, rng.randrange(256), rng.randrange(256)))


def actor_id(rng: random.Random) -> int:
    """A selected actor's id: never the sentinel in its low half, sometimes with a random upper half."""
    low = rng.choice((0, 2, 0x7FFF, 0x8000, 0xFFFF, rng.randrange(2, 1 << 16)))
    return s32((rng.choice((0, 0, 1, rng.randrange(1 << 16))) << 16) | low)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    opcode, length = OPCODES[mode]
    operands = [operand_byte(rng) for _ in range(length - 1)]
    if mode == DISPLAY_FLAGS:
        operands = [rng.randrange(256)]
    elif mode == TRANSFORM_PALETTES:
        operands[0] = (rng.choice(TARGETS) << 4) | rng.randrange(16)
        operands[3] = rng.choice((*SHIFTS, rng.randrange(256)))
    return {
        "mode": mode,
        "alignment": number % 4,
        "bytes": [opcode, *operands],
        "before": [rng.randrange(256) for _ in range(NEIGHBOURS)],
        "after": [rng.randrange(256) for _ in range(NEIGHBOURS)],
        "phase": s32(rng.choice((*PHASES, rng.randrange(1 << 32)))),
        "actors": [actor_id(rng) for _ in range(rng.choice((0, 1, 2, 3)))],
    }


def recorder(calls: list[list[int]], value: dict):
    def record(kind: int, count: int, stacked: int):
        def stub(machine: Machine) -> None:
            words = [s32(machine.r[4 + index]) for index in range(count)]
            words += [machine.load(machine.r[29] + 0x10 + index * 4, 4, signed=True) for index in range(stacked)]
            calls.append([kind, *words])

        return stub

    def phase(machine: Machine) -> None:
        calls.append([PHASE_CALL])
        machine.r[2] = value["phase"] & 0xFFFFFFFF

    cursor = iter(value["actors"])

    def next_actor(machine: Machine) -> None:
        calls.append([NEXT_ACTOR_CALL])
        machine.r[2] = next(cursor, SELECTION_END) & 0xFFFFFFFF

    stubs = {address: record(*shape) for address, shape in CALLS.items()}
    stubs[PHASE_ADDRESS] = phase
    stubs[NEXT_ACTOR_ADDRESS] = next_actor
    return stubs


def run(machine: Machine, value: dict, entry: int) -> tuple:
    machine.map_zero(SCRIPT, SCRIPT_BYTES)
    address = SCRIPT + NEIGHBOURS + value["alignment"]
    machine.store(address - NEIGHBOURS, bytes(value["before"]))
    machine.store(address, bytes(value["bytes"]))
    machine.store(address + len(value["bytes"]), bytes(value["after"]))
    calls: list[list[int]] = []
    for callee, stub in recorder(calls, value).items():
        machine.stub(callee, stub)
    machine.follow.add(GET_SHORT)
    machine.r[4] = address
    machine.execute(entry)

    values = [s32(machine.r[2]), 0]
    names = ["flow", "refusals"]
    inputs = [
        value["mode"],
        value["phase"],
        len(value["actors"]),
        *value["actors"],
        len(value["bytes"]),
        *value["bytes"],
    ]
    return recorded_observation(machine, values, names, calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    return [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    if not results:
        return []
    held = sum(1 for _, observation, _ in results if observation["values"][0])
    lines = [f"{held} of {len(results)} vectors held the script"]
    mode = results[0][0]["mode"]
    kinds = sorted({call[0] for _, observation, _ in results for call in observation["calls"]})
    lines.append(f"call kinds made: {kinds}")
    if mode == TRANSFORM_PALETTES:
        actors = sum(1 for _, observation, _ in results for call in observation["calls"] if call[0] == 14)
        lines.append(f"{actors} actor colour transforms across {len(results)} vectors")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return {**mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]), "INCLUDES": INCLUDES}
