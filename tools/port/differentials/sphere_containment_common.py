"""Original target-volume instructions, real resident SDK calls and semantic observations."""

from __future__ import annotations

import random
import struct
from functools import partial

from differential import (
    DEFAULT_STACK,
    IR,
    MAC,
    Machine,
    R,
    field_problems,
    s16,
    s32,
    store_problems,
)

SHAPE = 0x80500000
POINT = SHAPE + 0x100
WORK = SHAPE + 0x200
MATRIX = SHAPE + 0x300
ROT_MATRIX = 0x80041200
APPLY_MATRIX_SV = 0x80041724
SPANS = (
    (0x800C0FA8, 0x800C1034),
    (0x800C1034, 0x800C110C),
    (0x800C110C, 0x800C123C),
    (0x800C123C, 0x800C1384),
    (0x800C1384, 0x800C1564),
    (0x800C1564, 0x800C1664),
)
SOURCES = (
    r"Native\Game\src\Battle\SphereContainment.cpp",
    r"Native\Game\src\Battle\SphereContainmentLive.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Log.cpp",
)


def shape_bytes(extents, point=(0, 0, 0), kind=1, yaw=0):
    return struct.pack(
        "<BBH4B4H",
        kind,
        0xA7,
        yaw & 0xFFFF,
        *extents,
        *(value & 0xFFFF for value in point),
        0xBEEF,
    )


def fixed_vectors():
    yield shape_bytes((0, 1, 255, 0)), (0, 0, 0), 0, -1
    for kind in range(1, 8):
        for point in (
            (0, 0, 0),
            (255, 0, 0),
            (256, 0, 0),
            (0, -128, 0),
            (127, -64, 0),
            (128, -64, 0),
            (0, 1, 0),
            (256, 0, 256),
            (128, 0, 128),
            (0, 0, 255),
            (0, 129, 0),
            (0, -129, 0),
            (0, 32767, 0),
            (0, -32768, 0),
        ):
            yield shape_bytes((8, 4, 8, 0), kind=kind), point, 0, -1
    for extents, points in (
        ((0, 0, 0, 0), ((32767, 0, -32768), (0, 0, 0), (0, 1, 0))),
        ((1, 0, 1, 0), ((5000, 0, 5000), (8192, 0, 0), (8193, 0, 0))),
        ((1, 1, 1, 0), ((5000, 5000, 0), (31, 0, 0), (32, 0, 0))),
        ((255, 255, 255, 0), ((8160, -8160, 8160), (8161, 0, 0), (0, -8161, 0))),
        ((8, 0, 8, 0), ((256, 0, 0), (257, 0, 0))),
    ):
        for point in points:
            for mode in (0, 1, -7, -0x80000000):
                yield shape_bytes(extents), point, mode, -1
    for yaw in (0, 1, 512, 1024, 32767, 32768, 65535):
        for pitch in (0, 1, 32, 64, 127, 128, 255):
            yield (
                shape_bytes((255, 255, 255, pitch), yaw=yaw),
                (32767, 32767, 32767),
                0,
                -1,
            )
            yield (
                shape_bytes((1, 1, 1, pitch), yaw=yaw),
                (-32768, -32768, -32768),
                1,
                -1,
            )
    yield shape_bytes((255, 128, 255, 0), (0, 255, 0), kind=2), (0, 0, 0), 0, 4
    yield shape_bytes((255, 8, 255, 128), (0, 127, 0), kind=5), (0, 0, 0), 0, 4
    yield shape_bytes((8, 4, 8, 128), kind=5), (127, 64, 0), 0, -1
    for kind in (0, 8, 129, 255):
        yield shape_bytes((255, 1, 0, 255), kind=kind), (0, 0, 0), 0, -2


FIXED = tuple(fixed_vectors())


