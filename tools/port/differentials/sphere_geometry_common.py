"""Shared R3000 vectors and native driver interface for A32-05."""

from __future__ import annotations

import random
import struct
from functools import partial

from differential import DEFAULT_STACK, R, s16, s32, store_problems

SPHERE = 0x80500000
SCRATCH = 0x1F800000
WIRE = SPHERE + 0x48
SCALE = SPHERE + 0xA00
SPHERE_POINTER = 0x800EB9B8
ROT_MATRIX = 0x80041200
APPLY_MATRIX_SV = 0x80041724
COSINE = 0x80040F24
SINE = 0x80040F28
ROT_MATRIX_YXZ = 0x80040F8C
APPEND_ARC = 0x800C0758
APPEND_CIRCLE = 0x800C2254
APPEND_CONE = 0x800C2368

# Table spans stop at the next data symbol, not the over-wide C declarations.
ARC_BASES = (0x800EA234, 72)
MIRRORED_VERTICES = (0x800EA2C4, 186)
SHAPE_STARTS = (0x800EA438, 52)
COMMANDS = (0x800EA46C, 218)

SOURCES = (
    r"Native\Game\src\Battle\SphereGeometry.cpp",
    r"Native\Game\src\Battle\SphereGeometryLive.cpp",
    r"Native\Engine\src\Core\Log.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
)

_SENTINEL = -1234
_VERTEX_COUNT = 297
_OUTPUT_COMPONENTS = 4
_SINE_COUNT = 32
_STACK_SPAN = (DEFAULT_STACK - 0x4000, DEFAULT_STACK + 32)


def _signed16_values(rng: random.Random, count: int, choices) -> tuple[int, ...]:
    return tuple(s16(rng.choice(choices)) for _ in range(count))


def _base_vector(rng: random.Random, number: int, operation: int) -> dict:
    value = {
        "operation": operation,
        "phase": rng.randrange(-128, 128),
        "segments": rng.choice((0, 1, 2, 7, 28, 30, 31, 32)),
        "index": 0,
        "angle": rng.choice((0, 1, 512, 1024, 2048, 32767, -32768)),
        "step": rng.randrange(1024),
        "radius": rng.randrange(9),
        "type": rng.randrange(5),
        "state": _signed16_values(rng, 4, (0, 1, 2, 31, 255, 32767, -32768)),
        "extents": tuple(rng.choice((0, 1, 2, 8, 64, 128, 255)) for _ in range(4)),
        "basis": _signed16_values(rng, 12, (0, 1, -1, 256, -256, 4096, -4096)),
        "sine": _signed16_values(rng, _SINE_COUNT, (0, 1, -1, 1024, -1024, 4096, -4096)),
        "arc_bases": (0,) * 72,
        "mirrored": (0,) * 186,
        "starts": (0,) * 52,
        "commands": (0,) * 218,
    }

    if operation == 0:
        value["index"] = rng.randrange(0, _VERTEX_COUNT - value["segments"])
    elif operation == 2:
        value["index"] = rng.randrange(0, _VERTEX_COUNT - 32)
    elif operation == 3:
        count = 4 * (value["radius"] + 1) + (33 if value["radius"] == 0 else 66)
        value["index"] = rng.randrange(0, _VERTEX_COUNT - count + 1)
    elif operation == 4:
        value["type"] = number % 5
        value["state"] = (
            rng.randrange(-32768, 32768),
            rng.randrange(-32768, 32768),
            rng.randrange(-32768, 32768),
            rng.randrange(0, 4),
        )
    return value


def _pack_tables(image) -> tuple[tuple[int, ...], ...]:
    arc_address, arc_count = ARC_BASES
    mirror_address, mirror_count = MIRRORED_VERTICES
    starts_address, starts_count = SHAPE_STARTS
    commands_address, command_count = COMMANDS
    arc = struct.unpack("<72h", image.bytes(arc_address, arc_count * 2))
    mirrored = struct.unpack(
        "<186h", image.bytes(mirror_address, mirror_count * 2)
    )
    starts = tuple(image.byte(starts_address + index) for index in range(starts_count))
    commands = struct.unpack(
        "<218H", image.bytes(commands_address, command_count * 2)
    )
    return arc, mirrored, starts, commands


def _encode(value: dict) -> list[int]:
    return [
        value["operation"],
        value["phase"],
        value["segments"],
        value["index"],
        value["angle"],
        value["step"],
        value["radius"],
        value["type"],
        *value["state"],
        *value["extents"],
        *value["basis"],
        *value["sine"],
        *value["arc_bases"],
        *value["mirrored"],
        *value["starts"],
        *value["commands"],
    ]


def _signed_shorts(data: bytes) -> list[int]:
    return list(struct.unpack(f"<{len(data) // 2}h", data))


def _initial_vertex_bytes() -> bytes:
    return struct.pack("<4h", *([_SENTINEL] * _OUTPUT_COMPONENTS)) * _VERTEX_COUNT


def _vertices(machine) -> list[int]:
    return _signed_shorts(machine.bytes(WIRE, _VERTEX_COUNT * 8))


