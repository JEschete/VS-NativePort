"""Shared harness for the effect script state and register opcode differentials
(BATTLE.PRG 5BF94.c:7540-7705, #276).

Each case runs one original opcode handler under the R3000 interpreter against the typed
VS::Effects opcode core in effect_script_opcode_driver.cpp. As in the loop harness, the
instance's program is a full 16-bit block of zeros with placed operand bytes, so wrapped
operand reads compare too; the original's script reads and branch are followed.

Native differences outside the compared domain (covered by EffectScriptTests.cpp): register
operands past the four registers are refused instead of writing or reading the instance
fields after them (no shipped program names one), and the Live adapters refuse null
instances. Vectors therefore only name registers 0-3.
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
    READ_HALFWORD_ADDRESS,
    SCRIPT_FIELD,
    place,
    start_offset,
)

DRIVER = "effect_script_opcode_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\EffectScript.cpp",)

BRANCH_ADDRESS = 0x800D46DC
INSTANCE_BYTES = 0xD60
PARTICLE_COUNT_FIELD = 0x8
PRIORITY_FIELD = 0x9
REGISTERS_FIELD = 0x10
REGISTERS = 4
WORD_FIELD = 0x14
TIMELINE_FIELD = 0xD4C
RENDERER_FIELD = 0xD50
SELECTOR = 0x800F5610
VARIANT = 0x800F522C
VARIANT_VALUES = 0x800EC32C
# The original's four variant values, and the bytes after them a mutated bound would read.
PLACED_VALUES = 8

ALLOWED_STORES = [
    (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
    (INSTANCE + OFFSET_FIELD, INSTANCE + OFFSET_FIELD + 2),
    (INSTANCE + REGISTERS_FIELD, INSTANCE + REGISTERS_FIELD + REGISTERS),
    (SELECTOR, SELECTOR + 4),
    (VARIANT, VARIANT + 1),
]

(SELECTOR_MODE, RENDERERS, VARIANT_CLEARED, LOAD_VARIANT, SET_REGISTER, REGISTER_EQUAL,
 PARTICLE_EQUAL, CLEAR_VARIANT) = range(8)
# The operand bytes each branch opcode reads before the branch's own operand.
BRANCH_OPERANDS = {RENDERERS: 0, VARIANT_CLEARED: 0, REGISTER_EQUAL: 3, PARTICLE_EQUAL: 1}
NAMES = ("result", "offset", *(f"register {index}" for index in range(REGISTERS)), "selector", "variant")


def byte(rng: random.Random) -> int:
    return rng.choice((0, 1, 2, 3, 0x7F, 0x80, 0xFF, rng.randrange(256), rng.randrange(256)))


def halfword(rng: random.Random, low: int) -> int:
    """A halfword around a register's value: equal, equal in the low byte only, or any."""
    return rng.choice((low, low, low | 0x100, low | 0xFF00, rng.randrange(0x10000)))


def operands(rng: random.Random, mode: int, value: dict) -> tuple[int, ...]:
    target = rng.randrange(PROGRAM_BYTES)
    branch = (target & 0xFF, target >> 8)
    if mode == SELECTOR_MODE:
        return (byte(rng),)
    if mode in (RENDERERS, VARIANT_CLEARED):
        return branch
    if mode in (SET_REGISTER, REGISTER_EQUAL):
        index = rng.randrange(REGISTERS)
        compared = halfword(rng, value["registers"][index])
        written = (index, compared & 0xFF, compared >> 8)
        return written + branch if mode == REGISTER_EQUAL else written
    if mode == PARTICLE_EQUAL:
        count = value["particles"] if rng.randrange(2) else byte(rng)
        return (count, *branch)
    return ()


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value = {
        "mode": mode, "number": number, "offset": start_offset(rng),
        "registers": [byte(rng) for _ in range(REGISTERS)],
        "particles": byte(rng), "priority": byte(rng),
        "renderer": rng.choice((0, 0, 1, 0x80123450, rng.randrange(1 << 32))),
        "timeline": rng.choice((0, 1, 0x80123460)),
        "selector": rng.choice((0, -1, rng.randrange(-0x80000000, 0x80000000))),
        "variant": rng.choice((0, 0, 1, 2, 3, 4, 5, 0xFF, rng.randrange(256))),
        "values": [byte(rng) for _ in range(PLACED_VALUES)],
        "word": rng.randrange(1 << 32),
    }
    placed: dict[int, int] = {}
    place(placed, value["offset"], operands(rng, mode, value))
    value["placed"] = sorted(placed.items())
    return value


