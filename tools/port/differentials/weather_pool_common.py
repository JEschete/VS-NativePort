"""Shared harness for the weather pool differentials (BATTLE.PRG 146C.c:17445-17737 and 18200-18221, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Effects core
(WeatherPool.cpp) in weather_pool_driver.cpp. Every vector draws the pool globals around
D_800F1DC0 (count, capacity, intensity, screen fade and kind), the velocity and wind words at
D_800F1D98, the room record's weather kind and area (or no room), the particle buffer's 0x800
bytes, the words rand(), rsin() and rcos() return and what the heap returns. The stubs return the
drawn words in call order and record the heap, room weather and trigonometry calls with their
argument registers; the driver's host does the same. The driver prints the stored buffer pointer,
the pool globals, func_8008EB04's outputs, the velocity words, how many random words were used,
the whole buffer and the calls.

The random words cover the whole signed range, not only rand()'s 0..0x7FFF, so a remainder that
loses its sign, or one taken unsigned, differs. The room extents include 0 and -1: the original's
DIV has no zero check, and the core reproduces what the R3000 leaves in HI.

Native differences outside the compared domain (covered by WeatherPoolTests.cpp): a spawn larger
than the 128-particle buffer is refused after setting the counts, and respawns only the particles
that fit; the Live adapter refuses a null buffer and particles outside it.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Machine,
    mode_case,
    recorded_observation,
    s32,
)

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "weather_pool_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\WeatherPool.cpp",)

ENVIRONMENT_POINTER = 0x800F1C84
ENVIRONMENT = 0x80500000
ENVIRONMENT_BYTES = 0x60
WEATHER_KIND_OFFSET = 0x34
WEATHER_AREA_OFFSET = 0x48
BUFFER_POINTER = 0x800F1DD4
BUFFER = 0x80510000
BUFFER_BYTES = 0x800
PARTICLE_BYTES = 0x10
PARTICLES = BUFFER_BYTES // PARTICLE_BYTES
OUTPUTS = 0x80520000
POOL = 0x800F1DC0
POOL_BYTES = 0x10
PARAMETERS = 0x800F1D98
PARAMETER_HALVES = 8

COUNT = POOL
CAPACITY = POOL + 2
INTENSITY = POOL + 4
SCREEN_FADE = POOL + 7
KIND = POOL + 0xA

RAND_ADDRESS = 0x80026EC4
SINE_ADDRESS = 0x80040F28
COSINE_ADDRESS = 0x80040F24
FREE_ADDRESS = 0x80043D50
ALLOCATE_ADDRESS = 0x80043EC4
WEATHER_ADDRESS = 0x80090C2C
RESET_ADDRESS = 0x80090A1C

# The kinds the driver prints for each recorded call.
CALL_FREE, CALL_ALLOCATE, CALL_WEATHER, CALL_SINE, CALL_COSINE = range(1, 6)

ALLOCATE, ROOM_WEATHER, WEATHER_STATE, FALLING, SCREEN, RESET, VELOCITY = range(7)

SIGNED_WORDS = (0, 1, -1, 0x7FFF, 0x8000, 0x7FFFFFFF, -0x80000000, -0x8000, 0xFFF, -0xFFF, 0x1FF, -0x200)
HALVES = (0, 1, -1, 0x7FFF, -0x8000, 0x100, 0x800, -0x800)
EXTENTS = (0, -1, 1, 2, 0x180, 0x7FFF, -0x8000, 0x1000)
ROOM_KINDS = (0, 1, 1, 2, 3, 4, -1, 0x101, -0x7FFF)
SPAWN_COUNTS = (-0x80000000, -1, 0, 1, 2, 3, PARTICLES - 1, PARTICLES)
TRIG = (0, 4096, -4096, 2048, -2048, 1, -1, 0x7FFFFFFF, -0x80000000, 0x10000)


def word(rng: random.Random) -> int:
    return rng.choice((*SIGNED_WORDS, s32(rng.randrange(1 << 32)), rng.randrange(0x8000)))


def half(rng: random.Random, choices=HALVES) -> int:
    return rng.choice((*choices, rng.randrange(-0x8000, 0x8000)))


def randoms_for(mode: int, count: int) -> int:
    if mode in (FALLING, SCREEN):
        return 4 * max(count, 0)
    return 4 if mode == RESET else 0


def vector(rng: random.Random, number: int, mode: int) -> dict:
    count = rng.choice((*SPAWN_COUNTS, rng.randrange(PARTICLES + 1)))
    value = {
        "mode": mode,
        "room": rng.random() < 0.8,
        "room_kind": rng.choice((*ROOM_KINDS, rng.randrange(-0x8000, 0x8000))),
        "area": [half(rng), half(rng), half(rng, EXTENTS), half(rng, EXTENTS)],
        "arguments": [word(rng), word(rng)],
        "current": rng.choice((0, 0, BUFFER, 0x80123450, rng.randrange(1 << 32))),
        "allocated": rng.choice((0, BUFFER, 0x80234560, rng.randrange(1 << 32))),
        "pool": [rng.randrange(256) for _ in range(POOL_BYTES)],
        "parameters": [half(rng) for _ in range(PARAMETER_HALVES)],
        "trig": [rng.choice((*TRIG, s32(rng.randrange(1 << 32)), rng.randrange(-4096, 4097))) for _ in range(5)],
        "buffer": [rng.randrange(256) for _ in range(BUFFER_BYTES)],
        "index": rng.choice((0, PARTICLES - 1, rng.randrange(PARTICLES))),
    }
    if mode in (FALLING, SCREEN):
        value["arguments"][0] = count
    if mode != ALLOCATE:
        value["current"] = BUFFER
    value["randoms"] = [word(rng) for _ in range(randoms_for(mode, count))]
    return value


def recorder(machine: Machine, value: dict, calls: list[list[int]], used: list[int]) -> None:
    randoms = value["randoms"]
    trig = list(value["trig"])

    def rand(machine: Machine) -> None:
        machine.r[2] = randoms[used[0]] & 0xFFFFFFFF if used[0] < len(randoms) else 0
        used[0] += 1

    def record(kind: int, result=None):
        def stub(machine: Machine) -> None:
            calls.append([kind, s32(machine.r[4])])
            if result is not None:
                machine.r[2] = result() & 0xFFFFFFFF

        return stub

    machine.stub(RAND_ADDRESS, rand)
    machine.stub(FREE_ADDRESS, record(CALL_FREE))
    machine.stub(ALLOCATE_ADDRESS, record(CALL_ALLOCATE, lambda: value["allocated"]))
    machine.stub(WEATHER_ADDRESS, record(CALL_WEATHER))
    machine.stub(SINE_ADDRESS, record(CALL_SINE, lambda: trig.pop(0) if trig else 0))
    machine.stub(COSINE_ADDRESS, record(CALL_COSINE, lambda: trig.pop(0) if trig else 0))


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    machine.map_zero(ENVIRONMENT, ENVIRONMENT_BYTES)
    machine.map_zero(OUTPUTS, 8)
    machine.put(ENVIRONMENT + WEATHER_KIND_OFFSET, value["room_kind"], 2)
    for index, field in enumerate(value["area"]):
        machine.put(ENVIRONMENT + WEATHER_AREA_OFFSET + 2 * index, field, 2)
    machine.put(ENVIRONMENT_POINTER, ENVIRONMENT if value["room"] else 0)
    machine.store(BUFFER, bytes(value["buffer"]))
    machine.put(BUFFER_POINTER, value["current"])
    machine.store(POOL, bytes(value["pool"]))
    for index, field in enumerate(value["parameters"]):
        machine.put(PARAMETERS + 2 * index, field, 2)
    calls: list[list[int]] = []
    used = [0]
    recorder(machine, value, calls, used)
    if mode == FALLING:
        machine.follow.add(RESET_ADDRESS)
    machine.r[4] = value["arguments"][0] & 0xFFFFFFFF
    machine.r[5] = value["arguments"][1] & 0xFFFFFFFF
    if mode == WEATHER_STATE:
        machine.r[4], machine.r[5] = OUTPUTS, OUTPUTS + 4
    elif mode == RESET:
        machine.r[4] = BUFFER + PARTICLE_BYTES * value["index"]
    machine.r[2] = 0
    machine.execute(entry)

    values = [
        s32(machine.r[2]) if mode == ROOM_WEATHER else 0,
        machine.load(BUFFER_POINTER),
        machine.load(COUNT, 2, signed=True),
        machine.load(CAPACITY, 2, signed=True),
        machine.load(INTENSITY, 2, signed=True),
        machine.load(SCREEN_FADE, 1),
        machine.load(KIND, 1),
        machine.load(OUTPUTS, 4, signed=True),
        machine.load(OUTPUTS + 4, 4, signed=True),
        *(machine.load(PARAMETERS + 2 * index, 2, signed=True) for index in range(3)),
        used[0],
        *(machine.load(BUFFER + 4 * index) for index in range(BUFFER_BYTES // 4)),
    ]
    names = ["result", "buffer pointer", "count", "capacity", "intensity", "screen fade", "kind",
             "kind output", "intensity output", "velocity x", "velocity y", "velocity z", "randoms used",
             *(f"particle {index // 4} word {index % 4}" for index in range(BUFFER_BYTES // 4))]
    inputs = [
        int(value["room"]),
        value["room_kind"],
        *value["area"],
        *value["arguments"],
        value["current"],
        value["allocated"],
        *value["pool"],
        *value["parameters"],
        value["index"],
        len(value["randoms"]),
        *value["randoms"],
        *value["trig"],
        *value["buffer"],
    ]
    return recorded_observation(machine, values, names, calls), [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    mode = value["mode"]
    if mode == ALLOCATE:
        allowed.append((BUFFER_POINTER, BUFFER_POINTER + 4))
    elif mode == WEATHER_STATE:
        allowed.append((OUTPUTS, OUTPUTS + 8))
    elif mode in (FALLING, SCREEN):
        allowed += [(COUNT, CAPACITY + 2), (KIND, KIND + 1), (BUFFER, BUFFER + BUFFER_BYTES)]
        if mode == SCREEN:
            allowed.append((SCREEN_FADE, SCREEN_FADE + 1))
    elif mode == RESET:
        particle = BUFFER + PARTICLE_BYTES * value["index"]
        allowed.append((particle, particle + PARTICLE_BYTES))
    elif mode == VELOCITY:
        allowed.append((PARAMETERS, PARAMETERS + 6))
    return allowed


def describe(image, results) -> list[str]:
    used = sum(observation["values"][12] for _, observation, _ in results)
    called = sum(1 for _, observation, _ in results if observation["calls"])
    roomless = sum(1 for value, _, _ in results if not value["room"])
    zero = sum(1 for value, _, _ in results if 0 in value["area"][2:])
    return [f"{called} of {len(results)} vectors made a recorded call, {used} random words used",
            f"{roomless} vectors had no room, {zero} a zero extent"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
