"""F01751 (771A8.c func_800E2F5C) obstacle detour step vs GroundNavigation.cpp.

func_800E2F5C advances an active obstacle detour by one decision. It nudges the actor blocking
the turn side into waiting, reverses a detour whose heading is closed before it has turned,
stops at the room's walkable bounds or a tile with the blocked flag, and ends at a boundary
the path passed. Otherwise it compares the cardinal passability mask with the recorded cell
and turns around the obstacle: it stops when the inner side opens again, or it scans from the
inner side for an open heading (walk, 5) or a free neighbouring tile (step, 14, which stores
the tile as the next tile).

The blocker cache, tile edge, destination occupancy, cardinal mask and ground-height lookups
run as original code over a 32x32 room, a 16-actor list, actor records and frame AI states;
the edge test, the occupancy walk and the cardinal mask's GTE scratch registers all execute.
The walk/flight probe the caller passes is a stub that answers from a per-direction table and
records each direction with a snapshot of the state, and the random stream is a stub that
records its range.

The step's next tile takes byte 3 from the caller's $fp, which is the movement mode (0..3) in
the only caller (func_800E3600), so byte 3 is 0. The generated port left that byte
indeterminate (#344); here the whole next_tile word is compared, and the next-tile-tag mutant
shows a byte 3 from another register is detected.
"""

from __future__ import annotations

import random
from collections import Counter

from differential import (
    Fault,
    Image,
    Machine,
    call_problems,
    field_problems,
    near,
    pack_fields,
    position,
    s16,
    s32,
    store_problems,
)

ID = "F01751"
ENTRY, END = 0x800E2F5C, 0x800E3600
MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "obstacle_detour_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\GroundNavigation.cpp",
    r"Native\Game\src\Battle\Ai\NavigationPrimitives.cpp",
    r"Native\Game\src\Field\TerrainHeight.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1751
MUTANT_VECTORS = 768

BLOCKER, EDGE, OCCUPANCY, CARDINAL = 0x800D98E8, 0x800D96C8, 0x800D954C, 0x800E3D5C
GROUND_HEIGHT, HEIGHT_LOOKUP = 0x800E42EC, 0x800E4320
RANDOM = 0x800E45D4
# The caller's probe (0x800D9B6C walk or 0x800E0678 flight); any address outside the modules.
PROBE = 0x80600000

SCRATCH = 0x1F800000
FRAME_AI = 0x1F80037C
HEIGHT_MAP, FLAG_MAP = 0x1F8003BC, 0x1F8003C0
BLOCKER_VALID, BLOCKER_CACHE = 0x1F8003D2, 0x1F8003D4
GRID_BOUNDS = 0x1F8003DC
SCRATCH_DIRECTIONS, DIRECTION_TABLE = 0x1F8003EC, 0x800F16EC
ACTOR_LIST, ACTOR_RECORDS, ROOM_AI_STATE = 0x800F1928, 0x800F4538, 0x800F58BC

DATA = 0x80500000
STATE = DATA
AI_SIZE = 0x200
OTHERS = DATA + 0x1000
RECORD_SIZE = 0x40
RECORDS = DATA + 0x3000
NODES = DATA + 0x3400
BOUNDS = DATA + 0x3480
# The height map's rows 0..255: occupancy reads the height of a wrapped neighbour (x or z 255)
# before its bound check.
HEIGHTS = DATA + 0x4000
# The side check reads the flag map at x or z -1..32 before its bound check.
FLAGS = DATA + 0x8800
DATA_SIZE = 0x9200
ACTORS, GRID, DIRECTIONS = 16, 32, 8
STACK_TOP = 0x80400000

