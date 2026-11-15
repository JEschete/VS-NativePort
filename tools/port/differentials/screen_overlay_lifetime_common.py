"""Shared harness for the screen overlay lifetime differentials (BATTLE.PRG 5BF94.c, #276).

Each case runs one original under the R3000 interpreter against its typed
VS::Effects::ScreenDistortion core (ScreenOverlayLifetime.cpp) in screen_overlay_lifetime_driver.cpp.
The controller pointer (D_800EB9B4) starts null, at the default controller (D_800F4ED8) or at another
controller; both controllers start with random words. Every callee is stubbed and recorded: memset
clears through the interpreter, the disc request allocator returns a queued request or null (a
refused request), and a request's state halfword decides the load poll. The original and the
core are compared as a lifetime: attached when the pointer is set, with the words of the
controller it points at (the default controller when it is null).

The original enqueued a refused (null) request; the core instead completes it without a request
(#282). For a refused vector the original's [enqueue null] call is compared as the core's
[complete refused] call and its stored null request as the completed record. The allocator's
file and the enqueue destination are checked here (SCREFF2.PRG and the secondary overlay slot);
the driver records them without arguments because the core leaves them to the live loader.

Native differences outside the compared domain (covered by ScreenOverlayLifetimeTests.cpp): the
live binding refuses a load without a controller and reports a poll without a request; the
original read through both.
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
DRIVER = "screen_overlay_lifetime_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\ScreenDistortion\ScreenOverlayLifetime.cpp",)

POINTER = 0x800EB9B4
DEFAULTS = 0x800F4ED8
OTHER = 0x80500000
RECORD_BYTES = 0x10
CONTROLLERS = (0, DEFAULTS, OTHER)
OVERLAY_SLOTS = 0x80010000
SECONDARY_SLOT = OVERLAY_SLOTS + 4
# Request records: 1 is the one the allocator hands out, 2 the controller's earlier request and
# 3 (native only) the completed record of a refused request.
QUEUED, EARLIER, COMPLETED = 1, 2, 3
REQUESTS = 0x80510000
REQUEST_ADDRESSES = (0, REQUESTS, REQUESTS + 0x20)
OTHER_REQUEST = 99
SCREFF2_FILE = (135000, 4096)

MEMSET, ALLOCATE, ENQUEUE, FREE, WAIT = (
    0x80026EB4,
    0x80044B10,
    0x80044BC4,
    0x80044B80,
    0x80042C74,
)
ACQUIRE_WORKSPACE, RELEASE_WORKSPACE, RETIRE_PAGES = 0x8007E180, 0x8007E1C0, 0x8007E0A8
WARP, TILES, VIGNETTE = 0x800F9BD8, 0x800F986C, 0x800F9FB8
FOLLOW = (0x800CB678, 0x800CA97C)
(
    CALL_ALLOCATE,
    CALL_ENQUEUE,
    CALL_FREE,
    CALL_WAIT,
    CALL_ACQUIRE,
    CALL_RELEASE,
    CALL_RETIRE,
    CALL_WARP,
    CALL_TILES,
    CALL_VIGNETTE,
    CALL_COMPLETE_REFUSED,
) = range(1, 12)

RESET, LOAD, DRAW, ACQUIRE, RELEASE = range(5)

# (offset, size, signed) of the controller words before the request pointer at +0xC.
FIELDS = (
    (0, 1, False),
    (1, 1, False),
    (2, 2, False),
    (4, 2, False),
    (6, 2, True),
    (8, 4, True),
)
NAMES = (
    "controller",
    "loading",
    "pages reserved",
    "feedback layer",
    "warp scale",
    "warp growth",
    "vignette",
    "request",
)
STATES = (4, 4, 0, 1, 2, 3, 5, -1, 0x104, -0x7FFC)


def word(rng: random.Random) -> int:
    return rng.choice(
        (0, 1, -1, 2, 0x100, 0x10000, -0x80000000, s32(rng.getrandbits(32)))
    )


def record(rng: random.Random) -> list[int]:
    return [
        rng.choice((0, 0, 1, 2, 0xFF, rng.randrange(256))),
        rng.choice((0, 1, 1, 2, 0xFF, rng.randrange(256))),
        rng.choice((0, 0, 1, 0x100, 0xFFFF, rng.randrange(1 << 16))),
        rng.choice(
            (0x1000, 0x1000, 0x1001, 0, 0x11000 & 0xFFFF, rng.randrange(1 << 16))
        ),
        rng.choice((0, 0, 1, -1, rng.randrange(-0x8000, 0x8000))),
        rng.choice((0, 0, 1, word(rng))),
        rng.choice((0, QUEUED, EARLIER)),
    ]


def vector(rng: random.Random, number: int, mode: int) -> dict:
    controller = rng.choice((0, 1, 1, 2)) if mode != LOAD else rng.choice((1, 1, 2))
    value = {
        "mode": mode,
        "controller": controller,
        "defaults": record(rng),
        "other": record(rng),
        "begin": rng.choice((0, 0, 1, word(rng))),
        "allocate": rng.choice((QUEUED, QUEUED, QUEUED, 0)),
        "states": [rng.choice(STATES), rng.choice(STATES)],
        "secondary": rng.getrandbits(32),
    }
    target = value["other"] if controller == 2 else value["defaults"]
    # A poll reads through the request, which the original never checked for null.
    if mode in (LOAD, DRAW) and target[6] == 0:
        target[6] = rng.choice((QUEUED, EARLIER))
    return value


def target(value: dict) -> list[int]:
    return value["other"] if value["controller"] == 2 else value["defaults"]


def request_number(address: int) -> int:
    return (
        REQUEST_ADDRESSES.index(address)
        if address in REQUEST_ADDRESSES
        else OTHER_REQUEST
    )


def put_record(machine: Machine, base: int, fields: list[int]) -> None:
    for (offset, size, _), field in zip(FIELDS, fields, strict=False):
        machine.put(base + offset, field, size)
    machine.put(base + 0xC, REQUEST_ADDRESSES[fields[6]])


def lifetime(machine: Machine) -> list[int]:
    pointer = machine.load(POINTER)
    base = pointer or DEFAULTS
    words = [
        machine.load(base + offset, size, signed=signed)
        for offset, size, signed in FIELDS
    ]
    controller = CONTROLLERS.index(pointer) if pointer in CONTROLLERS else OTHER_REQUEST
    return [controller, *words, request_number(machine.load(base + 0xC))]


def stubs(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    def memset(m: Machine) -> None:
        for offset in range(m.r[6]):
            m.write(m.r[4] + offset, m.r[5], 1)

    def allocate(m: Machine) -> None:
        file = (m.load(m.r[4]), m.load(m.r[4] + 4))
        calls.append(
            [CALL_ALLOCATE] if file == SCREFF2_FILE else [CALL_ALLOCATE, *file]
        )
        m.r[2] = REQUEST_ADDRESSES[value["allocate"]]

    def enqueue(m: Machine) -> None:
        call = [CALL_ENQUEUE, request_number(m.r[4])]
        calls.append(call if m.r[5] == value["secondary"] else [*call, s32(m.r[5])])

    machine.stub(MEMSET, memset)
    machine.stub(ALLOCATE, allocate)
    machine.stub(ENQUEUE, enqueue)
    machine.stub(FREE, lambda m: calls.append([CALL_FREE, request_number(m.r[4])]))
    machine.stub(WAIT, lambda m: calls.append([CALL_WAIT]))
    machine.stub(ACQUIRE_WORKSPACE, lambda m: calls.append([CALL_ACQUIRE, s32(m.r[4])]))
    machine.stub(RELEASE_WORKSPACE, lambda m: calls.append([CALL_RELEASE, s32(m.r[4])]))
    machine.stub(
        RETIRE_PAGES,
        lambda m: calls.append([CALL_RETIRE, *(s32(m.r[4 + i]) for i in range(3))]),
    )
    for callee, kind in (
        (WARP, CALL_WARP),
        (TILES, CALL_TILES),
        (VIGNETTE, CALL_VIGNETTE),
    ):
        machine.stub(callee, lambda m, kind=kind: calls.append([kind]))
    machine.follow.update(FOLLOW)


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    calls: list[list[int]] = []
    stubs(machine, value, calls)
    for base, fields in ((DEFAULTS, value["defaults"]), (OTHER, value["other"])):
        machine.map_zero(base, RECORD_BYTES)
        put_record(machine, base, fields)
    machine.map_zero(REQUESTS, 0x40)
    for address, state in zip(REQUEST_ADDRESSES[1:], value["states"], strict=True):
        machine.put(address, state, 2)
    machine.map_zero(OVERLAY_SLOTS, 0x10)
    machine.put(SECONDARY_SLOT, value["secondary"])
    machine.put(POINTER, CONTROLLERS[value["controller"]])
    machine.r[4] = value["begin"] & 0xFFFFFFFF
    machine.execute(entry)

    values = lifetime(machine)
    if [CALL_ENQUEUE, 0] in calls:
        # The core completes a refused request instead of enqueueing null.
        calls[calls.index([CALL_ENQUEUE, 0])] = [CALL_COMPLETE_REFUSED]
        if values[7] == 0:
            values[7] = COMPLETED
    inputs = [
        value["controller"],
        *target(value),
        value["begin"],
        value["allocate"],
        *value["states"],
    ]
    return recorded_observation(machine, values, list(NAMES), calls), [
        value["mode"],
        *inputs,
    ]


def check(
    image, value: dict, observation: dict, native: list[int], reference
) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    polls = sum(
        1
        for value, observation, _ in results
        if value["mode"] in (LOAD, DRAW)
        and observation["values"][1] == 0
        and target(value)[0] != 0
    )
    refused = sum(
        1
        for _, observation, _ in results
        if [CALL_COMPLETE_REFUSED] in observation["calls"]
    )
    return [
        f"{polls} vectors completed a pending load, {refused} vectors had their disc request refused"
    ]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (POINTER, POINTER + 4),
    ]
    if value["controller"] == 2 and value["mode"] in (LOAD, DRAW):
        allowed.append((OTHER, OTHER + RECORD_BYTES))
    else:
        allowed.append((DEFAULTS, DEFAULTS + RECORD_BYTES))
    return allowed


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
