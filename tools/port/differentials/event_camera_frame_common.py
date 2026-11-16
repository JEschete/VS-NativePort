"""Shared harness for the event camera frame differentials (BATTLE.PRG 4A0A8.c, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Event core
(EventCameraFrame.cpp) in event_camera_frame_driver.cpp. The event camera (D_800F4BA4) points at
a record filled with a pattern, then with random values in every field the four originals read:
the track modes and positions, the partner positions, the origin, the shake count and offsets,
the roll and lens settings and every completion duration. The field camera override (D_800F4BA0),
the script backdrop flag (D_800F4C6A) and the scratchpad words 0x1F800088-0x1F800118 start random.

Callees are stubbed and recorded: the field camera's spherical initialisation (writes salted
vectors), the look-at, position and roll setters, the screen rotation and the projection, near
and far clip setters, _atan2FixedPoint (returns a salted word), RotMatrix (writes a salted
matrix) and ApplyMatrixLV (writes a salted product of its matrix and vector), and the track,
angle tween, lens tween and shake updates. _copyVector, _vectorAdd, _sVectorToFixedPointVector
and, for func_800BB41C, func_800BD610 run as the original.

Each run prints its result, the track positions, the partner positions, the five changed flags
and the calls. Stores are allowed only to the stack, the scratchpad words the original used, the
track and partner positions and the changed flags.

Outside the compared domain (covered by EventCameraFrameTests.cpp): the live entries skip a
missing camera or instruction instead of reading through them.
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
DRIVER = "event_camera_frame_driver.cpp"
SOURCES = (r"Native\Game\src\Event\EventCameraFrame.cpp",)

POINTER = 0x800F4BA4
FIELD_OVERRIDE = 0x800F4BA0
BACKDROP = 0x800F4C6A
CAMERA = 0x80400000
CAMERA_BYTES = 0x28C
SCRATCH = 0x1F800000
SCRATCH_BYTES = 0x120
SCRIPT = CAMERA + 0x1000

WAIT, APPLY, UPDATE, MASK = range(4)

COMPLETION_MASK = 0x800BD610
COPY_VECTOR, ADD_VECTORS, WIDEN_VECTOR = 0x800BE71C, 0x800BE628, 0x800BE824
INIT_FIELD, SET_LOOK_AT, SET_POSITION, SET_ROLL = 0x8007CD70, 0x8007AC6C, 0x8007AC18, 0x8007AC94
SET_SCREEN, SET_PROJECTION, SET_NEAR, SET_FAR = 0x8007DDAC, 0x8007CCF0, 0x8007CCCC, 0x8007CD14
ATAN, ROT_MATRIX, APPLY_MATRIX = 0x800BE878, 0x80041200, 0x800415C8
TRACK, ANGLE_TWEEN, LENS_TWEEN, SHAKE = 0x800BCA8C, 0x800BD57C, 0x800BDF6C, 0x800BE180
(
    CALL_INIT,
    CALL_LOOK_AT,
    CALL_POSITION,
    CALL_ROLL,
    CALL_SCREEN,
    CALL_PROJECTION,
    CALL_NEAR,
    CALL_FAR,
    CALL_ATAN,
    CALL_ROTATION,
    CALL_APPLY,
    CALL_TRACK,
    CALL_ANGLE,
    CALL_LENS,
    CALL_SHAKE,
) = range(1, 16)

LOOK_AT_CURRENT, EYE_CURRENT = 0x94, 0x148
LOOK_AT_PARTNER, EYE_PARTNER = 0xA4, 0x158
ORIGIN_ACTOR, ORIGIN_OFFSET = 0x1E8, 0x1EC
SHAKE_COUNT, LOOK_AT_SHAKE, EYE_SHAKE = 0x1A4, 0x1BE, 0x1C6
SETTINGS = (0x168, 0x174, 0x180, 0x18C, 0x198)
# (offset, size, signed) of each completion field: the track modes, the roll, lens and warp
# durations and the two shake bytes.
PROGRESS = (
    (0x000, 1, False),
    (0x0B4, 1, False),
    (0x172, 2, True),
    (0x189, 1, False),
    (0x195, 1, False),
    (0x1A1, 1, False),
    (0x27A, 2, True),
    (0x1AF, 1, False),
    (0x1BB, 1, False),
)
SCRATCH_SPANS = ((0x98, 0xB8), (0xD8, 0xE0), (0xF8, 0x118))

NAMES = (
    "result",
    *(f"{track} {axis}" for track in ("look-at", "eye") for axis in "xyz"),
    *(f"{track} partner {axis}" for track in ("look-at", "eye") for axis in "xyz"),
    *(f"{name} changed" for name in ("roll", "screen", "projection", "near", "far")),
)


def half(rng: random.Random) -> int:
    return rng.choice((0, 1, -1, 0x7FFF, -0x8000, 0x800, rng.randrange(-0x8000, 0x8000)))


def word(rng: random.Random) -> int:
    return rng.choice((0, 0x1000, -1, 0x7FFFFFFF, -0x80000000, s32(rng.getrandbits(32))))


def triple(rng: random.Random, item) -> list[int]:
    return [item(rng) for _ in range(3)]


def progress(rng: random.Random) -> list[int]:
    def mode() -> int:
        return rng.choice((0xF, 0xF, 0, 0xB, 0x8F, rng.randrange(256)))

    def small() -> int:
        return rng.choice((0, 0, 0, 1, 0x80, 0xFF, rng.randrange(256)))

    def wide() -> int:
        return rng.choice((0, 0, 0, 1, -1, 0x100, half(rng)))

    shake = rng.choice(([0, 0], [0, 0], [0xFF, 1], [0x80, 0x80], [small(), small()]))
    return [mode(), mode(), wide(), small(), small(), small(), wide(), *shake]


def vector(rng: random.Random, number: int, mode: int) -> dict:
    del number
    return {
        "mode": mode,
        "override": rng.choice((0, 0, 1, 0x80, rng.randrange(256))),
        "backdrop": rng.choice((0, 1, 0xFF, rng.randrange(256))),
        "thread": half(rng),
        "operand": rng.choice((0, 0xFF, 0x80, 1 << rng.randrange(8), rng.randrange(256))),
        "progress": progress(rng),
        "look_at": triple(rng, word),
        "eye": triple(rng, word),
        "partners": [word(rng) for _ in range(6)],
        "origin_actor": rng.choice((0x2000, 0x2000, 0, 0x1FFF, 0xA000, rng.randrange(0x10000))),
        "origin_offset": triple(rng, word),
        "shake_count": rng.choice((0, 0, 1, -1, 0x100, half(rng))),
        "look_at_shake": triple(rng, half),
        "eye_shake": triple(rng, half),
        "settings": [
            [rng.choice((0, 0, 1, 0x80, rng.randrange(256))), half(rng)] for _ in SETTINGS
        ],
        "field": [word(rng) for _ in range(6)],
        "atan": s32(rng.getrandbits(32)),
        "matrix": [half(rng) for _ in range(9)],
        "salt": triple(rng, word),
        "scratch": [rng.getrandbits(32) for _ in range(SCRATCH_BYTES // 4)],
    }


def product(matrix: list[int], vector_words: list[int], salt: list[int]) -> list[int]:
    """The salted product ApplyMatrixLV's stub writes and the driver's port returns."""
    return [
        s32((sum(matrix[row * 3 + column] * vector_words[column] for column in range(3)) >> 12)
            + salt[row])
        for row in range(3)
    ]


def stubs(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    def words(address: int) -> list[int]:
        return [machine.load(address + 4 * index, 4, signed=True) for index in range(3)]

    def initialise(m: Machine) -> None:
        calls.append([CALL_INIT, s32(m.r[6]), s32(m.r[7])])
        look_at, eye = value["field"][:3], value["field"][3:]
        for index in range(3):
            m.write(m.r[4] + 4 * index, eye[index] & 0xFFFFFFFF, 4)
            m.write(m.r[5] + 4 * index, look_at[index] & 0xFFFFFFFF, 4)

    def single(kind: int):
        return lambda m: calls.append([kind, s32(m.r[4])])

    def atan(m: Machine) -> None:
        calls.append([CALL_ATAN, s32(m.r[4]), s32(m.r[5])])
        m.r[2] = value["atan"] & 0xFFFFFFFF

    def rotation(m: Machine) -> None:
        angles = [m.load(m.r[4] + 2 * index, 2, signed=True) for index in range(3)]
        calls.append([CALL_ROTATION, m.r[4] - SCRATCH, m.r[5] - SCRATCH, *angles])
        for index, element in enumerate(value["matrix"]):
            m.write(m.r[5] + 2 * index, element & 0xFFFF, 2)

    def apply(m: Machine) -> None:
        matrix = [m.load(m.r[4] + 2 * index, 2, signed=True) for index in range(9)]
        source = words(m.r[5])
        calls.append(
            [CALL_APPLY, m.r[4] - SCRATCH, m.r[5] - SCRATCH, m.r[6] - SCRATCH, *matrix, *source]
        )
        for index, item in enumerate(product(matrix, source, value["salt"])):
            m.write(m.r[6] + 4 * index, item & 0xFFFFFFFF, 4)

    def offsets(kind: int, count: int):
        return lambda m: calls.append(
            [kind, *(s32(m.r[4 + index] - CAMERA) for index in range(count))]
        )

    machine.stub(INIT_FIELD, initialise)
    machine.stub(SET_LOOK_AT, lambda m: calls.append([CALL_LOOK_AT, *words(m.r[4])]))
    machine.stub(SET_POSITION, lambda m: calls.append([CALL_POSITION, *words(m.r[4])]))
    machine.stub(SET_ROLL, single(CALL_ROLL))
    machine.stub(SET_SCREEN, single(CALL_SCREEN))
    machine.stub(SET_PROJECTION, single(CALL_PROJECTION))
    machine.stub(SET_NEAR, single(CALL_NEAR))
    machine.stub(SET_FAR, lambda m: calls.append([CALL_FAR, s32(m.r[4]), s32(m.r[5])]))
    machine.stub(ATAN, atan)
    machine.stub(ROT_MATRIX, rotation)
    machine.stub(APPLY_MATRIX, apply)
    machine.stub(TRACK, offsets(CALL_TRACK, 2))
    machine.stub(ANGLE_TWEEN, offsets(CALL_ANGLE, 1))
    machine.stub(LENS_TWEEN, offsets(CALL_LENS, 1))
    machine.stub(SHAKE, lambda m: calls.append([CALL_SHAKE]))
    machine.follow.update((COMPLETION_MASK, COPY_VECTOR, ADD_VECTORS, WIDEN_VECTOR))


def put_words(machine: Machine, address: int, items: list[int], size: int = 4) -> None:
    for index, item in enumerate(items):
        machine.put(address + index * size, item, size)


def load_words(machine: Machine, address: int) -> list[int]:
    return [machine.load(address + 4 * index, 4, signed=True) for index in range(3)]


def prepare(machine: Machine, value: dict) -> None:
    machine.map_zero(CAMERA, CAMERA_BYTES)
    for index in range(CAMERA_BYTES // 4):
        machine.put(CAMERA + index * 4, 0xA5A5A5A5 ^ (index * 0x01010101))
    for (offset, size, _), item in zip(PROGRESS, value["progress"], strict=True):
        machine.put(CAMERA + offset, item, size)
    put_words(machine, CAMERA + LOOK_AT_CURRENT, value["look_at"])
    put_words(machine, CAMERA + EYE_CURRENT, value["eye"])
    put_words(machine, CAMERA + LOOK_AT_PARTNER, value["partners"][:3])
    put_words(machine, CAMERA + EYE_PARTNER, value["partners"][3:])
    machine.put(CAMERA + ORIGIN_ACTOR, value["origin_actor"], 2)
    put_words(machine, CAMERA + ORIGIN_OFFSET, value["origin_offset"])
    machine.put(CAMERA + SHAKE_COUNT, value["shake_count"], 2)
    put_words(machine, CAMERA + LOOK_AT_SHAKE, value["look_at_shake"], 2)
    put_words(machine, CAMERA + EYE_SHAKE, value["eye_shake"], 2)
    for offset, (changed, setting) in zip(SETTINGS, value["settings"], strict=True):
        machine.put(CAMERA + offset, changed, 1)
        machine.put(CAMERA + offset + 2, setting, 2)
    for address, item, size in (
        (POINTER, CAMERA, 4),
        (FIELD_OVERRIDE, value["override"], 1),
        (BACKDROP, value["backdrop"], 1),
    ):
        machine.map_zero(address, size)
        machine.put(address, item, size)
    machine.map_zero(SCRATCH, SCRATCH_BYTES)
    put_words(machine, SCRATCH, value["scratch"])
    machine.map_zero(SCRIPT, 4)
    machine.store(SCRIPT, bytes((0xE0, value["operand"])))


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    calls: list[list[int]] = []
    stubs(machine, value, calls)
    prepare(machine, value)
    machine.r[4] = SCRIPT
    machine.r[5] = value["thread"] & 0xFFFFFFFF
    machine.execute(entry)

    mode = value["mode"]
    result = s32(machine.r[2]) if mode in (WAIT, MASK) else 0
    values = [
        result,
        *load_words(machine, CAMERA + LOOK_AT_CURRENT),
        *load_words(machine, CAMERA + EYE_CURRENT),
        *load_words(machine, CAMERA + LOOK_AT_PARTNER),
        *load_words(machine, CAMERA + EYE_PARTNER),
        *(machine.load(CAMERA + offset, 1) for offset in SETTINGS),
    ]
    inputs = [
        value["override"],
        value["backdrop"],
        value["operand"],
        *value["progress"],
        *value["look_at"],
        *value["eye"],
        *value["partners"],
        value["origin_actor"],
        *value["origin_offset"],
        value["shake_count"],
        *value["look_at_shake"],
        *value["eye_shake"],
        *(item for setting in value["settings"] for item in setting),
        *value["field"],
        value["atan"],
        *value["matrix"],
        *value["salt"],
    ]
    return recorded_observation(machine, values, list(NAMES), calls), [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    del value
    return [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        *((SCRATCH + low, SCRATCH + high) for low, high in SCRATCH_SPANS),
        *(
            (CAMERA + offset, CAMERA + offset + 12)
            for offset in (LOOK_AT_CURRENT, EYE_CURRENT, LOOK_AT_PARTNER, EYE_PARTNER)
        ),
        *((CAMERA + offset, CAMERA + offset + 1) for offset in SETTINGS),
    ]


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    del image, reference
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    del image
    shaken = sum(
        1
        for _, observation, _ in results
        if any(call[0] == CALL_APPLY for call in observation["calls"])
    )
    field = sum(
        1 for _, observation, _ in results if any(call[0] == CALL_INIT for call in observation["calls"])
    )
    busy = sum(1 for _, observation, _ in results if observation["values"][0] not in (0, 0xFF))
    return [f"{shaken} vectors shook, {field} used the field camera, {busy} returned a partial result"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
