"""Shared harness for the floating label placement and equipment feedback differentials (BATTLE.PRG 2842C.c, #276).

Each case runs one original (F00719, F00720, F00722, F00725, F00738, F00739 or F00742) under the R3000 interpreter
against its typed VS::Battle core (FloatingLabelPlacement.cpp) in floating_placement_driver.cpp.

- UPDATE (F00719) follows the original placement search (F00720) and queue advance (F00721). The tagged bone
  (func_800A190C), RotTransPers, the ability-timing cue (func_800970BC), the six leaf draws and the compaction
  (func_8009639C) are stubs that record their arguments in call order; a leaf records the label's six words as the
  updater left them. The bone stub answers from a per-actor table and the projection is a fixed formula, so the
  native world computes the same results.
- PLACE (F00720) runs alone over a random placed-position owner.
- VALUE (F00722), STATUS (F00725) and EQUIP_DRAW (F00739) follow their real callees (F00723, F00726 and AddPrim)
  over seeded primitive storage; the label layer (ot - 20), the text layer (ot - 16) and the stat-change layer
  (ot + 8) are walked link by link and every packet word is compared, with the primitive words used.
- QUEUE (F00738) follows the original reserve and remove with the SLUS word copy and clear; sprintf("%d") and
  strlen are stubs over the interpreter's memory.
- COMMIT (F00742) runs alone.

Native refusals outside the compared domain (covered by FloatingLabelPlacementTests.cpp): a placed count outside
0-31, a text length past the record's four text bytes, a status frame past 30, a status layout index past the
32-entry table, a value caption mode past 8, a stat-change word past its table and a refused reserve. The
original's publish past a full queue counts 33, where the typed queue keeps 32, so an original count above 32
compares as 32 (as in floating_label_producers_common.py).
"""

from __future__ import annotations

import random
import struct
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Fault,
    Machine,
    mode_case,
    printed_problems,
    recorded_observation,
    s16,
    s32,
)
from differentials.floating_label_producers_common import stub_decimal_text

MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "floating_placement_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\FloatingLabelPlacement.cpp",
    r"Native\Game\src\Battle\FloatingLabelRendering.cpp",
    r"Native\Game\src\Battle\FloatingLabelQueue.cpp",
    r"Native\Game\src\Battle\FloatingLabelProducers.cpp",
    r"Native\Engine\src\Render\UiTexturedQuads.cpp",
    r"Native\Engine\src\Render\GameFont.cpp",
    r"Native\Engine\src\Render\UiPackets.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
INCLUDES = (r"Native\Tests\include",)

UPDATE, PLACE, VALUE, STATUS, QUEUE, EQUIP_DRAW, COMMIT = range(7)

STRIDE, CAPACITY = 24, 32
LABEL_WORDS = STRIDE * CAPACITY // 4
LABELS = 0x80200000
OVERFLOW_RECORDS = 32
PLACED = 0x80201000
POSITION = 0x80202000
OWNER = 0x800F1BAC
PLACED_OWNER = 0x800F1CBC
COUNT = 0x800F227E
PLACED_COUNT = 0x800F227C
CUE_FRAMES = 0x800F2272
STEP_TIMER = 0x800F2278
SUPPRESSION = 0x800F227A
PAUSED = 0x800F1BA4
TICK_SPEED = 0x8005E24C
SETTINGS = 0x80060020

PLACE_ENTRY, ADVANCE = 0x80094844, 0x80094AF8
CUE, TAGGED_BONE, PROJECT, COMPACT = 0x800970BC, 0x800A190C, 0x80041814, 0x8009639C
LEAVES = (0x8009506C, 0x80094B0C, 0x80095258, 0x800958A4, 0x80095A4C, 0x8009695C)
CUE_CALL, BONE_CALL, PROJECT_CALL, LEAF_CALL, COMPACT_CALL = range(1, 6)
ACTORS = 4

RESERVE, REMOVE, COPY, ZERO = 0x800962E4, 0x80096444, 0x800490B0, 0x80049110
STACK_FILL = 0x200

