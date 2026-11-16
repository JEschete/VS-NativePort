"""Shared R3000 vectors and native driver interface for A32-07: the target sphere's wire line,
traversal, camera basis, position cursor and terrain row cache (BATTLE.PRG)."""

from __future__ import annotations

import random
import struct

from differential import (
    DEFAULT_STACK,
    MASK,
    OFX,
    OFY,
    SXY,
    Fault,
    H,
    R,
    s16,
    s32,
    store_problems,
)

SCRATCH = 0x1F800000
SPHERE_POINTER = 0x800EB9B8
SPHERE = 0x80500000
PRIMITIVES = 0x80600000
ORDERING = 0x80700010
POINTS = 0x80580000
MATRICES = 0x80590000
SPRITES = 0x805A0000
GIMMICKS = 0x805B0000
CELLS = 0x805C0000
GIMMICK_TABLE = 0x800F45E0
COSINE = 0x80040F24
SINE = 0x80040F28

# F01289's callees.
FRAME_VIEW = 0x800C02A8
VOLUME_TRANSFORM = 0x800C085C
BUILD_WIREFRAME = 0x800C253C
DRAW_VOLUME = 0x800C0B50
UPDATE_LABELS = 0x800C20B4
STAGED = SCRATCH + 0x378
SINE_SAMPLES = SCRATCH + 0x3B0
# F01306's camera sync.
SYNC_CAMERA = 0x8007A850
BASIS = SCRATCH + 0x350
# F01307's tables, blink frame and sprite queue.
BLINK = 0x800EA6AC
OFFSETS = 0x800EA670
SIZES = 0x800EA684
TEXCOORDS = 0x800EA698
BRIGHTNESS = 0x800EC270
QUEUE_SPRITE = 0x800C0224
# F01308's callees.
MEMSET = 0x80026EB4
MAP_DIMENSIONS = 0x8008B70C
CELL_POINTER = 0x8008B764
STACK_OFFSET = 0x800FA188
TOP_GIMMICK = 0x800A91DC
FLOOR_CEILING = 0x8008DDA8

