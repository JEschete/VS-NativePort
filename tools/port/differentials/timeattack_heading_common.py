"""Shared R3000 vectors and native driver protocol for B50-06 time-attack heading, countdown and palette pulse
(MENUF.PRG 3B8.c:1146-1292).

The original side runs each function's own instructions with its callees (_renderTexturePopIn,
_renderTextureFadeIn, _renderTextureFadeInClut, _renderTime, LoadImage) stubbed and recorded at the call
boundary: the origin, the texture index, the brightness or timer, the palette and record words on the stack, both
CVECTORs behind the color pointer and, for LoadImage, the RECT and the 160 halfwords it uploads. The time-attack
course (vs_main_stateFlags.timeAttackBoss) and the first-place trial scores live outside MENUF.PRG and are placed in
the machine's overlay. The native driver replays the same atlas, course, scores and palette through Menu::Results
with a recording TimeAttackDrawHost and ResultsClutTarget.
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
    atlas_entry,
    call_difference,
    coordinate,
    edit_mutants,
    place_atlas,
    verify_edits,
)

MODULE = "MENU/MENUF.PRG"
MODULE_BASES[MODULE] = 0x80102800

ATLAS_ENTRIES = 127
HEADING_COLORS = (0x8010972C, 0x80109738)  # _renderTimeAttackHeader's static CVECTORs
CLUT = 0x801098AC  # _clutBuf
CLUT_WORDS = 160
PULSE_TARGET = (CLUT + 2 * 128, CLUT + 2 * CLUT_WORDS)
COURSE = 0x8006165C  # vs_main_stateFlags.timeAttackBoss
SCORES = 0x8005FEC8  # vs_main_scoredata.bossTimeTrialScores
SCORE_ROW = 12
COURSES = 8

POP_IN = 0x80106A80  # _renderTexturePopIn
FADE_IN = 0x80105C34  # _renderTextureFadeIn
FADE_IN_CLUT = 0x80105DD8  # _renderTextureFadeInClut
RENDER_TIME = 0x80105F6C  # _renderTime
LOAD_IMAGE = 0x800288FC

# Recorded call kinds, as the driver prints them.
FLASH, FADE, FADE_CLUT, TIME = range(4)
CALL_WORDS = 16  # kind, x, y, index, brightness/timer, palette, time, record, eight color bytes
INDEXED = (FLASH, FADE, FADE_CLUT)
UPLOAD_WORDS = 4 + CLUT_WORDS

# operation, entry, end (one past the return delay slot), name.
SPECS = {
    "F03018": (0, 0x8010459C, 0x80104650, "_renderTimeAttackHeader (3B8.c:1146)"),
    "F03019": (1, 0x80104650, 0x801046F8, "_renderCourse (3B8.c:1175)"),
    "F03020": (2, 0x801046F8, 0x801047D4, "_renderBossName (3B8.c:1195)"),
    "F03021": (3, 0x801047D4, 0x8010489C, "_renderBestTimeHeader (3B8.c:1226)"),
    "F03022": (4, 0x8010489C, 0x80104914, "_renderStartCountdown (3B8.c:1248)"),
    "F03023": (5, 0x80104914, 0x80104A50, "_pulseTimeAttackEnd (3B8.c:1256)"),
}

# name: (address, original word, mutated word). The original words are checked against MENUF.PRG in describe();
# MUTANTS stays static so case discovery and --list never read a disc image.
EDITS = {
    "F03018": {
        "F03018-clamps-from-48": (0x801045C8, 0x28820041, 0x28820031),  # slti v0,a0,0x41
        "F03018-clamps-to-63": (0x801045D4, 0x24040040, 0x2404003F),
        "F03018-heading-entry-99": (0x801045E0, 0x24060064, 0x24060063),
        "F03018-dark-step-unset": (0x80104600, 0xA2240003, 0x00000000),
        "F03018-centres-by-quarter": (0x8010460C, 0x00021043, 0x00021083),
        "F03018-second-entry-102": (0x80104620, 0x24060065, 0x24060066),
        "F03018-second-half-first-color": (0x80104628, 0x26270004, 0x26270000),
    },
    "F03019": {
        "F03019-clamps-from-48": (0x8010466C, 0x2A220041, 0x2A220031),
        "F03019-clamps-to-63": (0x8010467C, 0x24110040, 0x2411003F),
        "F03019-label-entry-83": (0x80104688, 0x24060054, 0x24060053),
        "F03019-padding-25": (0x8010469C, 0x24420018, 0x24420019),
        "F03019-digit-same-row": (0x801046B0, 0x2665FFFF, 0x26650000),
        "F03019-palette-7ff5": (0x801046B8, 0x24027FF4, 0x24027FF5),
        "F03019-gap-13": (0x801046BC, 0x2643000C, 0x2643000D),
        "F03019-digit-is-course": (0x801046D8, 0x24C60001, 0x24C60000),
    },
    "F03020": {
        "F03020-paired-course-5": (0x80104738, 0x24020006, 0x24020005),
        "F03020-first-name-86": (0x80104750, 0x24C60055, 0x24C60056),
        "F03020-centres-whole-width": (0x80104764, 0x00042042, 0x00042002),
        "F03020-death-entry-92": (0x80104770, 0x2406005B, 0x2406005C),
        "F03020-pair-centres-by-quarter": (0x8010478C, 0x00021043, 0x00021083),
        "F03020-ogre-entry-94": (0x801047A0, 0x2406005D, 0x2406005E),
    },
    "F03021": {
        "F03021-label-entry-96": (0x8010480C, 0x2406005F, 0x24060060),
        "F03021-padding-97": (0x80104820, 0x24420060, 0x24420061),
        "F03021-time-mask-drops-low-bits": (0x80104838, 0x3529FFFF, 0x3529FFF0),
        "F03021-drop-3": (0x8010483C, 0x26650002, 0x26650003),
        "F03021-second-place": (0x80104868, 0x8C470028, 0x8C47002C),
        "F03021-gap-13": (0x8010486C, 0x2628000C, 0x2628000D),
        "F03021-record-palette": (0x80104870, 0xAFA00010, 0xAFB20010),  # sw s2 (the timer)
    },
    "F03022": {
        "F03022-numbers-every-7": (0x801048C8, 0x000840C3, 0x00084083),
        "F03022-first-number-97": (0x801048D0, 0x25060060, 0x25060061),
        "F03022-remainder-of-8": (0x801048E4, 0x00081100, 0x000810C0),
        "F03022-full-width": (0x801048F0, 0x00031842, 0x00031802),
        "F03022-full-height": (0x801048F4, 0x00094842, 0x00094802),
    },
    "F03023": {
        "F03023-weights-of-17": (0x80104928, 0x24020010, 0x24020011),
        "F03023-second-row-shifted": (0x80104930, 0x240B00C0, 0x240B00C2),
        "F03023-first-row-shifted": (0x80104934, 0x240A0040, 0x240A0042),
        "F03023-target-row-shifted": (0x80104938, 0x24090100, 0x24090102),
        "F03023-floors-negative-red": (0x80104978, 0x24A5000F, 0x24A50000),
        "F03023-red-by-eighth": (0x80104994, 0x00051103, 0x000510C3),
        "F03023-mask-bit-14": (0x801049EC, 0x30638000, 0x30634000),
        "F03023-31-colors": (0x80104A04, 0x29C20020, 0x29C2001F),
        "F03023-upload-row-510": (0x80104A18, 0x240201FF, 0x240201FE),
        "F03023-upload-159-wide": (0x80104A20, 0x240200A0, 0x2402009F),
    },
}



# ---------------------------------------------------------------------------------------------- vectors
EDGE_TIMERS = (-0x80000000, -1, 0, 1, 2, 31, 32, 33, 48, 49, 62, 63, 64, 65, 66, 0x7FFFFFFF)
EDGE_COUNTDOWN = (-0x80000000, -1455, -1454, -16, -15, -14, -1, 0, 1, 14, 15, 16, 29, 30, 44, 45, 59, 60, 464,
                  465, 0x7FFFFFFF)
EDGE_COURSES = (0, 5, 6, 7, 8, 41, 42, 125, 126, 127, 255)
EDGE_WEIGHTS = (-0x80000000, -1, 0, 1, 8, 15, 16, 17, 0x7FFFFFFF)
EDGE_COLORS = (0x0000, 0x7FFF, 0x8000, 0xFFFF, 0x001F, 0x03E0, 0x7C00, 0x8421)


def timer(rng: random.Random, operation: int, number: int) -> int:
    pattern = number % 4
    if operation == 4:
        if pattern == 0:
            return EDGE_COUNTDOWN[number // 4 % len(EDGE_COUNTDOWN)]
        if number % 32 == 3:
            return s32(rng.getrandbits(32))
        return rng.randrange(-20, 80) if pattern != 2 else rng.randrange(-1500, 500)
    if pattern == 0:
        return EDGE_TIMERS[number // 4 % len(EDGE_TIMERS)]
    if number % 32 == 3:
        return s32(rng.getrandbits(32))
    return rng.randrange(-3, 70)


def course(rng: random.Random, number: int) -> int:
    if number % 7 == 0:
        return EDGE_COURSES[number // 7 % len(EDGE_COURSES)]
    if number % 11 == 5:
        return rng.randrange(256)
    return rng.choice((0, 1, 2, 3, 4, 5, 6, 6, 7))


def score(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.2:
        return (rng.getrandbits(8) << 24) | 0x800000
    if roll < 0.3:
        return rng.choice((0, 0xFFFFFFFF, 0x00FFFFFF, 0xFF000000))
    return rng.getrandbits(32)


def weight(rng: random.Random, number: int) -> int:
    if number % 5 == 0:
        return EDGE_WEIGHTS[number // 5 % len(EDGE_WEIGHTS)]
    if number % 13 == 4:
        return s32(rng.getrandbits(32))
    return rng.randrange(17)


def color(rng: random.Random) -> int:
    return rng.choice(EDGE_COLORS) if rng.random() < 0.15 else rng.getrandbits(16)


def vector_for(identifier: str, rng: random.Random, number: int) -> dict:
    operation = SPECS[identifier][0]
    pulse = operation == 5
    return {
        "operation": operation,
        "x": coordinate(rng),
        "y": coordinate(rng),
        "timer": timer(rng, operation, number),
        "course": course(rng, number),
        "weight": weight(rng, number) if pulse else 0,
        # Every course's three trial scores; the driver gets the first places.
        "scores": [[score(rng) for _ in range(3)] for _ in range(COURSES)],
        "clut": [color(rng) for _ in range(CLUT_WORDS)] if pulse else [0] * CLUT_WORDS,
        # None keeps the module's own _disMap.
        "atlas": None if number % 2 == 0 else [atlas_entry(rng) for _ in range(ATLAS_ENTRIES)],
    }



def run_original(machine: Machine, value: dict) -> tuple[dict, list[int]]:
    operation = value["operation"]
    entry = next(spec[1] for spec in SPECS.values() if spec[0] == operation)
    atlas = place_atlas(machine, value["atlas"])
    firsts = [row[0] for row in value["scores"]]
    inputs = [operation, value["x"], value["y"], value["timer"], value["course"], value["weight"], *firsts,
              *value["clut"], *(field for entry_fields in atlas for field in entry_fields)]
    machine.put(COURSE, value["course"], 1)
    for row, words in enumerate(value["scores"]):
        for column, word in enumerate(words):
            machine.put(SCORES + SCORE_ROW * row + 4 * column, word)
    for index, word in enumerate(value["clut"]):
        machine.put(CLUT + 2 * index, word, 2)
    calls: list[list[int]] = []
    uploads: list[list[int]] = []

    def draw(kind: int):
        def record(m: Machine) -> None:
            call = [kind, s32(m.r[4]), s32(m.r[5])] + [0] * (CALL_WORDS - 3)
            if kind == FLASH:
                call[3] = s32(m.r[6])
                call[8:16] = list(m.bytes(m.r[7], 8))
            elif kind == TIME:
                call[4], call[6], call[7] = s32(m.r[6]), s32(m.r[7]), s32(m.load(m.r[29] + 0x10))
            else:
                call[3], call[4] = s32(m.r[6]), s32(m.r[7])
                if kind == FADE_CLUT:
                    call[5] = s32(m.load(m.r[29] + 0x10))
            calls.append(call)
        return record

    def load_image(m: Machine) -> None:
        area = [m.load(m.r[4] + 2 * i, 2, signed=True) for i in range(4)]
        if m.r[5] != CLUT:
            raise ValueError(f"LoadImage from {m.r[5]:#x}, not _clutBuf")
        uploads.append(area + [m.load(m.r[5] + 2 * i, 2) for i in range(CLUT_WORDS)])

    machine.stub(POP_IN, draw(FLASH))
    machine.stub(FADE_IN, draw(FADE))
    machine.stub(FADE_IN_CLUT, draw(FADE_CLUT))
    machine.stub(RENDER_TIME, draw(TIME))
    machine.stub(LOAD_IMAGE, load_image)
    machine.r[4] = (value["weight"] if operation == 5 else value["x"]) & MASK
    machine.r[5] = value["y"] & MASK
    machine.r[6] = value["timer"] & MASK
    try:
        machine.execute(entry)
    except Fault:
        # Only a course past the eight trial-score rows or a glyph far outside _disMap reads unmapped memory.
        return {"skipped": True}, inputs
    return {
        "skipped": False,
        "calls": calls,
        "uploads": uploads,
        "clut": [machine.load(CLUT + 2 * i, 2) for i in range(CLUT_WORDS)],
        "stores": list(machine.stores),
    }, inputs


def expects_refusal(value: dict, observation: dict) -> bool:
    """Native refuses what the original would draw from outside _disMap (a course digit, boss name or countdown
    number past entry 126) or read past the eight trial-score rows."""
    if observation["skipped"]:
        return True
    if value["operation"] == 3 and observation["calls"] and value["course"] >= COURSES:
        return True
    return any(call[0] in INDEXED and not 0 <= call[3] < ATLAS_ENTRIES for call in observation["calls"])


def visible(call: list[int]) -> bool:
    """_renderTextureFadeIn draws nothing at a brightness of zero or less; native skips that call."""
    return not (call[0] == FADE and call[4] <= 0)


def check_original(_image, value: dict, observation: dict, native: list[int], _reference: dict) -> list[str]:
    if len(native) < 2:
        return [f"invalid native protocol {native[:8]}"]
    status, count = native[0], native[1]
    calls_end = 2 + count * CALL_WORDS
    if len(native) < calls_end + CLUT_WORDS + 1:
        return [f"invalid native protocol {native[:8]}"]
    calls = [native[2 + i * CALL_WORDS:2 + (i + 1) * CALL_WORDS] for i in range(count)]
    clut = native[calls_end:calls_end + CLUT_WORDS]
    upload_count = native[calls_end + CLUT_WORDS]
    rest = native[calls_end + CLUT_WORDS + 1:]
    if len(rest) != upload_count * UPLOAD_WORDS:
        return [f"invalid native upload protocol ({upload_count} uploads, {len(rest)} words)"]
    uploads = [rest[i * UPLOAD_WORDS:(i + 1) * UPLOAD_WORDS] for i in range(upload_count)]
    if observation["skipped"]:
        return [] if status == 2 and not calls and not uploads else [f"native drew a refused vector {native[:8]}"]
    problems = []
    original_calls = observation["calls"]
    if value["operation"] == 5:
        if status != 0:
            problems.append(f"pulse status {status}")
        if clut != observation["clut"]:
            first = next(i for i, (a, b) in enumerate(zip(clut, observation["clut"], strict=True)) if a != b)
            problems.append(f"clutBuf word {first} native {clut[first]:#06x} original "
                            f"{observation['clut'][first]:#06x}")
        if uploads != observation["uploads"]:
            problems.append(f"uploads differ: native {[u[:4] for u in uploads]} original "
                            f"{[u[:4] for u in observation['uploads']]}")
    elif expects_refusal(value, observation):
        if status != 2:
            problems.append(f"status native {status}, expected the refusal 2")
        if calls:
            problems.append("native drew on a refusal")
        original_calls = []
    else:
        original_calls = [call for call in original_calls if visible(call)]
        expected_status = 0 if original_calls else 1
        if status != expected_status:
            problems.append(f"status native {status} original {expected_status}")
    if value["operation"] != 5 and (uploads or clut != value["clut"]):
        problems.append("native touched the palette while drawing")
    problems += call_difference(calls, original_calls)

    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK), HEADING_COLORS]
    if value["operation"] == 5:
        allowed.append(PULSE_TARGET)
    return problems + store_problems(observation["stores"], allowed)


def build_case(identifier: str) -> dict:
    operation, entry, end, name = SPECS[identifier]

    def vector(rng: random.Random, number: int) -> dict:
        return vector_for(identifier, rng, number)

    def describe(image, results) -> list[str]:
        verify_edits(EDITS[identifier], image)
        refused = [(v, o) for v, o, _ in results if operation != 5 and expects_refusal(v, o)]
        executed = [(v, o) for v, o, _ in results if not o["skipped"]]
        lines = [
            f"MENUF.PRG {name}: executed {(end - entry) // 4} original instructions including the return delay "
            "slot, the draw callees and LoadImage stubbed at the call boundary.",
        ]
        if operation == 5:
            weights = sorted({v["weight"] for v, _ in executed if 0 <= v["weight"] <= 16})
            lines.append("Compared all 160 clutBuf words afterwards, every LoadImage RECT and the 160 halfwords it "
                         "uploads, and every original store.")
            lines.append(f"{len(executed)} pulses, blend weights 0-16 seen: {len(weights)}, "
                         f"{sum(not 0 <= v['weight'] <= 16 for v, _ in executed)} outside 0-16 (32-bit wrap).")
            return lines
        hidden = [c for _, o in executed for c in o["calls"] if not visible(c)]
        drawn = sum(any(visible(c) for c in o["calls"]) for v, o in executed if not expects_refusal(v, o))
        lines += [
            "Compared every call in order (kind, origin, texture index, brightness or timer, palette, time, record "
            "flag, both CVECTORs behind the color pointer) and every original store.",
            f"{len(refused)} vectors are documented refusals (a glyph outside the 127 _disMap entries or a course "
            "past the eight trial-score rows); native must refuse exactly those without drawing.",
            f"{drawn} vectors drew, {len(results) - len(refused) - drawn} drew nothing; {len(hidden)} original "
            "fade-in calls at a brightness of zero or less were documented no-ops.",
        ]
        if operation in (1, 2, 3):
            courses = sorted({v["course"] for v, o in executed if o["calls"] and not expects_refusal(v, o)})
            lines.append(f"Drawn courses: {courses}.")
        return lines

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "MODULES": (MODULE,),
        "DRIVER": "timeattack_heading_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\Results\TimeAttackHeading.cpp",),
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
