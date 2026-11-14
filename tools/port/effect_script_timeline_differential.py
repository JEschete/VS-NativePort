"""Shared harness for the effect script timeline, transform and particle-environment opcode
differentials (BATTLE.PRG 5BF94.c:7569-7740, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects core in
effect_script_timeline_driver.cpp. The originals' runtime callees (the timeline load and tick,
the heap free, the transform update and set-up, and the particle environment) are stubs that
record their calls; the driver's recording host prints the calls the core makes, so a case
compares the step, the script offset, which callee ran with which argument, and the timeline
field. The bound package is a fake record behind D_800F569C whose block 9 holds the vector's
timeline ids at +0x16 and whose block 3, when bound, leads with its timeline count.

Native differences outside the compared domain (covered by EffectScriptTests.cpp): register
operands past the four registers are refused instead of reading the instance bytes that
follow, and the timeline tick and finished branch refuse an instance without a timeline
instead of reading through null. Vectors therefore name registers 0-3 and give the tick and
the branch a timeline. The native host interface carries the instance implicitly, so each call
is checked against the original's: the instance (or the freed timeline) in $a0, and the
original's always-zero third load argument in $a2.
"""

from __future__ import annotations

import random

from differential import DEFAULT_STACK, DEFAULT_STACK_SIZE, Machine, s32, store_problems
from effect_script_differential import (
    INSTANCE,
    OFFSET_FIELD,
    PROGRAM,
    PROGRAM_BYTES,
    READ_BYTE_ADDRESS,
    READ_HALFWORD_ADDRESS,
    SCRIPT_FIELD,
    case_globals,
    place,
    start_offset,
    value_problems,
)

DRIVER = "effect_script_timeline_driver.cpp"

INSTANCE_BYTES = 0xD60
PARENT_FIELD = 0x4
REGISTERS_FIELD = 0x10
TIMELINE_FIELD = 0xD4C
BRANCH_ADDRESS = 0x800D46DC

PACKAGE_POINTER = 0x800F569C
PACKAGE = 0x80420000
PACKAGE_BYTES = 0xC0
BLOCK9_FIELD = 0xB4
BLOCK3_FIELD = 0xBC
BLOCK9 = 0x80440000
BLOCK9_BYTES = 0x20
TIMELINE_IDS_FIELD = 0x16
BLOCK3 = 0x80430000
TIMELINE = 0x80460000
TIMELINE_BYTES = 8
TRACK_COUNT_FIELD = 0
ACTIVE_TRACKS_FIELD = 4
PARENT = 0x80450000
TIMELINE_IDS = 4

LOAD_ADDRESS = 0x800D55A4
TICK_ADDRESS = 0x800D6AEC
FREE_ADDRESS = 0x80043C60
UPDATE_ADDRESS = 0x800D0D08
INITIALIZE_ADDRESS = 0x800CFEF0
ENVIRONMENT_ADDRESS = 0x800D78F0

IMMEDIATE, REGISTER, TICK, FINISHED, FREE, UPDATE, INITIALIZE, ENVIRONMENT = range(8)
LOADED, TICKED, FREED, UPDATED, INITIALIZED, REFRESHED = range(1, 7)
CALL_NAMES = {LOADED: "load", TICKED: "tick", FREED: "free", UPDATED: "update",
              INITIALIZED: "initialize", REFRESHED: "refresh"}
PROGRAM_MODES = (IMMEDIATE, REGISTER, TICK, FINISHED)
SERVICE_CALLS = {UPDATE: (UPDATE_ADDRESS, UPDATED), INITIALIZE: (INITIALIZE_ADDRESS, INITIALIZED),
                 ENVIRONMENT: (ENVIRONMENT_ADDRESS, REFRESHED)}


def byte(rng: random.Random) -> int:
    return rng.choice((0, 1, 2, 3, 4, 5, 0x30, 0x7F, 0x80, 0xFF, rng.randrange(256), rng.randrange(256)))


def register_value(rng: random.Random) -> int:
    """Register values 0-3 are the ones a root instance maps through the timeline ids."""
    return rng.choice((0, 1, 1, 2, 3, 0, 1, 2, 3, byte(rng)))


def timeline_id(rng: random.Random) -> int:
    return rng.choice((0, 1, 2, 3, 4, 5, 5, 6, 0x3C, 0xFF, rng.randrange(256)))


