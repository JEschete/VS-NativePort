"""Shared original-instruction and native-driver support for the MENUE manual illustrations."""

from __future__ import annotations

import random

import differential
from differential import Machine

MODULE = "MENU/MENUE.PRG"
MODULE_BASE = 0x80102800
MODULES = (MODULE,)
DRIVER = "manual_illustration_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\ManualIllustration.cpp",)
INCLUDES = (
    r"Native\Engine\include",
    r"Native\Game\include",
)

differential.MODULE_BASES[MODULE] = MODULE_BASE

__all__ = ("DRIVER", "INCLUDES", "MODULES", "SOURCES")

# Original callees and data.
LOAD_IMAGE = 0x8002AD9C
ADD_PRIM = 0x8002B4D4
GET_ANIMATION_STATE = 0x80104C30
SCROLL_POSITION = 0x80105244
HELP_TEXT = 0x80105254
SCRATCH = 0x1F800000

# Scratch memory for the inputs, the primitive buffer and the ordering table.
INPUT = 0x80200000
INPUT_SIZE = 0x10000
PRIMITIVES = 0x80280000
PRIMITIVES_SIZE = 0x20000
ORDERING_TABLE = 0x80300000

# Driver operations, shared with manual_illustration_driver.cpp.
OP_UPLOAD_SPRITES = 1
OP_UPLOAD_CLUTS = 2
OP_ANIMATION = 5
OP_ILLUSTRATION = 6
OP_DIAGRAM_LINES = 7

# Native DrawResult values.
DRAWN, HIDDEN, DISABLED, REFUSED = 0, 1, 2, 3
# Native UploadResult values.
UPLOADED, TOO_MANY_IMAGES = 0, 1

LOW24 = 0x00FFFFFF
HIGH8 = 0xFF000000


def signed(value: int, bits: int = 32) -> int:
    value &= (1 << bits) - 1
    return value - (1 << bits) if value >> (bits - 1) else value


def int16(rng: random.Random, low: int, high: int) -> int:
    """Mostly in [low, high], sometimes any halfword."""
    return rng.randrange(low, high + 1) if rng.randrange(8) else signed(rng.getrandbits(16), 16)


def animation(rng: random.Random) -> list[int]:
    """The seven AnimationState_t halfwords, biased to the deciding values."""
    return [
        rng.choice((0, 0, 1, int16(rng, -2, 2))),
        signed(rng.getrandbits(16), 16),
        rng.choice((-1, 0, 1, 1, 2, 3, int16(rng, -2, 6))),
        rng.choice((0, 1, -1, int16(rng, 0, 1))),
        rng.choice((0, 7, 14, 15, 15, 16, 17, -1, int16(rng, -40, 20))),
        rng.choice((-1, 0, 0, 1, 2, 3, 0x7FFF, int16(rng, -3, 6))),
        rng.choice((0, 1, int16(rng, 0, 1))),
    ]


def scroll(rng: random.Random) -> int:
    return rng.randrange(-3, 16) if rng.randrange(8) else signed(rng.getrandbits(32))


def put_halfwords(machine: Machine, address: int, values: list[int]) -> None:
    for index, value in enumerate(values):
        machine.put(address + index * 2, value, 2)


def load_halfwords(machine: Machine, address: int, count: int) -> list[int]:
    return [machine.load(address + index * 2, 2, True) for index in range(count)]


def add_prim(m: Machine) -> None:
    """libgpu AddPrim: the packet takes the table entry's link, the entry the packet's."""
    table, packet = m.r[4], m.r[5]
    entry = m.load(table)
    m.put(packet, (m.load(packet) & HIGH8) | (entry & LOW24))
    m.put(table, (entry & HIGH8) | (packet & LOW24))


def install_frame(machine: Machine, ordering_word: int) -> None:
    """Map the scratch cursor and ordering-table pointers, the primitive buffer and the table."""
    machine.map_zero(SCRATCH, 8)
    machine.map_zero(PRIMITIVES, PRIMITIVES_SIZE)
    machine.map_zero(ORDERING_TABLE, 4)
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, ORDERING_TABLE)
    machine.put(ORDERING_TABLE, ordering_word)
    machine.stub(ADD_PRIM, add_prim)


def chain_problems(machine: Machine, ordering_word: int, packets: list[tuple[int, int]]) -> list[str]:
    """Check that `packets`, (address, length) in emission order, were each linked in front of
    the ordering-table entry 0 with their length byte kept."""
    problems = []
    link = ordering_word
    for address, length in packets:
        word = machine.load(address)
        if word >> 24 != length:
            problems.append(f"packet {address:#x} length {word >> 24} not {length}")
        if word & LOW24 != link & LOW24:
            problems.append(f"packet {address:#x} links {word & LOW24:#x} not {link & LOW24:#x}")
        link = address
    expected = ordering_word if not packets else (ordering_word & HIGH8) | (link & LOW24)
    if machine.load(ORDERING_TABLE) != expected:
        problems.append(f"ordering table {machine.load(ORDERING_TABLE):#x} not {expected:#x}")
    return problems


def compare(native: list[int], expected: list[int]) -> list[str]:
    if native == expected:
        return []
    for index, (left, right) in enumerate(zip(native, expected, strict=False)):
        if left != right:
            return [f"output {index}: native {left} original {right} "
                    f"(native {len(native)} values, original {len(expected)})"]
    return [f"native {len(native)} values, original {len(expected)}"]


VRAM_HEIGHT = 512


def run_upload(machine: Machine, count: int, operation: int,
               image_words: int) -> tuple[list[list[int]], list[int]]:
    """Run an upload loop over a buffer at INPUT; record each LoadImage rect and word offset."""
    uploads: list[list[int]] = []

    def load_image(m: Machine) -> None:
        rect, buffer = m.r[4], m.r[5]
        offset = buffer - INPUT
        if offset % 4:
            raise ValueError(f"image buffer {buffer:#x} is misaligned")
        uploads.append([m.load(rect + index * 2, 2, True) for index in range(4)]
                       + [offset // 4, image_words])

    machine.stub(LOAD_IMAGE, load_image)
    machine.r[4] = INPUT
    machine.r[5] = count & 0xFFFFFFFF
    machine.execute(ENTRY_OF[operation])
    return uploads, [operation, count]


def check_upload(count: int, limit: int, uploads: list[list[int]], native: list[int]) -> list[str]:
    if count > limit:
        # The original runs past its grid into rows below VRAM; the native code refuses.
        last = uploads[-1] if uploads else None
        if last is None or last[1] + last[3] <= VRAM_HEIGHT:
            return [f"count {count}: the original's last image {last} stays inside VRAM"]
        return compare(native, [TOO_MANY_IMAGES, 0])
    return compare(native, [UPLOADED, len(uploads), *(value for upload in uploads
                                                      for value in upload)])


def upload_vector(rng: random.Random, number: int, limit: int, edges: tuple[int, ...],
                  small: int) -> list[int]:
    """[count]: images to upload, with the grid's edges and runs past it."""
    if number < len(edges):
        return [edges[number]]
    if number % 16 == 0:
        return [rng.randrange(limit - 4, limit + 40)]
    return [rng.choice((rng.randrange(-3, small), rng.randrange(1, limit + 1)))]


ENTRY_OF = {OP_UPLOAD_SPRITES: 0x80103A24, OP_UPLOAD_CLUTS: 0x80103AD8}
