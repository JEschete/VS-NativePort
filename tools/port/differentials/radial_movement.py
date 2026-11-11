"""F01735-F01737, F01750 (771A8.c:1330-1437, 2272) radial movement vs RadialMovement.cpp.

func_800E1388 (projection), func_800E153C (walk to the destination), func_800E1764 (retreat)
and func_800E2CCC (movement recovery) run with the arctangent, the Q12 sine and cosine, the
squared lengths and the ground height followed, so each routine also runs the others it calls.
Destination occupancy, the command dispatch, the step toward the destination, the reservation
release and the terrain movement start are stubs: each call is recorded with its argument and a
snapshot of the fields the routines write, and occupancy returns queued answers.

The original projection kept bytes 1 and 3 of its caller's $s0 in the destination (#341); the
direct projection runs with $s0 zero, and through the walk and the retreat those bytes of the
destination, the next tile and the occupancy tiles are not compared.
"""

from __future__ import annotations

import random

from differential import Image, Machine, pack_fields, s16, s32

ID = "F01735-F01737, F01750"
ENTRY, END = 0x800E1388, 0x800E1850
RECOVER_END = 0x800E2F5C
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "radial_movement_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\RadialMovement.cpp",
    r"Native\Game\src\Field\TerrainHeight.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1735
MUTANT_VECTORS = 2048

PROJECT = 0x800E1388
MOVE_TO = 0x800E153C
MOVE_AWAY = 0x800E1764
RECOVER = 0x800E2CCC
ARCTANGENT = 0x8004137C
COSINE = 0x80040F24
SINE = 0x80040F28
SQUARED_LENGTH = 0x800E4660
SQUARED_TILE_DISTANCE = 0x800E45F4
GROUND_HEIGHT_FLAGS = 0x800E4320
OCCUPANCY = 0x800D954C
COMMAND = 0x800DEEFC
TOWARD = 0x800E4B2C
RELEASE = 0x800E678C
BEGIN_TERRAIN = 0x800E4B10

DIRECTION_CODES = 0x800F16EC
ACTOR_RECORDS = 0x800F4538
ROOM_AI_STATE = 0x800F58BC
SCRATCH = 0x1F800000
HEIGHT_MAP = 0x1F8003BC
GRID_BOUNDS = 0x1F8003DC
SCRATCH_DIRECTIONS = 0x1F8003EC

DATA = 0x80500000
HEIGHTS = DATA
BOUNDS = DATA + 0x800
MODEL = DATA + 0x840
RECORDS, RECORD_SIZE = DATA + 0x900, 0x40
STATE, STATE_SIZE = DATA + 0x1000, 0x230
DATA_SIZE = 0x1300
GRID = 32
ACTORS = 17
NO_TARGET = 16
FLIES = 0x200000
K_PROJECT, K_MOVE, K_AWAY, K_RECOVER = range(4)
ENTRIES = {K_PROJECT: PROJECT, K_MOVE: MOVE_TO, K_AWAY: MOVE_AWAY, K_RECOVER: RECOVER}

# RadialMover in declaration order, less the model's fields: name, offset, size, signed.
FIELDS = (
    ("toward_point", 0x09, 1, False), ("approach_directly", 0x0D, 1, False), ("tile", 0x34, 4, False),
    ("saved_destination", 0x48, 4, False), ("x", 0x4C, 2, True), ("z", 0x50, 2, True),
    ("move_attempts", 0x8C, 2, False), ("move_distance", 0xB4, 4, True), ("turn_timer", 0x13A, 2, False),
    ("turn_period", 0x13C, 2, False), ("target_x", 0x158, 2, True), ("target_z", 0x15A, 2, True),
    ("movement_target", 0x15C, 1, False), ("movement_flags", 0x15E, 2, False),
    ("destination", 0x168, 4, False), ("next_tile", 0x170, 4, False), ("move_x", 0x17C, 4, True),
    ("move_z", 0x180, 4, True), ("heading", 0x188, 2, True), ("angle", 0x18A, 2, True),
    ("stall_counter", 0x228, 4, True),
)
NAMES = [name for name, *_ in FIELDS]
# The fields a stubbed call records, as the driver's Note lists them.
SNAPSHOT = ("destination", "next_tile", "move_attempts", "turn_timer", "heading", "angle", "stall_counter")
RECORD = 2 + len(SNAPSHOT)
C_OCCUPANCY, C_COMMAND, C_TOWARD, C_RELEASE, C_BEGIN = range(5)
# Added to a call's code when the original passed something other than the actor.
NOT_THE_ACTOR = 100
# Bytes 1 and 3 of a tile word; see the module docstring.
TAG_BYTES = 0xFF00FF00

