"""Shared harness for the floating label producer differentials (BATTLE.PRG 2842C.c:1544-1755, #276).

Each case runs one original producer (F00714-F00718) under the R3000 interpreter against its typed VS::Battle core
(FloatingLabelProducers.cpp) over the typed floating label queue (FloatingLabelQueue.cpp) in
floating_label_producers_driver.cpp. The original's own reserve (func_800962E4) and remove (func_80096444) run with
the SLUS word copy and clear they call, and F00717 runs the original F00718 it calls. The skeleton callees
(func_800A152C, func_800A1AF8, func_800A190C and func_800A1280) and rand() are stubs that return scripted results,
write their scripted point and record their arguments in call order; sprintf("%d") and strlen are stubs over the
interpreter's memory. The queue's 32 records start as random bytes biased toward the producers' kinds, actors,
limbs and stacking orders, and the caller's stack is random, so a field the original fails to write differs from the
native one. The comparison covers the return value, the published count, all 32 records and the calls.

Native differences outside the compared domain (covered by FloatingLabelProducersTests.cpp): the original's publish
past a full queue counts 33 (and F00717's later labels count on from there, scanning a zeroed record 32), where the
typed queue keeps 32, so an original count above 32 compares as 32; the anchor scratch the original keeps on the
stack starts zeroed; and a queue the typed reserve refuses (a count outside 0-32) refuses the producer.
"""

from __future__ import annotations

import random
import struct
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Fault,
    Machine,
    mode_case,
    printed_problems,
    recorded_observation,
    s16,
    s32,
)

MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "floating_label_producers_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\FloatingLabelProducers.cpp",
    r"Native\Game\src\Battle\FloatingLabelQueue.cpp",
)
INCLUDES = (r"Native\Tests\include",)

VALUE, TIMING, CHAIN, STATUSES, STATUS = range(5)

OWNER = 0x800F1BAC
COUNT = 0x800F227E
LABELS = 0x80200000
STRIDE, CAPACITY = 24, 32
# Zeroed records past the owner that an original F00717 scans once its count passes 32.
OVERFLOW_RECORDS = 32
RESERVE, REMOVE, STATUS_ENTRY = 0x800962E4, 0x80096444, 0x8009406C
COPY, ZERO = 0x800490B0, 0x80049110
FIND_BONE_TAG, BONE_POSITION, TAGGED_POSITION, PROJECT = (
    0x800A152C,
    0x800A1AF8,
    0x800A190C,
    0x800A1280,
)
SPRINTF, STRLEN, RAND = 0x80026EF4, 0x80026E94, 0x80026EC4
DECIMAL_FORMAT = 0x800691E4
FIND_CALL, BONE_CALL, TAGGED_CALL, PROJECT_CALL, RANDOM_CALL = range(1, 6)
STACK_FILL = 0x200
# One scripted result per call: bone positions, tagged positions, projections.
BONE_RESULTS, TAGGED_RESULTS, PROJECTIONS = 1, 2, 29
ANCHOR_TAG = 0xFB


def coordinate(rng: random.Random, axis: int) -> int:
    edge = (320, 321, 240, 241)[axis * 2 : axis * 2 + 2] if axis < 2 else (0, 1)
    return s16(
        rng.choice(
            (
                0,
                1,
                -1,
                *edge,
                *edge,
                0x7FFF,
                -0x8000,
                rng.randrange(0, 241),
                rng.randrange(0, 241),
                rng.randrange(0, 330),
                rng.getrandbits(16),
            )
        )
    )


def point_result(rng: random.Random) -> list[int]:
    status = rng.choice((0,) * 12 + (1, -1, 2))
    return [status, *(coordinate(rng, axis) for axis in range(3)), s16(rng.getrandbits(16))]


