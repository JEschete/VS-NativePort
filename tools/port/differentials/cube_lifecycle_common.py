"""Shared R3000 vectors and native driver protocol for B50-10 cube-puzzle initialization and screen transitions
(MENUF.PRG 3B8.c:1294-1330 and 2260-2608).

The original side runs each function's own instructions. BATTLE.PRG's TIM parser (func_8008D820), the SLUS memcpy
(func_800490B0) and rsin (func_80040F28), the empty _nop0/_nop1 and, for _initCubePuzzleEnd, _initCubePuzzleStart
are executed too. Every other callee is stubbed and recorded at the call boundary:
- the texture-page reserve and retire (func_8007DFF0, func_8007E0A8);
- the heap and disc queue (vs_main_allocHeapR, vs_main_allocateCdQueueSlot, vs_main_cdEnqueue,
  vs_main_freeCdQueueSlot, vs_main_freeHeapR), which hand out a fixed buffer and request;
- LoadImage, with the RECT, the uploaded span's offset in the file and the file bytes in that span that differ from
  the input at the time of the call;
- the sound calls (vs_main_playSfxDefault, func_80045D64), _pulseCubePuzzleEnd and B50-09's draws.

The file's TIM headers, palettes and puzzle records go into the machine's overlay over a zero-filled buffer. The
minimap scene, the request state, the screen counters, the rating, the puzzle clock, the buttons and the misc score
go to their PS1 addresses. The native driver replays the same state and file bytes through Menu::Results with
recording CubePuzzleLoadHost, CubePuzzleScreenHost and ResultsClutTarget hosts.

Documented differences, which the expected native output encodes:
- Native ParseTim refuses a TIM whose blocks do not fit the file or exceed VRAM, and the initializer refuses an image
  holding fewer than 255 rows (CubePuzzleRefusal::kImage) and a first palette under 160 colors (kPalette). The
  original uploads whatever lies there instead, reading and writing past the TIM.
- The original writes the upload rectangles into the file's TIM headers; native passes them directly. Those header
  bytes are left out of every file comparison.
- A file too short for the 64 puzzle records (kRecords) or a missing minimap scene (kRoom) keeps the default rating
  600/1; the original read past the file or through a null scene pointer.
- _renderCubePuzzleEnd adds _puzzleModeRankPoints[15 - cap] unchecked when the time is within par; native finishes
  without points for a rank outside the 16-entry table.
"""

from __future__ import annotations

import random

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MASK,
    MODULE_BASES,
    Fault,
    Machine,
    s32,
    store_problems,
)
from differentials.results_drawing_common import (
    call_difference,
    edit_mutants,
    verify_edits,
)

MODULE = "MENU/MENUF.PRG"
MODULE_BASES[MODULE] = 0x80102800
BATTLE = "BATTLE/BATTLE.PRG"
SLUS = "SLUS_010.40"

# Module state.
SUBMENU_STATE = 0x800F1CD8  # vs_battle_submenuState, the load step
SECTION_F = 0x800F1C94  # the minimap scene; its first room's zone and map at +0xC and +0xE
DIS_FILES = 0x80102AE0  # _disFiles, 8-byte entries ending in the file size
IQ_SLOT, IQ_DATA, ESC_SLOT, ESC_DATA = 0x80109868, 0x8010986C, 0x80109870, 0x80109874
BUFF_REEL = 0x80109878
ANIMATION = 0x8010988C
MAP_COMPLETION, SCORE, TIMER, SCREEN_STATE = 0x80109894, 0x80109898, 0x8010989C, 0x801098A0
SCREEN_WORDS = (MAP_COMPLETION, SCORE, TIMER, SCREEN_STATE, ANIMATION, BUFF_REEL)
CLUT = 0x801098AC  # _clutBuf
CLUT_WORDS = 160
PAR, CAP = 0x801099F4, 0x801099F6  # _puzzleParTime, _puzzleRankCap
FLAGS = 0x80061638  # vs_main_stateFlags puzzle hundredths, seconds, minutes, puzzleSetMaxTimer
MISC_SCORE = 0x8005FFA8  # vs_main_scoredata.miscScore
BUTTONS = 0x8005E1D0  # vs_main_buttonsPressed
POINTS = 0x8010978C  # _puzzleModeRankPoints

# Where the vector's file buffer, request and scene live (zero-filled outside the placed bytes).
ASSET = 0x80200000
ASSET_SPAN = 0x180000
SLOT = 0x80390000
OTHER_SLOT, OTHER_DATA = 0x80391000, 0x80392000
SCENE = 0x803A0000

# Callees.
RESERVE, RETIRE = 0x8007DFF0, 0x8007E0A8
ALLOC, ALLOC_SLOT, ENQUEUE, FREE_SLOT, FREE_HEAP = 0x80043E3C, 0x80044B10, 0x80044BC4, 0x80044B80, 0x80043C60
LOAD_IMAGE = 0x800288FC
PARSE_TIM, MEMCPY, RSIN = 0x8008D820, 0x800490B0, 0x80040F28
NOP0, NOP1 = 0x80108E38, 0x80108E40
PLAY, STOP_SFX = 0x80045988, 0x80045D64
PULSE = 0x80104A50
INIT_START = 0x80107A8C
# B50-09's draws, by CubePuzzleMessage.
MESSAGES = {0x801085D4: 0, 0x8010873C: 1, 0x80108784: 2, 0x8010887C: 3, 0x80108688: 4}
GIVING_UP, CHICKEN = 0x801084F4, 0x80108564
RANK, PROMPT = 0x8010880C, 0x801064D4
# LoadImage return addresses: the image and the first palette of _initCubePuzzleStart and _initCubePuzzleQuit.
IMAGE_SITES = (0x80107BB4, 0x80107EA4)
PALETTE_SITES = (0x80107C18, 0x80107F08)

# Recorded rows, as the driver prints them.
(RESERVE_ROW, READ_ROW, UPLOAD_ROW, RELEASE_ROW, FREE_ROW, PLAY_ROW, STOP_ROW, RETIRE_ROW, PULSE_ROW, MESSAGE_ROW,
 RANK_ROW, PROMPT_ROW, REFUSE_ROW, CLUT_ROW) = range(14)
REFUSE_IMAGE, REFUSE_PALETTE, REFUSE_RECORDS, REFUSE_ROOM = range(4)

