"""Shared harness for the secondary hit and spatial effect request differentials (BATTLE.PRG 146C.c:1927-3298,
#276).

Each case runs one original producer under the R3000 interpreter against its typed VS::Effects core
(SecondaryEffectRequests.cpp) in secondary_effects_driver.cpp. The stubs record each callee in call order: the
secondary request begin (func_800CF3F8) with its parameter and the request fields the producer defines, read from the
request it passed; the bone tag lookup (func_800A152C) with its arguments; and the weapon hit sounds (func_8006C5AC)
with the attacker. The caller's stack is filled with random bytes first, so a field the original fails to write
differs from the native one, while fields no producer writes (the request's padding and unused endpoint bytes) are
never compared. func_8006C84C runs its own call to func_8006C4C4 in the interpreter.

Native differences outside the compared domain (covered by SecondaryEffectRequestsTests.cpp): fields the original left
as stack bytes are zero, a missing action state or a queued target past the request's 255 targets ends the walk, and
a null input pointer is refused.
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
    s16,
    s32,
)

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "secondary_effects_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\SecondaryEffectRequests.cpp",)

HIT, IMPACTS, GROUND, TRAP, LEARNED = range(5)

ACTION_STATE_POINTER = 0x800F19CC
BUSY_FLAGS = 0x800F1868
DEFINITIONS = 0x8004B9DC
DEFINITION_STRIDE = 0x34
DEFINED_ACTIONS = 0x28
PREREQUISITE_WORD = 0x14
BEGIN, BONE_TAG, SOUNDS = 0x800CF3F8, 0x800A152C, 0x8006C5AC
HIT_EFFECT = 0x8006C4C4
BEGIN_CALL, BONE_CALL, SOUND_CALL = 1, 2, 3

STATE = 0x80500000
QUEUE, QUEUE_STRIDE = 0x854, 0x84C
STATE_BYTES = QUEUE + 4 * QUEUE_STRIDE
TARGETS, TARGET_STRIDE = 0x4C, 0x44
TARGETS_POSITION = 0x40
INPUT = 0x80600000
STACK_FILL = 0x400
ACTOR_ENDPOINT = 4
# Where a recorded begin call holds the target kind, per mode.
KIND_INDEX = {HIT: 8, IMPACTS: 8, GROUND: 7, TRAP: 7, LEARNED: 6}


def word(rng: random.Random) -> int:
    return s32(
        rng.choice(
            (
                0,
                1,
                0x7F,
                0x80,
                0xFF,
                0x100,
                0x7FFF,
                0x8000,
                0xFFFF,
                -1,
                rng.getrandbits(32),
                rng.getrandbits(32),
            )
        )
    )


def target(rng: random.Random) -> dict:
    position = rng.random() < 0.3
    return {
        "bytes": rng.randbytes(TARGET_STRIDE),
        "impact": rng.choice((0, 1, 2, 3, 4, 5, 6, 7, 0xFF, rng.randrange(256))),
        "limb": rng.choice((0, 1, 5, 0x7F, 0x80, 0xFF, rng.randrange(256))),
        "position": rng.choice((1, 2, 0x80000000, 0xFFFFFFFF, rng.getrandbits(32) | 1))
        if position
        else 0,
    }


def action_id(rng: random.Random) -> int:
    return rng.choice(
        (
            0,
            1,
            0x15,
            0x16,
            0x16,
            0x16,
            0x17,
            0x27,
            0x28,
            0x28,
            0x29,
            0xFFFF,
            rng.randrange(0x16),
            rng.randrange(0x16, 0x28),
            rng.randrange(0x16, 0x28),
            rng.randrange(0x28, 0x10000),
        )
    )


def vector(rng: random.Random, number: int, mode: int) -> dict:
    prerequisite = rng.getrandbits(32)
    if rng.random() < 0.35:
        prerequisite = (prerequisite & ~(0x3F << 7)) | 3 << 7
    count = 1 if mode == HIT else rng.choice((0, 1, 1, 2, 3, 4, 6))
    return {
        "mode": mode,
        "queue_index": rng.getrandbits(32),
        "entry": rng.randbytes(TARGETS),
        "action": action_id(rng),
        "variant": rng.choice((0, 1, 0x7F, 0x80, 0xFF, rng.randrange(256))),
        "prerequisite": prerequisite,
        "targets": [target(rng) for _ in range(count)],
        "bone_tags": [word(rng) for _ in range(count)],
        "effect": word(rng),
        "limb": word(rng),
        "attacker": word(rng),
        "ground": [
            rng.choice((0, 0x7FFF, 0x8000, 0xFFFF, rng.randrange(1 << 16)))
            for _ in range(3)
        ],
        "tile": [
            rng.choice((0, 1, 0x7F, 0x80, 0xFF, rng.randrange(256))) for _ in range(3)
        ],
        "height": rng.choice((0, 0x7FFF, 0x8000, 0xFFFF, rng.randrange(1 << 16))),
        "busy": rng.choice((0, 2, 1, 3, 0xFFFFFFFD, 0xFFFFFFFF, rng.getrandbits(32))),
        "stack": rng.randbytes(STACK_FILL),
        "number": number,
    }


def target_bytes(value: dict) -> bytes:
    """A queued or passed target: random bytes with the vector's limb, impact kind and position flag."""
    result = bytearray(value["bytes"])
    result[1] = value["limb"]
    result[3] = value["impact"]
    result[TARGETS_POSITION : TARGETS_POSITION + 4] = value["position"].to_bytes(
        4, "little"
    )
    return bytes(result)


def target_inputs(value: dict) -> list[int]:
    raw = target_bytes(value)
    return [
        raw[0],
        raw[1] - 0x100 if raw[1] >= 0x80 else raw[1],
        raw[2],
        raw[3],
        int.from_bytes(raw[4:6], "little", signed=True),
        int.from_bytes(raw[6:8], "little", signed=True),
        int(value["position"] != 0),
    ]


