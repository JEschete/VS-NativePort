"""Shared R3000 vectors and native driver protocol for B50-04 game-clear score and rank drawing (MENUF.PRG
3B8.c:1332-1652).

The original side runs each function's own instructions with its draw callees (_renderTexturePopIn,
_renderTextureFadeInTint, _renderTextureWipe) and the sound calls (vs_main_playSfxDefault, func_80045D64) stubbed
and recorded at the call boundary: the texture index, the origin, both CVECTORs the color pointer reaches and the
wipe edge. sprintf is modelled for "%09d" and "%03d". The native driver replays the same atlas, rank table, results
values and counters through Menu::Results and a recording GameClearDrawHost.
"""

from __future__ import annotations

import random
import struct

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MASK,
    MODULE_BASES,
    Machine,
    s32,
    store_problems,
)
from differentials.results_drawing_common import atlas_entry, c_string, coordinate

MODULE = "MENU/MENUF.PRG"
MODULE_BASES[MODULE] = 0x80102800

ATLAS = 0x801091D8  # _disMap
ATLAS_ENTRIES = 127
RANKS = 0x801095D0  # _riskbreakerRanks
RANK_ROWS = 16
RANK = 0x8010987C
SCORE = 0x80109884
MAP_COMPLETION = 0x80109888
SHOWN_MAP = 0x80109894
SHOWN_SCORE = 0x80109898
COUNTING_SCORE = 0x801099EC
COUNTING_MAP = 0x801099F0
COLORS = (0x80109738, 0x80109774)  # the seven functions' static CVECTOR pairs
ARGUMENT_FIVE = DEFAULT_STACK + 0x10

POP_IN = 0x80106A80  # _renderTexturePopIn
TINT_FADE = 0x8010664C  # _renderTextureFadeInTint
WIPE = 0x80107140  # _renderTextureWipe
SPRINTF = 0x80026EF4
PLAY_SFX = 0x80045988  # vs_main_playSfxDefault
STOP_SFX = 0x80045D64

# Recorded call kinds, as the driver prints them.
TINT, FLASH, KIND_WIPE, PLAY, STOP = range(5)
CALL_WORDS = 13  # kind, x, y, index, eight color bytes, wipe edge
DRAWS = (TINT, FLASH, KIND_WIPE)

# operation, entry, end (one past the return delay slot), name.
SPECS = {
    "F03025": (0, 0x80104B8C, 0x80104C40, "_renderCongratulations (3B8.c:1332)"),
    "F03026": (1, 0x80104C40, 0x80104DBC, "_renderScore (3B8.c:1357)"),
    "F03027": (2, 0x80104DBC, 0x80105020, "_renderIncrementalScore (3B8.c:1409)"),
    "F03028": (3, 0x80105020, 0x8010516C, "_renderMapCompletion (3B8.c:1486)"),
    "F03029": (4, 0x8010516C, 0x801053B0, "_renderIncrementalMapCompletion (3B8.c:1527)"),
    "F03030": (5, 0x801053B0, 0x8010540C, "_renderRiskbreakerRankHeader (3B8.c:1598)"),
    "F03031": (6, 0x8010540C, 0x8010559C, "_renderRiskbreakerRank (3B8.c:1618)"),
}