TIM_STRIDE = 0x8220
RECORDS = 0x10440
RECORDS_END = RECORDS + 64 * 8
IQ_FILE_SIZE, ESC_FILE_SIZE = 0x10800, 0x8800  # the _disFiles sizes the buffers are allocated with
VRAM_BYTES = 2 * 1024 * 512
DEFAULT_RATING = (600, 1)

# operation, entry, end (one past the return delay slot), name.
SPECS = {
    "F03024": (0, 0x80104A50, 0x80104B8C, "_pulseCubePuzzleEnd (3B8.c:1294)"),
    "F03046": (1, 0x80107A8C, 0x80107D4C, "_initCubePuzzleStart (3B8.c:2260)"),
    "F03047": (2, 0x80107D4C, 0x80107D98, "_initCubePuzzleEnd (3B8.c:2351)"),
    "F03048": (3, 0x80107D98, 0x80107FC0, "_initCubePuzzleQuit (3B8.c:2363)"),
    "F03049": (4, 0x80107FC0, 0x801080E4, "_renderCubePuzzleStart (3B8.c:2432)"),
    "F03050": (5, 0x801080E4, 0x801083AC, "_renderCubePuzzleEnd (3B8.c:2477)"),
    "F03051": (6, 0x801083AC, 0x801084F4, "_renderCubePuzzleQuit (3B8.c:2566)"),
}
PULSE_OP, START_OP, RESULTS_OP, QUIT_OP, START_UPDATE, RESULTS_UPDATE, QUIT_UPDATE = range(7)
INITIALIZERS = (START_OP, RESULTS_OP, QUIT_OP)
RECORD_OPS = (START_OP, RESULTS_OP)

# name: (address, original word, mutated word). The original words are checked against MENUF.PRG in describe();
# MUTANTS stays static so case discovery and --list never read a disc image.
EDITS = {
    "F03024": {
        "F03024-weights-of-17": (0x80104A64, 0x24020010, 0x24020011),
        "F03024-complement-row-shifted": (0x80104A6C, 0x240B0080, 0x240B0082),
        "F03024-weighted-row-shifted": (0x80104A70, 0x240A00C0, 0x240A00C2),
        "F03024-target-row-shifted": (0x80104A74, 0x24090100, 0x24090102),
        "F03024-floors-negative-red": (0x80104AB4, 0x24A5000F, 0x24A50000),
        "F03024-mask-bit-14": (0x80104B28, 0x30638000, 0x30634000),
        "F03024-31-colors": (0x80104B40, 0x29C20020, 0x29C2001F),
        "F03024-upload-row-510": (0x80104B54, 0x240201FF, 0x240201FE),
        "F03024-upload-159-wide": (0x80104B5C, 0x240200A0, 0x2402009F),
    },
    "F03046": {
        "F03046-reserves-3-pages": (0x80107AB4, 0x24050002, 0x24050003),
        "F03046-allocates-esc-size": (0x80107AC8, 0x8E04001C, 0x8E040024),
        "F03046-loaded-state-3": (0x80107B44, 0x24020004, 0x24020003),
        "F03046-image-x-833": (0x80107B58, 0x24120340, 0x24120341),
        "F03046-image-y-257": (0x80107B90, 0x24020100, 0x24020101),
        "F03046-image-254-rows": (0x80107B9C, 0x240200FF, 0x240200FE),
        "F03046-palette-x-769": (0x80107BC8, 0x24020300, 0x24020301),
        "F03046-palette-row-510": (0x80107BDC, 0x240201FF, 0x240201FE),
        "F03046-palette-159-wide": (0x80107BE8, 0x240200A0, 0x2402009F),
        "F03046-keeps-first-colors": (0x80107C04, 0xAC400000, 0x00000000),
        "F03046-copies-158-colors": (0x80107C28, 0x24060140, 0x2406013C),
        "F03046-one-image": (0x80107C38, 0x2A020002, 0x2A020001),
        "F03046-records-one-later": (0x80107C48, 0x34840440, 0x34840448),
        "F03046-default-par-601": (0x80107C54, 0x24020258, 0x24020259),
        "F03046-default-cap-0": (0x80107C60, 0x24020001, 0x24020000),
        "F03046-63-records": (0x80107CAC, 0x2A020040, 0x2A02003F),
        "F03046-record-stride-10": (0x80107CB4, 0x24840008, 0x2484000A),
        "F03046-keeps-buff-reel": (0x80107D18, 0xAC409878, 0x00000000),
    },
    "F03047": {
        "F03047-maximum-58-minutes": (0x80107D68, 0x2402003B, 0x2402003A),
        "F03047-maximum-98-hundredths": (0x80107D74, 0x24020063, 0x24020062),
        "F03047-keeps-maximum-flag": (0x80107D78, 0xA06000A3, 0x00000000),
    },
    "F03048": {
        "F03048-reserves-2-pages": (0x80107DC0, 0x24050001, 0x24050002),
        "F03048-allocates-iq-size": (0x80107DD4, 0x8E040024, 0x8E04001C),
        "F03048-image-x-833": (0x80107E48, 0x24120340, 0x24120341),
        "F03048-palette-x-769": (0x80107EB8, 0x24020300, 0x24020301),
        "F03048-copies-158-colors": (0x80107F18, 0x24060140, 0x2406013C),
        "F03048-two-images": (0x80107F28, 0x1A00FFC9, 0x1E00FFC9),  # blez -> bgtz
        "F03048-keeps-buff-reel": (0x80107F94, 0xAC609878, 0x00000000),
    },
    "F03049": {
        "F03049-reveal-sfx-7c": (0x80107FF0, 0x2405007D, 0x2405007C),
        "F03049-second-sfx-at-31": (0x80107FF8, 0x2402001E, 0x2402001F),
        "F03049-average-delay-16": (0x80108030, 0x24C6FFF1, 0x24C6FFF0),
        "F03049-par-delay-29": (0x80108044, 0x24C6FFE2, 0x24C6FFE3),
        "F03049-timer-limit-7ffe": (0x80108064, 0x28627FFF, 0x28627FFE),
        "F03049-confirm-on-bit-6": (0x80108080, 0x30420020, 0x30420040),
        "F03049-ends-at-79": (0x80108094, 0x2842004E, 0x2842004F),
        "F03049-retires-3-pages": (0x801080AC, 0x24050002, 0x24050003),
        "F03049-skips-to-79": (0x801080C8, 0x2402004E, 0x2402004F),
    },
    "F03050": {
        "F03050-pulse-floors-negative": (0x80108108, 0x244201FF, 0x24420000),
        "F03050-animation-wraps-at-2048": (0x80108128, 0x30420FFF, 0x304207FF),
        "F03050-par-rank-16": (0x801081B0, 0x2402000F, 0x24020010),
        "F03050-steps-per-second": (0x801081D0, 0x00081983, 0x00081943),
        "F03050-late-rank-from-14": (0x801081E8, 0x2403000F, 0x2403000E),
        "F03050-counting-sfx-at-17": (0x80108214, 0x24020010, 0x24020011),
        "F03050-counting-sfx-71": (0x80108228, 0x24050072, 0x24050071),
        "F03050-counted-sfx-74": (0x8010824C, 0x24050073, 0x24050074),
        "F03050-first-reveal-at-68": (0x80108254, 0x24020043, 0x24020044),
        "F03050-food-chain-delay-76": (0x801082C0, 0x24C6FFB5, 0x24C6FFB4),
        "F03050-ends-at-139": (0x80108328, 0x2842008A, 0x2842008B),
        "F03050-retires-3-pages": (0x80108340, 0x24050002, 0x24050003),
        "F03050-points-one-later": (0x80108358, 0x2442978C, 0x24429790),
    },
    "F03051": {
        "F03051-quit-sfx-7b": (0x80108414, 0x2405007A, 0x2405007B),
        "F03051-second-sfx-at-16": (0x8010841C, 0x2402000F, 0x24020010),
        "F03051-chicken-delay-16": (0x80108448, 0x24A5FFF1, 0x24A5FFF0),
        "F03051-ends-at-64": (0x80108498, 0x2842003F, 0x28420040),
        "F03051-skips-to-62": (0x801084A0, 0x2402003F, 0x2402003E),
        "F03051-retires-2-pages": (0x801084B4, 0x24050001, 0x24050002),
    },
}


