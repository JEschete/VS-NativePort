"""F01638, F01639, F01640, F01643 (6FC00.c:646, 660, 679, 983) AI perception vs ActorPerception.cpp.

Kind 0 runs func_800D87E8 (target perception, with the inlined occlusion checks F01638 and F01639)
with the squared distance, the facing cosine and its helper, the arctangent, the cosine, the sine
and the square root followed. The terrain segment test func_800E44E8 is a stub: each call records
its two points, its heights and whether it got the target model's and the observer's positions,
and returns a queued answer. The observer's phase, relation words and distances are compared.

Kind 1 runs func_800D8EF4 (the perception pass) over up to four listed actors with all fourteen
callees stubbed: each records its arguments and the actor's fields, and the elapsed-tick and
candidate-readiness calls return queued answers. The room, the engagement flag, the last budget
tick and every listed actor's fields are compared; the battle actor's other command bits and the
navigation word's other bits must keep their values.

Vectors keep target, listed and held IDs inside their tables, give every listed actor a model, an
AI state, a link and stats, keep the tile flags' coordinates inside the 32-tile grid and the
history index inside its eight samples; the native refuses the rest where the original would read
past a table or through a null pointer. Distances sit at the near, far and blocking bounds,
listed actors stand across the line of sight, the view cosine sits at both ends, and relation
words carry timers and counters at their expiry and decay marks.
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
    s16,
    s32,
    store_problems,
)

ID = "F01638, F01639, F01640, F01643"
ENTRY, END = 0x800D87E8, 0x800D9538
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "perception_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\ActorPerception.cpp",
    r"Native\Game\src\Battle\Ai\MovementBehavior.cpp",
    r"Native\Game\src\Battle\Ai\BlockedMovement.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1640
MUTANT_VECTORS = 768

TARGET_PERCEPTION = 0x800D87E8
PASS = 0x800D8EF4
K_TARGET, K_PASS = 0, 1

# Kind 0 followed routines and the terrain stub.
SQUARED_DISTANCE = 0x800E4624
FACING_COSINE = 0x800E437C
FACING_HELPER = 0x80041AF4
ARCTANGENT = 0x8004137C
COSINE = 0x80040F24
SINE = 0x80040F28
SQUARE_ROOT = 0x80041B94
TERRAIN = 0x800E44E8

# Kind 1 stubs, by call code.
DISENGAGE, RESET, UNLINK, AWARENESS, TIMERS = 0x800D82CC, 0x800D821C, 0x800DBF00, 0x800D8280, 0x800E4358
BUILDUP, ELAPSED, REFRESH_TILE, RECOVER = 0x800DBFE4, 0x800E45B4, 0x800E48F8, 0x800E2CCC
FORGET, REACTION, RESOURCES, READINESS = 0x800E4CE8, 0x800E48A8, 0x800E4F14, 0x800D85D8
(C_DISENGAGE, C_RESET, C_UNLINK, C_AWARENESS, C_TIMERS, C_BUILDUP, C_TARGET, C_ELAPSED, C_REFRESH_TILE,
 C_RECOVER, C_FORGET, C_REACTION, C_RESOURCES, C_READINESS) = range(14)
CALL_NAMES = ("disengage", "reset", "unlink", "awareness", "timers", "buildup", "target", "elapsed",
              "refresh-tile", "recover", "forget", "reaction", "resources", "readiness")
# Added to a call's code when the original passed something other than the actor's AI state.
NOT_THE_ACTOR = 100

# Globals.
BATTLE_ACTORS = 0x800F1928
AI_STATES = 0x800F5878
GIMMICKS = 0x800F45E0
RECORDS_TABLE, RECORD_SLOTS = 0x800F4538, 17
ROOM_POINTER = 0x800F58BC
ENGAGED = 0x800F58E0
BUDGET = 0x800F16F4
SECOND_TARGET = 0x800F58D4
SCRATCH = 0x1F800000
FRAME_ACTORS = 0x1F80037C
FRAME_FLAGS = 0x1F8003C0
LAST_TICK = 0x1F8003FC
SLOTS = 16
STACK_SPAN = (0x80400000 - 0x4000, 0x80400000)

# Kind 0 layout.
DATA = 0x80500000
OBSERVER = DATA
POSE = DATA + 0x300
AI_STATE, FRAME_STATE, STATE_STRIDE = DATA + 0x400, DATA + 0x1000, 0xC0
MODEL, MODEL_STRIDE = DATA + 0x1C00, 0x80
NODE, NODE_STRIDE = DATA + 0x2400, 0x50
FLAGS, GRID = DATA + 0x2A00, 32
GIMMICK = DATA + 0x3380

# Kind 1 layout.
ROOM = DATA + 0x3200
PASS_STATE, PASS_STATE_STRIDE = DATA + 0x3400, 0x200
OBJECT, OBJECT_STRIDE = DATA + 0x3C00, 0x600
PASS_NODE, PASS_NODE_STRIDE = DATA + 0x5400, 0x40
STATS, STATS_STRIDE = DATA + 0x5600, 0xA00
POINT, POINT_STRIDE = DATA + 0x7E00, 0x10
LINK_TILE = DATA + 0x7E80
RECORD, RECORD_STRIDE = DATA + 0x8000, 0x60
DATA_SIZE = 0x8800
MAX_LISTED = 4

# name, offset, size, signed
OBSERVER_FIELDS = (
    ("decays", 0x22, 1, False), ("ignores", 0x25, 1, False), ("tile", 0x34, 4, False), ("x", 0x4C, 2, True),
    ("y", 0x4E, 2, True), ("z", 0x50, 2, True), ("point", 0x82, 2, True), ("id", 0x88, 1, False),
    ("near", 0xA4, 4, False), ("view", 0xA8, 4, False), ("far", 0xAC, 4, False), ("body", 0xB8, 2, True),
    ("phase", 0x12B, 1, False),
)
POSE_FIELDS = (("x", 0x1C, 2, True), ("y", 0x1E, 2, True), ("z", 0x20, 2, True), ("facing", 0x26, 2, True),
               ("look_yaw", 0x14, 2, True))
TARGET_MODEL_FIELDS = (("word8", 0x08, 4, False), ("x", 0x1C, 2, True), ("y", 0x1E, 2, True),
                       ("z", 0x20, 2, True), ("tile", 0x5C, 4, False), ("last_tile", 0x60, 4, False))
SHAPE_FIELDS = (("point", 0x82, 2, True), ("radius", 0x92, 2, False), ("body", 0xB8, 2, True))
RELATIONS, DISTANCES = 0x1A4, 0x1E4
PHASE = 0x12B
TERRAIN_RECORD = 9
LOCOMOTING = 0x180000
MARKED = 0x2000

ACTOR_FIELDS = (
    ("stayed", 0x08, 1, False), ("alert", 0x0E, 1, False), ("skipped", 0x14, 1, False),
    ("alerted", 0x18, 1, False), ("holds", 0x1C, 1, False), ("partial", 0x1D, 1, False),
    ("tile", 0x34, 4, False), ("previous_tile", 0x38, 4, True), ("memory", 0x40, 2, False),
    ("detour_step_frames", 0x45, 1, False), ("stall", 0x46, 2, False), ("x", 0x4C, 2, True),
    ("y", 0x4E, 2, True), ("z", 0x50, 2, True), ("pad", 0x52, 2, True), ("x0", 0x70, 4, False),
    ("x1", 0x74, 4, False), ("z0", 0x78, 4, False), ("z1", 0x7C, 4, False), ("index", 0x80, 1, False),
    ("mask", 0x81, 1, False), ("action_delay", 0x90, 2, False), ("detour_step", 0xFA, 1, False),
    ("detour_target", 0xFB, 1, False), ("fine_tile", 0x120, 4, False), ("observed", 0x124, 1, False),
    ("movement_wait", 0x156, 2, False), ("recovery", 0x19A, 1, False),
)
READ_ONLY_FIELDS = (
    ("alert_rule", 0x2F, 1, False), ("remembered", 0x3C, 4, False), ("flee", 0x114, 2, False),
    ("awareness", 0x12A, 1, False), ("action_status", 0x131, 1, False), ("marker", 0x138, 2, False),
    ("override", 0x18C, 4, False),
)
ACTOR_NAMES = [name for name, *_ in ACTOR_FIELDS]
READ_ONLY_NAMES = [name for name, *_ in READ_ONLY_FIELDS]
SNAPSHOT_NAMES = [*ACTOR_NAMES, "navigation_counter", "command_flags"]
NAVIGATION = 0x190
NAVIGATION_COUNTER = 0x7F << 5
COMMAND_FLAGS = 0x03000000
CALL_RECORD = 4 + len(SNAPSHOT_NAMES)
ROOM_NAMES = ("readiness", "approach", "engaged", "last_tick")

MUTANTS = {
    # func_800D87E8
    "no-state-height-minus-0x30": {0x800D88A4: 0x2407FFD0},
    "locomoting-height-bit-19-only": {0x800D8850: 0x3C030008},
    "moving-height-not-negated": {0x800D8880: 0x00403821},
    "distance-not-stored": {0x800D88D4: 0x00000000},
    "gimmick-ignored": {0x800D88EC: 0x00000000},
    "tile-mask-layer": {0x800D88FC: 0x34A5FFFF},
    "last-tile-ignored": {0x800D893C: 0x10000009},
    "shared-tile-no-point-bit": {0x800D8954: 0x34840000},
    "phase-not-stored": {0x800D8970: 0x00000000},
    "phase-parity-swapped": {0x800D8978: 0x14400018},
    "body-default-0x40": {0x800D899C: 0x24060040},
    "body-bit-29": {0x800D89C8: 0x00021740},
    "point-default-0x80": {0x800D8A18: 0x24060080},
    "point-not-magnitude": {0x800D8A0C: 0x00000000},
    "observer-point-not-magnitude": {0x800D8A2C: 0x00000000},
    "point-clear-not-implying-body": {0x800D8A80: 0x00000000},
    "look-yaw-ignored": {0x800D8ABC: 0x00403021},
    "view-unsigned": {0x800D8AD0: 0x0044102B},
    "self-not-visible": {0x800D8AF4: 0x34650001},
    "far-signed": {0x800D8B10: 0x0043102A},
    "ignore-body-line-ignored": {0x800D8B3C: 0x00000000},
    "timer-not-decremented": {0x800D8B74: 0x00000000},
    "timer-not-forcing-active": {0x800D8B88: 0x00000000},
    "near-reversed": {0x800D8BDC: 0x0062102B},
    "marked-bit-0x1000": {0x800D8C4C: 0x30421000},
    "flag-row-32": {0x800D8C38: 0x00021140},
    "radius-default-64": {0x800D8CB8: 0x24160040},
    "frame-radius-ignored": {0x800D8CF0: 0x00000000},
    "observer-not-skipped": {0x800D8D10: 0x00000000},
    "target-not-skipped": {0x800D8D18: 0x00000000},
    "nearer-reversed": {0x800D8D30: 0x0048102B},
    "behind-not-skipped": {0x800D8D6C: 0x00000000},
    "offset-shift-11": {0x800D8D94: 0x000812C3},
    "offset-not-magnitude": {0x800D8DA0: 0x00000000},
    "blocked-bit-26": {0x800D8DDC: 0x00081680},
    "self-decays": {0x800D8DF0: 0x00000000},
    "decay-without-frame": {0x800D8E08: 0x00000000},
    "class-timer-cleared": {0x800D8E34: 0x00000000},
    "previous-class-not-restored": {0x800D8E48: 0x00000000},
    "decay-always": {0x800D8E54: 0x00000000},
    "hostile-class-2": {0x800D8E60: 0x24030002},
    "counter-not-reset": {0x800D8E7C: 0x00000000},
    "decay-at-112": {0x800D8EA0: 0x28427001},
    "decay-to-class-2": {0x800D8EB4: 0x34840080},
    # func_800D8EF4
    "engaged-not-cleared": {0x800D8F20: 0x00000000},
    "readiness-ceiling-0xffff": {0x800D8F28: 0x3402FFFE},
    "approach-not-cleared": {0x800D8F34: 0x00000000},
    "command-flags-kept": {0x800D8F74: 0x00000000},
    "command-flags-mask-bit-26": {0x800D8F54: 0x3C03F8FF},
    "partial-not-cleared": {0x800D8F88: 0x00000000},
    "action-delay-not-copied": {0x800D8F9C: 0x00000000},
    "disengage-bit-4": {0x800D8FA0: 0x32220010},
    "held-not-skipped": {0x800D8FE0: 0x00000000},
    "scripted-not-skipped": {0x800D8FF4: 0x00000000},
    "override-not-skipped": {0x800D9004: 0x00000000},
    "running-ignored": {0x800D902C: 0x30420000},
    "movement-mode-bit-18-ignored": {0x800D9018: 0x30840003},
    "id-0-perceives": {0x800D903C: 0x00000000},
    "forced-not-partial": {0x800D904C: 0x00000000},
    "budget-reversed": {0x800D9074: 0x0043182A},
    "last-tick-not-stored": {0x800D9084: 0x00000000},
    "alert-marker-5": {0x800D90A4: 0x24020005},
    "alerted-not-cleared": {0x800D90C8: 0x00000000},
    "recovery-not-decremented": {0x800D90E4: 0x00000000},
    "stall-floor-3": {0x800D90FC: 0x2C620003},
    "skipped-not-cleared": {0x800D9120: 0x00000000},
    "link-tile-mask-layer": {0x800D911C: 0x3484FFFF},
    "stall-0xa8d": {0x800D9138: 0x24020A8D},
    "detour-target-17": {0x800D9168: 0x24020011},
    "detour-frames-41": {0x800D9170: 0x24020029},
    "previous-tile-not-stored": {0x800D917C: 0x00000000},
    "detour-steps-6": {0x800D91B4: 0x2C420006},
    "detour-step-wraps-to-0": {0x800D91C0: 0xA20000FA},
    "stayed-not-set": {0x800D91CC: 0x00000000},
    "left-tile-not-forgotten": {0x800D91E0: 0x00000000},
    "no-command-9": {0x800D91F0: 0x24040009},
    "reaction-delay-0x20": {0x800D921C: 0x24060020},
    "history-command-0": {0x800D922C: 0x34A52601},
    "fine-z-shift-5": {0x800D925C: 0x00021142},
    "position-pad-not-stored": {0x800D9254: 0xA6020050},
    "stalled-mask-0x7f": {0x800D9280: 0x2402007F},
    "recovery-31": {0x800D92BC: 0x2402001F},
    "history-z-from-y": {0x800D92E4: 0x9203004E},
    "history-index-mod-16": {0x800D9304: 0x3063000F},
    "mask-kept": {0x800D9318: 0x00000000},
    "navigation-not-decremented": {0x800D9348: 0x00000000},
    "second-target-ignored": {0x800D9368: 0x10000005},
    "second-target-twice": {0x800D9374: 0x2A220003},
    "readiness-ignored": {0x800D9390: 0x00000000},
    "low-hp-factor-2": {0x800D93C8: 0x00041040},
    "half-hp-factor-4": {0x800D93D8: 0x00041080},
    "flee-ignored": {0x800D93F8: 0x00000000},
    "visible-bit-4": {0x800D942C: 0x00031102},
    "alert-class-2": {0x800D9410: 0x24050002},
    "fleeing-status-3": {0x800D945C: 0x24020003},
    "movement-wait-kept": {0x800D9480: 0x00000000},
    "alert-not-stored": {0x800D9478: 0x00000000},
    "memory-not-decremented": {0x800D94B4: 0x00000000},
    "remembered-tile-always-same": {0x800D94F4: 0x10000003},
    "negative-previous-forgets": {0x800D949C: 0x00000000},
}

def tile_word(rng: random.Random, near: int | None = None) -> int:
    if near is not None and rng.random() < 0.5:
        return near & 0xFF00FF | rng.getrandbits(8) << 8 | rng.getrandbits(8) << 24
    return rng.randrange(GRID) | rng.getrandbits(8) << 8 | rng.randrange(GRID) << 16 | rng.getrandbits(8) << 24


def relation_word(rng: random.Random) -> int:
    if rng.random() < 0.1:
        return rng.getrandbits(32)
    classification = rng.choices((0, 1, 2, 3), (15, 15, 10, 60))[0]
    counter = rng.choice((0, 1, 118, 119, 120, 121, 126, 127, rng.randrange(128)))
    timer = rng.choice((0, 0, 0, 1, 2, 31, rng.randrange(32)))
    class_timer = rng.choice((0, 0, 0, 1, 2, 127, rng.randrange(128)))
    return (rng.getrandbits(6) | classification << 6 | counter << 8 | timer << 15 | class_timer << 20
            | rng.getrandbits(1) << 27 | rng.getrandbits(2) << 28 | rng.getrandbits(2) << 30)


def height(rng: random.Random) -> int:
    return rng.choice((0, 0x40, 0x80, -0x40, -0x80, rng.randrange(-0x200, 0x200), s16(rng.getrandbits(16))))


def bound(rng: random.Random, approximate: int) -> int:
    roll = rng.random()
    if roll < 0.55:
        spread = max(1, approximate >> rng.choice((2, 4, 8, 16)))
        return max(0, approximate + rng.choice((0, -1, 1, rng.randrange(-spread, spread + 1))))
    if roll < 0.7:
        return rng.choice((0, 1, 0xFFFFFFFF, 0x7FFFFFFF, 0x80000000))
    return rng.getrandbits(32) if rng.random() < 0.3 else rng.randrange(max(2, 2 * approximate))


def slot_distance(rng: random.Random, approximate: int) -> int:
    roll = rng.random()
    if roll < 0.5:
        return bound(rng, approximate) & 0x7FFFFFFF
    if roll < 0.65:
        return rng.randrange(0x100)
    if roll < 0.9:
        return rng.randrange(0x80000000)
    return rng.getrandbits(32)


def clamp16(value: float) -> int:
    return max(-0x8000, min(0x7FFF, round(value)))


def target_vector(rng: random.Random) -> dict:
    target = rng.randrange(SLOTS)
    ox, oy, oz = rng.randrange(-0x3000, 0x3000), rng.randrange(-0x100, 0x100), rng.randrange(-0x3000, 0x3000)
    observer = {
        "decays": rng.choice((0, 0, 1, rng.getrandbits(8))), "ignores": rng.choice((0, 0, 1, rng.getrandbits(8))),
        "tile": tile_word(rng), "x": ox, "y": oy, "z": oz, "point": height(rng),
        "id": target if rng.random() < 0.1 else rng.choice((rng.randrange(SLOTS), rng.getrandbits(8))),
        "view": rng.choice((-4096, 4096, 0, rng.randrange(-4096, 4097), rng.randrange(-4096, 4097), 0x80000000,
                            0x7FFFFFFF, rng.getrandbits(32))) & MASK,
        "body": height(rng), "phase": rng.getrandbits(8),
    }
    near_pose = rng.random() < 0.7
    pose = {"x": ox if near_pose else clamp16(ox + rng.randrange(-0x40, 0x40)), "y": oy,
            "z": oz if near_pose else clamp16(oz + rng.randrange(-0x40, 0x40)),
            "facing": s16(rng.getrandbits(16)) if rng.random() < 0.3 else rng.randrange(0x1000),
            "look_yaw": rng.choice((0, 0, rng.randrange(-0x400, 0x400), s16(rng.getrandbits(16))))}
    actors = []
    for i in range(SLOTS):
        reach = rng.choice((0x100, 0x800, 0x1400))
        locomoting = rng.random() < 0.4
        word8 = rng.getrandbits(32) & ~LOCOMOTING | (rng.choice((0x80000, 0x100000, 0x180000)) if locomoting else 0)
        actors.append({
            "present": i in (0, target) or rng.random() < 0.7,
            "x": clamp16(ox + rng.randrange(-reach, reach)), "y": oy + rng.randrange(-0x100, 0x100),
            "z": clamp16(oz + rng.randrange(-reach, reach)),
            "tile": tile_word(rng, observer["tile"] if rng.random() < 0.4 else None),
            "last_tile": tile_word(rng, observer["tile"] if rng.random() < 0.3 else None), "word8": word8,
            "ai": rng.random() < 0.8, "ai_body": height(rng), "ai_point": height(rng),
            "ai_radius": rng.getrandbits(16), "frame": rng.random() < 0.8, "frame_body": height(rng),
            "frame_point": height(rng),
            "frame_radius": rng.choice((0, 1, 20, 63, 64, 100, 0x200, rng.randrange(0x400), rng.getrandbits(16))),
            "gimmick": rng.random() < 0.08,
        })
    t = actors[target]
    approximate = (t["x"] - ox) ** 2 + (t["y"] - oy) ** 2 + (t["z"] - oz) ** 2
    observer.update(near=bound(rng, approximate), far=bound(rng, approximate))
    present = [i for i in range(1, SLOTS) if actors[i]["present"]]
    members = [0, *rng.sample(present, rng.randrange(len(present) + 1))]
    v = {"kind": K_TARGET, "target": target, "observer": observer, "pose": pose, "actors": actors,
         "relations": [relation_word(rng) for _ in range(SLOTS)], "members": members,
         "marked": rng.random() < 0.5, "flag_bits": rng.getrandbits(16),
         "terrain": [rng.choice((0, 0, 1, 1, 2, 0x100))], "noise_seed": rng.getrandbits(32)}
    v["distances"] = [slot_distance(rng, (a["x"] - ox) ** 2 + (a["y"] - oy) ** 2 + (a["z"] - oz) ** 2)
                      for a in actors]
    if rng.random() < 0.5:
        across_line(rng, v)
    return v


# Listed actors standing near the observer's line to the target, their perpendicular offsets at the
# target's radius, with their distance slots at their true squared distances.
def across_line(rng: random.Random, v: dict) -> None:
    target, pose, actors = v["target"], v["pose"], v["actors"]
    t = actors[target]
    lx, lz = t["x"] - pose["x"], t["z"] - pose["z"]
    length = math.hypot(lx, lz)
    if length < 4:
        return
    radius = t["frame_radius"] if t["frame"] else 63
    observer = v["observer"]["id"]
    candidates = [i for i in v["members"] if i not in (target, observer)]
    chosen = rng.sample(candidates, min(len(candidates), rng.choice((1, 1, 2))))
    # The observer's own slot on the line is skipped only by its id.
    if observer in v["members"] and observer != target and rng.random() < 0.4:
        chosen.insert(0, observer)
    for candidate in chosen:
        along = rng.uniform(0.05, 0.95) * length
        offset = rng.choice((radius - 2, radius - 1, radius, radius + 1, radius + 2,
                             rng.uniform(-2 * radius, 2 * radius))) * rng.choice((1, -1))
        a = actors[candidate]
        a["x"] = clamp16(pose["x"] + (along * lx - offset * lz) / length)
        a["z"] = clamp16(pose["z"] + (along * lz + offset * lx) / length)
        squared = (a["x"] - pose["x"]) ** 2 + (a["z"] - pose["z"]) ** 2
        v["distances"][candidate] = max(0, squared + rng.choice((0, 0, -1, 1, rng.randrange(-64, 65))))

def listed_actor(rng: random.Random, actor_id: int, records: list, tracked: bool = False) -> dict:
    tile = tile_word(rng)
    held = rng.randrange(RECORD_SLOTS)
    record = records[held]
    remembered = tile_word(rng, record if record is not None and rng.random() < 0.7 else None)
    previous = -rng.randrange(1, 0x80000000) if rng.random() < 0.1 else \
        held << 24 | rng.getrandbits(24)
    x0, z0 = rng.getrandbits(32), rng.getrandbits(32)
    stalled = rng.random() < 0.15
    state = {
        "stayed": rng.getrandbits(8), "alert": rng.getrandbits(8), "skipped": rng.getrandbits(8),
        "alerted": rng.choice((0, 0, 1, rng.getrandbits(8))), "holds": rng.choice((0, 0, 1, rng.getrandbits(8))),
        "partial": rng.getrandbits(8), "tile": tile, "previous_tile": previous,
        "memory": rng.choice((0, 1, 2, 3, rng.getrandbits(16))),
        "detour_step_frames": rng.choice((0, 1, 2, 40, rng.getrandbits(8))),
        "stall": rng.choice((0, 1, 2, 3, 0xA8C, rng.getrandbits(16))),
        "x": s16(rng.getrandbits(16)), "y": s16(rng.getrandbits(16)), "z": s16(rng.getrandbits(16)),
        "pad": s16(rng.getrandbits(16)), "x0": x0, "x1": x0 if stalled else rng.getrandbits(32), "z0": z0,
        "z1": z0 if stalled and rng.random() < 0.8 else rng.getrandbits(32), "index": rng.randrange(8),
        "mask": 0xFF if stalled else rng.choice((0, 0xFF, rng.getrandbits(8))),
        "action_delay": rng.getrandbits(16), "detour_step": rng.choice((0, 1, 3, 4, 5, 6, rng.getrandbits(8))),
        "detour_target": rng.getrandbits(8), "fine_tile": rng.getrandbits(32),
        "observed": rng.choice((8, 9, 10, 13, 16, 0, rng.getrandbits(8))),
        "movement_wait": rng.choice((0, 1, rng.getrandbits(16))),
        "recovery": rng.choice((0, 0, 0, 1, 2, rng.getrandbits(8))),
        "alert_rule": rng.choice((0, 1, 2, 3, 3, rng.getrandbits(8))), "remembered": remembered,
        "flee": rng.choice((0, 0, 1, rng.getrandbits(16))), "awareness": rng.choice((0, 1, rng.getrandbits(8))),
        "action_status": rng.choice((0, 2, rng.getrandbits(8))), "marker": rng.choice((6, 6, 0, rng.getrandbits(16))),
        "override": 0 if rng.random() < 0.85 else rng.getrandbits(32) | 1,
    }
    moving = rng.random() < 0.15 and not tracked
    max_hp = rng.choice((0, 1, 4, 100, 999, rng.getrandbits(16)))
    current = rng.choice((max_hp // 4 + rng.choice((-1, 0, 1)), max_hp // 2 + rng.choice((-1, 0, 1)),
                          rng.getrandbits(16))) & 0xFFFF
    if tracked:
        # Past every command and frame gate, on the detour-step and HP-rule edges.
        state.update(alerted=0, recovery=0, override=0, stall=rng.choice((0, 1, 2, 3)),
                     detour_step_frames=rng.choice((1, 1, 2)), detour_step=rng.choice((0, 3, 4, 4, 5, 0xFF)),
                     alert_rule=rng.choice((1, 1, 2, 2, 3)), action_status=rng.choice((0, 0, 1, 3)))
    actor = {
        "id": actor_id,
        "command_state": rng.choice((0, 0, 0, 0x20, 0x80, 0x40, 0x10, 0x08, rng.getrandbits(8))),
        "opcode": rng.choice((8, 9, 10, 13, 16, 0, rng.getrandbits(8))), "command_byte_a": rng.getrandbits(8),
        "command_byte_b": rng.getrandbits(8), "recovery_timer": rng.getrandbits(16), "current_hp": current,
        "max_hp": max_hp,
        "model_word": rng.getrandbits(32) & ~0x801F0000
        | (rng.choice((1, 2, 4, 4, 8, 16, rng.getrandbits(5))) << 16 if moving else 0)
        | (0x80000000 if rng.random() < 0.15 else 0),
        "running": rng.getrandbits(16) & ~0x8000 | (0x8000 if rng.random() < 0.1 else 0),
        "model_tile": tile_word(rng, tile if rng.random() < 0.5 else None),
        "link_tile": tile_word(rng, tile if rng.random() < 0.6 else None),
        "point": [rng.getrandbits(32), rng.getrandbits(32)], "state": state,
        "navigation": rng.getrandbits(32) & ~NAVIGATION_COUNTER | rng.choice((0, 1, 2, rng.getrandbits(7))) << 5,
        "relations": [relation_word(rng) | (0x60 if rng.random() < 0.2 else 0) for _ in range(SLOTS)],
    }
    if tracked:
        actor.update(command_state=0, model_word=actor["model_word"] & ~0x801F0000,
                     running=actor["running"] & ~0x8000,
                     link_tile=tile if rng.random() < 0.8 else actor["link_tile"])
    return actor


def pass_vector(rng: random.Random) -> dict:
    count = rng.choice((1, 2, 3, 4, 4))
    tracking = rng.random() < 0.35
    ids = rng.sample(range(SLOTS), count)
    if rng.random() < 0.3 and 0 not in ids:
        ids[0] = 0
    records = [None if rng.random() < 0.2 else tile_word(rng) for _ in range(RECORD_SLOTS)]
    elapsed = [rng.choice((0, 1, 100, 1000, rng.randrange(0x10000), s32(rng.getrandbits(32))))
               for _ in range(count)]
    budget = elapsed[0] + rng.choice((-1, 0, 1, 1000, -1000)) if rng.random() < 0.6 \
        else s32(rng.getrandbits(32))
    if tracking:
        budget = 0x7FFFFFFF
    return {
        "kind": K_PASS,
        "actors": [listed_actor(rng, actor_id, records, tracking and rng.random() < 0.75) for actor_id in ids],
        "records": records, "elapsed": elapsed, "budget": s32(budget),
        "second": rng.choice((0, 0, 1, 5, 15, s32(rng.getrandbits(32)))), "last_tick": s32(rng.getrandbits(32)),
        "readiness_answers": [0 if tracking else rng.choice((0, 0, 1, 0x100)) for _ in range(count)],
        "readiness": rng.choice((0xFFFD, 0xFFFE, 0xFFFF, 0, rng.getrandbits(16))),
        "approach": s32(rng.getrandbits(32)), "engaged": s32(rng.getrandbits(32)),
        "noise_seed": rng.getrandbits(32),
    }


def vector(rng: random.Random, number: int) -> dict:
    return target_vector(rng) if number % 2 == 0 else pass_vector(rng)

def field_values(fields, values: dict) -> list[int]:
    """Each field's value as memory holds it: truncated to its size, sign-extended when signed."""
    out = []
    for name, _, size, signed in fields:
        value = values[name] & ((1 << size * 8) - 1)
        out.append(value - (1 << size * 8) if signed and value >> (size * 8 - 1) else value)
    return out


