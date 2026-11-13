"""Shared original-instruction vectors for the six floating-label queue policies."""

from __future__ import annotations

import struct
from collections import Counter

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Fault,
    R,
    s16,
    store_problems,
)

LABELS = 0x80200000
POSITION = 0x80210000
GLOBALS = 0x800F2260
OWNER = 0x800F1BAC
STRIDE, CAPACITY, GLOBAL_BYTES = 24, 32, 64
COPY, ZERO = 0x800490B0, 0x80049110
CONFIG = {
    "reset": ("F00712", 0x80093AB4, 0x80093B04),
    "world": ("F00713", 0x80093B04, 0x80093B68),
    "advance": ("F00721", 0x80094AF8, 0x80094B0C),
    "reserve": ("F00734", 0x800962E4, 0x8009639C),
    "compact": ("F00735", 0x8009639C, 0x80096444),
    "remove": ("F00736", 0x80096444, 0x8009651C),
}
MUTATIONS = {
    "reset": {
        "marker-command-store": (0x80093ABC, 0),
        "cue-frame-store": (0x80093AC4, 0),
        "queue-count-store": (0x80093ACC, 0),
        "status-step-store": (0x80093AD4, 0),
        "suppression-store": (0x80093ADC, 0),
        "zero-entire-owner": (0x80093AF0, 0x240502E8),
        "zero-call": (0x80093AEC, 0),
    },
    "world": {
        "elapsed-frame-store": (0x80093B20, 0),
        "world-kind": (0x80093B1C, 0x24020002),
        "x-source": (0x80093B28, 0x96020002),
        "y-destination": (0x80093B3C, 0xA4830000),
        "z-source": (0x80093B44, 0x96060002),
        "publish-count": (0x80093B58, 0x24630002),
        "zero-result": (0x80093B48, 0x24020001),
    },
    "advance": {
        "delay-source": (0x80094AF8, 0x94820004),
        "elapsed-frame-store": (0x80094AFC, 0),
        "decrement-wrap": (0x80094B00, 0x24420001),
        "delay-destination": (0x80094B08, 0xA4820004),
    },
    "reserve": {
        "capacity-boundary": (0x80096300, 0x2882001F),
        "free-slot-stride": (0x8009631C, 0x00031880),
        "complete-payload-copy": (0x80096340, 0x24060014),
        "last-shift": (0x80096354, 0x2A42001E),
        "complete-tail-clear": (0x80096368, 0x24050014),
        "tail-clear-slot": (0x80096370, 0x248402D0),
        "copy-call": (0x80096348, 0),
        "clear-call": (0x8009636C, 0),
        "returned-tail-slot": (0x8009637C, 0x244202D0),
    },
    "compact": {
        "count-signed-load": (0x800963C4, 0x9462227E),
        "inactive-kind-byte": (0x800963E8, 0x90420009),
        "remove-call": (0x800963F8, 0),
        "pass-after-remove": (0x800963FC, 0x26100002),
        "scan-stride": (0x80096418, 0x24630014),
        "zero-removal-gate": (0x800963B4, 0x0640001D),
    },
    "remove": {
        "count-decrement": (0x80096468, 0x2442FFFE),
        "count-destination": (0x8009646C, 0xA462227C),
        "whole-copy": (0x800964AC, 0x24060014),
        "copy-call": (0x800964B4, 0),
        "source-stride": (0x800964BC, 0x26520014),
        "whole-tail-clear": (0x800964D4, 0x24050014),
        "tail-stride": (0x800964F0, 0x00042080),
        "clear-call": (0x800964F4, 0),
    },
}


