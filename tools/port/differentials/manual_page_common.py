"""Shared original-instruction and native-driver support for the MENUE manual page frame."""

from __future__ import annotations

import random

import differential
from differential import Machine
from manual_illustration_common import add_prim, compare, int16, signed

MODULE = "MENU/MENUE.PRG"
MODULE_BASE = 0x80102800
MODULES = (MODULE,)
DRIVER = "manual_page_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\ManualPage.cpp",)
INCLUDES = (
    r"Native\Engine\include",
    r"Native\Game\include",
)

differential.MODULE_BASES[MODULE] = MODULE_BASE

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES", "int16")

# Original callees and data.
ADD_PRIM = 0x8002B4D4
SET_DRAW_AREA = 0x800293DC
DRAW_CONTENT_LINES = 0x80104A44
DRAW_SPRITE = 0x80103E6C
DRAW_PAGINATION_ARROW = 0x80104908
SCRATCH = 0x1F800000
FRAME_BUFFER = 0x8005E210
DRAW_ENVIRONMENTS = 0x8005E0D0
DRAW_ENVIRONMENT_BYTES = 0x5C
PREVIOUS_BUTTONS = 0x8005E1C0
SQUARE = 0x80
PAGE_ARROW_STATE = 0x8010524C
HELP_TEXT = 0x80105254

# Scratch memory for the inputs, the primitive buffer and the ordering table.
INPUT = 0x80200000
INPUT_SIZE = 0x10000
PRIMITIVES = 0x80280000
PRIMITIVES_SIZE = 0x20000
# Bytes the original never wrote keep this pattern.
UNWRITTEN = 0xA5
ORDERING_TABLE = 0x80300000
ORDERING_ENTRIES = 32
# Render_frameScratch.orderingTable points at entry 16, so slots -16..15 are in the table.
SCENE_ENTRY = 16

# Driver operations, shared with manual_page_driver.cpp.
OP_CONTENT, OP_BACKGROUND, OP_UPPER_FADE, OP_LOWER_FADE, OP_ARROW, OP_CONTROLS = 1, 2, 3, 4, 5, 6
# Native PageDrawResult values.
DRAWN, REFUSED = 0, 1
# Observation events.
EVENT_PACKET, EVENT_LINES, EVENT_ILLUSTRATION = 1, 2, 3

LOW24 = 0x00FFFFFF
HIGH8 = 0xFF000000
RAW_TEXTURE = 0x01

# Per packet body length, the body bytes (offsets from the first word after the tag) the
# original never writes: the padding bytes of the second, third and fourth colours and of the
# third and fourth texel words.
UNWRITTEN_BYTES = {
    8: (11, 19, 27),
    9: (26, 27, 34, 35),
    4: (11,),
}
# A raw-texture POLY_FT4 has no colour.
RAW_TEXTURE_COLOR = (0, 1, 2)


def int32(rng: random.Random, low: int, high: int) -> int:
    """Mostly in [low, high], sometimes any word."""
    return rng.randrange(low, high + 1) if rng.randrange(8) else signed(rng.getrandbits(32))


def arrow_state(rng: random.Random) -> int:
    """_pageArrowAnimState, biased to its 0-15 cycle and the bounce edge at 9."""
    return rng.choice((rng.randrange(16), rng.randrange(16), 8, 9, rng.randrange(-40, 60),
                       signed(rng.getrandbits(32))))


def buttons(rng: random.Random) -> int:
    return rng.choice((0, SQUARE, rng.getrandbits(16), rng.getrandbits(32)))


def sentinel(entry: int) -> int:
    """A distinct ordering-table word per entry, outside the primitive buffer."""
    return 0x5A000000 | (0x00E00000 + entry * 4)


def install_frame(machine: Machine, written: int) -> None:
    """Map the scratch cursor and ordering-table pointer, a primitive buffer whose first
    `written` bytes hold the unwritten pattern, and a table of sentinel entries."""
    machine.map_zero(SCRATCH, 8)
    machine.map_zero(PRIMITIVES, PRIMITIVES_SIZE)
    machine.store(PRIMITIVES, bytes([UNWRITTEN]) * written)
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, ORDERING_TABLE + SCENE_ENTRY * 4)
    for entry in range(ORDERING_ENTRIES):
        machine.put(ORDERING_TABLE + entry * 4, sentinel(entry))
    machine.stub(ADD_PRIM, add_prim)


