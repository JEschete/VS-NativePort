"""F03005 Menu::manual::DrawDiagramLines, MENUE.PRG _drawContentLines 494.c:883."""

from __future__ import annotations

import random

from differential import Machine
from manual_illustration_common import (
    DISABLED,
    DRAWN,
    DRIVER,
    GET_ANIMATION_STATE,
    HELP_TEXT,
    INCLUDES,
    INPUT,
    INPUT_SIZE,
    MODULES,
    OP_DIAGRAM_LINES,
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

ID = "F03005"
ENTRY, END = 0x80104A44, 0x80104C30
VECTORS, SEED = 512, 3005
MUTANT_VECTORS = 256
MUTANTS = {
    "zero-flag-enabled": {0x80104A70: 0x04400067},
    "header-bytes": {0x80104AB0: 0x24420014},
    "record-header": {0x80104AF4: 0x26100012},
    "content-left": {0x80104B10: 0x2442000E},
    "content-top": {0x80104B38: 0x2463FFCA},
    "green-byte": {0x80104B70: 0x92020002},
    "top-clip": {0x80104B88: 0x28620038},
    "bottom-clip": {0x80104BA8: 0x286200BB},
}
LINE_BYTES = 16
LINE_HALFWORDS = 14
LINE_FIELDS = 7


def line_y(rng: random.Random, scroll: int) -> int:
    if rng.randrange(3):
        return rng.randrange(-20, 230)
    # An edge of the clip rows 55 and 186 after scrolling.
    return signed(rng.choice((54, 55, 185, 186)) + 13 * scroll - 55, 16)


def vector(rng: random.Random, number: int) -> list[int]:
    """[scroll position, ordering-table word, help-text halfwords...]: section sizes, the line
    flag, then (after both sections) the line count and the line records."""
    scroll = rng.choice((0, 1, 2, rng.randrange(-3, 16), signed(rng.getrandbits(32))))
    while True:
        text_bytes = rng.choice((rng.randrange(0, 80), rng.randrange(-20, 160), 0))
        art_bytes = rng.choice((rng.randrange(0, 80), rng.randrange(-20, 160), 0))
        offset = (int(text_bytes / 4) + int(art_bytes / 4)) * 4 + 16
        if offset >= 12:
            break
    flag = rng.choice((1, 1, 1, 2, 0, -1, 0x7FFFFFFF, -0x80000000))
    count = rng.choice((1, 2, 3, 4, 6, 0, -1, rng.randrange(-0x8000, 0), rng.randrange(8, 40)))
    records = []
    for _ in range(max(count, 0)):
        header = animation(rng) + [int16(rng, 0, 1)]
        records.extend((*header, rng.randrange(-20, 300), line_y(rng, scroll),
                        rng.randrange(-20, 300), line_y(rng, scroll),
                        signed(rng.getrandbits(16), 16), signed(rng.getrandbits(16), 16)))
    count_index = offset // 2
    text = [signed(rng.getrandbits(16), 16) for _ in range(count_index + 1)]
    words = (text_bytes, art_bytes, flag)
    for index, word in enumerate(words):
        text[index * 2] = signed(word, 16)
        text[index * 2 + 1] = signed(word >> 16, 16)
    text[count_index] = count
    padding = [int16(rng, 0, 255) for _ in range(rng.randrange(3))]
    return [scroll, rng.getrandbits(32), *text, *records, *padding]


def run(machine: Machine, vector: list[int]) -> tuple[list, list[int]]:
    scroll, ordering_word, *text = vector
    machine.map_zero(INPUT, INPUT_SIZE)
    put_halfwords(machine, INPUT, text)
    machine.put(SCROLL_POSITION, scroll)
    machine.put(HELP_TEXT, INPUT)
    install_frame(machine, ordering_word)
    machine.follow.add(GET_ANIMATION_STATE)
    machine.execute(ENTRY)
    used = machine.load(SCRATCH) - PRIMITIVES
    problems = []
    if used < 0 or used % LINE_BYTES:
        problems.append(f"cursor advanced {used} bytes")
        used = 0
    lines, packets = [], []
    for start in range(PRIMITIVES, PRIMITIVES + used, LINE_BYTES):
        packets.append((start, 3))
        if machine.load(start + 7, 1) != 0x40:
            problems.append(f"line {start:#x} code {machine.load(start + 7, 1):#x}")
        lines.extend((machine.load(start + 8, 2, True), machine.load(start + 10, 2, True),
                      machine.load(start + 12, 2, True), machine.load(start + 14, 2, True),
                      machine.load(start + 4, 1), machine.load(start + 5, 1),
                      machine.load(start + 6, 1)))
    problems.extend(chain_problems(machine, ordering_word, packets))
    flag = (text[4] & 0xFFFF) | (text[5] & 0xFFFF) << 16
    result = DRAWN if signed(flag) > 0 else DISABLED
    observation = [result, len(text), *load_halfwords(machine, INPUT, len(text)),
                   len(lines) // LINE_FIELDS, *lines]
    return [problems, observation], [OP_DIAGRAM_LINES, scroll, len(text), *text]


def check(image, vector: list[int], observation: list, native: list[int],
          reference: list) -> list[str]:
    problems, expected = observation
    return problems + compare(native, expected)


def describe(image, results: list) -> list[str]:
    def drawn(expected: list[int]) -> int:
        return expected[2 + expected[1]]

    drew = sum(1 for _, (_, expected), _ in results if drawn(expected))
    lines = sum(drawn(expected) for _, (_, expected), _ in results)
    disabled = sum(1 for _, (_, expected), _ in results if expected[0] == DISABLED)
    return [f"coverage: {drew} vectors drew lines ({lines} lines), {disabled} were disabled"]