def record(rng: random.Random, actor: int, limb: int, kinds: tuple[int, ...]) -> bytes:
    raw = bytearray(rng.randbytes(STRIDE))
    raw[8] = rng.choice((*kinds, *kinds, 0, 3, rng.randrange(256)))
    raw[9] = rng.choice((actor & 0xFF, actor & 0xFF, actor & 0xFF, 0, 0, (actor ^ 1) & 0xFF, rng.randrange(256)))
    raw[10] = rng.choice((limb & 0xFF, limb & 0xFF, 0xFF, 0xFB, rng.randrange(256)))
    raw[15] = rng.choice((0, 3, 12, 24, 25, 36, rng.randrange(256)))
    struct.pack_into("<h", raw, 6, rng.choice((-1, 0, 0, 1, 1, 2, 3, 4, 5, -0x8000, s16(rng.getrandbits(16)))))
    return bytes(raw)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    actor = rng.choice((0, 1, 2, 3, 5, 0xFF, 0x103, -1))
    limb_tag = rng.choice((-1, -1, 0, 1, 6, 0xFB, 0xFF, rng.randrange(256)))
    found = rng.choice((0, 1, 4, 6, 6, 0xFF, -1, rng.randrange(256)))
    limb = 0xFF if limb_tag == -1 else found
    kinds = {VALUE: (1,), TIMING: (4,), CHAIN: (5,), STATUSES: (2,), STATUS: (2,)}[mode]
    count = rng.choice((0, 1, 2, 3, 5, 8, 16, 29, 30, 31, 32, 32, rng.randrange(33)))
    flags = rng.choice((0, 0x80, 0x80, 0xFF, 0x7F, 0x85, rng.getrandbits(32)))
    if mode == VALUE:
        arguments = [
            actor,
            limb_tag,
            s32(
                rng.choice(
                    (
                        0,
                        7,
                        42,
                        999,
                        999,
                        1000,
                        0x80000000 | rng.choice((5, 999, 1000, 0x7FFF)),
                        0x7FFF,
                        0x8000,
                        0xFFFF,
                        0x80000000 | rng.randrange(2000),
                        0x7FFF0000 | rng.randrange(1 << 16),
                        rng.getrandbits(32),
                    )
                )
            ),
            flags,
        ]
    elif mode == TIMING:
        arguments = [s32(rng.choice((0, 1, 2, 3, 4, 0xFFFF, 0x12345, rng.getrandbits(32)))), 0, 0, 0]
    elif mode == CHAIN:
        arguments = [
            s32(rng.choice((0, 0, 1, 2, 999, 1000, 0x8005, 0x83E7, 0xFFFF, 0x10000, 0x103E7, 0x103E8, -1, rng.getrandbits(32)))),
            0,
            0,
            0,
        ]
    elif mode == STATUSES:
        mask = rng.choice(
            (
                0,
                1,
                1 << 28,
                0x30000000,
                0x80000000,
                0xFFFFFFFF,
                (1 << rng.randrange(32)) | (1 << rng.randrange(32)),
                rng.getrandbits(32) & rng.getrandbits(32),
                rng.getrandbits(32),
            )
        )
        arguments = [actor, rng.getrandbits(32), s32(mask), flags]
    else:
        arguments = [
            actor,
            rng.choice((ANCHOR_TAG, 0, 6, 0x1FB, rng.randrange(256))),
            rng.choice((0, 1, 28, 29, 0xFF, 0x10005, rng.randrange(1 << 16))),
            flags,
        ]
    return {
        "mode": mode,
        "arguments": arguments,
        "count": count,
        "labels": b"".join(record(rng, actor, limb, kinds) for _ in range(CAPACITY)),
        "found": found,
        "bones": [point_result(rng) for _ in range(BONE_RESULTS)],
        "tagged": [point_result(rng) for _ in range(TAGGED_RESULTS)],
        "projections": [point_result(rng) for _ in range(PROJECTIONS)],
        "random": rng.getrandbits(15),
        "stack": rng.randbytes(STACK_FILL),
    }


def label_words(machine: Machine) -> list[int]:
    return list(struct.unpack(f"<{STRIDE * CAPACITY // 4}I", machine.bytes(LABELS, STRIDE * CAPACITY)))


def stack_or_label(machine: Machine, address: int) -> None:
    in_labels = address >= LABELS and address + 8 <= LABELS + STRIDE * CAPACITY
    in_stack = address >= DEFAULT_STACK - DEFAULT_STACK_SIZE and address + 8 <= DEFAULT_STACK
    if not (in_labels or in_stack):
        raise Fault(f"point output {address:#x} is outside the labels and the stack")


