"""Shared harness for the effect timeline playback differentials (BATTLE.PRG 5BF94.c:7885-8460, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects selection in
timeline_playback_driver.cpp. The effect instance is random bytes with its timeline pointer
(+0xD4C) leading to a random timeline; the package pointer (0x800F569C) leads to a random package
whose action request, vibration block and stream handle the case draws. A track's keys are random
words with the current key's due frame mostly matching the timeline frame.

The callees record their arguments: the curve sampler, particle emitter, stream and sound players,
child effect creation, renderer attachment, vibration, camera shake and the key advance, whose
result is drawn. The load's section, track data, free and allocation stubs return fixed buffers;
the dispatcher's twelve track ticks return drawn results and may change the timeline flags.

Native differences outside the compared domain (covered by TimelinePlaybackTests.cpp): a missing
section or track data, a child target's event actor slot past the key, a failed child creation, a
negative child index, a vibration pattern outside its block and the track kinds without a tick (0,
11 and from 14) are refused instead of read.
"""

from __future__ import annotations

import random
import sys
from collections import Counter

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

MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "timeline_playback_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\TimelinePlayback.cpp",)
# The sound bank tags the sound track uses include the CD drive's file identifiers.
INCLUDES = (r"Native\Content\include",)

INSTANCE = 0x80400000
INSTANCE_BYTES = 0xD60
SOUND_INSTANCE_BYTES = 0x3400
CHILD_WORD = 0x14
TRANSFORMS = 64
TRANSFORM_BYTES = 0xD0
TRANSLATION = 0x68
SOURCE_TYPE, SOURCE_ACTOR = 0xD1C, 0xD20
TARGET_TYPE, TARGET_ACTOR = 0xD34, 0xD38
TIMELINE_POINTER = 0xD4C
TIMELINE = 0x80410000
TIMELINE_BYTES = 0x1000
TIMELINE_FLAGS, TIMELINE_FRAME, TIMELINE_ACTIVE, TIMELINE_TRACKS = 1, 2, 4, 8
TRACK = 0x80420000
TRACK_BYTES = 0x10
KIND, KEY_COUNT, KEY_INDEX = 4, 5, 6
CURVE_FRAME, KEY_TIMER, KEY_PARAMETER, CHILD_INDEX = 8, 0xA, 0xC, 0xE
KEYS = 0x80430000
KEY_SLOTS = 0x40
KEY_BYTES = 8
CHILD_KEY_BYTES = 9
PACKAGE_POINTER = 0x800F569C
PACKAGE = 0x80440000
PACKAGE_BYTES = 0x100
PACKAGE_BLOCK11, PACKAGE_STREAM, PACKAGE_REQUEST = 0xC0, 0xCC, 0xD0
REQUEST = 0x80450000
REQUEST_BYTES = 0xC10
REQUEST_COUNT, REQUEST_TARGETS, TARGET_BYTES = 2, 0x10, 0xC
TARGET_ACTOR_BYTE, TARGET_SLOT = 4, 5
BLOCK11 = 0x80460000
BLOCK11_BYTES = 0x100
VIBRATION_PATTERNS = 32
VIBRATION_OFFSETS = 4
RENDERERS = 0x800F56A8
RENDERER_COUNT = 8
RENDERER_BASE = 0x80470000
CHILDREN = 0x80480000
CHILD_BYTES = 0x100
CHILD_ACTOR = 0x11
NEW_TIMELINE = 0x80490000
SECTION = 0x804A0000
SECTION_BYTES = 0x404
TRACK_DATA = 0x804B0000
ACTOR_PART = 4
CHILD_COUNT_SHIFT, CHILD_COUNT_BITS = 11, 0xF800

