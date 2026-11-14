"""Shared harness for the effect script transform, colour and child opcode differentials and
the diagnostic hook (BATTLE.PRG 5BF94.c:5045 and 7602-7799, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects core in
effect_script_particle_driver.cpp. The bound package is a fake record behind D_800F569C whose
block 5 holds the vector's particle definitions and whose block 9 holds its transform count.
The colour presets the originals index are the module's D_800EC330 and D_800EC368 bytes, read
from the image and passed to the driver.

Native differences outside the compared domain (covered by EffectScriptTests.cpp and
EffectResourcesTransformsTests.cpp): block-9 transform counts past the instance's 16 slots
are clamped instead of writing past the transforms; definitions past the bound package and
presets past the seven are refused instead of writing or reading whatever memory follows;
and the Live adapters refuse null instances. Vectors therefore keep counts to 16, every
definition inside the package and presets below seven. The original's invalid-instruction
handler returned its own address, which the loop reads as a yield; the case compares the
step the loop takes. The original's diagnostic hook is empty: its case checks that the hook
leaves $v0 and memory alone and that the native report describes exactly the two codes the
original's only call sites (0x800D2814 and 0x800D5158) pass.
"""

from __future__ import annotations

import random

from differential import DEFAULT_STACK, DEFAULT_STACK_SIZE, Machine, s32, store_problems
from effect_script_differential import (
    INSTANCE,
    MODULES,
    OFFSET_FIELD,
    PROGRAM,
    PROGRAM_BYTES,
    READ_BYTE_ADDRESS,
    SCRIPT_FIELD,
    place,
    start_offset,
)

DRIVER = "effect_script_particle_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\EffectScript.cpp",)

INSTANCE_BYTES = 0xD60
PARENT_FIELD = 0x4
REGISTER3_FIELD = 0x13
TRANSFORM_Y_FIELD = 0x3C
TRANSFORM_STRIDE = 0xD0
TRANSFORMS = 16
SOURCE_TYPE_FIELD = 0xD1C
SOURCE_ACTOR_FIELD = 0xD20
TARGET_PRESET_FIELD = 0xD36
EFFECT_ID_FIELD = 0xD54

PACKAGE_POINTER = 0x800F569C
PACKAGE = 0x80420000
PACKAGE_BYTES = 0xB8
BLOCK5_FIELD = 0x8
BLOCK9_FIELD = 0xB4
BLOCK5 = 0x80430000
DEFINITION_HEADER = 4
DEFINITION_BYTES = 0xCC
CURVES_FIELD = 0xC4
MAX_DEFINITIONS = 64
BLOCK9 = 0x80440000
BLOCK9_BYTES = 0x16
TRANSFORM_COUNT_FIELD = 0x15
PARENT = 0x80450000
WORD_FIELD = 0x14
CHILD_SHIFT = 11
CHILD_MASK = 0x1F

SIGNAL_ADDRESS = 0x8006CDB8
DIAGNOSTIC_ADDRESS = 0x800CE644
INVALID_ADDRESS = 0x800D5150
PAIRED_PRESETS = 0x800EC330
PRESET_TABLE = 0x800EC368
PRESETS = 7
PAIRED_BYTES = PRESETS * 2 * 4
TABLE_BYTES = PRESETS * 5 * 4
# The definitions D_800EC368's entries name reach 36.
TABLE_DEFINITIONS = 37
DIAGNOSTIC_CODES = (0x14, 0x1E)

VERTICAL, SIGNAL, CURVES, PAIRED, CHILD, TABLE, INVALID, DIAGNOSTIC = range(8)
NAMES = {
    VERTICAL: ("result", *(f"height {index}" for index in range(TRANSFORMS))),
    SIGNAL: ("result", "signals", "actor"),
    CHILD: ("result", "parent word"),
    INVALID: ("step", "reports", "code"),
    DIAGNOSTIC: ("v0 kept", "described"),
}
CURVE_MODES = (CURVES, PAIRED, TABLE)