# name: (address, original word, mutated word). The original words are checked against MENUF.PRG in describe();
# MUTANTS stays static so case discovery and --list never read a disc image.
EDITS = {
    "F03025": {
        "F03025-clamps-from-48": (0x80104BB8, 0x28820041, 0x28820030),  # slti v0,a0,0x41
        "F03025-clamps-to-63": (0x80104BC4, 0x24040040, 0x2404003F),
        "F03025-banner-entry-24": (0x80104BD0, 0x24060016, 0x24060018),
        "F03025-light-step-unset": (0x80104BF4, 0xA2240007, 0x00000000),
        "F03025-centres-by-quarter": (0x80104BFC, 0x00021043, 0x00021083),
        "F03025-second-half-first-color": (0x80104C18, 0x26270004, 0x26270000),
    },
    "F03026": {
        "F03026-label-entry-19": (0x80104CB4, 0x24060012, 0x24060013),
        "F03026-padding-117": (0x80104CE8, 0x24420074, 0x24420075),
        "F03026-digit-drop-2": (0x80104D28, 0x26650003, 0x26650002),
        "F03026-second-comma-after-seventh": (0x80104D4C, 0x24020005, 0x24020006),
        "F03026-comma-drop-13": (0x80104D5C, 0x2665000E, 0x2665000D),
        "F03026-eight-digits": (0x80104D7C, 0x2A220009, 0x2A220008),
    },
    "F03027": {
        "F03027-wipe-stride-4": (0x80104E04, 0x001118C0, 0x00111880),
        "F03027-counting-flag-2": (0x80104E3C, 0x24020001, 0x24020002),
        "F03027-counted-sfx-0x74": (0x80104E8C, 0x24050073, 0x24050074),
        "F03027-climbs-by-sixteenth": (0x80104EA8, 0x00021142, 0x00021102),
        "F03027-snaps-at-63": (0x80104EBC, 0x24020040, 0x2402003F),
        "F03027-eight-digits": (0x80104FD8, 0x2A220009, 0x2A220008),
    },
    "F03028": {
        "F03028-label-entry-22": (0x801050A0, 0x24060015, 0x24060016),
        "F03028-padding-39": (0x801050C4, 0x24420026, 0x24420027),
        "F03028-two-digits": (0x80105120, 0x2A620003, 0x2A620002),
        "F03028-percent-drop-9": (0x80105130, 0x26850008, 0x26850009),
        "F03028-percent-entry-20": (0x80105134, 0x24060013, 0x24060014),
    },
    "F03029": {
        "F03029-wipe-stride-4": (0x801051BC, 0x001018C0, 0x00101880),
        "F03029-counting-sfx-0x71": (0x80105204, 0x24050072, 0x24050071),
        "F03029-scales-from-64": (0x80105260, 0x2C620020, 0x2C620040),
        "F03029-scales-by-sixteenth": (0x8010527C, 0x00081142, 0x00081102),
        "F03029-steps-by-two": (0x80105298, 0x24820001, 0x24820002),
        "F03029-percent-entry-20": (0x80105374, 0x24060013, 0x24060014),
    },
    "F03030": {
        "F03030-clamps-from-48": (0x801053C4, 0x28C20041, 0x28C20030),
        "F03030-clamps-to-63": (0x801053D0, 0x24060040, 0x2406003F),
        "F03030-step-unset": (0x801053E8, 0xA0E60003, 0x00000000),
        "F03030-heading-entry-13": (0x801053EC, 0x2406000C, 0x2406000D),
        "F03030-centres-by-quarter": (0x801053F0, 0x00042042, 0x00042082),
    },
    "F03031": {
        "F03031-step-unset": (0x8010545C, 0xA046976F, 0x00000000),
        "F03031-measures-heights": (0x801054A8, 0x90420002, 0x90420003),
        "F03031-centres-by-quarter": (0x801054C8, 0x00022043, 0x00022083),
        "F03031-second-color-pair": (0x80105530, 0x24E7976C, 0x24E79770),
        "F03031-advances-left": (0x80105574, 0x02238821, 0x02238823),
    },
}


def mutants(identifier: str) -> dict:
    return {name: {address: edit} for name, (address, _original, edit) in EDITS[identifier].items()}


def verify_original_words(identifier: str, image) -> None:
    for name, (address, original, _edit) in EDITS[identifier].items():
        found = image.read(address)
        if found != original:
            raise SystemExit(f"{name}: {address:#x} holds {found:#010x}, expected {original:#010x}")