SOURCES = (
    r"Native\Game\src\Battle\SphereDraw.cpp",
    r"Native\Engine\src\Core\Gte.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
# The callee's frame plus the caller's four argument home slots.
STACK_SPAN = (DEFAULT_STACK - 0x4000, DEFAULT_STACK + 16)
IDENTITY = (0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000)
EVENT_WORDS = 7


def edge16(rng: random.Random) -> int:
    return rng.choice((-0x8000, -0x7FFF, -1, 0, 1, 0x7FFF)) if rng.random() < 0.15 else rng.randrange(-0x8000, 0x8000)


def view(rng: random.Random) -> dict:
    """A frame view: rotation, translation and projection the GTE holds while drawing."""
    if rng.random() < 0.5:
        rotation = list(IDENTITY)
    else:
        rotation = [rng.randrange(-0x1000, 0x1001) for _ in range(9)]
    if rng.random() < 0.1:
        rotation = [edge16(rng) for _ in range(9)]
    translation = [rng.randrange(-0x800, 0x800), rng.randrange(-0x800, 0x800), rng.randrange(0x100, 0x4000)]
    if rng.random() < 0.1:
        translation = [rng.randrange(-0x80000, 0x80000) for _ in range(3)]
    return {
        "rotation": rotation,
        "translation": translation,
        "ofx": rng.choice((160, 0, -0x400, 0x3FF)) if rng.random() < 0.2 else 160,
        "ofy": rng.choice((120, 0, -0x400, 0x3FF)) if rng.random() < 0.2 else 120,
        "h": rng.choice((256, 512, 1, 0x7FFF)) if rng.random() < 0.3 else rng.randrange(64, 1024),
    }


def view_inputs(value: dict) -> list[int]:
    return [*value["rotation"], *value["translation"], value["ofx"], value["ofy"], value["h"]]


def load_projection(machine, value: dict) -> None:
    machine.gte_projection[OFX] = s32(value["ofx"] << 16)
    machine.gte_projection[OFY] = s32(value["ofy"] << 16)
    machine.gte_projection[H] = value["h"] & 0xFFFF


def load_matrices(machine, value: dict) -> None:
    rotation = value["rotation"]
    for word in range(4):
        machine.gte_rotation[word] = (rotation[2 * word] & 0xFFFF) | (rotation[2 * word + 1] & 0xFFFF) << 16
    machine.gte_rotation[4] = s16(rotation[8])
    machine.gte_translation[:] = [s32(axis) for axis in value["translation"]]


def words(machine, address: int, count: int) -> list[int]:
    return [machine.load(address + 4 * index) for index in range(count)]


def compare(names, native: list[int], expected: list[int]) -> list[str]:
    if len(native) != len(expected):
        return [f"native printed {len(native)} values, original {len(expected)}"]
    problems = [f"{name} native {mine & MASK:#x} original {theirs & MASK:#x}"
                for name, mine, theirs in zip(names, native, expected, strict=True)
                if (mine ^ theirs) & MASK]
    return problems[:8]


# ---------------------------------------------------------------- F01287 wire line
def line_vector(rng: random.Random, number: int) -> dict:
    def point() -> list[int]:
        x, y = rng.randrange(-0x400, 0x400), rng.randrange(-0x400, 0x400)
        return [x, y, rng.randrange(-0x40, 0x840) if rng.random() < 0.85 else edge16(rng)]

    start, end = point(), point()
    shape = number % 8
    if shape == 0:
        end[:2] = start[:2]
    elif shape == 1:
        end[0] = start[0]
    elif shape == 2:
        end[1] = start[1]
    elif shape == 3:
        delta = rng.randrange(-0x200, 0x200)
        end[0], end[1] = start[0] + delta, start[1] + rng.choice((delta, -delta))
    elif shape == 4:
        start[:2] = [edge16(rng), edge16(rng)]
        end[:2] = [edge16(rng), edge16(rng)]
    return {
        "start": start,
        "end": end,
        "pads": [edge16(rng), edge16(rng)],
        "color": rng.getrandbits(32),
        "intensity": rng.choice((-1, 0, 1, 62, 63, 64, 0x7FFFFFFF, -0x80000000))
        if rng.random() < 0.3 else rng.randrange(-8, 80),
        "link": rng.getrandbits(32),
    }


def line_run(machine, value: dict) -> tuple[dict, list[int]]:
    start, end = POINTS, POINTS + 8
    for address, point, pad in ((start, value["start"], value["pads"][0]), (end, value["end"], value["pads"][1])):
        machine.store(address, struct.pack("<4h", *(s16(axis) for axis in point), s16(pad)))
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(PRIMITIVES, 0x100)
    machine.map_zero(ORDERING - 0x10, 0x2020)
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, ORDERING)
    depth = (s16(value["start"][2]) + s16(value["end"][2])) >> 1
    if 0 <= depth < 0x800:
        machine.put(ORDERING + 4 * depth, value["link"])
    machine.r[R["a0"]], machine.r[R["a1"]] = start, end
    machine.r[R["a2"]], machine.r[R["a3"]] = value["color"], value["intensity"] & MASK
    machine.execute(0x800C0990)
    links = [address for address, _ in machine.stores if ORDERING <= address < ORDERING + 0x2000]
    problems = []
    if links:
        slot = (links[0] - ORDERING) // 4
        packet = words(machine, PRIMITIVES, 5)
        expected = [1, slot, *packet[1:]]
        if packet[0] != (value["link"] & 0xFFFFFF) | 0x04000000:
            problems.append(f"tag {packet[0]:#x} is not the slot's old link with the line command")
        if machine.load(ORDERING + 4 * slot) != PRIMITIVES & 0xFFFFFF:
            problems.append("the slot does not link the packet")
        if machine.load(SCRATCH) != PRIMITIVES + 0x14:
            problems.append("the primitive cursor did not advance five words")
    else:
        expected = [1 if machine.load(SCRATCH) != PRIMITIVES else 0, 0, 0, 0, 0, 0]
    observation = {
        "expected": expected,
        "problems": problems,
        "stores": list(machine.stores),
        "allowed": [STACK_SPAN, (SCRATCH, SCRATCH + 4), (PRIMITIVES, PRIMITIVES + 0x14),
                    (ORDERING, ORDERING + 0x2000)],
    }
    inputs = [1287, *value["start"], *value["end"], value["color"], s32(value["intensity"])]
    return observation, inputs


