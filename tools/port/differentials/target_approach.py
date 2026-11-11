"""F01748, F01749 (771A8.c:1973, 2185) target approach and facing vs TargetApproach.cpp.

func_800E24EC (approach) and func_800E2B2C (facing) run with the arctangent, the squared length,
the square root and the near-target test (func_800E4FB0) followed. The near-target test squares
its constant 10 rather than the range it is passed, so the native compares the squared distance
with 100. The command dispatch, the radial projection, the locomotion dispatch, the movement
recovery, the awareness marker, the hesitation turn and the target-tile refresh are stubs: each
call records its arguments and every approach field, the projection writes a queued destination
and the hesitation turn returns queued answers. The walk-test scratch byte the close approach
clears is compared at the end.

Vectors keep the target in the actor slots and give the actor a model and a battle actor; the
native refuses the rest where the original would read through a null pointer or past the actor
table. They aim the target's distance at each range-mode threshold, the restart checks at their
equal and unequal sides and the chase at its arrival, timeout and look-around paths.
"""

from __future__ import annotations

import random

from differential import (
    MASK,
    Image,
    Machine,
    call_problems,
    field_problems,
    near,
    pack_fields,
    packed,
    placed_state,
    position,
    s16,
    s32,
    store_problems,
)

ID = "F01748, F01749"
ENTRY, END = 0x800E24EC, 0x800E2CCC
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "target_approach_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\TargetApproach.cpp",
    r"Native\Game\src\Battle\Ai\DecisionPrimitives.cpp",
    r"Native\Game\src\Battle\Ai\PlanningPrimitives.cpp",
    r"Native\Engine\src\Core\PointerSort.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1748
MUTANT_VECTORS = 1024

APPROACH = 0x800E24EC
FACING = 0x800E2B2C
ARCTANGENT = 0x8004137C
SQUARE_ROOT = 0x80041B94
SQUARED_LENGTH = 0x800E4660
NEAR_TARGET = 0x800E4FB0
COMMAND = 0x800DEEFC
PROJECT = 0x800E1388
DISPATCH = 0x800E50A0
RECOVER = 0x800E2CCC
AWARENESS = 0x800D8260
HESITATION = 0x800E4180
REFRESH = 0x800E4904

ACTOR_RECORDS = 0x800F4538
SCRATCH = 0x1F800000
WALK_TESTS = 0x1F8003D0

DATA = 0x80500000
MODEL = DATA
BATTLE_ACTOR = DATA + 0x40
RECORDS, RECORD_SIZE = DATA + 0x100, 0x40
STATE, STATE_SIZE = DATA + 0x800, 0x490
DATA_SIZE = 0x1000
ACTORS = 16
GRID = 32
FLIES = 0x200000
MOVING = 0x1F0000
RELATIONS, DISTANCES = 0x1A4, 0x1E4
K_APPROACH, K_FACING = range(2)
ENTRIES = {K_APPROACH: APPROACH, K_FACING: FACING}
# The original arctangent's `ax << 9` wraps for magnitudes of 2^22 and above and reads outside its
# table; the native one (ScalarMath.cpp) reads the entry for the true ratio. The facing's move
# point, which the approach only sets from a model position, stays inside the defined range.
WIDE_POINT = 1 << 21