# ---------------------------------------------------------------------------------------------- vectors
def u16_pack(*values: int) -> bytes:
    return b"".join((value & 0xFFFF).to_bytes(2, "little") for value in values)


def u32_pack(*values: int) -> bytes:
    return b"".join((value & MASK).to_bytes(4, "little") for value in values)


def colors(rng: random.Random, count: int) -> bytes:
    return u16_pack(*(rng.choice((0, 0x8000, 0x7FFF)) if rng.random() < 0.05 else rng.getrandbits(16)
                      for _ in range(count)))


def tim_runs(rng: random.Random, base: int, first: bool) -> tuple[str, list[tuple[int, bytes]]]:
    """One TIM at `base`: its kind and the (offset, bytes) runs of its header, palette and image header."""
    kinds = (("real", 40), ("variant", 18), ("noclut", 7), ("short-rows", 9), ("bad-clut", 10), ("wide", 6),
             ("zero-width", 3))
    if first:
        kinds += (("short-palette", 9),)
    kind = rng.choices([name for name, _ in kinds], [weight for _, weight in kinds])[0]
    if kind == "noclut":
        mode = rng.choice((0, 1, 2, 3, 0x10, 0xF7))
    else:
        mode = 8 | rng.choice((0, 0, 1, 2, 3, 0x30, 0xFF0))
    clut_size, image_size = (16, 16), (64, 256)
    clut_length = None
    if kind == "variant":
        clut_size = rng.choice(((160, 1), (256, 1), (16, 10), (32, 5), (16, 16), (80, 2), (20, 8), (15, 11)))
        image_size = (rng.choice((64, 64, 32, 48, 63, 1, 16) + ((128, 96) if first else ())),
                      rng.choice((255, 256, 255, 300, 512)))
        clut_length = 12 + 2 * clut_size[0] * clut_size[1] + rng.choice((0, 0, 1, 2, 3, 4, 8))
    elif kind == "short-palette":
        clut_size = rng.choice(((16, 8), (159, 1), (10, 10), (1, 1), (0, 0), (16, 9)))
    elif kind == "short-rows":
        image_size = (rng.choice((64, 1, 32, 64)), rng.choice((254, 128, 1, 0, 200, 254)))
    elif kind == "bad-clut":
        roll = rng.random()
        if roll < 0.4:
            clut_length = rng.choice((0, 4, 8, 11))
        elif roll < 0.7:
            clut_length = rng.randrange(12 + VRAM_BYTES + 1, 0x170000)
        else:
            clut_size = rng.choice(((16, 17), (161, 1), (16, 32)))
            clut_length = 12 + 2 * 16 * 16
    elif kind == "wide":
        image_size = rng.choice(((1025, 255), (0x8000, 256), (0xFFFF, 255), (64, 513), (64, 0x8000), (1, 0xFFFF)))
    elif kind == "zero-width":
        image_size = (0, rng.choice((0, 255, 256)))
    if clut_length is None:
        clut_length = 12 + 2 * clut_size[0] * clut_size[1]
    runs = [(base, u32_pack(0x10, mode))]
    image_offset = base + 8
    if mode & 8:
        x, y = rng.choice(((256, 496), (0, 496), (0, 480))) if rng.random() < 0.7 else (rng.getrandbits(16),
                                                                                         rng.getrandbits(16))
        runs.append((base + 8, u32_pack(clut_length) + u16_pack(x, y, *clut_size)))
        runs.append((base + 20, colors(rng, min(clut_size[0] * clut_size[1], 256))))
        image_offset = base + 8 + (clut_length & ~3)
    x, y = rng.choice(((512, 256), (768, 256), (768, 0))) if rng.random() < 0.7 else (rng.getrandbits(16),
                                                                                       rng.getrandbits(16))
    runs.append((image_offset, u32_pack(12 + 2 * image_size[0] * image_size[1]) + u16_pack(x, y, *image_size)))
    if rng.random() < 0.3:
        # A few image bytes; nothing reads them, but they sit inside the uploaded span.
        runs.append((image_offset + 12, bytes(rng.getrandbits(8) for _ in range(16))))
    return kind, runs


ZONES = (0, 1, 2, 3, 9, 10, 11, 14, 24, 0x7FFF, -1, -0x8000)
MAPS = (0, 1, 2, 5, 9, 12, 20, 0x7FFF, -1)


def record_values(rng: random.Random) -> tuple[int, int]:
    par = rng.randrange(0, 1000) if rng.random() < 0.6 else rng.choice((600, 300, 120, 0, -1, 0x7FFF, -0x8000))
    cap = rng.choice((0, 0, 0, 1, 2, 3, 15, 16, -1, 0x7FFF, -0x8000))
    return par, cap


