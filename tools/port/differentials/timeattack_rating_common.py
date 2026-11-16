"""Shared R3000 vectors and native driver protocol for B50-07 time-attack results rating, time and leaderboard
(MENUF.PRG 3B8.c:1654-1874).

The original side runs each function's own instructions with its callees (_renderTexturePopIn,
_renderTextureFadeIn, _renderTextureFadeInClut, _renderTime, _renderTextureWipe) stubbed and recorded at the call
boundary: the origin, the texture index, the brightness or timer, the palette, time, record flag and wipe edge
arguments and both CVECTORs behind the color pointer. The completed time (_timeTrialTime) and the new record's rank
(_newTimeSlot) are written into the overlay; the time-attack course (vs_main_stateFlags.timeAttackBoss) and the
trial scores live outside MENUF.PRG and are placed in the machine's main memory. The native driver replays the same
atlas, time, rank, course and scores through Menu::Results with a recording TimeAttackResultsDrawHost.
"""

from __future__ import annotations

import functools
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
from differentials.timeattack_heading_common import COURSE, SCORE_ROW, SCORES, score

MODULE = "MENU/MENUF.PRG"
MODULE_BASES[MODULE] = 0x80102800

ATLAS_ENTRIES = 127
HEADING_COLORS = (0x80109774, 0x8010977C)  # _renderTimeAttackResultsHeader's static CVECTORs
NEW_TIME_SLOT = 0x801098A4  # _newTimeSlot
TIME_TRIAL_TIME = 0x801098A8  # _timeTrialTime
COURSES = 8
RANKS = 3

POP_IN = 0x80106A80  # _renderTexturePopIn
FADE_IN = 0x80105C34  # _renderTextureFadeIn
FADE_IN_CLUT = 0x80105DD8  # _renderTextureFadeInClut
RENDER_TIME = 0x80105F6C  # _renderTime
WIPE = 0x80107140  # _renderTextureWipe

# Recorded call kinds, as the driver prints them.
FLASH, FADE, FADE_CLUT, TIME, KIND_WIPE = range(5)
# kind, x, y, index, brightness/timer, palette, time, record, wipe edge, eight color bytes
CALL_WORDS = 17
INDEXED = (FLASH, FADE, FADE_CLUT, KIND_WIPE)

# operation, entry, end (one past the return delay slot), name.
SPECS = {
    "F03032": (0, 0x8010559C, 0x8010564C, "_renderTimeAttackResultsHeader (3B8.c:1654)"),
    "F03033": (1, 0x8010564C, 0x801056E8, "_renderTimeAttackRating (3B8.c:1678)"),
    "F03034": (2, 0x801056E8, 0x80105790, "_renderTimeAttackRatingWipe (3B8.c:1708)"),
    "F03035": (3, 0x80105790, 0x8010581C, "_renderHeaderAndTime (3B8.c:1743)"),
    "F03036": (4, 0x8010581C, 0x801059B8, "_renderHeaderAndIncrementalTime (3B8.c:1762)"),
    "F03037": (5, 0x801059B8, 0x801059FC, "_renderNewRecord (3B8.c:1801)"),
    "F03038": (6, 0x801059FC, 0x80105B30, "_renderAllStoredTimes (3B8.c:1816)"),
    "F03039": (7, 0x80105B30, 0x80105C34, "_renderStoredTime (3B8.c:1849)"),
}