# TargetApproacher in declaration order, less the model's fields and the relation and distance
# arrays: name, offset, size, signed.
FIELDS = (
    ("close_approach", 0x00, 1, False), ("gait", 0x01, 1, False), ("height_filter", 0x03, 1, False),
    ("near_target_detour", 0x04, 1, False), ("toward_point", 0x09, 1, False),
    ("facing_locked", 0x0C, 1, False), ("approach_directly", 0x0D, 1, False),
    ("chases_lost_target", 0x24, 1, False), ("approaches_target_point", 0x33, 1, False),
    ("tile", 0x34, 4, False), ("saved_destination", 0x48, 4, False), ("x", 0x4C, 2, True),
    ("z", 0x50, 2, True), ("actor_id", 0x88, 1, False), ("facing_delay", 0xBA, 2, False),
    ("range", 0xDC, 4, True), ("previous_range", 0xE0, 4, True), ("chasing_lost_target", 0xE4, 1, False),
    ("target_id", 0xE5, 1, False), ("chase_timeout", 0xE6, 1, False), ("recover_in_range", 0xE7, 1, False),
    ("walked_close", 0xE8, 1, False), ("range_mode", 0xE9, 1, False), ("previous_option", 0xEA, 1, False),
    ("previous_target_id", 0xEB, 1, False), ("chase_destination", 0xEC, 4, False),
    ("target_half_tile_x", 0xF4, 1, False), ("target_half_tile_z", 0xF6, 1, False),
    ("retarget_timer", 0xF8, 2, False), ("previous_facing_tile", 0x108, 4, False),
    ("facing_pending", 0x10C, 1, False), ("turn_timer", 0x13A, 2, False), ("target_x", 0x158, 2, True),
    ("target_z", 0x15A, 2, True), ("movement_target", 0x15C, 1, False), ("movement_flags", 0x15E, 2, False),
    ("destination", 0x168, 4, False), ("move_x", 0x17C, 4, True), ("move_z", 0x180, 4, True),
    ("angle", 0x18A, 2, True), ("reaction_pending", 0x481, 1, False),
)
NAMES = [name for name, *_ in FIELDS]
C_COMMAND, C_PROJECT, C_DISPATCH, C_RECOVER, C_AWARENESS, C_HESITATION, C_REFRESH = range(7)
CALL_NAMES = ("command", "project", "dispatch", "recover", "awareness", "hesitation", "refresh")
RECORD = 4 + len(FIELDS)
# Added to a call's code when the original passed something other than the actor.
NOT_THE_ACTOR = 100

