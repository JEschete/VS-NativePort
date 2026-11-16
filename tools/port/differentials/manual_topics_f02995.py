"""F02995 Menu::manual::ComposePageText, MENUE.PRG _copyNextPageText 494.c:531."""

from __future__ import annotations

import random

from differential import Machine
from manual_topics_common import (
    COPY_ENCODED_LINE,
    CXX_STANDARD,
    DRIVER,
    HELP_TEXT,
    INCLUDES,
    INPUT,
    MODULES,
    NEGATIVE_SCROLL,
    OP_COMPOSE_PAGE,
    PAGE_BUFFER,
    PAGE_CAPACITY,
    PAGE_OVERFLOW,
    READ_REFUSALS,
    SCROLL_POSITION,
    SET_TEXT_BOX,
    SOURCES,
    byte_stores,
    compare,
    encoded_line,
    mutants,
    run_original,
    signed,
    verify_original_words,
)

__all__ = ("CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F02995"
ENTRY, END = 0x80103BC8, 0x80103C9C
VECTORS, SEED = 768, 2995
MUTANTS = mutants(ID)
LINE_TABLE = 16
SHOWN = 5


def help_text(rng: random.Random, scroll: int) -> list[int]:
    """A help file: a header, the line count, the line table and the lines it points at."""
    if rng.randrange(24) == 0:
        return [rng.randrange(256) for _ in range(rng.randrange(18))]
    lines = rng.randrange(0, 25)
    count = rng.choice((lines, lines, lines, rng.randrange(0x8000, 0x10000), rng.randrange(0, lines + 1)))
    needed = min(count, scroll + 10) if 0 <= scroll < 0x1000 else 0
    entries = max(needed, lines)
    if rng.randrange(16) == 0:
        entries = rng.randrange(0, entries + 1)
    data = [rng.randrange(256) for _ in range(LINE_TABLE)] + [count & 0xFF, count >> 8] + [0] * (entries * 2)
    long_lines = rng.randrange(6) == 0
    starts = []
    for _ in range(rng.randrange(1, 7)):
        if len(data) % 2:
            data.append(rng.randrange(256))
        starts.append((len(data) - LINE_TABLE) // 2)
        length = rng.randrange(80, 260) if long_lines else rng.choice((0, rng.randrange(1, 12), rng.randrange(1, 40)))
        data += encoded_line(rng, length)
    for entry in range(entries):
        start = rng.choice(starts) if rng.randrange(40) else rng.randrange(0x10000)
        data[LINE_TABLE + 2 + entry * 2:LINE_TABLE + 4 + entry * 2] = (start & 0xFF, start >> 8)
    if rng.randrange(10) == 0:
        # The last line runs to the end of the file without its terminator.
        data.pop()
    return data


def vector(rng: random.Random, number: int) -> list[int]:
    """[scroll position, help text bytes...]: the help text ends where the overlay ends."""
    scroll = rng.choice((0, 0, 1, 2, 9, 10, 11, 20, rng.randrange(0, 30), rng.randrange(-3, 0),
                         0x7FFFFFFF - rng.randrange(12), signed(rng.getrandbits(32))))
    return [scroll, *help_text(rng, scroll)]


def run(machine: Machine, vector: list[int]) -> tuple[dict, list[int]]:
    scroll, *data = vector
    machine.store(INPUT, bytes(data))
    machine.put(SCROLL_POSITION, scroll)
    machine.put(HELP_TEXT, INPUT)
    machine.follow.add(COPY_ENCODED_LINE)
    calls: list[tuple[int, int, list[int]]] = []

    def set_text_box(m: Machine) -> None:
        stores = byte_stores(m)
        length = max(stores) + 1 - m.r[5] if stores else 0
        shown = max(length, SHOWN) if 0 < length <= PAGE_CAPACITY else 0
        calls.append((signed(m.r[4]), m.r[5], list(m.bytes(m.r[5], shown)), length))

    machine.stub(SET_TEXT_BOX, set_text_box)
    faulted = run_original(machine, ENTRY)
    overflow = any(address >= PAGE_BUFFER + PAGE_CAPACITY for address in byte_stores(machine))
    observation = {"faulted": faulted, "overflow": overflow, "calls": calls}
    return observation, [OP_COMPOSE_PAGE, scroll, len(data), *data]


def check(image, vector: list[int], observation: dict, native: list[int],
          reference: dict) -> list[str]:
    if vector[0] < 0:
        # The original reads line offsets before its table; the native code refuses the scroll.
        return compare(native, [NEGATIVE_SCROLL])
    if observation["overflow"]:
        # The original writes past its 1024-byte stack buffer; the native code refuses.
        return compare(native, [PAGE_OVERFLOW])
    if observation["faulted"]:
        # The original reads past the help text; the native code refuses at that read.
        if len(native) == 1 and native[0] in READ_REFUSALS:
            return []
        return [f"the original read past the help text; native printed {native[:4]}"]
    if len(observation["calls"]) != 1:
        return [f"the original called vs_battle_setTextBox {len(observation['calls'])} times"]
    box, buffer, page, length = observation["calls"][0]
    problems = []
    if box != 0 or buffer != PAGE_BUFFER:
        problems.append(f"vs_battle_setTextBox({box}, {buffer:#x}) not (0, {PAGE_BUFFER:#x})")
    if not native or native[0] != 0 or len(native) < 2 or native[1] != len(page):
        return [*problems, f"native {native[:4]}, original page of {length} bytes"]
    return problems + compare(native[2:], page)


def describe(image, results: list) -> list[str]:
    verify_original_words(ID, image)
    pages = [observation["calls"][0][3] for vector, observation, _ in results
             if vector[0] >= 0 and not observation["faulted"] and observation["calls"]]
    empty = sum(1 for length in pages if length == SHOWN)
    overflow = sum(1 for vector, observation, _ in results if vector[0] >= 0 and observation["overflow"])
    faulted = sum(1 for vector, observation, _ in results
                  if vector[0] >= 0 and observation["faulted"] and not observation["overflow"])
    return [f"coverage: {len(pages)} pages ({empty} empty, longest {max(pages, default=0)} bytes), "
            f"{overflow} overflowed, {faulted} read past the help text"]