# name: (address, original word, mutated word). The original words are checked against MENUF.PRG in describe();
# MUTANTS stays static so case discovery and --list never read a disc image.
EDITS = {
    "F03032": {
        "F03032-clamps-from-48": (0x801055C8, 0x28820041, 0x28820031),
        "F03032-clamps-to-63": (0x801055D4, 0x24040040, 0x2404003F),
        "F03032-heading-entry-71": (0x801055E0, 0x24060046, 0x24060047),
        "F03032-step-unset": (0x80105600, 0xA2240003, 0x00000000),
        "F03032-centres-by-quarter": (0x80105608, 0x00021043, 0x00021083),
        "F03032-second-entry-72": (0x8010561C, 0x24060047, 0x24060048),
        "F03032-second-half-second-color": (0x80105624, 0x02203821, 0x26270004),
    },
    "F03033": {
        "F03033-clamps-from-48": (0x80105660, 0x28E20041, 0x28E20031),
        "F03033-clamps-to-63": (0x8010566C, 0x24070040, 0x2407003F),
        "F03033-excellent-below-1e01": (0x80105680, 0x28621E00, 0x28621E01),
        "F03033-second-at-most-fffe": (0x8010568C, 0x3402FFFF, 0x3402FFFE),
        "F03033-third-at-most-11dfe": (0x801056A4, 0x34421DFF, 0x34421DFE),
        "F03033-fourth-at-most-1fffe": (0x801056BC, 0x3442FFFF, 0x3442FFFE),
        "F03033-first-rating-79": (0x801056D4, 0x24C6004E, 0x24C6004F),
    },
    "F03034": {
        "F03034-clamps-to-63": (0x80105704, 0x24060040, 0x2406003F),
        "F03034-wipe-stride-4": (0x8010570C, 0x000610C0, 0x00061080),
        "F03034-excellent-below-1e01": (0x8010571C, 0x28621E00, 0x28621E01),
        "F03034-second-at-most-fffe": (0x80105728, 0x3402FFFF, 0x3402FFFE),
        "F03034-third-at-most-11dfe": (0x80105740, 0x34421DFF, 0x34421DFE),
        "F03034-fourth-at-most-1fffe": (0x80105758, 0x3442FFFF, 0x3442FFFE),
        "F03034-first-rating-79": (0x80105774, 0x24C6004E, 0x24C6004F),
    },
    "F03035": {
        "F03035-clamps-from-48": (0x801057B8, 0x2A020041, 0x2A020031),
        "F03035-clamps-to-63": (0x801057C4, 0x24100040, 0x2410003F),
        "F03035-label-entry-73": (0x801057D4, 0x24060048, 0x24060049),
        "F03035-drop-3": (0x801057E0, 0x26450002, 0x26450003),
        "F03035-label-width-of-73": (0x801057F0, 0x9044941A, 0x90449422),
        "F03035-record-palette": (0x801057F8, 0xAFA00010, 0xAFB00010),  # sw s0 (the timer)
    },
    "F03036": {
        "F03036-clamps-to-63": (0x80105850, 0x24100040, 0x2410003F),
        "F03036-label-entry-73": (0x80105860, 0x24060048, 0x24060049),
        "F03036-counts-from-15": (0x8010587C, 0x2A020010, 0x2A02000F),
        "F03036-hundredths-7-bits": (0x801058A0, 0x308400FF, 0x3084007F),
        "F03036-seconds-of-92": (0x801058CC, 0x00451021, 0x00451023),
        "F03036-delay-15": (0x801058DC, 0x2602FFF0, 0x2602FFF1),
        "F03036-over-24-steps": (0x80105904, 0x000318C3, 0x00031883),
        "F03036-hundredths-unreduced": (0x80105970, 0x00E23823, 0x00000000),
        "F03036-record-palette": (0x8010598C, 0xAFA00010, 0xAFB00010),
    },
    "F03037": {
        "F03037-clamps-from-48": (0x801059CC, 0x28E20041, 0x28E20031),
        "F03037-clamps-to-63": (0x801059D8, 0x24070040, 0x2407003F),
        "F03037-label-entry-70": (0x801059E8, 0x24060045, 0x24060046),
    },
    "F03038": {
        "F03038-clamps-to-63": (0x80105A44, 0x24120040, 0x2412003F),
        "F03038-label-width-of-next": (0x80105A60, 0x24140258, 0x24140260),
        "F03038-palette-7ff3": (0x80105A70, 0x24137FF2, 0x24137FF3),
        "F03038-record-palette-7ff9": (0x80105A84, 0x24137FF8, 0x24137FF9),
        "F03038-time-mask-drops-low-bits": (0x80105A8C, 0x3508FFFF, 0x3508FFF0),
        "F03038-time-inset-83": (0x80105A90, 0x26A4FFAC, 0x26A4FFAD),
        "F03038-next-place-column": (0x80105AB0, 0x8C470028, 0x8C47002C),
        "F03038-label-entry-76": (0x80105AC8, 0x2606004B, 0x2606004C),
        "F03038-row-spacing-21": (0x80105AD8, 0x26310014, 0x26310015),
        "F03038-label-gap-89": (0x80105AF0, 0x2484FFA8, 0x2484FFA7),
        "F03038-two-rows": (0x80105AF4, 0x2A020003, 0x2A020002),
    },
    "F03039": {
        "F03039-clamps-to-63": (0x80105B6C, 0x24110040, 0x2411003F),
        "F03039-palette-7ff3": (0x80105B84, 0x24137FF2, 0x24137FF3),
        "F03039-record-palette-7ff9": (0x80105B88, 0x24137FF8, 0x24137FF9),
        "F03039-time-mask-drops-low-bits": (0x80105B90, 0x3508FFFF, 0x3508FFF0),
        "F03039-time-inset-83": (0x80105B94, 0x2684FFAC, 0x2684FFAD),
        "F03039-row-spacing-40": (0x80105BA0, 0x00108080, 0x001080C0),
        "F03039-drop-3": (0x80105BA8, 0x26050002, 0x26050003),
        "F03039-course-row-of-4": (0x80105BC0, 0x00471021, 0x00471023),
        "F03039-label-entry-76": (0x80105BE8, 0x2646004B, 0x2646004C),
        "F03039-label-gap-89": (0x80105C10, 0x2484FFA8, 0x2484FFA7),
    },
}