SECTION_ADDRESS = 0x800D5550
TRACK_DATA_ADDRESS = 0x800D5564
FREE_ADDRESS = 0x80043C60
ALLOC_ADDRESS = 0x80043E3C
ADVANCE_ADDRESS = 0x800D5780
SAMPLE_ADDRESS = 0x800D1104
EMIT_ADDRESS = 0x800D2ADC
STREAM_ADDRESS = 0x80046578
SFX_ADDRESS = 0x80045988
WORLD_SFX_ADDRESS = 0x80045AA8
CREATE_ADDRESS = 0x800CE83C
ATTACH_ADDRESS = 0x800CF694
VIBRATE_ADDRESS = 0x800433B4
SHAKE_ADDRESS = 0x800D1EF0
TICK_ADDRESSES = {
    1: 0x800D57FC,
    2: 0x800D5904,
    3: 0x800D5A98,
    4: 0x800D5D74,
    5: 0x800D5E00,
    6: 0x800D5F8C,
    7: 0x800D6048,
    8: 0x800D61AC,
    9: 0x800D6298,
    10: 0x800D6310,
    12: 0x800D6894,
    13: 0x800D6A18,
}
TARGET_COLOUR_KIND = 7

LOAD, PARTICLE, SOUND, CHILD, RENDERER, VIBRATION, SHAKE, DISPATCH = range(8)

# The kinds the driver prints for each call.
(
    CALL_SAMPLE,
    CALL_EMIT,
    CALL_STREAM,
    CALL_SFX,
    CALL_WORLD_SFX,
    CALL_SPAWN,
    CALL_ATTACH,
    CALL_VIBRATE,
    CALL_SHAKE,
    CALL_ADVANCE,
    CALL_TRACK_DATA,
    CALL_START,
    CALL_TICK,
) = range(1, 14)
CALL_NAMES = {
    CALL_SAMPLE: "sampled curves",
    CALL_EMIT: "emitted particles",
    CALL_STREAM: "played the stream",
    CALL_SFX: "played a centred sound",
    CALL_WORLD_SFX: "played a positioned sound",
    CALL_SPAWN: "spawned a child",
    CALL_ATTACH: "attached a renderer",
    CALL_VIBRATE: "vibrated",
    CALL_SHAKE: "shook the camera",
    CALL_ADVANCE: "advanced the track",
    CALL_START: "started a track",
    CALL_TICK: "ticked a track",
}


def word(data: bytes, at: int, size: int = 4) -> int:
    return int.from_bytes(data[at : at + size], "little")