SCRATCH = 0x1F800000
PRIMITIVES = 0x80180000
PREFIX = 0x80170000
ORDERING = 0x801F8000
END_LINK = 0xFFFFFF
MASK = 0xFFFFFFFF
PRIMITIVE_BYTES = 4096
ADD_PRIMITIVE = 0x8002B4D4
VALUE_CAPTION, REMOVAL_MARKER = 0x80094E18, 0x800957D0
PREFIX_WORDS = (0x03FFFFFF, 0x60ABCDEF, 0x000B000A, 0x00020001)
SEED_STRIDE = 0x9E3779B9
# The label layer, the (unused) text layer and the stat-change layer.
BUCKETS = (ORDERING - 20, ORDERING - 16, ORDERING + 8)
EDGE_X = (-2147483648, -65536, -40, -9, -8, -1, 0, 7, 8, 100, 311, 312, 319, 320, 327, 328, 65535, 2147483647)
EDGE_Y = (-2147483648, -32769, -32768, -1, 0, 40, 100, 172, 200, 32767, 32768, 2147483647)


def digits(rng: random.Random) -> bytes:
    if rng.randrange(8) == 0:
        return rng.randbytes(4)
    return bytes(rng.choice(b"0123456789") if rng.randrange(6) else rng.randrange(256) for _ in range(4))


def update_record(rng: random.Random) -> bytes:
    raw = bytearray(rng.randbytes(STRIDE))
    small = (-40, -12, -1, 0, 0, 5, 20, 33, rng.randrange(-30, 31), rng.randrange(-30, 31))
    struct.pack_into("<3h", raw, 0, *(rng.choice((*small, s16(rng.getrandbits(16)))) for _ in range(3)))
    struct.pack_into("<h", raw, 6, rng.choice((-2, -1, -1, 0, 0, 1, 2, 3, -0x8000, s16(rng.getrandbits(16)))))
    raw[8] = rng.choice((0, 1, 1, 1, 2, 2, 2, 3, 4, 5, 6, 6, 6, 7, 8, 0x80, 0xFF))
    raw[9] = rng.choice((0, 0, 1, 2, 3, rng.randrange(256)))
    raw[11] = rng.choice((0, 5, 9, 10, 29, 30, 31, 44, 45, 46, 59, 60, 61, 89, 90, 91, 254, 255, rng.randrange(256)))
    raw[14] = rng.choice((0, 1, 2, 3, 0x80, 0x81, 0x82, rng.randrange(256)))
    raw[15] = rng.choice((0, 1, 4, 12, rng.randrange(256)))
    if rng.randrange(3):
        struct.pack_into("<2h", raw, 16, 0x1000, 0x1000)
    elif rng.randrange(2):
        struct.pack_into("<2h", raw, 16, rng.choice((0, 8, -8, 0x1000)), rng.choice((0, 16, 40, 0x1000)))
    return bytes(raw)


def queue_record(rng: random.Random) -> bytes:
    raw = bytearray(rng.randbytes(STRIDE))
    raw[8] = rng.choice((0, 1, 2, 5, 6, 6, 7, 7, 8, rng.randrange(256)))
    struct.pack_into("<h", raw, 6, rng.choice((-2, -1, 0, 1, 2, s16(rng.getrandbits(16)))))
    return bytes(raw)


def amount(rng: random.Random) -> int:
    return s32(rng.choice((0, 7, 42, 999, 1000, 0x7FFF, 0x8000, 0xFFFF, 0x80000000 | rng.randrange(2000),
                           0x7FFF0000 | rng.randrange(1 << 16), rng.getrandbits(32))))


