"""Shared harness for the primary effect state and effect token differentials (BATTLE.PRG
5BF94.c:5373-5397 and 5546-5591, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects core in
primary_effect_driver.cpp. The originals read and write BATTLE.PRG globals: the primary phase
(D_800F531C), the track count and started count (D_800F5224, D_800F5228), the pending-track mask
(D_800F521C), the loaded byte (D_800F54A8) and the four token words (D_800F5320). Every vector
fills all of them, and the bytes beside the loaded flag, so an original reading the wrong one
differs.

Native differences outside the compared domain (covered by PrimaryEffectTests.cpp): a requeued
track outside 0-31, a phase number the runtime never sets and a released token outside 0-127 are
refused instead of shifting by the low five bits, storing the number or clearing a bit of the word
the token addressed. Vectors therefore requeue tracks 0-31, set the runtime's phases and release
tokens 0-127; the read entries see any stored phase.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Machine,
    field_problems,
    mode_case,
    s32,
    store_problems,
)

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "primary_effect_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\PrimaryEffect.cpp",)

PENDING = 0x800F521C
TRACK_COUNT = 0x800F5224
STARTED = 0x800F5228
PHASE = 0x800F531C
TOKENS = 0x800F5320
TOKEN_WORDS = 4
LOADED = 0x800F54A8

CAN_START, PLAYABLE, REQUEUE, IS_LOADED, SET_PHASE, READ_PHASE, ALLOCATE, RELEASE = (
    range(8)
)
RUNTIME_PHASES = (0, 2, 3, 4, 5, 6, 8, 9)
FULL_WORD = 0xFFFFFFFE


def word(rng: random.Random) -> int:
    return rng.choice(
        (
            0,
            0,
            1,
            2,
            0x80000000,
            0xFFFFFFFF,
            1 << rng.randrange(32),
            rng.randrange(1 << 32),
        )
    )


def phase(rng: random.Random) -> int:
    """Mostly the track phases, and some numbers outside the runtime's."""
    return rng.choice(
        (
            4,
            4,
            5,
            5,
            *RUNTIME_PHASES,
            1,
            7,
            10,
            0x10004,
            0xFFFFFFFF,
            rng.randrange(1 << 32),
        )
    )


def count(rng: random.Random) -> int:
    return rng.choice(
        (
            0,
            1,
            2,
            3,
            5,
            31,
            32,
            -1,
            -2,
            0x7FFFFFFF,
            -0x80000000,
            rng.randrange(-8, 40),
            rng.randrange(-0x80000000, 0x80000000),
        )
    )


def token_word(rng: random.Random) -> int:
    """Token words: empty, full, all set, one bit free and random."""
    free = rng.randrange(32)
    return rng.choice(
        (
            0,
            FULL_WORD,
            FULL_WORD,
            0xFFFFFFFF,
            0xFFFFFFFF & ~(1 << free),
            0x1,
            0x7,
            (1 << free) - 1,
            rng.randrange(1 << 32),
        )
    )


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {
        "mode": mode,
        "number": number,
        "phase": phase(rng),
        "count": count(rng),
        "started": count(rng),
        "pending": word(rng),
        "loaded": rng.choice((0, 0, 1, 1, 2, 0x80, 0xFF, rng.randrange(256))),
        "beside": [rng.randrange(256) for _ in range(2)],
        "tokens": [token_word(rng) for _ in range(TOKEN_WORDS)],
    }
    if mode in (CAN_START, PLAYABLE, REQUEUE) and rng.randrange(3):
        # Bias towards the comparison's edge.
        value["started"] = s32(value["count"] + rng.choice((-1, -1, 0, 1)))
    if mode == REQUEUE:
        value["track"] = rng.choice((0, 31, rng.randrange(32), rng.randrange(32)))
    elif mode == SET_PHASE:
        value["value"] = rng.choice(RUNTIME_PHASES)
    elif mode == RELEASE:
        value["token"] = rng.choice(
            (0, 1, 31, 32, 33, 64, 127, rng.randrange(128), rng.randrange(128))
        )
    return value