def writer(block: bytearray):
    def place(base: int, fields, values: dict) -> None:
        for name, offset, size, _ in fields:
            word(base + offset, values[name], size)

    def word(address: int, value: int, size: int = 4) -> None:
        block[address - DATA:address - DATA + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")

    return place, word


def model_values(a: dict) -> dict:
    return {"word8": a["word8"], "x": a["x"], "y": a["y"], "z": a["z"], "tile": a["tile"], "last_tile": a["last_tile"]}


def target_block(v: dict) -> bytes:
    block = bytearray(random.Random(v["noise_seed"]).randbytes(DATA_SIZE))
    place, word = writer(block)
    place(OBSERVER, OBSERVER_FIELDS, v["observer"])
    for i in range(SLOTS):
        word(OBSERVER + RELATIONS + 4 * i, v["relations"][i])
        word(OBSERVER + DISTANCES + 4 * i, v["distances"][i])
    word(OBSERVER + 0x58, POSE)
    place(POSE, POSE_FIELDS, v["pose"])
    for i, a in enumerate(v["actors"]):
        place(MODEL + i * MODEL_STRIDE, TARGET_MODEL_FIELDS, model_values(a))
        place(AI_STATE + i * STATE_STRIDE, SHAPE_FIELDS,
              {"point": a["ai_point"], "radius": a["ai_radius"], "body": a["ai_body"]})
        place(FRAME_STATE + i * STATE_STRIDE, SHAPE_FIELDS,
              {"point": a["frame_point"], "radius": a["frame_radius"], "body": a["frame_body"]})
        word(NODE + i * NODE_STRIDE + 4, i)
        word(NODE + i * NODE_STRIDE + 0x44, MODEL + i * MODEL_STRIDE)
    members = v["members"]
    for k, i in enumerate(members):
        word(NODE + i * NODE_STRIDE, NODE + members[k + 1] * NODE_STRIDE if k + 1 < len(members) else 0)
    tile = v["actors"][v["target"]]["tile"]
    flag = v["flag_bits"] & ~MARKED | (MARKED if v["marked"] else 0)
    word(FLAGS + (tile >> 16 & 0xFF) * GRID * 2 + (tile & 0xFF) * 2, flag, 2)
    return bytes(block)


def slot_addresses(slot: int) -> dict:
    return {"state": PASS_STATE + slot * PASS_STATE_STRIDE, "object": OBJECT + slot * OBJECT_STRIDE,
            "node": PASS_NODE + slot * PASS_NODE_STRIDE, "stats": STATS + slot * STATS_STRIDE,
            "point": POINT + slot * POINT_STRIDE, "link": LINK_TILE + slot * 4}


def node_word(a: dict) -> int:
    return a["command_state"] | a["opcode"] << 8 | a["command_byte_a"] << 16 | a["command_byte_b"] << 24


def pass_block(v: dict) -> bytes:
    block = bytearray(random.Random(v["noise_seed"]).randbytes(DATA_SIZE))
    place, word = writer(block)
    word(ROOM + 0x18, v["readiness"], 2)
    word(ROOM + 0x24, v["approach"])
    count = len(v["actors"])
    for slot, a in enumerate(v["actors"]):
        at = slot_addresses(slot)
        place(at["state"], ACTOR_FIELDS, a["state"])
        place(at["state"], READ_ONLY_FIELDS, a["state"])
        word(at["state"] + NAVIGATION, a["navigation"])
        for i in range(SLOTS):
            word(at["state"] + RELATIONS + 4 * i, a["relations"][i])
        for offset, pointer in ((0x54, at["node"]), (0x58, at["object"]), (0x60, at["point"]), (0x64, at["link"])):
            word(at["state"] + offset, pointer)
        word(at["node"], slot_addresses(slot + 1)["node"] if slot + 1 < count else 0)
        word(at["node"] + 4, a["id"])
        word(at["node"] + 8, node_word(a))
        word(at["node"] + 0x3C, at["stats"])
        word(at["stats"] + 0x18, a["current_hp"], 2)
        word(at["stats"] + 0x1A, a["max_hp"], 2)
        word(at["stats"] + 0x954, a["recovery_timer"], 2)
        word(at["object"] + 8, a["model_word"])
        word(at["object"] + 0x5C, a["model_tile"])
        word(at["object"] + 0x5AC, a["running"], 2)
        word(at["point"], a["point"][0])
        word(at["point"] + 4, a["point"][1])
        word(at["link"], a["link_tile"])
    for j, tile in enumerate(v["records"]):
        if tile is not None:
            word(RECORD + j * RECORD_STRIDE + 0x5C, tile)
    return bytes(block)

def target_inputs(v: dict) -> list[int]:
    o = v["observer"]
    inputs = [K_TARGET, v["target"], *field_values(OBSERVER_FIELDS, o), *v["relations"], *v["distances"],
              *field_values(POSE_FIELDS, v["pose"])]
    for a in v["actors"]:
        inputs += [int(a["present"]), *field_values(TARGET_MODEL_FIELDS[1:], model_values(a)),
                   int(a["word8"] & LOCOMOTING != 0), int(a["ai"]),
                   *field_values(SHAPE_FIELDS, {"point": a["ai_point"], "radius": a["ai_radius"], "body": a["ai_body"]}),
                   int(a["frame"]), *field_values(SHAPE_FIELDS, {"point": a["frame_point"], "radius": a["frame_radius"],
                                                                 "body": a["frame_body"]}), int(a["gimmick"])]
    return [*inputs, len(v["members"]), *v["members"], int(v["marked"]), len(v["terrain"]), *v["terrain"]]


def run_target(machine: Machine, v: dict):
    machine.images.append(Image(target_block(v), DATA))
    for i, a in enumerate(v["actors"]):
        machine.put(BATTLE_ACTORS + 4 * i, NODE + i * NODE_STRIDE if a["present"] else 0)
        machine.put(RECORDS_TABLE + 4 * i, MODEL + i * MODEL_STRIDE if a["present"] else 0)
        machine.put(AI_STATES + 4 * i, AI_STATE + i * STATE_STRIDE if a["ai"] else 0)
        machine.put(GIMMICKS + 4 * i, GIMMICK if a["gimmick"] else 0)
    machine.put(RECORDS_TABLE + 4 * SLOTS, 0)
    machine.map_zero(SCRATCH, 0x400)
    for i, a in enumerate(v["actors"]):
        machine.put(FRAME_ACTORS + 4 * i, FRAME_STATE + i * STATE_STRIDE if a["frame"] else 0)
    machine.put(FRAME_FLAGS, FLAGS)
    machine.follow.update({SQUARED_DISTANCE, SQUARED_DISTANCE + 0x3C, FACING_COSINE, FACING_HELPER, ARCTANGENT,
                           COSINE, SINE, SQUARE_ROOT})
    calls: list[list[int]] = []
    answers = list(v["terrain"])
    target_position = MODEL + v["target"] * MODEL_STRIDE + 0x1C

    def terrain(m: Machine) -> None:
        start, end = m.r[4], m.r[5]
        calls.append([int(start != target_position or end != OBSERVER + 0x4C),
                      *(m.load(start + k, 2, True) for k in (0, 2, 4)), *(m.load(end + k, 2, True) for k in (0, 2, 4)),
                      s32(m.r[6]), s32(m.r[7])])
        m.r[2] = (answers.pop(0) if answers else 0) & MASK

    machine.stub(TERRAIN, terrain)
    machine.r[4], machine.r[5] = OBSERVER, v["target"]
    machine.execute(TARGET_PERCEPTION)
    observation = {
        "phase": machine.load(OBSERVER + PHASE, 1),
        "relations": [machine.load(OBSERVER + RELATIONS + 4 * i) for i in range(SLOTS)],
        "distances": [machine.load(OBSERVER + DISTANCES + 4 * i) for i in range(SLOTS)],
        "calls": calls, "stores": machine.stores,
    }
    return observation, target_inputs(v)


def snapshot(m: Machine, slot: int) -> list[int]:
    at = slot_addresses(slot)
    return [*m.load_fields(at["state"], ACTOR_FIELDS, ACTOR_NAMES),
            m.load(at["state"] + NAVIGATION) >> 5 & 0x7F, m.load(at["node"] + 0xB, 1) & 3]


def pass_inputs(v: dict) -> list[int]:
    inputs = [K_PASS, len(v["actors"]), v["readiness"], v["approach"], v["engaged"], v["last_tick"], v["budget"],
              v["second"]]
    for tile in v["records"]:
        inputs += [0, 0] if tile is None else [1, tile]
    inputs += [len(v["elapsed"]), *v["elapsed"], len(v["readiness_answers"]), *v["readiness_answers"]]
    for a in v["actors"]:
        word = a["model_word"]
        inputs += [a["id"], a["command_state"], a["opcode"], a["command_byte_b"] & 3, a["recovery_timer"],
                   a["current_hp"], a["max_hp"], int(word >> 16 & 0x1F != 0), int(a["running"] & 0x8000 != 0),
                   int(word >> 31), a["model_tile"], a["link_tile"], *a["point"],
                   *field_values(ACTOR_FIELDS, a["state"]), *field_values(READ_ONLY_FIELDS[:-1], a["state"]),
                   int(a["state"]["override"] != 0), a["navigation"] >> 5 & 0x7F, *a["relations"]]
    return inputs


def run_pass(machine: Machine, v: dict):
    machine.images.append(Image(pass_block(v), DATA))
    count = len(v["actors"])
    machine.put(ROOM_POINTER, ROOM)
    machine.put(ENGAGED, v["engaged"])
    machine.put(BUDGET, v["budget"])
    machine.put(SECOND_TARGET, v["second"])
    machine.put(BATTLE_ACTORS, PASS_NODE)
    for j, tile in enumerate(v["records"]):
        machine.put(RECORDS_TABLE + 4 * j, 0 if tile is None else RECORD + j * RECORD_STRIDE)
    machine.map_zero(SCRATCH, 0x400)
    for slot, a in enumerate(v["actors"]):
        machine.put(FRAME_ACTORS + 4 * a["id"], slot_addresses(slot)["state"])
    machine.put(LAST_TICK, v["last_tick"])
    states = [slot_addresses(slot)["state"] for slot in range(count)]
    calls: list[list[int]] = []
    elapsed, readiness = list(v["elapsed"]), list(v["readiness_answers"])

    def record(m: Machine, code: int, first: int = 0, second: int = 0, check_actor: bool = True) -> None:
        slot = states.index(m.r[16]) if m.r[16] in states else -1
        if check_actor and m.r[4] != m.r[16]:
            code += NOT_THE_ACTOR
        calls.append([code, slot, first, second, *(snapshot(m, slot) if slot >= 0 else [0] * len(SNAPSHOT_NAMES))])

    def answer(queue: list[int]):
        def handler(m: Machine) -> None:
            m.r[2] = (queue.pop(0) if queue else 0) & MASK
        return handler

    def elapsed_ticks(m: Machine) -> None:
        record(m, C_ELAPSED, check_actor=False)
        answer(elapsed)(m)

    def ready(m: Machine) -> None:
        record(m, C_READINESS)
        answer(readiness)(m)

    for address, code in ((DISENGAGE, C_DISENGAGE), (RESET, C_RESET), (UNLINK, C_UNLINK), (AWARENESS, C_AWARENESS),
                          (TIMERS, C_TIMERS), (BUILDUP, C_BUILDUP), (REFRESH_TILE, C_REFRESH_TILE),
                          (RECOVER, C_RECOVER), (FORGET, C_FORGET), (RESOURCES, C_RESOURCES)):
        machine.stub(address, lambda m, code=code: record(m, code))
    machine.stub(TARGET_PERCEPTION, lambda m: record(m, C_TARGET, s32(m.r[5])))
    machine.stub(REACTION, lambda m: record(m, C_REACTION, s32(m.r[5]), s32(m.r[6])))
    machine.stub(ELAPSED, elapsed_ticks)
    machine.stub(READINESS, ready)
    machine.execute(PASS)
    extra = []
    for slot, a in enumerate(v["actors"]):
        at = slot_addresses(slot)
        if (machine.load(at["state"] + NAVIGATION) ^ a["navigation"]) & ~NAVIGATION_COUNTER & MASK:
            extra.append(f"slot {slot} original navigation word {machine.load(at['state'] + NAVIGATION):#x} "
                         f"from {a['navigation']:#x}")
        if (machine.load(at["node"] + 8) ^ node_word(a)) & ~COMMAND_FLAGS & MASK:
            extra.append(f"slot {slot} original command word {machine.load(at['node'] + 8):#x} from {node_word(a):#x}")
    observation = {
        "room": [machine.load(ROOM + 0x18, 2), machine.load(ROOM + 0x24, 4, True), machine.load(ENGAGED, 4, True),
                 machine.load(LAST_TICK, 4, True)],
        "actors": [snapshot(machine, slot) for slot in range(count)], "calls": calls, "extra": extra,
        "stores": machine.stores,
    }
    return observation, pass_inputs(v)


def run(machine: Machine, v: dict):
    return run_target(machine, v) if v["kind"] == K_TARGET else run_pass(machine, v)

RELATION_NAMES = [f"relation[{i}]" for i in range(SLOTS)]
DISTANCE_NAMES = [f"distance[{i}]" for i in range(SLOTS)]


def target_stores(v: dict) -> list[tuple[int, int]]:
    relation, distance = OBSERVER + RELATIONS + 4 * v["target"], OBSERVER + DISTANCES + 4 * v["target"]
    return [(OBSERVER + PHASE, OBSERVER + PHASE + 1), (relation, relation + 4), (distance, distance + 4), STACK_SPAN]


def pass_stores(v: dict) -> list[tuple[int, int]]:
    spans = [(ROOM + 0x18, ROOM + 0x1A), (ROOM + 0x24, ROOM + 0x28), (ENGAGED, ENGAGED + 4), (LAST_TICK, LAST_TICK + 4),
             STACK_SPAN]
    for slot in range(len(v["actors"])):
        at = slot_addresses(slot)
        spans += [(at["state"] + offset, at["state"] + offset + size) for _, offset, size, _ in ACTOR_FIELDS]
        # The position words are copied whole.
        spans += [(at["state"] + 0x4C, at["state"] + 0x54)]
        spans += [(at["state"] + NAVIGATION, at["state"] + NAVIGATION + 4), (at["node"] + 8, at["node"] + 0xC)]
    return spans


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    if v["kind"] == K_TARGET:
        problems = [] if native[1] == obs["phase"] else [f"phase native {native[1]:#x} original {obs['phase']:#x}"]
        problems += field_problems(RELATION_NAMES, native[2:2 + SLOTS], obs["relations"])
        problems += field_problems(DISTANCE_NAMES, native[2 + SLOTS:2 + 2 * SLOTS], obs["distances"])
        at = 2 + 2 * SLOTS
        calls = [native[at + 1 + i * TERRAIN_RECORD:at + 1 + (i + 1) * TERRAIN_RECORD] for i in range(native[at])]
        problems += call_problems(calls, obs["calls"])
        return problems + store_problems(obs["stores"], target_stores(v))
    problems = field_problems(ROOM_NAMES, native[1:5], obs["room"])
    at, size = 5, len(SNAPSHOT_NAMES)
    for slot, fields in enumerate(obs["actors"]):
        problems += [f"slot {slot} {problem}" for problem in field_problems(SNAPSHOT_NAMES, native[at:at + size], fields)]
        at += size
    calls = [native[at + 1 + i * CALL_RECORD:at + 1 + (i + 1) * CALL_RECORD] for i in range(native[at])]
    problems += call_problems(calls, obs["calls"])
    return problems + obs["extra"] + store_problems(obs["stores"], pass_stores(v))


def describe(image: Image, results: list) -> list[str]:
    events = dict.fromkeys(("shared tile", "body line", "point line", "terrain blocked", "in view", "active",
                            "timer forced", "was active", "visible", "marked", "blocked", "decayed to neutral",
                            "counter reset", "class restored", "self"), 0)
    paths: dict[str, int] = {}
    for v, obs, _native in results:
        if v["kind"] == K_TARGET:
            target = v["target"]
            before, after = v["relations"][target], obs["relations"][target]
            calls = obs["calls"]
            events["shared tile"] += not calls and after & 0x40000004 == 0x40000004
            events["body line"] += bool(calls) and obs["phase"] & 1 == 1
            events["point line"] += bool(calls) and obs["phase"] & 1 == 0
            events["terrain blocked"] += bool(calls) and v["terrain"][0] != 0
            for name, bit in (("in view", 3), ("active", 0), ("was active", 1), ("visible", 5), ("marked", 31),
                              ("blocked", 27)):
                events[name] += after >> bit & 1
            events["timer forced"] += before >> 15 & 0x1F != 0 and target != v["observer"]["id"]
            events["decayed to neutral"] += before >> 6 & 3 == 3 and after >> 6 & 3 == 1
            events["counter reset"] += before >> 8 & 0x7F != 0 and after >> 8 & 0x7F == 0
            events["class restored"] += before >> 20 & 0x7F == 1 and after >> 20 & 0x7F == 0
            events["self"] += target == v["observer"]["id"]
        else:
            signature = "+".join(CALL_NAMES[call[0] % NOT_THE_ACTOR] for call in obs["calls"]) or "none"
            for part in signature.split("+elapsed"):
                paths[part] = paths.get(part, 0) + 1
    ranked = sorted(paths.items(), key=lambda item: -item[1])
    return ["F01638-F01640 target perception: " + ", ".join(f"{name} {count}" for name, count in events.items()),
            "F01643 pass segments: " + ", ".join(f"{path} {count}" for path, count in ranked[:16]),
            f"F01643 distinct pass segments: {len(paths)}"]