"""F01729, F01730, F01732-F01734 (771A8.c:798-1278) flight movement vs FlightMovement.cpp.

func_800E02B4 (obstruction), func_800E0678 (flight test), func_800E0918 (ground transition),
func_800E0A68 (flight toward the destination) and func_800E1238 (directional flight) run with
the already promoted func_800E0850 (lowest reachable height), the ground heights, the squared
distances, the segment minimum, the arctangent, the Q12 sine and cosine and the square root
followed, so each routine also runs the others it calls. The moving platforms, the vertical step,
the movement resolution, destination occupancy, the command dispatch, the ground and terrain
movement starts, the reservation release and the recovery are stubs: each call is recorded with
its arguments and a snapshot of the fields the routines write, and the step, the resolution and
occupancy return queued answers.

The glide divided by zero without trapping (#342): a zero height sum and a step ratio of zero
take the R3000's zero-divisor quotients, and both are generated.
"""

from __future__ import annotations

import random
from collections import Counter

from differential import (
    Image,
    Machine,
    call_problems,
    field_problems,
    pack_fields,
    packed,
    placed_state,
    s16,
    s32,
    store_problems,
)

ID = "F01729, F01730, F01732-F01734"
ENTRY, END = 0x800E02B4, 0x800E1388
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "flight_movement_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\FlightMovement.cpp",
    r"Native\Game\src\Battle\Ai\FlightTerrain.cpp",
    r"Native\Game\src\Field\TerrainHeight.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 3072, 1729
MUTANT_VECTORS = 768

OBSTRUCTION = 0x800E02B4
CAN_FLY = 0x800E0678
LOWEST_REACHABLE = 0x800E0850
TRANSITION = 0x800E0918
MOVE = 0x800E0A68
DIRECTIONAL = 0x800E1238
FOLLOWED = (OBSTRUCTION, CAN_FLY, LOWEST_REACHABLE, TRANSITION, MOVE, DIRECTIONAL,
            0x8004137C, 0x80040F24, 0x80040F28, 0x80041B94,  # atan, cos, sin, rsqrt
            0x800E42E0, 0x800E42EC, 0x800E4308, 0x800E4318, 0x800E4320,  # ground heights
            0x800E45F4, 0x800E4660, 0x800E6A6C)  # squared distances, segment minimum
PLATFORMS = 0x8008D2C0
STEP = 0x800E4764
RESOLVE = 0x800E3600
OCCUPANCY = 0x800D954C
COMMAND = 0x800DEEFC
TOWARD = 0x800E4B2C
GROUND = 0x800E4B70
RELEASE = 0x800E678C
BEGIN_TERRAIN = 0x800E4B10
ALTERNATE = 0x800E4B18
RECOVER = 0x800E2CCC

DIRECTION_CODES = 0x800F16EC
ACTOR_LIST = 0x800F1928
ACTOR_RECORDS = 0x800F4538
ROOM_AI_STATE = 0x800F58BC
SCRATCH = 0x1F800000
FRAME_ACTORS = 0x1F80037C
HEIGHT_MAP = 0x1F8003BC
TILE_FLAGS = 0x1F8003C0
MOVEMENT_RESULT = 0x1F8003C8
CACHE = 0x1F8003D0
GRID_BOUNDS = 0x1F8003DC
SCRATCH_DIRECTIONS = 0x1F8003EC

DATA = 0x80500000
HEIGHTS = DATA
FLAGS = DATA + 0x800
ROOM = DATA + 0x1000
MODEL = DATA + 0x1040
NODES, NODE_SIZE = DATA + 0x1100, 8
OBSTACLE_MODELS, OBSTACLE_MODEL_SIZE = DATA + 0x1200, 0x30
OBSTACLES, OBSTACLE_SIZE = DATA + 0x1800, 0x100
STATE, STATE_SIZE = DATA + 0x2800, 0x230
RECORDS, RECORD_SIZE = DATA + 0x3000, 0x640
SLOTS, TARGETS = 16, 17
DATA_SIZE = RECORDS - DATA + TARGETS * RECORD_SIZE
GRID = 32
FLIES = 0x200000
NO_FLIGHT = 0x40
SPECIAL_TERRAIN = 0x20
VECTOR_INTENT = 8
FALL_SPEED = -0x1000
K_OBSTRUCTION, K_CAN_FLY, K_TRANSITION, K_MOVE, K_DIRECTIONAL = range(5)
ENTRIES = {K_OBSTRUCTION: OBSTRUCTION, K_CAN_FLY: CAN_FLY, K_TRANSITION: TRANSITION, K_MOVE: MOVE,
           K_DIRECTIONAL: DIRECTIONAL}