MUTANTS = {
    # func_800E24EC
    "self-intent-1": {0x800E2528: 0x24050001},
    "flier-option-ignored": {0x800E2560: 0x00000000},
    "moving-mask-bit-16": {0x800E2568: 0x3C030001},
    "half-tile-shift-7": {0x800E2584: 0x000215C3},
    "moving-z-from-x": {0x800E25BC: 0x92230168},
    "range-change-ignored": {0x800E25D8: 0x00000000},
    "target-change-ignored": {0x800E25E8: 0x00000000},
    "option-change-ignored": {0x800E25F8: 0x00000000},
    "retarget-after-182": {0x800E2610: 0x2C4200B6},
    "restart-keeps-walked": {0x800E2620: 0x00000000},
    "restart-keeps-recover": {0x800E2624: 0x00000000},
    "previous-range-not-stored": {0x800E2634: 0x00000000},
    "active-bit-1": {0x800E2648: 0x30620002},
    "chase-not-cleared": {0x800E2654: 0x00000000},
    "reachable-bit-1": {0x800E2660: 0x00021042},
    "far-margin-0x40": {0x800E26B4: 0x2442FFC0},
    "far-compare-signed": {0x800E26C4: 0x0122102A},
    "close-margin-0x100": {0x800E26F4: 0x2462FF00},
    "leave-close-margin-0": {0x800E2724: 0x24420000},
    "walked-recover-ignored": {0x800E2764: 0x00000000},
    "recover-in-range-ignored": {0x800E277C: 0x00000000},
    "close-needs-no-option": {0x800E2784: 0x00000000},
    "close-approach-not-set": {0x800E27D8: 0x00000000},
    "facing-not-locked": {0x800E27E8: 0x00000000},
    "hold-flag-4": {0x800E27F4: 0x30630004},
    "centre-0x20": {0x800E281C: 0x2484FFE0},
    "angle-unmasked": {0x800E2834: 0x00408021},
    "saved-not-stored": {0x800E286C: 0x00000000},
    "margin-not-absolute": {0x800E288C: 0x00000000},
    "direct-not-set": {0x800E28B8: 0x00000000},
    "destination-not-restored": {0x800E28BC: 0x00000000},
    "walk-kind-1": {0x800E28C4: 0x24050001},
    "far-target-not-set": {0x800E28D4: 0x00000000},
    "recover-target-not-set": {0x800E28E0: 0x00000000},
    "close-filter-not-set": {0x800E28F4: 0x00000000},
    "walk-tests-kept": {0x800E28FC: 0x00000000},
    "close-z-from-x": {0x800E2910: 0x8482001C},
    "point-gait-kept": {0x800E2924: 0x00000000},
    "point-target-not-set": {0x800E2938: 0x00000000},
    "step-kind-0": {0x800E295C: 0x00002821},
    "walking-opcode-10": {0x800E2978: 0x2C42000A},
    "lost-ignores-chase": {0x800E2994: 0x00000000},
    "was-active-bit-0": {0x800E2998: 0x00031002},
    "chase-timeout-254": {0x800E29BC: 0x240200FE},
    "chase-z-from-x": {0x800E29C8: 0x922300F4},
    "lost-visible-bit-4": {0x800E29E0: 0x00031102},
    "awareness-marker-6": {0x800E29F0: 0x24050006},
    "awareness-frames-0x20": {0x800E29F8: 0x24060020},
    "look-chance-0x40": {0x800E2A00: 0x24050040},
    "chase-destination-not-used": {0x800E2A3C: 0x00000000},
    "chase-timeout-kept": {0x800E2A44: 0x00000000},
    "chase-arrival-z-only": {0x800E2A4C: 0x34840000},
    "chase-kept-after-looking": {0x800E2AB0: 0x00000000},
    "chase-walks-at-timeout": {0x800E2ABC: 0x00000000},
    "chase-gait-kept": {0x800E2AD0: 0x00000000},
    "wait-kind-2": {0x800E2AE8: 0x24060002},
    "lost-keeps-approaching": {0x800E2AFC: 0x00000000},
    "refresh-skipped": {0x800E2B00: 0x00000000},
    # func_800E2B2C
    "facing-self-intent-1": {0x800E2B54: 0x24050001},
    "delay-ignored": {0x800E2B60: 0x00000000},
    "reaction-ignored": {0x800E2B70: 0x10000004},
    "reaction-kind-1": {0x800E2B80: 0x24060001},
    "facing-delta-reversed": {0x800E2B94: 0x00832023},
    "same-tile-turns": {0x800E2BBC: 0x00000000},
    "facing-visible-bit-4": {0x800E2BD0: 0x00021102},
    "pending-angle-not-stored": {0x800E2BF8: 0x00000000},
    "pending-tile-not-stored": {0x800E2BFC: 0x00000000},
    "turn-timer-not-set": {0x800E2C18: 0x00000000},
    "facing-without-look": {0x800E2C28: 0x00000000},
    "facing-mask-0x7ff": {0x800E2C30: 0x306307FF},
    "settled-visible-ignored": {0x800E2C58: 0x00000000},
    "settled-angle-unmasked": {0x800E2C8C: 0x00A01021},
    "look-slot-5": {0x800E2C70: 0x24070005},
    "pending-kept-after-looking": {0x800E2C80: 0x00000000},
    "turn-intent-3": {0x800E2C9C: 0x24050003},
}


def byte_flag(rng: random.Random, zero: float) -> int:
    return 0 if rng.random() < zero else rng.choice((1, 1, rng.getrandbits(8)))