# ---------------------------------------------------------------------------------------------- vectors
EDGE_TIMERS = (-0x80000000, -1, 0, 1, 2, 31, 32, 33, 34, 48, 62, 63, 64, 65, 66, 0x7FFFFFFF)
EDGE_SCORES = (0, 1, 31, 32, 33, 63, 64, 999_999_999, 1_000_000_000, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF,
               0x0FFFFFFF, 0x10000000, 0x20000000)
EDGE_MAPS = (0, 1, 2, 30, 31, 32, 33, 63, 64, 99, 100, 101, 999, 1000, 1234, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF,
             0x08000000, 0x08000001)


def timer(rng: random.Random, number: int) -> int:
    pattern = number % 4
    if pattern == 0:
        return EDGE_TIMERS[number // 4 % len(EDGE_TIMERS)]
    if pattern == 3 and number % 32 == 3:
        return s32(rng.getrandbits(32))
    return rng.randrange(-3, 70)


def results_value(rng: random.Random, edges: tuple[int, ...], number: int, limit: int) -> int:
    pattern = number % 5
    if pattern == 0:
        return rng.choice(edges)
    if pattern == 1:
        return rng.randrange(32)
    if pattern == 4:
        return rng.getrandbits(32)
    return rng.randrange(limit)


def shown_value(rng: random.Random, target: int, steps: int) -> int:
    roll = rng.random()
    if roll < 0.35:
        return 0
    if roll < 0.6:
        return s32(target)
    if roll < 0.75:
        # The previous frame's value, so the next step can reach the target.
        return s32((target >> 5) * max(steps - 1, 0)) if rng.random() < 0.5 else s32(target - 1)
    if roll < 0.9:
        return rng.randrange(-2, 1100)
    return s32(rng.getrandbits(32))


def rank_row(rng: random.Random) -> list[int]:
    roll = rng.random()
    count = rng.choice((0, 1, 2, 2, 3, 3, 3)) if roll < 0.9 else rng.choice((4, 5, 0x80, 0xFF))
    words = [rng.randrange(ATLAS_ENTRIES) if rng.random() < 0.93 else rng.randrange(ATLAS_ENTRIES, 256)
             for _ in range(3)]
    return [count, *words]


def rank_value(rng: random.Random, number: int) -> int:
    if number % 19 == 5:
        return rng.choice((-1, 16, 17, 0x7FFFFFFF, -0x80000000, rng.randrange(16, 4096)))
    return rng.randrange(RANK_ROWS)


def vector_for(identifier: str, rng: random.Random, number: int) -> dict:
    operation = SPECS[identifier][0]
    clock = timer(rng, number)
    steps = min(max(clock, 0), 64) - 32
    if operation in (3, 4):
        value = results_value(rng, EDGE_MAPS, number, 101 if number % 3 else 2000)
    else:
        value = results_value(rng, EDGE_SCORES, number, 1_000_000_000)
    return {
        "operation": operation,
        "x": coordinate(rng),
        "y": coordinate(rng),
        "timer": clock,
        "value": value,
        "shown": shown_value(rng, value, steps),
        "counting": rng.choice((0, 0, 1, 1, 2, -1)),
        "rank": rank_value(rng, number),
        "unused": s32(rng.getrandbits(32)),
        # None keeps the module's own _disMap and _riskbreakerRanks.
        "ranks": None if number % 2 == 0 else [rank_row(rng) for _ in range(RANK_ROWS)],
        "atlas": None if number % 2 == 0 else [atlas_entry(rng) for _ in range(ATLAS_ENTRIES)],
    }


def clamped(value: dict) -> int:
    return min(max(value["timer"], 0), 64)


def counter_addresses(operation: int) -> tuple[int, int]:
    return (SHOWN_MAP, COUNTING_MAP) if operation in (3, 4) else (SHOWN_SCORE, COUNTING_SCORE)


def sprintf(machine: Machine) -> None:
    form = c_string(machine, machine.r[5])
    if form not in (b"%09d", b"%03d"):
        raise ValueError(f"unmodelled format {form!r}")
    text = form % s32(machine.r[6])
    machine.store(machine.r[4], text + b"\0")
    machine.r[2] = len(text)


def place_atlas(machine: Machine, atlas: list[list[int]] | None) -> list[list[int]]:
    """Writes a vector's _disMap entries (u, v, w, h, page, palette), or reads the module's own."""
    if atlas is None:
        return [list(struct.unpack("<4B2H", machine.bytes(ATLAS + 8 * index, 8))) for index in range(ATLAS_ENTRIES)]
    for index, fields in enumerate(atlas):
        machine.store(ATLAS + 8 * index, struct.pack("<4B2H", *fields))
    return atlas


def run_original(machine: Machine, value: dict) -> tuple[dict, list[int]]:
    operation = value["operation"]
    entry = next(spec[1] for spec in SPECS.values() if spec[0] == operation)
    atlas = place_atlas(machine, value["atlas"])
    ranks = value["ranks"]
    if ranks is None:
        ranks = [list(machine.bytes(RANKS + 4 * row, 4)) for row in range(RANK_ROWS)]
    else:
        machine.store(RANKS, bytes(byte for row in ranks for byte in row))
    shown_address, counting_address = counter_addresses(operation)
    inputs = [operation, value["x"], value["y"], value["timer"], value["value"], value["shown"], value["counting"],
              value["rank"], *(byte for row in ranks for byte in row),
              *(field for entry_fields in atlas for field in entry_fields)]
    if operation == 6 and clamped(value) > 0 and not 0 <= value["rank"] < RANK_ROWS:
        return {"skipped": True, "ranks": ranks}, inputs
    machine.put(SCORE, value["value"] if operation not in (3, 4) else 0)
    machine.put(MAP_COMPLETION, value["value"] if operation in (3, 4) else 0)
    machine.put(RANK, value["rank"])
    machine.put(shown_address, value["shown"])
    machine.put(counting_address, value["counting"])
    calls: list[list[int]] = []

    def draw(kind: int):
        def record(m: Machine) -> None:
            colors = list(m.bytes(m.r[7], 8))
            edge = s32(m.load(m.r[29] + 0x10)) if kind == KIND_WIPE else 0
            calls.append([kind, s32(m.r[4]), s32(m.r[5]), s32(m.r[6]), *colors, edge])
        return record

    def sound(kind: int):
        def record(m: Machine) -> None:
            calls.append([kind, s32(m.r[4]), s32(m.r[5])] + [0] * (CALL_WORDS - 3))
        return record

    machine.stub(POP_IN, draw(FLASH))
    machine.stub(TINT_FADE, draw(TINT))
    machine.stub(WIPE, draw(KIND_WIPE))
    machine.stub(PLAY_SFX, sound(PLAY))
    machine.stub(STOP_SFX, sound(STOP))
    machine.stub(SPRINTF, sprintf)
    machine.r[4] = value["x"] & MASK
    machine.r[5] = value["y"] & MASK
    machine.r[6] = value["timer"] & MASK
    machine.r[7] = value["unused"] & MASK
    machine.execute(entry)
    return {
        "skipped": False,
        "ranks": ranks,
        "calls": calls,
        "shown": s32(machine.load(shown_address)),
        "counting": s32(machine.load(counting_address)),
        "stores": list(machine.stores),
    }, inputs


def expects_refusal(value: dict, observation: dict) -> bool:
    """Native refuses what the original would draw from outside _disMap (a "%0Nd" minus sign or a rank word of 127
    or more) or past its rank row (more than three words)."""
    if observation["skipped"]:
        return True
    if any(call[0] in DRAWS and not 0 <= call[3] < ATLAS_ENTRIES for call in observation["calls"]):
        return True
    if value["operation"] == 6 and clamped(value) > 0:
        return observation["ranks"][value["rank"]][0] > 3
    return False


def check_original(_image, value: dict, observation: dict, native: list[int], _reference: dict) -> list[str]:
    if len(native) < 4 or len(native) != 4 + native[3] * CALL_WORDS:
        return [f"invalid native protocol {native[:8]}"]
    status, shown, counting = native[0], native[1], native[2]
    calls = [native[4 + i * CALL_WORDS:4 + (i + 1) * CALL_WORDS] for i in range(native[3])]
    if observation["skipped"]:
        unchanged = shown == value["shown"] and counting == value["counting"]
        return [] if status == 2 and not calls and unchanged else [f"native drew a refused rank {native[:8]}"]
    problems = []
    if shown != observation["shown"] or counting != observation["counting"]:
        problems.append(f"counters native ({shown}, {counting}) original ({observation['shown']}, "
                        f"{observation['counting']})")
    original_calls = observation["calls"]
    if expects_refusal(value, observation):
        if status != 2:
            problems.append(f"status native {status}, expected the refusal 2")
        if any(call[0] in DRAWS for call in calls):
            problems.append("native drew on a refusal")
        original_calls = [call for call in original_calls if call[0] not in DRAWS]
    else:
        expected_status = 0 if any(call[0] in DRAWS for call in original_calls) else 1
        if status != expected_status:
            problems.append(f"status native {status} original {expected_status}")
    if calls != original_calls:
        first = next((i for i, (a, b) in enumerate(zip(calls, original_calls, strict=False)) if a != b),
                     min(len(calls), len(original_calls)))
        problems.append(f"calls differ from {first} ({len(calls)} native, {len(original_calls)} original): "
                        f"native {calls[first:first + 1]} original {original_calls[first:first + 1]}")
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK), COLORS, (SHOWN_MAP, SHOWN_SCORE + 4),
               (COUNTING_SCORE, COUNTING_MAP + 4)]
    return problems + store_problems(observation["stores"], allowed)


