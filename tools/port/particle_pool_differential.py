"""Shared harness for the particle pool and force differentials (BATTLE.PRG 5BF94.c:6777-6880 and
7826-7828, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects core that
particle_pool_driver.cpp drives. Particles 0-7 are 0x78-byte records at fixed, contiguous
addresses (the pool's block) and the effect instance is one record; both runs print every link
as a record number (-1 for null), so the lists each run leaves are compared. The heap allocator
(func_80043E3C), the free (func_80043C60), the texture animation clear (func_800D6CCC) and the
effect diagnostic (func_800CE644) are stubs recording their arguments.

The gravity case compares all four words: the core sets x, y and z, and the driver copies the
fourth word as ParticlePoolLive.cpp does.

Native differences outside the compared domain (covered by ParticlePoolTests.cpp): the first
reference refuses a count below 1, where the original reserved a negative size, and links a
single particle alone, where the original linked a second one past the block; a release with no
reference is refused, where the original counted below zero; and an allocation from an empty
free list is refused with the diagnostic, where the original wrote through the null particle.
Vectors therefore acquire at least two particles, release referenced pools and allocate from
non-empty free lists.
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
DRIVER = "particle_pool_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\ParticlePool.cpp",)

(
    RESET,
    ACQUIRE,
    RELEASE,
    ALLOCATE,
    RELEASE_PARTICLE,
    RELEASE_ALL,
    GRAVITY,
    DRAG,
) = range(8)
LIST_MODES = (ALLOCATE, RELEASE_PARTICLE, RELEASE_ALL)

CAPACITY = 0x800F55F0
FREE = 0x800F55F4
USED = 0x800F55F8
POOL = 0x800F55FC
REFERENCES = 0x800F5600
DRAG_FORCE = 0x800F5618
GRAVITY_FORCE = 0x800F5620
GRAVITY_WORDS = 4

PARTICLES = 0x80500000
PARTICLE_BYTES = 0x78
RECORDS = 8
PREVIOUS = 0x00
NEXT = 0x04
TEXTURE = 0x3C
AGE = 0x77
INSTANCE = 0x80510000
COUNT = 0x08
FIRST = 0x18
VALUE = 0x80520000

HEAP_ALLOCATE = 0x80043E3C
HEAP_FREE = 0x80043C60
DIAGNOSTIC = 0x800CE644
TEXTURE_CLEAR = 0x800D6CCC

NULL = -1
OTHER = 999
ALLOCATE_CALL, FREE_CALL, TEXTURE_CALL, DIAGNOSTIC_CALL = range(4)
POOL_NAMES = ["references", "block", "free", "capacity", "used"]
LINK_NAMES = [f"particle {index} {link}" for link in ("previous", "next") for index in range(RECORDS)]
AGE_NAMES = [f"particle {index} age" for index in range(RECORDS)]


def particle(index: int) -> int:
    return PARTICLES + index * PARTICLE_BYTES


ADDRESSES = {particle(index): index for index in range(RECORDS)}


def ref(address: int) -> int:
    """The record number at `address`: -1 for null, OTHER for an address no record has."""
    return NULL if address == 0 else ADDRESSES.get(address, OTHER)


def address_of(record: int) -> int:
    return 0 if record == NULL else particle(record)


def word(rng: random.Random) -> int:
    return s32(rng.choice((0, 1, -1, 2, 0x7F, 0x80, 0xFF, 0x100, 0x7FFFFFFF, -0x80000000, rng.getrandbits(32))))


def stale(rng: random.Random) -> int:
    return rng.choice((NULL, rng.randrange(RECORDS)))


def walk(first: int, nexts: list[int]) -> list[int]:
    """The records a list links forward, in order."""
    order: list[int] = []
    while first != NULL:
        order.append(first)
        first = nexts[first]
    return order


def link(order: list[int], previous: list[int], nexts: list[int]) -> int:
    """Links `order` doubly into `previous` and `nexts`; the first record, or -1."""
    for at, record in enumerate(order):
        previous[record] = order[at - 1] if at else NULL
        nexts[record] = order[at + 1] if at + 1 < len(order) else NULL
    return order[0] if order else NULL


def pool_vector(rng: random.Random, value: dict) -> None:
    value["pool"] = [word(rng), stale(rng), stale(rng), word(rng), word(rng)]
    value["previous"] = [stale(rng) for _ in range(RECORDS)]
    value["next"] = [stale(rng) for _ in range(RECORDS)]


def list_vector(rng: random.Random, value: dict, mode: int) -> None:
    pool_vector(rng, value)
    records = rng.sample(range(RECORDS), RECORDS)
    split = rng.randrange(1 if mode == RELEASE_PARTICLE else 0, RECORDS + (0 if mode == ALLOCATE else 1))
    listed, spare = records[:split], records[split:]
    value["first"] = link(listed, value["previous"], value["next"])
    if mode == ALLOCATE:
        free = spare[: rng.randrange(1, len(spare) + 1)]
        for at, record in enumerate(free):
            value["next"][record] = free[at + 1] if at + 1 < len(free) else NULL
        value["pool"][2] = free[0]
    else:
        value["pool"][2] = rng.choice((NULL, *spare)) if spare else NULL
    value["count"] = rng.choice((len(listed), len(listed), 0, 0x7F, 0x80, 0x90, 0xFF, rng.randrange(256)))
    value["age"] = [rng.randrange(256) for _ in range(RECORDS)]
    if mode == RELEASE_PARTICLE:
        value["target"] = rng.choice(listed)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode, "number": number}
    if mode in (RESET, ACQUIRE, RELEASE):
        pool_vector(rng, value)
    if mode == ACQUIRE:
        if rng.random() < 0.6:
            value["pool"][0] = 0
            value["count"] = rng.randrange(2, RECORDS + 1)
        else:
            # A later reference ignores the count.
            value["pool"][0] = value["pool"][0] or rng.choice((1, -1))
            value["count"] = word(rng)
    elif mode == RELEASE:
        value["pool"][0] = rng.choice((1, 1, 1, 2, 3, 0x7FFFFFFF, rng.randrange(1, 1 << 31)))
        value["pool"][1] = rng.choice((0, 0, NULL))
    elif mode in LIST_MODES:
        list_vector(rng, value, mode)
    elif mode == GRAVITY:
        value["gravity"] = [word(rng) for _ in range(GRAVITY_WORDS)]
        value["old"] = [word(rng) for _ in range(GRAVITY_WORDS + 1)]
    elif mode == DRAG:
        value["drag"] = [word(rng), word(rng)]
        value["old"] = word(rng)
    return value


def stubs(machine: Machine, calls: list[list[int]]) -> None:
    def allocate(machine: Machine) -> None:
        calls.append([ALLOCATE_CALL, s32(machine.r[4]), 0, 0, 0])
        machine.r[2] = PARTICLES

    def texture(machine: Machine) -> None:
        cleared = machine.r[4] - TEXTURE
        calls.append([TEXTURE_CALL, ref(cleared) if (machine.r[4] - PARTICLES) % PARTICLE_BYTES == TEXTURE else OTHER,
                      0, 0, 0])

    machine.stub(HEAP_ALLOCATE, allocate)
    machine.stub(HEAP_FREE, lambda machine: calls.append([FREE_CALL, ref(machine.r[4]), 0, 0, 0]))
    machine.stub(TEXTURE_CLEAR, texture)
    machine.stub(DIAGNOSTIC, lambda machine: calls.append([DIAGNOSTIC_CALL, s32(machine.r[4]), 0, 0, 0]))


def put_pool(machine: Machine, value: dict) -> None:
    references, block, free, capacity, used = value["pool"]
    machine.put(REFERENCES, references)
    machine.put(POOL, address_of(block))
    machine.put(FREE, address_of(free))
    machine.put(CAPACITY, capacity)
    machine.put(USED, used)
    machine.map_zero(PARTICLES, RECORDS * PARTICLE_BYTES)
    for index in range(RECORDS):
        machine.put(particle(index) + PREVIOUS, address_of(value["previous"][index]))
        machine.put(particle(index) + NEXT, address_of(value["next"][index]))


def pool_values(machine: Machine) -> list[int]:
    return [
        s32(machine.load(REFERENCES)),
        ref(machine.load(POOL)),
        ref(machine.load(FREE)),
        s32(machine.load(CAPACITY)),
        s32(machine.load(USED)),
    ]


def links(machine: Machine) -> list[int]:
    return [ref(machine.load(particle(index) + offset)) for offset in (PREVIOUS, NEXT) for index in range(RECORDS)]


def pool_inputs(value: dict) -> list[int]:
    return [*value["pool"], *value["previous"], *value["next"]]


def run_pool(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    put_pool(machine, value)
    stubs(machine, calls)
    machine.r[4] = value.get("count", 0) & 0xFFFFFFFF
    machine.execute(entry)
    inputs = pool_inputs(value)
    if value["mode"] == ACQUIRE:
        inputs.append(value["count"])
    return [*pool_values(machine), *links(machine)], [*POOL_NAMES, *LINK_NAMES], inputs


def run_list(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    mode = value["mode"]
    put_pool(machine, value)
    machine.map_zero(INSTANCE, 0x20)
    machine.put(INSTANCE + COUNT, value["count"], 1)
    machine.put(INSTANCE + FIRST, address_of(value["first"]))
    for index in range(RECORDS):
        machine.put(particle(index) + AGE, value["age"][index], 1)
    stubs(machine, calls)
    if mode == RELEASE_PARTICLE:
        machine.r[4], machine.r[5] = particle(value["target"]), INSTANCE
    else:
        machine.r[4] = INSTANCE
    machine.execute(entry)
    result = [ref(machine.r[2])] if mode == ALLOCATE else []
    values = [
        *result,
        *pool_values(machine),
        ref(machine.load(INSTANCE + FIRST)),
        machine.load(INSTANCE + COUNT, 1),
        *links(machine),
        *(machine.load(particle(index) + AGE, 1) for index in range(RECORDS)),
    ]
    names = [*(["result"] if result else []), *POOL_NAMES, "first", "count", *LINK_NAMES, *AGE_NAMES]
    inputs = [*pool_inputs(value), value["first"], value["count"], *value["age"]]
    if mode == RELEASE_PARTICLE:
        inputs.append(value["target"])
    return values, names, inputs


def run_forces(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    for index, old in enumerate(value["old"][:GRAVITY_WORDS] if value["mode"] == GRAVITY else []):
        machine.put(GRAVITY_FORCE + index * 4, old)
    machine.put(DRAG_FORCE, value["old"][-1] if value["mode"] == GRAVITY else value["old"])
    source = value["gravity"] if value["mode"] == GRAVITY else value["drag"]
    machine.map_zero(VALUE, 0x20)
    for index, item in enumerate(source):
        machine.put(VALUE + index * 4, item)
    machine.r[4] = VALUE
    machine.execute(entry)
    if value["mode"] == GRAVITY:
        values = [s32(machine.load(GRAVITY_FORCE + index * 4)) for index in range(GRAVITY_WORDS)]
        names = ["gravity x", "gravity y", "gravity z", "gravity word 3"]
        return values, names, [*value["gravity"], *value["old"]]
    return [s32(machine.load(DRAG_FORCE))], ["drag"], [*value["drag"], value["old"]]


RUNNERS = {
    **dict.fromkeys((RESET, ACQUIRE, RELEASE), run_pool),
    **dict.fromkeys(LIST_MODES, run_list),
    **dict.fromkeys((GRAVITY, DRAG), run_forces),
}


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    recorded: list[list[int]] = []
    values, names, inputs = RUNNERS[mode](machine, value, entry, recorded)
    observation = recorded_observation(machine, values, names, recorded)
    return observation, [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    """The spans the original may store to: the stack and the mode's globals and records."""
    stack = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)
    if value["mode"] == GRAVITY:
        return [stack, (GRAVITY_FORCE, GRAVITY_FORCE + GRAVITY_WORDS * 4)]
    if value["mode"] == DRAG:
        return [stack, (DRAG_FORCE, DRAG_FORCE + 4)]
    spans = [stack, (CAPACITY, REFERENCES + 4)]
    spans += [(particle(index), particle(index) + NEXT + 4) for index in range(RECORDS)]
    if value["mode"] in LIST_MODES:
        spans += [(INSTANCE + COUNT, INSTANCE + COUNT + 1), (INSTANCE + FIRST, INSTANCE + FIRST + 4)]
        spans += [(particle(index) + AGE, particle(index) + AGE + 1) for index in range(RECORDS)]
    return spans


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    vectors = [value for value, _, _ in results]
    modes = {value["mode"] for value in vectors}
    if modes == {ACQUIRE}:
        first = sum(1 for value in vectors if value["pool"][0] == 0)
        return [f"{first} of {len(vectors)} vectors were the first reference"]
    if modes and modes <= set(LIST_MODES):
        high = sum(1 for value in vectors if value["count"] >= 0x80)
        listed = sum(len(walk(value["first"], value["next"])) for value in vectors)
        return [f"{high} of {len(vectors)} vectors held a count >= 0x80; {listed} particles listed"]
    return []

def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
