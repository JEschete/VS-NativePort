"""Shared harness for the event-script splash image differentials (BATTLE.PRG 4A0A8.c:609 and 851-914, #276).

Each case runs one original opcode handler under the R3000 interpreter against its typed
VS::Event core (ScriptSplashCommands.cpp) in event_splash_commands_driver.cpp. The original's
instruction lies at every alignment between random neighbour bytes. The image set pointer
(D_800EB9BC) points at three random 0x3F8-byte layers or is null, the tween records (D_800F4B30)
and the skip byte (D_800F4C2C) start random, and the stubs record the image callees' argument
registers. The driver's host keeps the same layer words and tween records as the original's memory.

Native differences outside the compared domain (covered by EventScriptVmTests.cpp and
EventScriptVmLiveTests.cpp): an instruction shorter than its opcode's operands is refused without
reading them; selector 3, which the original indexed past the three layers and three tween records,
reaches a host that has no such layer; and the file wait answers false without an image set, where
the original read through the null pointer.
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
DRIVER = "event_splash_commands_driver.cpp"
SOURCES = (r"Native\Game\src\Event\ScriptSplashCommands.cpp",)

SCRIPT = 0x80500000
SCRIPT_BYTES = 0x40
NEIGHBOURS = 6
GET_SHORT = 0x800BFE00

IMAGE_SET_POINTER = 0x800EB9BC
IMAGE_SET = 0x80600000
LAYER_BYTES = 0x3F8
LAYERS = 3
TWEENS = 0x800F4B30
TWEEN_BYTES = 0x14
SKIP_STATE = 0x800F4C2C

LOAD_ADDRESS = 0x800C8430
ACTIVATE_ADDRESS = 0x800C8550
RELEASE_ADDRESS = 0x800C86AC

(
    WAIT_FILE,
    LOAD,
    CONFIGURE,
    TWEEN,
    FREE,
    WAIT_TWEEN,
) = range(6)

# Each mode's opcode byte and length (the BATTLE.PRG size table at 0x800E9C30).
OPCODES = {
    WAIT_FILE: (0x0C, 1),
    LOAD: (0x16, 4),
    CONFIGURE: (0x17, 6),
    TWEEN: (0x18, 7),
    FREE: (0x19, 1),
    WAIT_TWEEN: (0x1A, 1),
}

# The operand holding each mode's layer selector in bits 6-7.
SELECTORS = {CONFIGURE: 1, TWEEN: 5}

# The kinds the driver prints for each call, and the argument registers each callee takes.
CALLS = {
    LOAD_ADDRESS: (1, 2),
    ACTIVATE_ADDRESS: (2, 1),
    RELEASE_ADDRESS: (3, 0),
}

# (offset, size, signed) of the layer fields the driver prints: the flag word, x, y and scale.
LAYER_FIELDS = ((0, 4, False), (8, 2, True), (0xA, 2, True), (0xC, 2, True))
LAYER_NAMES = ("word", "x", "y", "scale")
# The tween record fields after its reserved byte.
TWEEN_FIELDS = (
    (1, 1, False), (2, 1, False), (3, 1, False),
    (4, 2, True), (6, 2, True), (8, 2, True), (0xA, 2, True),
    (0xC, 2, True), (0xE, 2, True), (0x10, 2, True), (0x12, 2, True),
)
TWEEN_NAMES = ("easing", "elapsed", "duration", "start x", "start y", "delta x", "delta y",
               "start scale", "delta scale", "start brightness", "delta brightness")

BOUNDARY_BYTES = (0, 1, 0x7F, 0x80, 0xFF)
BOUNDARY_HALVES = (0, 1, 0x7FFF, 0x8000, 0xFFFF)
SKIP_BYTES = (0, 1, 2, 3, 0xFF)


def operand_byte(rng: random.Random) -> int:
    return rng.choice((*BOUNDARY_BYTES, rng.randrange(256), rng.randrange(256)))


def half(rng: random.Random) -> int:
    value = rng.choice((*BOUNDARY_HALVES, rng.randrange(1 << 16), rng.randrange(1 << 16)))
    return value - (1 << 16) if value & 0x8000 else value


def duration(rng: random.Random) -> int:
    return rng.choice((0, 0, 0, 1, 0x80, rng.randrange(256)))


def vector(rng: random.Random, number: int, mode: int) -> dict:
    opcode, length = OPCODES[mode]
    operands = [operand_byte(rng) for _ in range(length - 1)]
    if mode in SELECTORS:
        # Selector 3 indexed past the original's three layers and records.
        at = SELECTORS[mode] - 1
        operands[at] = (operands[at] & 0x3F) | (rng.randrange(LAYERS) << 6)
    return {
        "mode": mode,
        "alignment": number % 4,
        "bytes": [opcode, *operands],
        "before": [rng.randrange(256) for _ in range(NEIGHBOURS)],
        "after": [rng.randrange(256) for _ in range(NEIGHBOURS)],
        # The file wait read through the pointer unguarded.
        "present": mode == WAIT_FILE or rng.randrange(8) != 0,
        "layers": [[rng.randrange(1 << 32), half(rng), half(rng), half(rng)] for _ in range(LAYERS)],
        "tweens": [[rng.randrange(256), rng.randrange(256), duration(rng),
                    *(half(rng) for _ in range(len(TWEEN_FIELDS) - 3))] for _ in range(LAYERS)],
        "skip": rng.choice((*SKIP_BYTES, rng.randrange(256))),
    }


class CallLog:
    """The stubbed callees' arguments, in call order, as [kind, *a0..]."""

    def __init__(self) -> None:
        self.calls: list[list[int]] = []

    def attach(self, machine: Machine) -> None:
        for callee, (kind, count) in CALLS.items():
            machine.stub(callee, lambda m, kind=kind, count=count: self.calls.append(
                [kind, *(s32(m.r[4 + index]) for index in range(count))]))