def build_case(identifier: str) -> dict:
    operation, entry, end, name = SPECS[identifier]

    def vector(rng: random.Random, number: int) -> dict:
        return vector_for(identifier, rng, number)

    def describe(image, results) -> list[str]:
        verify_original_words(identifier, image)
        refused = [(v, o) for v, o, _ in results if expects_refusal(v, o)]
        executed = [(v, o) for v, o, _ in results if not o["skipped"]]
        drawn = sum(any(c[0] in DRAWS for c in o["calls"]) for v, o in executed if not expects_refusal(v, o))
        lines = [
            f"MENUF.PRG {name}: executed {(end - entry) // 4} original instructions including the return delay "
            "slot, the draw and sound callees stubbed at the call boundary and sprintf modelled for %09d/%03d.",
            "Compared every call in order (kind, origin, texture index, both CVECTORs behind the color pointer, "
            "wipe edge; sound group and effect), the final counters and every original store.",
            f"{len(refused)} vectors are documented refusals (a minus-sign glyph, a rank word outside the 127 "
            "_disMap entries, a row of more than three words or a rank outside the 16 rows); native must refuse "
            "exactly those and still match the sounds and counters.",
            f"{drawn} vectors drew, {len(results) - len(refused) - drawn} drew nothing.",
        ]
        if operation in (2, 4):
            sounds = [c for _, o in executed for c in o["calls"] if c[0] in (PLAY, STOP)]
            counted = sum(c[0] == PLAY and c[2] == 0x73 for c in sounds)
            lines.append(f"{len(sounds)} sound calls compared, {counted} counted effects (0x73).")
            snapped = sum(clamped(v) == 64 for v, o in executed)
            lines.append(f"{snapped} vectors at the final step, "
                         f"{sum(32 <= clamped(v) < 64 for v, _ in executed)} counting.")
        if operation == 6:
            rows = sorted({o["ranks"][v["rank"]][0] for v, o in executed if 0 <= v["rank"] < RANK_ROWS})
            lines.append(f"Row word counts seen: {rows}.")
        return lines

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "MODULES": (MODULE, "SLUS_010.40"),
        "DRIVER": "gameclear_score_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\Results\GameClearScore.cpp",),
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