LINE_NAMES = ("drawn", "slot", "mode", "color", "start xy", "end xy")


# ---------------------------------------------------------------- F01289 traversal
def volume(rng: random.Random) -> dict:
    return {
        "kind": rng.randrange(0, 9) if rng.random() < 0.9 else rng.getrandbits(8),
        "reserved": rng.getrandbits(8),
        "angle": edge16(rng),
        "extents": [rng.getrandbits(8) for _ in range(4)],
        "position": [rng.randrange(-0x1000, 0x1000) if rng.random() < 0.8 else edge16(rng) for _ in range(3)],
        "pad": edge16(rng),
    }


def sphere_vector(rng: random.Random, number: int) -> dict:
    volumes = [volume(rng), volume(rng)]
    if number < 49:  # every pair of kinds 1-7 leads the vectors
        volumes[0]["kind"], volumes[1]["kind"] = number // 7 + 1, number % 7 + 1
    return {
        "absent": number >= 49 and rng.random() < 0.08,
        "view": view(rng),
        "volumes": volumes,
        "matrices": [[edge16(rng) if rng.random() < 0.3 else rng.randrange(-0x1000, 0x1001) for _ in range(9)]
                     for _ in range(2)],
        "depth": rng.getrandbits(32),
    }


def volume_words(record: dict) -> bytes:
    return struct.pack("<BBh4B4h", record["kind"], record["reserved"], s16(record["angle"]), *record["extents"],
                       *(s16(axis) for axis in record["position"]), s16(record["pad"]))


def sphere_run(machine, value: dict) -> tuple[dict, list[int]]:
    present = not value["absent"]
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(SPHERE, 0x100)
    machine.map_zero(MATRICES, 0x100)
    machine.put(SPHERE_POINTER, SPHERE if present else 0)
    machine.store(SPHERE, b"".join(volume_words(record) for record in value["volumes"]))
    machine.put(SPHERE + 0x20, value["depth"])
    for index, matrix in enumerate(value["matrices"]):
        machine.store(MATRICES + 0x20 * index, struct.pack("<9h", *(s16(entry) for entry in matrix)))
    load_projection(machine, value["view"])
    events: list[list[int]] = []
    transforms = [0]

    def frame_view(m) -> None:
        load_matrices(m, value["view"])
        slot = sum(1 for event in events if event[0] == 1)
        events.append([1, slot, *words(m, STAGED + 0x10 * slot, 4), m.load(SPHERE + 0x20)])

    def transform(m) -> None:
        events.append([2, m.load(m.r[R["a0"]]), s16(m.r[R["a1"]]), 0, 0, 0, 0])
        m.r[R["v0"]] = MATRICES + 0x20 * transforms[0]
        transforms[0] += 1

    def draw(m) -> None:
        address = m.r[R["a0"]]
        slot = (address - STAGED) // 0x10 if (address - STAGED) % 0x10 == 0 else 99
        events.append([4, slot, *words(m, address, 4), m.r[R["a1"]]])

    machine.stub(FRAME_VIEW, frame_view)
    machine.stub(VOLUME_TRANSFORM, transform)
    machine.stub(BUILD_WIREFRAME, lambda m: events.append([3, m.r[R["a0"]], 0, 0, 0, 0, 0]))
    machine.stub(DRAW_VOLUME, draw)
    machine.stub(UPDATE_LABELS, lambda m: events.append([5, m.load(SPHERE + 0x20), 0, 0, 0, 0, 0]))
    machine.follow.add(SINE)
    machine.execute(0x800C0D78)
    sine = list(struct.unpack("<40h", machine.bytes(SINE_SAMPLES, 80)))
    expected = [*sine, len(events), *(word for event in events for word in event)]
    observation = {
        "expected": expected,
        "problems": [],
        "stores": list(machine.stores),
        "allowed": [STACK_SPAN, (STAGED, STAGED + 0x20), (SINE_SAMPLES, SINE_SAMPLES + 80),
                    (SPHERE + 0x20, SPHERE + 0x24)],
    }
    volumes = []
    for record in value["volumes"]:
        volumes += [record["kind"], record["reserved"], record["angle"], *record["extents"], *record["position"],
                    record["pad"]]
    inputs = [1289, int(present), *view_inputs(value["view"]), *volumes,
              *(entry for matrix in value["matrices"] for entry in matrix), value["depth"]]
    return observation, inputs