def update_vector(rng: random.Random) -> dict:
    return {
        "count": rng.choice((0, 1, 2, 3, 5, 8, 16, 31, 32, rng.randrange(33))),
        "labels": b"".join(update_record(rng) for _ in range(CAPACITY)),
        "placed": rng.randbytes(CAPACITY * 4),
        "bones": [
            [rng.choice((0, 0, 0, 0, 1, -2)), *(rng.choice((-20, 0, 10, 300, rng.randrange(-40, 41),
                                                            s16(rng.getrandbits(16)))) for _ in range(3))]
            for _ in range(ACTORS)
        ],
        "cue": rng.choice((-2, -1, 0, 1, 2, 3, -0x8000, 0x7FFF)),
        "timer": rng.choice((-1, 0, 5, 18, 19, 20, 21, 22, 0x7FFF, -0x8000, s16(rng.getrandbits(16)))),
        "tick": rng.choice((-3, -1, 0, 1, 2, 3, 4, 5, 8, 40, 255, -0x80000000, 0x7FFFFFFF)),
        "paused": rng.choice((0, 0, 0, 1, 0x8000)),
        "suppression": rng.choice((0, 0, 1, -1)),
    }


def place_vector(rng: random.Random) -> dict:
    x = rng.choice((0, 31, 32, 100, 160, 288, 289, -0x8000, 0x7FFF, rng.randrange(-100, 400)))
    y = rng.choice((0, 39, 40, 100, 199, 200, 201, -0x8000, 0x7FFF, rng.randrange(-100, 300)))
    raw = bytearray(update_record(rng))
    if rng.randrange(4):
        struct.pack_into("<2h", raw, 16, 0x1000, 0x1000)
    count = rng.choice((0, 1, 2, 4, 8, 16, 30, 31, rng.randrange(32)))
    spread = rng.choice((8, 24, 48, 80))
    placed = b"".join(
        struct.pack("<2h", s16(x + rng.randrange(-spread, spread + 1)), s16(y + rng.randrange(-spread, spread + 1)))
        if rng.randrange(8) else rng.randbytes(4)
        for _ in range(CAPACITY)
    )
    return {"x": x, "y": y, "label": bytes(raw), "count": count, "placed": placed}


def draw_record(rng: random.Random, mode: int) -> bytes:
    raw = bytearray(rng.randbytes(STRIDE))
    raw[20:24] = digits(rng)
    raw[15] = rng.randrange(5)
    if mode == VALUE:
        raw[14] = rng.choice((0, 1, 2, rng.randrange(3, 9), rng.randrange(9)))
        raw[11] = rng.choice((0, 1, 15, 16, rng.randrange(32), rng.randrange(256)))
    elif mode == STATUS:
        struct.pack_into("<H", raw, 12, rng.randrange(32))
        raw[11] = rng.choice((0, 3, 4, 5, 6, 14, 15, 16, 19, 20, 24, 25, 26, 27, 30, rng.randrange(31)))
    else:
        raw[14] = rng.randrange(7)
        raw[10] = rng.randrange(13)
        struct.pack_into("<h", raw, 18, rng.choice((0, 10, 20, -10, s16(rng.getrandbits(16)))))
    return bytes(raw)


def draw_vector(rng: random.Random, mode: int) -> dict:
    x = rng.choice((*EDGE_X, rng.randrange(-60, 340), s32(rng.getrandbits(32))))
    y = rng.choice((*EDGE_Y, rng.randrange(-20, 260)))
    return {"x": x, "y": y, "label": draw_record(rng, mode), "seed": rng.getrandbits(32)}


def queue_vector(rng: random.Random) -> dict:
    return {
        "arguments": [
            rng.choice((0, 1, 2, 3, 4, 5, 6, 7, 0xF0, 0xF0, 0xF1, 0xF1, 0xF2, -1, rng.randrange(256))),
            rng.choice((0, 5, 12, rng.randrange(256))),
            amount(rng),
        ],
        "settings": rng.choice((0, 0x40, 0x80, 0xC0, 0xC0, 0xFFFFFFFF, rng.getrandbits(32))),
        "count": rng.choice((0, 1, 2, 5, 16, 30, 31, 32, rng.randrange(33))),
        "labels": b"".join(queue_record(rng) for _ in range(CAPACITY)),
        "stack": rng.randbytes(STACK_FILL),
    }


def commit_vector(rng: random.Random) -> dict:
    return {
        "count": rng.choice((0, 1, 2, 5, 16, 31, 32, rng.randrange(33))),
        "labels": b"".join(queue_record(rng) for _ in range(CAPACITY)),
    }


