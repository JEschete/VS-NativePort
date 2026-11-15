"""Shared harness for the standalone presentation and actor effect callback differentials (BATTLE.PRG
146C.c:2149-5064, #276).

Each case runs one original callback under the R3000 interpreter against its typed VS::Effects core
(ActionEffectCallbacks.cpp) in action_callbacks_driver.cpp. The stubs record each callee in call order with its
arguments and the tinted actor (D_800F185C), field control mode (D_800F196C) and tick speed (vs_gametime_tickspeed)
it sees, so a global stored on the wrong side of a call differs: the actor animation advance (func_8009D854), the
actor palette transform (func_8007B29C, with its two stack arguments), the primary effect begin (func_800CF0E8, with
the request fields the standalone effect defines), the HUD (func_800CB654), the minimap (func_8008B4C8) and the
return to exploration (func_8006F5CC). The final globals and func_80074294's result are compared too. The caller's
stack is filled with random bytes first, so a request field the original fails to write differs from the native one,
while fields it never writes (the endpoints' colour presets and padding) are not compared.

Native differences outside the compared domain (covered by ActionEffectCallbacksTests.cpp): request fields the
original left as stack bytes are zero.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Machine,
    mode_case,
    recorded_observation,
    s32,
)

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "action_callbacks_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\ActionEffectCallbacks.cpp",)

SIGNAL, TINT, RESTORE, BEGIN, END, BUSY = range(6)

TINTED, CONTROL, BUSY_FLAGS = 0x800F185C, 0x800F196C, 0x800F1868
TICK = 0x8005E24C
GLOBALS_BASE, GLOBALS_SIZE = 0x800F1800, 0x200
ADVANCE, TRANSFORM, BEGIN_EFFECT = 0x8009D854, 0x8007B29C, 0x800CF0E8
HUD, MINIMAP, EXPLORE = 0x800CB654, 0x8008B4C8, 0x8006F5CC
ADVANCE_CALL, TRANSFORM_CALL, BEGIN_CALL, HUD_CALL, MINIMAP_CALL, EXPLORE_CALL = range(
    1, 7
)
STACK_FILL = 0x400
REQUEST_POSITION = 0xE0


def word(rng: random.Random) -> int:
    return s32(
        rng.choice(
            (
                0,
                1,
                2,
                3,
                4,
                5,
                8,
                0x10,
                0xFF,
                0x100,
                0x7FFF,
                0x8000,
                0xFFFF,
                0x10000,
                -1,
                rng.getrandbits(32),
                rng.getrandbits(32),
            )
        )
    )


def vector(rng: random.Random, number: int, mode: int) -> dict:
    return {
        "mode": mode,
        "tinted": word(rng),
        "control": word(rng),
        "tick": rng.choice((2, 4, word(rng))),
        "argument": word(rng),
        "unused": word(rng),
        "busy": rng.choice(
            (
                0,
                1,
                2,
                3,
                4,
                7,
                0xFFFFFFFC,
                0xFFFFFFFF,
                rng.getrandbits(32),
                rng.getrandbits(32),
            )
        ),
        "stack": rng.randbytes(STACK_FILL),
        "number": number,
    }


def standalone_request(machine: Machine, address: int) -> list[int]:
    """The fields of the standalone request at `address` that func_800733F8 defines."""

    def u8(offset: int) -> int:
        return machine.load(address + offset, 1)

    def u16(offset: int) -> int:
        return machine.load(address + offset, 2)

    def i16(offset: int) -> int:
        return machine.load(address + offset, 2, signed=True)

    return [
        u16(0),
        u8(2),
        u8(3),
        u16(4),
        u8(8),
        u8(9),
        u8(0xA),
        u16(0x10),
        u8(0x14),
        u8(0x15),
        u8(0x16),
        i16(REQUEST_POSITION),
        i16(REQUEST_POSITION + 2),
        i16(REQUEST_POSITION + 4),
    ]


def current_globals(machine: Machine) -> list[int]:
    return [
        s32(machine.load(TINTED)),
        s32(machine.load(CONTROL)),
        s32(machine.load(TICK)),
    ]


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    mode = value["mode"]
    calls: list[list[int]] = []
    machine.store(DEFAULT_STACK - STACK_FILL, value["stack"])
    machine.map_zero(GLOBALS_BASE, GLOBALS_SIZE)
    machine.map_zero(TICK, 4)
    machine.put(TINTED, value["tinted"])
    machine.put(CONTROL, value["control"])
    machine.put(TICK, value["tick"])
    machine.put(BUSY_FLAGS, value["busy"])

    def record(kind: int, *arguments: int) -> None:
        calls.append([kind, *arguments, *current_globals(machine)])

    def transform(m: Machine) -> None:
        sp = m.r[29]
        record(
            TRANSFORM_CALL,
            s32(m.r[4]),
            s32(m.r[5]),
            s32(m.r[6]),
            s32(m.r[7]),
            s32(m.load(sp + 0x10)),
            s32(m.load(sp + 0x14)),
        )

    machine.stub(ADVANCE, lambda m: record(ADVANCE_CALL, s32(m.r[4])))
    machine.stub(TRANSFORM, transform)
    machine.stub(
        BEGIN_EFFECT,
        lambda m: record(
            BEGIN_CALL, s32(m.r[5]), s32(m.r[6]), *standalone_request(m, m.r[4])
        ),
    )
    machine.stub(HUD, lambda m: record(HUD_CALL, s32(m.r[4])))
    machine.stub(MINIMAP, lambda m: record(MINIMAP_CALL, s32(m.r[4])))
    machine.stub(EXPLORE, lambda m: record(EXPLORE_CALL))
    machine.r[4] = value["argument"] & 0xFFFFFFFF
    machine.r[5] = value["unused"] & 0xFFFFFFFF
    inputs = [
        mode,
        value["tinted"],
        value["control"],
        value["tick"],
        value["argument"],
        s32(value["busy"]),
    ]
    machine.execute(entry)
    values = current_globals(machine)
    names = ["tinted", "control", "tick"]
    if mode == BUSY:
        values.append(s32(machine.r[2]))
        names.append("return")
    return recorded_observation(machine, values, names, calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    return [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (TINTED, TINTED + 4),
        (CONTROL, CONTROL + 4),
        (TICK, TICK + 4),
    ]


def describe(image, results) -> list[str]:
    if not results:
        return []
    calls = sum(len(observation["calls"]) for _, observation, _ in results)
    return [
        f"{calls} recorded callee calls over {len(results)} vectors",
        "Compared each call's arguments and the globals it saw, the final globals, the result and stores "
        "outside the stack and the three globals.",
    ]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