# GroundNavigator in the driver's read order: name, AI state offset, size, signed.
NAV = (
    ("actor_id", 0x88, 1, False), ("gait", 0x01, 1, False), ("is_stuck", 0x02, 1, False),
    ("height_filter", 0x03, 1, False), ("near_target_detour", 0x04, 1, False),
    ("target_distance_filter", 0x05, 1, False), ("movement_disabled", 0x07, 1, False),
    ("terrain_reject_mask", 0x32, 1, False), ("tile", 0x34, 4, False), ("previous_tile", 0x38, 4, True),
    ("x", 0x4C, 2, True), ("y", 0x4E, 2, True), ("z", 0x50, 2, True),
    ("requested_movement_mode", 0x89, 1, False), ("collision_radius", 0x94, 2, False),
    ("step_height", 0xB0, 2, False), ("tile_step_limit", 0xB2, 1, False), ("max_range", 0xB3, 1, False),
    ("ground_area", 0x132, 1, False), ("movement_flags", 0x15E, 2, False),
    ("destination", 0x168, 4, False), ("next_tile", 0x170, 4, False),
    ("navigation_flags", 0x190, 4, False), ("detour_run", 0x199, 1, False), ("repeat_wait", 0x19A, 1, False),
    ("pursuit_e4", 0xE4, 1, False), ("pursuit_e7", 0xE7, 1, False), ("pursuit_e8", 0xE8, 1, False),
)
NAV_NAMES = [name for name, *_ in NAV]
# The fields the detour may write, in the driver's output order.
RESULT_FIELDS = ("gait", "target_distance_filter", "pursuit_e4", "pursuit_e7", "pursuit_e8", "next_tile",
                 "navigation_flags", "repeat_wait")
# The state each probe call sees.
SNAPSHOT = ("gait", "target_distance_filter", "repeat_wait", "navigation_flags")
PROBE_RECORD = 1 + len(SNAPSHOT)
NOT_THE_ACTOR = 100

STOPPED, CHANGED = 1 << 16, 1 << 17
GROUND_BLOCKED_BIT, FLIGHT_BLOCKED_BIT = 11, 12

MUTANTS = {
    "no-step-limit": {0x800E2FA8: 0x00000000},
    "side-flipped": {0x800E2FBC: 0x14400003},
    "nudge-ignores-commitment": {0x800E3008: 0x30420000},
    "nudge-wait-base": {0x800E3030: 0x24420005},
    "reverse-when-near": {0x800E3040: 0x00000000},
    "reverse-after-change": {0x800E304C: 0x30420000},
    "reverse-occupied-open": {0x800E3100: 0x04410016},
    "reverse-keeps-side": {0x800E3120: 0x00000000},
    "reverse-quarter-turn": {0x800E3148: 0x24420002},
    "room-exit-flag-bit": {0x800E31C0: 0x00021007},
    "room-exit-keeps-pursuit": {0x800E3228: 0x00000000},
    "bound-z-far-side": {0x800E3250: 0x00A3102A},
    "bound-x-near-side": {0x800E3338: 0x0066102A},
    "cell-shift": {0x800E3384: 0x00111442},
    "turn-direction": {0x800E33B8: 0x2415FFFF},
    "self-nudge-ignores-commitment": {0x800E340C: 0x30420000},
    "self-nudge-wait-base": {0x800E3430: 0x24420005},
    "visited-complete": {0x800E3470: 0x2403000E},
    "changed-not-marked": {0x800E34C0: 0x00000000},
    "next-tile-tag": {0x800E34F8: 0x02631824},
    "edge-ignored": {0x800E354C: 0x00000000},
    "occupancy-zero-free": {0x800E355C: 0x04400009},
    "next-tile-not-stored": {0x800E356C: 0x00000000},
    "scan-three": {0x800E3594: 0x2A820003},
    "step-intent": {0x800E35B0: 0x2402000D},
    "walk-intent": {0x800E35BC: 0x24020004},
    "end-without-stop": {0x800E35C0: 0x3C020000},
}

# The module's direction codes (0x800F16EC): dx in bits 0-1 and dz in bits 4-5, two's complement.
CODES = (0x30, 0x33, 0x03, 0x13, 0x10, 0x11, 0x01, 0x31)


def component(value: int) -> int:
    value &= 3
    return value - 4 if value > 1 else value


DELTAS = [(component(code), component(code >> 4)) for code in CODES]


def wrapped_height(raw: int) -> int:
    raw &= 0x7FFF
    return raw - 0x8000 if raw & 0x4000 else raw