def put(data: bytearray, at: int, value: int, size: int = 4) -> None:
    data[at : at + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")


def due_frame(rng: random.Random, due: int) -> int:
    """Mostly the due frame, otherwise a frame beside it or anywhere."""
    return rng.choice(
        (due,) * 6
        + (due + 1, due - 1, due + rng.randint(2, 40), s16(rng.getrandbits(16)))
    )


def endpoint_type(rng: random.Random) -> int:
    """Mostly an actor part, otherwise a room point, nothing or one sharing its low byte."""
    return rng.choice((ACTOR_PART,) * 5 + (5, 0, 0x104, rng.getrandbits(16)))


def target_count(rng: random.Random) -> int:
    return rng.choice(
        (0, 1, 2, rng.randint(1, 16), rng.randint(1, 16), rng.randint(17, 60))
    )


def keyed(rng: random.Random, value: dict, key: bytes, stride: int) -> None:
    """Random keys `stride` bytes apart with `key` at a random current key (the first when 0)."""
    count = rng.randint(1, KEY_SLOTS)
    index = rng.choice((0, count - 1, rng.randrange(count)))
    keys = bytearray(rng.randbytes(KEY_SLOTS * KEY_BYTES))
    at = index * stride
    keys[at : at + len(key)] = key
    value.update(keys=bytes(keys), count=count, key_index=index)


def load_vector(rng: random.Random, value: dict) -> None:
    count = rng.choice(
        (0, 1, 2, rng.randint(1, 8), rng.randint(1, 40), rng.randrange(256))
    )
    section = bytearray(rng.randbytes(SECTION_BYTES))
    section[0] = count
    for track in range(count):
        kind = rng.choice((3, 3, 1, 2, 13, rng.randrange(256)))
        put(section, 4 + track * 4, rng.getrandbits(8) << 8 | kind, 2)
    value.update(
        section=bytes(section),
        section_index=rng.randrange(256),
        old_timeline=rng.choice((0, TIMELINE)),
    )


def particle_vector(rng: random.Random, value: dict) -> None:
    key = rng.getrandbits(32)
    keyed(rng, value, key.to_bytes(4, "little"), 4)
    value.update(
        key=key,
        frame=due_frame(rng, key & 0xFF),
        curve=rng.choice((0, rng.randint(0, 8), s16(rng.getrandbits(16)))),
        timer=rng.choice((1, 1, 2, rng.randint(1, 10), 0, s16(rng.getrandbits(16)))),
        flags=rng.getrandbits(8),
    )


def sound_vector(rng: random.Random, value: dict) -> None:
    key = rng.getrandbits(32)
    keyed(rng, value, key.to_bytes(4, "little"), 4)
    value.update(
        key=key,
        frame=due_frame(rng, key & 0x1FF),
        handle=rng.choice((0, 0, rng.getrandbits(32))),
        instance=rng.randbytes(SOUND_INSTANCE_BYTES),
    )


def child_vector(rng: random.Random, value: dict) -> None:
    key = bytearray(rng.randbytes(CHILD_KEY_BYTES))
    key[8] = rng.choice((0, 1, 2, rng.randint(0, 8), rng.randrange(256)))
    keyed(rng, value, bytes(key), 0)
    count = target_count(rng)
    request = bytearray(rng.randbytes(REQUEST_BYTES))
    request[REQUEST_COUNT] = count
    for target in range(count):
        at = REQUEST_TARGETS + target * TARGET_BYTES
        put(request, at, endpoint_type(rng), 2)
        request[at + TARGET_SLOT] = rng.randrange(CHILD_KEY_BYTES)
    instance = bytearray(rng.randbytes(INSTANCE_BYTES))
    # Mostly no children running, otherwise one, the field's top bit alone or any count.
    children = rng.choice((0, 0, 0, 1, 0x10, rng.randrange(0x20)))
    child_word = (
        word(instance, CHILD_WORD) & ~CHILD_COUNT_BITS | children << CHILD_COUNT_SHIFT
    )
    put(instance, CHILD_WORD, child_word)
    value.update(
        child_key=bytes(key),
        frame=due_frame(rng, key[7]),
        request=bytes(request),
        instance=bytes(instance),
        delay=rng.choice((0, 0, 0, 1, rng.randint(1, 5), s16(rng.getrandbits(16)))),
        index=rng.choice(
            (
                0,
                count,
                count + 1,
                rng.randint(0, count),
                rng.randint(0, count + 4),
                0x7FFF,
            )
        ),
    )


def renderer_vector(rng: random.Random, value: dict) -> None:
    timing = rng.getrandbits(32)
    high = rng.getrandbits(16)
    keyed(
        rng, value, timing.to_bytes(4, "little") + high.to_bytes(2, "little"), KEY_BYTES
    )
    value.update(timing=timing, high=high, frame=due_frame(rng, timing & 0x1FF))


def vibration_vector(rng: random.Random, value: dict) -> None:
    key = rng.getrandbits(16)
    keyed(rng, value, key.to_bytes(2, "little"), 2)
    instance = bytearray(rng.randbytes(INSTANCE_BYTES))
    for type_at, actor_at in ((SOURCE_TYPE, SOURCE_ACTOR), (TARGET_TYPE, TARGET_ACTOR)):
        put(instance, type_at, endpoint_type(rng), 2)
        instance[actor_at] = rng.choice((0, 0, 0, 1, rng.randrange(256)))
    count = target_count(rng)
    request = bytearray(rng.randbytes(REQUEST_BYTES))
    request[REQUEST_COUNT] = count
    for target in range(count):
        at = REQUEST_TARGETS + target * TARGET_BYTES
        put(request, at, endpoint_type(rng), 2)
        request[at + TARGET_ACTOR_BYTE] = rng.choice((0, 1, 1, 1, rng.randrange(256)))
    block = bytearray(rng.randbytes(BLOCK11_BYTES))
    offsets = [rng.randrange(BLOCK11_BYTES) for _ in range(VIBRATION_PATTERNS)]
    for pattern, offset in enumerate(offsets):
        put(block, VIBRATION_OFFSETS + pattern * 2, offset, 2)
    value.update(
        key=key,
        frame=due_frame(rng, key & 0x1FF),
        instance=bytes(instance),
        request=bytes(request),
        block=bytes(block),
        offsets=offsets,
    )


def shake_vector(rng: random.Random, value: dict) -> None:
    timing = rng.getrandbits(16)
    keyed(rng, value, timing.to_bytes(2, "little"), KEY_BYTES)
    keys = value["keys"]
    value.update(
        timing=timing,
        channels=word(keys, 4),
        frame=due_frame(rng, timing & 0x1FF),
        timer=rng.choice((1, 1, 2, rng.randint(1, 10), 0, s16(rng.getrandbits(16)))),
    )


VALID_KINDS = tuple(TICK_ADDRESSES)


def dispatch_vector(rng: random.Random, value: dict) -> None:
    count = rng.choice(
        (0, 1, 2, rng.randint(1, 8), rng.randint(1, 32), rng.randint(33, 255), 255)
    )
    value.update(
        track_count=count,
        frame=rng.choice((0, 0x7FFF, -1, s16(rng.getrandbits(16)))),
        active=rng.choice((0xFFFFFFFF, 0, rng.getrandbits(32), rng.getrandbits(32))),
        flags=rng.getrandbits(8),
        kinds=[
            rng.choice(VALID_KINDS + (TARGET_COLOUR_KIND,) * 2) for _ in range(count)
        ],
        ticks=[
            (
                rng.choice((0, 1, 1, 0x100, rng.getrandbits(32))),
                rng.choice((-1, -1, -1, rng.getrandbits(8))),
            )
            for _ in range(count)
        ],
        parent=rng.getrandbits(32),
    )


VECTORS = {
    LOAD: load_vector,
    PARTICLE: particle_vector,
    SOUND: sound_vector,
    CHILD: child_vector,
    RENDERER: renderer_vector,
    VIBRATION: vibration_vector,
    SHAKE: shake_vector,
    DISPATCH: dispatch_vector,
}


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {
        "mode": mode,
        "number": number,
        "instance": rng.randbytes(INSTANCE_BYTES),
        "timeline": rng.randbytes(TIMELINE_BYTES),
        "track": rng.randbytes(TRACK_BYTES),
        "package": rng.randbytes(PACKAGE_BYTES),
        "frame": s16(rng.getrandbits(16)),
        "flags": rng.getrandbits(8),
        "more": rng.choice((0, 1, 1, rng.getrandbits(32))),
    }
    VECTORS[mode](rng, value)
    return value


def prepare(machine: Machine, value: dict) -> None:
    mode = value["mode"]
    machine.store(INSTANCE, value["instance"])
    machine.put(INSTANCE + TIMELINE_POINTER, TIMELINE)
    machine.store(TIMELINE, value["timeline"])
    machine.put(TIMELINE + TIMELINE_FLAGS, value["flags"], 1)
    machine.put(TIMELINE + TIMELINE_FRAME, value["frame"], 2)
    machine.store(PACKAGE, value["package"])
    machine.put(PACKAGE_POINTER, PACKAGE)
    machine.put(PACKAGE + PACKAGE_REQUEST, REQUEST)
    machine.put(PACKAGE + PACKAGE_BLOCK11, BLOCK11)
    machine.put(PACKAGE + PACKAGE_STREAM, value.get("handle", 0))
    if "request" in value:
        machine.store(REQUEST, value["request"])
    if "block" in value:
        machine.store(BLOCK11, value["block"])
    if "keys" in value:
        machine.store(TRACK, value["track"])
        machine.put(TRACK, KEYS)
        machine.put(TRACK + KEY_COUNT, value["count"], 1)
        machine.put(TRACK + KEY_INDEX, value["key_index"], 2)
        machine.store(KEYS, value["keys"])
    if mode == PARTICLE:
        machine.put(TRACK + CURVE_FRAME, value["curve"], 2)
        machine.put(TRACK + KEY_TIMER, value["timer"], 2)
    elif mode == SHAKE:
        machine.put(TRACK + KEY_TIMER, value["timer"], 2)
    elif mode == CHILD:
        machine.put(TRACK + KEY_PARAMETER, value["delay"], 2)
        machine.put(TRACK + CHILD_INDEX, value["index"], 2)
        machine.map_zero(CHILDREN, CHILD_BYTES * 0x101)
    elif mode == RENDERER:
        for renderer in range(RENDERER_COUNT):
            machine.put(RENDERERS + renderer * 4, RENDERER_BASE + renderer * 0x10)
    elif mode == LOAD:
        machine.put(INSTANCE + TIMELINE_POINTER, value["old_timeline"])
        machine.store(SECTION, value["section"])
        machine.map_zero(NEW_TIMELINE, TIMELINE_BYTES)
    elif mode == DISPATCH:
        machine.put(TIMELINE, value["track_count"], 1)
        machine.put(TIMELINE + TIMELINE_ACTIVE, value["active"])
        for track, kind in enumerate(value["kinds"]):
            machine.put(
                TIMELINE + TIMELINE_TRACKS + track * TRACK_BYTES + KIND, kind, 1
            )


def stack(machine: Machine, offset: int) -> int:
    return machine.load(machine.r[29] + offset, 4, signed=True)


def advance_stub(calls: list[list[int]], value: dict):
    def advance(machine: Machine) -> None:
        calls.append([CALL_ADVANCE])
        machine.r[2] = value["more"] & 0xFFFFFFFF

    return advance


def recorder(calls: list[list[int]], children: list[int], value: dict) -> dict:
    """The stubs every mode shares; each appends its call to `calls`."""

    def sample(machine: Machine) -> None:
        calls.append([CALL_SAMPLE, s32(machine.r[4])])

    def emit(machine: Machine) -> None:
        calls.append([CALL_EMIT, *(s32(machine.r[index]) for index in (5, 6, 7))])

    def stream(machine: Machine) -> None:
        calls.append([CALL_STREAM, s32(machine.r[4])])

    def sfx(machine: Machine) -> None:
        calls.append([CALL_SFX, s32(machine.r[4]), s32(machine.r[5])])

    def world_sfx(machine: Machine) -> None:
        point = machine.r[6]
        coordinates = [machine.load(point + at, 2, signed=True) for at in (0, 2, 4)]
        calls.append(
            [CALL_WORLD_SFX, s32(machine.r[4]), s32(machine.r[5]), *coordinates]
        )

    def create(machine: Machine) -> None:
        arguments = machine.r[4]
        target = machine.load(arguments + 8) - REQUEST - REQUEST_TARGETS
        child = CHILDREN + len(children) * CHILD_BYTES
        children.append(child)
        calls.append([CALL_SPAWN, s32(target) // TARGET_BYTES, -1])
        machine.r[2] = child

    def attach(machine: Machine) -> None:
        renderer = machine.r[5] - RENDERER_BASE
        calls.append([CALL_ATTACH, s32(renderer) // 0x10, s32(machine.r[6])])

    def vibrate(machine: Machine) -> None:
        calls.append([CALL_VIBRATE, s32(machine.r[4] - BLOCK11)])

    def shake(machine: Machine) -> None:
        registers = [s32(machine.r[index]) for index in (4, 5, 6, 7)]
        calls.append(
            [CALL_SHAKE, *registers, *(stack(machine, at) for at in (0x10, 0x14, 0x18))]
        )

    return {
        SAMPLE_ADDRESS: sample,
        EMIT_ADDRESS: emit,
        STREAM_ADDRESS: stream,
        SFX_ADDRESS: sfx,
        WORLD_SFX_ADDRESS: world_sfx,
        CREATE_ADDRESS: create,
        ATTACH_ADDRESS: attach,
        VIBRATE_ADDRESS: vibrate,
        SHAKE_ADDRESS: shake,
        ADVANCE_ADDRESS: advance_stub(calls, value),
    }


def load_stubs(calls: list[list[int]]) -> dict:
    def section(machine: Machine) -> None:
        machine.r[2] = SECTION

    def track_data(machine: Machine) -> None:
        track = machine.r[6]
        calls.append([CALL_TRACK_DATA, s32(track)])
        machine.r[2] = TRACK_DATA + track * TRACK_BYTES

    def allocate(machine: Machine) -> None:
        machine.r[2] = NEW_TIMELINE

    def start(machine: Machine) -> None:
        track = machine.r[4]
        index = (track - NEW_TIMELINE - TIMELINE_TRACKS) // TRACK_BYTES
        fields = [
            machine.load(track + KIND, 2),
            machine.load(track + KEY_INDEX, 2, signed=True),
            machine.load(track + CURVE_FRAME, 2, signed=True),
            machine.load(track + KEY_TIMER, 2, signed=True),
            machine.load(track + KEY_PARAMETER, 4, signed=True),
        ]
        calls.append([CALL_START, index, *fields])

    return {
        SECTION_ADDRESS: section,
        TRACK_DATA_ADDRESS: track_data,
        FREE_ADDRESS: None,
        ALLOC_ADDRESS: allocate,
        ADVANCE_ADDRESS: start,
    }


def dispatch_stubs(calls: list[list[int]], value: dict) -> dict:
    ticks = iter(value["ticks"])

    def tick(kind: int):
        def stub(machine: Machine) -> None:
            track = (machine.r[5] - TIMELINE - TIMELINE_TRACKS) // TRACK_BYTES
            extra = int(machine.r[6] != 0) if kind == TARGET_COLOUR_KIND else 0
            calls.append([CALL_TICK, kind, s32(track), extra])
            result, flags = next(ticks, (0, -1))
            if flags >= 0:
                machine.put(TIMELINE + TIMELINE_FLAGS, flags, 1)
            machine.r[2] = result

        return stub

    return {address: tick(kind) for kind, address in TICK_ADDRESSES.items()}


def state(machine: Machine, mode: int) -> tuple[list[int], list[str]]:
    """The values the original may have changed and its result."""
    if mode == LOAD:
        values = [
            machine.load(NEW_TIMELINE, 1),
            machine.load(NEW_TIMELINE + TIMELINE_FLAGS, 1),
            machine.load(NEW_TIMELINE + TIMELINE_ACTIVE, 4, signed=True),
        ]
        return values, ["track count", "flags", "active tracks"]
    if mode == DISPATCH:
        values = [
            machine.load(TIMELINE + TIMELINE_ACTIVE, 4, signed=True),
            machine.load(TIMELINE + TIMELINE_FRAME, 2, signed=True),
        ]
        return values, ["active tracks", "frame"]
    values, names = [s32(machine.r[2])], ["result"]
    if mode == PARTICLE:
        values.append(machine.load(TRACK + CURVE_FRAME, 2, signed=True))
        names.append("curve frame")
    if mode in (PARTICLE, SHAKE):
        values.append(machine.load(TRACK + KEY_TIMER, 2, signed=True))
        names.append("key timer")
    if mode == CHILD:
        values += [
            machine.load(TRACK + KEY_PARAMETER, 2, signed=True),
            machine.load(TRACK + CHILD_INDEX, 2, signed=True),
            machine.load(INSTANCE + CHILD_WORD, 4, signed=True),
        ]
        names += ["delay", "index", "child word"]
    return values, names


def translations(instance: bytes) -> list[int]:
    """Each transform's world translation words, as the sound track would read them."""
    values = []
    for transform in range(TRANSFORMS):
        at = transform * TRANSFORM_BYTES + TRANSLATION
        values += [s32(word(instance, at + axis * 4)) for axis in range(3)]
    return values


def targets_of(request: bytes, count: int, byte: int) -> list[int]:
    values = []
    for target in range(count):
        at = REQUEST_TARGETS + target * TARGET_BYTES
        values += [word(request, at, 2), request[at + byte]]
    return values


def inputs_of(value: dict, machine: Machine) -> list[int]:
    mode, more = value["mode"], s32(value["more"])
    if mode == LOAD:
        section = value["section"]
        count = section[0]
        return [
            mode,
            count,
            *(word(section, 4 + track * 4, 2) for track in range(count)),
        ]
    if mode == PARTICLE:
        held = value["flags"] & 1
        return [
            mode,
            value["key"],
            value["frame"],
            value["curve"],
            value["timer"],
            held,
            more,
        ]
    if mode == SOUND:
        instance = machine.bytes(INSTANCE, SOUND_INSTANCE_BYTES)
        return [
            mode,
            value["key"],
            value["frame"],
            s32(value["handle"]),
            more,
            *translations(instance),
        ]
    if mode == CHILD:
        request = value["request"]
        count = request[REQUEST_COUNT]
        instance = value["instance"]
        return [
            mode,
            *value["child_key"],
            value["frame"],
            count,
            *targets_of(request, count, TARGET_SLOT),
            value["delay"],
            value["index"],
            word(instance, CHILD_WORD),
        ]
    if mode == RENDERER:
        return [mode, value["timing"], value["high"], value["frame"], more]
    if mode == VIBRATION:
        instance, request = value["instance"], value["request"]
        count = request[REQUEST_COUNT]
        endpoints = [
            word(instance, SOURCE_TYPE, 2),
            instance[SOURCE_ACTOR],
            word(instance, TARGET_TYPE, 2),
            instance[TARGET_ACTOR],
        ]
        return [
            mode,
            value["key"],
            value["frame"],
            *endpoints,
            count,
            *targets_of(request, count, TARGET_ACTOR_BYTE),
            *value["offsets"],
            more,
        ]
    if mode == SHAKE:
        return [
            mode,
            value["timing"],
            value["channels"],
            value["frame"],
            value["timer"],
            more,
        ]
    count = value["track_count"]
    ticks = [item for result, flags in value["ticks"] for item in (s32(result), flags)]
    return [
        mode,
        count,
        value["frame"],
        s32(value["active"]),
        value["flags"],
        *value["kinds"],
        *ticks,
    ]


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    prepare(machine, value)
    calls: list[list[int]] = []
    children: list[int] = []
    stubs = recorder(calls, children, value)
    if mode == LOAD:
        stubs.update(load_stubs(calls))
    elif mode == DISPATCH:
        stubs.update(dispatch_stubs(calls, value))
    for address, stub in stubs.items():
        machine.stub(address, stub)
    machine.r[4] = INSTANCE
    if mode == LOAD:
        machine.r[5] = value["section_index"]
    elif mode == DISPATCH:
        machine.r[5] = value["parent"]
    else:
        machine.r[5] = TRACK
    machine.execute(entry)
    spawns = (call for call in calls if call[0] == CALL_SPAWN)
    for call, child in zip(spawns, children, strict=True):
        call[2] = machine.load(child + CHILD_ACTOR, 1)
    return recorded_observation(machine, *state(machine, mode), calls), inputs_of(
        value, machine
    )


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    mode = value["mode"]
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if mode == LOAD:
        allowed += [
            (INSTANCE + TIMELINE_POINTER, INSTANCE + TIMELINE_POINTER + 4),
            (NEW_TIMELINE, NEW_TIMELINE + TIMELINE_BYTES),
        ]
    elif mode == PARTICLE:
        allowed.append((TRACK + CURVE_FRAME, TRACK + KEY_TIMER + 2))
    elif mode == SHAKE:
        allowed.append((TRACK + KEY_TIMER, TRACK + KEY_TIMER + 2))
    elif mode == CHILD:
        allowed += [
            (TRACK + KEY_PARAMETER, TRACK + TRACK_BYTES),
            (INSTANCE + CHILD_WORD, INSTANCE + CHILD_WORD + 4),
            (CHILDREN, CHILDREN + CHILD_BYTES * 0x101),
        ]
    elif mode == DISPATCH:
        allowed.append((TIMELINE + TIMELINE_FRAME, TIMELINE + TIMELINE_TRACKS))
    return allowed


def check(
    image, value: dict, observation: dict, native: list[int], reference
) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    """How many vectors made each kind of call."""
    made = Counter(
        kind
        for _, observation, _ in results
        for kind in {call[0] for call in observation["calls"]}
    )
    return [
        f"{made[call]} of {len(results)} vectors {name}"
        for call, name in CALL_NAMES.items()
        if made[call]
    ]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    case = mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
    return {**case, "INCLUDES": INCLUDES}