def record_run(rng: random.Random, room: tuple[int, int] | None) -> tuple[int, bytes]:
    filled = rng.choice((18, 64, 64, 30, 0, 1))
    records = []
    for index in range(64):
        if index < filled:
            records.append([rng.choice(ZONES), rng.choice(MAPS), *record_values(rng)])
        else:
            records.append([0, 0, 0, 0])
    if room is not None and rng.random() < 0.6:
        index = rng.choice((0, 63, rng.randrange(64), rng.randrange(64), max(filled - 1, 0)))
        records[index] = [*room, *record_values(rng)]
        if rng.random() < 0.3 and index < 63:
            later = rng.randrange(index + 1, 64)
            records[later] = [*room, *record_values(rng)]
    return RECORDS, b"".join(u16_pack(*record) for record in records)


def asset_size(rng: random.Random, operation: int) -> int:
    roll = rng.random()
    if operation == QUIT_OP:
        if roll < 0.6:
            return ESC_FILE_SIZE
        if roll < 0.8:
            return TIM_STRIDE
        if roll < 0.85:
            return rng.randrange(TIM_STRIDE, IQ_FILE_SIZE)
        if roll < 0.95:
            return rng.randrange(0x200, TIM_STRIDE)
        return rng.randrange(0, 0x40)
    if roll < 0.5:
        return IQ_FILE_SIZE
    if roll < 0.68:
        return RECORDS_END
    if roll < 0.73:
        return RECORDS_END + rng.randrange(1, 0x1C0)
    if roll < 0.82:
        return rng.randrange(RECORDS, RECORDS_END)
    if roll < 0.87:
        return rng.randrange(TIM_STRIDE + 1, RECORDS)
    if roll < 0.9:
        return TIM_STRIDE
    if roll < 0.96:
        return rng.randrange(0x200, TIM_STRIDE)
    return rng.randrange(0, 0x40)


def clip(runs: list[tuple[int, bytes]], size: int) -> list[tuple[int, bytes]]:
    return [(offset, data[:size - offset]) for offset, data in runs if offset < size]


EDGE_STEPS = (3, -1, 0x7FFFFFFF, -0x80000000, 2)
OTHER_SLOT_STATES = (0, 1, 2, 3, 5, -1, 0x7FFF, -0x8000)


def initializer_vector(rng: random.Random, operation: int, number: int, value: dict) -> None:
    pattern = number % 8
    if pattern < 2:
        value["step"] = 0
    elif pattern < 6:
        value["step"] = 1
    elif pattern == 6:
        value["step"] = 2
    else:
        value["step"] = rng.choice(EDGE_STEPS) if rng.random() < 0.7 else s32(rng.getrandbits(32))
    value["slot_state"] = 4 if rng.random() < 0.85 else rng.choice(OTHER_SLOT_STATES)
    value["room"] = None if rng.random() < 0.1 else (
        (0, 0) if rng.random() < 0.1 else (rng.choice(ZONES), rng.choice(MAPS)))
    size = asset_size(rng, operation)
    runs, kinds = [], []
    for index in range(2 if operation in RECORD_OPS else 1):
        kind, tim = tim_runs(rng, index * TIM_STRIDE, index == 0)
        kinds.append(kind)
        runs += tim
    if operation in RECORD_OPS:
        runs.append(record_run(rng, value["room"]))
    value["size"] = size
    value["runs"] = clip(runs, size)
    value["kinds"] = kinds


def timer_edges(operation: int) -> tuple[int, ...]:
    common = (0, 1, -1, 0x7FFE, 0x7FFF, 0x8000, 0x7FFFFFFF, -0x80000000)
    return common + {
        START_UPDATE: (14, 15, 16, 29, 30, 31, 0x4C, 0x4D, 0x4E, 0x4F),
        RESULTS_UPDATE: (15, 16, 17, 63, 64, 65, 66, 67, 68, 74, 75, 76, 89, 90, 91, 0x88, 0x89, 0x8A, 0x8B),
        QUIT_UPDATE: (14, 15, 16, 0x3D, 0x3E, 0x3F, 0x40),
    }[operation]


END_TIMERS = {START_UPDATE: 0x4E, RESULTS_UPDATE: 0x8A, QUIT_UPDATE: 0x3F}
ANIMATION_EDGES = (0, 0x400, 0x800, 0xC00, 0xFFF, 0x1000, 0xFFFFFFFF, 0x80000000, 0x7FFFFFFF, 0xFFFFF000)


def rank_inputs(rng: random.Random, value: dict) -> None:
    par = rng.randrange(0, 1000) if rng.random() < 0.7 else rng.choice((600, 0, -1, -100, 0x7FFF, -0x8000))
    cap = rng.choice((0, 0, 1, 1, 2, 3)) if rng.random() < 0.8 else rng.choice(
        (15, 16, 20, -1, -100, 0x7FFF, -0x8000, s32(rng.getrandbits(16) << 16) >> 16))
    total = None
    if rng.random() < 0.6:
        delta = rng.choice((-1, 0, 1, 199, 200, 201, 399, 400, 401, 2999, 3000, 3001, -6000)) if rng.random() < 0.5 \
            else rng.randrange(-200, 3400)
        total = par * 100 + delta
    if total is not None and 0 <= total < 255 * 6000:
        minutes, rest = divmod(total, 6000)
        seconds, hundredths = divmod(rest, 100)
    elif rng.random() < 0.8:
        minutes, seconds, hundredths = rng.randrange(60), rng.randrange(60), rng.randrange(100)
    else:
        minutes, seconds, hundredths = rng.getrandbits(8), rng.getrandbits(8), rng.getrandbits(8)
    value["par"], value["cap"] = par, cap
    value["flags"] = [hundredths, seconds, minutes, value["flags"][3]]