def cardinal_mask(open_directions: list[int]) -> int:
    return open_directions[0] << 3 | open_directions[2] << 2 | open_directions[4] << 1 | open_directions[6]


def tile_limits(rng: random.Random, tile: int) -> tuple[int, int]:
    if rng.random() < 0.1:
        low, high = rng.randrange(GRID), rng.randrange(GRID)
        return min(low, high), max(low, high)
    if rng.random() < 0.3:
        return max(0, tile - rng.randrange(2)), min(GRID - 1, tile + rng.randrange(2))
    return max(0, tile - rng.randrange(1, 6)), min(GRID - 1, tile + rng.randrange(1, 6))


def bound_tile(rng: random.Random, tile: int, destination: int) -> int:
    if rng.random() < 0.6:
        return 0
    return max(0, min(GRID - 1, rng.choice((tile, destination)) + rng.randrange(-1, 2)))


def placed(rng: random.Random, tile_x: int, tile_z: int, heights: list[int], width: int, depth: int):
    """An actor position on or beside a neighbour of the tile, at about its ground height."""
    if rng.random() < 0.6:
        return position(rng, rng.randrange(GRID)), s16(rng.getrandbits(16)), position(rng, rng.randrange(GRID))
    dx, dz = rng.choice(DELTAS)
    x, z = tile_x + dx, tile_z + dz
    ground = wrapped_height(heights[z * GRID + x]) if 0 <= x < width and 0 <= z < depth else 0
    return (s16(x * 128 + 64 + rng.randrange(-0x60, 0x61)), s16(ground + rng.randrange(-0xA0, 0xA1)),
            s16(z * 128 + 64 + rng.randrange(-0x60, 0x61)))


def navigation_flags(rng: random.Random, tile_x: int, tile_z: int, destination: int, mask: int) -> int:
    steps = rng.choices((0, 1, rng.randrange(2, 128)), (5, 10, 85))[0]
    visited = rng.choice((0, 0xF, 0x7, 0xB, 0xD, 0xE)) if rng.random() < 0.5 else rng.getrandbits(4)
    cell = mask if rng.random() < 0.3 else rng.getrandbits(4)
    bound_x = bound_tile(rng, tile_x, destination & 0xFF)
    bound_z = bound_tile(rng, tile_z, destination >> 16 & 0xFF)
    return (rng.getrandbits(1) | rng.randrange(DIRECTIONS) << 1 | rng.getrandbits(1) << 4 | steps << 5
            | visited << 12 | rng.getrandbits(1) << 16 | (rng.random() < 0.3) << 17 | cell << 18
            | bound_x << 22 | bound_z << 27)


