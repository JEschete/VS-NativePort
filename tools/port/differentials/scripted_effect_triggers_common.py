"""Shared harness for the scripted actor effect trigger lifecycle differentials (BATTLE.PRG 146C.c:8581-8617, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Effects core
(ScriptedEffectTriggers.cpp) in scripted_effect_triggers_driver.cpp. The actor table (vs_battle_actors) gets a random
null mask over its first 16 slots. The stubs record each callee in call order with its arguments: the primary effect
begin (func_800CF0E8, with every request field func_8007BE20 defines and the others it lists), the effect track start
(func_800CEF74), the primary effect phase (func_800CF48C) and the track readiness test (func_800CEEBC); the phase
and readiness stubs return the vector's words. func_8007BEE4's and func_8007BF0C's results are compared too. The
caller's stack is filled with random bytes first, so a request field the original fails to write differs from the
native one, while fields it never writes (the colour presets, the target's slot byte, padding and the request
position) are not compared.

Native differences outside the compared domain (covered by ScriptedEffectTriggersTests.cpp): request fields the
original left as stack bytes are zero, and func_8007BEC4 returns nothing rather than leaving the track start's
result in v0 (its callers ignore it).
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
DRIVER = "scripted_effect_triggers_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\ScriptedEffectTriggers.cpp",)

START, RELEASE, PHASE, READY = range(4)

ACTORS, ACTOR_SLOTS = 0x800F1928, 16
ACTORS_BASE, ACTORS_SIZE = 0x800F1900, 0x100
BEGIN_EFFECT, START_TRACK, EFFECT_PHASE, CAN_START = (
    0x800CF0E8,
    0x800CEF74,
    0x800CF48C,
    0x800CEEBC,
)
BEGIN_CALL, TRACK_CALL, PHASE_CALL, CAN_START_CALL = range(1, 5)
STACK_FILL = 0x400
REQUEST_OTHERS = 0xD0
REQUEST_OTHERS_CAPACITY = 16


def word(rng: random.Random) -> int:
    return s32(
        rng.choice(
            (
                0,
                1,
                2,
                3,
                0xF,
                0x10,
                0xFF,
                0x100,
                0x7FFF,
                0x8000,
                0xFFFF,
                0x10000,
                -1,
                rng.getrandbits(32),
            )
        )
    )


def target(rng: random.Random) -> int:
    return s32(
        rng.choice(
            (
                rng.randrange(ACTOR_SLOTS),
                rng.randrange(ACTOR_SLOTS),
                rng.randrange(ACTOR_SLOTS),
                -1,
                ACTOR_SLOTS,
                0x100 + rng.randrange(ACTOR_SLOTS),
                rng.getrandbits(32),
            )
        )
    )


def vector(rng: random.Random, number: int, mode: int) -> dict:
    return {
        "mode": mode,
        "effect": word(rng),
        "target": target(rng),
        "loaded": rng.choice(
            (
                0,
                0xFFFF,
                1 << rng.randrange(ACTOR_SLOTS),
                rng.getrandbits(16),
                rng.getrandbits(16),
            )
        ),
        "phase": word(rng),
        "can_start": word(rng),
        "stack": rng.randbytes(STACK_FILL),
        "pointers": [rng.getrandbits(32) | 1 for _ in range(ACTOR_SLOTS)],
        "number": number,
    }


def defined_request(machine: Machine, address: int) -> list[int]:
    """The fields of the request at `address` that func_8007BE20 defines, then the others it lists."""

    def u8(offset: int) -> int:
        return machine.load(address + offset, 1)

    def u16(offset: int) -> int:
        return machine.load(address + offset, 2)

    def i16(offset: int) -> int:
        return machine.load(address + offset, 2, signed=True)

    others = min(u8(3), REQUEST_OTHERS_CAPACITY)
    return [
        u16(0),
        u8(2),
        u8(3),
        u16(4),
        i16(8),
        i16(0xA),
        i16(0xC),
        u16(0x10),
        u8(0x14),
        u8(0x16),
        *(u8(REQUEST_OTHERS + i) for i in range(others)),
    ]


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    mode = value["mode"]
    calls: list[list[int]] = []
    machine.store(DEFAULT_STACK - STACK_FILL, value["stack"])
    machine.map_zero(ACTORS_BASE, ACTORS_SIZE)
    for slot in range(ACTOR_SLOTS):
        if value["loaded"] >> slot & 1:
            machine.put(ACTORS + 4 * slot, value["pointers"][slot])

    def answer(kind: int, result: int):
        def stub(m: Machine) -> None:
            calls.append([kind])
            m.r[2] = result & 0xFFFFFFFF

        return stub

    machine.stub(
        BEGIN_EFFECT,
        lambda m: calls.append(
            [BEGIN_CALL, s32(m.r[5]), s32(m.r[6]), *defined_request(m, m.r[4])]
        ),
    )
    machine.stub(START_TRACK, lambda m: calls.append([TRACK_CALL, s32(m.r[4])]))
    machine.stub(EFFECT_PHASE, answer(PHASE_CALL, value["phase"]))
    machine.stub(CAN_START, answer(CAN_START_CALL, value["can_start"]))
    # a0 is the effect id (func_8007BE20) or the track (func_8007BEC4); a1 is the target actor.
    machine.r[4] = value["effect"] & 0xFFFFFFFF
    machine.r[5] = value["target"] & 0xFFFFFFFF
    inputs = [
        mode,
        value["effect"],
        value["target"],
        value["loaded"],
        value["phase"],
        value["can_start"],
    ]
    machine.execute(entry)
    values: list[int] = []
    names: list[str] = []
    if mode in (PHASE, READY):
        values.append(s32(machine.r[2]))
        names.append("return")
    return recorded_observation(machine, values, names, calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    return [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]


def describe(image, results) -> list[str]:
    if not results:
        return []
    calls = sum(len(observation["calls"]) for _, observation, _ in results)
    return [
        f"{calls} recorded callee calls over {len(results)} vectors",
        "Compared each call's arguments (the defined request fields and listed others), the results and stores "
        "outside the stack.",
    ]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
