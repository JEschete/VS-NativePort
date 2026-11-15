"""Shared harness for the event screen tween differentials (BATTLE.PRG 4A0A8.c, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Event core
(EventScreenTweens.cpp) in event_screen_tweens_driver.cpp. The event camera (D_800F4BA4) points at
a record whose two roll tweens (+0x168, +0x174) and four screen tweens (+0x1FC, +0x21C, +0x23C,
+0x25C) start with random fields; the tick speed (vs_gametime_tickspeed) and the scratchpad word
0x1F800088 start random. The easing curve (func_800BFEBC) is stubbed with a salted function of its
three arguments, which both runs record; every framebuffer effect setter and reader and the radial
warp setter (SCREFF2.PRG func_800F9BC0) are stubbed and recorded. getShort (func_800BFE00) and,
for func_800BDAB4, func_800BDBB4 run as the original.

Each run prints its result, the fields of all six tweens, the scratchpad word and the calls.
Stores are allowed only to the stack, the scratchpad word and the tweens' non-reserved fields.

Outside the compared domain (covered by EventScreenTweensTests.cpp): func_800BD444 wrote through
an unset pointer for an opcode other than 0xE2 and 0xE3, and func_800BD6C4 returned a stale
register for an unhandled opcode; the typed cores refuse both. The live entries skip a missing
camera instead of reading through it.
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
DRIVER = "event_screen_tweens_driver.cpp"
SOURCES = (r"Native\Game\src\Event\EventScreenTweens.cpp",)

POINTER = 0x800F4BA4
TICK_SPEED = 0x8005E24C
CAMERA = 0x80400000
CAMERA_BYTES = 0x28C
SCRATCH = 0x1F800000
EASED = SCRATCH + 0x88
ROLLS = (0x168, 0x174)
TWEENS = (0x1FC, 0x21C, 0x23C, 0x25C)

GET_SHORT, CURVE, ADVANCE_TWEEN = 0x800BFE00, 0x800BFEBC, 0x800BDBB4
(
    SET_ENABLED,
    SET_DEPTH,
    SET_PREVIOUS,
    SET_SCALE,
    SET_TINT,
    SET_OFFSET,
    SET_BLEND,
    READ_SCALE,
    READ_TINT,
    READ_OFFSET,
    SET_WARP,
) = (
    0x8007DD50,
    0x8007DE44,
    0x8007DE5C,
    0x8007DDB8,
    0x8007DDD4,
    0x8007DDF8,
    0x8007DE2C,
    0x8007DE88,
    0x8007DEA8,
    0x8007DECC,
    0x800F9BC0,
)
(
    CALL_ENABLED,
    CALL_DEPTH,
    CALL_PREVIOUS,
    CALL_SCALE,
    CALL_TINT,
    CALL_OFFSET,
    CALL_BLEND,
    CALL_READ_SCALE,
    CALL_READ_TINT,
    CALL_READ_OFFSET,
    CALL_WARP,
    CALL_EASE,
) = range(1, 13)

CAPTURE, ROLL, ANGLE, TRANSFORM, TWEEN_SET, TWEEN = range(6)
CURVE_MODULUS = 0x2001
CURVE_BIAS = 0x800

# (offset, size, signed) of each compared field.
ANGLE_FIELDS = (
    (0, 1, False),
    (1, 1, False),
    (2, 2, True),
    (4, 2, True),
    (6, 2, True),
    (8, 2, True),
    (0xA, 2, True),
)
ANGLE_NAMES = ("changed", "easing", "angle", "start", "delta", "elapsed", "duration")
TWEEN_FIELDS = (
    (1, 1, False),
    *(
        (offset, 2, True)
        for offset in (2, 4, 6, 0xA, 0xC, 0xE, 0x12, 0x14, 0x16, 0x1C, 0x1E)
    ),
)
TWEEN_NAMES = (
    "easing",
    *(
        f"{kind} {index}"
        for kind in ("current", "start", "delta")
        for index in range(3)
    ),
    "elapsed",
    "duration",
)
NAMES = (
    "result",
    *(
        f"{roll} {name}"
        for roll in ("camera roll", "screen roll")
        for name in ANGLE_NAMES
    ),
    *(
        f"{tween} {name}"
        for tween in ("scale", "tint", "offset", "warp")
        for name in TWEEN_NAMES
    ),
    "scratch",
)
TRANSFORM_OPCODES = (0xB9, 0xBA, 0xBF, 0xE4, 0xE5, 0xE6, 0xE7, 0xED)


def half(rng: random.Random) -> int:
    return rng.choice(
        (
            0,
            1,
            -1,
            0x7FFF,
            -0x8000,
            0x800,
            -0x800,
            0x801,
            -0x801,
            rng.randrange(-0x8000, 0x8000),
        )
    )


def duration(rng: random.Random) -> int:
    return rng.choice((0, 0, 1, 2, 8, -1, rng.randrange(1, 0x100), half(rng)))


def tick_speed(rng: random.Random) -> int:
    return rng.choice((0, 1, 2, 3, 4, 7, -1, -2, 0x7FFFFFFF, s32(rng.getrandbits(32))))


def angle_tween(rng: random.Random, tick: int) -> list[int]:
    fields = [
        rng.randrange(256),
        rng.randrange(256),
        half(rng),
        half(rng),
        half(rng),
        half(rng),
        duration(rng),
    ]
    if rng.randrange(3) == 0:
        fields[5] = ((fields[6] - (tick >> 1) + 0x8000) & 0xFFFF) - 0x8000
    return fields


def screen_tween(rng: random.Random, tick: int) -> list[int]:
    fields = [
        rng.randrange(256),
        *(half(rng) for _ in range(9)),
        half(rng),
        duration(rng),
    ]
    if rng.randrange(3) == 0:
        fields[10] = ((fields[11] - (tick >> 1) + 0x8000) & 0xFFFF) - 0x8000
    return fields


def instruction(rng: random.Random, mode: int) -> list[int]:
    data = [
        rng.choice((0, 0, 1, 0x7F, 0x80, 0xFF, rng.randrange(256))) for _ in range(6)
    ]
    if mode == CAPTURE:
        data[0] = 0xE1
    elif mode == ROLL:
        data[0] = rng.choice((0xE2, 0xE3))
        data[3] = rng.choice((0, 1, 2, 3, 0xF)) << 4 | rng.randrange(16)
    else:
        data[0] = rng.choice(TRANSFORM_OPCODES)
    return data


def vector(rng: random.Random, number: int, mode: int) -> dict:
    tick = tick_speed(rng)

    def word() -> int:
        return rng.choice((0, 0x1000, -1, 0x7FFF, 0x12345, s32(rng.getrandbits(32))))

    return {
        "mode": mode,
        "which": rng.randrange(2 if mode == ANGLE else 4),
        "instruction": instruction(rng, mode),
        "tick": tick,
        "reads": [
            word(),
            word(),
            *(rng.randrange(256) for _ in range(3)),
            word(),
            word(),
        ],
        "scratch": s32(rng.getrandbits(32)),
        "salt": rng.randrange(CURVE_MODULUS),
        "rolls": [angle_tween(rng, tick) for _ in ROLLS],
        "tweens": [screen_tween(rng, tick) for _ in TWEENS],
    }


def curve(salt: int, mode: int, frame: int, length: int) -> int:
    return (
        mode * 0x35 + frame * 0x101 + length * 7 + salt
    ) % CURVE_MODULUS - CURVE_BIAS


def stubs(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    scale_x, scale_y, red, green, blue, offset_x, offset_y = value["reads"]

    def record(kind: int):
        return lambda m: calls.append([kind, s32(m.r[4])])

    def pair(kind: int):
        return lambda m: calls.append(
            [kind, s32(m.load(m.r[4])), s32(m.load(m.r[4] + 4))]
        )

    def read_pair(kind: int, x: int, y: int):
        def handler(m: Machine) -> None:
            calls.append([kind])
            m.write(m.r[4], x & 0xFFFFFFFF, 4)
            m.write(m.r[4] + 4, y & 0xFFFFFFFF, 4)

        return handler

    def read_tint(m: Machine) -> None:
        calls.append([CALL_READ_TINT])
        for index, channel in enumerate((red, green, blue)):
            m.write(m.r[4] + index, channel, 1)

    def ease(m: Machine) -> None:
        arguments = [s32(m.r[register]) for register in (4, 5, 6)]
        calls.append([CALL_EASE, *arguments])
        m.r[2] = curve(value["salt"], *arguments) & 0xFFFFFFFF

    machine.stub(SET_ENABLED, record(CALL_ENABLED))
    machine.stub(SET_DEPTH, record(CALL_DEPTH))
    machine.stub(SET_PREVIOUS, record(CALL_PREVIOUS))
    machine.stub(SET_BLEND, record(CALL_BLEND))
    machine.stub(SET_SCALE, pair(CALL_SCALE))
    machine.stub(SET_OFFSET, pair(CALL_OFFSET))
    machine.stub(
        SET_TINT,
        lambda m: calls.append([CALL_TINT, *(m.load(m.r[4] + i, 1) for i in range(3))]),
    )
    machine.stub(READ_SCALE, read_pair(CALL_READ_SCALE, scale_x, scale_y))
    machine.stub(READ_OFFSET, read_pair(CALL_READ_OFFSET, offset_x, offset_y))
    machine.stub(READ_TINT, read_tint)
    machine.stub(
        SET_WARP,
        lambda m: calls.append([CALL_WARP, m.r[4] & 0xFFFF, s32(m.r[5] << 16) >> 16]),
    )
    machine.stub(CURVE, ease)
    machine.follow.update((GET_SHORT, ADVANCE_TWEEN))


def put_fields(machine: Machine, base: int, layout, fields: list[int]) -> None:
    for (offset, size, _), field in zip(layout, fields, strict=True):
        machine.put(base + offset, field, size)


def fields(machine: Machine, base: int, layout) -> list[int]:
    return [
        machine.load(base + offset, size, signed=signed)
        for offset, size, signed in layout
    ]


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    calls: list[list[int]] = []
    stubs(machine, value, calls)
    machine.map_zero(CAMERA, CAMERA_BYTES)
    for index in range(CAMERA_BYTES // 4):
        machine.put(CAMERA + index * 4, 0xA5A5A5A5 ^ index)
    for offset, roll in zip(ROLLS, value["rolls"], strict=True):
        put_fields(machine, CAMERA + offset, ANGLE_FIELDS, roll)
    for offset, tween in zip(TWEENS, value["tweens"], strict=True):
        put_fields(machine, CAMERA + offset, TWEEN_FIELDS, tween)
    machine.map_zero(POINTER, 4)
    machine.put(POINTER, CAMERA)
    machine.map_zero(TICK_SPEED, 4)
    machine.put(TICK_SPEED, value["tick"] & 0xFFFFFFFF)
    machine.map_zero(SCRATCH, 0x100)
    machine.put(EASED, value["scratch"] & 0xFFFFFFFF)
    script = CAMERA + 0x1000
    machine.map_zero(script, 8)
    machine.store(script, bytes(value["instruction"]))

    mode = value["mode"]
    if mode == ANGLE:
        machine.r[4] = CAMERA + ROLLS[value["which"]]
    elif mode == TWEEN:
        machine.r[4] = CAMERA + TWEENS[value["which"]]
    else:
        machine.r[4] = script
    machine.r[5] = 0
    machine.execute(entry)

    result = 0 if mode in (ANGLE, TWEEN_SET) else s32(machine.r[2])
    values = [
        result,
        *(
            item
            for offset in ROLLS
            for item in fields(machine, CAMERA + offset, ANGLE_FIELDS)
        ),
        *(
            item
            for offset in TWEENS
            for item in fields(machine, CAMERA + offset, TWEEN_FIELDS)
        ),
        machine.load(EASED, 4, signed=True),
    ]
    inputs = [
        value["which"],
        *value["instruction"],
        value["tick"],
        *value["reads"],
        value["scratch"],
        value["salt"],
        *(item for roll in value["rolls"] for item in roll),
        *(item for tween in value["tweens"] for item in tween),
    ]
    return recorded_observation(machine, values, list(NAMES), calls), [mode, *inputs]


def check(
    image, value: dict, observation: dict, native: list[int], reference
) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    eased = sum(
        1
        for _, observation, _ in results
        if any(call[0] == CALL_EASE for call in observation["calls"])
    )
    busy = sum(1 for _, observation, _ in results if observation["values"][0] == 1)
    return [f"{eased} vectors eased a tween, {busy} vectors returned 1"]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK), (EASED, EASED + 4)]
    for base, layout in (
        *((offset, ANGLE_FIELDS) for offset in ROLLS),
        *((offset, TWEEN_FIELDS) for offset in TWEENS),
    ):
        allowed += [
            (CAMERA + base + offset, CAMERA + base + offset + size)
            for offset, size, _ in layout
        ]
    return allowed


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