KIND_NAMES = ("F01729 obstruction", "F01730 flight test", "F01732 transition", "F01733 flight",
              "F01734 directional")

# FlightMover in declaration order, less the model's flag: name, offset, size, signed.
FIELDS = (
    ("close_approach", 0x00, 1, False), ("movement_disabled", 0x07, 1, False), ("tile", 0x34, 4, False),
    ("previous_tile", 0x38, 4, False), ("x", 0x4C, 2, True), ("y", 0x4E, 2, True), ("z", 0x50, 2, True),
    ("actor_id", 0x88, 1, False), ("radius", 0x92, 2, False), ("max_range", 0xB3, 1, False),
    ("body_height", 0xB8, 2, True), ("ground_clearance", 0xBC, 2, True), ("flight_height", 0xBE, 2, True),
    ("approach_target", 0xE5, 1, False), ("movement_flags", 0x15E, 2, False),
    ("destination", 0x168, 4, False), ("move_x", 0x17C, 4, True), ("move_z", 0x180, 4, True),
    ("move_y", 0x184, 4, True), ("heading", 0x188, 2, True), ("stall_counter", 0x228, 4, True),
)
NAMES = [name for name, *_ in FIELDS]
# The fields a stubbed call records, as the driver's Note lists them.
SNAPSHOT = ("movement_disabled", "move_x", "move_z", "move_y", "heading", "stall_counter")
RECORD = 3 + len(SNAPSHOT)
(C_PLATFORMS, C_OCCUPANCY, C_COMMAND, C_TOWARD, C_GROUND, C_RELEASE, C_BEGIN, C_ALTERNATE, C_STEP,
 C_RESOLVE, C_RECOVER) = range(11)
CODE_NAMES = ("platforms", "occupancy", "command", "toward", "ground", "release", "begin", "alternate",
              "step", "resolve", "recover")
# Added to a call's code when the original passed something other than the actor.
NOT_THE_ACTOR = 100
# The cache bytes the driver prints: the three bit sets and the eight answers.
CACHE_BYTES = (0, 1, 2, *range(4, 12))

MUTANTS = {
    # func_800E02B4
    "cache-bit-1": {0x800E02F4: 0x30420002},
    "cached-answer-plus-2": {0x800E0358: 0x24420002},
    "platform-reach-0x50": {0x800E0468: 0x24650050},
    "platform-slack-0x40": {0x800E046C: 0x24420040},
    "platform-behind-ignored": {0x800E0404: 0x00000000},
    "self-not-skipped": {0x800E050C: 0x00000000},
    "reach-without-mover-radius": {0x800E059C: 0x00601821},
    "actor-height-quarter": {0x800E05CC: 0x00021883},
    "clear-answer-not-cached": {0x800E064C: 0x00000000},
    # func_800E0678
    "test-behind-too": {0x800E06D0: 0x24060000},
    "max-x-ignored": {0x800E0718: 0x00000000},
    "flag-0x20": {0x800E0778: 0x30420020},
    "previous-tile-ignored": {0x800E079C: 0x00000000},
    "ceiling-compare-reversed": {0x800E07CC: 0x0062102A},
    "open-not-cached": {0x800E07F8: 0x00000000},
    # func_800E0918
    "transition-flag-2-only": {0x800E095C: 0x30420002},
    "near-17": {0x800E097C: 0x2C420011},
    "transition-ceiling-reversed": {0x800E09E4: 0x0062102A},
    "lands-on-zero": {0x800E0A18: 0x0440000C},
    "toward-is-ground": {0x800E0948: 0x24524B70},
    # func_800E0A68
    "movement-enabled": {0x800E0AD4: 0x00000000},
    "adjacent-3": {0x800E0B3C: 0x2C420003},
    "hover-flag-0x20": {0x800E0B70: 0x30420020},
    "heading-base-0x400": {0x800E0C40: 0x24420400},
    "settle-special-0x40": {0x800E0DBC: 0x30420040},
    "collision-quarter": {0x800E0E38: 0x00021082},
    "heading-result-6": {0x800E0EE0: 0x24020006},
    "heading-shift-8": {0x800E0F04: 0x00028200},
    "fall-0x800": {0x800E0F28: 0x2402F800},
    "clearance-from-flight": {0x800E0FB4: 0x862400BE},
    "zero-sum-root-plus-1": {0x800E1044: 0x24431001},
    "climb-compare-reversed": {0x800E107C: 0x0242102A},
    "floor-compare-reversed": {0x800E1088: 0x0094102A},
    "climb-stall-kept": {0x800E10D0: 0x00000000},
    "glide-divide-signed": {0x800E1110: 0x0065001A},
    "vector-intent-9": {0x800E11EC: 0x24050009},
    # func_800E1238
    "directional-mode-2": {0x800E1280: 0x24060002},
    "directional-movement-enabled": {0x800E128C: 0x00000000},
    "unresolved-not-released": {0x800E12AC: 0x00000000},
    "directional-stall-kept": {0x800E1368: 0x00000000},
    "transition-not-toward": {0x800E124C: 0x24060001},
}