def vector(rng: random.Random, _number: int, mode: int) -> dict:
    if mode == UPDATE:
        value = update_vector(rng)
    elif mode == PLACE:
        value = place_vector(rng)
    elif mode in (VALUE, STATUS, EQUIP_DRAW):
        value = draw_vector(rng, mode)
    elif mode == QUEUE:
        value = queue_vector(rng)
    else:
        value = commit_vector(rng)
    value["mode"] = mode
    return value


def words(data: bytes) -> list[int]:
    return list(struct.unpack(f"<{len(data) // 4}I", data))


def label_words(machine: Machine) -> list[int]:
    return words(machine.bytes(LABELS, STRIDE * CAPACITY))


def label_names() -> list[str]:
    return [f"label {index // 6} word {index % 6}" for index in range(LABEL_WORDS)]


def placed_names() -> list[str]:
    return [f"placed {index}" for index in range(CAPACITY)]


def map_labels(machine: Machine, labels: bytes, count: int) -> None:
    machine.map_zero(LABELS, STRIDE * (CAPACITY + OVERFLOW_RECORDS))
    machine.store(LABELS, labels)
    machine.put(OWNER, LABELS)
    machine.put(COUNT, count, 2)


def run_update(machine: Machine, value: dict, entry: int, calls: list[list[int]]):
    map_labels(machine, value["labels"], value["count"])
    machine.map_zero(PLACED, 0x100)
    machine.store(PLACED, value["placed"])
    machine.put(PLACED_OWNER, PLACED)
    machine.put(PLACED_COUNT, 0x5A5A, 2)
    machine.put(CUE_FRAMES, value["cue"], 2)
    machine.put(STEP_TIMER, value["timer"], 2)
    machine.put(SUPPRESSION, value["suppression"], 2)
    machine.put(PAUSED, value["paused"], 2)
    machine.put(TICK_SPEED, value["tick"] & MASK)
    machine.follow.update((PLACE_ENTRY, ADVANCE))

    def cue(_m: Machine) -> None:
        calls.append([CUE_CALL])

    def bone(m: Machine) -> None:
        actor, tag, out, endpoint = s32(m.r[4]), s32(m.r[5]), m.r[6], s32(m.r[7])
        calls.append([BONE_CALL, actor, tag, endpoint])
        status, x, y, z = value["bones"][actor % ACTORS]
        if status == 0:
            m.store(out, struct.pack("<4h", s16(x), s16(y), s16(z), 0))
        m.r[2] = status & MASK

    def project(m: Machine) -> None:
        x, y, z = struct.unpack("<3h", m.bytes(m.r[4], 6))
        calls.append([PROJECT_CALL, x, y, z])
        m.store(m.r[5], struct.pack("<2h", s16(x + 160), s16(y + 120)))
        m.r[2] = (z + 50) & MASK

    def leaf(code: int):
        def handler(m: Machine) -> None:
            index, rest = divmod(m.r[6] - LABELS, STRIDE)
            if rest or not 0 <= index < CAPACITY:
                raise Fault(f"leaf record {m.r[6]:#x} is outside the labels")
            calls.append([LEAF_CALL, code, index, s32(m.r[4]), s32(m.r[5]),
                          *words(m.bytes(m.r[6], STRIDE))])

        return handler

    def compact(m: Machine) -> None:
        calls.append([COMPACT_CALL, s32(m.r[4])])

    machine.stub(CUE, cue)
    machine.stub(TAGGED_BONE, bone)
    machine.stub(PROJECT, project)
    machine.stub(COMPACT, compact)
    for code, address in enumerate(LEAVES):
        machine.stub(address, leaf(code))
    machine.execute(entry)
    placed_count = machine.load(PLACED_COUNT, 2, signed=True)
    values = [
        *label_words(machine),
        placed_count,
        *words(machine.bytes(PLACED, CAPACITY * 4)),
        machine.load(CUE_FRAMES, 2, signed=True),
        machine.load(STEP_TIMER, 2, signed=True),
    ]
    names = [*label_names(), "placed count", *placed_names(), "cue frames", "step timer"]
    inputs = [
        value["count"],
        *words(value["labels"]),
        *words(value["placed"]),
        *(item for result in value["bones"] for item in result),
        value["cue"],
        value["timer"],
        value["tick"],
        int(value["paused"] != 0),
        int(value["suppression"] != 0),
    ]
    return values, names, inputs


