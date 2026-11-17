"""Shared fixtures only; execution is the repository's differential.Machine."""

from __future__ import annotations

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MODULE_BASES,
    Fault,
    Machine,
    printed_problems,
)

MODULES = (r"MENU\MENU2.PRG",)
MODULE_BASES[MODULES[0]] = 0x80102800
SOURCES = (
    r"Native\Game\src\Menu\BattleAbilitiesUnlock.cpp",
    r"tools\port\differentials\menu2_ability_foundation_driver.cpp",
)
INCLUDES = (r"Native\Tests\include",)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
VECTORS = 1024
MUTANT_VECTORS = 256
MODULE_SHA256 = "06945720517251a1fc7eb8e877240d28f428c3e56eda0786604cad180f36a176"

ACTORS = 0x800F19FC
ACTOR = 0x80200000
PROGRESS = 0x80060040
ART_REQUIREMENTS = 0x8004FDE4
ABILITY_REQUIREMENTS = 0x8004FE5C
ACTIONS = 0x8004B9DC
ACTION_STRIDE = 52
BOX_FLAGS = 0x800F5130
CONTEXT = 0x800F5168
INPUT = 0x8005E1D0
MENU_MEMORY = 0x800F4EE8
CURSOR = 0x80060021
REMAINING_CHAIN = 0x80105100
REMAINING_DEFENSE = 0x80105110
CHAIN_COUNT = 0x8010511B
DEFENSE_COUNT = 0x8010511C
PHASE = 0x8010511D
TIMEOUT = 0x8010511E
FIRST_VISIT = 0x8010511F
ITEM = 0x80210000
ABILITY_TEXT = 0x80104690
MENU_TEXT = 0x80104ED8
DESCRIPTION_LOOKUP = 0x80105078
CHAIN = (24, 25, 34, 37, 35, 30, 26, 27, 32, 29, 31)
DEFENSE = (47, 48, 49, 50, 41, 42, 51, 52, 44, 45, 53)
STACK = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)


def case_settings() -> dict:
    return {
        "MODULES": MODULES, "SOURCES": SOURCES, "INCLUDES": INCLUDES,
        "COMPILER": COMPILER, "CXX_STANDARD": CXX_STANDARD,
        "VECTORS": VECTORS, "MUTANT_VECTORS": MUTANT_VECTORS,
    }


def validate(machine: Machine) -> None:
    image = machine.images[0]
    if image.sha256(image.base, image.base + len(image.data)) != MODULE_SHA256:
        raise Fault("MENU2.PRG does not match the reviewed canonical image")
    if machine.bytes(0x80105060, 11) != bytes(CHAIN):
        raise Fault("canonical chain reward table differs")
    if machine.bytes(0x8010506C, 11) != bytes(DEFENSE):
        raise Fault("canonical defense reward table differs")


def initial_flags(seed: int, mask: int) -> list[int]:
    flags = [(seed ^ (index * 0x9E3779B9)) & 0xFFFF7FFF for index in range(256)]
    for bit, action in enumerate((*CHAIN, *DEFENSE)):
        if mask & (1 << bit):
            flags[action] |= 0x8000
    return flags


def setup(machine: Machine, seed: int, mask: int = 0) -> None:
    validate(machine)
    machine.map_zero(PROGRESS, 36)
    machine.map_zero(ART_REQUIREMENTS, 120)
    machine.map_zero(ABILITY_REQUIREMENTS, 300)
    machine.map_zero(ACTIONS, 256 * ACTION_STRIDE)
    for action, flags in enumerate(initial_flags(seed, mask)):
        machine.put(ACTIONS + action * ACTION_STRIDE + 12, flags)
    machine.map_zero(CONTEXT, 56)
    machine.put(CONTEXT + 40, 0x80310000)
    machine.put(CONTEXT + 44, 0x80320000)
    machine.map_zero(INPUT, 4)
    machine.put(BOX_FLAGS, 0)


def flag_values(machine: Machine) -> list[int]:
    return [machine.load(ACTIONS + action * ACTION_STRIDE + 12)
            for action in range(256)]


def flag_spans() -> list[tuple[int, int]]:
    return [(ACTIONS + action * ACTION_STRIDE + 12,
             ACTIONS + action * ACTION_STRIDE + 16) for action in range(256)]


def observation(machine: Machine, values: list[int], calls: list[list[int]],
                allowed: list[tuple[int, int]], names: list[str] | None = None) -> dict:
    output = [*values, len(calls)]
    for call in calls:
        output.extend((len(call), *call))
    labels = list(names or [])
    labels.extend(f"field[{index}]" for index in range(len(labels), len(values)))
    labels.append("call count")
    for index, call in enumerate(calls):
        labels.extend([f"call[{index}] size",
                       *(f"call[{index}][{word}]" for word in range(len(call)))])
    return {"values": output, "names": labels, "stores": list(machine.stores),
            "allowed": [STACK, *allowed], "calls": calls}


def check(image, vector, observed, native, reference) -> list[str]:
    if len(native) != len(observed["values"]):
        return [f"field count native={len(native)} original={len(observed['values'])}; "
                f"prefix native={native[:7]} original={observed['values'][:7]}"]
    # Keep diagnostics bounded; comparisons and store-span checks remain complete.
    return [problem[:800] for problem in
            printed_problems(observed, native, observed["allowed"])]


def message_calls(machine: Machine, calls: list[list[int]]) -> None:
    machine.stub(0x800C8E04, lambda m: calls.append([1, m.r[4]]))
    machine.stub(0x800FFC04, lambda m: calls.append([
        2, m.r[4], m.load(CONTEXT + 40), m.load(CONTEXT + 44)]))


def item_values(machine: Machine) -> list[int]:
    return [machine.load(ITEM, 1), machine.load(ITEM + 6, 1),
            machine.load(ITEM + 24, 2, signed=True)]


def describe(image, results) -> list[str]:
    calls = {call[0] for _, observed, _ in results for call in observed["calls"]}
    return [f"canonical MENU2.PRG sha256={MODULE_SHA256}",
            f"observed callback kinds={sorted(calls)}; stores checked by span"]
