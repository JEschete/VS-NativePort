"""F01846-F01849 (7EE98.c:506, 553, 599, 753) arena boss orbit vs BossOrbit.cpp.

func_800E7698 (radial retreat), func_800E78F4 (action readiness), func_800E7960 (orbit tracking)
and func_800E7F8C (behaviour) run with the arctangent, the cosine and sine, the square root, the
squared length and the four routines' calls to one another followed. The command dispatch, the
movement recovery, the candidate cooldown test, the engagement test, the engage and disengage
calls, the script-flag read and write and rand() are stubs: each call records its arguments and
every boss and orbit field, and the tests, the flag read and rand() return queued answers. The
model's flag bit, the arena orbit, its two globals and the battle actor's command words are
compared at the end; the model word's other bits must keep their values.

Vectors give the boss a model, stats, a battle actor with stats, a ranked first candidate and the
room AI state, and keep the retreat path step inside the 31-entry table; the native refuses the
rest where the original would read through a null pointer or past the table. The path is the
module's own table or a random one with end entries placed at the step. Stats aim the HP ratio at
both health marks, the orbit's radius, height, angular speed and height offset sit at each easing,
clamp and snap threshold, and the readiness threshold sits at the room's level.
"""

from __future__ import annotations

import math
import random

from differential import (
    MASK,
    Image,
    Machine,
    call_problems,
    field_problems,
    pack_fields,
    s16,
    s32,
    store_problems,
)

ID = "F01846, F01847, F01848, F01849"
ENTRY, END = 0x800E7698, 0x800E8184
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "boss_orbit_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\BossOrbit.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1846
MUTANT_VECTORS = 768

RETREAT = 0x800E7698
READY = 0x800E78F4
TRACKING = 0x800E7960
BEHAVIOR = 0x800E7F8C
ARCTANGENT = 0x8004137C
COSINE = 0x80040F24
SINE = 0x80040F28
SQUARE_ROOT = 0x80041B94
SQUARED_LENGTH = 0x800E4660
COMMAND = 0x800DEEFC
RECOVER = 0x800E2CCC
COOLDOWN = 0x800DBCB4
ENGAGE_READY = 0x800D826C
ENGAGE = 0x800D836C
DISENGAGE = 0x800D82CC
PROGRESS = 0x800BEBF4
SET_PROGRESS = 0x800BEC14
RANDOM = 0x80026EC4

RETREAT_PATH, PATH_STEPS = 0x800F17DC, 31
PLAYER_POINTER = 0x800F4538
ROOM_POINTER = 0x800F58BC
VELOCITY, HEIGHT_OFFSET, ORBIT_POINTER = 0x800F5918, 0x800F591C, 0x800F5920
PROGRESS_FLAG = 0xA6

DATA = 0x80500000
MODEL, MODEL_WORD = DATA, DATA + 0x5B0
STATS = DATA + 0x600
ACTOR_STATS = DATA + 0xD80
PLAYER = DATA + 0x1500
STATE, STATE_SIZE = DATA + 0x1C00, 0x490
BATTLE_ACTOR = DATA + 0x2100
ORBIT, ORBIT_SIZE = DATA + 0x2140, 0x20
ROOM = DATA + 0x2160
CANDIDATE = DATA + 0x2180
DATA_SIZE = 0x2200
STACK_SPAN = (0x80400000 - 0x4000, 0x80400000)

CENTRE = 0x7C0
MOVING = 0x70000
FLAG = 0x10
K_RETREAT, K_READY, K_TRACKING, K_BEHAVIOR = range(4)
ENTRIES = {K_RETREAT: RETREAT, K_READY: READY, K_TRACKING: TRACKING, K_BEHAVIOR: BEHAVIOR}