def vector(rng: random.Random, number: int) -> dict:
    if number < len(FIXED):
        shape, point, mode, alias = FIXED[number]
    else:
        extents = [
            rng.choice((0, 1, 2, 8, 16, 127, 128, 255, rng.randrange(256)))
            for _ in range(4)
        ]
        origin = tuple(s16(rng.getrandbits(16)) for _ in range(3))
        kind = rng.choice((0, 1, 2, 3, 4, 5, 6, 7, 8, 129, 255))
        yaw = rng.choice((0, 512, 1024, 32767, 32768, 65535, rng.getrandbits(16)))
        shape = shape_bytes(extents, origin, kind, yaw)
        if number % 4:
            point = tuple(
                s16(
                    value
                    + rng.choice((0, 1, -1, 31, 32, 127, 128, 255, 256, -8160, -32768))
                )
                for value in origin
            )
        else:
            point = tuple(s16(rng.getrandbits(16)) for _ in range(3))
        mode = rng.choice((0, 0, 1, -1, 0x7FFFFFFF, -0x80000000))
        alias = rng.choice((-1, -1, -1, 0, 2, 4, 6, 8, 10))
        if kind not in range(1, 8) and number % 2:
            alias = -2
    return {"shape": shape, "point": point, "mode": mode, "alias": alias}


def run(operation: int, machine: Machine, value: dict):
    shape = value["shape"]
    machine.map_zero(SHAPE, 0x400)
    machine.store(SHAPE, shape)
    machine.store(
        POINT, struct.pack("<3H", *(item & 0xFFFF for item in value["point"]))
    )
    machine.store(WORK, bytes([0xA5]) * 32)
    machine.store(MATRIX, bytes([0x5A]) * 32)
    alias = value["alias"]
    # Only the dispatcher can leave its point unused. Other entries always
    # get the same readable independent point for the lazy-dispatch vectors.
    point = (
        SHAPE + alias
        if alias >= 0
        else 0x80700000
        if alias == -2 and operation == 5
        else POINT
    )
    machine.r[R["a0"]] = SHAPE
    machine.r[R["a1"]] = WORK if operation == 0 else point
    machine.r[R["a2"]] = MATRIX if operation == 0 else value["mode"] & 0xFFFFFFFF
    machine.follow.update(entry for entry, _ in SPANS)
    machine.follow.update((ROT_MATRIX, APPLY_MATRIX_SV))
    machine.execute(SPANS[operation][0])
    outputs = [0 if operation == 0 else s32(machine.r[R["v0"]])]
    outputs.extend(machine.bytes(SHAPE, 16))
    if operation == 0:
        outputs.extend(
            machine.load(WORK + 0x10 + 2 * axis, 2, True) for axis in range(3)
        )
        outputs.extend(
            machine.load(WORK + 0x18 + 2 * axis, 2, True) for axis in range(3)
        )
        outputs.extend(machine.load(MATRIX + 2 * axis, 2, True) for axis in range(9))
    elif operation in range(1, 5):
        outputs.extend(s16(machine.gte[register]) for register in IR)
    nclip_destination = DEFAULT_STACK - (0x78 if operation == 4 else 0x98) + 0x24
    observation = {
        "outputs": outputs,
        "stores": list(machine.stores),
        "stubs": len(machine.stubs),
        "ir_saturated": operation != 0
        and any(
            s32(machine.gte[mac]) != s16(machine.gte[ir])
            for ir, mac in zip(IR, MAC, strict=True)
        ),
        "nclip_stores": sum(
            address == nclip_destination and size == 4
            for address, size in machine.stores
        ),
    }
    inputs = [
        operation,
        value["mode"],
        alias,
        *shape,
        *(item & 0xFFFF for item in value["point"]),
    ]
    return observation, inputs


def check(operation: int, _image, _value, observation, native, _reference):
    expected = observation["outputs"]
    if len(native) != len(expected):
        return [f"output width native {len(native)} original {len(expected)}"]
    names = ["return", *(f"shape-byte-{index}" for index in range(16))]
    names.extend(
        f"semantic-component-{index}" for index in range(len(expected) - len(names))
    )
    allowed = [(DEFAULT_STACK - 0x4000, DEFAULT_STACK)]
    if operation == 0:
        allowed.extend(((WORK, WORK + 32), (MATRIX, MATRIX + 18)))
    if operation == 5:
        allowed.append((SHAPE + 4, SHAPE + 8))
    problems = field_problems(names, native, expected)
    problems.extend(store_problems(observation["stores"], allowed))
    if observation["stubs"]:
        problems.append("original unexpectedly used a callee stub")
    return problems