def sphere_names(count: int) -> list[str]:
    names = [f"sine {index}" for index in range(40)] + ["events"]
    return names + [f"event {index // EVENT_WORDS} word {index % EVENT_WORDS}" for index in range(count)]


# ---------------------------------------------------------------- F01306 camera basis
def basis_vector(rng: random.Random, number: int) -> dict:
    if number < 64:
        yaw = number * 64
    elif rng.random() < 0.3:
        yaw = rng.choice((-1, -0x1000, 0x1000, 0x7FFF, -0x8000, 0x8000, 0x7FFFFFFF, -0x80000000))
    else:
        yaw = s32(rng.getrandbits(32)) if rng.random() < 0.3 else rng.randrange(-0x2000, 0x2000)
    return {"yaw": yaw, "fill": rng.getrandbits(32)}


def basis_run(machine, value: dict) -> tuple[dict, list[int]]:
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(SPHERE, 0x100)
    machine.put(SPHERE_POINTER, SPHERE)
    machine.put(SPHERE + 0x20, value["fill"])

    def sync(m) -> None:
        m.write(m.r[R["a0"]], value["yaw"] & MASK, 4)

    machine.stub(SYNC_CAMERA, sync)
    machine.follow.update((COSINE, SINE))
    machine.execute(0x800C282C)
    expected = [machine.load(BASIS + offset, 2, True) for offset in (0, 4, 0x10, 0x14)]
    expected.append(machine.load(SPHERE + 0x22, 2, True))
    problems = []
    if machine.r[R["v0"]] != BASIS:
        problems.append(f"returned {machine.r[R['v0']]:#x}, not the scratch basis")
    if machine.load(SPHERE + 0x20, 2) != value["fill"] & 0xFFFF:
        problems.append("the sphere depth halfword changed")
    observation = {
        "expected": expected,
        "problems": problems,
        "stores": list(machine.stores),
        "allowed": [STACK_SPAN, (BASIS, BASIS + 0x18), (SPHERE + 0x22, SPHERE + 0x24)],
    }
    return observation, [1306, value["yaw"]]


BASIS_NAMES = ("cosine", "sine", "negated sine", "cosine again", "yaw")


# ---------------------------------------------------------------- F01307 position cursor
def cursor_vector(rng: random.Random, number: int) -> dict:
    tables = None
    if number % 2:
        tables = {
            "offsets": [s32(rng.getrandbits(32)) for _ in range(5)],
            "sizes": [s32(rng.getrandbits(32)) for _ in range(5)],
            "texcoords": [s32(rng.getrandbits(32)) for _ in range(5)],
            "brightness": [rng.getrandbits(8) for _ in range(16)],
        }
    return {
        "view": view(rng),
        "position": [rng.randrange(-0x800, 0x800) if rng.random() < 0.8 else edge16(rng) for _ in range(3)],
        "pad": edge16(rng),
        "lock": number // 2 % 2,
        "blink": rng.getrandbits(8) if rng.random() < 0.3 else rng.randrange(16),
        "tables": tables,
        "link": rng.getrandbits(32),
    }