# name, offset, size, signed
BOSS_INPUTS = (("x", 0x4C, 2, True), ("y", 0x4E, 2, True), ("z", 0x50, 2, True))
BOSS_FIELDS = (
    ("movement_target", 0x15C, 1, False), ("move_x", 0x17C, 4, True), ("move_z", 0x180, 4, True),
    ("move_y", 0x184, 4, True), ("candidate_count", 0x470, 4, True),
)
ORBIT_FIELDS = (
    ("heading", 0x00, 2, False), ("retreat_phase", 0x02, 1, False), ("distance_cached", 0x03, 1, False),
    ("distance_squared", 0x04, 4, True), ("is_orbiting", 0x0A, 1, False), ("path_step", 0x0B, 1, False),
    ("cooldown", 0x0C, 2, True), ("move_requested", 0x0E, 2, True), ("retreat_angle", 0x10, 2, True),
    ("saved_height", 0x12, 2, True), ("health_phase", 0x14, 1, False), ("plan_saved", 0x16, 1, False),
    ("saved_plan_count", 0x17, 1, False), ("start_x", 0x18, 2, True), ("start_z", 0x1A, 2, True),
    ("started", 0x1C, 1, False),
)
STATS_FIELDS = (("current_hp", 0x18, 2, True), ("max_hp", 0x1A, 2, True), ("core_hp", 0x708, 2, True))
COMMAND_FIELDS = (
    ("command_state", 0x08, 1, False), ("opcode", 0x09, 1, False), ("arg0", 0x0C, 4, True),
    ("arg1", 0x10, 4, True), ("arg2", 0x14, 4, True),
)
PLAYER_FIELDS = (("word8", 0x08, 4, False), ("x", 0x1C, 2, True), ("y", 0x1E, 2, True), ("z", 0x20, 2, True),
                 ("collision_height", 0x63E, 2, False))
BOSS_NAMES = [name for name, *_ in BOSS_FIELDS]
ORBIT_NAMES = [name for name, *_ in ORBIT_FIELDS]
COMMAND_NAMES = [name for name, *_ in COMMAND_FIELDS]
NAMES = ["model_flag", *BOSS_NAMES, *ORBIT_NAMES, "angular_velocity", "height_offset"]

C_COMMAND, C_RECOVER, C_COOLDOWN, C_ENGAGE_READY, C_ENGAGE, C_DISENGAGE, C_PROGRESS, C_SET_PROGRESS, C_RANDOM = \
    range(9)
CALL_NAMES = ("command", "recover", "cooldown", "engage?", "engage", "disengage", "progress", "set-progress",
              "rand")
RECORD = 3 + len(NAMES)
# Added to a call's code when the original passed something other than the boss (or the progress
# flag, or the first candidate).
NOT_THE_ACTOR = 100

