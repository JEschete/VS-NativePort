"""Shared harness for the effect instance lifecycle differentials (BATTLE.PRG 5BF94.c:5074-5160,
5548 and 5593-5665, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects core that
effect_instances_driver.cpp drives. Instances 0-6, renderer slots 0-5 and the slots' renderer
states are records at fixed addresses, and both runs print every link as a record number (-1 for
null), so the chains each run leaves are compared. The allocator (func_80043E3C), free
(func_80043C60), particle release (func_800D2904), token release (func_800CF514), the instance
start (func_800CE714, from the creations) and the renderers are stubs recording their arguments;
the primary destruction's detach (func_800CF70C) runs as the original.

The instance start compares every word of the 0xD5C-byte record: the driver writes the PS1 offsets
of the Effects_EffectInstance fields EffectInstancesLive.cpp writes.

Native differences outside the compared domain (covered by EffectInstancesTests.cpp): a full heap
is refused where the original wrote through the null record, the destruction of an instance from
an empty chain ends it where the original read address 0, the detach of a slot the instance does
not hold is refused where the original searched past the chain's end, and a phase the runtime
never sets is refused. Vectors therefore allocate, destroy from chains of at least one instance
(the instance in the chain or outside it), detach held slots and set phases 2-4.
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
DRIVER = "effect_instances_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\EffectInstances.cpp",)

(
    INIT,
    CREATE_PRIMARY,
    CREATE_SECONDARY,
    DESTROY_PRIMARY,
    DESTROY_SECONDARY,
    ATTACH,
    DETACH,
    PHASE,
) = range(8)
CREATES = (CREATE_PRIMARY, CREATE_SECONDARY)
DESTROYS = (DESTROY_PRIMARY, DESTROY_SECONDARY)
PRIMARIES = (CREATE_PRIMARY, DESTROY_PRIMARY)

PRIMARY = 0x800F53B8
SECONDARY = 0x800F54B0
INSTANCES = 0x80500000
INSTANCE_STRIDE = 0x1000
INSTANCE_BYTES = 0xD5C
INSTANCE_RECORDS = 7
# The chains link instances 0-5; instance 6 is the new one or one outside the chain.
OUTSIDE = 6
SLOTS = 0x80510000
STATES = 0x80511000
SLOT_STRIDE = 0x10
SLOT_RECORDS = 6
SLOT_BYTES = 0xC
# Attach allocates slot 5; the instance holds some of slots 0-4.
FRESH = 5
SPAWN = 0x80512000
SPAWN_BYTES = 0x24
ENDPOINTS = SPAWN + 0x100
ENDPOINT_STRIDE = 0x10
RENDERERS = 0x80520000
RENDERER_STRIDE = 0x40

PRIORITY = 9
TOKEN = 0x14
RENDERER = 0xD50
EFFECT_ID = 0xD54
PHASE_FIELD = 0xD56
PARAMETER = 0xD58
SLOT_RENDERER = 4
SLOT_STATE = 8
SPAWN_PRIORITY = 0x10

ALLOCATE = 0x80043E3C
FREE = 0x80043C60
START = 0x800CE714
PARTICLES = 0x800D2904
RELEASE_TOKEN = 0x800CF514
DETACH_ADDRESS = 0x800CF70C

NULL = -1
SLOT_REF = 100
STATE_REF = 200
SPAWN_REF = 300
OTHER = 999
ALLOCATE_CALL, PARTICLES_CALL, RENDERER_CALL, FREE_CALL, TOKEN_CALL, START_CALL = range(
    6
)


def instance(index: int) -> int:
    return INSTANCES + index * INSTANCE_STRIDE


def slot(index: int) -> int:
    return SLOTS + index * SLOT_STRIDE


def state(index: int) -> int:
    return STATES + index * SLOT_STRIDE


def renderer(index: int) -> int:
    return RENDERERS + index * RENDERER_STRIDE


ADDRESSES = {
    **{instance(index): index for index in range(INSTANCE_RECORDS)},
    **{slot(index): SLOT_REF + index for index in range(SLOT_RECORDS)},
    **{state(index): STATE_REF + index for index in range(SLOT_RECORDS)},
    SPAWN: SPAWN_REF,
}
RENDERER_INDEXES = {renderer(index): index for index in range(SLOT_RECORDS)}
INSTANCE_NAMES = [f"instance {index} next" for index in range(INSTANCE_RECORDS)]
SLOT_NAMES = [f"slot {index} next" for index in range(SLOT_RECORDS)]


def ref(address: int) -> int:
    """The record number at `address`: -1 for null, OTHER for an address no record has."""
    return NULL if address == 0 else ADDRESSES.get(address, OTHER)


def address_of(record: int) -> int:
    if record == NULL:
        return 0
    if record < SLOT_REF:
        return instance(record)
    return slot(record - SLOT_REF)


def walk(first: int, nexts: list[int], base: int = 0) -> list[int]:
    """The records a chain links, in order."""
    order: list[int] = []
    while first != NULL:
        order.append(first)
        first = nexts[first - base]
    return order


def chain(
    rng: random.Random, records: int, minimum: int, base: int = 0
) -> tuple[int, list[int]]:
    """A chain over some of `records` in random order: its first record and every record's next.
    Records outside the chain hold a stale link the originals never read."""
    order = rng.sample(range(records), rng.randrange(minimum, records + 1))
    nexts = [rng.choice((NULL, base + rng.randrange(records))) for _ in range(records)]
    for at, record in enumerate(order):
        nexts[record] = base + order[at + 1] if at + 1 < len(order) else NULL
    return (base + order[0] if order else NULL), nexts


def priority_byte(rng: random.Random) -> int:
    return rng.choice(
        (0, 1, 2, 3, 4, 0x7F, 0x80, 0x81, 0xFF, rng.randrange(8), rng.randrange(256))
    )


def spawn_priority(rng: random.Random, priorities: list[int]) -> int:
    near = rng.choice(priorities)
    extremes = (
        0,
        -1,
        0x80,
        0xFF,
        0x100,
        0x104,
        0x17F,
        -0x80,
        rng.randrange(256),
        rng.randrange(1 << 32),
    )
    return s32(rng.choice((near, near, near - 1, near + 1, *extremes)))


def instance_vector(rng: random.Random, value: dict, minimum: int) -> None:
    value["head"], nexts = chain(rng, OUTSIDE, minimum)
    value["next"] = [*nexts, rng.choice((NULL, rng.randrange(OUTSIDE)))]
    value["priority"] = [priority_byte(rng) for _ in range(INSTANCE_RECORDS)]
    value["token"] = [rng.randrange(256) for _ in range(INSTANCE_RECORDS)]
    value["beside"] = [rng.randrange(256) for _ in range(INSTANCE_RECORDS)]


def renderer_vector(
    rng: random.Random, value: dict, records: int, minimum: int
) -> None:
    value["renderers"], nexts = chain(rng, records, minimum, SLOT_REF)
    value["slot_next"] = nexts + [rng.choice((NULL, SLOT_REF))] * (
        SLOT_RECORDS - records
    )
    value["state"] = [rng.choice((0, state(index))) for index in range(SLOT_RECORDS)]


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode, "number": number}
    if mode == INIT:
        value["spawn"] = list(rng.randbytes(SPAWN_BYTES))
        value["endpoints"] = [rng.randrange(1 << 32) for _ in range(12)]
        value["record"] = list(rng.randbytes(INSTANCE_BYTES))
    elif mode in CREATES:
        instance_vector(rng, value, 0)
        value["spawn_priority"] = spawn_priority(rng, value["priority"][:OUTSIDE])
    elif mode in DESTROYS:
        instance_vector(rng, value, 1)
        chained = walk(value["head"], value["next"])
        value["target"] = rng.choice((*chained, *chained, OUTSIDE))
        renderer_vector(rng, value, SLOT_RECORDS, 0)
    elif mode == ATTACH:
        renderer_vector(rng, value, FRESH, 0)
        value["fresh"] = [rng.randrange(1 << 32) for _ in range(3)]
        value["renderer"] = rng.randrange(SLOT_RECORDS)
        value["parameters"] = s32(
            rng.choice((0, 1, 2, -1, 0x7FFFFFFF, rng.randrange(1 << 32)))
        )
    elif mode == DETACH:
        renderer_vector(rng, value, SLOT_RECORDS, 1)
        value["target"] = rng.choice(
            walk(value["renderers"], value["slot_next"], SLOT_REF)
        )
    else:
        value["phase"] = rng.choice((2, 3, 4))
        value["old"] = [
            rng.randrange(1 << 16),
            rng.randrange(1 << 16),
            rng.randrange(1 << 32),
        ]
    return value


def stubs(machine: Machine, calls: list[list[int]], fresh: int) -> None:
    def allocate(machine: Machine) -> None:
        calls.append([ALLOCATE_CALL, machine.r[4], 0, 0, 0])
        machine.r[2] = fresh

    def recorder(kind: int, convert):
        return lambda machine: calls.append([kind, convert(machine.r[4]), 0, 0, 0])

    def renderer_stub(index: int):
        return lambda machine: calls.append(
            [RENDERER_CALL, ref(machine.r[4]), index, machine.r[5], s32(machine.r[6])]
        )

    machine.stub(ALLOCATE, allocate)
    machine.stub(
        START,
        lambda machine: calls.append(
            [START_CALL, ref(machine.r[4]), ref(machine.r[5]), 0, 0]
        ),
    )
    machine.stub(FREE, recorder(FREE_CALL, ref))
    machine.stub(PARTICLES, recorder(PARTICLES_CALL, ref))
    machine.stub(RELEASE_TOKEN, recorder(TOKEN_CALL, s32))
    for index in range(SLOT_RECORDS):
        machine.stub(renderer(index), renderer_stub(index))


def put_instances(machine: Machine, value: dict, head: int) -> None:
    machine.map_zero(INSTANCES, INSTANCE_RECORDS * INSTANCE_STRIDE)
    machine.put(PRIMARY, 0)
    machine.put(SECONDARY, 0)
    machine.put(head, address_of(value["head"]))
    for index in range(INSTANCE_RECORDS):
        machine.put(instance(index), address_of(value["next"][index]))
        machine.put(instance(index) + PRIORITY, value["priority"][index], 1)
        machine.put(instance(index) + TOKEN, value["token"][index], 1)
        machine.put(instance(index) + TOKEN + 1, value["beside"][index], 1)


def put_slots(machine: Machine, value: dict, owner: int) -> None:
    machine.map_zero(SLOTS, SLOT_RECORDS * SLOT_STRIDE)
    machine.map_zero(STATES, SLOT_RECORDS * SLOT_STRIDE)
    machine.put(instance(owner) + RENDERER, address_of(value["renderers"]))
    for index in range(SLOT_RECORDS):
        machine.put(slot(index), address_of(value["slot_next"][index]))
        machine.put(slot(index) + SLOT_RENDERER, renderer(index))
        machine.put(slot(index) + SLOT_STATE, value["state"][index])


def instance_links(machine: Machine) -> list[int]:
    return [ref(machine.load(instance(index))) for index in range(INSTANCE_RECORDS)]


def slot_links(machine: Machine) -> list[int]:
    return [ref(machine.load(slot(index))) for index in range(SLOT_RECORDS)]


def state_flags(value: dict) -> list[int]:
    return [int(word != 0) for word in value["state"]]


def run_init(
    machine: Machine, value: dict, entry: int, calls: list[list[int]]
) -> tuple:
    machine.map_zero(INSTANCES, INSTANCE_STRIDE)
    machine.map_zero(SPAWN, 0x200)
    machine.store(instance(0), bytes(value["record"]))
    machine.store(SPAWN, bytes(value["spawn"]))
    for index in range(4):
        endpoint = ENDPOINTS + index * ENDPOINT_STRIDE
        machine.put(SPAWN + index * 4, endpoint)
        for word in range(3):
            machine.put(endpoint + word * 4, value["endpoints"][index * 3 + word])
    machine.r[4], machine.r[5] = SPAWN, instance(0)
    machine.execute(entry)
    spawn = bytes(value["spawn"])
    record = bytes(value["record"])
    word = lambda data, offset: int.from_bytes(data[offset : offset + 4], "little")  # noqa: E731
    inputs = [
        s32(word(spawn, 0x10)),
        s32(word(spawn, 0x18)),
        spawn[0x20],
        word(spawn, 0x1C),
        word(spawn, 0x14),
        *value["endpoints"],
        *(word(record, offset) for offset in range(0, INSTANCE_BYTES, 4)),
    ]
    values = [
        machine.load(instance(0) + offset) for offset in range(0, INSTANCE_BYTES, 4)
    ]
    names = [f"word {offset:#x}" for offset in range(0, INSTANCE_BYTES, 4)]
    return values, names, inputs


def run_phase(
    machine: Machine, value: dict, entry: int, calls: list[list[int]]
) -> tuple:
    machine.map_zero(INSTANCES, INSTANCE_STRIDE)
    effect, phase, parameter = value["old"]
    machine.put(instance(0) + EFFECT_ID, effect, 2)
    machine.put(instance(0) + PHASE_FIELD, phase, 2)
    machine.put(instance(0) + PARAMETER, parameter)
    machine.r[4], machine.r[5] = value["phase"], instance(0)
    machine.execute(entry)
    values = [
        machine.load(instance(0) + EFFECT_ID, 2),
        machine.load(instance(0) + PHASE_FIELD, 2),
        machine.load(instance(0) + PARAMETER),
    ]
    return values, ["effect id", "phase", "parameter"], [value["phase"], *value["old"]]


def run_create(
    machine: Machine, value: dict, entry: int, calls: list[list[int]]
) -> tuple:
    head = PRIMARY if value["mode"] in PRIMARIES else SECONDARY
    put_instances(machine, value, head)
    machine.map_zero(SPAWN, 0x200)
    machine.put(SPAWN + SPAWN_PRIORITY, value["spawn_priority"])
    stubs(machine, calls, instance(OUTSIDE))
    machine.r[4] = SPAWN
    machine.execute(entry)
    started = int(
        calls
        == [
            [ALLOCATE_CALL, INSTANCE_BYTES, 0, 0, 0],
            [START_CALL, SPAWN_REF, OUTSIDE, 0, 0],
        ]
    )
    calls.clear()
    values = [
        ref(machine.r[2]),
        ref(machine.load(head)),
        *instance_links(machine),
        started,
    ]
    inputs = [
        value["head"],
        *value["next"],
        *value["priority"],
        value["spawn_priority"],
    ]
    return values, ["result", "first", *INSTANCE_NAMES, "started"], inputs


def run_destroy(
    machine: Machine, value: dict, entry: int, calls: list[list[int]]
) -> tuple:
    head = PRIMARY if value["mode"] in PRIMARIES else SECONDARY
    target = value["target"]
    put_instances(machine, value, head)
    put_slots(machine, value, target)
    stubs(machine, calls, 0)
    machine.follow.add(DETACH_ADDRESS)
    machine.r[4] = instance(target)
    machine.execute(entry)
    values = [
        ref(machine.load(head)),
        *instance_links(machine),
        ref(machine.load(instance(target) + RENDERER)),
        *slot_links(machine),
    ]
    names = ["first", *INSTANCE_NAMES, "renderers", *SLOT_NAMES]
    inputs = [
        value["head"],
        *value["next"],
        *value["token"],
        target,
        value["renderers"],
        *value["slot_next"],
        *state_flags(value),
    ]
    return values, names, inputs


def run_attach(
    machine: Machine, value: dict, entry: int, calls: list[list[int]]
) -> tuple:
    machine.map_zero(INSTANCES, INSTANCE_STRIDE)
    put_slots(machine, value, 0)
    fresh = slot(FRESH)
    for offset, word in zip(
        (0, SLOT_RENDERER, SLOT_STATE), value["fresh"], strict=True
    ):
        machine.put(fresh + offset, word)
    stubs(machine, calls, fresh)
    machine.r[4], machine.r[5], machine.r[6] = (
        instance(0),
        renderer(value["renderer"]),
        value["parameters"],
    )
    machine.execute(entry)
    allocated = int(calls[:1] == [[ALLOCATE_CALL, SLOT_BYTES, 0, 0, 0]])
    del calls[:allocated]
    values = [
        ref(machine.r[2]),
        ref(machine.load(instance(0) + RENDERER)),
        *slot_links(machine),
        RENDERER_INDEXES.get(machine.load(fresh + SLOT_RENDERER), OTHER),
        machine.load(fresh + SLOT_STATE),
        allocated,
    ]
    names = [
        "result",
        "renderers",
        *SLOT_NAMES,
        "new renderer",
        "new state",
        "allocated",
    ]
    inputs = [
        value["renderers"],
        *value["slot_next"],
        value["renderer"],
        value["parameters"],
    ]
    return values, names, inputs


def run_detach(
    machine: Machine, value: dict, entry: int, calls: list[list[int]]
) -> tuple:
    machine.map_zero(INSTANCES, INSTANCE_STRIDE)
    put_slots(machine, value, 0)
    stubs(machine, calls, 0)
    machine.r[4], machine.r[5] = instance(0), slot(value["target"] - SLOT_REF)
    machine.execute(entry)
    values = [0, ref(machine.load(instance(0) + RENDERER)), *slot_links(machine)]
    inputs = [
        value["renderers"],
        *value["slot_next"],
        *state_flags(value),
        value["target"],
    ]
    return values, ["refused", "renderers", *SLOT_NAMES], inputs


RUNNERS = {
    INIT: run_init,
    CREATE_PRIMARY: run_create,
    CREATE_SECONDARY: run_create,
    DESTROY_PRIMARY: run_destroy,
    DESTROY_SECONDARY: run_destroy,
    ATTACH: run_attach,
    DETACH: run_detach,
    PHASE: run_phase,
}


def run(machine: Machine, value: dict, entry: int) -> tuple:
    calls: list[list[int]] = []
    values, names, inputs = RUNNERS[value["mode"]](machine, value, entry, calls)
    return recorded_observation(machine, values, names, calls), [value["mode"], *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    mode = value["mode"]
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if mode == INIT:
        allowed.append((instance(0), instance(0) + INSTANCE_BYTES))
    elif mode == PHASE:
        allowed.append((instance(0) + PHASE_FIELD, instance(0) + PHASE_FIELD + 2))
    elif mode in (ATTACH, DETACH):
        allowed.append((instance(0) + RENDERER, instance(0) + RENDERER + 4))
        allowed += [(slot(index), slot(index) + 4) for index in range(SLOT_RECORDS)]
        if mode == ATTACH:
            allowed.append((slot(FRESH), slot(FRESH) + SLOT_BYTES))
    else:
        allowed.append(
            (PRIMARY, PRIMARY + 4) if mode in PRIMARIES else (SECONDARY, SECONDARY + 4)
        )
        allowed += [
            (instance(index), instance(index) + 4) for index in range(INSTANCE_RECORDS)
        ]
        if mode == DESTROY_PRIMARY:
            owner = instance(value["target"]) + RENDERER
            allowed.append((owner, owner + 4))
    return allowed


def check(
    image, value: dict, observation: dict, native: list[int], reference
) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    if not results:
        return []
    mode = results[0][0]["mode"]
    total = len(results)
    if mode in CREATES:
        first = sum(
            1
            for _, observation, _ in results
            if observation["values"][0] == observation["values"][1]
        )
        high = sum(
            1 for value, _, _ in results if max(value["priority"][:OUTSIDE]) >= 0x80
        )
        return [
            f"{first} of {total} vectors linked the new instance first; {high} chains held a priority >= 0x80"
        ]
    if mode in DESTROYS:
        outside = sum(1 for value, _, _ in results if value["target"] == OUTSIDE)
        held = sum(
            len(walk(value["renderers"], value["slot_next"], SLOT_REF))
            for value, _, _ in results
        )
        return [
            f"{outside} of {total} vectors destroyed an instance outside the chain; {held} renderers held"
        ]
    if mode == DETACH:
        newest = sum(
            1 for value, _, _ in results if value["target"] == value["renderers"]
        )
        return [f"{newest} of {total} vectors detached the newest slot"]
    return []


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