def range_value(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.8:
        return rng.choice((0, 32, 96, 128, 160, 256, 384, 512, 1024, rng.randrange(2048)))
    if roll < 0.9:
        return -rng.randrange(1, 512)
    return s32(rng.getrandbits(32))


def target_distance(rng: random.Random, range_: int) -> int:
    roll = rng.random()
    if roll < 0.75:
        margin = rng.choice((0, 32, 96, 128))
        base = ((range_ - margin) & MASK) ** 2 & MASK
        return (base + rng.choice((-1, 0, 1, rng.randrange(-5000, 5000)))) & MASK
    if roll < 0.9:
        return rng.choice((0, 99, 100, 101, rng.randrange(400)))
    return rng.getrandbits(32)


def relation(rng: random.Random) -> int:
    word = rng.getrandbits(32) & ~0x27
    for bit, chance in ((1, 0.6), (2, 0.5), (4, 0.7), (0x20, 0.4)):
        if rng.random() < chance:
            word |= bit
    return word


def model(rng: random.Random) -> tuple[int, int, int]:
    flags = rng.getrandbits(32)
    if rng.random() < 0.5:
        flags &= ~MOVING
    return flags, position(rng, rng.randrange(GRID)), position(rng, rng.randrange(GRID))


def half_tiles(rng: random.Random, target_model: tuple[int, int, int], destination: int) -> tuple[int, int]:
    flags, x, z = target_model
    if rng.random() < 0.25:
        return rng.getrandbits(8), rng.getrandbits(8)
    if flags & MOVING:
        return ((destination & 0xFF) << 1 | rng.getrandbits(1)) & 0xFF, \
               ((destination >> 16 & 0xFF) << 1 | rng.getrandbits(1)) & 0xFF
    return (x >> 6) & 0xFF, (z >> 6) & 0xFF


def fields(rng: random.Random, kind: int, actor_id: int, target: int, option: int,
           target_model: tuple[int, int, int]) -> dict:
    tx, tz = rng.randrange(GRID), rng.randrange(GRID)
    tile = packed(rng, tx, tz)
    roll = rng.random()
    if roll < 0.2:
        destination = packed(rng, tx, tz)
    elif roll < 0.9:
        destination = near(rng, tile, 3)
    else:
        destination = rng.getrandbits(32)
    half_x, half_z = half_tiles(rng, target_model, destination)
    range_ = range_value(rng)
    chase = rng.random()
    if chase < 0.35:
        chase_destination = packed(rng, tx, tz)
    elif chase < 0.9:
        chase_destination = near(rng, tile, 3)
    else:
        chase_destination = rng.getrandbits(32)
    facing_tile = packed(rng, destination & 0xFF, destination >> 16 & 0xFF) if rng.random() < 0.35 \
        else near(rng, destination, 2)
    flags = rng.getrandbits(16)
    if rng.random() < 0.5:
        flags |= 8
    elif rng.random() < 0.7:
        flags &= ~8
    _, model_x, model_z = target_model
    return {
        "close_approach": byte_flag(rng, 0.6), "gait": byte_flag(rng, 0.5),
        "height_filter": byte_flag(rng, 0.6), "near_target_detour": byte_flag(rng, 0.6),
        "toward_point": byte_flag(rng, 0.6), "facing_locked": byte_flag(rng, 0.6),
        "approach_directly": byte_flag(rng, 0.6), "chases_lost_target": byte_flag(rng, 0.5),
        "approaches_target_point": byte_flag(rng, 0.6), "tile": tile,
        "saved_destination": rng.getrandbits(32), "x": position(rng, tx), "z": position(rng, tz),
        "actor_id": actor_id, "facing_delay": 0 if rng.random() < 0.15 else rng.getrandbits(16),
        "range": range_,
        "previous_range": range_ if rng.random() < 0.75 else range_value(rng),
        "chasing_lost_target": byte_flag(rng, 0.5), "target_id": target,
        "chase_timeout": rng.choice((0, 1, 2, 254, 255, rng.getrandbits(8))),
        "recover_in_range": byte_flag(rng, 0.8), "walked_close": byte_flag(rng, 0.7),
        "range_mode": rng.choice((0, 0, 1, 1, 2, 2, 3, rng.getrandbits(8))),
        "previous_option": option & 0xFF if rng.random() < 0.75 else rng.getrandbits(8),
        "previous_target_id": target if rng.random() < 0.8 else rng.getrandbits(8),
        "chase_destination": chase_destination,
        "target_half_tile_x": half_x, "target_half_tile_z": half_z,
        "retarget_timer": rng.choice((0, 5, 179, 180, 181, 182, 0xFFFF, rng.getrandbits(16))),
        "previous_facing_tile": facing_tile, "facing_pending": byte_flag(rng, 0.5),
        "turn_timer": rng.getrandbits(16),
        "target_x": s16(rng.getrandbits(16)), "target_z": s16(rng.getrandbits(16)),
        "movement_target": rng.getrandbits(8), "movement_flags": flags, "destination": destination,
        "move_x": s32(model_x if rng.random() < 0.8 else rng.randrange(-WIDE_POINT, WIDE_POINT)),
        "move_z": s32(model_z if rng.random() < 0.8 else rng.randrange(-WIDE_POINT, WIDE_POINT)),
        "angle": rng.randrange(0x1000) if rng.random() < 0.5 else s16(rng.getrandbits(16)),
        "reaction_pending": byte_flag(rng, 0.75),
    }


def vector(rng: random.Random, number: int) -> dict:
    kind = number % 2 if number < 8 else (K_APPROACH if rng.random() < 0.7 else K_FACING)
    actor_id = rng.randrange(ACTORS)
    target = actor_id if rng.random() < 0.05 else rng.randrange(ACTORS)
    option = rng.choice((0, 0, 1, 1, 2, 255, 0x101, s32(rng.getrandbits(32))))
    models = [model(rng) for _ in range(ACTORS)]
    f = fields(rng, kind, actor_id, target, option, models[target])
    relations = [relation(rng) for _ in range(ACTORS)]
    distances = [rng.getrandbits(32) for _ in range(ACTORS)]
    distances[target] = target_distance(rng, f["range"])
    look = rng.randrange(0x1000) if rng.random() < 0.5 else s16(rng.getrandbits(16))
    facing = s16((f["angle"] - look) & 0xFFFF) if rng.random() < 0.4 else s16(rng.getrandbits(16))
    tile = f["tile"]
    projected = packed(rng, tile & 0xFF, tile >> 16 & 0xFF) if rng.random() < 0.4 else near(rng, tile, 3)
    return {
        "kind": kind, "argument": option if kind == K_APPROACH else target, "fields": f,
        "self_flags": rng.getrandbits(32) & ~FLIES | (FLIES if rng.random() < 0.3 else 0),
        "look": look, "facing": facing, "relations": relations, "distances": distances,
        "models": models,
        "opcode": rng.choice((0, 2, 3, 8, 9, 10, 11, rng.randrange(24), rng.getrandbits(8))),
        "walk_tests": rng.randrange(1, 0x100), "projected": [projected],
        "hesitations": [rng.choice((0, 0, 1, s32(rng.getrandbits(32)))) for _ in range(2)],
        "state": rng.randbytes(STATE_SIZE),
    }


def data_block(v: dict) -> bytes:
    block = bytearray(DATA_SIZE)
    model_at = MODEL - DATA
    block[model_at + 8:model_at + 12] = v["self_flags"].to_bytes(4, "little")
    block[model_at + 0x14:model_at + 0x16] = (v["look"] & 0xFFFF).to_bytes(2, "little")
    block[model_at + 0x26:model_at + 0x28] = (v["facing"] & 0xFFFF).to_bytes(2, "little")
    block[BATTLE_ACTOR - DATA + 9] = v["opcode"]
    for index, (flags, x, z) in enumerate(v["models"]):
        record = RECORDS - DATA + index * RECORD_SIZE
        block[record + 8:record + 12] = flags.to_bytes(4, "little")
        block[record + 0x1C:record + 0x1E] = (x & 0xFFFF).to_bytes(2, "little")
        block[record + 0x20:record + 0x22] = (z & 0xFFFF).to_bytes(2, "little")
    state = pack_fields(v["state"], FIELDS, v["fields"])
    state[0x54:0x58] = BATTLE_ACTOR.to_bytes(4, "little")
    for index in range(ACTORS):
        state[RELATIONS + index * 4:RELATIONS + index * 4 + 4] = v["relations"][index].to_bytes(4, "little")
        state[DISTANCES + index * 4:DISTANCES + index * 4 + 4] = v["distances"][index].to_bytes(4, "little")
    return placed_state(block, STATE - DATA, state, MODEL)


def run(machine: Machine, v: dict):
    machine.images.append(Image(data_block(v), DATA))
    machine.map_zero(SCRATCH, 0x400)
    machine.put(WALK_TESTS, v["walk_tests"], 1)
    for index in range(ACTORS):
        machine.put(ACTOR_RECORDS + index * 4, RECORDS + index * RECORD_SIZE)
    machine.follow.update({ARCTANGENT, SQUARE_ROOT, SQUARED_LENGTH, NEAR_TARGET})
    calls: list[list[int]] = []
    projected = list(v["projected"])
    hesitations = list(v["hesitations"])

    def record(m: Machine, code: int, *arguments: int) -> None:
        if m.r[4] != STATE:
            code += NOT_THE_ACTOR
        padded = [*arguments, 0, 0, 0][:3]
        calls.append([code, *padded, *m.load_fields(STATE, FIELDS, NAMES)])

    def project(m: Machine) -> None:
        record(m, C_PROJECT, m.r[5] & MASK, s32(m.r[6]), m.r[7] & MASK)
        if projected:
            m.put(STATE + 0x168, projected.pop(0))

    def hesitate(m: Machine) -> None:
        record(m, C_HESITATION, s32(m.r[5]), s32(m.r[6]), s32(m.r[7]))
        m.r[2] = (hesitations.pop(0) if hesitations else 0) & MASK

    machine.stub(COMMAND, lambda m: record(m, C_COMMAND, s32(m.r[5])))
    machine.stub(PROJECT, project)
    machine.stub(DISPATCH, lambda m: record(m, C_DISPATCH, s32(m.r[5]), s32(m.r[6])))
    machine.stub(RECOVER, lambda m: record(m, C_RECOVER))
    machine.stub(AWARENESS, lambda m: record(m, C_AWARENESS, s32(m.r[5]), s32(m.r[6])))
    machine.stub(HESITATION, hesitate)
    machine.stub(REFRESH, lambda m: record(m, C_REFRESH))
    machine.r[4] = STATE
    machine.r[5] = v["argument"] & MASK
    machine.execute(ENTRIES[v["kind"]])
    observation = {"result": machine.r[2], "fields": machine.load_fields(STATE, FIELDS, NAMES),
                   "walk_tests": machine.load(WALK_TESTS, 1), "calls": calls, "stores": machine.stores}
    f = v["fields"]
    inputs = [v["kind"], v["argument"], *(f[name] for name in NAMES), v["self_flags"], v["look"], v["facing"],
              *v["relations"], *v["distances"],
              *(value for flags, x, z in v["models"] for value in (int(flags & MOVING != 0), x, z)),
              v["opcode"], v["walk_tests"], len(v["projected"]), *v["projected"], len(v["hesitations"]),
              *v["hesitations"]]
    return observation, inputs


def allowed_stores() -> list[tuple[int, int]]:
    spans = [(STATE + offset, STATE + offset + size) for _, offset, size, _ in FIELDS]
    return [*spans, (WALK_TESTS, WALK_TESTS + 1), (0x80400000 - 0x4000, 0x80400000)]


ALLOWED = allowed_stores()


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    problems = []
    if native[1] != obs["result"]:
        problems.append(f"result native {native[1]} original {obs['result']}")
    start = 2 + len(FIELDS)
    problems += field_problems(NAMES, native[2:start], obs["fields"])
    if native[start] != obs["walk_tests"]:
        problems.append(f"walk tests native {native[start]:#x} original {obs['walk_tests']:#x}")
    count = native[start + 1]
    first = start + 2
    calls = [native[first + i * RECORD:first + (i + 1) * RECORD] for i in range(count)]
    problems += call_problems(calls, obs["calls"])
    return problems + store_problems(obs["stores"], ALLOWED)


def describe(image: Image, results: list) -> list[str]:
    paths: list[dict[str, int]] = [{}, {}]
    restarted = directly = walked = 0
    for v, obs, _native in results:
        signature = "+".join(CALL_NAMES[call[0] % NOT_THE_ACTOR] for call in obs["calls"]) or "none"
        signature += f" -> {obs['result']}"
        kind = v["kind"]
        paths[kind][signature] = paths[kind].get(signature, 0) + 1
        if kind != K_APPROACH:
            continue
        final = dict(zip(NAMES, obs["fields"], strict=True))
        restarted += final["retarget_timer"] == 0 and v["fields"]["retarget_timer"] != 0xFFFF
        directly += any(call[0] == C_PROJECT for call in obs["calls"]) and final["approach_directly"] == 1 \
            and v["fields"]["approach_directly"] != 1
        walked += final["walked_close"] == 1 and v["fields"]["walked_close"] != 1
    lines = []
    for kind, name in ((K_APPROACH, "F01748 approach"), (K_FACING, "F01749 facing")):
        ranked = sorted(paths[kind].items(), key=lambda item: -item[1])
        lines.append(f"{name} paths: " + ", ".join(f"{path} {count}" for path, count in ranked))
    lines.append(f"F01748 events: restarted {restarted}, projected onto its own tile {directly}, "
                 f"closing in issued a walk {walked}")
    return lines
