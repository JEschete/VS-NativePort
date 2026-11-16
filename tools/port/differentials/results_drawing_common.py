"""Shared R3000 vectors and native driver protocol for B50-03 results drawing (MENUF.PRG 3B8.c:1877-1993, 2117-2165).

The original side runs the function's own instructions with the SDK AddPrim (SLUS_010.40) followed, _renderTime
following _renderTextureFadeInClut, sprintf modelled for its two formats and the compact text call recorded. The
native driver replays the same atlas, stale primitive words and ordering link through Menu::Results with a
ResultsDrawTarget that models the PS1 primitive cursor and ordering entry.
"""

from __future__ import annotations

import random

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MASK,
    MODULE_BASES,
    Machine,
    s32,
    store_problems,
)

MODULE = "MENU/MENUF.PRG"
MODULE_BASES[MODULE] = 0x80102800

SCRATCH = 0x1F800000
PRIMITIVES = 0x80180000
ORDERING = 0x801F8000
QUAD_ENTRY = ORDERING - 0x1C
PACKET_WORDS = 10
PACKET_BYTES = PACKET_WORDS * 4
PACKET_CAPACITY = 12
ATLAS = 0x801091D8  # _disMap
ATLAS_ENTRIES = 127
ARGUMENT_FIVE = DEFAULT_STACK + 0x10
ADD_PRIMITIVE = 0x8002B4D4
SPRINTF = 0x80026EF4
COMPACT_TEXT = 0x800C6540  # vs_battle_renderTextRawColor (BATTLE.PRG)
FADE_CLUT = 0x80105DD8
TEXTS = {b"PRESS": 0, b"BUTTON": 1}
NO_TIME = 0x800000

# operation, entry, end (one past the return delay slot), name.
SPECS = {
    "F03040": (0, 0x80105C34, 0x80105DD8, "_renderTextureFadeIn (3B8.c:1877)"),
    "F03041": (1, 0x80105DD8, 0x80105F6C, "_renderTextureFadeInClut (3B8.c:1914)"),
    "F03042": (2, 0x80105F6C, 0x801060A8, "_renderTime (3B8.c:1951)"),
    "F03044": (3, 0x801064D4, 0x8010664C, "_renderPressButtonPrompt (3B8.c:2117)"),
}

# Encodings checked against the MENUF.PRG disassembly: blez/bltz a3; slti v0,a3,imm; sll v0,a3,n;
# ori v0,v0,imm; addiu v0|v1,..,imm; sh t0,0xe(s1); lui v0,0x80; addiu s2,zero,clut; addiu a2,zero,index;
# addiu a1,s3,-n; addiu s0,s0,n; slti v0,s1,6; bltz/blez a2; slti v0,a2,0x31; addiu v1,s3,0x120;
# addiu s0,s3,0x12; ori a2,a2,0x80ff; slti v0,a3,9; sra v0,a3,1; addiu v0,zero,0x37fb|0x2d|0xc.
MUTATIONS = {
    "F03040": {
        "F03040-draws-at-zero-brightness": {0x80105C40: 0x04E00060},
        "F03040-fades-for-nine-steps": {0x80105D4C: 0x28E20009},
        "F03040-fade-scale-eight": {0x80105D54: 0x000710C0},
        "F03040-fade-subtractive-blend": {0x80105D6C: 0x34420040},
        "F03040-fade-palette-plus-two": {0x80105D7C: 0x24420002},
        "F03040-full-brightness-127": {0x80105D80: 0x2402007F},
        "F03040-opaque": {0x80105D44: 0x34420000},
        "F03040-links-six-entries-back": {0x80105DBC: 0x2484FFE8},
        "F03040-cursor-advances-36": {0x80105DB4: 0x26310024},
    },
    "F03041": {
        "F03041-draws-at-zero-brightness": {0x80105DE8: 0x04E0005B},
        "F03041-fades-for-nine-steps": {0x80105EF4: 0x28E20009},
        "F03041-fade-palette-plus-two": {0x80105F10: 0x25030002},
        "F03041-fade-subtractive-blend": {0x80105F1C: 0x34420040},
        "F03041-full-brightness-127": {0x80105F20: 0x2402007F},
        "F03041-palette-not-stored": {0x80105F34: 0x00000000},
    },
    "F03042": {
        "F03042-no-time-sentinel-0x810000": {0x80105F90: 0x3C020081},
        "F03042-time-palette-0x7ff4": {0x80105FE0: 0x24127FF4},
        "F03042-record-palette-0x7ff6": {0x80105FEC: 0x24127FF6},
        "F03042-dash-glyph-82": {0x80106010: 0x24060052},
        "F03042-digit-rise-two": {0x8010601C: 0x2665FFFE},
        "F03042-glyph-advance-11": {0x80106034: 0x2610000B},
        "F03042-prime-uses-double-prime": {0x80106054: 0x2406004A},
        "F03042-prime-advance-7": {0x80106074: 0x26100007},
        "F03042-five-glyphs": {0x8010607C: 0x2A220005},
    },
    "F03044": {
        "F03044-hides-at-zero-slide": {0x801064F4: 0x18C0004E},
        "F03044-slide-clamp-test-50": {0x801064FC: 0x28C20032},
        "F03044-offset-289": {0x80106504: 0x26630121},
        "F03044-text-indent-19": {0x80106524: 0x26700013},
        "F03044-text-color-0x80fe": {0x80106538: 0x34C680FE},
        "F03044-bounce-turns-at-seven": {0x8010656C: 0x28E20007},
        "F03044-bounce-rise-quarter": {0x8010657C: 0x00071083},
        "F03044-icon-palette-0x37fc": {0x801065F8: 0x240237FC},
        "F03044-icon-shaded": {0x801065AC: 0x2402002C},
        "F03044-icon-page-0xd": {0x801065F0: 0x2402000D},
    },
}