def packets(machine: Machine, problems: list[str]) -> list[tuple[int, list[int]]]:
    """Each packet from the buffer start to the cursor, (address, body words), with the bytes the
    original never wrote checked for the pattern and cleared."""
    cursor = machine.load(SCRATCH)
    if not PRIMITIVES <= cursor <= PRIMITIVES + PRIMITIVES_SIZE:
        problems.append(f"cursor {cursor:#x} outside the primitive buffer")
        return []
    found, address = [], PRIMITIVES
    while address < cursor:
        length = machine.load(address + 3, 1)
        if not length or address + 4 + length * 4 > cursor:
            problems.append(f"packet {address:#x} length {length} overruns the cursor {cursor:#x}")
            break
        body = bytearray(machine.bytes(address + 4, length * 4))
        unwritten = list(UNWRITTEN_BYTES.get(length, ()))
        if length == 9 and body[3] & RAW_TEXTURE:
            unwritten.extend(RAW_TEXTURE_COLOR)
        for offset in unwritten:
            if body[offset] != UNWRITTEN:
                problems.append(f"packet {address:#x} body byte {offset} written {body[offset]:#x}")
            body[offset] = 0
        found.append((address, [int.from_bytes(body[index:index + 4], "little")
                                for index in range(0, len(body), 4)]))
        address += 4 + length * 4
    return found


def slots(machine: Machine, found: list[tuple[int, list[int]]], problems: list[str]) -> dict[int, int]:
    """Walk every ordering-table chain: each packet's slot, checking that each chain holds its
    packets in reverse emission order and ends at the entry's sentinel."""
    addresses = {address & LOW24: address for address, _ in found}
    slot_of: dict[int, int] = {}
    for entry in range(ORDERING_ENTRIES):
        word = machine.load(ORDERING_TABLE + entry * 4)
        if word & HIGH8 != sentinel(entry) & HIGH8:
            problems.append(f"table entry {entry} high byte {word >> 24:#x}")
        link, previous = word & LOW24, None
        while link in addresses:
            address = addresses.pop(link)
            if previous is not None and address >= previous:
                problems.append(f"slot {entry - SCENE_ENTRY} links {previous:#x} before {address:#x}")
            slot_of[address] = entry - SCENE_ENTRY
            previous = address
            link = machine.load(address) & LOW24
        if link != sentinel(entry) & LOW24:
            problems.append(f"table entry {entry} chain ends at {link:#x}")
    for address in addresses.values():
        problems.append(f"packet {address:#x} is in no table chain")
    return slot_of


def events(machine: Machine, problems: list[str],
           calls: list[tuple[int, list[int]]] = ()) -> list[int]:
    """The packets, as [1, slot, length, body...], merged with `calls`, (cursor at the call,
    event), each before the packets queued after it."""
    found = packets(machine, problems)
    slot_of = slots(machine, found, problems)
    ordered = [(address, 1, index, [EVENT_PACKET, slot_of.get(address, 99), len(body), *body])
               for index, (address, body) in enumerate(found)]
    ordered += [(cursor, 0, index, event) for index, (cursor, event) in enumerate(calls)]
    return [value for *_, event in sorted(ordered, key=lambda item: item[:3]) for value in event]


def check(image, vector: list[int], observation: list, native: list[int],
          reference: list) -> list[str]:
    problems, expected = observation
    return problems + compare(native, expected)


def put_arrow(machine: Machine, state: int, held: int) -> int:
    """Store the arrow globals; returns the driver's Square flag."""
    machine.put(PAGE_ARROW_STATE, state)
    machine.put(PREVIOUS_BUTTONS, held)
    return int(bool(held & SQUARE))


# A fade: the arrow quad, 16 rows of 3 chunk quads, 16 tiles and the texture page.
FADE_BYTES = 40 + 16 * 3 * 40 + 16 * 16 + 12


def fade_vector(rng: random.Random) -> list[int]:
    """[_pageArrowAnimState, Input_previousButtons]."""
    return [arrow_state(rng), buttons(rng)]


def run_fade(machine: Machine, vector: list[int], entry: int, operation: int) -> tuple[list, list[int]]:
    state, held = vector
    install_frame(machine, FADE_BYTES)
    square = put_arrow(machine, state, held)
    machine.execute(entry)
    problems: list[str] = []
    return [problems, [DRAWN, *events(machine, problems)]], [operation, state, square]


def fade_describe(results: list) -> list[str]:
    drawn, _, queued = counts(results)
    return [f"coverage: {drawn} fades, {queued} packets"]


def counts(results: list) -> tuple[int, int, int]:
    """(drawn vectors, refused vectors, packets) over the original observations."""
    drawn = refused = queued = 0
    for _, (_, expected), _ in results:
        if expected[0] == REFUSED:
            refused += 1
            continue
        drawn += 1
        index = 1
        while index < len(expected):
            kind = expected[index]
            if kind == EVENT_PACKET:
                queued += 1
                index += 3 + expected[index + 2]
            else:
                index += 2 if kind == EVENT_ILLUSTRATION else 1
    return drawn, refused, queued