def cursor_run(machine, value: dict) -> tuple[dict, list[int]]:
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(PRIMITIVES, 0x100)
    machine.map_zero(ORDERING - 0x10, 0x40)
    machine.map_zero(SPRITES, 0x100)
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, ORDERING)
    machine.put(ORDERING - 4, value["link"])
    machine.store(POINTS, struct.pack("<4h", *(s16(axis) for axis in value["position"]), s16(value["pad"])))
    machine.put(BLINK, value["blink"], 1)
    tables = value["tables"]
    if tables is None:
        tables = {
            "offsets": [s32(machine.load(OFFSETS + 4 * index)) for index in range(5)],
            "sizes": [s32(machine.load(SIZES + 4 * index)) for index in range(5)],
            "texcoords": [s32(machine.load(TEXCOORDS + 4 * index)) for index in range(5)],
            "brightness": list(machine.bytes(BRIGHTNESS, 16)),
        }
    else:
        for base, name in ((OFFSETS, "offsets"), (SIZES, "sizes"), (TEXCOORDS, "texcoords")):
            for index, entry in enumerate(tables[name]):
                machine.put(base + 4 * index, entry)
        machine.store(BRIGHTNESS, bytes(tables["brightness"]))
    load_projection(machine, value["view"])
    load_matrices(machine, value["view"])
    sprites: list[list[int]] = []

    def queue(m) -> None:
        sprites.append([m.r[R["a0"]], m.r[R["a1"]], m.r[R["a2"]], m.r[R["a3"]]])
        m.r[R["v0"]] = SPRITES + 0x20 * (len(sprites) - 1)

    machine.stub(QUEUE_SPRITE, queue)
    machine.r[R["a0"]], machine.r[R["a1"]] = POINTS, value["lock"]
    machine.execute(0x800C28AC)
    quads = value["lock"] == 0
    problems = []
    expected = [machine.gte[SXY[2]], machine.byte(BLINK), int(quads)]
    if quads:
        expected += words(machine, PRIMITIVES + 4, 9) + words(machine, PRIMITIVES + 0x2C, 9)
        if machine.load(PRIMITIVES) != (value["link"] & 0xFFFFFF) | 0x09000000:
            problems.append("first quad's tag is not the old link with the quad command")
        if machine.load(PRIMITIVES + 0x28) != (PRIMITIVES & 0xFFFFFF) | 0x09000000:
            problems.append("second quad's tag does not link the first")
        if machine.load(ORDERING - 4) != PRIMITIVES + 0x28 or machine.load(SCRATCH) != PRIMITIVES + 0x50:
            problems.append("the slot or primitive cursor is not past the second quad")
    else:
        expected += [0] * 18
        if machine.load(ORDERING - 4) != value["link"] or machine.load(SCRATCH) != PRIMITIVES:
            problems.append("a locked cursor changed the slot or primitive cursor")
    for index, (brightness, xy, wh, ot) in enumerate(sprites):
        expected += [brightness, xy, wh, machine.load(SPRITES + 0x20 * index + 0x10)]
        if ot != ORDERING - 4:
            problems.append(f"sprite {index} links {ot:#x}, not the slot before the ordering table")
    observation = {
        "expected": expected,
        "problems": problems,
        "stores": list(machine.stores),
        "allowed": [STACK_SPAN, (SCRATCH, SCRATCH + 4), (PRIMITIVES, PRIMITIVES + 0x50), (ORDERING - 4, ORDERING),
                    (BLINK, BLINK + 1), *((SPRITES + 0x20 * index + 0x10, SPRITES + 0x20 * index + 0x14)
                                           for index in range(3))],
    }
    inputs = [1307, *view_inputs(value["view"]), *value["position"], value["lock"], value["blink"],
              *tables["offsets"], *tables["sizes"], *tables["texcoords"], *tables["brightness"]]
    return observation, inputs