def vector(rng: random.Random, number: int) -> dict:
    width, depth = rng.randrange(4, GRID + 1), rng.randrange(4, GRID + 1)
    self_id = rng.randrange(ACTORS)
    tile_x, tile_z = rng.randrange(width), rng.randrange(depth)
    min_x, max_x = tile_limits(rng, tile_x)
    min_z, max_z = tile_limits(rng, tile_z)
    area = rng.randrange(16)
    base = rng.randrange(-0x400, 0x400)
    heights = []
    for _ in range(GRID * GRID):
        roll = rng.random()
        offset = 0 if roll < 0.5 else rng.randrange(-0x100, 0x101) if roll < 0.8 else rng.randrange(-0x1000, 0x1001)
        heights.append((base + offset if roll < 0.95 else rng.getrandbits(16)) & 0xFFFF)
    flags = []
    for _ in range(GRID * GRID):
        cell = area if rng.random() < 0.8 else rng.randrange(16)
        for bit, chance in ((0x10, 0.04), (0x20, 0.04), (0x80, 0.04), (0x8000, 0.03), (0x800, 0.06),
                            (0x1000, 0.06)):
            cell |= bit if rng.random() < chance else 0
        flags.append(cell | (rng.getrandbits(16) if rng.random() < 0.05 else 0))
    open_chance = rng.uniform(0.1, 0.7)
    open_directions = [int(rng.random() < open_chance) for _ in range(DIRECTIONS)]
    tile = rng.getrandbits(8) << 24 | tile_z << 16 | rng.getrandbits(8) << 8 | tile_x
    destination = near(rng, tile, 6)
    dx, dz = rng.choice(DELTAS)
    roll = rng.random()
    if roll < 0.4:
        middle = 0 if roll < 0.3 else rng.getrandbits(8)
        previous = rng.getrandbits(8) << 24 | (tile_z + dz & 0xFF) << 16 | middle << 8 | (tile_x + dx & 0xFF)
    else:
        previous = near(rng, tile, 3)
    fields = {
        "actor_id": self_id, "gait": rng.getrandbits(8),
        "is_stuck": 0 if rng.random() < 0.7 else rng.choice((1, rng.getrandbits(8))),
        "height_filter": rng.getrandbits(1), "near_target_detour": 0 if rng.random() < 0.7 else rng.randrange(1, 256),
        "target_distance_filter": rng.getrandbits(8), "movement_disabled": int(rng.random() < 0.1),
        "terrain_reject_mask": 0 if rng.random() < 0.6 else rng.getrandbits(8), "tile": tile,
        "previous_tile": previous, "x": position(rng, tile_x), "y": s16(base + rng.randrange(-0x100, 0x101)),
        "z": position(rng, tile_z), "requested_movement_mode": rng.getrandbits(8),
        "collision_radius": rng.randrange(0x80) if rng.random() < 0.9 else rng.getrandbits(16),
        "step_height": rng.choice((0x40, 0x80, 0x100, 0x200, rng.getrandbits(16))),
        "tile_step_limit": rng.randrange(4), "max_range": rng.getrandbits(8),
        "ground_area": area if rng.random() < 0.9 else rng.randrange(16),
        "movement_flags": 0 if rng.random() < 0.4 else rng.getrandbits(16), "destination": destination,
        "next_tile": rng.getrandbits(32),
        "navigation_flags": navigation_flags(rng, tile_x, tile_z, destination, cardinal_mask(open_directions)),
        "detour_run": rng.getrandbits(8), "repeat_wait": 0 if rng.random() < 0.6 else rng.getrandbits(8),
        "pursuit_e4": rng.getrandbits(8), "pursuit_e7": rng.getrandbits(8), "pursuit_e8": rng.getrandbits(8),
    }
    actors = []
    for _ in range(ACTORS):
        x, y, z = placed(rng, tile_x, tile_z, heights, width, depth)
        planned_x, planned_y, planned_z = placed(rng, tile_x, tile_z, heights, width, depth)
        actors.append({
            "moving": 0 if rng.random() < 0.5 else rng.getrandbits(8), "x": x, "y": y, "z": z,
            "planned_x": planned_x, "planned_y": planned_y, "planned_z": planned_z,
            "radius": rng.randrange(0x80), "movement_flags": 0 if rng.random() < 0.6 else rng.getrandbits(16),
            "wait": 0 if rng.random() < 0.6 else rng.getrandbits(8),
        })
    members = rng.sample(range(ACTORS), rng.randrange(1, 9))
    if self_id not in members and rng.random() < 0.3:
        members.insert(rng.randrange(len(members) + 1), self_id)
    blockers = [0 if rng.random() < 0.75 else rng.randrange(1, ACTORS + 1) for _ in range(DIRECTIONS)]
    bit = rng.choice((GROUND_BLOCKED_BIT, FLIGHT_BLOCKED_BIT)) if rng.random() < 0.9 else rng.randrange(32)
    return {
        "width": width, "depth": depth, "limits": (min_x, min_z, max_x, max_z), "heights": heights,
        "flags": flags, "state": rng.randbytes(AI_SIZE), "fields": fields, "actors": actors,
        "members": members, "blockers": blockers, "open": open_directions,
        "randoms": [rng.randrange(4) for _ in range(2)], "bit": bit, "mode": rng.randrange(4),
    }