# ---------------------------------------------------------------------------------------------- vectors
EDGE_TIMERS = (-0x80000000, -1, 0, 1, 2, 15, 16, 17, 31, 32, 48, 63, 64, 65, 66, 0x7FFFFFFF)
EDGE_TIMES = (-0x80000000, -1, 0, 0x1DFF, 0x1E00, 0x1E01, 0xFFFF, 0x10000, 0x11DFF, 0x11E00, 0x1FFFF, 0x20000,
              0x3B3B63, 0x800000, 0xFFFFFF, 0xFFFFFF00, 0x7FFFFFFF, 0x00FF00FF, 0x00636363)
EDGE_SLOTS = (-0x80000000, -1, 0, 1, 2, 3, 4, 0x7FFFFFFF)
EDGE_COURSES = (0, 6, 7, 8, 9, 127, 255)


def timer(rng: random.Random, number: int) -> int:
    if number % 4 == 0:
        return EDGE_TIMERS[number // 4 % len(EDGE_TIMERS)]
    if number % 32 == 3:
        return s32(rng.getrandbits(32))
    return rng.randrange(-3, 70)


def time(rng: random.Random, number: int) -> int:
    pattern = number % 5
    if pattern == 0:
        return EDGE_TIMES[number // 5 % len(EDGE_TIMES)]
    if pattern == 1:
        return rng.randrange(4) << 16 | rng.randrange(60) << 8 | rng.randrange(100)
    if pattern == 2:
        return rng.getrandbits(24)
    if pattern == 3:
        return s32(rng.getrandbits(32))
    return rng.randrange(60) << 16 | rng.randrange(60) << 8 | rng.randrange(100)


def slot(rng: random.Random, number: int) -> int:
    if number % 9 == 0:
        return EDGE_SLOTS[number // 9 % len(EDGE_SLOTS)]
    if number % 37 == 5:
        return s32(rng.getrandbits(32))
    return rng.randrange(RANKS)


def new_slot(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.75:
        return rng.randrange(-1, 4)
    return rng.choice((-0x80000000, 0x7FFFFFFF, 4, 0x100)) if roll < 0.9 else s32(rng.getrandbits(32))


def course(rng: random.Random, number: int) -> int:
    if number % 7 == 0:
        return EDGE_COURSES[number // 7 % len(EDGE_COURSES)]
    if number % 23 == 5:
        return rng.randrange(256)
    return rng.randrange(COURSES)


def trial_score(rng: random.Random) -> int:
    if rng.random() < 0.15:
        # No time recorded: the dashes sentinel with any round and difficulty.
        return (rng.getrandbits(8) << 24) | 0x800000
    return score(rng)


def vector_for(identifier: str, rng: random.Random, number: int) -> dict:
    return {
        "operation": SPECS[identifier][0],
        "x": coordinate(rng),
        "y": coordinate(rng),
        "timer": timer(rng, number),
        "slot": slot(rng, number),
        "time": time(rng, number),
        "new_slot": new_slot(rng),
        "course": course(rng, number),
        "scores": [[trial_score(rng) for _ in range(RANKS)] for _ in range(COURSES)],
        # None keeps the module's own _disMap.
        "atlas": None if number % 2 == 0 else [atlas_entry(rng) for _ in range(ATLAS_ENTRIES)],
    }


def clamped(value: dict) -> int:
    return max(0, min(64, value["timer"]))


def run_original(machine: Machine, value: dict) -> tuple[dict, list[int]]:
    operation = value["operation"]
    entry = next(spec[1] for spec in SPECS.values() if spec[0] == operation)
    atlas = place_atlas(machine, value["atlas"])
    inputs = [operation, value["x"], value["y"], value["timer"], value["slot"], value["time"], value["new_slot"],
              value["course"], *(word for row in value["scores"] for word in row),
              *(field for entry_fields in atlas for field in entry_fields)]
    machine.put(TIME_TRIAL_TIME, value["time"] & MASK)
    machine.put(NEW_TIME_SLOT, value["new_slot"] & MASK)
    machine.put(COURSE, value["course"], 1)
    for row, words in enumerate(value["scores"]):
        for column, word in enumerate(words):
            machine.put(SCORES + SCORE_ROW * row + 4 * column, word)
    calls: list[list[int]] = []

    def draw(kind: int):
        def record(m: Machine) -> None:
            call = [kind, s32(m.r[4]), s32(m.r[5])] + [0] * (CALL_WORDS - 3)
            if kind == TIME:
                call[4], call[6], call[7] = s32(m.r[6]), s32(m.r[7]), s32(m.load(m.r[29] + 0x10))
                calls.append(call)
                return
            call[3] = s32(m.r[6])
            if kind in (FLASH, KIND_WIPE):
                call[9:17] = list(m.bytes(m.r[7], 8))
                if kind == KIND_WIPE:
                    call[8] = s32(m.load(m.r[29] + 0x10))
            else:
                call[4] = s32(m.r[7])
                if kind == FADE_CLUT:
                    call[5] = s32(m.load(m.r[29] + 0x10))
            calls.append(call)
        return record

    machine.stub(POP_IN, draw(FLASH))
    machine.stub(FADE_IN, draw(FADE))
    machine.stub(FADE_IN_CLUT, draw(FADE_CLUT))
    machine.stub(RENDER_TIME, draw(TIME))
    machine.stub(WIPE, draw(KIND_WIPE))
    machine.r[4] = value["x"] & MASK
    machine.r[5] = value["y"] & MASK
    if operation == 7:
        machine.r[6] = value["slot"] & MASK
        machine.r[7] = value["timer"] & MASK
    else:
        machine.r[6] = value["timer"] & MASK
    try:
        machine.execute(entry)
    except Fault:
        # Only a leaderboard slot far outside the three ranks reads unmapped memory.
        return {"skipped": True}, inputs
    return {"skipped": False, "calls": calls, "stores": list(machine.stores)}, inputs


def visible(call: list[int]) -> bool:
    """_renderTextureFadeIn, _renderTextureFadeInClut and _renderTime draw nothing at a brightness or timer of zero
    or less; native skips those calls."""
    return not (call[0] in (FADE, FADE_CLUT, TIME) and call[4] <= 0)


def expects_refusal(value: dict) -> bool:
    """Native refuses a revealed leaderboard row whose course lies past the eight trial-score rows or whose slot lies
    outside the three ranks; the original reads a neighbouring row and draws a rating or other entry as a label."""
    if clamped(value) <= 0:
        return False
    if value["operation"] == 6:
        return value["course"] >= COURSES
    if value["operation"] == 7:
        return value["course"] >= COURSES or not 0 <= value["slot"] < RANKS
    return False


def check_original(_image, value: dict, observation: dict, native: list[int], _reference: dict) -> list[str]:
    if len(native) < 2 or len(native) != 2 + native[1] * CALL_WORDS:
        return [f"invalid native protocol {native[:8]}"]
    status, count = native[0], native[1]
    calls = [native[2 + i * CALL_WORDS:2 + (i + 1) * CALL_WORDS] for i in range(count)]
    hidden = clamped(value) <= 0
    if observation["skipped"]:
        expected = 1 if hidden else 2
        if not expects_refusal(value) and not hidden:
            return ["the original faulted on a vector native does not refuse"]
        return [] if status == expected and not calls else [f"native status {status} on a faulting vector"]
    problems = []
    if expects_refusal(value):
        if status != 2:
            problems.append(f"status native {status}, expected the refusal 2")
        if calls:
            problems.append("native drew on a refusal")
        original_calls = []
    else:
        original_calls = [call for call in observation["calls"] if visible(call)]
        if any(call[0] in INDEXED and not 0 <= call[3] < ATLAS_ENTRIES for call in original_calls):
            problems.append("the original drew outside _disMap on a vector native accepts")
        expected_status = 0 if original_calls else 1
        if status != expected_status:
            problems.append(f"status native {status} original {expected_status}")
    problems += call_difference(calls, original_calls)
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK), HEADING_COLORS]
    return problems + store_problems(observation["stores"], allowed)


def build_case(identifier: str) -> dict:
    operation, entry, end, name = SPECS[identifier]

    def describe(image, results) -> list[str]:
        verify_edits(EDITS[identifier], image)
        executed = [(v, o) for v, o, _ in results if not o["skipped"]]
        refused = [v for v, o, _ in results if o["skipped"] or expects_refusal(v)]
        hidden = [c for _, o in executed for c in o["calls"] if not visible(c)]
        drawn = sum(any(visible(c) for c in o["calls"]) for v, o in executed if not expects_refusal(v))
        lines = [
            f"MENUF.PRG {name}: executed {(end - entry) // 4} original instructions including the return delay "
            "slot, the draw callees stubbed at the call boundary.",
            "Compared every call in order (kind, origin, texture index, brightness or timer, palette, time, record "
            "flag, wipe edge, both CVECTORs behind the color pointer) and every original store.",
            f"{len(refused)} vectors are documented refusals (a revealed leaderboard course past the eight "
            "trial-score rows or slot outside the three ranks); native must refuse exactly those without drawing.",
            f"{drawn} vectors drew, {len(results) - len(refused) - drawn} drew nothing; {len(hidden)} original "
            "fade-in or time calls at a brightness or timer of zero or less were documented no-ops.",
        ]
        if operation in (1, 2):
            ratings = sorted({c[3] for _, o in executed for c in o["calls"] if c[0] in INDEXED})
            lines.append(f"Ratings drawn (atlas entries): {ratings}.")
        if operation == 4:
            counting = sum(1 for v, o in executed if 16 < clamped(v) < 64 and o["calls"])
            lines.append(f"{counting} vectors drew a partly counted time (timer 17-63).")
        if operation in (6, 7):
            highlighted = sum(1 for _, o in executed for c in o["calls"] if c[0] == TIME and c[7] and visible(c))
            lines.append(f"{highlighted} visible ranked times drew in the new-record palette.")
        return lines

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "MODULES": (MODULE,),
        "DRIVER": "timeattack_rating_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\Results\TimeAttackRating.cpp",),
        "CXX_STANDARD": "c++latest",
        "VECTORS": 1024,
        "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 512,
        "MUTANTS": edit_mutants(EDITS[identifier]),
        "vector": functools.partial(vector_for, identifier),
        "run": run_original,
        "check": check_original,
        "describe": describe,
    }
