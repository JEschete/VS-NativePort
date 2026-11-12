"""F01649-F01654 (71618.c:511-941) candidate movement vs CandidateMovement.cpp.

All six entries call one another's original instructions. The original arctangent,
cosine, sine, signed square root and both GTE squared-distance routines are followed.
External movement, engagement, readiness, terrain and random calls have queued
answers. Every call records its arguments, every mover field, all candidate words
and the room approach counter before applying scripted callback writes. Candidate
handles are stable rank indices, including when a callback changes their contents.
Final fields, candidate words, unrelated bytes and the original's stores are checked.

The domain supplies all candidate/model/height records, a bounded acyclic actor
roster and in-range actor IDs (or the original's 16/no-related-actor sentinels).
Missing frame actors are included. Candidate squares and height-offset geometry
stay nonnegative as signed int32; the original square root indexes before its
table on negative input. Range growth also stays below that limit when fed back
to square root. Invalid original table accesses, negative linked IDs and missing
records belong to the native refusal tests, not this differential.
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
    field_writer,
    packed,
    s16,
    s32,
    store_problems,
)

ID = "F01649, F01650, F01651, F01652, F01653, F01654"
ENTRY, END = 0x800D9E18, 0x800DAA6C
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "candidate_movement_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\CandidateMovement.cpp",
    r"Native\Game\src\Battle\Ai\PlanningPrimitives.cpp",
    r"Native\Game\src\Battle\Ai\DecisionPrimitives.cpp",
    r"Native\Engine\src\Core\PointerSort.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED, MUTANT_VECTORS = 6144, 1649, 1152
ENTRIES = (ENTRY, 0x800DA1D4, 0x800DA4BC, 0x800DA5CC, 0x800DA6A4, 0x800DA738)
FOLLOW = (*ENTRIES, 0x8004137C, 0x80040F24, 0x80040F28, 0x80041B94, 0x800E4624, 0x800E4660)

CALL_NAMES = ("can-approach", "cooldown", "engage", "disengage", "approach", "track", "flee", "wander",
              "move-tile", "find-related", "release", "recover", "project", "dispatch", "random",
              "awareness", "ground")
(C_CAN, C_COOLDOWN, C_ENGAGE, C_DISENGAGE, C_APPROACH, C_TRACK, C_FLEE, C_WANDER, C_MOVE, C_FIND,
 C_RELEASE, C_RECOVER, C_PROJECT, C_DISPATCH, C_RANDOM, C_AWARENESS, C_GROUND) = range(len(CALL_NAMES))
CALLEES = (0x800DB8AC, 0x800DBCB4, 0x800D836C, 0x800D82CC, 0x800DB370, 0x800DB74C, 0x800DB61C,
           0x800DB5B8, 0x800DB5DC, 0x800DBB2C, 0x800E678C, 0x800E2CCC, 0x800E1388, 0x800E50A0,
           0x800E45D4, 0x800D8260, 0x800E42D4)
NOT_THE_ACTOR = 100
SLOTS, CANDIDATE_COUNT, CANDIDATE_SIZE = 16, 4, 20
DATA, STATE_SIZE = 0x80500000, 0x490
STATE = DATA
ROOM, CANDIDATES = DATA + 0x500, DATA + 0x600
MODELS, MODEL_STRIDE = DATA + 0x700, 0x80
FRAMES, FRAME_STRIDE = DATA + 0xF00, 0x240
NODES, NODE_STRIDE = DATA + 0x3300, 8
DATA_SIZE = 0x3400
FRAME_ACTORS, ACTOR_RECORDS, AI_STATES = 0x1F80037C, 0x800F4538, 0x800F5878
ROOM_POINTER, BATTLE_ACTORS = 0x800F58BC, 0x800F1928
STACK_SPAN = (0x803FC000, 0x80400000)

# name, AI-record offset, width, signed
FIELDS = (
    ("gait", 0x01, 1, False), ("fleeing", 0x0E, 1, False), ("guarding", 0x0F, 1, False),
    ("linked_actor", 0x16, 1, True), ("alerted", 0x18, 1, False), ("guard_delay", 0x44, 1, False),
    ("x", 0x4C, 2, True), ("y", 0x4E, 2, True), ("z", 0x50, 2, True),
    ("point_height", 0x82, 2, True), ("actor_id", 0x88, 1, False), ("max_range", 0xB3, 1, False),
    ("range_mode", 0xE9, 1, False), ("plan_mode", 0x12E, 1, False),
    ("action_status", 0x131, 1, False), ("planned_tile", 0x134, 4, False),
    ("movement_target", 0x15C, 1, False), ("movement_flags", 0x15E, 2, False),
    ("destination", 0x168, 4, False), ("candidate_count", 0x470, 4, True),
)
SCALAR_NAMES = [name for name, *_ in FIELDS]
ARRAY_FIELDS = tuple((f"relation[{i}]", 0x1A4 + i * 4, 4, False) for i in range(SLOTS)) + tuple(
    (f"distance[{i}]", 0x1E4 + i * 4, 4, False) for i in range(SLOTS))
MOVER_FIELDS = (*FIELDS, *ARRAY_FIELDS)
MOVER_NAMES = [name for name, *_ in MOVER_FIELDS]
NAMES = [*MOVER_NAMES, "approach_moves",
         *(f"candidate[{i}].word[{j}]" for i in range(CANDIDATE_COUNT) for j in range(5))]
RECORD = 4 + len(NAMES)

MUTANTS = {
    # F01649: modes, range/engagement, minimum range, destination and raw flee result.
    "F01649-zero-count-ignored": {0x800D9E4C: 0},
    "F01649-frame-check-ignored": {0x800D9E80: 0},
    "F01649-range-flag-4": {0x800D9ED4: 0x30420004},
    "F01649-unbounded-range-0x1f00": {0x800D9EDC: 0x24111F00},
    "F01649-range-scale-16": {0x800D9EF4: 0x00031900},
    "F01649-allied-option-one": {0x800D9F24: 0x24130001},
    "F01649-minimum-range-0x63000": {0x800D9F64: 0x36103000},
    "F01649-approach-count-not-stored": {0x800D9F94: 0},
    "F01649-engage-range-signed": {0x800D9FA8: 0x0051102A},
    "F01649-engage-bit-2": {0x800D9FD0: 0x30420002},
    "F01649-engage-skipped": {0x800D9FDC: 0},
    "F01649-approach-option-zero": {0x800D9FF4: 0x00003821},
    "F01649-cooldown-ignored": {0x800DA024: 0},
    "F01649-minimum-range-option-ignored": {0x800DA03C: 0x1000002A},
    "F01649-destination-range-reversed": {0x800DA0DC: 0x0050802A},
    "F01649-destination-threshold-0xfffd": {0x800DA128: 0x3403FFFD},
    "F01649-wander-radius-3": {0x800DA13C: 0x24060003},
    "F01649-wander-target-kept": {0x800DA144: 0},
    "F01649-move-radius-255": {0x800DA150: 0x240600FF},
    "F01649-gait-kept": {0x800DA160: 0},
    "F01649-flee-kind-one": {0x800DA174: 0x24050001},
    "F01649-player-distance-639": {0x800DA1A4: 0x2410027F},
    # F01650: readiness, first-target identity, pending fallthrough and nearest selection.
    "F01650-readiness-reversed": {0x800DA28C: 0x00C2102A},
    "F01650-first-target-not-skipped": {0x800DA2A4: 0x10000074},
    "F01650-disengage-skipped": {0x800DA2C4: 0},
    "F01650-pending-distance-639": {0x800DA340: 0x2402027F},
    "F01650-second-count-three": {0x800DA34C: 0x28420003},
    "F01650-second-distance-first": {0x800DA358: 0x8E020430},
    "F01650-pending-option-one": {0x800DA368: 0x24120001},
    "F01650-no-pending-fallthrough": {0x800DA394: 0x10000040},
    "F01650-nearest-signed": {0x800DA42C: 0x0073102A},
    "F01650-nearest-id-kept": {0x800DA43C: 0},
    "F01650-nearest-distance-639": {0x800DA45C: 0x2406027F},
    # F01651.
    "F01651-readiness-ignored": {0x800DA4E0: 0},
    "F01651-range-scale-16": {0x800DA500: 0x00031900},
    "F01651-counter-kept": {0x800DA524: 0},
    "F01651-engage-range-signed": {0x800DA538: 0x0043102A},
    "F01651-engage-bit-2": {0x800DA560: 0x30420002},
    "F01651-engage-skipped": {0x800DA56C: 0},
    "F01651-approach-option-zero": {0x800DA584: 0x24070000},
    # F01652.
    "F01652-pending-ignored": {0x800DA5EC: 0},
    "F01652-linked-bound-15": {0x800DA60C: 0x28A2000F},
    "F01652-follow-distance-287": {0x800DA61C: 0x2406011F},
    "F01652-range-mode-two": {0x800DA634: 0x24020002},
    "F01652-plan-mode-six": {0x800DA644: 0x24020006},
    "F01652-release-skipped": {0x800DA65C: 0},
    "F01652-target-kept": {0x800DA660: 0},
    "F01652-recover-skipped": {0x800DA664: 0},
    # F01653.
    "F01653-flee-flag-ignored": {0x800DA6C0: 0x10000005},
    "F01653-flee-kind-zero": {0x800DA6CC: 0x24050000},
    "F01653-enemy-class-two": {0x800DA6E0: 0x24060002},
    "F01653-ally-class-three": {0x800DA6F8: 0x24060003},
    "F01653-ally-required-ignored": {0x800DA6FC: 0},
    "F01653-enemy-distance-1023": {0x800DA708: 0x240603FF},
    "F01653-pursuit-skipped": {0x800DA71C: 0},
    # F01654.
    "F01654-self-check-ignored": {0x800DA78C: 0},
    "F01654-guard-class-three": {0x800DA7C8: 0x38420003},
    "F01654-guard-flag-kept": {0x800DA7E0: 0},
    "F01654-visible-bit-six": {0x800DA7F8: 0x00021182},
    "F01654-delay-not-decremented": {0x800DA8CC: 0},
    "F01654-dot-threshold-minus-1000": {0x800DA8D0: 0x2A02FC18},
    "F01654-radial-distance-319": {0x800DA91C: 0x2407013F},
    "F01654-radial-gait-kept": {0x800DA934: 0},
    "F01654-delay-nine": {0x800DA948: 0x24020009},
    "F01654-near-threshold-0x23ffe": {0x800DA988: 0x34843FFE},
    "F01654-near-target-kept": {0x800DA998: 0},
    "F01654-near-recover-skipped": {0x800DA99C: 0},
    "F01654-guard-distance-319": {0x800DA9B4: 0x2406013F},
    "F01654-random-mask-127": {0x800DA9F8: 0x2404007F},
    "F01654-random-threshold-127": {0x800DA9FC: 0x2C42007F},
    "F01654-alert-kept": {0x800DAA08: 0},
    "F01654-awareness-marker-five": {0x800DAA10: 0x24050005},
    "F01654-second-flee-skipped": {0x800DAA34: 0},
}


def relation(rng: random.Random, classification: int | None = None) -> int:
    return (rng.getrandbits(32) & ~0xC0) | (
        rng.randrange(4) if classification is None else classification) << 6


def candidate(rng: random.Random, readiness: int) -> list[int]:
    radius = rng.choice((0, 1, 255, 256, 288, 320, 639, 640, 799, 800, 1024, 36000,
                         rng.randrange(36001)))
    threshold = min(65535, max(0, readiness + rng.choice((-1, 0, 1))))
    words = [rng.getrandbits(32) for _ in range(5)]
    words[1] = (words[1] & ~(0xFFFF << 13)) | threshold << 13
    words[2] = max(0, radius * radius + rng.choice((-1, 0, 1)))
    words[4] = (words[4] & ~(0xFFFF << 6)) | rng.choice((0, 1, 32, 255, 640, 65535)) << 6
    return words


def vector(rng: random.Random, number: int) -> dict:
    kind, lane = number % 6, number // 6 % 32
    readiness = rng.choice((0, 1, 100, 65534, 65535, rng.randrange(65536)))
    f = {
        "gait": rng.randrange(256), "fleeing": rng.choice((0, 0, 1, 255)),
        "guarding": rng.choice((0, 1, 255)), "linked_actor": rng.randrange(17),
        "alerted": rng.randrange(256), "guard_delay": rng.choice((0, 0, 1, 2, 10, 255)),
        "x": s16(rng.getrandbits(16)), "y": rng.randrange(-256, 257),
        "z": s16(rng.getrandbits(16)), "point_height": rng.randrange(-128, 129),
        "actor_id": rng.randrange(16), "max_range": rng.choice((0, 1, 2, 20, 255)),
        "range_mode": rng.choice((0, 1, 1, 2, 255)), "plan_mode": rng.choice((0, 1, 1, 2, 4, 7, 255)),
        "action_status": rng.choice((0, 0, 1, 2, 255)), "planned_tile": rng.getrandbits(32),
        "movement_target": rng.randrange(256), "movement_flags": rng.getrandbits(16),
        "destination": rng.getrandbits(32), "candidate_count": rng.randrange(CANDIDATE_COUNT + 1),
    }
    relations = [relation(rng) for _ in range(SLOTS)]
    distances = [rng.choice((0, 1, 0x63FFF, 0x64000, 0x64001, 0x7FFFFFFF, 0x80000000, MASK,
                             rng.getrandbits(32))) for _ in range(SLOTS)]
    candidates = [candidate(rng, readiness) for _ in range(CANDIDATE_COUNT)]
    frames = [rng.random() < 0.85 for _ in range(SLOTS)]
    models = [(rng.randrange(30001), rng.randrange(-256, 257), rng.randrange(30001),
               rng.getrandbits(32), rng.randrange(-128, 129)) for _ in range(SLOTS)]
    frame_distances = [[rng.choice((0, 0x23FFE, 0x23FFF, 0x24000, MASK, rng.getrandbits(32)))
                        for _ in range(SLOTS)] for _ in range(SLOTS)]
    roster = rng.sample(range(SLOTS), rng.randrange(SLOTS + 1))
    answers = [[rng.choice((0, 0, 1, 2, -1, -7)) for _ in range(12)] for _ in CALL_NAMES]
    answers[C_FIND] = [rng.choice((-1, -2, 0, 1, 15, rng.randrange(16))) for _ in range(12)]
    answers[C_RANDOM] = [rng.choice((0, 1, 126, 127, 128, 129, 255, -1, -128)) for _ in range(12)]
    answers[C_GROUND] = [rng.randrange(-256, 257) for _ in range(12)]
    tiles = [rng.getrandbits(32) for _ in range(12)]
    handle = lane % CANDIDATE_COUNT
    target = candidates[handle][3] & 15

    if kind == 0:
        f["candidate_count"] = 0 if lane == 0 else rng.randrange(1, 5)
        frames[target] = lane != 1
        f["plan_mode"] = (1 if lane < 12 else 2 if lane < 21 else 4 if lane < 24
                          else 7 if lane < 28 else rng.choice((0, 3, 5, 6, 255)))
        if 2 <= lane < 9:
            relations[target] = relation(rng, 2)
        if 9 <= lane < 12:
            relations[target] = relation(rng, 3)
            answers[C_CAN] = [0 if lane == 9 else 1] * 12
            if lane == 9:
                f["movement_flags"] &= ~8
                f["max_range"] = 0
                candidates[handle][2] = 0
        if 12 <= lane < 21:
            answers[C_COOLDOWN][0] = 0 if lane == 12 else 1
            candidates[handle][4] = (candidates[handle][4] & ~2) | (2 if lane >= 17 else 0)
            x, z = (f["destination"] & 255) * 128 + 64, (f["destination"] >> 16 & 255) * 128 + 64
            f.update(x=s16(x - rng.choice((0, 255, 256, 257))), y=0, z=s16(z))
            answers[C_GROUND] = [0] * 12
            if lane == 16:
                # 65535 is not a sum of three integer squares; 65534 is the
                # closest reachable value below the destination threshold.
                f.update(x=s16(x - 255), y=-22, z=s16(z - 5))
        if 24 <= lane < 28:
            relations[0] = relation(rng, 3 if lane % 2 else 2)
    elif kind == 1:
        f["action_status"] = lane % 4
        if f["action_status"] == 0:
            f["candidate_count"], f["plan_mode"] = 4, 1
            frames[candidates[0][3] & 15] = False
            candidates[1][3] = (candidates[1][3] & ~15) | (candidates[0][3] & 15)
            candidates[2][1] = (candidates[2][1] & ~(0xFFFF << 13)) | readiness << 13
        elif f["action_status"] == 1:
            f["candidate_count"] = rng.choice((1, 2, 4))
            answers[C_APPROACH] = [0] * 12
            answers[C_TRACK] = [0] * 12
            if lane % 8 == 1:
                roster = []
                relations[0] = relation(rng, 2)
                relations[candidates[0][3] & 15] = relation(rng, 3)
        elif f["action_status"] == 2 and lane % 8 == 2:
            roster = [3, 8, 1]
            for actor in roster:
                relations[actor] = relation(rng, 3)
            distances[3], distances[8], distances[1] = 0x80000000, 1, 1
            relations[0] = relation(rng, 2)
    elif kind == 2:
        answers[C_CAN][0] = 0 if lane % 4 == 0 else 1
        candidates[handle][2] = rng.choice((0, 1, 409600, 1048576))
        limit = (math.isqrt(candidates[handle][2]) + f["max_range"] * 32) ** 2
        distances[target] = max(0, limit + rng.choice((-1, 0, 1)))
        relations[target] = (relations[target] & ~1) | int(lane % 3 != 0)
    elif kind == 3:
        f["action_status"] = lane % 2
        f["linked_actor"] = (15, 16, 0, 1)[lane % 4]
        f["range_mode"] = 1 if lane % 4 < 3 else 2
        f["plan_mode"] = 7 if lane % 8 == 0 else 1
        answers[C_CAN][0] = 0 if lane % 3 else 1
        answers[C_APPROACH][0] = 0 if lane % 5 == 0 else 1
    elif kind == 4:
        f["fleeing"] = 1 if lane % 4 == 0 else 0
        answers[C_FIND][:2] = [(lane % 3) - 1, (lane % 4) - 1]
        answers[C_APPROACH][0] = 0 if lane % 2 else 1
        if lane in (7, 15, 23, 31):
            answers[C_FIND][:2] = [3, -1]
            answers[C_APPROACH][0] = 1
            f["action_status"] = 255
    else:
        f["actor_id"], f["linked_actor"] = 1, 2
        active = lane % 8 not in (0, 1, 2)
        frames[2] = lane % 8 != 1
        relations[2] = relation(rng, 2 if active else 3)
        if lane % 8 == 2:
            f["linked_actor"] = 1
            relations[1] = relation(rng, 2)
            frames[1] = True
        visible = lane % 4 != 0
        relations[2] = (relations[2] & ~32) | (32 if visible else 0)
        answers[C_FIND][0] = -1 if lane % 7 == 0 else 3
        f["guard_delay"] = (0, 1, 2, 10)[lane % 4]
        f["guarding"] = 0 if lane % 3 == 0 else 1
        models[2] = (2048, 0, 2048, packed(rng, 16, 16), 0)
        tiles[0] = packed(rng, 24, 16)
        angle = rng.choice((0, math.pi, math.acos(-1200 / 4096),
                            math.acos(-1000 / 4096), rng.random() * 2 * math.pi))
        f.update(x=2048 + round(1000 * math.cos(angle)), z=2048 + round(1000 * math.sin(angle)))
        frame_distances[2][3] = (0x23FFE, 0x23FFF, 0x24000, MASK)[lane % 4]
        answers[C_APPROACH][0], answers[C_TRACK][0] = 0, 0
        answers[C_FLEE][:2] = [0, rng.choice((0, 1, 2, -7))]
        answers[C_RANDOM][0] = (127, 128, -1, 0)[lane % 4]
        if lane in (11, 19, 27):
            f["guard_delay"] = 0
        if lane == 11:
            angle = math.atan2(64, 1088) + math.acos(-1100 / 4096)
            f.update(x=2048 + round(1000 * math.cos(angle)), z=2048 + round(1000 * math.sin(angle)))

    # A callee can change the actor and the candidate record it was given. The
    # scalar indexes below use MOVER_FIELDS, followed by the room counter.
    effects = []
    if number // 192 % 3 == 1:
        call = (C_COOLDOWN if kind == 0 and f["plan_mode"] == 2 else
                C_CAN if kind in (0, 2, 3) else
                C_DISENGAGE if kind == 1 else C_FIND)
        field = rng.choice(("max_range", "range_mode", "plan_mode", "movement_target", "guard_delay",
                            "linked_actor", "gait", "candidate_count"))
        value = {"max_range": rng.randrange(256), "range_mode": rng.randrange(3),
                 "plan_mode": rng.choice((1, 2, 4, 7)), "movement_target": rng.randrange(256),
                 "guard_delay": rng.choice((0, 1, 2, 10)), "linked_actor": rng.randrange(17),
                 "gait": rng.randrange(256), "candidate_count": rng.randrange(5)}[field]
        rank = handle if kind in (0, 2) else 0
        word = rng.choice((2, 3, 4))
        replacement = (rng.randrange(1000001) if word == 2 else rng.getrandbits(32))
        effects.append((call, 0, SCALAR_NAMES.index(field), value, rank, word, replacement))
        effects.append((C_APPROACH, 0, SCALAR_NAMES.index("range_mode"), 1, 0, 3, rng.getrandbits(32)))
        effects.append((C_DISPATCH, 0, SCALAR_NAMES.index("guard_delay"), 0, -1, 0, 0))
    return {"kind": kind, "handle": handle, "fields": f, "relations": relations, "distances": distances,
            "candidates": candidates, "frames": frames, "models": models, "frame_distances": frame_distances,
            "roster": roster, "readiness": readiness, "approach_moves": rng.choice((0, 1, MASK, rng.getrandbits(32))),
            "answers": answers, "tiles": tiles, "effects": effects, "noise": rng.randbytes(256)}


def data_block(v: dict) -> bytes:
    block = bytearray(v["noise"] * (DATA_SIZE // len(v["noise"])))
    put = field_writer(block, DATA)
    for name, offset, size, _ in FIELDS:
        put(STATE + offset, v["fields"][name], size)
    for index in range(SLOTS):
        put(STATE + 0x1A4 + index * 4, v["relations"][index], 4)
        put(STATE + 0x1E4 + index * 4, v["distances"][index], 4)
        model, frame = MODELS + index * MODEL_STRIDE, FRAMES + index * FRAME_STRIDE
        x, y, z, tile, height = v["models"][index]
        for offset, value in ((0x1C, x), (0x1E, y), (0x20, z)):
            put(model + offset, value, 2)
        put(model + 0x5C, tile, 4)
        put(frame + 0x82, height, 2)
        for target, distance in enumerate(v["frame_distances"][index]):
            put(frame + 0x1E4 + target * 4, distance, 4)
    for rank, words in enumerate(v["candidates"]):
        put(STATE + 0x430 + rank * 4, CANDIDATES + rank * CANDIDATE_SIZE, 4)
        for word, value in enumerate(words):
            put(CANDIDATES + rank * CANDIDATE_SIZE + word * 4, value, 4)
    for rank, actor in enumerate(v["roster"]):
        put(NODES + rank * NODE_STRIDE, NODES + (rank + 1) * NODE_STRIDE
            if rank + 1 < len(v["roster"]) else 0, 4)
        put(NODES + rank * NODE_STRIDE + 4, actor, 4)
    put(ROOM + 0x18, v["readiness"], 2)
    put(ROOM + 0x24, v["approach_moves"], 4)
    return bytes(block)


def snapshot(m: Machine) -> list[int]:
    return [*m.load_fields(STATE, MOVER_FIELDS, MOVER_NAMES), m.load(ROOM + 0x24),
            *(m.load(CANDIDATES + rank * CANDIDATE_SIZE + word * 4)
              for rank in range(CANDIDATE_COUNT) for word in range(5))]


def effect_address(field: int) -> tuple[int, int]:
    if field == len(MOVER_FIELDS):
        return ROOM + 0x24, 4
    _, offset, size, _ = MOVER_FIELDS[field]
    return STATE + offset, size


def run(machine: Machine, v: dict):
    if "data" not in v:
        v["data"] = data_block(v)
    initial = v["data"]
    machine.images.append(Image(initial, DATA))
    machine.put(ROOM_POINTER, ROOM)
    machine.put(BATTLE_ACTORS, NODES if v["roster"] else 0)
    for actor in range(SLOTS):
        machine.put(FRAME_ACTORS + actor * 4, FRAMES + actor * FRAME_STRIDE if v["frames"][actor] else 0)
        machine.put(ACTOR_RECORDS + actor * 4, MODELS + actor * MODEL_STRIDE)
        machine.put(AI_STATES + actor * 4, FRAMES + actor * FRAME_STRIDE)
    machine.follow.update(FOLLOW)
    calls, occurrences = [], [0] * len(CALL_NAMES)

    def stub(m: Machine, code: int) -> None:
        a0, a1, a2, a3 = (s32(value) for value in m.r[4:8])
        wrong = code not in (C_RANDOM, C_GROUND) and m.r[4] != STATE
        args = [0, 0, 0]
        if code in (C_CAN, C_COOLDOWN):
            offset = m.r[5] - CANDIDATES
            args[0] = offset // CANDIDATE_SIZE if offset % CANDIDATE_SIZE == 0 else -offset - 1
        elif code in (C_APPROACH, C_PROJECT):
            args = [a1 if code == C_APPROACH else m.r[5], a2, a3]
        elif code in (C_TRACK, C_FLEE):
            args[0] = a1
        elif code in (C_WANDER, C_MOVE):
            args = [m.r[5], a2, 0]
        elif code == C_FIND:
            args = [a2, int(a1 != 0), 0]
        elif code in (C_DISPATCH, C_AWARENESS):
            args = [a1, a2, 0]
        elif code == C_RANDOM:
            args[0] = a0
        elif code == C_GROUND:
            args = [a0, a1, 0]
        calls.append([code + NOT_THE_ACTOR * wrong, *args, *snapshot(m)])
        occurrence = occurrences[code]
        for effect_code, at, field, value, rank, word, replacement in v["effects"]:
            if effect_code != code or at != occurrence:
                continue
            address, size = effect_address(field)
            m.write(address, value, size)
            if rank >= 0:
                m.write(CANDIDATES + rank * CANDIDATE_SIZE + word * 4, replacement, 4)
        answer = v["answers"][code][occurrence] if occurrence < len(v["answers"][code]) else 0
        if code == C_FIND and m.r[5]:
            m.write(m.r[5], v["tiles"][occurrence], 4)
        m.r[2] = answer & MASK
        occurrences[code] += 1

    for code, address in enumerate(CALLEES):
        machine.stub(address, lambda m, code=code: stub(m, code))
    machine.r[4], machine.r[5] = STATE, CANDIDATES + v["handle"] * CANDIDATE_SIZE
    machine.execute(ENTRIES[v["kind"]])
    changed = [(address, initial[address - DATA], after)
               for address, after in machine.overlay.items()
               if DATA <= address < DATA + DATA_SIZE and initial[address - DATA] != after]
    observation = {"result": s32(machine.r[2]), "fields": snapshot(machine), "calls": calls,
                   "stores": machine.stores, "changed": changed}
    inputs = [v["kind"], v["handle"], *(v["fields"][name] for name in SCALAR_NAMES),
              *v["relations"], *v["distances"], v["approach_moves"], v["readiness"],
              *(word for words in v["candidates"] for word in words), *(int(frame) for frame in v["frames"]),
              *(value for model in v["models"] for value in model),
              *(value for row in v["frame_distances"] for value in row), len(v["roster"]), *v["roster"],
              *(value for queue in v["answers"] for value in (len(queue), *queue)),
              len(v["tiles"]), *v["tiles"], len(v["effects"]), *(value for effect in v["effects"] for value in effect)]
    return observation, inputs


def allowed_stores(v: dict) -> list[tuple[int, int]]:
    spans = [(STATE + offset, STATE + offset + size) for name, offset, size, _ in FIELDS
             if name in ("gait", "guarding", "guard_delay", "movement_target", "alerted")]
    spans.extend((STACK_SPAN, (ROOM + 0x24, ROOM + 0x28)))
    for _code, _at, field, _value, rank, word, _replacement in v["effects"]:
        address, size = effect_address(field)
        spans.append((address, address + size))
        if rank >= 0:
            address = CANDIDATES + rank * CANDIDATE_SIZE + word * 4
            spans.append((address, address + 4))
    return spans


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    end = 2 + len(NAMES)
    problems = field_problems(["result", *NAMES], native[1:end], [obs["result"], *obs["fields"]])
    count = native[end]
    calls = [native[end + 1 + i * RECORD:end + 1 + (i + 1) * RECORD] for i in range(count)]
    problems += call_problems(calls, obs["calls"])
    spans = allowed_stores(v)
    for address, before, after in obs["changed"]:
        if not any(low <= address < high for low, high in spans):
            problems.append(f"unrelated byte {address:#x}: {before:#x} -> {after:#x}")
            break
    return problems + store_problems(obs["stores"], spans)


def describe(image: Image, results: list) -> list[str]:
    paths: list[dict[str, int]] = [{} for _ in ENTRIES]
    events = dict.fromkeys(("callback effects", "pending fallthrough", "double flee", "raw flee result",
                            "radial", "wander", "move tile", "engage", "near recover"), 0)
    for v, obs, _native in results:
        codes = [call[0] for call in obs["calls"]]
        path = "+".join(CALL_NAMES[code % NOT_THE_ACTOR] for code in codes) or "none"
        path += f" -> {obs['result']}"
        group = paths[v["kind"]]
        group[path] = group.get(path, 0) + 1
        events["callback effects"] += bool(v["effects"])
        events["pending fallthrough"] += v["fields"]["action_status"] == 1 and codes.count(C_DISENGAGE) == 2
        events["double flee"] += codes.count(C_FLEE) == 2
        events["raw flee result"] += C_FLEE in codes and obs["result"] not in (0, 1)
        events["radial"] += C_PROJECT in codes
        events["wander"] += C_WANDER in codes
        events["move tile"] += C_MOVE in codes
        events["engage"] += C_ENGAGE in codes
        events["near recover"] += v["kind"] == 5 and C_RECOVER in codes
    lines = []
    for kind, paths_for_kind in enumerate(paths):
        total = sum(paths_for_kind.values())
        common = sorted(paths_for_kind.items(), key=lambda item: -item[1])[:8]
        lines.append(f"F{1649 + kind:05} {total} vectors, {len(paths_for_kind)} paths: " +
                     ", ".join(f"{path} ({count})" for path, count in common))
    lines.append("events: " + ", ".join(f"{name}={count}" for name, count in events.items()))
    return lines