EDGE_WORDS = (0, 1, -1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF, -0x80000000, -0x8000, -0x8001)
EDGE_LEVELS = (-1, 0, 1, 2, 7, 8, 9, 15, 16, 0x7FFFFFFF, -0x80000000, 0x10000007)


def coordinate(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.75:
        return rng.randrange(-64, 400)
    if roll < 0.9:
        return rng.choice(EDGE_WORDS)
    return s32(rng.getrandbits(32))


def level(rng: random.Random, number: int) -> int:
    pattern = number % 5
    if pattern == 0:
        return EDGE_LEVELS[number // 5 % len(EDGE_LEVELS)]
    if pattern == 4:
        return s32(rng.getrandbits(32))
    return rng.randrange(-2, 12)


def texture_index(rng: random.Random, number: int) -> int:
    if number % 23 == 7:
        return rng.choice((-1, -0x80000000, ATLAS_ENTRIES, ATLAS_ENTRIES + 1, 0x7FFFFFFF, rng.randrange(128, 4096)))
    return rng.randrange(ATLAS_ENTRIES)


def clut(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.3:
        return rng.choice((0x7FF2, 0x7FF8, 0xFFFF, 0x1FFFF, -1, 0, 0x10000))
    return s32(rng.getrandbits(32)) if roll < 0.6 else rng.getrandbits(16)


def time_value(rng: random.Random, number: int) -> int:
    pattern = number % 6
    if pattern == 0:
        return NO_TIME
    if pattern == 1:
        return rng.randrange(100) << 16 | rng.randrange(60) << 8 | rng.randrange(60)
    if pattern == 2:
        return rng.getrandbits(24)
    if pattern == 3:
        return rng.choice((NO_TIME + 1, NO_TIME - 1, NO_TIME | 0x1000000, -NO_TIME, 0, -1, 0x00FFFFFF))
    return s32(rng.getrandbits(32))


def slide(rng: random.Random, number: int) -> int:
    pattern = number % 4
    if pattern == 0:
        return rng.choice((-1, 0, 1, 47, 48, 49, 50, -0x80000000, 0x7FFFFFFF))
    if pattern == 3:
        return s32(rng.getrandbits(32))
    return rng.randrange(-4, 60)


def bounce(rng: random.Random, number: int) -> int:
    return number % 16 if number % 3 else s32(rng.getrandbits(32))


def atlas_entry(rng: random.Random) -> list[int]:
    return [rng.getrandbits(8), rng.getrandbits(8), rng.getrandbits(8), rng.getrandbits(8),
            rng.getrandbits(16), rng.getrandbits(16)]


def vector_for(identifier: str, rng: random.Random, number: int) -> dict:
    operation = SPECS[identifier][0]
    x, y = coordinate(rng), coordinate(rng)
    if operation == 0:
        arguments = [texture_index(rng, number), level(rng, number), 0]
    elif operation == 1:
        arguments = [texture_index(rng, number), level(rng, number), clut(rng)]
    elif operation == 2:
        record = rng.choice((0, 0, 1, -1, 0x100, s32(rng.getrandbits(32))))
        arguments = [level(rng, number), time_value(rng, number), record]
    else:
        arguments = [slide(rng, number), bounce(rng, number), 0]
    return {
        "operation": operation,
        "x": x,
        "y": y,
        "arguments": arguments,
        "link": rng.getrandbits(24),
        "stale": [rng.getrandbits(32) for _ in range(PACKET_CAPACITY * PACKET_WORDS)],
        # None keeps the module's own _disMap.
        "atlas": None if number % 2 == 0 else [atlas_entry(rng) for _ in range(ATLAS_ENTRIES)],
    }


def refused(value: dict) -> bool:
    """The native core refuses an atlas index the original would read past _disMap with."""
    if value["operation"] not in (0, 1):
        return False
    index, brightness, _ = value["arguments"]
    return brightness > 0 and not 0 <= index < ATLAS_ENTRIES


def c_string(machine: Machine, address: int) -> bytes:
    text = bytearray()
    while (byte := machine.byte(address + len(text))) != 0:
        text.append(byte)
    return bytes(text)


def sprintf(machine: Machine) -> None:
    """The two formats _renderTime prints: "------" and "%02d%02d%02d"."""
    form = c_string(machine, machine.r[5])
    arguments = [machine.r[6], machine.r[7], machine.load(machine.r[29] + 0x10)]
    text = bytearray()
    position = 0
    while position < len(form):
        if form[position:position + 4] == b"%02d":
            text += b"%02d" % s32(arguments.pop(0))
            position += 4
        elif form[position] == ord("%"):
            raise ValueError(f"unmodelled format {form!r}")
        else:
            text.append(form[position])
            position += 1
    machine.store(machine.r[4], bytes(text) + b"\0")
    machine.r[2] = len(text)


def run_original(machine: Machine, value: dict) -> tuple[dict, list[int]]:
    operation, entry = value["operation"], next(spec[1] for spec in SPECS.values() if spec[0] == value["operation"])
    atlas = value["atlas"]
    if atlas is None:
        atlas = [[machine.byte(ATLAS + 8 * index + field) for field in range(4)]
                 + [machine.load(ATLAS + 8 * index + 4, 2), machine.load(ATLAS + 8 * index + 6, 2)]
                 for index in range(ATLAS_ENTRIES)]
    else:
        for index, (u, v, w, h, page, palette) in enumerate(atlas):
            machine.store(ATLAS + 8 * index, bytes((u, v, w, h)) + page.to_bytes(2, "little")
                          + palette.to_bytes(2, "little"))
    inputs = [operation, value["x"], value["y"], *value["arguments"], value["link"], *value["stale"],
              *(field for entry_fields in atlas for field in entry_fields)]
    if refused(value):
        return {"refused": True}, inputs
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(DEFAULT_STACK, 0x20)
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 4, ORDERING)
    machine.map_zero(QUAD_ENTRY, 4)
    machine.put(QUAD_ENTRY, value["link"])
    for index, word in enumerate(value["stale"]):
        machine.put(PRIMITIVES + 4 * index, word)
    texts: list[list[int]] = []

    def text(m: Machine) -> None:
        texts.append([TEXTS.get(c_string(m, m.r[4]), 2), s32(m.r[5]), s32(m.r[6]), m.r[7]])

    machine.stub(SPRINTF, sprintf)
    machine.stub(COMPACT_TEXT, text)
    machine.follow.add(ADD_PRIMITIVE)
    machine.follow.add(FADE_CLUT)
    machine.r[4] = value["x"] & MASK
    machine.r[5] = value["y"] & MASK
    machine.r[6] = value["arguments"][0] & MASK
    machine.r[7] = value["arguments"][1] & MASK
    machine.put(ARGUMENT_FIVE, value["arguments"][2])
    machine.execute(entry)
    cursor = machine.load(SCRATCH)
    packets = (cursor - PRIMITIVES) // PACKET_BYTES
    problems = []
    if cursor < PRIMITIVES or (cursor - PRIMITIVES) % PACKET_BYTES or packets > PACKET_CAPACITY:
        problems.append(f"original cursor {cursor:#x} is not a whole packet past the buffer")
        packets = 0
    words = [machine.load(PRIMITIVES + 4 * index) for index in range(packets * PACKET_WORDS)]
    return {
        "refused": False,
        "problems": problems,
        "cursor": cursor,
        "ordering": machine.load(QUAD_ENTRY),
        "ordering_table": machine.load(SCRATCH + 4),
        "packets": packets,
        "words": words,
        "texts": texts,
        "stores": list(machine.stores),
    }, inputs


def check_original(_image, value: dict, observation: dict, native: list[int], _reference: dict) -> list[str]:
    if len(native) < 4:
        return [f"invalid native protocol {native}"]
    status, ordering, packets = native[0], native[1], native[2]
    words = native[3:3 + packets * PACKET_WORDS]
    rest = native[3 + packets * PACKET_WORDS:]
    if observation["refused"]:
        return [] if status == 2 and packets == 0 and rest == [0] else [f"native drew a refused index {native[:4]}"]
    problems = list(observation["problems"])
    expected_status = 0 if observation["packets"] else 1
    if status != expected_status:
        problems.append(f"status native {status} original {expected_status}")
    if packets != observation["packets"]:
        problems.append(f"packets native {packets} original {observation['packets']}")
    if ordering & MASK != observation["ordering"]:
        problems.append(f"ordering entry native {ordering & MASK:#x} original {observation['ordering']:#x}")
    if PRIMITIVES + PACKET_BYTES * packets != observation["cursor"]:
        problems.append(f"cursor native {PRIMITIVES + PACKET_BYTES * packets:#x} original {observation['cursor']:#x}")
    if observation["ordering_table"] != ORDERING:
        problems.append("the original moved the ordering table pointer")
    for index, (mine, theirs) in enumerate(zip(words, observation["words"], strict=False)):
        if mine & MASK != theirs:
            problems.append(f"packet {index // PACKET_WORDS} word {index % PACKET_WORDS}: "
                            f"native {mine & MASK:#010x} original {theirs:#010x}")
    texts = [entry for record in observation["texts"] for entry in record]
    if not rest or rest[0] != len(observation["texts"]) or rest[1:] != texts:
        problems.append(f"texts native {rest} original {observation['texts']}")
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK), (SCRATCH, SCRATCH + 4),
               (QUAD_ENTRY, QUAD_ENTRY + 4), (PRIMITIVES, PRIMITIVES + PACKET_BYTES * observation["packets"])]
    return problems + store_problems(observation["stores"], allowed)