def run_place(machine: Machine, value: dict, entry: int, _calls: list[list[int]]):
    map_labels(machine, value["label"], 1)
    machine.map_zero(PLACED, 0x100)
    machine.store(PLACED, value["placed"])
    machine.put(PLACED_OWNER, PLACED)
    machine.put(PLACED_COUNT, value["count"], 2)
    machine.map_zero(POSITION, 0x10)
    machine.store(POSITION, struct.pack("<2h", s16(value["x"]), s16(value["y"])))
    machine.r[4] = POSITION
    machine.r[5] = LABELS
    machine.execute(entry)
    x, y = struct.unpack("<2h", machine.bytes(POSITION, 4))
    values = [
        x,
        y,
        *words(machine.bytes(LABELS, STRIDE)),
        machine.load(PLACED_COUNT, 2, signed=True),
        *words(machine.bytes(PLACED, CAPACITY * 4)),
    ]
    names = ["x", "y", *(f"label word {index}" for index in range(6)), "placed count", *placed_names()]
    inputs = [s16(value["x"]), s16(value["y"]), *words(value["label"]), value["count"], *words(value["placed"])]
    return values, names, inputs


def traversal(machine: Machine, bucket: int) -> list[int]:
    """Walk actual OT links, retaining every command word and tag length."""
    result = []
    link = machine.load(bucket) & END_LINK
    visited = set()
    while link != END_LINK:
        if link in visited:
            raise Fault(f"primitive cycle at {link:#x}")
        visited.add(link)
        address = link | 0x80000000
        if address != PREFIX and not PRIMITIVES <= address < PRIMITIVES + PRIMITIVE_BYTES:
            raise Fault(f"primitive left its actual arena at {address:#x}")
        tag = machine.load(address)
        length = tag >> 24
        if length:
            result.append(length << 24)
            result.extend(machine.load(address + index * 4) for index in range(1, length + 1))
        link = tag & END_LINK
    return result


