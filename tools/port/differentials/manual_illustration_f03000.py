"""F03000 Menu::manual::DrawIllustration, MENUE.PRG _drawSprite 494.c:629."""

from __future__ import annotations

import random

from differential import Machine
from manual_illustration_common import (
    DRAWN,
    DRIVER,
    GET_ANIMATION_STATE,
    HIDDEN,
    INCLUDES,
    INPUT,
    INPUT_SIZE,
    MODULES,
    OP_ILLUSTRATION,
    PRIMITIVES,
    SCRATCH,
    SCROLL_POSITION,
    SOURCES,
    animation,
    chain_problems,
    compare,
    install_frame,
    int16,
    load_halfwords,
    put_halfwords,
    signed,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F03000"
ENTRY, END = 0x80103E6C, 0x80104204
VECTORS, SEED = 512, 3000
MUTANT_VECTORS = 192
MUTANTS = {
    "animation-skipped": {0x80103EA4: 0x10000006},
    "clut-column-base": {0x80103F28: 0x24420310},
    "eight-bit-stride": {0x80103F50: 0x24170004},
    "band-height": {0x80103FB0: 0x2416000F},
    "content-top": {0x80103FD0: 0x26A20038},
    # The inlined _add_int (row origin) and _min (column extent) of 494.c:619-627.
    "row-origin-subtracted": {0x80103FD4: 0x0062F023},
    "column-extent-max": {0x80103FE4: 0x02E7102A},
    "column-extent-unclipped": {0x80103FF0: 0x00000000},
    "visible-rows": {0x8010401C: 0x2C420093},
    "first-page": {0x80104090: 0x2484001E},
    "eight-bit-depth": {0x801040C0: 0x34940040},
    "page-dither": {0x80104154: 0x358C0000},
    "band-step": {0x801041BC: 0x26B50008},
}
SPRITE_BYTES, PAGE_BYTES = 20, 12
TILE_FIELDS = 8
ANIMATION = 7
TILES = 15
PAGE_COMMAND = 0xE1000200


def vector(rng: random.Random, number: int) -> list[int]:
    """[scroll position, ordering-table word, record halfwords...]: the animation header, x, y,
    width, height, tile count, colours, palette and the tile indices the record reads."""
    colors = rng.choice((16, 16, 8, 0, int16(rng, 0, 32)))
    stride = 16 if colors == 16 else 8
    width = rng.choice((rng.randrange(-4, 100), rng.randrange(1, 40), 0))
    height = rng.choice((rng.randrange(-4, 90), rng.randrange(1, 40), 0))
    cells = -(-height // 16) * -(-width // stride) if width > 0 and height > 0 else 0
    header = animation(rng) + [rng.getrandbits(16) - 0x8000]
    if number % 3 == 0:
        header[0] = 0
    x = rng.choice((rng.randrange(-40, 300), rng.randrange(0x7F00, 0x8000),
                    int16(rng, -0x8000, -0x7F00)))
    y = rng.choice((rng.randrange(-60, 140), rng.randrange(-60, 200), int16(rng, -0x8000, 0x7FFF)))
    scroll = rng.choice((0, 1, 2, rng.randrange(-3, 16), signed(rng.getrandbits(32))))
    if number % 2 and height > 0:
        # Put one band's top on an edge of the visible rows 39-184.
        edge = rng.choice((38, 39, 184, 185))
        band = 16 * rng.randrange(-(-height // 16))
        y = signed(edge - 55 - band + 13 * scroll, 16)
    tiles = [rng.choice((rng.randrange(0, 768), rng.randrange(-200, 0), int16(rng, 0, 47)))
             for _ in range(cells)]
    padding = [int16(rng, 0, 768) for _ in range(rng.randrange(3))]
    palette = rng.choice((rng.randrange(0, 256), int16(rng, -40, 300)))
    record = [*header, x, y, width, height, int16(rng, 0, 40), colors, palette, *tiles, *padding]
    return [scroll, rng.getrandbits(32), *record]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    scroll, ordering_word, *record = vector
    machine.map_zero(INPUT, INPUT_SIZE)
    put_halfwords(machine, INPUT, record)
    machine.put(SCROLL_POSITION, scroll)
    install_frame(machine, ordering_word)
    machine.follow.add(GET_ANIMATION_STATE)
    machine.r[4] = INPUT
    machine.execute(ENTRY)
    after = load_halfwords(machine, INPUT, ANIMATION)
    used = machine.load(SCRATCH) - PRIMITIVES
    problems = []
    if used < 0 or used % (SPRITE_BYTES + PAGE_BYTES):
        problems.append(f"cursor advanced {used} bytes")
        used = 0
    tiles, packets = [], []
    for start in range(PRIMITIVES, PRIMITIVES + used, SPRITE_BYTES + PAGE_BYTES):
        page = start + SPRITE_BYTES
        packets.extend(((start, 4), (page, 2)))
        if machine.load(start + 7, 1) != 0x65:
            problems.append(f"sprite {start:#x} code {machine.load(start + 7, 1):#x}")
        command = machine.load(page + 4)
        if command & ~0x1FF != PAGE_COMMAND or machine.load(page + 8):
            problems.append(f"page packet {page:#x} words {command:#x} {machine.load(page + 8):#x}")
        tiles.extend((machine.load(start + 8, 2, True), machine.load(start + 10, 2, True),
                      machine.load(start + 12, 1), machine.load(start + 13, 1),
                      machine.load(start + 14, 2), machine.load(start + 16, 2, True),
                      machine.load(start + 18, 2, True), command & 0x1FF))
    problems.extend(chain_problems(machine, ordering_word, packets))
    hidden = record[0] != 0 and after[6] != 0
    observation = [HIDDEN if hidden else DRAWN, *after, len(tiles) // TILE_FIELDS, *tiles]
    return [problems, observation], [OP_ILLUSTRATION, scroll, len(record), *record]


def check(image, vector: list[int], observation: list, native: list[int],
          reference: list) -> list[str]:
    problems, expected = observation
    return problems + compare(native, expected)


def describe(image, results: list) -> list[str]:
    drawn = sum(1 for _, (_, expected), _ in results if expected[0] == DRAWN and expected[8])
    hidden = sum(1 for _, (_, expected), _ in results if expected[0] == HIDDEN)
    tiles = sum(expected[8] for _, (_, expected), _ in results)
    return [f"coverage: {drawn} vectors drew tiles ({tiles} tiles), {hidden} were hidden"]
