"""Shared harness for the effect script loop differentials (BATTLE.PRG 5BF94.c:7506-7826, #276).

Each case runs one original under the R3000 interpreter against the typed
VS::Effects script core in effect_script_driver.cpp. The instance's program is a full
16-bit block of zeros with placed bytes, so every offset, including the wrapped ones the
original reaches through its 16-bit offset, reads the same byte on both sides.

Native differences outside the compared domain (covered by EffectScriptTests.cpp): reads
past a recorded program, opcodes past the 49-entry handler table and null instances are
refused instead of reading whatever memory follows.

For the loop, the handler table keeps the original yield, jump, end and continue handlers
(followed) and replaces every other entry with a stub returning a per-opcode result, so the
loop's dispatch and result tests compare while the operand handlers move the offset.
"""

from __future__ import annotations

import functools
import random
from collections import Counter

from differential import Image, Machine, s32

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "effect_script_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\EffectScript.cpp",)

PROGRAM = 0x80400000
PROGRAM_BYTES = 0x10000
INSTANCE = 0x80410000
INSTANCE_BYTES = 0x20
OFFSET_FIELD = 0xA
SCRIPT_FIELD = 0xC
HANDLER_TABLE = 0x800EC3F4
OPCODES = 49
FAKE_HANDLERS = 0x80480000
MASK = 0xFFFFFFFF
# A loop chain's opcodes, and the bytes past a jump target no other chain byte may use
# (MAX_CHAIN three-byte steps plus the stop).
MAX_CHAIN = 12
CHAIN_WINDOW = 64

BRANCH, YIELD, JUMP, END, CONTINUE, READ_BYTE, READ_HALFWORD, RUN = range(8)

READ_BYTE_ADDRESS = 0x800D5170
READ_HALFWORD_ADDRESS = 0x800D5198
# Handler kinds the driver dispatches: a stubbed result, or one of the typed handlers.
STUBBED, YIELD_HANDLER, JUMP_HANDLER, END_HANDLER, CONTINUE_HANDLER = range(5)
REAL_HANDLERS = {
    0x800D4720: YIELD_HANDLER,
    0x800D474C: JUMP_HANDLER,
    0x800D47C0: END_HANDLER,
    0x800D4DA8: CONTINUE_HANDLER,
}


@functools.cache
def handler_kinds() -> tuple[int, ...]:
    """The kind of each original handler table entry; read on first use, so listing the
    cases does not need the disc image."""
    image = Image.load(MODULES[0])
    return tuple(REAL_HANDLERS.get(image.read(HANDLER_TABLE + 4 * opcode), STUBBED) for opcode in range(OPCODES))


def start_offset(rng: random.Random) -> int:
    return rng.choice((0, 0xFFFF, 0xFFFE, 0xFFFD, rng.randrange(0x40), rng.randrange(PROGRAM_BYTES)))


def result(rng: random.Random) -> int:
    return rng.choice((0, 1, 1, 1, 2, 2, 3, -1, rng.randrange(-0x80000000, 0x80000000)))


def place(placed: dict[int, int], offset: int, values) -> int:
    for value in values:
        placed[offset & 0xFFFF] = value
        offset += 1
    return offset & 0xFFFF


def run_program(rng: random.Random, offset: int, results: list[int]) -> dict[int, int]:
    """Opcodes from `offset` that end with a non-continuing step. Each jump lands in a
    window no placed byte reaches and a chain never outgrows its window, so no chain meets
    itself and every run stops."""
    placed: dict[int, int] = {}
    kinds = handler_kinds()
    continuing = [op for op in range(OPCODES)
                  if kinds[op] == CONTINUE_HANDLER or (kinds[op] == STUBBED and results[op] == 1)]
    stopping = [op for op in range(OPCODES)
                if kinds[op] in (YIELD_HANDLER, END_HANDLER) or (kinds[op] == STUBBED and results[op] != 1)]
    jump = kinds.index(JUMP_HANDLER)
    for _ in range(rng.randrange(MAX_CHAIN)):
        if rng.randrange(4):
            offset = place(placed, offset, (rng.choice(continuing),))
            continue
        target = rng.randrange(PROGRAM_BYTES)
        window = {(target + step) & 0xFFFF for step in range(CHAIN_WINDOW)}
        jump_bytes = {(offset + step) & 0xFFFF for step in range(3)}
        if window & (placed.keys() | jump_bytes):
            continue
        place(placed, offset, (jump, target & 0xFF, target >> 8))
        offset = target
    stop = rng.choice(stopping)
    operands = (rng.randrange(256), rng.randrange(256)) if kinds[stop] == YIELD_HANDLER else ()
    place(placed, offset, (stop, *operands))
    return placed