def stub_decimal_text(machine: Machine) -> None:
    """Stubs the SLUS sprintf("%d") into the label table and strlen."""

    def sprintf(m: Machine) -> None:
        if m.r[5] != DECIMAL_FORMAT or m.bytes(DECIMAL_FORMAT, 3) != b"%d\0":
            raise Fault("sprintf without the %d format")
        text = str(s32(m.r[6])).encode() + b"\0"
        if not (m.r[4] >= LABELS and m.r[4] + len(text) <= LABELS + STRIDE * CAPACITY):
            raise Fault(f"sprintf destination {m.r[4]:#x} is outside the labels")
        m.store(m.r[4], text)
        m.r[2] = len(text) - 1

    def strlen(m: Machine) -> None:
        length = 0
        while m.byte(m.r[4] + length):
            length += 1
        m.r[2] = length

    machine.stub(SPRINTF, sprintf)
    machine.stub(STRLEN, strlen)


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    calls: list[list[int]] = []
    machine.map_zero(LABELS, STRIDE * (CAPACITY + OVERFLOW_RECORDS))
    machine.store(LABELS, value["labels"])
    machine.put(OWNER, LABELS)
    machine.put(COUNT, value["count"], 2)
    machine.store(DEFAULT_STACK - STACK_FILL, value["stack"])
    machine.follow.update((RESERVE, REMOVE, COPY, ZERO, STATUS_ENTRY))
    scripts = {
        BONE_CALL: iter(value["bones"]),
        TAGGED_CALL: iter(value["tagged"]),
        PROJECT_CALL: iter(value["projections"]),
    }

    def find(m: Machine) -> None:
        calls.append([FIND_CALL, s32(m.r[4]), s32(m.r[5]), s32(m.r[6])])
        m.r[2] = value["found"] & 0xFFFFFFFF

    def point(kind: int):
        def handler(m: Machine) -> None:
            calls.append([kind, s32(m.r[4]), s32(m.r[5]), s32(m.r[7])])
            result = next(scripts[kind], None)
            if result is None:
                m.r[2] = 0
                return
            stack_or_label(m, m.r[6])
            m.store(m.r[6], struct.pack("<4h", *result[1:]))
            m.r[2] = result[0] & 0xFFFFFFFF

        return handler

    def rand(m: Machine) -> None:
        calls.append([RANDOM_CALL, 0, 0, 0])
        m.r[2] = value["random"]

    machine.stub(FIND_BONE_TAG, find)
    machine.stub(BONE_POSITION, point(BONE_CALL))
    machine.stub(TAGGED_POSITION, point(TAGGED_CALL))
    machine.stub(PROJECT, point(PROJECT_CALL))
    stub_decimal_text(machine)
    machine.stub(RAND, rand)
    for register, argument in zip((4, 5, 6, 7), value["arguments"], strict=True):
        machine.r[register] = argument & 0xFFFFFFFF
    machine.execute(entry)
    # The typed queue publishes a full queue's reserve at 32, not the original's 33 (see the module notes).
    count = machine.load(COUNT, 2, signed=True)
    values = [s32(machine.r[2]), min(count, CAPACITY), *label_words(machine)]
    names = ["return", "count", *(f"label {index // 6} word {index % 6}" for index in range(STRIDE * CAPACITY // 4))]
    inputs = [
        *value["arguments"],
        value["count"],
        *struct.unpack(f"<{STRIDE * CAPACITY // 4}I", value["labels"]),
        value["found"],
        *(item for result in value["bones"] for item in result),
        *(item for result in value["tagged"] for item in result),
        *(item for result in value["projections"] for item in result),
        value["random"],
    ]
    return recorded_observation(machine, values, names, calls), [value["mode"], *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    return [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (LABELS, LABELS + STRIDE * CAPACITY),
        (COUNT, COUNT + 2),
    ]


def describe(_image, results) -> list[str]:
    if not results:
        return []
    published = sum(observation["values"][1] > value["count"] for value, observation, _ in results)
    full = sum(value["count"] == CAPACITY for value, _, _ in results)
    calls = sum(len(observation["calls"]) for _, observation, _ in results)
    returns = sorted({observation["values"][0] for _, observation, _ in results})
    return [
        f"{published} vectors grew the count, {full} started full; {calls} recorded callee calls; returns {returns}",
        "Compared the return, the published count (an original count past 32 as 32), all 32 records, the callee "
        "calls in order and stores outside the labels, the count and the stack.",
    ]


def check(_image, value, observation, native, _reference):
    return printed_problems(observation, native, allowed_stores(value))


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    case = mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
    case["INCLUDES"] = INCLUDES
    # Each mutant runs over every vector: several only show on a rare stacking or on-screen combination.
    case["MUTANT_VECTORS"] = case["VECTORS"]
    return case