def timeline_count(rng: random.Random) -> int | None:
    return rng.choice((None, 0, 1, 3, 5, 6, 6, 7, 0x3D, 0x100, -1, -2, 0x7FFFFFFF, -0x80000000,
                       rng.randrange(-0x80000000, 0x80000000)))


def word(rng: random.Random) -> int:
    return rng.choice((0, 0, 1, 2, 0x80000000, 0xFFFFFFFF, 1 << rng.randrange(32), rng.randrange(1 << 32)))


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode, "number": number}
    if mode in PROGRAM_MODES:
        offset = start_offset(rng)
        operands: tuple[int, ...]
        if mode == IMMEDIATE:
            operands = (byte(rng),)
        elif mode == REGISTER:
            operands = (rng.choice((1, 1, 0, 2, 3)),)
        else:
            operands = (rng.randrange(256), rng.choice((0, 0, 1, rng.randrange(256))))
        placed: dict[int, int] = {}
        place(placed, offset, operands)
        value |= {"offset": offset, "placed": sorted(placed.items()), "operands": list(operands)}
    if mode == REGISTER:
        value |= {"registers": [register_value(rng) for _ in range(4)], "parent": rng.randrange(4) == 0,
                  "ids": [timeline_id(rng) for _ in range(TIMELINE_IDS)], "count": timeline_count(rng),
                  "filler": [rng.randrange(1, 256) for _ in range(BLOCK9_BYTES)]}
    elif mode == FINISHED:
        value |= {"active": word(rng), "tracks": word(rng)}
    elif mode == FREE:
        value |= {"timeline": rng.choice((0, 0, TIMELINE, TIMELINE, rng.randrange(1, 1 << 32)))}
    return value


def bind_package(machine: Machine, value: dict) -> None:
    machine.put(PACKAGE_POINTER, PACKAGE)
    machine.map_zero(PACKAGE, PACKAGE_BYTES)
    machine.put(PACKAGE + BLOCK9_FIELD, BLOCK9)
    machine.map_zero(BLOCK9, BLOCK9_BYTES)
    machine.store(BLOCK9, bytes(value["filler"]))
    machine.store(BLOCK9 + TIMELINE_IDS_FIELD, bytes(value["ids"]))
    if value["count"] is not None:
        machine.put(PACKAGE + BLOCK3_FIELD, BLOCK3)
        machine.map_zero(BLOCK3, 4)
        machine.put(BLOCK3, value["count"])


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if value["mode"] in PROGRAM_MODES:
        allowed.append((INSTANCE + OFFSET_FIELD, INSTANCE + OFFSET_FIELD + 2))
    elif value["mode"] == FREE:
        allowed.append((INSTANCE + TIMELINE_FIELD, INSTANCE + TIMELINE_FIELD + 4))
    return allowed


def prepare(machine: Machine, value: dict) -> None:
    mode = value["mode"]
    machine.map_zero(INSTANCE, INSTANCE_BYTES)
    if mode in PROGRAM_MODES:
        machine.map_zero(PROGRAM, PROGRAM_BYTES)
        for offset, operand in value["placed"]:
            machine.put(PROGRAM + offset, operand, 1)
        machine.put(INSTANCE + OFFSET_FIELD, value["offset"], 2)
        machine.put(INSTANCE + SCRIPT_FIELD, PROGRAM)
        machine.follow.update((READ_BYTE_ADDRESS, READ_HALFWORD_ADDRESS, BRANCH_ADDRESS))
    if mode == REGISTER:
        bind_package(machine, value)
        machine.store(INSTANCE + REGISTERS_FIELD, bytes(value["registers"]))
        machine.map_zero(PARENT, 4)
        machine.put(INSTANCE + PARENT_FIELD, PARENT if value["parent"] else 0)
    elif mode == FINISHED:
        machine.map_zero(TIMELINE, TIMELINE_BYTES)
        machine.put(TIMELINE + TRACK_COUNT_FIELD, value["tracks"])
        machine.put(TIMELINE + ACTIVE_TRACKS_FIELD, value["active"])
        machine.put(INSTANCE + TIMELINE_FIELD, TIMELINE)
    elif mode == FREE:
        machine.put(INSTANCE + TIMELINE_FIELD, value["timeline"])
    elif mode == TICK:
        machine.put(INSTANCE + TIMELINE_FIELD, TIMELINE)