def data_block(v: dict) -> tuple[bytes, list[int]]:
    """The data image and the navigator fields as the original reads them back."""
    block = bytearray(DATA_SIZE)
    state = pack_fields(v["state"], NAV, v["fields"])
    block[:AI_SIZE] = state
    for index, actor in enumerate(v["actors"]):
        ai = OTHERS - DATA + index * AI_SIZE
        for offset, size, value in ((0x94, 2, actor["radius"]), (0x15E, 2, actor["movement_flags"]),
                                    (0x174, 2, actor["planned_x"]), (0x176, 2, actor["planned_y"]),
                                    (0x178, 2, actor["planned_z"]), (0x19A, 1, actor["wait"])):
            block[ai + offset:ai + offset + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")
        record = RECORDS - DATA + index * RECORD_SIZE
        for offset, size, value in ((0xA, 1, actor["moving"]), (0x1C, 2, actor["x"]), (0x1E, 2, actor["y"]),
                                    (0x20, 2, actor["z"])):
            block[record + offset:record + offset + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")
    for index, member in enumerate(v["members"]):
        node = NODES - DATA + index * 8
        following = NODES + (index + 1) * 8 if index + 1 < len(v["members"]) else 0
        block[node:node + 8] = following.to_bytes(4, "little") + member.to_bytes(4, "little")
    min_x, min_z, max_x, max_z = v["limits"]
    block[BOUNDS - DATA:BOUNDS - DATA + 4] = bytes((max_x, max_z, min_x, min_z))
    for z in range(GRID):
        for x in range(GRID):
            cell = z * GRID + x
            heights = HEIGHTS - DATA + z * 64 + x * 2
            flags = FLAGS - DATA + z * 64 + x * 2
            block[heights:heights + 2] = v["heights"][cell].to_bytes(2, "little")
            block[flags:flags + 2] = v["flags"][cell].to_bytes(2, "little")
    fields = [int.from_bytes(state[offset:offset + size], "little", signed=signed) for _, offset, size, signed in NAV]
    return bytes(block), fields


def run(machine: Machine, v: dict):
    block, fields = data_block(v)
    machine.images.append(Image(block, DATA))
    machine.map_zero(SCRATCH, 0x400)
    self_id = v["fields"]["actor_id"]
    for index in range(ACTORS):
        machine.put(FRAME_AI + index * 4, STATE if index == self_id else OTHERS + index * AI_SIZE)
        machine.put(ACTOR_RECORDS + index * 4, RECORDS + index * RECORD_SIZE)
    machine.put(HEIGHT_MAP, HEIGHTS)
    machine.put(FLAG_MAP, FLAGS)
    # Every direction's blocker is cached this frame, so the blocker probe never runs.
    machine.put(BLOCKER_VALID, 0xFF, 1)
    machine.store(BLOCKER_CACHE, bytes(v["blockers"]))
    machine.put(GRID_BOUNDS, v["width"], 1)
    machine.put(GRID_BOUNDS + 1, v["depth"], 1)
    codes = machine.bytes(DIRECTION_TABLE, DIRECTIONS)
    machine.store(SCRATCH_DIRECTIONS, codes)
    machine.put(ROOM_AI_STATE, BOUNDS)
    machine.put(ACTOR_LIST, NODES)
    machine.follow.update({BLOCKER, EDGE, OCCUPANCY, CARDINAL, GROUND_HEIGHT, HEIGHT_LOOKUP})
    probes: list[list[int]] = []
    ranges: list[list[int]] = []
    answers = list(v["randoms"])

    def probe(m: Machine) -> None:
        direction = s32(m.r[5])
        if not 0 <= direction < DIRECTIONS:
            raise Fault(f"probe direction {direction}")
        code = direction + (NOT_THE_ACTOR if m.r[4] != STATE else 0)
        probes.append([code, *m.load_fields(STATE, NAV, SNAPSHOT)])
        m.r[2] = v["open"][direction]

    def draw(m: Machine) -> None:
        ranges.append([s32(m.r[4])])
        m.r[2] = answers.pop(0) if answers else 0

    machine.stub(PROBE, probe)
    machine.stub(RANDOM, draw)
    machine.r[4], machine.r[5], machine.r[6] = STATE, PROBE, v["bit"]
    machine.r[16], machine.r[30] = STATE, v["mode"]
    machine.execute(ENTRY)
    observation = {
        "result": s32(machine.r[2]), "fields": machine.load_fields(STATE, NAV, RESULT_FIELDS),
        "waits": [machine.load(OTHERS + index * AI_SIZE + 0x19A, 1) for index in range(ACTORS)],
        "probes": probes, "ranges": ranges, "stores": machine.stores,
    }
    min_x, min_z, max_x, max_z = v["limits"]
    inputs = [*codes, v["width"], v["depth"], min_x, min_z, max_x, max_z, *v["heights"], *v["flags"],
              *v["blockers"], *fields]
    for actor in v["actors"]:
        inputs += [actor[name] & 0xFF if name == "moving" else actor[name] for name in
                   ("moving", "x", "y", "z", "planned_x", "planned_y", "planned_z", "radius", "movement_flags",
                    "wait")]
    inputs += [len(v["members"]), *v["members"], *v["open"], len(v["randoms"]), *v["randoms"], v["bit"]]
    return observation, inputs


def allowed_stores(v: dict) -> list[tuple[int, int]]:
    layout = {name: (offset, size) for name, offset, size, _ in NAV}
    spans = [(STATE + layout[name][0], STATE + layout[name][0] + layout[name][1]) for name in RESULT_FIELDS]
    spans += [(OTHERS + index * AI_SIZE + 0x19A, OTHERS + index * AI_SIZE + 0x19B) for index in range(ACTORS)]
    # The callee frames and the argument slot the detour saves the probe in (its caller's frame).
    return [*spans, (STACK_TOP - 0x4000, STACK_TOP + 0x40)]


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    problems = []
    if native[1] != obs["result"]:
        problems.append(f"result native {native[1]} original {obs['result']}")
    at = 2 + len(RESULT_FIELDS)
    problems += field_problems(RESULT_FIELDS, native[2:at], obs["fields"])
    waits = native[at:at + ACTORS]
    others = [index for index in range(ACTORS) if index != v["fields"]["actor_id"]]
    problems += field_problems([f"actor {index} wait" for index in others], [waits[index] for index in others],
                               [obs["waits"][index] for index in others])
    at += ACTORS
    count = native[at]
    probes = [native[at + 1 + i * PROBE_RECORD:at + 1 + (i + 1) * PROBE_RECORD] for i in range(count)]
    at += 1 + count * PROBE_RECORD
    ranges = [[value] for value in native[at + 1:at + 1 + native[at]]]
    problems += call_problems(probes, obs["probes"])
    problems += call_problems(ranges, obs["ranges"])
    return problems + store_problems(obs["stores"], allowed_stores(v))


def describe(image: Image, results: list) -> list[str]:
    outcomes: Counter[str] = Counter()
    events: Counter[str] = Counter()
    flags_index = RESULT_FIELDS.index("navigation_flags")
    pursuit = [RESULT_FIELDS.index(name) for name in ("pursuit_e4", "pursuit_e7", "pursuit_e8")]
    for v, obs, _native in results:
        before = v["fields"]["navigation_flags"]
        after = obs["fields"][flags_index]
        result = obs["result"]
        if result == 5:
            outcomes["walk an open heading (5)"] += 1
        elif result == 14:
            outcomes["step to a free tile (14)"] += 1
        elif after & STOPPED and not before & STOPPED:
            outcomes["stopped (-1)"] += 1
        else:
            outcomes["ended without stopping (-1)"] += 1
        if after & CHANGED and not before & CHANGED:
            events["reversed or turned"] += 1
        if all(obs["fields"][index] == 0 for index in pursuit) and any(
                v["fields"][name] for name in ("pursuit_e4", "pursuit_e7", "pursuit_e8")):
            events["left the room"] += 1
        if obs["ranges"]:
            events["nudged a blocker"] += 1
        if result == 14 and obs["fields"][RESULT_FIELDS.index("next_tile")] >> 24 == 0:
            events["stepped with next_tile byte 3 = 0 (#344)"] += 1
    return [
        "Outcomes: " + ", ".join(f"{name} {count}" for name, count in sorted(outcomes.items())),
        "Events: " + ", ".join(f"{name} {count}" for name, count in sorted(events.items())),
    ]