_battle: Image | None = None


def battle() -> Image:
    global _battle
    if _battle is None:
        _battle = Image.load("BATTLE/BATTLE.PRG")
    return _battle


def tile(rng: random.Random) -> int:
    return rng.randrange(GRID)


def direction_step(code: int) -> tuple[int, int]:
    """DecodeDirectionCode: two signed 2-bit components, x low and z at bit 4."""
    def component(value: int) -> int:
        value &= 3
        return value - 4 if value & 2 else value
    return component(code), component(code >> 4)


def height(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.6:
        return rng.randrange(4)
    if roll < 0.8:
        return rng.randrange(0x3FC, 0x400) | rng.getrandbits(6) << 10
    return rng.getrandbits(16)


def tile_flags(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.8:
        return 0
    if roll < 0.9:
        return NO_FLIGHT
    if roll < 0.95:
        return SPECIAL_TERRAIN
    return rng.getrandbits(16)


def ground(cell: int) -> int:
    """The wrapped ground height of a terrain cell (func_800E42EC)."""
    unwrapped = s16(cell << 6 & 0xFFFF) - ((cell & 0x1C00) >> 3)
    value = unwrapped & 0x7FFF
    return value - 0x8000 if value & 0x4000 else value


def intent(rng: random.Random) -> int:
    """A step's command intent, never the vector intent the glide issues itself."""
    return rng.choice([value for value in range(16) if value != VECTOR_INTENT])


def angle(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.3:
        return rng.choice((0, 0x200, 0x400, 0x600, 0x800, 0xA00, 0xC00, 0xE00))
    if roll < 0.85:
        return rng.randrange(-0x1000, 0x1000)
    return s32(rng.getrandbits(32))


def step_size(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.8:
        return rng.choice((0, 1, 2, 8, 16, 32, 64, 128, 256, rng.randrange(1, 0x400)))
    return rng.choice((0x1000, 0x10000, -1, -0x40, s32(rng.getrandbits(32))))


def room(rng: random.Random, kind: int, flat: bool) -> tuple[int, int, int, int, int]:
    if rng.random() < 0.85:
        min_x, min_z = rng.randrange(0, 6), rng.randrange(0, 6)
        bounds = (min_x, min_z, rng.randrange(max(min_x, 20), GRID), rng.randrange(max(min_z, 20), GRID))
    else:
        bounds = (rng.randrange(GRID), rng.randrange(GRID), rng.randrange(GRID), rng.randrange(GRID))
    if flat or rng.random() < (0.2 if kind == K_TRANSITION else 0.45):
        ceiling = 0
    elif kind == K_TRANSITION and rng.random() < 0.6:
        # Above most generated ground, so the transition's ceiling test passes.
        ceiling = rng.randrange(0x100, 0x800)
    else:
        ceiling = rng.choice((rng.randrange(-0x2000, 0x800), rng.randrange(-0x200, 0x100), -1, 1))
    return (*bounds, ceiling)


def fields(rng: random.Random, kind: int, mode: str) -> dict:
    tx, tz = tile(rng), tile(rng)
    here = packed(rng, tx, tz)
    roll = rng.random()
    if roll < 0.25 or mode == "hover" and roll < 0.5:
        destination = packed(rng, tx, tz)
    elif mode == "hover":
        step = rng.choice(((1, 0), (-1, 0), (0, 1), (0, -1)))
        destination = packed(rng, min(max(tx + step[0], 0), GRID - 1), min(max(tz + step[1], 0), GRID - 1))
    elif roll < 0.5:
        destination = packed(rng, min(max(tx + rng.randrange(-1, 2), 0), GRID - 1),
                             min(max(tz + rng.randrange(-1, 2), 0), GRID - 1))
    elif kind == K_TRANSITION and roll < 0.65:
        # Four tiles along an axis: a squared distance of 16, just outside "near".
        if rng.random() < 0.5:
            destination = packed(rng, tx + 4 if tx + 4 < GRID else tx - 4, tz)
        else:
            destination = packed(rng, tx, tz + 4 if tz + 4 < GRID else tz - 4)
    else:
        destination = packed(rng, tile(rng), tile(rng))
    if mode in ("flat", "close", "level") and (destination ^ here) & 0xFF00FF == 0:
        destination = packed(rng, (tx + rng.randrange(1, GRID)) % GRID, tz)
    dx, dz = destination & 0xFF, destination >> 16 & 0xFF
    if mode == "close":
        # A glide that ends within a unit or two of the goal: the step ratio rounds to zero.
        x, z = dx * 128 + 64 + rng.choice((-1, 1, 0, 2)), dz * 128 + 64 + rng.choice((0, 1, -1))
    elif kind in (K_MOVE, K_DIRECTIONAL) or rng.random() < 0.9:
        x, z = tx * 128 + rng.randrange(128), tz * 128 + rng.randrange(128)
    else:
        x, z = s16(rng.getrandbits(16)), s16(rng.getrandbits(16))
    flat = mode == "flat"
    flags = rng.getrandbits(16)
    if rng.random() < 0.6:
        flags &= rng.choice((0x06, 0x02, 0x04, 0x06, 0))
    if kind == K_TRANSITION and rng.random() < 0.8:
        flags |= rng.choice((0x02, 0x04, 0x06))
    if mode != "plain":
        flags &= ~0x06
    clearance = 0 if flat else rng.randrange(-0x100, 0x100)
    flight_height = 0 if flat else rng.choice((0, rng.randrange(-0x200, 0x200)))
    y = rng.randrange(-0x400, 0x400) if rng.random() < 0.85 else s16(rng.getrandbits(16))
    if mode == "level":
        # Low ground with the actor just above the glide floor: the glide's
        # height change fits in one step, so it issues the vector command.
        clearance, flight_height = rng.randrange(0x40), rng.randrange(0x40, 0x100)
        y = clearance + rng.randrange(0x40)
    return {
        "close_approach": rng.choice((0, 1, 1, rng.getrandbits(8))) if mode == "plain" else int(mode == "hover"),
        "movement_disabled": rng.choice((0, 1, rng.getrandbits(8))),
        "tile": here,
        "previous_tile": packed(rng, tx + rng.randrange(-1, 2), tz + rng.randrange(-1, 2))
        if rng.random() < 0.3 else rng.getrandbits(32),
        "x": x, "y": y,
        "z": z,
        "actor_id": rng.randrange(1, SLOTS),
        "radius": rng.randrange(0x80) if rng.random() < 0.9 else rng.getrandbits(16),
        "max_range": rng.randrange(0x40) if rng.random() < 0.9 else rng.getrandbits(8),
        "body_height": rng.randrange(0x100) if rng.random() < 0.9 else s16(rng.getrandbits(16)),
        "ground_clearance": clearance,
        "flight_height": flight_height,
        "approach_target": rng.randrange(TARGETS),
        "movement_flags": flags,
        "destination": destination,
        "move_x": s32(rng.getrandbits(32)), "move_z": s32(rng.getrandbits(32)),
        "move_y": s32(rng.getrandbits(32)),
        "heading": s16(rng.getrandbits(16)),
        "stall_counter": s32(rng.getrandbits(32)),
    }


def obstacles(rng: random.Random, f: dict) -> list[tuple[int, ...]]:
    result = []
    for _ in range(SLOTS):
        roll = rng.random()
        actor_id = 0 if roll < 0.2 else f["actor_id"] if roll < 0.3 else rng.randrange(1, 256)
        result.append((actor_id, rng.randrange(0x60), rng.randrange(0x30), rng.randrange(0x100),
                       s16(f["x"] + rng.randrange(-0x300, 0x300)), s16(f["y"] + rng.randrange(-0x100, 0x100)),
                       s16(f["z"] + rng.randrange(-0x300, 0x300))))
    return result


def vector(rng: random.Random, number: int) -> dict:
    image = battle()
    kind = number % 5 if number < 20 else rng.choices(range(5), (18, 20, 14, 33, 15))[0]
    mode = "plain"
    if kind == K_MOVE:
        mode = rng.choices(("plain", "flat", "close", "level", "hover"), (76, 6, 6, 6, 6))[0]
    flat = mode == "flat"
    f = fields(rng, kind, mode)
    codes = ([image.read(DIRECTION_CODES + i, 1) for i in range(8)] if rng.random() < 0.85
             else [rng.getrandbits(8) for _ in range(8)])
    if flat:
        heights = [0] * (GRID * GRID)
    elif mode == "level":
        heights = [rng.randrange(4) for _ in range(GRID * GRID)]
    else:
        heights = [height(rng) for _ in range(GRID * GRID)]
    flags = [tile_flags(rng) for _ in range(GRID * GRID)]
    destination = f["destination"]
    destination_index = (destination >> 16 & 0xFF) * GRID + (destination & 0xFF)
    if mode == "hover" or rng.random() < 0.4:
        flags[destination_index] |= NO_FLIGHT
    elif rng.random() < 0.5:
        flags[destination_index] |= SPECIAL_TERRAIN
    if rng.random() < 0.6:
        cache = [0] * 11
    else:
        cache = [rng.getrandbits(8) for _ in range(3)] + [rng.choice((0, 1, rng.randrange(2, 18)))
                                                        for _ in range(8)]
        # A real flight test never caches an off-grid neighbour as open; the
        # original would read outside the height map for one.
        tile_x, tile_z = f["tile"] & 0xFF, f["tile"] >> 16 & 0xFF
        for direction, code in enumerate(codes):
            dx, dz = direction_step(code)
            if not (0 <= tile_x + dx < GRID and 0 <= tile_z + dz < GRID):
                cache[1] &= ~(1 << direction)
    platform_count = 0 if rng.random() < 0.6 else rng.randrange(1, 5)
    platforms = []
    for _ in range(platform_count):
        if rng.random() < 0.5:
            # Just outside the height window, where the original's slack decides.
            window = ((f["body_height"] + 0x20) >> 1) + rng.randrange(1, 0x11)
            platforms.append((s16(f["x"] + rng.randrange(-0x40, 0x41)),
                              s16(f["y"] + rng.choice((-window, window))),
                              s16(f["z"] + rng.randrange(-0x40, 0x41)), rng.randrange(8)))
        else:
            platforms.append((s16(f["x"] + rng.randrange(-0x180, 0x180)), s16(f["y"] + rng.randrange(-0x80, 0x80)),
                              s16(f["z"] + rng.randrange(-0x180, 0x180)), rng.randrange(8)))
    actors = [rng.randrange(SLOTS) for _ in range(rng.choice((0, 0, 1, 2, 3, 4, 6)))]
    targets = []
    for _ in range(TARGETS):
        on_destination = rng.random() < 0.5
        targets.append((s16(f["y"] + rng.randrange(-0x200, 0x200)),
                        destination & 0xFF if on_destination else tile(rng),
                        destination >> 16 & 0xFF if on_destination else tile(rng),
                        rng.randrange(0x200) if rng.random() < 0.9 else rng.getrandbits(16)))
    steps = [(int(rng.random() < 0.85), step_size(rng), intent(rng)) for _ in range(2)]
    if mode == "close":
        steps = [(1, 0, intent(rng)) for _ in range(2)]
    elif mode == "flat":
        steps = [(1, step_size(rng) & 0xFF, intent(rng)) for _ in range(2)]
    elif mode == "level":
        steps = [(1, rng.randrange(0x10, 0x81), intent(rng)) for _ in range(2)]
    elif mode == "hover":
        # Within a step of the hover height, but often a step or more from
        # the tile centre, so the hover heading decides the move.
        steps = [(1, rng.randrange(0x10, 0x41), intent(rng)) for _ in range(2)]
        here = ground(heights[(f["tile"] >> 16 & 0xFF) * GRID + (f["tile"] & 0xFF)])
        f["y"] = s16(here + f["flight_height"] + rng.randrange(-0x10, 0x11))
        target = f["approach_target"]
        targets[target] = (s16(here + rng.randrange(0x100)), *targets[target][1:])
    resolutions = [(rng.choice((5, 7, 5, 7, 5, 7, 0, 1, 3, 6, rng.getrandbits(8))), s16(rng.getrandbits(16)))
                   for _ in range(2)]
    if mode != "plain":
        resolutions[0] = (7, resolutions[0][1])
    return {
        "kind": kind, "mode": mode, "fields": f, "flies": mode != "plain" or rng.random() < 0.75,
        "model_flags": rng.getrandbits(32) & ~FLIES,
        "argument": rng.randrange(8) if kind in (K_OBSTRUCTION, K_CAN_FLY) else angle(rng),
        "flag": rng.randrange(2),
        "codes": codes, "room": room(rng, kind, mode in ("flat", "level")), "heights": heights,
        "flags": flags, "cache": cache,
        "platforms": platforms, "actors": actors, "obstacles": obstacles(rng, f), "targets": targets,
        "steps": steps, "resolutions": resolutions,
        "occupancy": [rng.choice((-1, 0, 1, 2, s32(rng.getrandbits(32)))) for _ in range(2)],
        "state": rng.randbytes(STATE_SIZE),
        "obstacle_states": rng.randbytes(SLOTS * OBSTACLE_SIZE),
        "record_bytes": rng.randbytes(TARGETS * 2),
    }


def put16(block: bytearray, offset: int, value: int) -> None:
    block[offset:offset + 2] = (value & 0xFFFF).to_bytes(2, "little")


def put32(block: bytearray, offset: int, value: int) -> None:
    block[offset:offset + 4] = (value & 0xFFFFFFFF).to_bytes(4, "little")


def data_block(v: dict) -> bytes:
    block = bytearray(DATA_SIZE)
    for index, cell in enumerate(v["heights"]):
        put16(block, HEIGHTS - DATA + index * 2, cell)
    for index, cell in enumerate(v["flags"]):
        put16(block, FLAGS - DATA + index * 2, cell)
    min_x, min_z, max_x, max_z, ceiling = v["room"]
    block[ROOM - DATA:ROOM - DATA + 4] = bytes((max_x, max_z, min_x, min_z))
    put32(block, ROOM - DATA + 0x14, ceiling)
    put32(block, MODEL - DATA + 8, v["model_flags"] | (FLIES if v["flies"] else 0))
    for index, actor_id in enumerate(v["actors"]):
        node = NODES - DATA + index * NODE_SIZE
        put32(block, node, NODES + (index + 1) * NODE_SIZE if index + 1 < len(v["actors"]) else 0)
        put32(block, node + 4, actor_id)
    states = OBSTACLES - DATA
    block[states:states + SLOTS * OBSTACLE_SIZE] = v["obstacle_states"]
    for index, (actor_id, radius, max_range, body, x, y, z) in enumerate(v["obstacles"]):
        state = states + index * OBSTACLE_SIZE
        model = OBSTACLE_MODELS + index * OBSTACLE_MODEL_SIZE
        put32(block, state + 0x58, model)
        block[state + 0x88] = actor_id
        put16(block, state + 0x92, radius)
        block[state + 0xB3] = max_range
        put16(block, state + 0xB8, body)
        put16(block, model - DATA + 0x1C, x)
        put16(block, model - DATA + 0x1E, y)
        put16(block, model - DATA + 0x20, z)
    for index, (y, tile_x, tile_z, collision) in enumerate(v["targets"]):
        record = RECORDS - DATA + index * RECORD_SIZE
        put16(block, record + 0x1E, y)
        block[record + 0x5C:record + 0x60] = bytes((tile_x, v["record_bytes"][index * 2], tile_z,
                                                    v["record_bytes"][index * 2 + 1]))
        put16(block, record + 0x63E, collision)
    return placed_state(block, STATE - DATA, pack_fields(v["state"], FIELDS, v["fields"]), MODEL)


def run(machine: Machine, v: dict):
    machine.images.append(Image(data_block(v), DATA))
    machine.map_zero(SCRATCH, 0x400)
    for slot in range(SLOTS):
        machine.put(FRAME_ACTORS + slot * 4, OBSTACLES + slot * OBSTACLE_SIZE)
    machine.put(HEIGHT_MAP, HEIGHTS)
    machine.put(TILE_FLAGS, FLAGS)
    cache = v["cache"]
    machine.store(CACHE, bytes((*cache[:3], 0, *cache[3:])))
    machine.put(GRID_BOUNDS, GRID, 1)
    machine.put(GRID_BOUNDS + 1, GRID, 1)
    machine.store(SCRATCH_DIRECTIONS, bytes(v["codes"]))
    machine.put(ROOM_AI_STATE, ROOM)
    machine.put(ACTOR_LIST, NODES if v["actors"] else 0)
    for index in range(TARGETS):
        machine.put(ACTOR_RECORDS + index * 4, RECORDS + index * RECORD_SIZE)
    machine.follow.update(FOLLOWED)
    records: list[list[int]] = []
    steps, resolutions, answers = list(v["steps"]), list(v["resolutions"]), list(v["occupancy"])

    def record(m: Machine, code: int, argument: int = 0, extra: int = 0, actor: bool = True) -> None:
        if actor and m.r[4] != STATE:
            code += NOT_THE_ACTOR
        records.append([code, argument, extra, *m.load_fields(STATE, FIELDS, SNAPSHOT)])

    def platforms(m: Machine) -> None:
        record(m, C_PLATFORMS, actor=False)
        for index, contact in enumerate(v["platforms"]):
            for offset, value in enumerate(contact):
                m.put(m.r[4] + index * 8 + offset * 2, value, 2)
        m.r[2] = len(v["platforms"])

    def step(m: Machine) -> None:
        record(m, C_STEP)
        present, size, intent = steps.pop(0) if steps else (0, 0, 0)
        if present:
            m.put(m.r[5], size)
            m.put(m.r[6], intent)
        m.r[2] = present

    def resolve(m: Machine) -> None:
        record(m, C_RESOLVE, s32(m.r[5]), s32(m.r[6]))
        if resolutions:
            result, heading = resolutions.pop(0)
            m.put(STATE + 0x188, heading, 2)
        else:
            result = 0
        m.put(MOVEMENT_RESULT, result)

    def occupy(m: Machine) -> None:
        record(m, C_OCCUPANCY, m.r[5])
        m.r[2] = (answers.pop(0) if answers else 0) & 0xFFFFFFFF

    machine.stub(PLATFORMS, platforms)
    machine.stub(STEP, step)
    machine.stub(RESOLVE, resolve)
    machine.stub(OCCUPANCY, occupy)
    machine.stub(COMMAND, lambda m: record(m, C_COMMAND, s32(m.r[5])))
    machine.stub(TOWARD, lambda m: record(m, C_TOWARD, s32(m.r[5])))
    machine.stub(GROUND, lambda m: record(m, C_GROUND, s32(m.r[5])))
    machine.stub(RELEASE, lambda m: record(m, C_RELEASE))
    machine.stub(BEGIN_TERRAIN, lambda m: record(m, C_BEGIN))
    machine.stub(ALTERNATE, lambda m: record(m, C_ALTERNATE))
    machine.stub(RECOVER, lambda m: record(m, C_RECOVER))
    kind = v["kind"]
    machine.r[4] = STATE
    machine.r[5] = v["argument"] & 0xFFFFFFFF
    machine.r[6] = v["flag"]
    machine.execute(ENTRIES[kind])
    observation = {"result": s32(machine.r[2]), "fields": machine.load_fields(STATE, FIELDS, NAMES),
                   "cache": [machine.byte(CACHE + offset) for offset in CACHE_BYTES],
                   "records": records, "stores": machine.stores}
    f = v["fields"]
    inputs = [kind, v["argument"], v["flag"], *v["codes"], *v["room"], *v["heights"], *v["flags"], *cache,
              *(f[name] for name in NAMES), int(v["flies"]),
              len(v["platforms"]), *(value for contact in v["platforms"] for value in contact),
              len(v["actors"]), *v["actors"],
              *(value for obstacle in v["obstacles"] for value in obstacle),
              *(value for target in v["targets"] for value in target),
              len(v["steps"]), *(value for step_entry in v["steps"] for value in step_entry),
              len(v["resolutions"]), *(value for entry in v["resolutions"] for value in entry),
              len(v["occupancy"]), *v["occupancy"]]
    return observation, inputs


def allowed_stores() -> list[tuple[int, int]]:
    spans = [(STATE + offset, STATE + offset + size) for _, offset, size, _ in FIELDS]
    return [*spans, (CACHE, CACHE + 12), (0x80400000 - 0x4000, 0x80400000)]


ALLOWED = allowed_stores()


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    problems = []
    if v["kind"] in (K_OBSTRUCTION, K_CAN_FLY, K_TRANSITION) and native[1] != obs["result"]:
        problems.append(f"result native {native[1]} original {obs['result']}")
    start = 2
    fields = native[start:start + len(FIELDS)]
    start += len(FIELDS)
    cache = native[start:start + len(CACHE_BYTES)]
    start += len(CACHE_BYTES)
    count = native[start]
    start += 1
    records = [native[start + i * RECORD:start + (i + 1) * RECORD] for i in range(count)]
    problems += field_problems(NAMES, fields, obs["fields"])
    if cache != obs["cache"]:
        problems.append(f"cache native {cache} original {obs['cache']}")
    problems += call_problems(records, obs["records"])
    return problems + store_problems(obs["stores"], ALLOWED)


def path(v: dict, obs: dict) -> str:
    """The calls a vector made, as names, with the obstruction answer for F01729."""
    calls = [CODE_NAMES[record[0] % NOT_THE_ACTOR] for record in obs["records"]
             if record[0] % NOT_THE_ACTOR != C_PLATFORMS]
    kind = v["kind"]
    if kind == K_OBSTRUCTION:
        tested = any(record[0] == C_PLATFORMS for record in obs["records"])
        answer = obs["result"]
        return ("tested " if tested else "cached ") + ("clear" if answer == 0 else "platform/actor 0"
                                                       if answer == 1 else "actor")
    if kind == K_CAN_FLY:
        bit = 1 << v["argument"]
        if v["cache"][0] & bit:
            return "cached"
        if not obs["cache"][0] & bit:
            return "crosses the ceiling"
        return "open" if obs["result"] else "closed"
    if kind == K_TRANSITION:
        return "+".join(calls) or ("changed" if obs["result"] else "unchanged")
    moved = obs["fields"][NAMES.index("move_y")]
    codes = [record[0] for record in obs["records"]]
    if kind == K_MOVE and C_RESOLVE in codes and C_COMMAND in codes:
        if v["resolutions"][0][0] == 5:
            return "heading flight" + (" fell" if moved == FALL_SPEED else "")
        if codes.count(C_STEP) > 1:
            return "glide to directional" + (" fell" if moved == FALL_SPEED else "")
        command = obs["records"][codes.index(C_COMMAND)]
        if command[1] == VECTOR_INTENT:
            return "glide vector"
        if moved == FALL_SPEED:
            return "glide fell"
        f = v["fields"]
        goal_x = (f["destination"] & 0xFF) * 128 + 64
        goal_z = (f["destination"] >> 16 & 0xFF) * 128 + 64
        if (obs["fields"][NAMES.index("move_x")] == goal_x - (f["x"] & 0xFFFF)
                and obs["fields"][NAMES.index("move_z")] == goal_z - f["z"]):
            return "climb"
        return "glide step"
    suffix = " fell" if moved == FALL_SPEED and C_COMMAND in codes else ""
    return "+".join(calls) + suffix if calls else "none"


def describe(image: Image, results: list) -> list[str]:
    paths = [Counter() for _ in range(5)]
    modes = Counter()
    for v, obs, _native in results:
        paths[v["kind"]][path(v, obs)] += 1
        if v["mode"] != "plain":
            commands = [record for record in obs["records"] if record[0] == C_COMMAND]
            modes[f"{v['mode']} {'commanded' if commands else 'no command'}"] += 1
    lines = []
    for kind, counter in enumerate(paths):
        common = counter.most_common(14)
        lines.append(f"{KIND_NAMES[kind]} ({sum(counter.values())}): "
                     + ", ".join(f"{name} {count}" for name, count in common))
    lines.append("F01733 targeted glides (flat/close: #342 quotients; level: vector command): "
                 + ", ".join(f"{name} {count}" for name, count in sorted(modes.items())))
    return lines
