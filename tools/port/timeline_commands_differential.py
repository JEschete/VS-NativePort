"""Shared harness for the effect timeline action-control command differentials (BATTLE.PRG
5BF94.c:8213-8340, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects selection in
timeline_commands_driver.cpp. The originals read the effect instance's four endpoints (source
+0xD1C, emit-from +0xD28, target +0xD34, emit-to +0xD40) and call one battle, field, actor or audio
routine; the stubs record each call's arguments, reading a passed point's four halfwords. Every
vector fills all four endpoints, including the colour preset beside each type, so an original
reading the wrong endpoint or field differs.

The reaction sound stub for rand() continues the BIOS sequence from the vector's seed, and the
last reaction variant byte (D_800EC4B8) sits between random neighbours.

Native differences outside the compared domain (covered by TimelineCommandsTests.cpp): a missing
instance is refused instead of read.
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
DRIVER = "timeline_commands_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\TimelineCommands.cpp",)

INSTANCE = 0x80400000
INSTANCE_BYTES = 0xD60
ENDPOINTS = (0xD1C, 0xD28, 0xD34, 0xD40)
VALUE_FIELD = 4
LAST_VARIANT = 0x800EC4B8

TARGET_IMPACT_ADDRESS = 0x8006CB04
SOURCE_IMPACT_ADDRESS = 0x8006CA20
DISABLE_TRAP_ADDRESS = 0x8006CDD8
SIGNAL_ADDRESS = 0x8006CDB8
BRIGHTNESS_ADDRESS = 0x8009F898
RAND_ADDRESS = 0x80026EC4
SOUND_ADDRESS = 0x800AE68C
UNLOCK_ADDRESS = 0x8006CE70
ARM_ADDRESS = 0x80069C6C

(
    TARGET_IMPACT,
    SOURCE_IMPACT,
    ROOM_TILE,
    SIGNAL,
    BRIGHTNESS,
    SOUND,
    GIMMICK,
    DEFENSE,
) = range(8)

# The kinds the driver prints for each call.
CALL_TARGET_IMPACT, CALL_SOURCE_IMPACT, CALL_TRAP, CALL_SIGNAL = 1, 2, 3, 4
CALL_BRIGHTNESS, CALL_SOUND, CALL_UNLOCK, CALL_ARM = 5, 6, 7, 8

RAND_MULTIPLIER = 0x41C64E6D
RAND_INCREMENT = 0x3039


def endpoint(rng: random.Random) -> dict:
    """Mostly actor (4) and room point (5) endpoints, and types whose low byte or bits match."""
    return {
        "type": rng.choice(
            (
                4,
                4,
                4,
                5,
                5,
                5,
                0,
                1,
                2,
                3,
                6,
                0x104,
                0x105,
                0x400,
                rng.randrange(1 << 16),
            )
        ),
        "colour": rng.randrange(1 << 16),
        "point": [
            rng.choice((0, 1, -1, 0x7FFF, -0x8000, rng.randrange(-0x8000, 0x8000)))
            for _ in range(4)
        ],
    }


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {
        "mode": mode,
        "number": number,
        "endpoints": [endpoint(rng) for _ in ENDPOINTS],
    }
    if mode == BRIGHTNESS:
        value["brightness"] = rng.choice((0, 0, 1, 0x80, 0xFF, rng.randrange(256)))
    elif mode == SOUND:
        # Bias towards an actor emitter whose last random variant can repeat.
        if rng.randrange(4):
            value["endpoints"][1]["type"] = 4
        value["variant"] = rng.choice((0, 3, 4, 4, 4, 5, 0xFF, rng.randrange(256)))
        value["last"] = rng.choice((0, 1, 2, 0, 1, 2, 3, 0xFF, rng.randrange(256)))
        value["beside"] = [rng.randrange(256) for _ in range(2)]
        value["seed"] = rng.randrange(1 << 32)
    return value


def prepare(machine: Machine, value: dict) -> None:
    machine.map_zero(INSTANCE, INSTANCE_BYTES)
    for offset, item in zip(ENDPOINTS, value["endpoints"], strict=True):
        machine.put(INSTANCE + offset, item["type"], 2)
        machine.put(INSTANCE + offset + 2, item["colour"], 2)
        for index, half in enumerate(item["point"]):
            machine.put(INSTANCE + offset + VALUE_FIELD + index * 2, half, 2)
    if value["mode"] == SOUND:
        machine.put(LAST_VARIANT - 1, value["beside"][0], 1)
        machine.put(LAST_VARIANT, value["last"], 1)
        machine.put(LAST_VARIANT + 1, value["beside"][1], 1)


def point(machine: Machine, address: int) -> list[int]:
    return [machine.load(address + index * 2, 2, signed=True) for index in range(4)]


def recorder(calls: list[list[int]], rand: dict):
    def impact(kind: int):
        def stub(machine: Machine) -> None:
            pointer = machine.r[5]
            located = point(machine, pointer) if pointer else [0, 0, 0, 0]
            calls.append([kind, s32(machine.r[4]), 1 if pointer else 0, *located])

        return stub

    def at_point(kind: int):
        def stub(machine: Machine) -> None:
            calls.append([kind, *point(machine, machine.r[4])])

        return stub

    def registers(kind: int, count: int):
        def stub(machine: Machine) -> None:
            calls.append([kind, *(s32(machine.r[4 + index]) for index in range(count))])

        return stub

    def draw(machine: Machine) -> None:
        rand["state"] = (rand["state"] * RAND_MULTIPLIER + RAND_INCREMENT) & 0xFFFFFFFF
        rand["draws"] += 1
        machine.r[2] = (rand["state"] >> 16) & 0x7FFF

    return {
        TARGET_IMPACT_ADDRESS: impact(CALL_TARGET_IMPACT),
        SOURCE_IMPACT_ADDRESS: impact(CALL_SOURCE_IMPACT),
        DISABLE_TRAP_ADDRESS: at_point(CALL_TRAP),
        SIGNAL_ADDRESS: registers(CALL_SIGNAL, 1),
        BRIGHTNESS_ADDRESS: registers(CALL_BRIGHTNESS, 3),
        RAND_ADDRESS: draw,
        SOUND_ADDRESS: registers(CALL_SOUND, 2),
        UNLOCK_ADDRESS: at_point(CALL_UNLOCK),
        ARM_ADDRESS: registers(CALL_ARM, 1),
    }


def allowed_stores(mode: int) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if mode == SOUND:
        allowed.append((LAST_VARIANT, LAST_VARIANT + 1))
    return allowed


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    prepare(machine, value)
    calls: list[list[int]] = []
    rand = {"state": value.get("seed", 0), "draws": 0}
    for address, stub in recorder(calls, rand).items():
        machine.stub(address, stub)
    machine.r[4] = INSTANCE
    inputs: list[int] = [mode]
    for item in value["endpoints"]:
        inputs += [item["type"], *item["point"]]
    if mode == BRIGHTNESS:
        machine.r[5] = value["brightness"]
        inputs.append(value["brightness"])
    elif mode == SOUND:
        machine.r[5] = value["variant"]
        inputs += [value["variant"], value["last"], value["seed"]]
    machine.execute(entry)

    values = [len(calls), *(item for call in calls for item in call)]
    names = [
        "calls",
        *(
            f"call {index} word {word}"
            for index, call in enumerate(calls)
            for word in range(len(call))
        ),
    ]
    if mode == SOUND:
        values += [rand["draws"], machine.load(LAST_VARIANT, 1)]
        names += ["draws", "last variant"]
    return {"values": values, "names": names, "stores": list(machine.stores)}, inputs


def check(
    image, value: dict, observation: dict, native: list[int], reference
) -> list[str]:
    ours = observation["values"]
    if native != ours and len(native) != len(ours):
        return [f"native printed {native}, original {ours}"]
    return field_problems(
        observation["names"], native, ours, hexadecimal=False
    ) + store_problems(observation["stores"], allowed_stores(value["mode"]))


def describe(image, results) -> list[str]:
    if not results:
        return []
    called = sum(1 for _, observation, _ in results if observation["values"][0])
    lines = [f"{called} of {len(results)} vectors made the call"]
    if results[0][0]["mode"] == SOUND:
        redrawn = sum(
            1 for _, observation, _ in results if observation["values"][-2] > 1
        )
        lines.append(f"{redrawn} of {len(results)} vectors redrew a repeated variant")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