def record(calls: list[tuple[int, int]], problems: list[str], kind: int, expected_a0: int | None = INSTANCE,
           arg_register: int | None = None):
    def stub(machine: Machine) -> None:
        calls.append((kind, machine.r[arg_register] if arg_register is not None else 0))
        if expected_a0 is not None and machine.r[4] != expected_a0:
            problems.append(f"{CALL_NAMES[kind]} passed $a0 {machine.r[4]:#x}, expected {expected_a0:#x}")
        if kind == LOADED and machine.r[6] != 0:
            problems.append(f"load passed $a2 {machine.r[6]:#x}, expected 0")
    return stub


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    prepare(machine, value)
    calls: list[tuple[int, int]] = []
    problems: list[str] = []
    machine.stub(LOAD_ADDRESS, record(calls, problems, LOADED, arg_register=5))
    machine.stub(TICK_ADDRESS, record(calls, problems, TICKED, arg_register=5))
    machine.stub(FREE_ADDRESS, record(calls, problems, FREED, value.get("timeline", 0)))
    machine.stub(UPDATE_ADDRESS, record(calls, problems, UPDATED))
    machine.stub(INITIALIZE_ADDRESS, record(calls, problems, INITIALIZED))
    machine.stub(ENVIRONMENT_ADDRESS, record(calls, problems, REFRESHED, None))
    machine.r[4] = INSTANCE
    machine.execute(entry)

    values: list[int] = [s32(machine.r[2])]
    inputs: list[int] = [mode]
    if mode in PROGRAM_MODES:
        values.append(machine.load(INSTANCE + OFFSET_FIELD, 2))
        inputs += [value["offset"], len(value["placed"]), *(item for pair in value["placed"] for item in pair)]
    if mode != FINISHED:
        values += [len(calls), *(item for call in calls for item in call)]
    if mode == REGISTER:
        count = value["count"]
        inputs += [*value["registers"], 1 if value["parent"] else 0, *value["ids"],
                   0 if count is None else 1, 0 if count is None else count]
    elif mode == FINISHED:
        inputs += [value["active"]]
    elif mode == FREE:
        values.append(machine.load(INSTANCE + TIMELINE_FIELD))
        inputs += [value["timeline"]]
    return {"values": values, "problems": problems, "stores": list(machine.stores)}, inputs


def names(value: dict, count: int) -> tuple[str, ...]:
    mode = value["mode"]
    head = ("result", "offset") if mode in PROGRAM_MODES else ("result",)
    if mode == FINISHED:
        return head[:count]
    calls = tuple(f"call {index // 2} {'kind' if index % 2 == 0 else 'argument'}"
                  for index in range(max(0, count - len(head) - 1 - (mode == FREE))))
    return (*head, "calls", *calls, *(("timeline",) if mode == FREE else ()))[:count]


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    if not native:
        return ["native printed nothing"]
    problems = [f"native refused ({native[0]})"] if native[0] else []
    problems += observation["problems"]
    ours = observation["values"]
    theirs = native[1:]
    problems += value_problems(names(value, len(ours)), ours, theirs)
    return problems + store_problems(observation["stores"], allowed_stores(value))


def describe(image, results) -> list[str]:
    if not results:
        return []
    mode = results[0][0]["mode"]
    if mode == REGISTER:
        remapped = sum(1 for value, observation, _ in results
                       if observation["values"][-1] != value["registers"][value["operands"][0]])
        return [f"{remapped} of {len(results)} vectors loaded a remapped timeline"]
    if mode == FINISHED:
        taken = sum(1 for value, observation, _ in results if not value["active"])
        return [f"{taken} of {len(results)} vectors branched on a finished timeline"]
    if mode == FREE:
        freed = sum(1 for _, observation, _ in results if observation["values"][1])
        return [f"{freed} of {len(results)} vectors freed a timeline"]
    loads = sorted({observation["values"][-1] for _, observation, _ in results})
    if mode in (IMMEDIATE, TICK):
        return [f"{len(loads)} distinct {'timelines' if mode == IMMEDIATE else 'child offsets'} passed"]
    return [f"original results {sorted({observation['values'][0] for _, observation, _ in results})}"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return case_globals(function, entry, end, mutants, driver=DRIVER,
                        vector=lambda rng, number: vector(rng, number, mode),
                        run=lambda machine, value: run(machine, value, entry), check=check, describe=describe)