MUTANTS = {
    # func_800E1388
    "heading-base-0x400": {0x800E13A0: 0x24C60400},
    "step-quarter": {0x800E13B0: 0x0011B882},
    "x-centre-0x20": {0x800E1400: 0x24420020},
    "z-from-x-origin": {0x800E1438: 0x02C21021},
    "free-on-zero": {0x800E1454: 0x04410006},
    "seven-tries": {0x800E1460: 0x2A420007},
    "min-x-from-max": {0x800E1478: 0x90850000},
    "max-z-from-min": {0x800E14E4: 0x90840003},
    "destination-not-stored": {0x800E1508: 0x00000000},
    # func_800E153C
    "same-tile-walks": {0x800E1570: 0x00000000},
    "distance-compare-signed": {0x800E15E0: 0x0102102A},
    "direct-flag-2-only": {0x800E1604: 0x30420002},
    "approach-ignored": {0x800E1618: 0x00000000},
    "near-17": {0x800E1630: 0x2C420011},
    "saved-bit-22": {0x800E1644: 0x00031240},
    "five-attempts": {0x800E1678: 0x2C420005},
    "neighbour-stride-1": {0x800E16B4: 0x00111000},
    "three-neighbours": {0x800E1710: 0x2A220003},
    "walk-intent-12": {0x800E172C: 0x2405000C},
    "attempts-kept": {0x800E1730: 0x00000000},
    "next-tile-not-set": {0x800E1738: 0x00000000},
    # func_800E1764
    "opposite-quarter-turn": {0x800E1780: 0x26500400},
    "move-z-from-x": {0x800E17D0: 0x8E22017C},
    "farther-compare-reversed": {0x800E1818: 0x0050802B},
    # func_800E2CCC
    "flight-bit-22": {0x800E2D04: 0x3C030040},
    "stall-limit-119": {0x800E2D24: 0x28630078},
    "stall-not-stored": {0x800E2D2C: 0x00000000},
    "mode-flag-0x10-only": {0x800E2D40: 0x30620010},
    "flier-free-on-zero": {0x800E2D58: 0x04400007},
    "target-z-from-x": {0x800E2DA8: 0x8445001C},
    "target-angle-unmasked": {0x800E2DB8: 0x00408021},
    "target-timer-not-set": {0x800E2DD0: 0x00000000},
    "point-intent-4": {0x800E2E0C: 0x24050004},
    "quadrant-midpoint-0x100": {0x800E2E44: 0x2C420101},
    # The start quadrant's `& 3` is redundant (the loop masks the direction with 7), so a wider wrap
    # is an equivalent mutant; wrapping to 1 moves quadrants 1 and 2 and changes tie-breaks.
    "quadrant-wrapped-to-1": {0x800E2E54: 0x30420001},
    "depth-from-width": {0x800E2EBC: 0x92620000},
    "height-compare-reversed": {0x800E2EE0: 0x0074102A},
    "stand-intent-1": {0x800E2F10: 0x24050001},
    "angle-shift-8": {0x800E2F1C: 0x00151200},
}

_battle: Image | None = None


def battle() -> Image:
    global _battle
    if _battle is None:
        _battle = Image.load("BATTLE/BATTLE.PRG")
    return _battle


