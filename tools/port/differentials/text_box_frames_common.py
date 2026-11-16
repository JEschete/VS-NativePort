"""Original BATTLE header, backdrop, point reflection and shaped-frame draws.

The original follows the resident sprite, primitive and reflection instructions.
Native runs the owned frame cores and packet builders. Compare every emitted
word, the cursor and all three chain heads. The native VM lease/refusal paths
and output capacities are exercised by TextBoxFrameTests, not this valid-data
comparison. Packet arenas start zeroed; arbitrary prior unwritten packet
contents are outside this comparison. Temporary PS1 point storage is not part
of the native interface.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    mode_case,
    recorded_observation,
    s32,
)

HEADER, BACKDROP, REFLECT, FRAME = range(4)
MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "text_box_frames_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\TextBoxFrames.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
    r"Native\Engine\src\Render\UiPackets.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)

BOXES = 0x800F4FE0
BOX_BYTES = 0x30
TABLE_POINTER = 0x800F51B8
STYLE_POINTERS = 0x800EB588
HEADER_SIZES = 0x800EC2CC
HEADER_TEXTURES = 0x800EC2D8
FRAME_BUFFER = 0x8005E210
SCRATCH = 0x1F800000
PRIMITIVES = 0x80600000
PRIMITIVE_BYTES = 0x8000
TABLE = 0x80700000
HEADER_CHAIN = TABLE + 8 * 4 * 4
SHAPE = 0x80800000
FOLLOW = (0x800C0214, 0x800C0230, 0x800CCCB8, 0x800CCD00, 0x800CD0FC, 0x800CD3A0)
BOUNDARIES = (0, 1, -1, 127, 128, 255, 256, 32767, -32768, 0x7FFFFFFF, -0x80000000)
BOX_FIELDS = ((0x16, 2), (0x24, 2), (0x26, 2), (0x28, 2), (0x2A, 2),
              (0x21, 1), (0x22, 1), (0x1C, 2), (0x1E, 1))


def shape(rng: random.Random, number: int) -> list[int]:
    layouts = (
        (3, 0, 3, 1, 0, 0, 255, 0, 0, 255, 0, 1, 2, 255),
        (4, 0, 4, 1, 0, 0, 255, 0, 255, 255, 0, 255, 0, 1, 2, 3),
        (8, 2, 4, 1, 0, 1, 2, 1, 2, 3, 2, 1,
         0, 0, 255, 0, 255, 255, 0, 255, 0, 32, 32, 32, 0, 224, 224, 224,
         0, 1, 2, 3, 0, 4, 5, 1, 2, 6, 7, 3),
    )
    result = list(layouts[number % len(layouts)])
    first_vertex = (result[1] + 1) * 4
    if number & 1:
        for index in range(first_vertex, first_vertex + result[0] * 2):
            result[index] = rng.randrange(256)
    if result[1] and number % 4:
        result[first_vertex + result[0] * 2 + 3] = 255
    # The original reads the selected descriptor before checking the part.
    # Retain defined enclosing storage for all sixteen encoded part values.
    result.extend(rng.randrange(256) for _ in range(64 - len(result)))
    return result


def vector(rng: random.Random, number: int, mode: int) -> dict:
    box = [
        rng.choice((0, 1, -1, 32767)),
        rng.randrange(-32768, 32768), rng.randrange(-32768, 32768),
        rng.randrange(-32768, 32768), rng.randrange(-32768, 32768),
        rng.randrange(256), rng.randrange(256), rng.randrange(-20, 280),
        rng.randrange(256), number % 4, rng.randrange(16), number % 16,
    ]
    if mode == BACKDROP:
        scenario = number % 8
        box[5] = rng.randrange(1, 6)
        box[4] = rng.randrange(0, 90)
        if scenario == 0:
            box[0] = 1
        elif scenario == 1:
            box[0] = 0
        elif scenario == 2:
            box[5] = rng.randrange(256)
            box[4] = box[5] * 13 + 4 - rng.choice((-1, 0, 3, 4, 5, 15, 16, 17))
        elif scenario == 3:
            box[4] = rng.choice((0, 1, 127, 128, 239, 240))
        elif scenario == 4:
            box[0], box[4], box[6] = 1, 20, 255
        elif scenario == 5:
            box[5] = rng.randrange(128, 256)
            box[4] = box[5] * 13
        elif scenario == 6:
            box[4] = rng.choice((-32768, -1, 32767))
        box[7] = box[6] + 1 if number % 3 else box[6]
        box[10] = number % 5
    first = rng.choice((*BOUNDARIES, s32(rng.getrandbits(32))))
    second = rng.choice((0, 1, 2, 3, 4, 255, -1, s32(rng.getrandbits(32))))
    if mode == HEADER:
        second = number % 5 if number % 3 else second
    return {
        "mode": mode, "args": [first, second], "index": rng.randrange(8),
        "frame": rng.choice((0, 0, 1, 2, -1, -0x80000000)),
        "box": box, "sizes": [rng.getrandbits(32) for _ in range(3)],
        "textures": [rng.getrandbits(32) for _ in range(3)],
        "start": rng.randrange(16), "heads": [rng.getrandbits(32) for _ in range(3)],
        "shape": shape(rng, number) if mode == FRAME else [],
    }


def entries(value: dict) -> list[int]:
    base = TABLE + value["index"] * 16
    return [HEADER_CHAIN, base + 4, base + 8]


def run(machine, value: dict, entry: int):
    machine.follow.update(FOLLOW)
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(PRIMITIVES, PRIMITIVE_BYTES)
    machine.map_zero(TABLE, 8 * 16 + 4)
    machine.map_zero(BOXES, 8 * BOX_BYTES)
    box = BOXES + value["index"] * BOX_BYTES
    for (offset, size), field in zip(BOX_FIELDS, value["box"], strict=False):
        machine.put(box + offset, field, size)
    alignment, style, part = value["box"][9:]
    machine.put(box, (alignment << 6) | (style << 8) | (part << 12))
    machine.put(TABLE_POINTER, TABLE)
    machine.put(FRAME_BUFFER, value["frame"])
    machine.put(SCRATCH, PRIMITIVES + value["start"] * 4)
    machine.put(SCRATCH + 8, HEADER_CHAIN)
    for address, head in zip(entries(value), value["heads"], strict=True):
        machine.put(address, head)
    for index, (size, texture) in enumerate(zip(value["sizes"], value["textures"], strict=True)):
        machine.put(HEADER_SIZES + index * 4, size)
        machine.put(HEADER_TEXTURES + index * 4, texture)
    machine.store(SHAPE, bytes(value["shape"]))
    machine.put(STYLE_POINTERS + style * 4, SHAPE)
    if value["mode"] in (HEADER, REFLECT):
        machine.r[4], machine.r[5] = (item & 0xFFFFFFFF for item in value["args"])
    else:
        machine.r[4] = value["index"]
    machine.execute(entry)
    if value["mode"] == REFLECT:
        values, names = [s32(machine.r[2])], ["reflected point"]
    else:
        start = PRIMITIVES + value["start"] * 4
        end = machine.load(SCRATCH)
        count = (end - start) // 4
        values = [end - PRIMITIVES, count,
                  *(s32(machine.load(start + index * 4)) for index in range(count)),
                  *(s32(machine.load(address)) for address in entries(value))]
        names = ["cursor", "word count", *(f"word {index}" for index in range(count)),
                 "header head", "backdrop head", "frame head"]
    inputs = [value["mode"], *value["args"], value["frame"], *value["box"],
              *(s32(item) for item in value["sizes"]), *(s32(item) for item in value["textures"]),
              value["start"], *(s32(item) for item in value["heads"]),
              len(value["shape"]), *value["shape"]]
    return recorded_observation(machine, values, names, []), inputs


def allowed_stores(_value: dict):
    return [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
            (PRIMITIVES, PRIMITIVES + PRIMITIVE_BYTES),
            (TABLE, HEADER_CHAIN + 4), (SCRATCH, SCRATCH + 4),
            (SCRATCH + 0x88, SCRATCH + 0x88 + 255 * 4)]


def describe(_image, results):
    if results[0][0]["mode"] == REFLECT:
        return [f"{len(results)} complete 32-bit reflection results compared"]
    words = sum(observation["values"][1] for value, observation, _ in results
                if value["mode"] != REFLECT)
    return [f"{len(results)} vectors; {words} complete packet words and all chain heads compared",
            "Original follows resident drawing helpers; no original algorithm or callee is stubbed."]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict):
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]) | {
        "VECTORS": 1024, "MUTANT_VECTORS": 96,
    }