def run(machine: Machine, value: dict, entry: int) -> tuple:
    machine.map_zero(PROGRAM, PROGRAM_BYTES)
    for offset, operand in value["placed"]:
        machine.put(PROGRAM + offset, operand, 1)
    machine.store(INSTANCE, bytes(INSTANCE_BYTES))
    machine.put(INSTANCE + PARTICLE_COUNT_FIELD, value["particles"], 1)
    machine.put(INSTANCE + PRIORITY_FIELD, value["priority"], 1)
    machine.put(INSTANCE + OFFSET_FIELD, value["offset"], 2)
    machine.put(INSTANCE + SCRIPT_FIELD, PROGRAM)
    machine.store(INSTANCE + REGISTERS_FIELD, bytes(value["registers"]))
    machine.put(INSTANCE + WORD_FIELD, value["word"])
    machine.put(INSTANCE + TIMELINE_FIELD, value["timeline"])
    machine.put(INSTANCE + RENDERER_FIELD, value["renderer"])
    machine.put(SELECTOR, value["selector"])
    machine.put(VARIANT, value["variant"], 1)
    machine.store(VARIANT_VALUES, bytes(value["values"]))
    machine.follow.update((READ_BYTE_ADDRESS, READ_HALFWORD_ADDRESS, BRANCH_ADDRESS))
    machine.r[4] = INSTANCE
    machine.execute(entry)

    observation = {
        "values": [s32(machine.r[2]), machine.load(INSTANCE + OFFSET_FIELD, 2),
                   *machine.bytes(INSTANCE + REGISTERS_FIELD, REGISTERS),
                   machine.load(SELECTOR, signed=True), machine.load(VARIANT, 1)],
        "stores": list(machine.stores),
    }
    inputs = [value["mode"], value["offset"], *value["registers"], value["particles"],
              1 if value["renderer"] else 0, value["selector"], value["variant"],
              *value["values"][:REGISTERS], len(value["placed"]),
              *(item for pair in value["placed"] for item in pair)]
    return observation, inputs


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    if len(native) != 1 + len(NAMES):
        return [f"native printed {len(native)} values"]
    problems = [f"native refused ({native[0]})"] if native[0] else []
    problems += [f"{name} native {theirs} original {ours}"
                 for name, theirs, ours in zip(NAMES, native[1:], observation["values"], strict=True)
                 if theirs != ours]
    return problems + store_problems(observation["stores"], ALLOWED_STORES)


def describe(image, results) -> list[str]:
    if not results:
        return []
    mode = results[0][0]["mode"]
    if mode in BRANCH_OPERANDS:
        stepped = sum(1 for value, observation, _ in results
                      if observation["values"][1] == (value["offset"] + BRANCH_OPERANDS[mode] + 2) & 0xFFFF)
        return [f"{len(results) - stepped} vectors took the branch, {stepped} stepped past it"]
    changed = sum(1 for value, observation, _ in results
                  if observation["values"][2:] != [*value["registers"], value["selector"], value["variant"]])
    return [f"{changed} of {len(results)} vectors changed a register, the selector or the variant"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one effect script opcode compared in `mode`."""
    return {
        "ID": f"F{function:05}", "ENTRY": entry, "END": end,
        "DRIVER": DRIVER, "SOURCES": SOURCES, "MODULES": MODULES, "CXX_STANDARD": "c++latest",
        "VECTORS": 256, "SEED": function, "MUTANT_VECTORS": 64, "MUTANTS": mutants,
        "vector": lambda rng, number: vector(rng, number, mode),
        "run": lambda machine, value: run(machine, value, entry),
        "check": check, "describe": describe,
    }