def vector(operation, rng, number):
    labels = bytearray(rng.randbytes(STRIDE * CAPACITY))
    state = bytearray(rng.randbytes(GLOBAL_BYTES))
    count = number % 33
    argument = 0
    alias = -1
    if operation == "world":
        count = number % CAPACITY
        if number % 3:
            alias = (number // 3) % CAPACITY
    elif operation == "advance":
        argument = number % CAPACITY
        delay = (0, 1, 0x7FFF, 0x8000, 0xFFFF, rng.getrandbits(16))[number % 6]
        struct.pack_into("<H", labels, argument * STRIDE + 6, delay)
    elif operation == "reserve":
        count = (0, 1, 30, 31, 32, 33, 32767)[number % 7]
    elif operation == "compact":
        count = (0, 1, 2, 5, 12, 32, -1, -32768)[number % 8]
        argument = (-3, 0, 1, 2, 3, 4, 5)[number % 7]
        for index in range(CAPACITY):
            labels[index * STRIDE + 8] = (
                0
                if number % 5 == 0 or index % 3 == number % 3
                else (1, 2, 3, 7, 0x80, 0xFF)[(number + index) % 6]
            )
        if number == 0:
            count, argument = 32, 32
    elif operation == "remove":
        count = 1 + number % CAPACITY
        argument = (0, count // 2, count - 1, count, count + 199)[number % 5]
    if operation == "reset":
        count = s16(rng.getrandbits(16))
    struct.pack_into("<H", state, 30, count & 0xFFFF)
    return {
        "operation": list(CONFIG).index(operation),
        "argument": argument,
        "alias": alias,
        "state": bytes(state),
        "labels": bytes(labels),
        "position": [s16(rng.getrandbits(16)) for _ in range(3)],
        "count": count,
    }


def words(block):
    return list(struct.unpack(f"<{len(block) // 4}I", block))


def run(machine, value, operation):
    machine.store(OWNER, LABELS.to_bytes(4, "little"))
    machine.store(GLOBALS, value["state"])
    machine.store(LABELS, value["labels"])
    machine.store(POSITION, struct.pack("<3h", *value["position"]))
    machine.follow.update((COPY, ZERO, 0x800962E4, 0x80096444))
    argument = value["argument"]
    if operation == "world":
        argument = POSITION if value["alias"] < 0 else LABELS + value["alias"] * STRIDE
    elif operation == "advance":
        argument = LABELS + argument * STRIDE
    machine.r[R["a0"]] = argument & 0xFFFFFFFF
    machine.execute(CONFIG[operation][1])
    result = 0
    if operation == "reserve":
        offset = machine.r[R["v0"]] - LABELS
        if offset % STRIDE:
            raise Fault("Original allocator returned a non-record-aligned pointer")
        result = offset // STRIDE
    elif operation == "world":
        result = machine.r[R["v0"]]
    observation = {
        "values": [
            0,
            result,
            *words(machine.bytes(GLOBALS, GLOBAL_BYTES)),
            *words(machine.bytes(LABELS, STRIDE * CAPACITY)),
        ],
        "stores": machine.stores,
        "steps": machine.steps,
    }
    inputs = [
        value["operation"],
        value["argument"],
        value["alias"],
        *words(value["state"]),
        *words(value["labels"]),
        *value["position"],
    ]
    return observation, inputs


def check(_image, value, observation, native, _reference):
    expected = observation["values"]
    if len(native) != len(expected):
        return [f"output width {len(native)} != {len(expected)}"]
    differences = [
        f"field {index}: native {mine:#x}, original {theirs:#x}"
        for index, (mine, theirs) in enumerate(zip(native, expected, strict=True))
        if mine != theirs
    ]
    operation = list(CONFIG)[value["operation"]]
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if operation == "advance":
        label = LABELS + value["argument"] * STRIDE
        allowed.extend(((label + 6, label + 8), (label + 11, label + 12)))
    elif operation == "world":
        label = LABELS + value["count"] * STRIDE
        allowed.extend(
            (
                (label, label + 6),
                (label + 8, label + 9),
                (label + 11, label + 12),
                (GLOBALS + 30, GLOBALS + 32),
            )
        )
    else:
        allowed.append((LABELS, LABELS + STRIDE * CAPACITY))
        if operation in ("reset", "compact", "remove"):
            allowed.append((GLOBALS + 30, GLOBALS + 32))
        if operation == "reset":
            allowed.extend(
                (GLOBALS + offset, GLOBALS + offset + 2) for offset in (16, 18, 24, 26)
            )
    return differences[:8] + store_problems(observation["stores"], allowed)


def describe(_image, results):
    counts = Counter(value["count"] for value, _, _ in results)
    return [
        f"queue-count coverage: min={min(counts)}, max={max(counts)}, "
        f"empty={counts[0]}, full={counts[32]}, beyond-full={counts[33]}, "
        f"negative={sum(count for size, count in counts.items() if size < 0)}",
        f"original executed steps: {sum(item['steps'] for _, item, _ in results)}",
        "F00713 parity includes commit count 32, not the diagnosed count-33 publication defect.",
        "Real original SLUS CopyAlignedWords/ZeroAlignedWords callees executed; no callback stubs.",
    ]


def configure(namespace, operation):
    identifier, entry, end = CONFIG[operation]
    namespace.update(
        ID=identifier,
        ENTRY=entry,
        END=end,
        MODULES=("BATTLE/BATTLE.PRG", "SLUS_010.40"),
        DRIVER="floating_label_queue_driver.cpp",
        SOURCES=(r"Native\Game\src\Battle\FloatingLabelQueue.cpp",),
        INCLUDES=(r"Native\Tests\include",),
        VECTORS=1024,
        SEED=entry,
        MUTANT_VECTORS=128,
        MUTANTS={
            name: {address: word}
            for name, (address, word) in MUTATIONS[operation].items()
        },
        vector=lambda rng, number: vector(operation, rng, number),
        run=lambda machine, value: run(machine, value, operation),
        check=check,
        describe=describe,
    )