def update_vector(rng: random.Random, operation: int, number: int, value: dict) -> None:
    edges = timer_edges(operation)
    if number % 2 == 0:
        timer = edges[number // 2 % len(edges)]
    elif number % 16 == 3:
        timer = s32(rng.getrandbits(32))
    else:
        timer = rng.randrange(-5, END_TIMERS[operation] + 30)
    screen = value["screen"]
    screen[2] = timer
    screen[3] = 0 if rng.random() < 0.85 else rng.choice((1, -1, 2, s32(rng.getrandbits(32))))
    screen[4] = rng.choice(ANIMATION_EDGES) if number % 5 == 0 else (
        rng.getrandbits(32) if number % 23 == 7 else rng.randrange(0x1000))
    value["buttons"] = (rng.getrandbits(32) & ~0x20) | (0x20 if rng.random() < 0.5 else 0)
    if operation == RESULTS_UPDATE:
        rank_inputs(rng, value)
        if number % 32 == 1:
            # Circle at the end with the time within par and a cap outside 0-15: the original's unchecked
            # _puzzleModeRankPoints read.
            screen[2] = rng.choice((0x89, 0x8A, 0x8B, 0x100, 0x7FFF))
            screen[3] = 0
            value["buttons"] |= 0x20
            value["par"] = rng.randrange(1, 1000)
            value["cap"] = rng.choice((16, 17, 20, 0x7FFF, -1, -2, -100, -0x8000))
            minutes, rest = divmod(value["par"] * 100 - rng.randrange(0, 100), 6000)
            value["flags"] = [rest % 100, rest // 100, minutes, value["flags"][3]]


EDGE_WEIGHTS = (-0x80000000, -1, 0, 1, 7, 8, 9, 15, 16, 17, 0x7FFFFFFF)


def vector_for(identifier: str, rng: random.Random, number: int) -> dict:
    operation = SPECS[identifier][0]
    value = {
        "op": operation,
        "step": 0,
        "slot_state": 4,
        # map completion, score, timer, screen state, animation, buff reel selection.
        "screen": [s32(rng.getrandbits(32)) if rng.random() < 0.7 else 0 for _ in range(4)]
        + [rng.getrandbits(32) if rng.random() < 0.7 else 0 for _ in range(2)],
        "par": s32(rng.getrandbits(16) << 16) >> 16,
        "cap": s32(rng.getrandbits(16) << 16) >> 16,
        # hundredths, seconds, minutes, puzzleSetMaxTimer.
        "flags": [rng.getrandbits(8) for _ in range(3)] + [rng.choice((0, 0, 1, 0xFF, rng.getrandbits(8)))],
        "buttons": rng.getrandbits(32),
        "misc": rng.getrandbits(32),
        "weight": 0,
        "room": None,
        "clut": [rng.getrandbits(16) for _ in range(CLUT_WORDS)],
        "size": 0,
        "runs": [],
        "kinds": [],
    }
    if operation == PULSE_OP:
        value["weight"] = (EDGE_WEIGHTS[number // 4 % len(EDGE_WEIGHTS)] if number % 4 == 0 else
                           s32(rng.getrandbits(32)) if number % 13 == 5 else rng.randrange(17))
        value["clut"] = [rng.choice((0, 0x7FFF, 0x8000, 0xFFFF, 0x001F, 0x03E0, 0x7C00)) if rng.random() < 0.1 else
                         rng.getrandbits(16) for _ in range(CLUT_WORDS)]
    elif operation in INITIALIZERS:
        initializer_vector(rng, operation, number, value)
    else:
        update_vector(rng, operation, number, value)
    return value


def driver_inputs(value: dict) -> list[int]:
    room = value["room"]
    inputs = [value["op"], value["step"], value["slot_state"], *value["screen"], value["par"], value["cap"],
              *value["flags"], value["buttons"], value["misc"], value["weight"], int(room is not None),
              *(room or (0, 0)), *value["clut"], value["size"], len(value["runs"])]
    for offset, data in value["runs"]:
        inputs += [offset, len(data), *data]
    return inputs


# ---------------------------------------------------------------------------------------------- original
def read_state(machine: Machine) -> list[int]:
    """Load step, the six screen words, par, cap, the four clock bytes and the misc score, as the driver prints."""
    words = [s32(machine.load(address)) for address in SCREEN_WORDS[:4]]
    words += [machine.load(address) for address in SCREEN_WORDS[4:]]
    return [s32(machine.load(SUBMENU_STATE)), *words, machine.load(PAR, 2, signed=True),
            machine.load(CAP, 2, signed=True), *machine.bytes(FLAGS, 4), machine.load(MISC_SCORE)]


def stored_asset_bytes(machine: Machine, low: int, high: int) -> dict[int, int]:
    """The file bytes the original has stored to within [low, high), by offset, with their current values."""
    found = {}
    for address, size in machine.stores:
        for byte in range(address, address + size):
            if low <= byte < high and ASSET <= byte < ASSET + ASSET_SPAN:
                found[byte - ASSET] = machine.byte(byte)
    return found


def run_original(machine: Machine, value: dict) -> tuple[dict, list[int]]:
    operation = value["op"]
    entry = next(spec[1] for spec in SPECS.values() if spec[0] == operation)
    inputs = driver_inputs(value)
    m = machine
    m.put(SUBMENU_STATE, value["step"])
    for address, word in zip(SCREEN_WORDS, value["screen"], strict=True):
        m.put(address, word)
    m.put(PAR, value["par"], 2)
    m.put(CAP, value["cap"], 2)
    for index, byte in enumerate(value["flags"]):
        m.put(FLAGS + index, byte, 1)
    m.put(BUTTONS, value["buttons"])
    m.put(MISC_SCORE, value["misc"])
    for index, word in enumerate(value["clut"]):
        m.put(CLUT + 2 * index, word, 2)
    # The request and buffer statics as step 0 leaves them; step 0 overwrites them.
    iq = operation in RECORD_OPS
    m.put(IQ_SLOT, SLOT if iq else OTHER_SLOT)
    m.put(IQ_DATA, ASSET if iq else OTHER_DATA)
    m.put(ESC_SLOT, OTHER_SLOT if iq else SLOT)
    m.put(ESC_DATA, OTHER_DATA if iq else ASSET)
    m.map_zero(ASSET, ASSET_SPAN)
    m.map_zero(SLOT, 0x3000)
    m.put(SLOT, value["slot_state"], 2)
    m.map_zero(SCENE, 0x20)
    if value["room"] is None:
        # The original reads the room through the null scene pointer (kernel RAM on the PS1).
        m.put(SECTION_F, 0)
        m.map_zero(0, 0x10)
    else:
        m.put(SECTION_F, SCENE)
        m.put(SCENE + 0xC, value["room"][0], 2)
        m.put(SCENE + 0xE, value["room"][1], 2)
    for offset, data in value["runs"]:
        m.store(ASSET + offset, data)
    if operation == RESULTS_UPDATE:
        # 15 - cap may index _puzzleModeRankPoints anywhere within +-128 KiB.
        m.map_zero(0x800E0000, 0x80102800 - 0x800E0000)
        m.map_zero(0x801099F8, 0x80130000 - 0x801099F8)
    m.follow |= {PARSE_TIM, MEMCPY, RSIN, NOP0, NOP1, INIT_START}
    rows: list[list] = []
    tags: list[str] = []
    rects: list[int] = []
    read = {}

    def add(tag: str, row: list) -> None:
        rows.append(row)
        tags.append(tag)

    def alloc(mm: Machine) -> None:
        read["size"] = mm.r[4]
        mm.r[2] = ASSET

    def alloc_slot(mm: Machine) -> None:
        read["file"] = mm.r[4]
        mm.r[2] = SLOT

    def enqueue(mm: Machine) -> None:
        file = read.get("file", 0)
        index = (file - DIS_FILES) // 8 if (file - DIS_FILES) % 8 == 0 else -1
        size_ok = "size" in read and read["size"] == mm.load(file + 4)
        statics = {3: (IQ_SLOT, IQ_DATA), 4: (ESC_SLOT, ESC_DATA)}.get(index)
        statics_ok = statics is not None and mm.load(statics[0]) == SLOT and mm.load(statics[1]) == ASSET
        add("read", [READ_ROW, index, int(size_ok), int(mm.r[4] == SLOT and mm.r[5] == ASSET), int(statics_ok)])

    def load_image(mm: Machine) -> None:
        area = [mm.load(mm.r[4] + 2 * k, 2, signed=True) for k in range(4)]
        source = mm.r[5]
        if operation == PULSE_OP:
            add("clut", [CLUT_ROW, *area, CLUT_WORDS, int(source == CLUT)])
            return
        rects.append(mm.r[4] - ASSET)
        count = area[2] * area[3] * 2 if area[2] > 0 and area[3] > 0 else 0
        span = stored_asset_bytes(mm, source, source + count)
        tag = "image" if mm.r[31] in IMAGE_SITES else "palette" if mm.r[31] in PALETTE_SITES else "upload"
        add(tag, [UPLOAD_ROW, *area, s32(source - ASSET), count, span])

    def record(kind: int, registers: int, prefix: tuple = ()):
        def handler(mm: Machine) -> None:
            add("call", [kind, *prefix, *(s32(mm.r[4 + k]) for k in range(registers))])
        return handler

    m.stub(RESERVE, record(RESERVE_ROW, 3))
    m.stub(RETIRE, record(RETIRE_ROW, 3))
    m.stub(ALLOC, alloc)
    m.stub(ALLOC_SLOT, alloc_slot)
    m.stub(ENQUEUE, enqueue)
    m.stub(FREE_SLOT, lambda mm: add("release", [RELEASE_ROW, int(mm.r[4] == SLOT)]))
    m.stub(FREE_HEAP, lambda mm: add("free", [FREE_ROW, int(mm.r[4] == ASSET)]))
    m.stub(LOAD_IMAGE, load_image)
    m.stub(PLAY, record(PLAY_ROW, 2))
    m.stub(STOP_SFX, record(STOP_ROW, 2))
    m.stub(PULSE, record(PULSE_ROW, 1))
    for address, message in MESSAGES.items():
        m.stub(address, record(MESSAGE_ROW, 3, (message,)))
    m.stub(GIVING_UP, record(MESSAGE_ROW, 2, (5, 0)))
    m.stub(CHICKEN, record(MESSAGE_ROW, 2, (6, 0)))
    m.stub(RANK, record(RANK_ROW, 4))
    m.stub(PROMPT, record(PROMPT_ROW, 4))
    if operation == PULSE_OP:
        m.r[4] = value["weight"] & MASK
    try:
        m.execute(entry)
    except Fault as fault:
        return {"fault": str(fault)}, inputs
    return {
        "fault": None,
        "status": s32(m.r[2]),
        "state": read_state(m),
        "clut": [m.load(CLUT + 2 * k, 2) for k in range(CLUT_WORDS)],
        "rows": rows,
        "tags": tags,
        "rects": rects,
        "dirty": stored_asset_bytes(m, ASSET, ASSET + value["size"]),
        "stores": list(m.stores),
    }, inputs


# ---------------------------------------------------------------------------------------------- expectations
def pristine_bytes(value: dict) -> dict[int, int]:
    found = {}
    for offset, data in value["runs"]:
        for index, byte in enumerate(data):
            found[offset + index] = byte
    return found


def native_tim(pristine: dict[int, int], size: int, index: int):
    """Render::ParseTim over the file from TIM `index`: ((clut width, height, pixel bytes) or None, (image width,
    height, pixel bytes)), or None when native refuses to parse it."""
    base = index * TIM_STRIDE
    if base >= size:
        return None
    available = size - base

    def u16(offset: int) -> int:
        return pristine.get(base + offset, 0) | pristine.get(base + offset + 1, 0) << 8

    def u32(offset: int) -> int:
        return u16(offset) | u16(offset + 2) << 16

    def block(offset: int, limit: int):
        if limit > available or offset > limit or limit - offset < 12:
            return None
        width, height = u16(offset + 8), u16(offset + 10)
        if width > 1024 or height > 512:
            return None
        pixel_bytes = width * height * 2
        return (width, height, pixel_bytes) if limit - (offset + 12) >= pixel_bytes else None

    if available < 8:
        return None
    mode = u32(4)
    clut = None
    image_offset = 8
    if mode & 8:
        if available - 8 < 4:
            return None
        length = u32(8)
        if length < 12 or length > 12 + VRAM_BYTES:
            return None
        image_offset = 8 + (length & ~3)
        clut = block(8, min(available, 8 + length))
        if clut is None:
            return None
    image = block(image_offset, available)
    return None if image is None else (clut, image)


def image_refused(tim) -> bool:
    return tim is None or tim[1][0] * 2 * 255 > tim[1][2]


def palette_outcome(tim) -> str:
    """What native does with the first TIM's palette: 'dropped', 'refused' or 'uploaded'."""
    if tim is None or tim[0] is None:
        return "dropped"
    return "refused" if tim[0][2] < 2 * CLUT_WORDS else "uploaded"


def expected_rows(value: dict, observation: dict) -> tuple[list[list], str | None, int | None]:
    """The rows native must print (from the original's), the palette outcome and the original's cleared palette."""
    if value["op"] not in INITIALIZERS:
        return observation["rows"], None, None
    pristine = pristine_bytes(value)
    size = value["size"]
    expected: list[list] = []
    images = 0
    palette, cleared = None, None
    for row, tag in zip(observation["rows"], observation["tags"], strict=True):
        if tag == "image":
            expected.append([REFUSE_ROW, REFUSE_IMAGE] if image_refused(native_tim(pristine, size, images)) else row)
            images += 1
        elif tag == "palette":
            palette = palette_outcome(native_tim(pristine, size, 0))
            cleared = row[5]
            if palette == "refused":
                expected.append([REFUSE_ROW, REFUSE_PALETTE])
            elif palette == "uploaded":
                expected.append(row)
        elif tag == "release" and value["op"] in RECORD_OPS:
            if size < RECORDS_END:
                expected.append([REFUSE_ROW, REFUSE_RECORDS])
            elif value["room"] is None:
                expected.append([REFUSE_ROW, REFUSE_ROOM])
            expected.append(row)
        else:
            expected.append(row)
    return expected, palette, cleared


def normalize(rows: list[list], excluded: set[int], pristine: dict[int, int]) -> list[list[int]]:
    """Upload rows with their changed file bytes as sorted (offset, value) pairs outside the header writes."""
    normalized = []
    for row in rows:
        if row[0] != UPLOAD_ROW:
            normalized.append(list(row))
            continue
        changed = sorted((offset, byte) for offset, byte in row[7].items()
                         if offset not in excluded and byte != pristine.get(offset, 0))
        normalized.append(row[:7] + [len(changed)] + [item for pair in changed for item in pair])
    return normalized


STATE_NAMES = ("load step", "map completion", "score", "timer", "screen state", "animation", "buff reel", "par",
               "cap", "hundredths", "seconds", "minutes", "maximum flag", "misc score")
STATE_WORDS = len(STATE_NAMES)


def parse_native(native: list[int]) -> dict | None:
    head = 1 + STATE_WORDS + CLUT_WORDS
    if len(native) < head + 1:
        return None
    rows, at = [], head + 1
    for _ in range(native[head]):
        if at >= len(native) or at + 1 + native[at] > len(native):
            return None
        rows.append(native[at + 1:at + 1 + native[at]])
        at += 1 + native[at]
    if at >= len(native) or len(native) != at + 1 + 2 * native[at]:
        return None
    pairs = native[at + 1:]
    parsed_rows = []
    for row in rows:
        if row and row[0] == UPLOAD_ROW:
            if len(row) < 8 or len(row) != 8 + 2 * row[7]:
                return None
            row = row[:7] + [dict(zip(row[8::2], row[9::2], strict=True))]
        parsed_rows.append(row)
    return {
        "status": native[0],
        "state": native[1:1 + STATE_WORDS],
        "clut": native[1 + STATE_WORDS:head],
        "rows": parsed_rows,
        "dirty": dict(zip(pairs[::2], pairs[1::2], strict=True)),
    }


def allowed_stores(operation: int) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if operation == PULSE_OP:
        return allowed + [(CLUT + 2 * 128, CLUT + 2 * CLUT_WORDS)]
    if operation in INITIALIZERS:
        allowed += [(SUBMENU_STATE, SUBMENU_STATE + 4), (ANIMATION, ANIMATION + 4), (BUFF_REEL, BUFF_REEL + 4),
                    (MAP_COMPLETION, SCREEN_STATE + 4), (CLUT, CLUT + 2 * CLUT_WORDS), (ASSET, ASSET + ASSET_SPAN)]
        allowed.append((ESC_SLOT, ESC_DATA + 4) if operation == QUIT_OP else (IQ_SLOT, IQ_DATA + 4))
        if operation in RECORD_OPS:
            allowed.append((PAR, CAP + 2))
        if operation == RESULTS_OP:
            allowed.append((FLAGS, FLAGS + 4))
        return allowed
    allowed.append((TIMER, TIMER + 4))
    if operation in (RESULTS_UPDATE, QUIT_UPDATE):
        allowed.append((ANIMATION, ANIMATION + 4))
    if operation == RESULTS_UPDATE:
        allowed.append((MISC_SCORE, MISC_SCORE + 4))
    return allowed


def out_of_table(observation: dict) -> bool:
    ranks = [row[4] for row in observation["rows"] if row[0] == RANK_ROW]
    return bool(ranks) and not 0 <= ranks[0] < 16


def check_original(_image, value: dict, observation: dict, native: list[int], _reference: dict) -> list[str]:
    parsed = parse_native(native)
    if parsed is None:
        return [f"invalid native protocol {native[:8]}"]
    if observation["fault"]:
        return [f"original faulted: {observation['fault']}"]
    operation = value["op"]
    problems = []
    rows, palette, cleared = expected_rows(value, observation)
    excluded = {offset for rect in observation["rects"] for offset in range(rect, rect + 8)}
    if palette is not None and palette != "uploaded" and cleared is not None:
        excluded |= set(range(cleared, cleared + 4))
    pristine = pristine_bytes(value)
    problems += call_difference(normalize(parsed["rows"], excluded, pristine), normalize(rows, excluded, pristine))

    status = parsed["status"]
    expected_status = observation["status"]
    finished_outside = operation == RESULTS_UPDATE and status == 2
    if finished_outside:
        if expected_status != 1 or not out_of_table(observation):
            problems.append("native finished without points for a rank inside the table")
    elif operation != PULSE_OP and status != expected_status:
        problems.append(f"status native {status} original {expected_status}")

    state = list(observation["state"])
    loaded = operation in INITIALIZERS and value["step"] == 1 and value["slot_state"] == 4
    if loaded and operation in RECORD_OPS and (value["size"] < RECORDS_END or value["room"] is None):
        state[7], state[8] = DEFAULT_RATING
    if finished_outside:
        state[13] = value["misc"]
    for name, native_word, original_word in zip(STATE_NAMES, parsed["state"], state, strict=True):
        if native_word != original_word:
            problems.append(f"{name} native {native_word} expected {original_word}")
    clut = observation["clut"] if operation not in INITIALIZERS or palette == "uploaded" else value["clut"]
    if parsed["clut"] != clut:
        first = next(i for i, (a, b) in enumerate(zip(parsed["clut"], clut, strict=True)) if a != b)
        problems.append(f"clutBuf word {first} native {parsed['clut'][first]:#06x} expected {clut[first]:#06x}")

    def changed(found: dict[int, int]) -> dict[int, int]:
        return {offset: byte for offset, byte in found.items()
                if offset not in excluded and byte != pristine.get(offset, 0)}

    native_dirty, original_dirty = changed(parsed["dirty"]), changed(observation["dirty"])
    if native_dirty != original_dirty:
        problems.append(f"file bytes changed: native {sorted(native_dirty.items())[:4]} original "
                        f"{sorted(original_dirty.items())[:4]}")
    return problems + store_problems(observation["stores"], allowed_stores(operation))


# ---------------------------------------------------------------------------------------------- reporting
def refusals(output: list[int]) -> list[int]:
    parsed = parse_native(output)
    return [] if parsed is None else [row[1] for row in parsed["rows"] if row[0] == REFUSE_ROW]


def describe_initializer(operation: int, results) -> list[str]:
    executed = [(v, o, n) for v, o, n in results if not o["fault"]]
    steps = {"0": 0, "1 loading": 0, "1 loaded": 0, "free": 0}
    for v, _, _ in executed:
        if v["step"] == 0:
            steps["0"] += 1
        elif v["step"] == 1:
            steps["1 loaded" if v["slot_state"] == 4 else "1 loading"] += 1
        else:
            steps["free"] += 1
    reasons = [0, 0, 0, 0]
    for _, _, n in executed:
        for reason in refusals(n):
            reasons[reason] += 1
    uploads = sum(row[0] == UPLOAD_ROW for _, o, _ in executed for row, tag in zip(o["rows"], o["tags"],
                                                                                    strict=True) if tag == "image")
    palettes = sum(1 for v, o, _ in executed if expected_rows(v, o)[1] == "uploaded")
    kinds = {}
    for v, _, _ in executed:
        if v["step"] == 1 and v["slot_state"] == 4:
            for kind in v["kinds"]:
                kinds[kind] = kinds.get(kind, 0) + 1
    lines = [
        "Compared the status, the load step, the six screen counters, the rating, the puzzle clock, the misc "
        "score, all 160 clutBuf words, every call in order (texture pages, the read with its _disFiles size and "
        "statics, each LoadImage RECT with its file offset, length and changed bytes, release and free), the file "
        "bytes changed afterwards and every original store.",
        f"Steps: {steps}; loaded TIM kinds {kinds}.",
        f"Original image uploads {uploads}, native palette uploads {palettes}; native refusals: image "
        f"{reasons[0]}, palette {reasons[1]}, records {reasons[2]}, room {reasons[3]}.",
    ]
    if operation in RECORD_OPS:
        matched = sum(1 for v, o, _ in executed if v["step"] == 1 and v["slot_state"] == 4
                      and v["size"] >= RECORDS_END and v["room"] is not None
                      and tuple(o["state"][7:9]) != DEFAULT_RATING)
        lines.append(f"{matched} loaded vectors took a record's rating other than the default 600/1.")
    if operation == RESULTS_OP:
        maximum = sum(1 for v, _, _ in executed if v["flags"][3] != 0)
        lines.append(f"{maximum} vectors set the 59'59\"99 maximum.")
    return lines


def describe_update(operation: int, results) -> list[str]:
    executed = [(v, o, n) for v, o, n in results if not o["fault"]]
    finished = sum(1 for _, o, _ in executed if o["status"] == 1)
    skipped = sum(1 for v, o, _ in executed if o["status"] == 0 and v["buttons"] & 0x20 and v["screen"][3] == 0)
    hidden = sum(1 for v, _, _ in executed if v["screen"][3] != 0)
    lines = [
        "Compared the status, the timer, the animation, the misc score and the other module words, every call in "
        "order (sound play and stop, texture-page retire, palette pulse weight, each message, rank and prompt "
        "with its origin and timers) and every original store.",
        f"{finished} vectors finished, {skipped} skipped to the end on an early Circle, {hidden} had a nonzero "
        "screen state.",
    ]
    if operation == RESULTS_UPDATE:
        outside = sum(1 for _, o, _ in executed if out_of_table(o))
        refused = sum(1 for _, o, n in executed if parse_native(n) and parse_native(n)["status"] == 2)
        ranks = sorted({row[4] for _, o, _ in executed for row in o["rows"] if row[0] == RANK_ROW and 0 <= row[4] < 16})
        lines.append(f"Ranks 0-15 seen: {len(ranks)}; {outside} vectors drew a rank outside the table, {refused} of "
                     "them finished, where native awards no points (the original read past _puzzleModeRankPoints).")
    return lines


def build_case(identifier: str) -> dict:
    operation, entry, end, name = SPECS[identifier]

    def vector(rng: random.Random, number: int) -> dict:
        return vector_for(identifier, rng, number)

    def describe(image, results) -> list[str]:
        verify_edits(EDITS[identifier], image)
        faults = sum(1 for _, o, _ in results if o["fault"])
        lines = [f"MENUF.PRG {name}: executed {(end - entry) // 4} original instructions including the return delay "
                 f"slot; {faults} original faults."]
        if operation == PULSE_OP:
            weights = sorted({v["weight"] for v, _, _ in results if 0 <= v["weight"] <= 16})
            lines.append("Compared all 160 clutBuf words afterwards, the LoadImage RECT and source, and every "
                         "original store.")
            lines.append(f"Blend weights 0-16 seen: {len(weights)}; "
                         f"{sum(not 0 <= v['weight'] <= 16 for v, _, _ in results)} outside 0-16.")
        elif operation in INITIALIZERS:
            lines += describe_initializer(operation, results)
        else:
            lines += describe_update(operation, results)
        return lines

    modules = (MODULE, BATTLE, SLUS) if operation in INITIALIZERS else (MODULE, SLUS)
    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "MODULES": modules,
        "DRIVER": "cube_lifecycle_driver.cpp",
        "SOURCES": (
            r"Native\Game\src\Menu\Results\CubePuzzleLifecycle.cpp",
            r"Native\Game\src\Menu\Results\ResultsPalette.cpp",
            r"Native\Engine\src\Render\TimImage.cpp",
            r"Native\Engine\src\Core\Math\ScalarMath.cpp",
            r"Native\Engine\src\Core\Math\Distance.cpp",
        ),
        "CXX_STANDARD": "c++latest",
        "VECTORS": 1024,
        "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 512,
        "MUTANTS": edit_mutants(EDITS[identifier]),
        "vector": vector,
        "run": run_original,
        "check": check_original,
        "describe": describe,
    }