def request_fields(machine: Machine, address: int, mode: int) -> list[int]:
    """The fields of the request at `address` the producer of `mode` defines."""

    def u8(offset: int) -> int:
        return machine.load(address + offset, 1)

    def u16(offset: int) -> int:
        return machine.load(address + offset, 2)

    def i16(offset: int) -> int:
        return machine.load(address + offset, 2, signed=True)

    fields = [u16(0), u8(2), u8(3), u16(4)]
    if mode in (HIT, IMPACTS):
        fields += [u8(8), u8(0xA), u16(0x10), u16(0x12)]
        if u16(0x10) == ACTOR_ENDPOINT:
            return fields + [u8(0x14), u8(0x15), u8(0x16)]
        return fields + [i16(0x14), i16(0x16), i16(0x18), i16(0x1A)]
    if mode in (GROUND, TRAP):
        return fields + [u8(8), u16(0x10), i16(0x14), i16(0x16), i16(0x18)]
    return fields + [u16(0x10), u8(0x14), u8(0x16)]


def prepare_action(machine: Machine, value: dict) -> None:
    machine.map_zero(STATE, STATE_BYTES)
    machine.put(ACTION_STATE_POINTER, STATE)
    machine.put(STATE, value["queue_index"])
    entry = bytearray(value["entry"])
    entry[0:2] = value["action"].to_bytes(2, "little")
    entry[2] = value["variant"]
    entry[0x4A:0x4C] = len(value["targets"]).to_bytes(2, "little")
    queued = STATE + QUEUE + (value["queue_index"] & 3) * QUEUE_STRIDE
    machine.store(queued, bytes(entry))
    for index, item in enumerate(value["targets"]):
        machine.store(queued + TARGETS + index * TARGET_STRIDE, target_bytes(item))
    machine.map_zero(DEFINITIONS, DEFINED_ACTIONS * DEFINITION_STRIDE)
    if value["action"] < DEFINED_ACTIONS:
        machine.put(
            DEFINITIONS + value["action"] * DEFINITION_STRIDE + PREREQUISITE_WORD,
            value["prerequisite"],
        )


def action_inputs(value: dict) -> list[int]:
    return [value["action"], value["variant"], value["entry"][4], value["prerequisite"]]


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    mode = value["mode"]
    calls: list[list[int]] = []
    machine.store(DEFAULT_STACK - STACK_FILL, value["stack"])
    machine.put(BUSY_FLAGS, value["busy"])
    bone_tags = iter(value["bone_tags"])

    def begin(m: Machine) -> None:
        calls.append([BEGIN_CALL, s32(m.r[5]), *request_fields(m, m.r[4], mode)])

    def bone(m: Machine) -> None:
        calls.append([BONE_CALL, s32(m.r[4]), s32(m.r[5]), s32(m.r[6])])
        m.r[2] = next(bone_tags, 0) & 0xFFFFFFFF

    machine.stub(BEGIN, begin)
    machine.stub(BONE_TAG, bone)
    machine.stub(SOUNDS, lambda m: calls.append([SOUND_CALL, s32(m.r[4])]))
    values: list[int] = []
    inputs: list[int] = [mode]
    if mode in (HIT, IMPACTS):
        prepare_action(machine, value)
        inputs += action_inputs(value)
    if mode == HIT:
        machine.store(INPUT, target_bytes(value["targets"][0]))
        machine.r[4], machine.r[5], machine.r[6] = (
            value["effect"] & 0xFFFFFFFF,
            INPUT,
            value["limb"] & 0xFFFFFFFF,
        )
        inputs += [
            *value["bone_tags"],
            value["effect"],
            value["limb"],
            *target_inputs(value["targets"][0]),
        ]
    elif mode == IMPACTS:
        machine.follow.add(HIT_EFFECT)
        machine.r[4] = value["attacker"] & 0xFFFFFFFF
        inputs += [value["attacker"], len(value["targets"]), *value["bone_tags"]]
        for item in value["targets"]:
            inputs += target_inputs(item)
    elif mode == GROUND:
        for index, component in enumerate(value["ground"]):
            machine.put(INPUT + index * 2, component, 2)
        machine.r[4] = INPUT
        inputs += [value["ground"][0], value["ground"][2]]
    elif mode == TRAP:
        machine.store(
            INPUT, bytes([value["tile"][0], value["tile"][1], value["tile"][2], 0])
        )
        machine.put(INPUT + 4, value["ground"][0], 2)
        machine.put(INPUT + 6, value["height"], 2)
        machine.r[4] = INPUT
        inputs += [
            value["tile"][0],
            value["tile"][2],
            s16(value["height"]),
            value["busy"],
        ]
    machine.execute(entry)
    if mode == IMPACTS:
        values.append(s32(machine.r[2]))
    names = ["return"] if values else []
    return recorded_observation(machine, values, names, calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    return [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]


def describe(image, results) -> list[str]:
    if not results:
        return []
    begins = sounds = positions = actors = 0
    for _, observation, _ in results:
        for call in observation["calls"]:
            if call[0] == BEGIN_CALL:
                begins += 1
                kind = call[KIND_INDEX[results[0][0]["mode"]]]
                actors += kind == ACTOR_ENDPOINT
                positions += kind != ACTOR_ENDPOINT
            sounds += call[0] == SOUND_CALL
    return [
        f"{begins} requests begun ({actors} at actors, {positions} at positions), {sounds} weapon hit sound calls "
        f"over {len(results)} vectors",
        "Compared each request's defined fields, the call order and arguments and stores outside the stack.",
    ]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