def build_case(identifier: str) -> dict:
    operation, entry, end, name = SPECS[identifier]

    def vector(rng: random.Random, number: int) -> dict:
        return vector_for(identifier, rng, number)

    def describe(_image, results) -> list[str]:
        executed = [(v, o) for v, o, _ in results if not o["refused"]]
        drawn = sum(o["packets"] > 0 for _, o in executed)
        lines = [
            f"MENUF.PRG {name}: executed {(end - entry) // 4} original instructions including the return delay "
            "slot, with the SDK AddPrim (SLUS_010.40 0x8002B4D4) followed"
            + (" and _renderTextureFadeInClut followed." if operation == 2 else "."),
            "Compared every written FT4 word (stale words seeded), the primitive cursor, the ordering entry seven "
            "words before the table, the drawn text (string, xy, color, chain) and every original store.",
            f"{len(results) - len(executed)} vectors are documented refusals (an atlas index outside the 127 "
            "_disMap entries at a positive brightness) the original would read past the table with; native must "
            "refuse exactly those.",
            f"{drawn} vectors drew, {len(executed) - drawn} drew nothing.",
        ]
        if operation in (0, 1, 2):
            level_index = 1 if operation < 2 else 0
            fading = sum(0 < v["arguments"][level_index] < 8 for v, o in executed if o["packets"])
            lines.append(f"{fading} drawn vectors were still fading in (1-7), {drawn - fading} at full brightness.")
        if operation == 2:
            dashes = sum(v["arguments"][1] == NO_TIME for v, o in executed if o["packets"])
            wide = sum(any(((v["arguments"][1] >> shift) & 0xFF) >= 100 for shift in (0, 8, 16))
                       and v["arguments"][1] != NO_TIME for v, o in executed if o["packets"])
            lines.append(f"{dashes} drawn times were dashes, {wide} had a byte of 100 or more.")
        if operation == 3:
            clamped = sum(v["arguments"][0] > 48 for v, o in executed if o["packets"])
            late = sum(v["arguments"][1] & 0xF >= 9 for v, o in executed if o["packets"])
            lines.append(f"{clamped} drawn prompts clamped the slide, {late} bounced on the falling half.")
        return lines

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "MODULES": (MODULE, "SLUS_010.40"),
        "DRIVER": "results_drawing_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\Results\ResultsDrawing.cpp",
                    r"Native\Engine\src\Render\UiTexturedQuads.cpp"),
        "CXX_STANDARD": "c++latest",
        "VECTORS": 1024,
        "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 512,
        "MUTANTS": MUTATIONS[identifier],
        "vector": vector,
        "run": run_original,
        "check": check_original,
        "describe": describe,
    }