def tile_byte(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.8:
        return rng.randrange(GRID)
    return rng.choice((0, 1, 30, 31, 32, 254, 255, rng.getrandbits(8)))


def packed(rng: random.Random, x: int, z: int) -> int:
    return rng.getrandbits(8) << 24 | (z & 0xFF) << 16 | rng.getrandbits(8) << 8 | (x & 0xFF)


def near(rng: random.Random, tile: int, spread: int) -> int:
    x, z = tile & 0xFF, tile >> 16 & 0xFF
    return packed(rng, x + rng.randrange(-spread, spread + 1), z + rng.randrange(-spread, spread + 1))


def position(rng: random.Random, tile_coordinate: int) -> int:
    if rng.random() < 0.9:
        return s16(tile_coordinate * 128 + rng.randrange(-64, 192))
    return s16(rng.getrandbits(16))


def angle(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.3:
        return rng.choice((0, 0x400, 0x800, 0xC00, 0x200, 0x600, 0xA00, 0xE00))
    if roll < 0.8:
        return rng.randrange(-0x1000, 0x1000)
    return s32(rng.getrandbits(32))


def distance(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.7:
        return rng.choice((0, 1, 7, 8, 128, 256, 384, 512, 768, 1024, 1536, 2048, rng.randrange(4096)))
    # Around the 32-bit wrap of the squared distance (46341 squared exceeds 2^31).
    return rng.choice((0x10000, 46340, 46341, 65535, rng.randrange(1 << 20), rng.getrandbits(32)))


def occupancy(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.9:
        return rng.choice((-1, 0, 0, 0, 1, 1, 2))
    return s32(rng.getrandbits(32))


def height(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.6:
        return rng.randrange(4)
    if roll < 0.8:
        return rng.randrange(0x3FC, 0x400) | rng.getrandbits(6) << 10
    return rng.getrandbits(16)


def bounds(rng: random.Random) -> tuple[int, int, int, int]:
    if rng.random() < 0.8:
        min_x, min_z = rng.randrange(0, 8), rng.randrange(0, 8)
        return min_x, min_z, rng.randrange(min_x, GRID), rng.randrange(min_z, GRID)
    return tuple(rng.getrandbits(8) for _ in range(4))


def fields(rng: random.Random, kind: int) -> dict:
    tile = packed(rng, tile_byte(rng), tile_byte(rng))
    tx, tz = tile & 0xFF, tile >> 16 & 0xFF
    roll = rng.random()
    if roll < 0.15:
        destination = packed(rng, tx, tz)  # the same tile, other tag bytes
    elif roll < 0.85:
        destination = near(rng, tile, rng.choice((1, 2, 3, 6)))
    else:
        destination = rng.getrandbits(32)
    flags = rng.getrandbits(16)
    if rng.random() < 0.5:
        flags &= rng.choice((0x06, 0x30, 0x36, 0x12, 0x24, 0xC6, 0))
    stall = rng.choice((0, 119, 120, 121, 122, 0x7FFFFFFF, -1, s32(rng.getrandbits(32))))
    target = NO_TARGET if rng.random() < 0.6 else rng.randrange(NO_TARGET)
    if kind != K_RECOVER and rng.random() < 0.5:
        target = NO_TARGET
    return {
        "toward_point": rng.choice((0, 0, 1, rng.getrandbits(8))),
        "approach_directly": rng.choice((0, 0, 1, rng.getrandbits(8))),
        "tile": tile,
        "saved_destination": near(rng, tile, 4) | rng.choice((0, 0x800000)) if rng.random() < 0.8
        else rng.getrandbits(32),
        "x": position(rng, tx), "z": position(rng, tz),
        "move_attempts": rng.choice((0, 3, 4, 5, 6, 0xFFFF, rng.getrandbits(16))),
        "move_distance": s32(distance(rng)),
        "turn_timer": rng.getrandbits(16), "turn_period": rng.getrandbits(16),
        "target_x": position(rng, tile_byte(rng)), "target_z": position(rng, tile_byte(rng)),
        "movement_target": target,
        "movement_flags": flags,
        "destination": destination,
        "next_tile": rng.getrandbits(32),
        "move_x": s32(position(rng, tile_byte(rng)) if rng.random() < 0.9 else rng.getrandbits(32)),
        "move_z": s32(position(rng, tile_byte(rng)) if rng.random() < 0.9 else rng.getrandbits(32)),
        "heading": s16(rng.getrandbits(16)), "angle": s16(rng.getrandbits(16)),
        "stall_counter": stall,
    }


def vector(rng: random.Random, number: int) -> dict:
    image = battle()
    kind = number % 4 if number < 16 else rng.choices(range(4), (25, 30, 20, 25))[0]
    codes = ([image.read(DIRECTION_CODES + i, 1) for i in range(8)] if rng.random() < 0.8
             else [rng.getrandbits(8) for _ in range(8)])
    width = GRID if rng.random() < 0.7 else rng.randrange(1, GRID + 1)
    depth = GRID if rng.random() < 0.7 else rng.randrange(1, GRID + 1)
    f = fields(rng, kind)
    return {
        "kind": kind, "codes": codes, "bounds": bounds(rng), "width": width, "depth": depth,
        "heights": [height(rng) for _ in range(GRID * GRID)], "fields": f,
        "flies": rng.random() < 0.5,
        "model_flags": rng.getrandbits(32) & ~FLIES,
        "look": s16(rng.getrandbits(16)) if rng.random() < 0.5 else rng.randrange(0x1000),
        "facing": s16(rng.getrandbits(16)) if rng.random() < 0.5 else rng.randrange(0x1000),
        "actors": [(position(rng, tile_byte(rng)), position(rng, tile_byte(rng))) for _ in range(ACTORS)],
        "occupancy": [occupancy(rng) for _ in range(24)],
        "source": packed(rng, tile_byte(rng), tile_byte(rng)) if rng.random() < 0.7 else f["tile"],
        "angle": angle(rng), "distance": distance(rng),
        "state": rng.randbytes(STATE_SIZE),
    }


def data_block(v: dict) -> bytes:
    block = bytearray(DATA_SIZE)
    for index, cell in enumerate(v["heights"]):
        block[index * 2:index * 2 + 2] = cell.to_bytes(2, "little")
    min_x, min_z, max_x, max_z = v["bounds"]
    block[BOUNDS - DATA:BOUNDS - DATA + 4] = bytes((max_x, max_z, min_x, min_z))
    model = MODEL - DATA
    flags = v["model_flags"] | (FLIES if v["flies"] else 0)
    block[model + 8:model + 12] = flags.to_bytes(4, "little")
    block[model + 0x14:model + 0x16] = (v["look"] & 0xFFFF).to_bytes(2, "little")
    block[model + 0x26:model + 0x28] = (v["facing"] & 0xFFFF).to_bytes(2, "little")
    for index, (x, z) in enumerate(v["actors"]):
        record = RECORDS - DATA + index * RECORD_SIZE
        block[record + 0x1C:record + 0x1E] = (x & 0xFFFF).to_bytes(2, "little")
        block[record + 0x20:record + 0x22] = (z & 0xFFFF).to_bytes(2, "little")
    state = pack_fields(v["state"], FIELDS, v["fields"])
    state[0x58:0x5C] = MODEL.to_bytes(4, "little")
    block[STATE - DATA:STATE - DATA + STATE_SIZE] = state
    return bytes(block)


def run(machine: Machine, v: dict):
    machine.images.append(Image(data_block(v), DATA))
    machine.map_zero(SCRATCH, 0x400)
    machine.put(HEIGHT_MAP, HEIGHTS)
    machine.put(GRID_BOUNDS, v["width"], 1)
    machine.put(GRID_BOUNDS + 1, v["depth"], 1)
    machine.store(SCRATCH_DIRECTIONS, bytes(v["codes"]))
    machine.put(ROOM_AI_STATE, BOUNDS)
    for index in range(ACTORS):
        machine.put(ACTOR_RECORDS + index * 4, RECORDS + index * RECORD_SIZE)
    machine.follow.update({PROJECT, MOVE_TO, RECOVER, ARCTANGENT, COSINE, SINE, SQUARED_LENGTH,
                           SQUARED_TILE_DISTANCE, GROUND_HEIGHT_FLAGS})
    records: list[list[int]] = []
    answers = list(v["occupancy"])

    def record(m: Machine, code: int, argument: int) -> None:
        if m.r[4] != STATE:
            code += NOT_THE_ACTOR
        records.append([code, argument, *m.load_fields(STATE, FIELDS, SNAPSHOT)])

    def free(m: Machine) -> None:
        record(m, C_OCCUPANCY, m.r[5])
        m.r[2] = (answers.pop(0) if answers else 0) & 0xFFFFFFFF

    machine.stub(OCCUPANCY, free)
    machine.stub(COMMAND, lambda m: record(m, C_COMMAND, s32(m.r[5])))
    machine.stub(TOWARD, lambda m: record(m, C_TOWARD, s32(m.r[5])))
    machine.stub(RELEASE, lambda m: record(m, C_RELEASE, 0))
    machine.stub(BEGIN_TERRAIN, lambda m: record(m, C_BEGIN, 0))
    kind = v["kind"]
    machine.r[4] = STATE
    if kind == K_PROJECT:
        machine.r[5:8] = [v["source"], v["angle"] & 0xFFFFFFFF, v["distance"] & 0xFFFFFFFF]
        machine.r[16] = 0
    else:
        machine.r[5] = v["angle"] & 0xFFFFFFFF
    machine.execute(ENTRIES[kind])
    observation = {"fields": machine.load_fields(STATE, FIELDS, NAMES), "records": records,
                   "stores": machine.stores}
    min_x, min_z, max_x, max_z = v["bounds"]
    inputs = [kind, *v["codes"], min_x, min_z, max_x, max_z, v["width"], v["depth"], *v["heights"],
              *(v["fields"][name] for name in NAMES), int(v["flies"]), v["look"], v["facing"],
              *(value for actor in v["actors"] for value in actor), len(v["occupancy"]), *v["occupancy"],
              v["source"], v["angle"], v["distance"]]
    return observation, inputs


def allowed_stores() -> list[tuple[int, int]]:
    spans = [(STATE + offset, STATE + offset + size) for _, offset, size, _ in FIELDS]
    return [*spans, (0x80400000 - 0x4000, 0x80400000)]


ALLOWED = allowed_stores()
TILE_FIELDS = {NAMES.index("destination"), NAMES.index("next_tile")}
SNAPSHOT_TILES = {2 + SNAPSHOT.index("destination"), 2 + SNAPSHOT.index("next_tile")}


def untagged(kind: int, fields: list[int], records: list[list[int]]) -> tuple[list[int], list[list[int]]]:
    """Drop the tag bytes the original took from its caller's $s0 (#341)."""
    if kind not in (K_MOVE, K_AWAY):
        return fields, records
    fields = [value & ~TAG_BYTES if i in TILE_FIELDS else value for i, value in enumerate(fields)]
    records = [[value & ~TAG_BYTES if i in SNAPSHOT_TILES or (i == 1 and record[0] == C_OCCUPANCY) else value
                for i, value in enumerate(record)] for record in records]
    return fields, records


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    problems = []
    fields, count = native[1:1 + len(FIELDS)], native[1 + len(FIELDS)]
    start = 2 + len(FIELDS)
    records = [native[start + i * RECORD:start + (i + 1) * RECORD] for i in range(count)]
    fields, records = untagged(v["kind"], fields, records)
    original_fields, original_records = untagged(v["kind"], obs["fields"], obs["records"])
    differing = [f"{NAMES[i]} native {fields[i]:#x} original {original_fields[i]:#x}"
                 for i in range(len(FIELDS)) if fields[i] != original_fields[i]]
    if differing:
        problems.append("fields: " + ", ".join(differing))
    if records != original_records:
        first = next((i for i, (a, b) in enumerate(zip(records, original_records, strict=False)) if a != b),
                     min(len(records), len(original_records)))
        problems.append(f"calls differ from {first}: native {records[first:first + 2]} "
                        f"original {original_records[first:first + 2]}")
    for address, size in obs["stores"]:
        if not any(low <= address and address + size <= high for low, high in ALLOWED):
            problems.append(f"original stored {size} bytes at {address:#x}")
            break
    return problems


def beyond(f: dict) -> bool:
    """Whether the walk reprojects: the destination's centre lies beyond the move distance."""
    tile = f["destination"]
    dx = s16((tile & 0xFF) * 128 + 64 - f["x"])
    dz = s16((tile >> 16 & 0xFF) * 128 + 64 - f["z"])
    reach = f["move_distance"] & 0xFFFFFFFF
    return reach * reach & 0xFFFFFFFF < dx * dx + dz * dz


def describe(image: Image, results: list) -> list[str]:
    tries = [0] * 9
    walks = {"recovered on its tile": 0, "reprojected": 0, "stepped toward": 0, "counted and recovered": 0,
             "walked": 0, "walked to a neighbour": 0, "found no free tile": 0}
    retreats = {"kept along": 0, "kept opposite": 0}
    recoveries = {"released": 0, "turned to target": 0, "turned to point": 0, "turned uphill": 0,
                  "stood": 0}
    for v, obs, _native in results:
        records = obs["records"]
        codes = [record[0] for record in records]
        kind = v["kind"]
        if kind == K_PROJECT:
            tries[codes.count(C_OCCUPANCY)] += 1
            continue
        commands = [record[1] for record in records if record[0] == C_COMMAND]
        if kind == K_RECOVER:
            if C_RELEASE in codes:
                recoveries["released"] += 1
            elif commands == [0]:
                recoveries["stood"] += 1
            elif commands == [3]:
                recoveries["turned to point"] += 1
            elif v["fields"]["movement_target"] != NO_TARGET:
                recoveries["turned to target"] += 1
            else:
                recoveries["turned uphill"] += 1
            continue
        if kind == K_AWAY:
            along = obs["records"][codes.index(C_OCCUPANCY)] if C_OCCUPANCY in codes else None
            retreats["kept along" if along and obs["fields"][NAMES.index("destination")] & 0xFF00FF
                     == along[1] & 0xFF00FF else "kept opposite"] += 1
        f = v["fields"]
        if kind == K_MOVE and (f["tile"] ^ f["destination"]) & 0xFF00FF == 0:
            walks["recovered on its tile"] += 1
        elif C_TOWARD in codes:
            walks["stepped toward"] += 1
        elif 13 in commands:
            moved = obs["fields"][NAMES.index("destination")]
            walks["walked to a neighbour" if (moved ^ f["destination"]) & 0xFF00FF else "walked"] += 1
        elif obs["fields"][NAMES.index("move_attempts")] >= 6 and codes.count(C_OCCUPANCY) >= 5:
            walks["found no free tile"] += 1
        else:
            walks["counted and recovered"] += 1
        if kind == K_MOVE and (f["tile"] ^ f["destination"]) & 0xFF00FF and beyond(f):
            walks["reprojected"] += 1
    recover = image.data[RECOVER - image.base:RECOVER_END - image.base]
    return [f"F01750 {RECOVER:08X}-{RECOVER_END - 4:08X} inclusive: {len(recover) // 4} original "
            f"instructions; sha256={image.sha256(RECOVER, RECOVER_END)}",
            "F01735 occupancy tries: " + ", ".join(f"{n} {count}" for n, count in enumerate(tries) if count),
            "F01736/F01737 walks: " + ", ".join(f"{name} {count}" for name, count in walks.items()),
            "F01737 retreats: " + ", ".join(f"{name} {count}" for name, count in retreats.items()),
            "F01750 recoveries: " + ", ".join(f"{name} {count}" for name, count in recoveries.items())]