def byte(rng: random.Random) -> int:
    return rng.choice((0, 1, 0x3F, 0x40, 0x7F, 0x80, 0xC0, 0xFF, rng.randrange(256), rng.randrange(256)))


def effect_id(rng: random.Random) -> int:
    return rng.choice((5, 6, 21, 22, 22, 0x21, 0x22, 0x23, 0x24, 0x25, 0x37, 0x38, 0x38, 0x38, 0x38,
                       0x39, 0x8021, 0xFF24, 0xFFFF, rng.randrange(0x40), rng.randrange(0x10000)))


def curves(rng: random.Random, count: int) -> list[int]:
    return [rng.choice((0, 0x3F, rng.randrange(256))) for _ in range(3 * count)]


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode, "number": number}
    if mode == VERTICAL:
        value |= {"effect": effect_id(rng), "register3": rng.choice((0x2B, 0x2B, 0x2A, 0x2C, byte(rng))),
                  "count": rng.choice((0, 1, 2, TRANSFORMS, rng.randint(1, TRANSFORMS), rng.randint(0, TRANSFORMS))),
                  "heights": [rng.randrange(-0x80000000, 0x80000000) for _ in range(TRANSFORMS)]}
    elif mode == SIGNAL:
        value |= {"type": rng.choice((4, 4, 4, 0, 3, 5, 0x104, 0xFF04, rng.randrange(0x10000))),
                  "actor": [byte(rng) for _ in range(4)]}
    elif mode == CURVES:
        operands = [byte(rng) for _ in range(3)]
        least = (operands[0] & 0x3F) + 1
        count = rng.choice((least, least, least + 1, rng.randint(least, MAX_DEFINITIONS)))
        offset = start_offset(rng)
        placed: dict[int, int] = {}
        place(placed, offset, tuple(operands))
        value |= {"offset": offset, "placed": sorted(placed.items()),
                  "count": min(count, MAX_DEFINITIONS), "curves": None}
        value["curves"] = curves(rng, value["count"])
    elif mode in (PAIRED, TABLE):
        least = 2 if mode == PAIRED else TABLE_DEFINITIONS
        count = rng.choice((least, least, least + 1, rng.randint(least, MAX_DEFINITIONS)))
        value |= {"preset": rng.randrange(PRESETS), "count": count, "curves": curves(rng, count)}
    elif mode == CHILD:
        children = rng.choice((0, 1, 2, CHILD_MASK, rng.randrange(CHILD_MASK + 1)))
        word = rng.randrange(1 << 32) & ~(CHILD_MASK << CHILD_SHIFT) | children << CHILD_SHIFT
        value |= {"parent": rng.randrange(8) != 0, "word": word}
    elif mode == INVALID:
        value |= {"instance": rng.choice((INSTANCE, 0, rng.randrange(1 << 32)))}
    else:
        value |= {"code": rng.choice((*DIAGNOSTIC_CODES, *DIAGNOSTIC_CODES, 0, 0x15, 0x1F,
                                      rng.randrange(-0x80000000, 0x80000000))),
                  "v0": rng.choice((0, 1, 2, INVALID_ADDRESS, rng.randrange(1 << 32)))}
    return value


def definition(index: int) -> int:
    return BLOCK5 + DEFINITION_HEADER + index * DEFINITION_BYTES