def put_fields(machine: Machine, base: int, fields, values) -> None:
    for (offset, size, _), value in zip(fields, values, strict=True):
        machine.put(base + offset, value, size)


def load_fields(machine: Machine, base: int, fields) -> list[int]:
    return [machine.load(base + offset, size, signed=signed) for offset, size, signed in fields]


def run(machine: Machine, value: dict, entry: int) -> tuple:
    machine.map_zero(SCRIPT, SCRIPT_BYTES)
    address = SCRIPT + NEIGHBOURS + value["alignment"]
    machine.store(address - NEIGHBOURS, bytes(value["before"]))
    machine.store(address, bytes(value["bytes"]))
    machine.store(address + len(value["bytes"]), bytes(value["after"]))
    machine.map_zero(IMAGE_SET, LAYERS * LAYER_BYTES)
    for layer, fields in enumerate(value["layers"]):
        put_fields(machine, IMAGE_SET + layer * LAYER_BYTES, LAYER_FIELDS, fields)
    for layer, fields in enumerate(value["tweens"]):
        machine.put(TWEENS + layer * TWEEN_BYTES, 0, 1)
        put_fields(machine, TWEENS + layer * TWEEN_BYTES, TWEEN_FIELDS, fields)
    machine.put(IMAGE_SET_POINTER, IMAGE_SET if value["present"] else 0, 4)
    machine.put(SKIP_STATE, value["skip"], 1)
    log = CallLog()
    log.attach(machine)
    machine.follow.add(GET_SHORT)
    machine.r[4] = address
    machine.execute(entry)

    values = [s32(machine.r[2]), 0]
    names = ["flow", "refusals"]
    for layer in range(LAYERS):
        values += load_fields(machine, IMAGE_SET + layer * LAYER_BYTES, LAYER_FIELDS)
        names += [f"layer {layer} {name}" for name in LAYER_NAMES]
    for layer in range(LAYERS):
        values += load_fields(machine, TWEENS + layer * TWEEN_BYTES, TWEEN_FIELDS)
        names += [f"tween {layer} {name}" for name in TWEEN_NAMES]
    inputs = [
        value["mode"],
        int(value["present"]),
        *(field for fields in value["layers"] for field in fields),
        *(field for fields in value["tweens"] for field in fields),
        value["skip"],
        len(value["bytes"]),
        *value["bytes"],
    ]
    return recorded_observation(machine, values, names, log.calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if value["mode"] in (CONFIGURE, TWEEN):
        allowed.append((TWEENS, TWEENS + LAYERS * TWEEN_BYTES))
    if value["mode"] == CONFIGURE:
        allowed.append((IMAGE_SET, IMAGE_SET + LAYERS * LAYER_BYTES))
    return allowed


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    if not results:
        return []
    flows: dict[int, int] = {}
    for _, observation, _ in results:
        flows[observation["values"][0]] = flows.get(observation["values"][0], 0) + 1
    called = sum(1 for _, observation, _ in results if observation["calls"])
    absent = sum(1 for value, _, _ in results if not value["present"])
    return [
        "flows " + ", ".join(f"{flow}: {count}" for flow, count in sorted(flows.items())),
        f"{called} of {len(results)} vectors made an image call, {absent} had no image set",
    ]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