MUTANTS = {
    # func_800E7698
    "flag-steps-15": {0x800E76E0: 0x2C42000F},
    "is-orbiting-not-set": {0x800E7728: 0x00000000},
    "retreat-angle-0x800": {0x800E7748: 0x24420800},
    "path-step-not-reset": {0x800E7758: 0x00000000},
    "start-x-not-stored": {0x800E776C: 0x00000000},
    "start-z-not-stored": {0x800E7778: 0x00000000},
    "end-ignores-height": {0x800E77A4: 0x00000000},
    "moving-mask-bit-16": {0x800E77B8: 0x3C030001},
    "phase-not-reset": {0x800E77D8: 0x00000000},
    "heading-not-stored": {0x800E7800: 0x00000000},
    "end-returns-false": {0x800E7808: 0x24020000},
    "end-flag-kept": {0x800E7814: 0x00000000},
    "x-scaled-0x400": {0x800E7848: 0x00021280},
    "retreat-intent-9": {0x800E78AC: 0x24050009},
    "distance-cache-kept": {0x800E78C8: 0x00000000},
    "path-not-advanced": {0x800E78D0: 0x00000000},
    # func_800E78F4
    "no-candidates-ignored": {0x800E7910: 0x00000000},
    "cooldown-ignored": {0x800E7924: 0x00000000},
    "readiness-offset-0x1a": {0x800E7938: 0x9463001A},
    "threshold-shift-12": {0x800E793C: 0x00021302},
    "threshold-compare-reversed": {0x800E7944: 0x0062882A},
    # func_800E7960
    "started-not-set": {0x800E79D8: 0x00000000},
    "saved-height-not-initialised": {0x800E79DC: 0x00000000},
    "radius-grow-0x10": {0x800E7A4C: 0x26B50010},
    "radius-shrink-0x10": {0x800E7A5C: 0x26B5FFF0},
    "radius-snap-0x10": {0x800E7A70: 0x28420010},
    "height-target-0xa1": {0x800E7A9C: 0x2482FF5F},
    "height-snap-8": {0x800E7AAC: 0x28420008},
    "height-rise-8": {0x800E7AC0: 0x24620008},
    "height-fall-8": {0x800E7AC4: 0x2462FFF8},
    "away-heading-0x400": {0x800E7B50: 0x24F60400},
    "eased-speed-0x180000": {0x800E7BE4: 0x3C020018},
    "aligned-easing-quarter": {0x800E7C18: 0x00031883},
    "opposed-easing-eighth": {0x800E7C38: 0x000318C3},
    "ahead-tolerance-0x100": {0x800E7C60: 0x24040100},
    "never-reversed": {0x800E7C80: 0x00000000},
    "max-speed-0x810000": {0x800E7CA8: 0x3C020081},
    "max-speed-unclamped": {0x800E7CB0: 0x10000004},
    "first-mark-0x7fff": {0x800E7CF8: 0x34027FFF},
    "slow-speed-0x2ffff": {0x800E7D0C: 0x3C030002},
    "second-mark-0x2000": {0x800E7D10: 0x28A22000},
    "low-height-31": {0x800E7D6C: 0x2842001F},
    "random-mask-0xff": {0x800E7D80: 0x304200FF},
    "random-ignored": {0x800E7D84: 0x00000000},
    "movement-target-kept": {0x800E7DB8: 0x00000000},
    "closing-scale-0x40": {0x800E7E18: 0x2402FFC0},
    "pulling-scale-0x100": {0x800E7E44: 0x24020100},
    "stray-scale-8": {0x800E7E54: 0x24030008},
    "fold-at-0x400": {0x800E7E6C: 0x28820401},
    "zero-remaining-keeps-scale": {0x800E7E88: 0x00000000},
    "limit-times-2": {0x800E7EB8: 0x00051040},
    "min-height-minus-50": {0x800E7EF4: 0x2842FFCE},
    "max-height-0x100": {0x800E7F0C: 0x28420101},
    "move-y-cap-0xfe000": {0x800E7F48: 0x3442E000},
    "orbit-intent-9": {0x800E7F58: 0x24050009},
    # func_800E7F8C
    "engage-skipped": {0x800E7FB4: 0x00000000},
    "cooldown-from-zero": {0x800E7FD8: 0x04400002},
    "retreat-case-5": {0x800E7FEC: 0x24020005},
    "plan-saved-kept": {0x800E8014: 0x00000000},
    "plan-not-restored": {0x800E8018: 0x00000000},
    "is-orbiting-kept": {0x800E8020: 0x00000000},
    "core-alive-at-3": {0x800E8054: 0x28420003},
    "cooldown-0x385": {0x800E805C: 0x24020385},
    "requested-progress-2": {0x800E806C: 0x24130002},
    "plan-count-not-saved": {0x800E8070: 0x00000000},
    "started-kept": {0x800E8080: 0x00000000},
    "actor-core-alive-at-3": {0x800E80C4: 0x28420003},
    "attack-row-not-stored": {0x800E80D0: 0x00000000},
    "half-height-unshifted": {0x800E80FC: 0x00000000},
    "attack-opcode-0x19": {0x800E8114: 0x24020019},
    "command-state-0x40": {0x800E811C: 0x24020040},
    "player-z-not-stored": {0x800E8124: 0x00000000},
    "saved-height-not-restored": {0x800E8130: 0x00000000},
    "ended-progress-5": {0x800E8140: 0x24130005},
    "height-offset-kept": {0x800E8148: 0x00000000},
    "requested-kept": {0x800E8150: 0x00000000},
    "velocity-kept": {0x800E8154: 0x00000000},
    "progress-unmasked": {0x800E8160: 0x02602821},
}


