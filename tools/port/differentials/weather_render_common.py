"""Shared harness for the weather draw differentials (BATTLE.PRG 146C.c:17758-18197, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Effects core
(WeatherRender.cpp) in weather_render_driver.cpp. Every vector draws the frame globals (room updates
paused, tick speed and the D_800F1D98 velocity), the first background reference VECTOR, the pool
globals at D_800F1DC0, the room record's area (or no room), the particle buffer's 0x800 bytes, a view
matrix and projection, and the words rand(), rsin(), rcos(), the collision map and the floor heights
return. The originals' falling and ember respawns (func_80090A1C and func_8008FA14) run as code.

Projection (#371): SetRotMatrix and SetTransMatrix load the view matrix into the interpreter's GTE
model, and the RotTransPers, RotTransPers4 and RotTrans stubs run its RTPS and MVMVA. The stubs record
each call's vertices and what the GTE produced; the driver's host replays those results in call order.
The typed rules take projections as inputs, so the comparison covers the weather logic independently of
the native GTE (Core_Gte), whose projection differs from the SDK's on negative fractions and depth cue.
A native call with different vertices still differs, as a call.

The driver prints the pool globals, the primitive cursor's advance, the random words used, the buffer,
and the packets as the ordering table holds them: for each entry in order, its chain from the head,
each packet as [entry, length, words...]. Then the calls.

Native differences outside the compared domain (covered by WeatherRenderTests.cpp): a count larger than
the 128-particle buffer is refused and only the particles that fit are drawn; the Live adapter refuses
the ember calls without a room record and drops packets without an ordering entry or buffer room.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    DQA,
    DQB,
    IR0,
    OFX,
    OFY,
    VXY0,
    VZ0,
    Fault,
    H,
    Machine,
    mode_case,
    recorded_observation,
    s16,
    s32,
)

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "weather_render_driver.cpp"
SOURCES = (r"Native\Game\src\Effects\WeatherRender.cpp", r"Native\Game\src\Effects\WeatherPool.cpp")

ENVIRONMENT_POINTER = 0x800F1C84
ENVIRONMENT = 0x80500000
ENVIRONMENT_BYTES = 0x60
WEATHER_AREA_OFFSET = 0x48
BUFFER_POINTER = 0x800F1DD4
BUFFER = 0x80510000
BUFFER_BYTES = 0x800
PARTICLE_BYTES = 0x10
PARTICLES = BUFFER_BYTES // PARTICLE_BYTES
POOL = 0x800F1DC0
POOL_BYTES = 0x10
COUNT = POOL
CAPACITY = POOL + 2
SCREEN_FADE = POOL + 7
KIND = POOL + 0xA
PARAMETERS = 0x800F1D98
REFERENCE = 0x800F1CE0
PAUSED = 0x800F1BA4
TICK_SPEED = 0x8005E24C

SCRATCH = 0x1F800000
SCRATCH_BYTES = 0x400
MATRIX = SCRATCH + 0x14
PRIMITIVES = 0x80530000
PRIMITIVE_BYTES = 0x8000
ORDERING = 0x80540000
ORDERING_ENTRIES = 0x4000
CELL = 0x80580000
LINK_MASK = 0xFFFFFF

RAND_ADDRESS = 0x80026EC4
SINE_ADDRESS = 0x80040F28
COSINE_ADDRESS = 0x80040F24
SET_ROTATION_ADDRESS = 0x80041598
SET_TRANSLATION_ADDRESS = 0x80041578
PROJECT_ADDRESS = 0x80041814
PROJECT_QUAD_ADDRESS = 0x800418DC
TRANSFORM_ADDRESS = 0x800417EC
ADD_PRIMITIVE_ADDRESS = 0x8002B4D4
CELL_ADDRESS = 0x8008B764
FLOOR_ADDRESS = 0x8008DC7C
FALLING_RESET_ADDRESS = 0x80090A1C
EMBER_RESET_ADDRESS = 0x8008FA14

# The kinds the driver prints for each recorded call.
CALL_SINE, CALL_COSINE, CALL_BEGIN, CALL_PROJECT, CALL_QUAD, CALL_TRANSFORM, CALL_CELL, CALL_FLOOR = range(1, 9)

RAIN, EMBER_INIT, EMBER_RESET, EMBER_DRAW, SNOW, SCREEN = range(6)

RANDOM_WORDS = 1024
TRIG_WORDS = 512
CELL_REPLIES = 64
FLOOR_REPLIES = 64
SPECIAL_RANDOMS = (0, 1, 0x7F, 0x80, 0x7FFF, 0x100, 0x1FF, 0x200, -1, 0x7FFFFFFF, -0x80000000, 15, 16)
TRIG = (0, 4096, -4096, 2048, -2048, 1, -1, 0x7FFFFFFF, -0x80000000, 511, 512, 1023)
EXTENTS = (0, -1, 1, 2, 0x180, 0x7FFF, -0x8000, 0x1000)
COUNTS = (0, 1, 2, 3, 5, 8, 13, -1, -0x8000)


def half(rng: random.Random, choices=(0, 1, -1, 0x7FFF, -0x8000)) -> int:
    return rng.choice((*choices, rng.randrange(-0x8000, 0x8000)))


def random_word(rng: random.Random) -> int:
    return rng.choice((*SPECIAL_RANDOMS, rng.randrange(0x8000), rng.randrange(0x8000), s32(rng.randrange(1 << 32))))


def visible(rng: random.Random, value: dict) -> tuple[int, int, int]:
    """A point the view projects onto the screen (exactly when the rotation is the identity), at a chosen
    depth from just in front of the camera to past the far ordering entry."""
    tx, ty, tz = value["translation"]
    ofx, ofy, focal = (value["projection"][0] >> 16, value["projection"][1] >> 16, value["projection"][2])
    depth = rng.choice((rng.randrange(3, 7), rng.randrange(1, 70), rng.randrange(60, 600), rng.randrange(500, 2100)))
    view_z = 4 * depth + rng.randrange(4)
    x = (rng.randrange(-20, 340) - ofx) * view_z // focal - tx
    y = (rng.randrange(-20, 260) - ofy) * view_z // focal - ty
    return x, y, view_z - tz


def particle(rng: random.Random, value: dict) -> bytes:
    mode = value["mode"]
    if mode == SCREEN:
        x = rng.choice((0, 1, 160, 319, 320, 321, -1, -320, rng.randrange(-400, 700), rng.randrange(320)))
        y = rng.choice((0, 1, 120, 239, 240, 241, -1, -240, rng.randrange(-300, 500), rng.randrange(240)))
        z = rng.choice((0, 255, 256, -768, -767, -769, rng.randrange(256), rng.randrange(-0x8000, 0x8000)))
        screen_x = rng.choice((0, 160, 319, 320, -1, rng.randrange(320), rng.randrange(-0x8000, 0x8000)))
        screen_y = rng.choice((0, 120, 239, 240, -1, rng.randrange(240), rng.randrange(-0x8000, 0x8000)))
    elif rng.random() < 0.6:
        x, y, z = visible(rng, value)
        screen_x, screen_y = half(rng), half(rng)
    else:
        x = rng.choice((rng.randrange(-3000, 3000), rng.randrange(-600, 600), rng.randrange(-200, 200), half(rng)))
        y = rng.choice((rng.randrange(-2000, 0), rng.randrange(-2000, 0), rng.randrange(-300, 0),
                        rng.randrange(-200, 200), rng.randrange(1, 400), -1023, -1024, -1025, 0, 1, half(rng)))
        z = rng.choice((rng.randrange(-1500, 5000), rng.randrange(0, 3000), rng.randrange(0, 600), half(rng)))
        screen_x, screen_y = half(rng), half(rng)
    opaque = rng.choice((0, 0, 0, 1, 1, 2, 0xFF)) if mode in (RAIN, SNOW) else rng.choice((0, 0, 0, 0, 1))
    sway = rng.choice((0, 0, 1, 2, 5, 6, 7, 8, 9, 30, 31, 31, 31, 31, 32, 33, rng.randrange(256)))
    shift = rng.choice((0, 0, 0, 1, 2, 3, 31, 32, 33, rng.randrange(256)))
    speed = rng.choice((0, 1, 2, rng.randrange(16))) if mode == EMBER_DRAW and rng.random() < 0.7 else rng.randrange(256)
    pad = half(rng)
    data = b"".join((value & 0xFFFF).to_bytes(2, "little") for value in (x, y, z, pad, screen_x, screen_y))
    return data + bytes((opaque, sway, shift, speed))


def vector(rng: random.Random, number: int, mode: int) -> dict:
    count = rng.choice((*COUNTS, rng.randrange(24), rng.randrange(40)))
    if mode in (EMBER_INIT, EMBER_RESET):
        count = rng.choice((*COUNTS, rng.randrange(PARTICLES + 1), PARTICLES, -0x80000000))
    elif mode == EMBER_DRAW and rng.random() < 0.5:
        count = rng.randrange(8, 40)
    rotation = [4096, 0, 0, 0, 4096, 0, 0, 0, 4096, 0] if rng.random() < 0.6 else [
        rng.randrange(-4096, 4097) for _ in range(9)] + [0]
    value = {
        "mode": mode,
        "paused": rng.choice((0, 0, 0, 1, 0x100, 0x8000)),
        "tick": rng.choice((0, 1, 2, 2, 3, 4, 4, 5, 8, -2)),
        "velocity": [rng.choice((rng.randrange(-64, 64), rng.randrange(-8, 8), half(rng, (0, 0x80, -0x80)))),
                     rng.choice((rng.randrange(0, 200), rng.randrange(-200, 200), half(rng, (0, 8, 64)))),
                     rng.choice((rng.randrange(-64, 64), rng.randrange(-8, 8), half(rng, (0, 0x80, -0x80))))],
        "reference": [half(rng) for _ in range(3)],
        "pool": [rng.randrange(256) for _ in range(POOL_BYTES)],
        "room": mode not in (RAIN, SNOW) or rng.random() < 0.8,
        "area": [half(rng), half(rng), half(rng, EXTENTS), half(rng, EXTENTS)],
        "arguments": [count, rng.choice((0, 1, 2, 3, 4, 0xFF, 0x100, -1, rng.randrange(-0x8000, 0x8000)))],
        "index": rng.choice((0, PARTICLES - 1, rng.randrange(PARTICLES))),
        "rotation": rotation,
        "translation": [rng.randrange(-500, 500), rng.randrange(-500, 500),
                        rng.choice((0, 100, 200, 500, 1000, 2000, 2000, 4000, rng.randrange(-2000, 8000)))],
        "projection": [rng.choice((160, 0, 100)) << 16, rng.choice((120, 0, 100)) << 16,
                       rng.choice((128, 256, 512, 1000)), rng.randrange(-0x8000, 0x8000), rng.randrange(1 << 24)],
        "randoms": [random_word(rng) for _ in range(RANDOM_WORDS)],
        "trig": [rng.choice((*TRIG, rng.randrange(-4096, 4097), rng.randrange(-4096, 4097)))
                 for _ in range(TRIG_WORDS)],
        "cells": [rng.choice((None, rng.choice((0, 3, 31, 0x3F, 0xFFFFFFE0, rng.randrange(1 << 32)))))
                  for _ in range(CELL_REPLIES)],
        "floors": [rng.choice((0, -1, 0x7FFF, 0x8000, 0x10000, -0x8000, rng.randrange(-0x4000, 0x4000),
                               s32(rng.randrange(1 << 32)))) for _ in range(FLOOR_REPLIES)],
    }
    value["buffer"] = b"".join(particle(rng, value) for _ in range(PARTICLES))
    pool = value["pool"]
    if mode != EMBER_INIT:
        pool[0], pool[1] = count & 0xFF, (count >> 8) & 0xFF
    if mode == SCREEN:
        pool[7] = rng.choice((0, 8, 7, 88, 90, 96, 100, 128, 200, 255, rng.randrange(256)))
    return value


def gte_vertex(machine: Machine, address: int) -> list[int]:
    machine.gte_write(VXY0, machine.load(address))
    machine.gte_write(VZ0, machine.load(address + 4, 2))
    return [machine.load(address + offset, 2, signed=True) for offset in (0, 2, 4)]


def project(machine: Machine, address: int) -> tuple[list[int], int]:
    vertex = gte_vertex(machine, address)
    machine.gte_rtps()
    return vertex, machine.gte[14]


def depth_of(machine: Machine) -> int:
    return (machine.gte[19] & 0xFFFF) >> 2


def screen_of(sxy: int) -> list[int]:
    return [s16(sxy), s16(sxy >> 16)]


class Recorder:
    """The stubs: they record each call and the GTE's results in call order."""

    def __init__(self, machine: Machine, value: dict) -> None:
        self.value = value
        self.calls: list[list[int]] = []
        self.used = 0
        self.trig = 0
        self.cells = 0
        self.floors = 0
        self.projections: list[list[int]] = []
        self.quads: list[list[int]] = []
        self.transforms: list[list[int]] = []
        self.touched: set[int] = set()
        stubs = {
            RAND_ADDRESS: self.rand,
            SINE_ADDRESS: lambda machine: self.trigonometry(machine, CALL_SINE),
            COSINE_ADDRESS: lambda machine: self.trigonometry(machine, CALL_COSINE),
            SET_ROTATION_ADDRESS: self.set_rotation,
            SET_TRANSLATION_ADDRESS: self.set_translation,
            PROJECT_ADDRESS: self.project,
            PROJECT_QUAD_ADDRESS: self.project_quad,
            TRANSFORM_ADDRESS: self.transform,
            ADD_PRIMITIVE_ADDRESS: self.add_primitive,
            CELL_ADDRESS: self.cell,
            FLOOR_ADDRESS: self.floor,
        }
        for address, stub in stubs.items():
            machine.stub(address, stub)

    def rand(self, machine: Machine) -> None:
        randoms = self.value["randoms"]
        machine.r[2] = randoms[self.used] & 0xFFFFFFFF if self.used < len(randoms) else 0
        self.used += 1

    def trigonometry(self, machine: Machine, kind: int) -> None:
        self.calls.append([kind, s32(machine.r[4])])
        trig = self.value["trig"]
        machine.r[2] = trig[self.trig] & 0xFFFFFFFF if self.trig < len(trig) else 0
        self.trig += 1

    def set_rotation(self, machine: Machine) -> None:
        self.calls.append([CALL_BEGIN])
        machine.gte_rotation = [machine.load(machine.r[4] + 4 * index) for index in range(5)]

    @staticmethod
    def set_translation(machine: Machine) -> None:
        machine.gte_translation = [machine.load(machine.r[4] + 0x14 + 4 * index) for index in range(3)]

    def project(self, machine: Machine) -> None:
        vertex, sxy = project(machine, machine.r[4])
        machine.put(machine.r[5], sxy)
        machine.put(machine.r[6], machine.gte[IR0])
        machine.put(machine.r[7], machine.gte_flag or 0)
        machine.r[2] = depth_of(machine)
        self.calls.append([CALL_PROJECT, *vertex])
        self.projections.append([*screen_of(sxy), machine.r[2]])

    def project_quad(self, machine: Machine) -> None:
        stack = machine.r[29]
        vertices, screens = [], []
        for index, register in enumerate((4, 5, 6, 7)):
            vertex, sxy = project(machine, machine.r[register])
            machine.put(machine.load(stack + 0x10 + 4 * index), sxy)
            vertices += vertex
            screens += screen_of(sxy)
        machine.put(machine.load(stack + 0x20), machine.gte[IR0])
        machine.put(machine.load(stack + 0x24), machine.gte_flag or 0)
        machine.r[2] = depth_of(machine)
        self.calls.append([CALL_QUAD, *vertices])
        self.quads.append([*screens, machine.r[2]])

    def transform(self, machine: Machine) -> None:
        vertex = gte_vertex(machine, machine.r[4])
        machine.gte_write_flag(machine.gte_transform_v0(machine.gte_translation))
        result = [s32(machine.gte[mac]) for mac in (25, 26, 27)]
        for index, word in enumerate(result):
            machine.put(machine.r[5] + 4 * index, word)
        machine.put(machine.r[6], machine.gte_flag or 0)
        self.calls.append([CALL_TRANSFORM, *vertex])
        self.transforms.append(result)

    def add_primitive(self, machine: Machine) -> None:
        entry, primitive = machine.r[4], machine.r[5]
        if not ORDERING <= entry < ORDERING + 4 * ORDERING_ENTRIES or entry % 4:
            raise Fault(f"addPrim outside the ordering table {entry:#x}")
        link = machine.load(entry)
        tag = machine.load(primitive)
        machine.put(primitive, (tag & ~LINK_MASK & 0xFFFFFFFF) | (link & LINK_MASK))
        machine.put(entry, (link & ~LINK_MASK & 0xFFFFFFFF) | (primitive & LINK_MASK))
        self.touched.add((entry - ORDERING) // 4)

    def cell(self, machine: Machine) -> None:
        self.calls.append([CALL_CELL, s32(machine.r[4]), s32(machine.r[5]), s32(machine.r[6])])
        cells = self.value["cells"]
        reply = cells[self.cells] if self.cells < len(cells) else None
        self.cells += 1
        if reply is None:
            machine.r[2] = 0
            return
        machine.put(CELL, reply)
        machine.r[2] = CELL

    def floor(self, machine: Machine) -> None:
        self.calls.append([CALL_FLOOR, s32(machine.r[4]), s32(machine.r[5])])
        floors = self.value["floors"]
        machine.r[2] = floors[self.floors] & 0xFFFFFFFF if self.floors < len(floors) else 0
        self.floors += 1


def packets(machine: Machine, touched: set[int]) -> list[list[int]]:
    """The ordering table's chains, each packet as [entry, length, words...]."""
    entries = touched | {(address - ORDERING) // 4 for address, _ in machine.stores
                         if ORDERING <= address < ORDERING + 4 * ORDERING_ENTRIES}
    found = []
    for entry in sorted(entries):
        link = machine.load(ORDERING + 4 * entry) & LINK_MASK
        while link:
            if len(found) > 4096:
                raise Fault("ordering table loop")
            address = 0x80000000 | link
            tag = machine.load(address)
            size = tag >> 24
            found.append([entry, size, *(machine.load(address + 4 * (index + 1)) for index in range(size))])
            link = tag & LINK_MASK
    return found


def setup(machine: Machine, value: dict) -> None:
    machine.map_zero(ENVIRONMENT, ENVIRONMENT_BYTES)
    machine.map_zero(SCRATCH, SCRATCH_BYTES)
    machine.map_zero(PRIMITIVES, PRIMITIVE_BYTES)
    machine.map_zero(ORDERING, 4 * ORDERING_ENTRIES)
    machine.map_zero(CELL, 4)
    for index, field in enumerate(value["area"]):
        machine.put(ENVIRONMENT + WEATHER_AREA_OFFSET + 2 * index, field, 2)
    machine.put(ENVIRONMENT_POINTER, ENVIRONMENT if value["room"] else 0)
    machine.store(BUFFER, value["buffer"])
    machine.put(BUFFER_POINTER, BUFFER)
    machine.store(POOL, bytes(value["pool"]))
    for index, field in enumerate(value["velocity"]):
        machine.put(PARAMETERS + 2 * index, field, 2)
    for index, field in enumerate(value["reference"]):
        machine.put(REFERENCE + 2 * index, field, 2)
    machine.put(PAUSED, value["paused"], 2)
    machine.put(TICK_SPEED, value["tick"])
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, ORDERING)
    for index, field in enumerate(value["rotation"]):
        machine.put(MATRIX + 2 * index, field, 2)
    for index, field in enumerate(value["translation"]):
        machine.put(MATRIX + 0x14 + 4 * index, field)
    for register, field in zip((OFX, OFY, H, DQA, DQB), value["projection"], strict=True):
        machine.gte_projection[register] = field


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    setup(machine, value)
    recorder = Recorder(machine, value)
    machine.follow.update((FALLING_RESET_ADDRESS, EMBER_RESET_ADDRESS))
    machine.r[4] = value["arguments"][0] & 0xFFFFFFFF
    machine.r[5] = value["arguments"][1] & 0xFFFFFFFF
    if mode == EMBER_RESET:
        machine.r[4] = BUFFER + PARTICLE_BYTES * value["index"]
    machine.execute(entry, limit=4_000_000)

    found = packets(machine, recorder.touched)
    values = [
        machine.load(COUNT, 2, signed=True),
        machine.load(CAPACITY, 2, signed=True),
        machine.load(KIND, 1),
        machine.load(SCREEN_FADE, 1),
        s32(machine.load(SCRATCH) - PRIMITIVES),
        recorder.used,
        *(machine.load(BUFFER + 4 * index) for index in range(BUFFER_BYTES // 4)),
        len(found),
        *(word for packet in found for word in packet),
    ]
    names = ["count", "capacity", "kind", "screen fade", "cursor advance", "randoms used",
             *(f"particle {index // 4} word {index % 4}" for index in range(BUFFER_BYTES // 4)),
             "packets",
             *(f"packet {number} {'entry' if word == 0 else 'length' if word == 1 else f'word {word - 2}'}"
               for number, packet in enumerate(found) for word in range(len(packet)))]
    cells = [item for reply in value["cells"] for item in ((0, 0) if reply is None else (1, reply))]
    inputs = [
        value["paused"], value["tick"], *value["velocity"], *value["reference"],
        *value["pool"], int(value["room"]), *value["area"], *value["arguments"], value["index"],
        len(value["randoms"]), *value["randoms"],
        len(value["trig"]), *value["trig"],
        len(value["cells"]), *cells,
        len(value["floors"]), *value["floors"],
        len(recorder.projections), *(item for reply in recorder.projections for item in reply),
        len(recorder.quads), *(item for reply in recorder.quads for item in reply),
        len(recorder.transforms), *(item for reply in recorder.transforms for item in reply),
        *value["buffer"],
    ]
    return recorded_observation(machine, values, names, recorder.calls), [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    return [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (SCRATCH, SCRATCH + 4),
        (PRIMITIVES, PRIMITIVES + PRIMITIVE_BYTES),
        (ORDERING, ORDERING + 4 * ORDERING_ENTRIES),
        (BUFFER, BUFFER + BUFFER_BYTES),
        (POOL, CAPACITY + 2) if value["mode"] == EMBER_INIT else (SCREEN_FADE, SCREEN_FADE + 1),
        (KIND, KIND + 1) if value["mode"] == EMBER_INIT else (SCREEN_FADE, SCREEN_FADE + 1),
    ]


def describe(image, results) -> list[str]:
    packet_index = 6 + BUFFER_BYTES // 4
    drawn = sum(observation["values"][packet_index] for _, observation, _ in results)
    called = sum(1 for _, observation, _ in results if observation["calls"])
    used = sum(observation["values"][5] for _, observation, _ in results)
    paused = sum(1 for value, _, _ in results if value["paused"])
    return [f"{drawn} packets queued, {called} of {len(results)} vectors made a recorded call, "
            f"{used} random words used", f"{paused} vectors had room updates paused"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