def bind_package(machine: Machine, value: dict) -> None:
    machine.put(PACKAGE_POINTER, PACKAGE)
    machine.map_zero(PACKAGE, PACKAGE_BYTES)
    machine.put(PACKAGE + BLOCK5_FIELD, BLOCK5)
    machine.put(PACKAGE + BLOCK9_FIELD, BLOCK9)
    machine.map_zero(BLOCK5, DEFINITION_HEADER + MAX_DEFINITIONS * DEFINITION_BYTES)
    machine.map_zero(BLOCK9, BLOCK9_BYTES)
    machine.put(BLOCK9 + TRANSFORM_COUNT_FIELD, value.get("count", 0), 1)
    for index in range(len(value.get("curves") or ()) // 3):
        machine.store(definition(index) + CURVES_FIELD, bytes(value["curves"][3 * index:3 * index + 3]))


def read_curves(machine: Machine, count: int) -> list[int]:
    return [item for index in range(count) for item in machine.bytes(definition(index) + CURVES_FIELD, 3)]


def step_result(v0: int) -> int:
    """The handler result the script loop acts on: 2 ends, 1 continues, anything else yields."""
    return v0 if v0 in (1, 2) else 0


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    mode = value["mode"]
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if mode == VERTICAL:
        allowed += [(at, at + 4) for at in (INSTANCE + TRANSFORM_Y_FIELD + index * TRANSFORM_STRIDE
                                             for index in range(value["count"]))]
    elif mode in CURVE_MODES:
        allowed += [(definition(index) + CURVES_FIELD, definition(index) + CURVES_FIELD + 3)
                    for index in range(value["count"])]
        if mode == CURVES:
            allowed.append((INSTANCE + OFFSET_FIELD, INSTANCE + OFFSET_FIELD + 2))
    elif mode == CHILD and value["parent"]:
        allowed.append((PARENT + WORD_FIELD, PARENT + WORD_FIELD + 4))
    return allowed


def prepare(machine: Machine, value: dict) -> None:
    mode = value["mode"]
    machine.map_zero(INSTANCE, INSTANCE_BYTES)
    bind_package(machine, value)
    if mode == VERTICAL:
        machine.put(INSTANCE + EFFECT_ID_FIELD, value["effect"], 2)
        machine.put(INSTANCE + REGISTER3_FIELD, value["register3"], 1)
        for index, height in enumerate(value["heights"]):
            machine.put(INSTANCE + TRANSFORM_Y_FIELD + index * TRANSFORM_STRIDE, height)
    elif mode == SIGNAL:
        machine.put(INSTANCE + SOURCE_TYPE_FIELD, value["type"], 2)
        machine.store(INSTANCE + SOURCE_ACTOR_FIELD, bytes(value["actor"]))
    elif mode == CURVES:
        machine.map_zero(PROGRAM, PROGRAM_BYTES)
        for offset, operand in value["placed"]:
            machine.put(PROGRAM + offset, operand, 1)
        machine.put(INSTANCE + OFFSET_FIELD, value["offset"], 2)
        machine.put(INSTANCE + SCRIPT_FIELD, PROGRAM)
        machine.follow.add(READ_BYTE_ADDRESS)
    elif mode in (PAIRED, TABLE):
        machine.put(INSTANCE + TARGET_PRESET_FIELD, value["preset"], 2)
    elif mode == CHILD:
        machine.map_zero(PARENT, WORD_FIELD + 4)
        machine.put(PARENT + WORD_FIELD, value["word"])
        machine.put(INSTANCE + PARENT_FIELD, PARENT if value["parent"] else 0)


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    prepare(machine, value)
    calls: list[int] = []
    machine.stub(SIGNAL_ADDRESS, lambda m: calls.append(m.r[4]))
    if mode == INVALID:
        machine.stub(DIAGNOSTIC_ADDRESS, lambda m: calls.append(s32(m.r[4])))
        machine.r[2] = INVALID_ADDRESS
        machine.r[4] = value["instance"]
    elif mode == DIAGNOSTIC:
        machine.r[2] = value["v0"]
        machine.r[4] = value["code"] & 0xFFFFFFFF
    else:
        machine.r[4] = INSTANCE
    machine.execute(entry)

    v0 = machine.r[2]
    inputs: list[int] = [mode]
    if mode == VERTICAL:
        values = [s32(v0), *(machine.load(INSTANCE + TRANSFORM_Y_FIELD + index * TRANSFORM_STRIDE, signed=True)
                             for index in range(TRANSFORMS))]
        inputs += [value["effect"], value["register3"], value["count"], *value["heights"]]
    elif mode == SIGNAL:
        values = [s32(v0), len(calls), *calls]
        inputs += [value["type"], value["actor"][0]]
    elif mode in CURVE_MODES:
        values = [s32(v0), *read_curves(machine, value["count"])]
        if mode == CURVES:
            values.insert(1, machine.load(INSTANCE + OFFSET_FIELD, 2))
            inputs += [value["offset"], value["count"], *value["curves"], len(value["placed"]),
                       *(item for pair in value["placed"] for item in pair)]
        else:
            table = machine.bytes(*((PAIRED_PRESETS, PAIRED_BYTES) if mode == PAIRED else (PRESET_TABLE, TABLE_BYTES)))
            inputs += [value["preset"], value["count"], *value["curves"], *table]
    elif mode == CHILD:
        values = [s32(v0), machine.load(PARENT + WORD_FIELD)]
        inputs += [1 if value["parent"] else 0, value["word"]]
    elif mode == INVALID:
        values = [step_result(v0), len(calls), *calls]
    else:
        values = [1 if v0 == value["v0"] else 0]
        inputs += [value["code"]]
    return {"values": values, "stores": list(machine.stores)}, inputs


def names(value: dict, count: int) -> tuple[str, ...]:
    mode = value["mode"]
    if mode in CURVE_MODES:
        prefix = ("result", "offset") if mode == CURVES else ("result",)
        return prefix + tuple(f"definition {index // 3} {'rgb'[index % 3]}" for index in range(count - len(prefix)))
    return NAMES[mode][:count]


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    if not native:
        return ["native printed nothing"]
    problems = [f"native refused ({native[0]})"] if native[0] else []
    ours = observation["values"]
    theirs = native[1:]
    if value["mode"] == DIAGNOSTIC:
        expected = 1 if value["code"] in DIAGNOSTIC_CODES else 0
        if ours != [1]:
            problems.append("the original hook changed $v0")
        if theirs != [expected]:
            problems.append(f"native described code {value['code']:#x} as {theirs}, expected {expected}")
        return problems + store_problems(observation["stores"], allowed_stores(value))
    if len(theirs) != len(ours):
        return problems + [f"native printed {len(theirs)} values, original {len(ours)}: {theirs} {ours}"]
    problems += [f"{name} native {native_value} original {original}"
                 for name, native_value, original in zip(names(value, len(ours)), theirs, ours, strict=True)
                 if native_value != original]
    return problems + store_problems(observation["stores"], allowed_stores(value))


def describe(image, results) -> list[str]:
    if not results:
        return []
    mode = results[0][0]["mode"]
    if mode == VERTICAL:
        moved = sum(1 for value, observation, _ in results if observation["values"][1:] != value["heights"])
        return [f"{moved} of {len(results)} vectors moved a transform"]
    if mode == SIGNAL:
        signalled = sum(1 for _, observation, _ in results if observation["values"][1])
        return [f"{signalled} of {len(results)} vectors signalled an actor"]
    if mode in CURVE_MODES:
        start = 2 if mode == CURVES else 1
        changed = sum(1 for value, observation, _ in results if observation["values"][start:] != value["curves"])
        return [f"{changed} of {len(results)} vectors changed a definition's curves"]
    if mode == CHILD:
        wrapped = sum(1 for value, _, _ in results
                      if value["parent"] and not (value["word"] >> CHILD_SHIFT) & CHILD_MASK)
        return [f"{wrapped} vectors wrapped a parent's zero child count"]
    if mode == INVALID:
        return [f"original results {sorted({observation['values'][0] for _, observation, _ in results})}"]
    described = sum(1 for value, _, _ in results if value["code"] in DIAGNOSTIC_CODES)
    return [f"{described} of {len(results)} vectors passed a code the original's callers pass"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return {
        "ID": f"F{function:05}", "ENTRY": entry, "END": end,
        "DRIVER": DRIVER, "SOURCES": SOURCES, "MODULES": MODULES, "CXX_STANDARD": "c++latest",
        "VECTORS": 256, "SEED": function, "MUTANT_VECTORS": 64, "MUTANTS": mutants,
        "vector": lambda rng, number: vector(rng, number, mode),
        "run": lambda machine, value: run(machine, value, entry),
        "check": check, "describe": describe,
    }