def vector(rng: random.Random, number: int, mode: int) -> dict:
    offset = start_offset(rng)
    results = [result(rng) for _ in range(OPCODES)]
    placed: dict[int, int] = {}
    if mode == RUN:
        placed.update(run_program(rng, offset, results))
    else:
        for _ in range(rng.randrange(4)):
            placed[rng.randrange(PROGRAM_BYTES)] = rng.randrange(256)
        place(placed, offset, (rng.randrange(256) for _ in range(rng.randrange(1, 4))))
    condition = rng.choice((0, 0, 1, -1, 2, -0x80000000, rng.randrange(-0x80000000, 0x80000000)))
    return {"mode": mode, "offset": offset, "condition": condition, "results": results,
            "placed": sorted(placed.items()), "number": number}


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    stubbed: list[int] = []
    foreign: list[int] = []

    def handler(opcode: int):
        def call(core: Machine) -> None:
            stubbed.append(opcode)
            if core.r[4] != INSTANCE:
                foreign.append(core.r[4])
            core.r[2] = value["results"][opcode] & MASK

        return call

    machine.map_zero(PROGRAM, PROGRAM_BYTES)
    for offset, byte in value["placed"]:
        machine.put(PROGRAM + offset, byte, 1)
    machine.store(INSTANCE, bytes(INSTANCE_BYTES))
    machine.put(INSTANCE + OFFSET_FIELD, value["offset"], 2)
    machine.put(INSTANCE + SCRIPT_FIELD, PROGRAM)
    machine.follow.update((READ_BYTE_ADDRESS, READ_HALFWORD_ADDRESS, *REAL_HANDLERS))
    kinds = handler_kinds()
    for opcode, kind in enumerate(kinds):
        if kind == STUBBED:
            machine.put(HANDLER_TABLE + 4 * opcode, FAKE_HANDLERS + 4 * opcode)
            machine.stub(FAKE_HANDLERS + 4 * opcode, handler(opcode))

    if mode == BRANCH:
        machine.r[4] = value["condition"] & MASK
        machine.r[5] = INSTANCE
    else:
        machine.r[4] = INSTANCE
    machine.execute(entry)

    returned = machine.r[2] if mode in (READ_BYTE, READ_HALFWORD) else s32(machine.r[2])
    observation = (returned, machine.load(INSTANCE + OFFSET_FIELD, 2), tuple(stubbed), tuple(foreign))
    inputs = [mode, value["offset"], value["condition"], *kinds, *value["results"], len(value["placed"]),
              *(item for pair in value["placed"] for item in pair)]
    return observation, inputs


def check(image, value: dict, observation, native: list[int], reference) -> list[str]:
    mode = value["mode"]
    returned, offset, stubbed, foreign = observation
    if foreign:
        return [f"original handlers got instances {[hex(address) for address in foreign]}"]
    if len(native) < 3 or len(native) != 3 + native[2]:
        return [f"native printed {len(native)} values"]
    problems = []
    if mode != BRANCH and native[0] != returned:
        problems.append(f"result native {native[0]} original {returned}")
    if mode == BRANCH and native[0] != 1:
        problems.append("native refused the branch operand")
    if native[1] != offset:
        problems.append(f"offset native {native[1]:#x} original {offset:#x}")
    if tuple(native[3:]) != stubbed:
        problems.append(f"stubbed opcodes native {native[3:][:8]} original {list(stubbed)[:8]}")
    return problems


def describe(image, results) -> list[str]:
    if results and results[0][0]["mode"] == BRANCH:
        taken = sum(1 for value, _, _ in results if value["condition"])
        return [f"{taken} vectors took the branch, {len(results) - taken} stepped past it"]
    stubbed = sum(1 for _, observation, _ in results if observation[2])
    returned = Counter(observation[0] for _, observation, _ in results)
    common = ", ".join(f"{value}: {count}" for value, count in returned.most_common(4))
    return [f"{stubbed} vectors ran stubbed handlers; most common results {common}"]


def case_globals(function: int, entry: int, end: int, mutants: dict, *, driver: str, vector, run, check,
                 describe) -> dict:
    """Case-module globals for one effect script original that an effect script harness's
    `driver` compares; `vector` and `run` are the case's bound callbacks."""
    return {
        "ID": f"F{function:05}", "ENTRY": entry, "END": end,
        "DRIVER": driver, "SOURCES": SOURCES, "MODULES": MODULES, "CXX_STANDARD": "c++latest",
        "VECTORS": 256, "SEED": function, "MUTANT_VECTORS": 64, "MUTANTS": mutants,
        "vector": vector, "run": run, "check": check, "describe": describe,
    }


def value_problems(names: tuple[str, ...], ours: list[int], theirs: list[int]) -> list[str]:
    """The named values the native printed that differ from the original's."""
    if len(theirs) != len(ours):
        return [f"native printed {len(theirs)} values, original {len(ours)}: {theirs} {ours}"]
    return [f"{name} native {native_value} original {original}"
            for name, native_value, original in zip(names, theirs, ours, strict=True)
            if native_value != original]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one effect script function compared in `mode`."""
    return case_globals(function, entry, end, mutants, driver=DRIVER,
                        vector=lambda rng, number: vector(rng, number, mode),
                        run=lambda machine, value: run(machine, value, entry), check=check, describe=describe)