def prepare(machine: Machine, value: dict) -> None:
    machine.put(PHASE, value["phase"])
    machine.put(TRACK_COUNT, value["count"])
    machine.put(STARTED, value["started"])
    machine.put(PENDING, value["pending"])
    machine.put(LOADED - 1, value["beside"][0], 1)
    machine.put(LOADED, value["loaded"], 1)
    machine.put(LOADED + 1, value["beside"][1], 1)
    for index, token in enumerate(value["tokens"]):
        machine.put(TOKENS + index * 4, token)


def tokens(machine: Machine) -> list[int]:
    return [machine.load(TOKENS + index * 4) for index in range(TOKEN_WORDS)]


def allowed_stores(mode: int) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if mode == REQUEUE:
        allowed += [(STARTED, STARTED + 4), (PENDING, PENDING + 4)]
    elif mode == SET_PHASE:
        allowed.append((PHASE, PHASE + 4))
    elif mode in (ALLOCATE, RELEASE):
        allowed.append((TOKENS, TOKENS + TOKEN_WORDS * 4))
    return allowed


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    prepare(machine, value)
    state = [value["phase"], value["count"], value["started"], value["pending"]]
    inputs: list[int] = [mode]
    if mode == REQUEUE:
        machine.r[4] = value["track"]
    elif mode == SET_PHASE:
        machine.r[4] = value["value"]
    elif mode == RELEASE:
        machine.r[4] = value["token"]
    machine.execute(entry)

    result = machine.r[2]
    if mode in (CAN_START, PLAYABLE):
        inputs += state
        values = [s32(result)]
    elif mode == REQUEUE:
        inputs += [*state, value["track"]]
        values = [0, s32(machine.load(STARTED)), machine.load(PENDING)]
    elif mode == IS_LOADED:
        inputs.append(value["loaded"])
        values = [result]
    elif mode == SET_PHASE:
        inputs += [value["phase"], value["value"]]
        values = [0, machine.load(PHASE)]
    elif mode == READ_PHASE:
        inputs.append(value["phase"])
        values = [result]
    elif mode == ALLOCATE:
        inputs += value["tokens"]
        values = [result, *tokens(machine)]
    else:
        inputs += [*value["tokens"], value["token"]]
        values = [0, *tokens(machine)]
    return {"values": values, "stores": list(machine.stores)}, inputs


NAMES = {
    CAN_START: ("can start",),
    PLAYABLE: ("track count",),
    REQUEUE: ("refused", "started", "pending"),
    IS_LOADED: ("loaded",),
    SET_PHASE: ("refused", "phase"),
    READ_PHASE: ("phase",),
    ALLOCATE: ("token", *(f"word {index}" for index in range(TOKEN_WORDS))),
    RELEASE: ("refused", *(f"word {index}" for index in range(TOKEN_WORDS))),
}


def check(
    image, value: dict, observation: dict, native: list[int], reference
) -> list[str]:
    ours = observation["values"]
    if len(native) != len(ours):
        return [
            f"native printed {len(native)} values, original {len(ours)}: {native} {ours}"
        ]
    return field_problems(NAMES[value["mode"]], native, ours) + store_problems(
        observation["stores"], allowed_stores(value["mode"])
    )


def describe(image, results) -> list[str]:
    if not results:
        return []
    mode = results[0][0]["mode"]
    if mode in (CAN_START, IS_LOADED):
        started = sum(1 for _, observation, _ in results if observation["values"][0])
        return [f"{started} of {len(results)} vectors returned 1"]
    if mode == PLAYABLE:
        playing = sum(1 for value, _, _ in results if value["phase"] in (4, 5))
        return [f"{playing} of {len(results)} vectors in a track phase"]
    if mode == ALLOCATE:
        full = sum(1 for _, observation, _ in results if not observation["values"][0])
        return [f"{full} of {len(results)} vectors found the pool full"]
    if mode == RELEASE:
        cleared = sum(
            1
            for value, observation, _ in results
            if observation["values"][1:] != value["tokens"]
        )
        return [f"{cleared} of {len(results)} vectors cleared a set bit"]
    return []


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
