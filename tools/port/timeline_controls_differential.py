"""Shared harness for the effect timeline transform-control and action-control dispatch
differentials (BATTLE.PRG 5BF94.c:8240-8420, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects selection in
timeline_controls_driver.cpp. Every vector fills the whole effect instance with random bytes, so
the sixteen transforms (0xD0 bytes each from +0x1C: flags at +6, actor and bone at +0x34/+0x35)
all differ and an original reading or writing the wrong transform, field or stride differs. The
instance's timeline pointer (+0xD4C) leads to a timeline whose flags (+1) and frame (+2) are
random too.

The weapon trail, resource context, room object and track stubs record their calls. The
dispatcher's sixteen command routines are stubbed with recorders of the instance and the operands
the dispatcher passes each one.

Native differences outside the compared domain (covered by TimelineCommandsTests.cpp): transform
indices from 16 on, a missing instance, timeline or track, and a key index outside the track are
refused instead of read or written.
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
DRIVER = "timeline_controls_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\TimelineCommands.cpp",)

INSTANCE = 0x80400000
INSTANCE_BYTES = 0xD60
TRANSFORMS = 0x1C
TRANSFORM_BYTES = 0xD0
TRANSFORM_COUNT = 16
FLAGS, ACTOR, BONE = 6, 0x34, 0x35
TIMELINE_POINTER = 0xD4C
TIMELINE = 0x80410000
TIMELINE_BYTES = 8
TIMELINE_FLAGS, TIMELINE_FRAME = 1, 2
TRACK = 0x80420000
TRACK_BYTES = 8
KEY_INDEX = 6
KEYS = 0x80430000
KEY_BYTES = 4
MAX_KEYS = 8

SAVE_ADDRESS = 0x800D78B8
SPAWN_ADDRESS = 0x800CF7A8
RESTORE_ADDRESS = 0x800D78CC
FADE_ADDRESS = 0x800CF830
PAUSE_ADDRESS = 0x8006CE50
ADVANCE_ADDRESS = 0x800D5780
COMMAND_ADDRESSES = (
    0x800D6388,
    0x800D63D0,
    0x800D6418,
    0x800D6448,
    0x800D64E4,
    0x800D6508,
    0x800D6538,
    0x800D6554,
    0x800D6574,
    0x800D65A8,
    0x800D65D8,
    0x800D6628,
    0x800D66CC,
    0x800D66FC,
    0x800D67C4,
    0x800D6860,
)
# The operand registers ($a1, $a2) the dispatcher sets for each command.
COMMAND_OPERANDS = (0, 0, 0, 2, 0, 1, 0, 0, 0, 1, 1, 1, 0, 2, 1, 0)
FRAME_BITS = 0x1FF
COMMAND_SHIFT = 9

TRAIL, FADE, RESUME, HOLD, PAUSE, INVALIDATE, DISPATCH = range(7)

# The kinds the driver prints for each call; a command records CALL_COMMAND + its number.
CALL_SAVE, CALL_SPAWN, CALL_RESTORE, CALL_FADE, CALL_PAUSE = 1, 2, 3, 4, 5
CALL_COMMAND = 16
CALL_ADVANCE = 32


def transform(index: int) -> int:
    return INSTANCE + TRANSFORMS + index * TRANSFORM_BYTES


def timing(rng: random.Random) -> int:
    """Mostly the sixteen commands, sometimes the numbers past them, at any frame."""
    command = rng.randrange(16) if rng.randrange(8) else rng.randrange(16, 128)
    return command << COMMAND_SHIFT | rng.randrange(FRAME_BITS + 1)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {
        "mode": mode,
        "number": number,
        "instance": rng.randbytes(INSTANCE_BYTES),
        "timeline": rng.randbytes(TIMELINE_BYTES),
        "index": rng.randrange(TRANSFORM_COUNT),
        "style": rng.choice((0, 9, 10, 47, 99, 100, 0xFF, rng.randrange(256))),
        "high": [rng.choice((0, rng.getrandbits(24))) for _ in range(2)],
    }
    if mode == DISPATCH:
        count = rng.randint(1, MAX_KEYS)
        keys = bytearray(rng.randbytes(count * KEY_BYTES))
        key = rng.randrange(count)
        keys[key * KEY_BYTES : key * KEY_BYTES + 2] = timing(rng).to_bytes(2, "little")
        due = (
            int.from_bytes(keys[key * KEY_BYTES : key * KEY_BYTES + 2], "little")
            & FRAME_BITS
        )
        # Mostly due; otherwise a frame beside it, one sharing its low bits, or any frame.
        frame = rng.choice(
            (
                due,
                due,
                due,
                due,
                due,
                due,
                due + 1,
                due - 1,
                due + 0x100,
                due + 0x200,
                due - 0x200,
                rng.randrange(-0x8000, 0x8000),
            )
        )
        value.update(
            keys=bytes(keys),
            key=key,
            track=rng.randbytes(TRACK_BYTES),
            frame=frame,
            more=rng.randrange(2),
        )
    return value


def prepare(machine: Machine, value: dict) -> None:
    machine.store(INSTANCE, value["instance"])
    machine.put(INSTANCE + TIMELINE_POINTER, TIMELINE)
    machine.store(TIMELINE, value["timeline"])
    if value["mode"] == DISPATCH:
        machine.put(TIMELINE + TIMELINE_FRAME, value["frame"], 2)
        machine.store(TRACK, value["track"])
        machine.put(TRACK, KEYS)
        machine.put(TRACK + KEY_INDEX, value["key"], 2)
        machine.store(KEYS, value["keys"])


def recorder(calls: list[list[int]], value: dict) -> dict:
    def kind(call: int):
        def stub(machine: Machine) -> None:
            calls.append([call])

        return stub

    def spawn(machine: Machine) -> None:
        calls.append([CALL_SPAWN, *(s32(machine.r[4 + index]) for index in range(4))])

    def fade(machine: Machine) -> None:
        calls.append([CALL_FADE, s32(machine.r[4]), s32(machine.r[5])])

    stubs = {
        SAVE_ADDRESS: kind(CALL_SAVE),
        SPAWN_ADDRESS: spawn,
        RESTORE_ADDRESS: kind(CALL_RESTORE),
        FADE_ADDRESS: fade,
        PAUSE_ADDRESS: kind(CALL_PAUSE),
    }
    if value["mode"] != DISPATCH:
        return stubs

    def command(number: int):
        def stub(machine: Machine) -> None:
            operands = (
                s32(machine.r[5 + index]) for index in range(COMMAND_OPERANDS[number])
            )
            calls.append(
                [CALL_COMMAND + number, int(machine.r[4] == INSTANCE), *operands]
            )

        return stub

    def advance(machine: Machine) -> None:
        calls.append([CALL_ADVANCE, int(machine.r[4] == TRACK)])
        machine.r[2] = value["more"]

    stubs.update(
        {address: command(number) for number, address in enumerate(COMMAND_ADDRESSES)}
    )
    stubs[ADVANCE_ADDRESS] = advance
    return stubs


def at(value: dict, offset: int, size: int) -> int:
    return int.from_bytes(value["instance"][offset : offset + size], "little")


def state(machine: Machine, mode: int) -> tuple[list[int], list[str]]:
    """The transform flags or timeline flags the original may have changed, and the result."""
    if mode in (RESUME, INVALIDATE):
        flags = [
            machine.load(transform(index) + FLAGS, 2)
            for index in range(TRANSFORM_COUNT)
        ]
        return flags, [f"transform {index} flags" for index in range(TRANSFORM_COUNT)]
    if mode == HOLD:
        return [machine.load(TIMELINE + TIMELINE_FLAGS, 1)], ["timeline flags"]
    if mode == DISPATCH:
        return [s32(machine.r[2])], ["result"]
    return [], []


def inputs_of(value: dict) -> list[int]:
    mode = value["mode"]
    inputs = [mode]
    slots = range(TRANSFORM_COUNT)
    base = TRANSFORMS
    if mode == TRAIL:
        inputs += [value["index"], value["style"]]
        for index in slots:
            offset = base + index * TRANSFORM_BYTES
            inputs += [at(value, offset + ACTOR, 1), at(value, offset + BONE, 1)]
    elif mode in (RESUME, INVALIDATE):
        inputs.append(value["index"])
        inputs += [
            at(value, base + index * TRANSFORM_BYTES + FLAGS, 2) for index in slots
        ]
    elif mode == HOLD:
        inputs.append(value["timeline"][TIMELINE_FLAGS])
    elif mode == DISPATCH:
        key = value["keys"][value["key"] * KEY_BYTES : (value["key"] + 1) * KEY_BYTES]
        inputs += [
            int.from_bytes(key[:2], "little"),
            key[2],
            key[3],
            value["frame"],
            value["more"],
        ]
    return inputs


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    prepare(machine, value)
    calls: list[list[int]] = []
    for address, stub in recorder(calls, value).items():
        machine.stub(address, stub)
    machine.r[4] = INSTANCE
    # The byte operands arrive with random upper bits, which the originals mask off.
    high = value["high"]
    machine.r[5] = TRACK if mode == DISPATCH else value["index"] | high[0] << 8
    machine.r[6] = value["style"] | high[1] << 8
    machine.execute(entry)

    values, names = state(machine, mode)
    values += [len(calls), *(item for call in calls for item in call)]
    names += [
        "calls",
        *(
            f"call {index} word {word}"
            for index, call in enumerate(calls)
            for word in range(len(call))
        ),
    ]
    observation = {
        "values": values,
        "names": names,
        "calls": calls,
        "stores": list(machine.stores),
    }
    return observation, inputs_of(value)


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if value["mode"] in (RESUME, INVALIDATE):
        flags = transform(value["index"]) + FLAGS
        allowed.append((flags, flags + 2))
    elif value["mode"] == HOLD:
        allowed.append((TIMELINE + TIMELINE_FLAGS, TIMELINE + TIMELINE_FLAGS + 1))
    return allowed


def check(
    image, value: dict, observation: dict, native: list[int], reference
) -> list[str]:
    ours = observation["values"]
    if native != ours and len(native) != len(ours):
        return [f"native printed {native}, original {ours}"]
    return field_problems(
        observation["names"], native, ours, hexadecimal=False
    ) + store_problems(observation["stores"], allowed_stores(value))


def describe(image, results) -> list[str]:
    if not results or results[0][0]["mode"] != DISPATCH:
        return []
    performed = advanced = 0
    for _, observation, _ in results:
        kinds = [call[0] for call in observation["calls"]]
        performed += any(CALL_COMMAND <= kind < CALL_ADVANCE for kind in kinds)
        advanced += CALL_ADVANCE in kinds
    return [
        f"{performed} of {len(results)} vectors performed a command",
        f"{advanced} of {len(results)} vectors advanced the track",
    ]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