def describe(operation, _image, results):
    values = [value for value, _, _ in results]
    observations = [observed for _, observed, _ in results]
    calls = (
        "Real SDK RotMatrix 80041200; F01290 does not call ApplyMatrixSV."
        if operation == 0
        else "Real SDK RotMatrix 80041200 and ApplyMatrixSV 80041724; no callee stubs."
    )
    observed = (
        "Native transform compares semantic origin, reciprocal extents and actual SDK rotation."
        if operation == 0
        else "Native dispatcher compares full results and all shape bytes after restoring flags."
        if operation == 5
        else "Native local-point observations compare the production typed matrix's saturated "
        "results with the original SDK IR1-IR3, not with replacement-math stubs."
    )
    return [
        calls,
        observed,
        f"input kinds={sorted({value['shape'][0] for value in values})}; "
        f"point aliases={sorted({value['alias'] for value in values})}; "
        f"IR-saturated originals={sum(item['ir_saturated'] for item in observations)}",
        f"original NCLIP stores={sum(item['nclip_stores'] for item in observations)}; "
        f"per-vector edge stores={sorted({item['nclip_stores'] for item in observations})}",
        "No parity claim for retired, unconsumed GTE status or scratch-layout fields.",
    ]


def mutants(operation: int) -> dict:
    # Metadata discovery must not require extracted images; these are the
    # unchanged audited instruction patches, not reads of the original words.
    groups = (
        {
            "reciprocal-numerator": {0x800C0FB4: 0x340A7FFF},
            "signed-extent-byte": {0x800C0FD0: 0x80420004},
            "pitch-scale": {0x800C1000: 0x000210C0},
            "yaw-not-negated": {0x800C1014: 0x00021021},
        },
        {
            "point-origin-added": {0x800C1078: 0x00621821},
            "normalization-scale": {0x800C10CC: 0x000A12C3},
            "missing-z-axis": {0x800C10E4: 0x28A20002},
            "radius-inclusive-band": {0x800C1100: 0x2C420002},
            "signed-radius-comparison": {0x800C1100: 0x28420001},
        },
        {
            "inverted-height-mode": {0x800C1188: 0x16400006},
            "full-height-shift": {0x800C1198: 0x00021140},
            "signed-height-mask": {0x800C11B0: 0x30637FFF},
            "wrong-axis-reciprocal-stride": {0x800C11FC: 0x24E70002},
            "radius-inclusive-band": {0x800C121C: 0x2E100002},
        },
        {
            "half-height-shift": {0x800C12C4: 0x00021100},
            "cone-divisor-scale": {0x800C12F0: 0x00031B03},
            "radius-one-larger": {0x800C1348: 0x24020101},
            "unsigned-cone-comparison": {0x800C135C: 0x0067182B},
        },
        {
            "half-height-gate": {0x800C1444: 0x00021100},
            "inverted-footprint-mode": {0x800C1460: 0x16600004},
            "first-corner-x-sign": {0x800C1488: 0x00071821},
            "wrong-nclip-next-vertex": {0x800C150C: 0x48836800},
            "inverted-winding-rejection": {0x800C152C: 0x04410005},
        },
        {
            "unknown-kind-returns-one": {0x800C158C: 0x24040001},
            "height-shift-two-bits": {0x800C15C8: 0x24060002},
            "reversed-pitch-off-by-one": {0x800C160C: 0x2442007F},
            "only-first-flags-byte-restored": {0x800C1648: 0xA2110004},
        },
    )
    return groups[operation]


def configure(namespace: dict, operation: int) -> None:
    entry, end = SPANS[operation]
    namespace.update(
        ID=f"F0129{operation}",
        ENTRY=entry,
        END=end,
        MODULES=("BATTLE/BATTLE.PRG", "SLUS_010.40"),
        DRIVER="sphere_containment_driver.cpp",
        SOURCES=SOURCES,
        INCLUDES=(r"Native\Game\src",),
        VECTORS=4096,
        MUTANT_VECTORS=1024,
        SEED=1290 + operation,
        MUTANTS=mutants(operation),
        vector=vector,
        run=partial(run, operation),
        check=partial(check, operation),
        describe=partial(describe, operation),
    )
