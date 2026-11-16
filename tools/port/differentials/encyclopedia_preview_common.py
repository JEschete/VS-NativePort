"""Shared harness for the encyclopedia preview differentials (MENU9.PRG 57BC.c:10-59, 592C.c:171-299, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::Score core
(EncyclopediaPreview.cpp) in encyclopedia_preview_driver.cpp. Every vector draws the load step byte
(vs_battle_loadObjectDataState), the ZUD id argument, the six statics at 0x8010A4A0 (the id read last,
the disc request, the ZUD file block, the model buffer, the companion variant and the palette flag), the
ZUD file table entry of the id (written over the module's _zudLbas, _zudSizes and _paletteFlags), the
disc queue's, high-end and low-end heap's replies, the disc request's state halfword, the asset queue's
first free request, the preview actor record's flags word and animation byte, and the ZUD file's bytes.
vs_menu9_loadZud follows vs_menu9_loadZudFile and vs_menu9_parseZudFile as code and vs_menu9_freeZud
follows vs_menu9_freeZudFile; every other callee is a recording stub.

The driver prints the return value, the load step, the statics, the record's flags word and animation
byte, then the calls: each stub as [kind, arguments...] and each queued asset request as all of its
fields, so the requests' carried-over fields are compared.

Native differences outside the compared domain (covered by EncyclopediaPreviewTests.cpp): ids outside the
256-entry tables, negative file sizes, a read while an earlier read or block is held, an exhausted disc
queue or heap, a null model buffer and ZUD sections past the file are refused (the original indexed,
leaked or dereferenced them). The vectors keep inside that domain; a native refusal prints a call the
original never makes.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MODULE_BASES,
    Machine,
    mode_case,
    recorded_observation,
    s32,
)

MODULE_BASES["MENU/MENU9.PRG"] = 0x80102800

MODULES = ("MENU/MENU9.PRG",)
DRIVER = "encyclopedia_preview_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\Score\EncyclopediaPreview.cpp",)
INCLUDES = (r"Native\Content\include",)

LOAD, FREE, POSE, QUEUE_READ, PARTS, FREE_BUFFER = range(6)

QUEUE_READ_ENTRY = 0x8010812C
PARTS_ENTRY = 0x8010823C
FREE_BUFFER_ENTRY = 0x801084E4

LOAD_STEP = 0x800E8F28
RECORDS = 0x800F4538
ZUD_LBAS = 0x80109904
ZUD_SIZES = 0x80109D04
PALETTE_FLAGS = 0x8010A104
STATICS = 0x8010A4A0
ZUD_ID, CD_SLOT, ZUD_DATA, ZUD_BUFFER, MODEL_VARIANT, PALETTE_FLAG = (STATICS + 4 * index for index in range(6))
STATICS_END = STATICS + 0x18

RECORD = 0x80300000
RECORD_BYTES = 0x800
FLAGS_OFFSET = 8
ANIMATION_OFFSET = 0x5B8
POSE_OFFSET = 0x704
DISC_REQUEST = 0x80310000
FILE = 0x80320000
BUFFER = 0x80340000

FREE_HEAP_HIGH = 0x80043C60
ALLOCATE_HEAP_HIGH = 0x80043E3C
ALLOCATE_HEAP = 0x80043EC4
ALLOCATE_DISC = 0x80044B10
FREE_DISC = 0x80044B80
ENQUEUE_DISC = 0x80044BF4
FIRST_FREE_ASSET = 0x800995B0
QUEUE_ASSET = 0x800995E8
UPDATE_ASSETS = 0x8009967C
RELEASE_MODEL = 0x8009CFB0
CLEAR_COMPANION = 0x8009F940
PLAY_ANIMATION = 0x800A0204
DISPLAY_FLAGS = 0x800A087C
INITIALIZE_POSE = 0x800AD008
RESET_PREVIEW_POSE = 0x800F9CB0

# The kinds the driver prints for each recorded call.
(CALL_ALLOCATE_READ, CALL_ALLOCATE_FILE, CALL_ENQUEUE, CALL_ALLOCATE_BUFFER, CALL_RELEASE_READ, CALL_QUEUE,
 CALL_UPDATE, CALL_FIRST_FREE, CALL_FREE, CALL_RELEASE_MODEL, CALL_RESET_POSE, CALL_PLAY, CALL_POSE,
 CALL_DISPLAY, CALL_COMPANION) = range(1, 16)

COMPANIONS = (30, 31, 32, 33, 172, 173)
DISC_STATES = (4, 4, 4, 0, 1, 2, 3, 5, 0x104, -4)
STEPS = (0, 0, 1, 1, 1, 2, 2, 3, 4, 0x80, 0xFF)
HEADER_BYTES = 0x30


def section(rng: random.Random, size: int, empty: bool = False) -> tuple[int, int]:
    offset = rng.choice((HEADER_BYTES, size, rng.randrange(size + 1), rng.randrange(HEADER_BYTES, size + 1)))
    room = size - offset
    length = 0 if empty else rng.choice((room, 1 if room else 0, rng.randrange(room + 1), rng.randrange(room + 1)))
    return offset, length


def zud_file(rng: random.Random) -> bytes:
    size = rng.choice((HEADER_BYTES, HEADER_BYTES + 1, 0x40, rng.randrange(HEADER_BYTES, 0x200)))
    data = bytearray(rng.randrange(256) for _ in range(size))
    data[1] = rng.choice((0, 0, rng.randrange(1, 256)))
    data[4] = rng.choice((0, 0, rng.randrange(1, 256)))
    for offset in (0x08, 0x10, 0x18, 0x20, 0x28):
        start, length = section(rng, size, offset in (0x20, 0x28) and rng.random() < 0.3)
        data[offset:offset + 4] = start.to_bytes(4, "little")
        data[offset + 4:offset + 8] = length.to_bytes(4, "little")
    return bytes(data)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    zud_id = rng.choice((*COMPANIONS, 0, 255, rng.randrange(256), rng.randrange(256)))
    step = STEPS[number % len(STEPS)] if mode == LOAD else rng.randrange(256)
    reading = mode == QUEUE_READ or (mode == LOAD and step == 0)
    parsing = mode == PARTS or (mode == LOAD and step == 1)
    pending = parsing and rng.random() < 0.6
    read = DISC_REQUEST if pending else 0
    file = FILE if pending or (parsing and rng.random() < 0.6) else 0
    if not parsing:
        read = 0 if reading else rng.choice((0, DISC_REQUEST))
        file = 0 if reading else rng.choice((0, FILE))
    return {
        "mode": mode,
        "step": step,
        "zud_id": zud_id,
        "statics": [
            rng.choice((0, 7, rng.randrange(256))),
            read,
            file,
            0 if reading else BUFFER + 4 * rng.randrange(1, 0x100),
            rng.choice((0, 0, 0, rng.randrange(1, 8))),
            rng.choice((0, 1, 2, 3, 0xFE, 0xFF, rng.randrange(256))),
        ],
        "file_entry": [rng.choice((0, 1, rng.randrange(1 << 31))),
                       rng.choice((1, 0x800, 0x1E5C, rng.randrange(1, 0x40000), 0x7FFFFFFF)),
                       rng.randrange(256)],
        "replies": [DISC_REQUEST + 0x20 * rng.randrange(1, 8), FILE + 0x10 * rng.randrange(1, 0x100),
                    BUFFER + 0x10 * rng.randrange(1, 0x100)],
        "disc_state": rng.choice(DISC_STATES),
        "first_free": rng.choice((0, 0, 1, 3, 15, 16)),
        "flags": rng.choice((0, 0xFFFFFFFF, 0x00800000, 0xFF7FFFFF, rng.randrange(1 << 32))),
        "animation": rng.randrange(256),
        "file": zud_file(rng),
    }


class Recorder:
    """The stubs: each records its call, and the queries answer from the vector."""

    def __init__(self, machine: Machine, value: dict) -> None:
        self.value = value
        self.calls: list[list[int]] = []
        replies = value["replies"]
        stubs = {
            ALLOCATE_DISC: lambda m: self.reply(m, [CALL_ALLOCATE_READ, s32(m.load(m.r[4])),
                                                    s32(m.load(m.r[4] + 4))], replies[0]),
            ALLOCATE_HEAP_HIGH: lambda m: self.reply(m, [CALL_ALLOCATE_FILE, m.r[4]], replies[1]),
            ENQUEUE_DISC: lambda m: self.record([CALL_ENQUEUE, m.r[4], m.r[5]]),
            ALLOCATE_HEAP: lambda m: self.reply(m, [CALL_ALLOCATE_BUFFER, m.r[4]], replies[2]),
            FREE_DISC: lambda m: self.record([CALL_RELEASE_READ, m.r[4]]),
            QUEUE_ASSET: self.queue,
            UPDATE_ASSETS: lambda m: self.record([CALL_UPDATE]),
            FIRST_FREE_ASSET: lambda m: self.reply(m, [CALL_FIRST_FREE], value["first_free"]),
            FREE_HEAP_HIGH: lambda m: self.record([CALL_FREE, m.r[4]]),
            RELEASE_MODEL: lambda m: self.record([CALL_RELEASE_MODEL, s32(m.r[4])]),
            RESET_PREVIEW_POSE: lambda m: self.record([CALL_RESET_POSE]),
            PLAY_ANIMATION: lambda m: self.record([CALL_PLAY, *(s32(m.r[r]) for r in (4, 5, 6, 7))]),
            INITIALIZE_POSE: lambda m: self.record([CALL_POSE, m.r[4], m.r[5]]),
            DISPLAY_FLAGS: lambda m: self.record([CALL_DISPLAY, s32(m.r[4]), s32(m.r[5])]),
            CLEAR_COMPANION: lambda m: self.record([CALL_COMPANION, *(s32(m.r[r]) for r in (4, 5, 6, 7))]),
        }
        for address, stub in stubs.items():
            machine.stub(address, stub)

    def record(self, call: list[int]) -> None:
        self.calls.append(call)

    def reply(self, machine: Machine, call: list[int], value: int) -> None:
        self.calls.append(call)
        machine.r[2] = value & 0xFFFFFFFF

    def queue(self, machine: Machine) -> None:
        request = machine.r[4]
        self.calls.append([
            CALL_QUEUE,
            machine.load(request, 1),
            machine.load(request + 1, 1, signed=True),
            machine.load(request + 2, 2, signed=True),
            machine.load(request + 4),
            machine.load(request + 8),
            machine.load(request + 0xC),
            *(machine.load(request + 0x10 + index, 1) for index in range(4)),
        ])


def setup(machine: Machine, value: dict) -> None:
    machine.map_zero(RECORD, RECORD_BYTES)
    machine.put(LOAD_STEP, value["step"], 1)
    machine.put(RECORDS + 4, RECORD)
    machine.put(RECORD + FLAGS_OFFSET, value["flags"])
    machine.put(RECORD + ANIMATION_OFFSET, value["animation"], 1)
    for index, field in enumerate(value["statics"]):
        machine.put(STATICS + 4 * index, field)
    zud_id = value["zud_id"]
    lba, size, palette = value["file_entry"]
    machine.put(ZUD_LBAS + 4 * zud_id, lba)
    machine.put(ZUD_SIZES + 4 * zud_id, size)
    machine.put(PALETTE_FLAGS + zud_id, palette, 1)
    if value["statics"][1]:
        machine.put(value["statics"][1], value["disc_state"], 2)
    machine.store(FILE, value["file"])


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    setup(machine, value)
    recorder = Recorder(machine, value)
    if mode == LOAD:
        machine.follow.update((QUEUE_READ_ENTRY, PARTS_ENTRY))
    elif mode == FREE:
        machine.follow.add(FREE_BUFFER_ENTRY)
    machine.r[4] = value["zud_id"]
    machine.execute(entry)

    values = [
        s32(machine.r[2]) if mode in (LOAD, PARTS) else 0,
        machine.load(LOAD_STEP, 1),
        s32(machine.load(ZUD_ID)),
        machine.load(CD_SLOT),
        machine.load(ZUD_DATA),
        machine.load(ZUD_BUFFER),
        s32(machine.load(MODEL_VARIANT)),
        s32(machine.load(PALETTE_FLAG)),
        machine.load(RECORD + FLAGS_OFFSET),
        machine.load(RECORD + ANIMATION_OFFSET, 1),
    ]
    names = ["return", "load step", "zud id", "disc request", "file", "model buffer", "variant", "palette flag",
             "record flags", "record animation"]
    inputs = [
        value["step"], value["zud_id"], *value["statics"], *value["file_entry"], *value["replies"],
        value["disc_state"], value["first_free"], RECORD, value["flags"], value["animation"],
        len(value["file"]), *value["file"],
    ]
    return recorded_observation(machine, values, names, recorder.calls), [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    return [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (LOAD_STEP, LOAD_STEP + 1),
        (STATICS, STATICS_END),
        (RECORD + FLAGS_OFFSET, RECORD + FLAGS_OFFSET + 4),
        (RECORD + ANIMATION_OFFSET, RECORD + ANIMATION_OFFSET + 1),
    ]


def describe(image, results) -> list[str]:
    queued = sum(sum(1 for call in observation["calls"] if call[0] == CALL_QUEUE) for _, observation, _ in results)
    called = sum(1 for _, observation, _ in results if observation["calls"])
    companions = sum(1 for value, _, _ in results if value["zud_id"] in COMPANIONS)
    steps = sorted({value["step"] for value, _, _ in results if value["mode"] == LOAD})
    lines = [f"{queued} asset requests queued, {called} of {len(results)} vectors made a recorded call, "
             f"{companions} vectors read a companion enemy"]
    if steps:
        lines.append(f"load steps {steps}")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]) | {"INCLUDES": INCLUDES}
