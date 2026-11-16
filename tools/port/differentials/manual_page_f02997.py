"""F02997 Menu::Manual::DrawPageContent, MENUE.PRG _drawContent 494.c:577."""

from __future__ import annotations

import random

from differential import Machine
from manual_page_common import (
    DRAW_CONTENT_LINES,
    DRAW_ENVIRONMENT_BYTES,
    DRAW_ENVIRONMENTS,
    DRAW_SPRITE,
    DRAWN,
    DRIVER,
    EVENT_ILLUSTRATION,
    EVENT_LINES,
    FRAME_BUFFER,
    HELP_TEXT,
    INCLUDES,
    INPUT,
    INPUT_SIZE,
    MODULES,
    OP_CONTENT,
    REFUSED,
    SCRATCH,
    SET_DRAW_AREA,
    SOURCES,
    check,
    counts,
    events,
    install_frame,
    int16,
    signed,
)

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES", "check")

ID = "F02997"
ENTRY, END = 0x80103CF0, 0x80103E6C
VECTORS, SEED = 384, 2997
MUTANT_VECTORS = 192
MUTANTS = {
    "front-environment": {0x80103D18: 0x24420000},
    "text-size-floored": {0x80103D78: 0x24420000},
    "list-after-header-word": {0x80103D84: 0x24420014},
    "empty-list-entered": {0x80103D94: 0x00000000},
    "record-word-offsets": {0x80103DA8: 0x00042080},
    "right-buffer-edge": {0x80103E00: 0x28420141},
    "left-clip": {0x80103E08: 0x2402000E},
    "right-buffer-clip": {0x80103E0C: 0x2402014C},
    "clip-width": {0x80103E20: 0x2402012D},
    "clip-top": {0x80103E28: 0x24020038},
    "clip-height": {0x80103E30: 0x24020083},
}
HEADER = 8
AREA_BYTES = 12
DRAW_AREA_WORDS = 2


def list_start(text: list[int]) -> int | None:
    """The illustration list's halfword index, or None where the original reads past `text`."""
    if len(text) < 2:
        return None
    size = signed((text[0] & 0xFFFF) | (text[1] & 0xFFFF) << 16)
    steps = int(size / 4)
    start = HEADER + 2 * steps
    if steps < 0 or start >= len(text):
        return None
    count = text[start] & 0xFFFF
    if count >= len(text) - start:
        return None
    if any(start + (text[start + 1 + index] & 0xFFFF) >= len(text) for index in range(count)):
        return None
    return start


def help_text(rng: random.Random, number: int) -> list[int]:
    """A help file: header, a text section of `size` bytes and the list of record offsets."""
    size = rng.choice((0, 1, 3, 4, 8, rng.randrange(0, 64), -1, -3))
    if number % 32 == 5:
        size = rng.choice((-4, -5, signed(rng.getrandbits(32)), 4 * 0x4000))
    steps = max(0, int(size / 4))
    count = rng.choice((0, 1, 2, rng.randrange(0, 6)))
    records = [rng.randrange(count + 1, count + 12) for _ in range(count)]
    tail = [int16(rng, 0, 40) for _ in range(rng.randrange(1, 14))]
    text = [size & 0xFFFF, signed(size >> 16 & 0xFFFF, 16)]
    # A text section too long to hold puts the list past the file.
    text += [int16(rng, 0, 100) for _ in range(HEADER - 2 + 2 * min(steps, 0x100))]
    text += [count, *records, *tail]
    if number % 16 == 7:
        # Cut the file short so the list or a record falls outside it.
        text = text[:rng.randrange(1, len(text) + 1)]
    elif number % 16 == 9 and count:
        text[HEADER + 2 * steps + 1 + rng.randrange(count)] = rng.choice((len(text), -1, 0x7FFF))
    return text


def vector(rng: random.Random, number: int) -> list[int]:
    """[clip x, y, width, height, Render_frameBuf, help text halfwords...]: the back buffer's
    draw environment clip, each side of the right framebuffer edge."""
    x = rng.choice((0, 320, 319, 321, int16(rng, -40, 700)))
    clip = [x, int16(rng, 0, 240), int16(rng, 0, 320), int16(rng, 0, 240)]
    frame_buffer = rng.choice((0, 1, rng.getrandbits(32)))
    return [*clip, frame_buffer, *help_text(rng, number)]


def set_draw_area(m: Machine) -> None:
    """The draw area packet in the driver's raw form: the rectangle's halfword pairs."""
    packet, rect = m.r[4], m.r[5]
    m.put(packet + 3, DRAW_AREA_WORDS, 1)
    m.put(packet + 4, m.load(rect))
    m.put(packet + 8, m.load(rect + 4))


def run(machine: Machine, vector: list[int]) -> tuple[list, list[int]]:
    clip, frame_buffer, text = vector[:4], vector[4], vector[5:]
    inputs = [OP_CONTENT, *clip, len(text), *text]
    if list_start(text) is None:
        # The original reads its list or records past the file; the native code refuses.
        return [[], [REFUSED]], inputs
    install_frame(machine, 3 * AREA_BYTES)
    machine.map_zero(INPUT, INPUT_SIZE)
    for index, value in enumerate(text):
        machine.put(INPUT + index * 2, value, 2)
    machine.put(HELP_TEXT, INPUT)
    machine.put(FRAME_BUFFER, frame_buffer)
    back = DRAW_ENVIRONMENTS + ((frame_buffer + 1) & 1) * DRAW_ENVIRONMENT_BYTES
    front = DRAW_ENVIRONMENTS + (frame_buffer & 1) * DRAW_ENVIRONMENT_BYTES
    for index, value in enumerate(clip):
        machine.put(front + index * 2, ~value, 2)
        machine.put(back + index * 2, value, 2)
    calls: list[tuple[int, list[int]]] = []
    machine.stub(SET_DRAW_AREA, set_draw_area)
    machine.stub(DRAW_CONTENT_LINES, lambda m: calls.append((m.load(SCRATCH), [EVENT_LINES])))

    def draw_sprite(m: Machine) -> None:
        offset = m.r[4] - INPUT
        calls.append((m.load(SCRATCH), [EVENT_ILLUSTRATION, offset // 2 if offset % 2 == 0 else -1]))

    machine.stub(DRAW_SPRITE, draw_sprite)
    machine.execute(ENTRY)
    problems: list[str] = []
    return [problems, [DRAWN, *events(machine, problems, calls)]], inputs


def describe(image, results: list) -> list[str]:
    drawn, refused, _ = counts(results)
    illustrations = sum(record_count(vector[5:]) for vector, (_, expected), _ in results
                        if expected[0] == DRAWN)
    right = sum(1 for vector, (_, expected), _ in results
                if expected[0] == DRAWN and signed(vector[0], 16) >= 320)
    return [f"coverage: {drawn} pages ({illustrations} illustrations, {right} on the right "
            f"framebuffer), {refused} lists outside the file refused"]


def record_count(text: list[int]) -> int:
    start = list_start(text)
    return 0 if start is None else text[start] & 0xFFFF