def run(operation: int, machine, value: dict):
    mirrored_before = None
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(SPHERE, 0x1000)
    machine.put(SPHERE_POINTER, SPHERE)
    machine.store(WIRE, _initial_vertex_bytes())
    machine.store(
        SCRATCH + 0x3B0,
        struct.pack("<32h", *value["sine"]),
    )
    if operation in (0, 2):
        machine.store(
            SCRATCH + 0x398,
            struct.pack(f"<{len(value['basis'])}h", *value["basis"]),
        )
    if operation == 1:
        machine.store(SCALE, bytes(value["extents"]))
        machine.r[R["a0"]] = SCALE
        machine.r[R["a1"]] = value["angle"] & 0xFFFFFFFF
    elif operation == 0:
        machine.r[R["a0"]] = value["phase"] & 0xFFFFFFFF
        machine.r[R["a1"]] = value["segments"]
        machine.r[R["a2"]] = value["index"]
    elif operation == 2:
        machine.r[R["a0"]] = value["angle"] & 0xFFFFFFFF
        machine.r[R["a1"]] = value["index"]
    elif operation == 3:
        machine.r[R["a0"]] = value["step"]
        machine.r[R["a1"]] = value["radius"]
        machine.r[R["a2"]] = value["index"]
    else:
        machine.r[R["a0"]] = value["type"]
        machine.store(SPHERE + 0x24, struct.pack("<4h", *value["state"]))
        mirrored_before = machine.bytes(
            MIRRORED_VERTICES[0], MIRRORED_VERTICES[1] * 2
        )
        arc, mirrored, starts, commands = _pack_tables(machine)
        value = {
            **value,
            "arc_bases": arc,
            "mirrored": mirrored,
            "starts": starts,
            "commands": commands,
        }

    machine.follow.update(
        (
            ROT_MATRIX,
            APPLY_MATRIX_SV,
            COSINE,
            SINE,
            ROT_MATRIX_YXZ,
            APPEND_ARC,
            APPEND_CIRCLE,
            APPEND_CONE,
        )
    )
    machine.execute(
        {
            0: 0x800C0758,
            1: 0x800C085C,
            2: 0x800C2254,
            3: 0x800C2368,
            4: 0x800C253C,
        }[operation]
    )

    if operation in (0, 2):
        outputs = [1, s32(machine.r[R["v0"]]), *_vertices(machine)]
        allowed = [_STACK_SPAN, (WIRE, WIRE + _VERTEX_COUNT * 8)]
    elif operation == 1:
        outputs = [1, *_signed_shorts(machine.bytes(SCRATCH + 0x350, 18))]
        allowed = [
            _STACK_SPAN,
            (SCRATCH + 0x330, SCRATCH + 0x350),
            (SCRATCH + 0x350, SCRATCH + 0x394),
        ]
    elif operation == 3:
        outputs = [
            1,
            s32(machine.r[R["v0"]]),
            *_signed_shorts(machine.bytes(SCRATCH + 0x398, 18)),
            *_vertices(machine),
        ]
        allowed = [
            _STACK_SPAN,
            (SCRATCH + 0x398, SCRATCH + 0x3AA),
            (WIRE, WIRE + _VERTEX_COUNT * 8),
        ]
    else:
        outputs = [
            1,
            *(
                machine.load(SPHERE + offset, 2, True)
                for offset in (0x24, 0x26, 0x28, 0x2A, 0x32)
            ),
            *_vertices(machine),
        ]
        allowed = [
            _STACK_SPAN,
            (SPHERE + 0x24, SPHERE + 0x2C),
            (SPHERE + 0x32, SPHERE + 0x34),
            (WIRE, WIRE + _VERTEX_COUNT * 8),
            (SCRATCH + 0x398, SCRATCH + 0x3B8),
            (
                MIRRORED_VERTICES[0],
                MIRRORED_VERTICES[0] + MIRRORED_VERTICES[1] * 2,
            ),
        ]

    driver_input = _encode(value)
    observation = {
        "outputs": outputs,
        "stores": list(machine.stores),
        "allowed": allowed,
        "stubs": len(machine.stubs),
        "mirrored_unchanged": (
            mirrored_before is None
            or machine.bytes(MIRRORED_VERTICES[0], MIRRORED_VERTICES[1] * 2)
            == mirrored_before
        ),
    }
    return observation, driver_input


def check(_image, _value, observation, native, _reference):
    expected = observation["outputs"]
    problems = []
    if len(native) != len(expected):
        problems.append(f"output width native {len(native)} original {len(expected)}")
    else:
        differing = [
            f"word {index} native {actual:#x} original {wanted:#x}"
            for index, (actual, wanted) in enumerate(zip(native, expected, strict=True))
            if actual != wanted
        ]
        if differing:
            problems.extend(differing[:8])
    problems.extend(store_problems(observation["stores"], observation["allowed"]))
    if observation["stubs"]:
        problems.append(f"original used {observation['stubs']} unexpected stubs")
    if not observation["mirrored_unchanged"]:
        problems.append("original mirror table bytes were not restored")
    return problems


def configure(
    namespace: dict,
    function: int,
    operation: int,
    entry: int,
    end: int,
    native_variant: tuple[str, str],
) -> None:
    source, replacement = native_variant
    namespace.update(
        ID=f"F{function:05}",
        ENTRY=entry,
        END=end,
        MODULES=("BATTLE/BATTLE.PRG", "SLUS_010.40"),
        DRIVER="sphere_geometry_driver.cpp",
        SOURCES=SOURCES,
        COMPILER="clang-cl",
        INCLUDES=(
            r"Native\Game\src",
            r"Native\Engine",
            r"Native\Engine\src",
            r"Native\Content",
        ),
        VECTORS=512,
        SEED=function,
        MUTANT_VECTORS=128,
        MUTANTS={f"{function}-return-path": {end - 8: 0}},
        vector=partial(_base_vector, operation=operation),
        run=partial(run, operation),
        check=check,
        describe=lambda _image, _results: [],
        NATIVE_VARIANT=(source, replacement),
    )