def coordinate(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.08:
        return CENTRE
    if roll < 0.9:
        return CENTRE + rng.randrange(-0x600, 0x600)
    if roll < 0.97:
        return CENTRE + rng.randrange(-0x2000, 0x2000)
    return s16(rng.getrandbits(16))


# The cached value is non-negative in the game: SquaredVectorLength of s16 deltas only wraps when both
# deltas are -0x8000, and an eased radius is at most 46308, whose square fits. For a negative input
# vs_gte_rsqrt indexes before its table at D_80040C18, so the native root's 0 is not compared there.
def distance_squared(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.85:
        radius = rng.choice((0, 1, 900, 991, 992, 993, 1000, 1023, 1024, 1025, 1055, 1056, 1057, 1100, 2000,
                             46308, 46340, rng.randrange(0x1000)))
        return min(max(0, radius * radius + rng.choice((0, 0, -1, 1, rng.randrange(-2000, 2000)))), 0x7FFFFFFF)
    return rng.randrange(0x80000000)


def angular_velocity(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.1:
        return near_clamp(rng)
    if roll < 0.6:
        magnitude = rng.choice((0, 1, 0xFFFF, 0x10000, 0x3FFFF, 0x40000, 0x27FFFF, 0x280000, 0x280001,
                                0x81FFFF, 0x820000, 0x820001, 0x830000, rng.randrange(0x900000)))
        return magnitude if rng.random() < 0.5 else -magnitude
    if roll < 0.97:
        return rng.randrange(-0x900000, 0x900000)
    return s32(rng.getrandbits(32))


# Speeds that one frame's change (a sixteenth turn plus an eighth or a quarter of the speed, either way)
# can carry into (0x810000, 0x820000], just under the clamp.
def near_clamp(rng: random.Random) -> int:
    final = rng.randrange(0x810001, 0x820001)
    speed = rng.choice(((final - 0x10000) * 8 // 9, (final - 0x10000) * 4 // 5, (final + 0x10000) * 8 // 7,
                        (final + 0x10000) * 4 // 3))
    return speed if rng.random() < 0.5 else -speed


def height_offset(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.6:
        return rng.choice((-101, -100, -99, 0, 29, 30, 31, 499, 500, 501))
    if roll < 0.97:
        return rng.randrange(-600, 600)
    return s32(rng.getrandbits(32))


def stats(rng: random.Random) -> dict:
    max_hp = rng.choice((0, 1, 4, 100, 999, rng.randrange(1, 0x8000), rng.randrange(1, 0x8000),
                         s16(rng.getrandbits(16))))
    roll = rng.random()
    if roll < 0.6 and max_hp > 0:
        mark = rng.choice((0xC000, 0x4000))
        current = (mark * max_hp >> 16) + rng.choice((-1, 0, 0, 1, 2))
    elif roll < 0.9 and max_hp > 0:
        current = rng.randrange(max_hp + 1)
    else:
        current = s16(rng.getrandbits(16))
    return {"current_hp": s16(current), "max_hp": max_hp, "core_hp": core_hp(rng)}


def core_hp(rng: random.Random) -> int:
    return rng.choice((0, 1, 2, 2, 3, 100, -1, s16(rng.getrandbits(16))))


def byte_flag(rng: random.Random, zero: float) -> int:
    return 0 if rng.random() < zero else rng.choice((1, 1, rng.getrandbits(8)))


def path_table(rng: random.Random) -> list[tuple[int, int]]:
    steps = []
    for _ in range(PATH_STEPS):
        roll = rng.random()
        if roll < 0.12:
            steps.append((0, 0))
        elif roll < 0.22:
            steps.append((0, rng.randrange(-0x100, 0x100) or 1))
        elif roll < 0.32:
            steps.append((rng.randrange(-0x200, 0x200) or 1, 0))
        else:
            steps.append((rng.randrange(-0x200, 0x200), rng.randrange(-0x100, 0x100)))
    return steps


def orbit(rng: random.Random, kind: int, progress: int) -> dict:
    retreating = kind == K_RETREAT or (kind == K_BEHAVIOR and progress & 0xFF == 4)
    phase = rng.choices((0, 1, 2, rng.getrandbits(8)), (25, 60, 8, 7))[0] if retreating \
        else rng.choice((0, 1, rng.getrandbits(8)))
    step = rng.choice((rng.randrange(PATH_STEPS), rng.randrange(24, PATH_STEPS), 0, 13, 14))
    return {
        "heading": rng.getrandbits(16), "retreat_phase": phase, "distance_cached": byte_flag(rng, 0.5),
        "distance_squared": distance_squared(rng), "is_orbiting": byte_flag(rng, 0.4), "path_step": step,
        "cooldown": rng.choice((0, 0, 1, 2, -1, 900, s16(rng.getrandbits(16)))),
        "move_requested": rng.choice((0, 0, 1, s16(rng.getrandbits(16)))),
        "retreat_angle": s16(rng.getrandbits(16)),
        "saved_height": rng.choice((160, 159, 161, 144, 145, 175, 176, 0, 0x200, 0x7FFF, -0x8000,
                                    rng.randrange(-0x400, 0x400))),
        "health_phase": rng.choice((0, 0, 1, 1, 2, rng.getrandbits(8))), "plan_saved": byte_flag(rng, 0.5),
        "saved_plan_count": rng.getrandbits(8), "start_x": s16(rng.getrandbits(16)),
        "start_z": s16(rng.getrandbits(16)), "started": byte_flag(rng, 0.4),
    }


def vector(rng: random.Random, number: int) -> dict:
    kind = number % 4 if number < 16 else rng.choices((K_RETREAT, K_READY, K_TRACKING, K_BEHAVIOR),
                                                       (25, 15, 30, 30))[0]
    progress = rng.choices((0, 4, rng.choice((1, 6, 0x104, 0x100, 0x400, rng.getrandbits(32)))), (45, 40, 15))[0]
    o = orbit(rng, kind, progress)
    path = None if rng.random() < 0.65 else path_table(rng)
    if path is not None and o["retreat_phase"] == 1 and rng.random() < 0.4:
        path[o["path_step"]] = (0, 0)
    readiness = rng.choice((0, 1, 0x7FFF, 0x8000, 0xFFFF, rng.randrange(0x100), rng.getrandbits(16)))
    threshold = (readiness + rng.choice((-1, 0, 1))) & 0xFFFF if rng.random() < 0.6 else rng.getrandbits(16)
    v = {
        "kind": kind,
        "boss": {"x": coordinate(rng), "y": rng.choice((0, -0x100, rng.randrange(-0x800, 0x800),
                                                         s16(rng.getrandbits(16)))), "z": coordinate(rng)},
        "fields": {"movement_target": rng.getrandbits(8), "move_x": s32(rng.getrandbits(32)),
                   "move_z": s32(rng.getrandbits(32)), "move_y": s32(rng.getrandbits(32)),
                   "candidate_count": rng.choice((0, 0, 1, 2, 16, 0x100, s32(rng.getrandbits(32))))},
        "model_word": rng.getrandbits(32), "stats": stats(rng), "orbit": o,
        "velocity": angular_velocity(rng), "height_offset": height_offset(rng), "path": path,
        "player": {"word8": rng.getrandbits(32) & ~(MOVING if rng.random() < 0.5 else 0),
                   "x": coordinate(rng), "y": rng.randrange(-0x800, 0x800), "z": coordinate(rng),
                   "collision_height": rng.choice((0, 1, 0x80, 0xFF, rng.randrange(0x200), rng.getrandbits(16)))},
        "candidate_word": rng.getrandbits(3) << 29 | threshold << 13 | rng.getrandbits(13),
        "threshold": threshold, "readiness": readiness,
        "cooldowns": [rng.choice((0, 1, 1, 2, 0x80000000)) for _ in range(2)],
        "engagements": [rng.choice((0, 1, 1, 0x100))],
        "randoms": [rng.choice((0, 0x200, 0x400, 1, 0x1FF, 0x100, 0x300, rng.randrange(0x8000)))],
        "progress": progress,
        "actor_core_hp": core_hp(rng),
        "command": {"command_state": rng.getrandbits(8), "opcode": rng.getrandbits(8),
                    "arg0": s32(rng.getrandbits(32)), "arg1": s32(rng.getrandbits(32)),
                    "arg2": s32(rng.getrandbits(32))},
        "noise": rng.randbytes(DATA_SIZE),
    }
    if kind in (K_TRACKING, K_BEHAVIOR) and rng.random() < 0.08:
        random_draw(rng, v)
    if kind in (K_TRACKING, K_BEHAVIOR) and rng.random() < 0.1:
        same_bearing(rng, v)
    elif kind in (K_TRACKING, K_BEHAVIOR) and rng.random() < 0.08:
        beside_fleeing_heading(rng, v)
    return v


# The boss just outside the 0xDE cone around the heading away from the player, which decides an
# untracked turn's direction.
def beside_fleeing_heading(rng: random.Random, v: dict) -> None:
    boss = rng.randrange(0x1000)
    player = boss - 0x800 - rng.choice((1, -1)) * rng.randrange(0xE4, 0xFC)
    for target, angle in ((v["boss"], boss), (v["player"], player)):
        radius = rng.randrange(0x200, 0x600)
        theta = angle * math.pi / 0x800
        target["x"] = CENTRE - round(radius * math.sin(theta))
        target["z"] = CENTRE - round(radius * math.cos(theta))


# The player on the boss's bearing from the centre or opposite it, so no turn is left to make.
def same_bearing(rng: random.Random, v: dict) -> None:
    x, z = v["boss"]["x"], v["boss"]["z"]
    if rng.random() < 0.5:
        x, z = s16(2 * CENTRE - x), s16(2 * CENTRE - z)
    v["player"].update(x=x, z=z)
    v["height_offset"] = rng.choice((-80, -79, 0, 300, 301, 499, rng.randrange(-100, 501)))


# A ready, engaged boss turning slowly near its orbit height with no cooldown or health step left: the
# frame that may ask rand() for a move.
def random_draw(rng: random.Random, v: dict) -> None:
    v["orbit"].update(cooldown=0, health_phase=2, saved_height=rng.choice((160, 150, 170)))
    v["velocity"] = rng.randrange(-0x30000, 0x30000)
    v["height_offset"] = rng.choice((29, 30, rng.randrange(-200, 40)))
    v["fields"]["candidate_count"] = 1
    v["readiness"] = rng.randrange(1, 0x10000)
    v["threshold"] = rng.randrange(v["readiness"])
    v["candidate_word"] = rng.getrandbits(3) << 29 | v["threshold"] << 13 | rng.getrandbits(13)
    v["cooldowns"] = [1, rng.choice((0, 1))]
    v["engagements"] = [1]
    v["randoms"] = [rng.choice((0, 0x200, 0x400, 1, 0x1FF, 0x100, 0x300, rng.randrange(0x8000)))]


def data_block(v: dict) -> bytes:
    block = bytearray(v["noise"])

    def place(base: int, fields, values: dict, extent: int) -> None:
        at = base - DATA
        block[at:at + extent] = pack_fields(block[at:at + extent], fields, values)

    def word(address: int, value: int, size: int = 4) -> None:
        block[address - DATA:address - DATA + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")

    word(MODEL_WORD, v["model_word"])
    place(STATS, STATS_FIELDS, v["stats"], 0x70A)
    word(ACTOR_STATS + 0x708, v["actor_core_hp"], 2)
    place(PLAYER, PLAYER_FIELDS, v["player"], 0x640)
    place(STATE, BOSS_INPUTS, v["boss"], STATE_SIZE)
    place(STATE, BOSS_FIELDS, v["fields"], STATE_SIZE)
    word(STATE + 0x54, BATTLE_ACTOR)
    word(STATE + 0x58, MODEL)
    word(STATE + 0x5C, STATS)
    word(STATE + 0x430, CANDIDATE)
    place(BATTLE_ACTOR, COMMAND_FIELDS, v["command"], 0x40)
    word(BATTLE_ACTOR + 0x3C, ACTOR_STATS)
    place(ORBIT, ORBIT_FIELDS, v["orbit"], ORBIT_SIZE)
    word(ROOM + 0x18, v["readiness"], 2)
    word(CANDIDATE + 4, v["candidate_word"])
    return bytes(block)


def snapshot(m: Machine) -> list[int]:
    return [m.load(MODEL_WORD) >> 4 & 1, *m.load_fields(STATE, BOSS_FIELDS, BOSS_NAMES),
            *m.load_fields(ORBIT, ORBIT_FIELDS, ORBIT_NAMES), m.load(VELOCITY, 4, True),
            m.load(HEIGHT_OFFSET, 4, True)]


def run(machine: Machine, v: dict):
    machine.images.append(Image(data_block(v), DATA))
    machine.put(ORBIT_POINTER, ORBIT)
    machine.put(ROOM_POINTER, ROOM)
    machine.put(PLAYER_POINTER, PLAYER)
    machine.put(VELOCITY, v["velocity"])
    machine.put(HEIGHT_OFFSET, v["height_offset"])
    if v["path"] is not None:
        for index, (distance, height) in enumerate(v["path"]):
            machine.put(RETREAT_PATH + index * 4, distance, 2)
            machine.put(RETREAT_PATH + index * 4 + 2, height, 2)
    path = [(machine.load(RETREAT_PATH + index * 4, 2, True), machine.load(RETREAT_PATH + index * 4 + 2, 2, True))
            for index in range(PATH_STEPS)]
    machine.follow.update({ARCTANGENT, COSINE, SINE, SQUARE_ROOT, SQUARED_LENGTH, RETREAT, READY, TRACKING})
    calls: list[list[int]] = []
    cooldowns, engagements, randoms = list(v["cooldowns"]), list(v["engagements"]), list(v["randoms"])

    def record(m: Machine, code: int, argument: int = 0, expected: int = STATE) -> None:
        if m.r[4] != expected:
            code += NOT_THE_ACTOR
        calls.append([code, argument, 0, *snapshot(m)])

    def cooldown(m: Machine) -> None:
        record(m, C_COOLDOWN + (NOT_THE_ACTOR if m.r[5] != CANDIDATE else 0))
        m.r[2] = (cooldowns.pop(0) if cooldowns else 0) & MASK

    def engage_ready(m: Machine) -> None:
        record(m, C_ENGAGE_READY)
        m.r[2] = (engagements.pop(0) if engagements else 0) & MASK

    def progress(m: Machine) -> None:
        record(m, C_PROGRESS, expected=PROGRESS_FLAG)
        m.r[2] = v["progress"] & MASK

    def draw(m: Machine) -> None:
        record(m, C_RANDOM, expected=m.r[4])
        m.r[2] = (randoms.pop(0) if randoms else 0) & MASK

    machine.stub(COMMAND, lambda m: record(m, C_COMMAND, s32(m.r[5])))
    machine.stub(RECOVER, lambda m: record(m, C_RECOVER))
    machine.stub(COOLDOWN, cooldown)
    machine.stub(ENGAGE_READY, engage_ready)
    machine.stub(ENGAGE, lambda m: record(m, C_ENGAGE))
    machine.stub(DISENGAGE, lambda m: record(m, C_DISENGAGE))
    machine.stub(PROGRESS, progress)
    machine.stub(SET_PROGRESS, lambda m: record(m, C_SET_PROGRESS, m.r[5] & MASK, PROGRESS_FLAG))
    machine.stub(RANDOM, draw)
    machine.r[4] = STATE
    machine.execute(ENTRIES[v["kind"]])
    observation = {"result": machine.r[2], "fields": snapshot(machine), "model_word": machine.load(MODEL_WORD),
                   "command": machine.load_fields(BATTLE_ACTOR, COMMAND_FIELDS, COMMAND_NAMES), "calls": calls,
                   "stores": machine.stores}
    b, f, o, s, p, c = v["boss"], v["fields"], v["orbit"], v["stats"], v["player"], v["command"]
    inputs = [v["kind"], b["x"], b["y"], b["z"], v["model_word"] >> 4 & 1, s["current_hp"], s["max_hp"],
              s["core_hp"], *(f[name] for name in BOSS_NAMES), *(o[name] for name in ORBIT_NAMES), v["velocity"],
              v["height_offset"], *(value for step in path for value in step), int(p["word8"] & MOVING != 0),
              p["x"], p["y"], p["z"], p["collision_height"], v["threshold"], v["readiness"],
              len(v["cooldowns"]), *v["cooldowns"], len(v["engagements"]), *v["engagements"],
              len(v["randoms"]), *v["randoms"], v["progress"], v["actor_core_hp"],
              *(c[name] for name in COMMAND_NAMES)]
    return observation, inputs


def allowed_stores() -> list[tuple[int, int]]:
    spans = [(STATE + offset, STATE + offset + size) for _, offset, size, _ in BOSS_FIELDS]
    return [*spans, (MODEL_WORD, MODEL_WORD + 4), (ORBIT, ORBIT + ORBIT_SIZE), (VELOCITY, ORBIT_POINTER),
            (BATTLE_ACTOR + 8, BATTLE_ACTOR + 0x18), STACK_SPAN]


ALLOWED = allowed_stores()


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    problems = []
    # func_800E7960 returns nothing; the native reports success.
    expected = obs["result"] if v["kind"] in (K_RETREAT, K_READY) else 1
    if native[1] != expected:
        problems.append(f"result native {native[1]} original {expected}")
    start = 2 + len(NAMES)
    problems += field_problems(NAMES, native[2:start], obs["fields"])
    end = start + len(COMMAND_NAMES)
    problems += field_problems(COMMAND_NAMES, native[start:end], obs["command"])
    if (obs["model_word"] ^ v["model_word"]) & ~FLAG & MASK:
        problems.append(f"original model word {obs['model_word']:#x} from {v['model_word']:#x}")
    count = native[end]
    calls = [native[end + 1 + i * RECORD:end + 1 + (i + 1) * RECORD] for i in range(count)]
    problems += call_problems(calls, obs["calls"])
    return problems + store_problems(obs["stores"], ALLOWED)


def describe(image: Image, results: list) -> list[str]:
    paths: list[dict[str, int]] = [{}, {}, {}, {}]
    events = dict.fromkeys(("attack", "retreat ended", "move requested", "health phase", "random request",
                            "speed clamped", "speed under clamp", "same bearing", "height clamped",
                            "move-y capped", "radius snapped"), 0)
    for v, obs, _native in results:
        signature = "+".join(CALL_NAMES[call[0] % NOT_THE_ACTOR] for call in obs["calls"]) or "none"
        kind = v["kind"]
        if kind in (K_RETREAT, K_READY):
            signature += f" -> {obs['result']}"
        paths[kind][signature] = paths[kind].get(signature, 0) + 1
        final = dict(zip(NAMES, obs["fields"], strict=True))
        o = v["orbit"]
        events["attack"] += obs["command"][0] == 0x80 and obs["command"][1] == 0x18 \
            and v["command"]["opcode"] != 0x18
        events["retreat ended"] += any(c[0] == C_SET_PROGRESS and c[1] == 6 for c in obs["calls"])
        events["move requested"] += any(c[0] == C_SET_PROGRESS and c[1] == 1 for c in obs["calls"])
        events["health phase"] += final["health_phase"] != o["health_phase"]
        events["random request"] += any(c[0] == C_RANDOM for c in obs["calls"]) \
            and final["move_requested"] == 1 and o["move_requested"] != 1
        if kind == K_TRACKING:
            events["speed clamped"] += abs(final["angular_velocity"]) == 0x820000
            events["speed under clamp"] += 0x810000 < abs(final["angular_velocity"]) < 0x820000
            events["same bearing"] += (v["player"]["x"], v["player"]["z"]) in (
                (v["boss"]["x"], v["boss"]["z"]), (s16(2 * CENTRE - v["boss"]["x"]), s16(2 * CENTRE - v["boss"]["z"])))
            events["height clamped"] += final["height_offset"] in (-100, 500)
            events["move-y capped"] += final["move_y"] == 0xFF000
            events["radius snapped"] += final["distance_squared"] == 0x100000
    lines = []
    for kind, name in ((K_RETREAT, "F01846 retreat"), (K_READY, "F01847 ready"), (K_TRACKING, "F01848 tracking"),
                       (K_BEHAVIOR, "F01849 behaviour")):
        ranked = sorted(paths[kind].items(), key=lambda item: -item[1])
        lines.append(f"{name} paths: " + ", ".join(f"{path} {count}" for path, count in ranked[:12]))
    lines.append("events: " + ", ".join(f"{name} {count}" for name, count in events.items()))
    return lines