CURSOR_NAMES = ("sxy", "blink", "quads", *(f"quad {index // 9} word {index % 9 + 1}" for index in range(18)),
                *(f"sprite {index // 4} {('brightness', 'xy', 'wh', 'texcoord')[index % 4]}" for index in range(12)))


# ---------------------------------------------------------------- F01308 terrain row
def terrain_vector(rng: random.Random, number: int) -> dict:
    width, height = rng.randrange(0, 40), rng.randrange(0, 40)
    dimensions = (width << 4) | rng.getrandbits(4) | rng.getrandbits(4) << 16 | height << 20
    if rng.random() < 0.05:
        dimensions = s32(rng.getrandbits(32))
    centre = (rng.randrange(-3, 44) << 7) | rng.getrandbits(7), (rng.randrange(-3, 44) << 7) | rng.getrandbits(7)
    if rng.random() < 0.05:
        centre = edge16(rng), edge16(rng)

    def cell() -> int:
        word = rng.getrandbits(32)
        if rng.random() < 0.7:
            word &= ~(1 << 6)
        if rng.random() < 0.5:
            word &= ~(1 << 13)
        return word

    flat = rng.random() < 0.3
    floors = []
    shared = rng.getrandbits(15)
    for _ in range(64):
        word = rng.getrandbits(32) if rng.random() < 0.2 else rng.randrange(-0x400, 0x400) & 0x7FFF | (
            rng.randrange(0, 0x800) << 15)
        if flat:
            word = (word & ~0x7FFF) | shared
        floors.append(word & MASK)
    return {
        "x": s16(centre[0]),
        "z": s16(centre[1]),
        "row": number % 9 if rng.random() < 0.9 else rng.choice((9, 10, 0x7FFFFFFF, 0xFFFFFFFF)),
        "dimensions": s32(dimensions),
        "cells": [cell() for _ in range(9)],
        "offsets": [0 if rng.random() < 0.6 else (rng.randrange(-0x100, 0x200) or 1) for _ in range(9)],
        "objects": [rng.getrandbits(8) for _ in range(9)],
        "floors": floors,
    }


