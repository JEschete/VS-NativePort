"""Shared R3000 vectors and native driver protocol for B50-09 cube-puzzle text, rank and time drawing (MENUF.PRG
3B8.c:2610-2771).

The original side runs each function's own instructions with its draw callees (_renderTextureFadeIn,
_renderTexturePopIn, _renderTime) stubbed and recorded at the call boundary: the origin, the texture index and
brightness, both CVECTORs the color pointer reaches, or the time readout's timer, packed time and new-record flag. The
native driver replays the same atlas, par time and puzzle clock through Menu::Results and a recording
CubePuzzleTextHost.
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
from differentials.gameclear_score_common import place_atlas
from differentials.results_drawing_common import atlas_entry, coordinate

MODULE = "MENU/MENUF.PRG"
MODULE_BASES[MODULE] = 0x80102800

ATLAS = 0x801091D8  # _disMap
ATLAS_ENTRIES = 127
FIRST_RANK = 107
PAR_TIME = 0x801099F4  # _puzzleParTime
STATE_FLAGS = 0x80061598  # vs_main_stateFlags
CLOCK = STATE_FLAGS + 0xA0  # puzzleTimerHundredths, puzzleTimerSeconds, puzzleTimerMins
COLORS = (0x801097CC, 0x801097EC)  # the three functions' static CVECTOR arrays

FADE_IN = 0x80105C34  # _renderTextureFadeIn
POP_IN = 0x80106A80  # _renderTexturePopIn
TIME = 0x80105F6C  # _renderTime

# Recorded call kinds, as the driver prints them.
FADE, FLASH, KIND_TIME = range(3)
CALL_WORDS = 14  # kind, x, y, index or timer, brightness or time, eight color bytes, new-record flag

# operation, entry, end (one past the return delay slot), name.
SPECS = {
    "F03052": (0, 0x801084F4, 0x80108564, "_renderGivingUp (3B8.c:2610)"),
    "F03053": (1, 0x80108564, 0x801085D4, "_renderChicken (3B8.c:2624)"),
    "F03054": (2, 0x801085D4, 0x80108688, "_renderEvolveOrDie (3B8.c:2640)"),
    "F03055": (3, 0x80108688, 0x8010873C, "_renderFoodchain (3B8.c:2664)"),
    "F03056": (4, 0x8010873C, 0x80108784, "_renderAverageTimeHeader (3B8.c:2687)"),
    "F03057": (5, 0x80108784, 0x8010880C, "_renderCubePuzzleTime (3B8.c:2699)"),
    "F03058": (6, 0x8010880C, 0x8010887C, "_renderCubePuzzleRank (3B8.c:2712)"),
    "F03059": (7, 0x8010887C, 0x80108A0C, "_renderCubePuzzleIncrementalTime (3B8.c:2732)"),
}

# name: (address, original word, mutated word). The original words are checked against MENUF.PRG in describe();
# MUTANTS stays static so case discovery and --list never read a disc image.
EDITS = {
    "F03052": {
        "F03052-clamps-from-48": (0x80108514, 0x2A020041, 0x2A020030),  # slti v0,s0,0x41
        "F03052-clamps-to-63": (0x80108520, 0x24100040, 0x2410003F),
        "F03052-draws-at-zero": (0x80108524, 0x1A00000A, 0x0600000A),  # blez -> bltz
        "F03052-first-half-entry-124": (0x80108530, 0x2406007B, 0x2406007C),
        "F03052-second-half-x-161": (0x8010853C, 0x240400A0, 0x240400A1),
        "F03052-second-half-entry-125": (0x80108544, 0x2406007C, 0x2406007D),
    },
    "F03053": {
        "F03053-clamps-from-48": (0x80108584, 0x2A020041, 0x2A020030),
        "F03053-clamps-to-63": (0x80108590, 0x24100040, 0x2410003F),
        "F03053-draws-at-zero": (0x80108594, 0x1A00000A, 0x0600000A),
        "F03053-first-half-entry-126": (0x801085A0, 0x2406007D, 0x2406007E),
        "F03053-second-half-x-161": (0x801085AC, 0x240400A0, 0x240400A1),
        "F03053-second-half-entry-124": (0x801085B4, 0x2406007E, 0x2406007C),
    },
    "F03054": {
        "F03054-clamps-from-48": (0x80108600, 0x28820041, 0x28820030),
        "F03054-clamps-to-63": (0x8010860C, 0x24040040, 0x2404003F),
        "F03054-first-entry-103": (0x80108618, 0x24060066, 0x24060067),
        "F03054-measures-first-twice": (0x80108630, 0x9203033A, 0x92030332),
        "F03054-light-step-unset": (0x8010863C, 0xA2240007, 0x00000000),
        "F03054-centres-by-quarter": (0x80108644, 0x00021043, 0x00021083),
        "F03054-second-half-first-color": (0x80108660, 0x26270004, 0x26270000),
    },
    "F03055": {
        "F03055-clamps-from-48": (0x801086B4, 0x28820041, 0x28820030),
        "F03055-clamps-to-63": (0x801086C0, 0x24040040, 0x2404003F),
        "F03055-first-entry-106": (0x801086CC, 0x24060069, 0x2406006A),
        "F03055-light-step-unset": (0x801086F0, 0xA2240007, 0x00000000),
        "F03055-centres-by-quarter": (0x801086F8, 0x00021043, 0x00021083),
        "F03055-second-half-after-right": (0x80108710, 0x9204034A, 0x92040352),
        "F03055-second-half-first-color": (0x80108714, 0x26270004, 0x26270000),
    },
    "F03056": {
        "F03056-clamps-from-48": (0x80108748, 0x28E20041, 0x28E20030),
        "F03056-clamps-to-63": (0x80108754, 0x24070040, 0x2407003F),
        "F03056-draws-at-zero": (0x80108758, 0x18E00006, 0x04E00006),
        "F03056-heading-entry-105": (0x80108764, 0x24060068, 0x24060069),
        "F03056-centres-by-quarter": (0x80108768, 0x00042042, 0x00042082),
        "F03056-centres-right": (0x80108770, 0x00642023, 0x00642021),
    },
    "F03057": {
        "F03057-clamps-from-48": (0x8010878C, 0x28C20041, 0x28C20030),
        "F03057-clamps-to-63": (0x80108798, 0x24060040, 0x2406003F),
        "F03057-record-flag-x": (0x801087BC, 0xAFA00010, 0xAFA80010),
        "F03057-minutes-by-30": (0x801087D0, 0x00073943, 0x00073903),
        "F03057-seconds-modulo-120": (0x801087E0, 0x00021080, 0x000210C0),
        "F03057-minutes-in-seconds-byte": (0x801087EC, 0x00073C00, 0x00073A00),
        "F03057-seconds-zero-extended": (0x801087F0, 0x00031A03, 0x00031A02),
    },
    "F03058": {
        "F03058-clamps-from-48": (0x80108824, 0x28C20041, 0x28C20030),
        "F03058-clamps-to-63": (0x80108830, 0x24060040, 0x2406003F),
        "F03058-step-unset": (0x80108848, 0xA0E60003, 0x00000000),
        "F03058-title-base-108": (0x8010884C, 0x2486006B, 0x2486006C),
        "F03058-centres-by-quarter": (0x80108860, 0x00042042, 0x00042082),
        "F03058-centres-right": (0x80108868, 0x01042023, 0x01042021),
    },
    "F03059": {
        "F03059-clamps-from-48": (0x801088A4, 0x2A020041, 0x2A020030),
        "F03059-clamps-to-63": (0x801088B0, 0x24100040, 0x2410003F),
        "F03059-label-entry-73": (0x801088C0, 0x24060048, 0x24060049),
        # Counting from 17 instead would be equivalent: step 16 scales the clock by zero.
        "F03059-counts-from-15": (0x801088DC, 0x2A020010, 0x2A02000F),
        "F03059-hundredths-from-seconds": (0x801088F4, 0x908400A0, 0x908400A1),
        "F03059-scales-from-17": (0x80108930, 0x2602FFF0, 0x2602FFEF),
        "F03059-divides-by-96": (0x80108958, 0x000318C3, 0x00031903),
        "F03059-hundredths-not-reduced": (0x801089C4, 0x00E23823, 0x00E23821),
        "F03059-early-time-one": (0x801089DC, 0x00003821, 0x24070001),
        "F03059-time-drop-3": (0x801089E8, 0x26450002, 0x26450003),
    },
}


def mutants(identifier: str) -> dict:
    """Each named edit patched alone over the original instructions."""
    patched = {}
    for name, edit in EDITS[identifier].items():
        patched[name] = {edit[0]: edit[2]}
    return patched


def unexpected_words(identifier: str, image) -> list[str]:
    """Edits whose original word is not the one MENUF.PRG holds."""
    return [f"{name}: {edit[0]:#x} holds {image.read(edit[0]):#010x}, expected {edit[1]:#010x}"
            for name, edit in EDITS[identifier].items() if image.read(edit[0]) != edit[1]]


# ---------------------------------------------------------------------------------------------- vectors
EDGE_TIMERS = (-0x80000000, -1, 0, 1, 2, 7, 8, 9, 15, 16, 17, 40, 63, 64, 65, 0x7FFFFFFF)
EDGE_PARS = (-0x8000, -61, -60, -59, -1, 0, 1, 59, 60, 61, 119, 120, 3599, 3600, 0x7FFF)
EDGE_RANKS = (-0x80000000, -108, -107, -1, 16, 19, 20, 21, 0x7FFFFFFF)


def timer(rng: random.Random, number: int) -> int:
    """Edge steps every fourth vector, an arbitrary word every 32nd, otherwise a step around the reveal."""
    if number % 4 == 0:
        return EDGE_TIMERS[(number >> 2) % len(EDGE_TIMERS)]
    return s32(rng.getrandbits(32)) if number % 32 == 3 else rng.randrange(-3, 70)


def par_time(rng: random.Random, number: int) -> int:
    pattern = number % 3
    if pattern == 0:
        return rng.choice(EDGE_PARS)
    if pattern == 1:
        return rng.randrange(-0x8000, 0x8000)
    return rng.randrange(0, 600)


def clock(rng: random.Random, number: int) -> list[int]:
    """puzzleTimerMins, puzzleTimerSeconds, puzzleTimerHundredths."""
    if number % 5 == 0:
        return [rng.getrandbits(8) for _ in range(3)]
    if number % 7 == 1:
        return [rng.choice((0, 255)) for _ in range(3)]
    return [rng.randrange(10), rng.randrange(60), rng.randrange(100)]


def rank_value(rng: random.Random, number: int) -> int:
    if number % 11 == 5:
        return rng.choice(EDGE_RANKS + (s32(rng.getrandbits(32)), rng.randrange(-107, 0)))
    return rng.randrange(16)


def vector_for(identifier: str, rng: random.Random, number: int) -> dict:
    return {
        "operation": SPECS[identifier][0],
        "x": coordinate(rng),
        "y": coordinate(rng),
        "timer": timer(rng, number),
        "par": par_time(rng, number),
        "rank": rank_value(rng, number),
        "clock": clock(rng, number),
        # None keeps the module's own _disMap.
        "atlas": None if number % 2 == 0 else [atlas_entry(rng) for _ in range(ATLAS_ENTRIES)],
    }


def clamped(value: dict) -> int:
    return min(max(value["timer"], 0), 64)


def title_outside(value: dict) -> bool:
    """The rank title _renderCubePuzzleRank would read from outside _disMap."""
    return value["operation"] == 6 and clamped(value) > 0 and not 0 <= s32(value["rank"] + FIRST_RANK) < ATLAS_ENTRIES


def run_original(machine: Machine, value: dict) -> tuple[dict, list[int]]:
    operation = value["operation"]
    entry = next(spec[1] for spec in SPECS.values() if spec[0] == operation)
    atlas = place_atlas(machine, value["atlas"])
    minutes, seconds, hundredths = value["clock"]
    extra = value["par"] if operation == 5 else value["rank"]
    inputs = [operation, value["x"], value["y"], value["timer"], extra, minutes, seconds, hundredths,
              *(field for entry_fields in atlas for field in entry_fields)]
    if title_outside(value):
        return {"skipped": True}, inputs
    machine.put(PAR_TIME, value["par"] & 0xFFFF, 2)
    machine.store(CLOCK, bytes((hundredths, seconds, minutes)))
    calls: list[list[int]] = []

    def fade(m: Machine) -> None:
        calls.append([FADE, s32(m.r[4]), s32(m.r[5]), s32(m.r[6]), s32(m.r[7])] + [0] * 9)

    def flash(m: Machine) -> None:
        calls.append([FLASH, s32(m.r[4]), s32(m.r[5]), s32(m.r[6]), 0, *m.bytes(m.r[7], 8), 0])

    def time(m: Machine) -> None:
        calls.append([KIND_TIME, s32(m.r[4]), s32(m.r[5]), s32(m.r[6]), s32(m.r[7])] + [0] * 8
                     + [s32(m.load(m.r[29] + 0x10))])

    machine.stub(FADE_IN, fade)
    machine.stub(POP_IN, flash)
    machine.stub(TIME, time)
    if operation in (0, 1):
        machine.r[4] = value["y"] & MASK
        machine.r[5] = value["timer"] & MASK
    else:
        machine.r[4] = value["x"] & MASK
        machine.r[5] = value["y"] & MASK
        machine.r[6] = value["timer"] & MASK
        machine.r[7] = value["rank"] & MASK
    machine.execute(entry)
    return {"skipped": False, "calls": calls, "stores": list(machine.stores)}, inputs


def check_original(_image, _value: dict, observation: dict, native: list[int], _reference: dict) -> list[str]:
    if len(native) < 2 or len(native) != 2 + native[1] * CALL_WORDS:
        return [f"invalid native protocol {native[:8]}"]
    status = native[0]
    calls = [native[2 + i * CALL_WORDS:2 + (i + 1) * CALL_WORDS] for i in range(native[1])]
    if observation["skipped"]:
        return [] if status == 2 and not calls else [f"native drew a refused rank title {native[:8]}"]
    problems = []
    original_calls = observation["calls"]
    expected_status = 0 if original_calls else 1
    if status != expected_status:
        problems.append(f"status native {status} original {expected_status}")
    if len(calls) != len(original_calls):
        problems.append(f"{len(calls)} native calls, {len(original_calls)} original")
    problems.extend(f"call {number}: native {mine} original {theirs}"
                    for number, (mine, theirs) in enumerate(zip(calls, original_calls, strict=False))
                    if mine != theirs)
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK), COLORS]
    return problems + store_problems(observation["stores"], allowed)


def build_case(identifier: str) -> dict:
    operation, entry, end, name = SPECS[identifier]

    def vector(rng: random.Random, number: int) -> dict:
        return vector_for(identifier, rng, number)

    def describe(image, results) -> list[str]:
        if stale := unexpected_words(identifier, image):
            raise SystemExit("; ".join(stale))
        executed = [(v, o) for v, o, _ in results if not o["skipped"]]
        drawn = sum(bool(o["calls"]) for _, o in executed)
        lines = [
            f"MENUF.PRG {name}: executed {(end - entry) // 4} original instructions including the return delay "
            "slot, with _renderTextureFadeIn, _renderTexturePopIn and _renderTime stubbed at the call boundary.",
            "Compared every call in order (kind, origin, texture index and brightness or both CVECTORs behind the "
            "color pointer; time readout timer, packed time and new-record flag), the status and every original "
            "store.",
            f"{len(results) - len(executed)} vectors are documented refusals (a rank title outside the 127 _disMap "
            "entries); native must refuse exactly those.",
            f"{drawn} vectors drew, {len(executed) - drawn} drew nothing.",
        ]
        if operation == 5:
            negative = sum(v["par"] < 0 and clamped(v) > 0 for v, _ in executed)
            lines.append(f"{negative} drawn vectors had a negative par time.")
        if operation == 7:
            counting = sum(16 <= clamped(v) < 64 for v, _ in executed)
            lines.append(f"{counting} vectors counting, {sum(clamped(v) == 64 for v, _ in executed)} at the final "
                         f"step, {sum(0 < clamped(v) < 16 for v, _ in executed)} before counting starts.")
        if operation == 6:
            odd = sum(not 0 <= v["rank"] < 16 and clamped(v) > 0 for v, _ in executed)
            lines.append(f"{odd} drawn vectors had a rank outside 0-15 whose title is still inside _disMap.")
        return lines

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "MODULES": (MODULE, "SLUS_010.40"),
        "DRIVER": "cube_text_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\Results\CubePuzzleText.cpp",),
        "CXX_STANDARD": "c++latest",
        "VECTORS": 1024,
        "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 512,
        "MUTANTS": mutants(identifier),
        "vector": vector,
        "run": run_original,
        "check": check_original,
        "describe": describe,
    }