def run_draw(machine: Machine, value: dict, entry: int, _calls: list[list[int]]):
    machine.map_zero(SCRATCH, 0x100)
    machine.map_zero(ORDERING - 0x20, 0x40)
    machine.map_zero(PRIMITIVES, PRIMITIVE_BYTES)
    for index in range(PRIMITIVE_BYTES // 4):
        machine.put(PRIMITIVES + index * 4, value["seed"] ^ ((index * SEED_STRIDE) & MASK))
    machine.store(PREFIX, struct.pack("<4I", *PREFIX_WORDS))
    map_labels(machine, value["label"], 1)
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, ORDERING)
    for bucket in BUCKETS:
        machine.put(bucket, PREFIX & END_LINK)
    machine.follow.update((ADD_PRIMITIVE, VALUE_CAPTION, REMOVAL_MARKER))
    machine.r[4] = value["x"] & MASK
    machine.r[5] = value["y"] & MASK
    machine.r[6] = LABELS
    machine.execute(entry)
    values = [(machine.load(SCRATCH) - PRIMITIVES) // 4, *words(machine.bytes(LABELS, STRIDE))]
    names = ["used words", *(f"label word {index}" for index in range(6))]
    for layer, bucket in enumerate(BUCKETS):
        packets = traversal(machine, bucket)
        values += [len(packets), *packets]
        names += [f"layer {layer} words", *(f"layer {layer} word {index}" for index in range(len(packets)))]
    inputs = [value["x"], value["y"], *words(value["label"]), value["seed"]]
    return values, names, inputs


def run_queue(machine: Machine, value: dict, entry: int, _calls: list[list[int]]):
    map_labels(machine, value["labels"], value["count"])
    machine.put(SETTINGS, value["settings"] & MASK)
    machine.store(DEFAULT_STACK - STACK_FILL, value["stack"])
    machine.follow.update((RESERVE, REMOVE, COPY, ZERO))

    stub_decimal_text(machine)
    for register, argument in zip((4, 5, 6), value["arguments"], strict=True):
        machine.r[register] = argument & MASK
    machine.execute(entry)
    count = machine.load(COUNT, 2, signed=True)
    values = [s32(machine.r[2]), min(count, CAPACITY), *label_words(machine)]
    names = ["return", "count", *label_names()]
    weapon = int(value["settings"] & 0x40 != 0)
    armor = int(value["settings"] & 0x80 != 0)
    inputs = [*value["arguments"], weapon, armor, value["count"], *words(value["labels"])]
    return values, names, inputs


def run_commit(machine: Machine, value: dict, entry: int, _calls: list[list[int]]):
    map_labels(machine, value["labels"], value["count"])
    machine.execute(entry)
    values = [machine.load(COUNT, 2, signed=True), *label_words(machine)]
    return values, ["count", *label_names()], [value["count"], *words(value["labels"])]


RUNNERS = {
    UPDATE: run_update,
    PLACE: run_place,
    VALUE: run_draw,
    STATUS: run_draw,
    QUEUE: run_queue,
    EQUIP_DRAW: run_draw,
    COMMIT: run_commit,
}


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    calls: list[list[int]] = []
    values, names, inputs = RUNNERS[value["mode"]](machine, value, entry, calls)
    return recorded_observation(machine, values, names, calls), [value["mode"], *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    stack = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK + 16)  # includes the caller's argument home slots
    labels = (LABELS, LABELS + STRIDE * CAPACITY)
    mode = value["mode"]
    if mode == UPDATE:
        return [stack, labels, (PLACED, PLACED + CAPACITY * 4), (PLACED_COUNT, PLACED_COUNT + 2),
                (CUE_FRAMES, CUE_FRAMES + 2), (STEP_TIMER, STEP_TIMER + 2)]
    if mode == PLACE:
        return [stack, (LABELS + 16, LABELS + 20), (PLACED, PLACED + CAPACITY * 4),
                (PLACED_COUNT, PLACED_COUNT + 2), (POSITION, POSITION + 4)]
    if mode == QUEUE:
        return [stack, labels, (COUNT, COUNT + 2)]
    if mode == COMMIT:
        return [labels]
    return [stack, (PRIMITIVES, PRIMITIVES + PRIMITIVE_BYTES), (ORDERING - 20, ORDERING + 12), (SCRATCH, SCRATCH + 4)]


def check(_image, value, observation, native, _reference):
    return printed_problems(observation, native, allowed_stores(value))


def describe(_image, results) -> list[str]:
    if not results:
        return []
    mode = results[0][0]["mode"]
    calls = sum(len(observation["calls"]) for _, observation, _ in results)
    lines = [f"{len(results)} vectors; {calls} recorded callee calls"]
    if mode == UPDATE:
        leaves = sum(1 for _, observation, _ in results for call in observation["calls"] if call[0] == LEAF_CALL)
        compactions = sum(1 for _, observation, _ in results for call in observation["calls"]
                          if call[0] == COMPACT_CALL)
        placed = sum(observation["values"][LABEL_WORDS] > 0 for _, observation, _ in results)
        lines.append(f"{leaves} leaf draws, {compactions} compactions, {placed} vectors placed value labels")
    elif mode in (VALUE, STATUS, EQUIP_DRAW):
        drawn = sum(observation["values"][0] > 0 for _, observation, _ in results)
        lines.append(f"{drawn} vectors drew; every packet word of the three OT layers compared")
    elif mode == QUEUE:
        grew = sum(observation["values"][1] > value["count"] for value, observation, _ in results)
        lines.append(f"{grew} vectors queued a stat change")
    lines.append("Compared every printed value and the stores outside the owners the original may write.")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    case = mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
    case["INCLUDES"] = INCLUDES
    case["VECTORS"] = 1024
    case["MUTANT_VECTORS"] = 256
    return case