def terrain_run(machine, value: dict) -> tuple[dict, list[int]]:
    first_x = (value["x"] >> 7) - 4
    machine.map_zero(SPHERE, 0x5000)
    machine.map_zero(CELLS, 0x100)
    machine.map_zero(GIMMICKS, 0x2000)
    machine.store(SPHERE + 0x4370, b"\xA5" * 0x144)
    machine.store(SPHERE + 0x4AB8, b"\x5A" * 81)
    machine.put(SPHERE_POINTER, SPHERE)
    machine.store(POINTS, struct.pack("<4h", value["x"], 0x1234, value["z"], 0x5678))
    for column in range(9):
        machine.put(CELLS + 4 * column, value["cells"][column])
        machine.put(GIMMICK_TABLE + 4 * column, GIMMICKS + 0x200 * column)
        machine.put(GIMMICKS + 0x200 * column + 0x16C, value["objects"][column], 1)
    calls: list[list[int]] = []

    def column_of(m) -> int:
        column = s32(m.r[R["a0"]]) - first_x
        if not 0 <= column < 9:
            raise Fault(f"tile x {s32(m.r[R['a0']])} is outside the row")
        return column

    def memset(m) -> None:
        calls.append([m.r[R["a0"]], m.r[R["a1"]], m.r[R["a2"]]])
        for offset in range(m.r[R["a2"]]):
            m.write(m.r[R["a0"]] + offset, m.r[R["a1"]], 1)

    def dimensions(m) -> None:
        m.r[R["v0"]] = value["dimensions"] & MASK

    def cell_pointer(m) -> None:
        m.r[R["v0"]] = CELLS + 4 * column_of(m)

    def stack_offset(m) -> None:
        m.write(m.r[R["a2"]], value["offsets"][column_of(m)] & MASK, 4)

    def top_gimmick(m) -> None:
        m.r[R["v0"]] = column_of(m)

    def floor_ceiling(m) -> None:
        x, z = m.r[R["a0"]] >> 5, m.r[R["a1"]] >> 5
        m.r[R["v0"]] = value["floors"][(x * 7 + z * 13) & 63]

    for address, handler in ((MEMSET, memset), (MAP_DIMENSIONS, dimensions), (CELL_POINTER, cell_pointer),
                             (STACK_OFFSET, stack_offset), (TOP_GIMMICK, top_gimmick),
                             (FLOOR_CEILING, floor_ceiling)):
        machine.stub(address, handler)
    machine.r[R["a0"]], machine.r[R["a1"]] = POINTS, value["row"] & MASK
    machine.execute(0x800C2B0C)
    row = value["row"] & MASK
    problems = []
    if row < 9:
        first = row * 9
        expected = [0, *words(machine, SPHERE + 0x4370 + 4 * first, 9),
                    *machine.bytes(SPHERE + 0x4AB8 + first, 9)]
        if calls[:1] != [[SPHERE + 0x4AB8 + first, 0, 9]]:
            problems.append(f"the row's flags were not cleared first: {calls[:1]}")
        allowed = [STACK_SPAN, (SPHERE + 0x4370 + 4 * first, SPHERE + 0x4370 + 4 * first + 36),
                   (SPHERE + 0x4AB8 + first, SPHERE + 0x4AB8 + first + 9)]
    else:
        expected = [1, *([0] * 18)]
        allowed = [STACK_SPAN]
    observation = {"expected": expected, "problems": problems, "stores": list(machine.stores), "allowed": allowed}
    inputs = [1308, value["x"], value["z"], row, value["dimensions"], *value["cells"], *value["offsets"],
              *value["objects"], *value["floors"]]
    return observation, inputs


TERRAIN_NAMES = ("status", *(f"height {index}" for index in range(9)), *(f"flag {index}" for index in range(9)))


# ---------------------------------------------------------------- case wiring
FUNCTIONS = {
    1287: (0x800C0990, 0x800C0B50, line_vector, line_run, LINE_NAMES),
    1289: (0x800C0D78, 0x800C0FA8, sphere_vector, sphere_run, None),
    1306: (0x800C282C, 0x800C28AC, basis_vector, basis_run, BASIS_NAMES),
    1307: (0x800C28AC, 0x800C2B0C, cursor_vector, cursor_run, CURSOR_NAMES),
    1308: (0x800C2B0C, 0x800C2E24, terrain_vector, terrain_run, TERRAIN_NAMES),
}


def check_for(names):
    def check(_image, _value, observation, native, _reference) -> list[str]:
        expected = observation["expected"]
        shown = names if names is not None else sphere_names(max(0, len(expected) - 41))
        problems = list(observation["problems"])
        if names is None and expected[40:41] == [0] and native[40:41] == [0]:
            shown = sphere_names(0)
        problems += compare(shown, native, expected)
        problems += store_problems(observation["stores"], observation["allowed"])
        return problems

    return check


def configure(namespace: dict, function: int, mutants: dict, native_variant: tuple[str, str],
              describe=None) -> None:
    entry, end, vector, run, names = FUNCTIONS[function]
    namespace.update(
        ID=f"F{function:05}",
        ENTRY=entry,
        END=end,
        MODULES=("BATTLE/BATTLE.PRG", "SLUS_010.40"),
        DRIVER="sphere_draw_driver.cpp",
        SOURCES=SOURCES,
        COMPILER="clang-cl",
        INCLUDES=(r"Native\Game\src", r"Native\Engine", r"Native\Engine\src", r"Native\Content"),
        VECTORS=512,
        SEED=function,
        MUTANT_VECTORS=256,
        MUTANTS=mutants,
        vector=vector,
        run=run,
        check=check_for(names),
        describe=describe or (lambda _image, _results: []),
        NATIVE_VARIANT=native_variant,
    )
