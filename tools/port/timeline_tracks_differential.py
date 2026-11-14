"""Shared harness for the effect timeline key advance and colour track differentials (BATTLE.PRG
5BF94.c:7920-8211, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects selection in
timeline_tracks_driver.cpp. The track's keys are random words; its current key index, count and
kind are drawn so the advance reaches the last key, the first key (-1) and the halfword limit. The
effect instance is random bytes with the timeline pointer (+0xD4C) leading to a random timeline
whose frame (+2) mostly matches the current colour key's frame. The action request (0x800F5230)
is random bytes with its target and other-actor counts within its sixteen entries.

The actor state flags, palette animation, curve duration and track advance stubs record their
calls. Each actor's state flags are a random word; the driver gets the visible bit (2) of every
actor.

Native differences outside the compared domain (covered by TimelineTracksTests.cpp): a missing
track, keys, instance or timeline, a current key before the first or past the last, and target or
other counts past the request's sixteen are refused instead of read.
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
    s16,
    s32,
)

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "timeline_tracks_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\TimelineTracks.cpp",)

INSTANCE = 0x80400000
INSTANCE_BYTES = 0xD60
EMIT_FROM, EMIT_TO = 0xD28, 0xD40
ENDPOINT_ACTOR = 4
TIMELINE_POINTER = 0xD4C
TIMELINE = 0x80410000
TIMELINE_BYTES = 8
TIMELINE_FRAME = 2
TRACK = 0x80420000
TRACK_BYTES = 0x10
KIND, KEY_COUNT, KEY_INDEX, KEY_TIMER, KEY_PARAMETER = 4, 5, 6, 0xA, 0xC
KEYS = 0x80430000
KEY_SLOTS = 0x100
COLOUR_KEY_BYTES = 8
SHAKE_KEY_BYTES = 8
PARTICLE_KEY_BYTES = 4
MAX_COLOUR_KEYS = 8
REQUEST = 0x800F5230
REQUEST_BYTES = 0xE8
COUNT, OTHER_COUNT = 2, 3
TARGETS, TARGET_BYTES, OTHERS = 0x10, 0xC, 0xD0
ACTION_ENTRIES = 16
ACTORS = 0x100
MASK_WORDS = ACTORS // 32
VISIBLE = 2
ACTOR_ENDPOINT = 4
PARTICLE_TRACK, SHAKE_TRACK = 1, 13

STATE_FLAGS_ADDRESS = 0x800A0BE0
ACTOR_COLOURS_ADDRESS = 0x8007B29C
SCENE_COLOURS_ADDRESS = 0x8007B1B8
BACKGROUND_COLOURS_ADDRESS = 0x8007B344
CURVE_DURATION_ADDRESS = 0x800D12D8
PARTICLE_SETUP_ADDRESS = 0x800D5700
SHAKE_SETUP_ADDRESS = 0x800D5738
ADVANCE_ADDRESS = 0x800D5780

PARTICLE, SHAKE, ADVANCE, SOURCE, TARGET, OTHER, SCENE, BACKGROUND = range(8)
COLOUR_MODES = (SOURCE, TARGET, OTHER, SCENE, BACKGROUND)

# The kinds the driver prints for each call.
CALL_VISIBLE, CALL_ACTOR, CALL_SCENE, CALL_BACKGROUND, CALL_CURVE, CALL_ADVANCE = range(
    1, 7
)


def endpoint_type(rng: random.Random) -> int:
    """Mostly an actor part, otherwise a room point, nothing or one sharing its low byte."""
    return rng.choice((ACTOR_ENDPOINT,) * 5 + (5, 0, 0x104, rng.getrandbits(16)))


def track_kind(rng: random.Random) -> int:
    return rng.choice((PARTICLE_TRACK, SHAKE_TRACK, rng.randrange(256)))


def advance_index(rng: random.Random, count: int) -> int:
    """A current key from before the first to past the last, sometimes the halfword limit."""
    return rng.choice(
        (-1, -1, max(count - 1, -1), max(count - 2, -1), 0x7FFF)
        + tuple(rng.randrange(-1, max(count, 0) + 2) for _ in range(5))
    )


def colour_vector(rng: random.Random, value: dict) -> None:
    count = rng.randint(1, MAX_COLOUR_KEYS)
    key = rng.randrange(count)
    frame = s16(rng.getrandbits(16))
    keys = bytearray(value["keys"])
    due = rng.choice(
        (frame,) * 6 + (frame + 1, frame - 1, frame ^ 0x8000, s16(rng.getrandbits(16)))
    )
    at = key * COLOUR_KEY_BYTES
    keys[at : at + 2] = (due & 0xFFFF).to_bytes(2, "little")
    request = bytearray(value["request"])
    request[COUNT] = rng.choice((0, ACTION_ENTRIES, rng.randint(0, ACTION_ENTRIES)))
    request[OTHER_COUNT] = rng.choice(
        (0, ACTION_ENTRIES, rng.randint(0, ACTION_ENTRIES))
    )
    for target in range(ACTION_ENTRIES):
        at = TARGETS + target * TARGET_BYTES
        request[at : at + 2] = endpoint_type(rng).to_bytes(2, "little")
    instance = bytearray(value["instance"])
    for offset in (EMIT_FROM, EMIT_TO):
        instance[offset : offset + 2] = endpoint_type(rng).to_bytes(2, "little")
    value.update(
        keys=bytes(keys),
        count=count,
        key=key,
        frame=frame,
        request=bytes(request),
        instance=bytes(instance),
        flags=[rng.getrandbits(32) for _ in range(ACTORS)],
        all=rng.choice((0, 0, 2, 1, rng.getrandbits(32))),
        more=rng.randrange(2),
    )


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {
        "mode": mode,
        "number": number,
        "instance": rng.randbytes(INSTANCE_BYTES),
        "timeline": rng.randbytes(TIMELINE_BYTES),
        "track": rng.randbytes(TRACK_BYTES),
        "keys": rng.randbytes(KEY_SLOTS * SHAKE_KEY_BYTES),
        "request": rng.randbytes(REQUEST_BYTES),
        "duration": rng.choice((rng.getrandbits(32), rng.randrange(-0x8000, 0x8000))),
        "kind": track_kind(rng),
    }
    if mode in COLOUR_MODES:
        colour_vector(rng, value)
    elif mode == ADVANCE:
        count = rng.choice((0, 1, 2, rng.randint(1, 8), 0xFF, rng.randrange(256)))
        value.update(count=count, key=advance_index(rng, count))
    else:
        count = rng.choice((1, rng.randint(1, 8), rng.randint(1, 0xFF)))
        value.update(count=count, key=rng.randrange(count))
    return value


def prepare(machine: Machine, value: dict) -> None:
    machine.store(INSTANCE, value["instance"])
    machine.put(INSTANCE + TIMELINE_POINTER, TIMELINE)
    machine.store(TIMELINE, value["timeline"])
    machine.store(TRACK, value["track"])
    machine.put(TRACK, KEYS)
    machine.put(TRACK + KIND, value["kind"], 1)
    machine.put(TRACK + KEY_COUNT, value["count"], 1)
    machine.put(TRACK + KEY_INDEX, value["key"], 2)
    machine.store(KEYS, value["keys"])
    machine.store(REQUEST, value["request"])
    if value["mode"] in COLOUR_MODES:
        machine.put(TIMELINE + TIMELINE_FRAME, value["frame"], 2)


def recorder(calls: list[list[int]], value: dict) -> dict:
    def stack(machine: Machine, offset: int) -> int:
        return machine.load(machine.r[29] + offset, 4, signed=True)

    def state_flags(machine: Machine) -> None:
        actor = machine.r[4]
        calls.append([CALL_VISIBLE, s32(actor)])
        machine.r[2] = value["flags"][actor & 0xFF] if actor < ACTORS else 0

    def actor_colours(machine: Machine) -> None:
        arguments = [s32(machine.r[4 + index]) for index in range(4)]
        calls.append(
            [CALL_ACTOR, *arguments, stack(machine, 0x10), stack(machine, 0x14)]
        )

    def colours(call: int):
        def stub(machine: Machine) -> None:
            arguments = [s32(machine.r[4 + index]) for index in range(4)]
            calls.append([call, *arguments, stack(machine, 0x10)])

        return stub

    def curve_duration(machine: Machine) -> None:
        calls.append([CALL_CURVE, s32(machine.r[4])])
        machine.r[2] = value["duration"] & 0xFFFFFFFF

    stubs = {
        STATE_FLAGS_ADDRESS: state_flags,
        ACTOR_COLOURS_ADDRESS: actor_colours,
        SCENE_COLOURS_ADDRESS: colours(CALL_SCENE),
        BACKGROUND_COLOURS_ADDRESS: colours(CALL_BACKGROUND),
        CURVE_DURATION_ADDRESS: curve_duration,
    }
    if value["mode"] in COLOUR_MODES:

        def advance(machine: Machine) -> None:
            calls.append([CALL_ADVANCE, int(machine.r[4] == TRACK)])
            machine.r[2] = value["more"]

        stubs[ADVANCE_ADDRESS] = advance
    return stubs


def word(data: bytes, at: int) -> int:
    return (
        int.from_bytes(data[at : at + 4], "little") if 0 <= at <= len(data) - 4 else 0
    )


def track_inputs(value: dict) -> list[int]:
    track = value["track"]
    keys = value["keys"]
    index = value["key"]
    if value["mode"] == ADVANCE:
        index += 1
    inputs = [
        value["key"],
        value["count"],
        value["kind"],
        s16(int.from_bytes(track[KEY_TIMER : KEY_TIMER + 2], "little")),
        s32(word(track, KEY_PARAMETER)),
        word(keys, index * PARTICLE_KEY_BYTES),
        word(keys, index * SHAKE_KEY_BYTES + 4),
    ]
    return inputs + [s32(value["duration"])]


def endpoint(data: bytes, at: int) -> list[int]:
    return [int.from_bytes(data[at : at + 2], "little"), data[at + ENDPOINT_ACTOR]]


def colour_inputs(value: dict) -> list[int]:
    at = value["key"] * COLOUR_KEY_BYTES
    key = value["keys"][at : at + COLOUR_KEY_BYTES]
    inputs = [
        s16(int.from_bytes(key[0:2], "little")),
        s16(int.from_bytes(key[2:4], "little")),
        *key[4:8],
        value["frame"],
        value["more"],
    ]
    mode = value["mode"]
    instance = value["instance"]
    request = value["request"]
    if mode == SOURCE:
        inputs += endpoint(instance, EMIT_FROM)
    elif mode == TARGET:
        inputs += [s32(value["all"]), request[COUNT]]
        for target in range(request[COUNT]):
            inputs += endpoint(request, TARGETS + target * TARGET_BYTES)
        inputs += endpoint(instance, EMIT_TO)
    elif mode == OTHER:
        inputs.append(request[OTHER_COUNT])
        inputs += request[OTHERS : OTHERS + request[OTHER_COUNT]]
    if mode in (SOURCE, TARGET, OTHER):
        visible = [bool(flags & VISIBLE) for flags in value["flags"]]
        inputs += [
            sum(visible[base + bit] << bit for bit in range(32))
            for base in range(0, ACTORS, 32)
        ]
    return inputs


def inputs_of(value: dict) -> list[int]:
    if value["mode"] in COLOUR_MODES:
        return [value["mode"], *colour_inputs(value)]
    return [value["mode"], *track_inputs(value)]


def state(machine: Machine, mode: int) -> tuple[list[int], list[str]]:
    """The track fields the original may have changed and the result."""
    timer = machine.load(TRACK + KEY_TIMER, 2, signed=True)
    if mode == PARTICLE:
        parameter = machine.load(TRACK + KEY_PARAMETER, 4, signed=True)
        return [timer, parameter], ["key timer", "key parameter"]
    if mode == SHAKE:
        return [timer], ["key timer"]
    result = s32(machine.r[2])
    if mode == ADVANCE:
        index = machine.load(TRACK + KEY_INDEX, 2, signed=True)
        parameter = machine.load(TRACK + KEY_PARAMETER, 4, signed=True)
        return [result, index, timer, parameter], [
            "result",
            "key index",
            "key timer",
            "key parameter",
        ]
    return [result], ["result"]


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    prepare(machine, value)
    calls: list[list[int]] = []
    for address, stub in recorder(calls, value).items():
        machine.stub(address, stub)
    if mode in COLOUR_MODES:
        machine.r[4] = INSTANCE
        machine.r[5] = TRACK
        machine.r[6] = value["all"] & 0xFFFFFFFF
    else:
        machine.r[4] = TRACK
    if mode == ADVANCE:
        # The advance sets up the new key with the real particle and shake key setups.
        machine.follow.update((PARTICLE_SETUP_ADDRESS, SHAKE_SETUP_ADDRESS))
    machine.execute(entry)
    return recorded_observation(machine, *state(machine, mode), calls), inputs_of(value)


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    mode = value["mode"]
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if mode in (PARTICLE, ADVANCE):
        allowed.append((TRACK + KEY_PARAMETER, TRACK + KEY_PARAMETER + 4))
    if mode in (PARTICLE, SHAKE, ADVANCE):
        allowed.append((TRACK + KEY_TIMER, TRACK + KEY_TIMER + 2))
    if mode == ADVANCE:
        allowed.append((TRACK + KEY_INDEX, TRACK + KEY_INDEX + 2))
    return allowed


def check(
    image, value: dict, observation: dict, native: list[int], reference
) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    if not results:
        return []
    kinds = [
        {call[0] for call in observation["calls"]} for _, observation, _ in results
    ]
    shifted = sum(
        bool(found & {CALL_ACTOR, CALL_SCENE, CALL_BACKGROUND}) for found in kinds
    )
    advanced = sum(CALL_ADVANCE in found for found in kinds)
    curves = sum(CALL_CURVE in found for found in kinds)
    lines = [f"{shifted} of {len(results)} vectors shifted colours"]
    lines.append(f"{advanced} of {len(results)} vectors advanced the track")
    lines.append(f"{curves} of {len(results)} vectors read a curve duration")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
