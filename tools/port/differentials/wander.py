"""F01741-F01745 (771A8.c:1633-1845) wander movement vs Wander.cpp.

func_800E1BB8 (the wander tile chain) runs directly and under func_800E1EAC (the wander tick),
which also follows the arctangent. The random stream, the adjacency and traversal checks, the
fly/walk direction checks, the destination projection, the locomotion dispatch and the movement
recovery are stubs: each call is recorded with its arguments and, for the calls that take the
actor, a snapshot of the wander fields the routines write, and the checks return queued results.
"""

from __future__ import annotations

import random

from differential import MASK, Image, Machine, s16, s32

ID = "F01741-F01745"
ENTRY, END = 0x800E1BB8, 0x800E23AC
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "wander_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\Wander.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1741
MUTANT_VECTORS = 2048

CHOOSE_TILE = 0x800E1BB8
UPDATE = 0x800E1EAC
RANDOM = 0x800E45D4
ADJACENT = 0x800E4690
TRAVERSE = 0x800D96C8
FLY = 0x800E0678
WALK = 0x800D9B6C
PROJECT = 0x800E1388
DISPATCH = 0x800E50A0
RECOVER = 0x800E2CCC
ARCTANGENT = 0x8004137C

DIRECTION_CODES = 0x800F16EC
ROOM_AI_STATE = 0x800F58BC
SCRATCH = 0x1F800000
TILE_FLAGS = 0x1F8003C0
TERRAIN_REGIONS = 0x1F8003C4
DIRECTION_CACHE = 0x1F8003D0
REGION_WIDTH = 0x1F8003DC
SCRATCH_DIRECTIONS = 0x1F8003EC
ROOM_DATA = 0x1F8003F4

DATA = 0x80500000
FLAGS = DATA
REGIONS = DATA + 0x800
BOUNDS = DATA + 0xD00
OBJECT = DATA + 0xD80
STATE, STATE_SIZE = DATA + 0x1000, 0x190
DATA_SIZE = 0x1200
GRID = 32
FLIES = 0x200000
K_TILE, K_MOVE = range(2)

# WanderActor in declaration order (less flies): name, offset, size, signed.
FIELDS = (
    ("is_stuck", 0x02, 1, False), ("height_filter", 0x03, 1, False), ("stayed_on_tile", 0x08, 1, False),
    ("tile", 0x34, 4, False), ("saved_destination", 0x48, 4, False), ("x", 0x4C, 2, True),
    ("z", 0x50, 2, True), ("move_distance", 0xB4, 4, False), ("phase", 0xC0, 2, False),
    ("timer", 0xC2, 2, False), ("range_squared", 0xC4, 4, True), ("heading", 0xD0, 2, True),
    ("cooldown", 0xD2, 2, False), ("rest_period", 0xD4, 2, False), ("move_period", 0xD6, 2, False),
    ("rate", 0xD8, 1, False), ("goal", 0x11C, 4, False), ("behavior_mode", 0x12D, 1, False),
    ("count", 0x15D, 1, False), ("movement_flags", 0x15E, 2, False), ("destination", 0x168, 4, False),
)
FIELD = {name: (offset, size, signed) for name, offset, size, signed in FIELDS}
# The fields an actor call records, as the driver's Snapshot lists them.
SNAPSHOT = ("is_stuck", "height_filter", "saved_destination", "phase", "timer", "range_squared", "heading",
            "cooldown", "goal", "behavior_mode", "count", "destination")
RECORD = 4 + len(SNAPSHOT)
C_RANDOM, C_ADJACENT, C_TRAVERSE, C_FLY, C_WALK, C_PROJECT, C_DISPATCH, C_RECOVER = range(8)
# Added to an actor call's code when the original passed something other than the actor.
NOT_THE_ACTOR = 100

MUTANTS = {
    # func_800E1BB8
    "range-zero-continues": {0x800E1BE8: 0x00000000},
    "goal-not-reset": {0x800E1C00: 0x00000000},
    "projects-flag-4": {0x800E1C20: 0x30420004},
    "region-bit-by-index": {0x800E1C74: 0x02A21007},
    "traverse-ignored": {0x800E1D10: 0x00000000},
    "stuck-from-height-filter": {0x800E1D1C: 0x92220003},
    "area-mask-0x7": {0x800E1D68: 0x30630007},
    "no-wander-flag-0x10": {0x800E1D9C: 0x30420010},
    "choice-draw-0x800": {0x800E1DD0: 0x24040800},
    "redraw-without-heading": {0x800E1DE0: 0x00000000},
    "opposite-mask-3": {0x800E1DF4: 0x30630003},
    "complete-compare-reversed": {0x800E1E7C: 0x0056102B},
    # func_800E1EAC
    "flight-inverted": {0x800E1F00: 0x14600004},
    "hold-30-ticks": {0x800E1F34: 0x2C42001E},
    "release-draw-45": {0x800E1F48: 0x2C42002D},
    "move-switch-draw-45": {0x800E1F78: 0x2C42002D},
    "jitter-base-zero": {0x800E1F98: 0x34040000},
    "rest-only-period": {0x800E1FB0: 0x00401021},
    "rest-switch-draw-45": {0x800E1FC8: 0x2C42002D},
    "stuck-not-set": {0x800E1FE8: 0x00000000},
    "cache-kept": {0x800E1FF0: 0x00000000},
    "roaming-range-62": {0x800E1FFC: 0x2E62003E},
    "rate-draw-ignored": {0x800E2020: 0x00000000},
    "cooldown-never-restarts": {0x800E2030: 0x10000022},
    "roam-distance-base-0x40": {0x800E204C: 0x24630040},
    "restart-cooldown-30": {0x800E2064: 0x2402001E},
    "range-squared-not-stored": {0x800E2074: 0x00000000},
    "projection-flag-4": {0x800E20E8: 0x30420004},
    "barrier-flag-0x400": {0x800E21B8: 0x30420400},
    "bounds-step-1": {0x800E21C0: 0x24E20001},
    "one-bounds-check": {0x800E21D4: 0x29020001},
    "roam-locomotion-0": {0x800E21EC: 0x24050000},
    "height-filter-not-set": {0x800E21F8: 0x00000000},
    "start-heading-0": {0x800E224C: 0x24020000},
    "count-not-reset": {0x800E225C: 0x00000000},
    "complete-destination-not-set": {0x800E22A8: 0x00000000},
    "goal-x-times-64": {0x800E22CC: 0x00021180},
    "goal-centre-0x20": {0x800E22D8: 0x2484FFE0},
    "goal-angle-mask-0x7ff": {0x800E22FC: 0x304607FF},
    "saved-destination-not-stored": {0x800E2304: 0x00000000},
    "goal-locomotion-1": {0x800E230C: 0x24050001},
    "arrival-ignores-x": {0x800E2340: 0x34840000},
    "blocked-phase-kept": {0x800E2374: 0x00000000},
}

_battle: Image | None = None


def battle() -> Image:
    global _battle
    if _battle is None:
        _battle = Image.load("BATTLE/BATTLE.PRG")
    return _battle


def packed_tile(rng: random.Random, x: int, z: int) -> int:
    return rng.getrandbits(8) << 24 | z << 16 | rng.getrandbits(8) << 8 | x


def inner(rng: random.Random) -> int:
    return rng.randrange(2, GRID - 2)


def draw(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.2:
        return 0
    if roll < 0.45:  # around the 44-in-128 timer switch
        return rng.choice((43, 44, 45)) | rng.getrandbits(25) << 7
    return rng.getrandbits(32)


def vector(rng: random.Random, number: int) -> dict:
    image = battle()
    kind = K_TILE if rng.random() < 0.4 else K_MOVE
    codes = ([image.read(DIRECTION_CODES + i, 1) for i in range(8)] if rng.random() < 0.7
             else [rng.getrandbits(8) for _ in range(8)])
    width = rng.randrange(GRID, 41)
    regions = [rng.getrandbits(8) & rng.getrandbits(8) for _ in range(width * GRID)]
    flags = []
    for _ in range(GRID * GRID):
        cell = rng.randrange(2) if rng.random() < 0.8 else rng.getrandbits(4)
        for bit, chance in ((0x20, 0.25), (0x800, 0.15), (0x400, 0.15)):
            if rng.random() < chance:
                cell |= bit
        if rng.random() < 0.1:
            cell |= rng.getrandbits(16)
        flags.append(cell)
    tx, tz = inner(rng), inner(rng)
    bounds = []
    for t in (tx, tz):
        if rng.random() < 0.7:
            bounds.append((rng.randrange(0, t), rng.randrange(t + 1, GRID)))
        else:
            low = rng.randrange(GRID)
            bounds.append((low, rng.randrange(low, GRID)))
    (min_x, max_x), (min_z, max_z) = bounds
    f = {name: s32(rng.getrandbits(32)) if signed and size == 4 else
         (s16(rng.getrandbits(16)) if signed else rng.getrandbits(size * 8))
         for name, _, size, signed in FIELDS}
    f["tile"] = packed_tile(rng, tx, tz)
    if rng.random() < 0.3:
        f["goal"] = packed_tile(rng, tx, tz)
    elif rng.random() < 0.5:
        f["goal"] = packed_tile(rng, min(max(tx + rng.randrange(-1, 2), 2), GRID - 3),
                                min(max(tz + rng.randrange(-1, 2), 2), GRID - 3))
    else:
        f["goal"] = packed_tile(rng, inner(rng), inner(rng))
    for name in ("is_stuck", "height_filter", "stayed_on_tile"):
        f[name] = rng.choice((0, 1, f[name]))
    f["x"] = s16(tx * 128 + rng.randrange(-200, 200)) if rng.random() < 0.8 else f["x"]
    f["z"] = s16(tz * 128 + rng.randrange(-200, 200)) if rng.random() < 0.8 else f["z"]
    if rng.random() < 0.4:
        f["movement_flags"] |= 8
    elif rng.random() < 0.6:
        f["movement_flags"] &= ~8
    f["move_period"] = rng.randrange(0, 120)
    f["rest_period"] = rng.randrange(0, 120)
    move, rest = f["move_period"], f["rest_period"]
    after = rng.choice((move, move, move - 1, move + 1, rest + move, rest + move, rest + move - 1,
                        rest + move + 1, 29, 30, 31, 0, rng.getrandbits(16)))
    f["timer"] = (after - 1) & 0xFFFF
    f["rate"] = rng.choice((0, 1, 2, 3, rng.randrange(40), f["rate"]))
    f["behavior_mode"] = 1 if rng.random() < 0.3 else rng.choice((0, 2, f["behavior_mode"]))
    f["phase"] = rng.choice((0, 1, 1, 2, 2, 3, f["phase"]))
    f["cooldown"] = rng.choice((0, 1, 60, f["cooldown"]))
    f["count"] = rng.choice((0, 0, 1, 2, 3, f["count"]))
    if kind == K_TILE:
        f["heading"] = rng.choice((-1, 0, rng.randrange(8), rng.randrange(8), rng.randrange(-512, 4096),
                                   f["heading"]))
        span = rng.choice((0, 1, 2, 3, 5, rng.randrange(70), rng.getrandbits(32)))
    else:
        # The original indexes the codes by the roaming heading >> 9 before masking it; it is
        # held to direction 7 or the byte before the codes, which mirrors it.
        f["heading"] = rng.choice((-1, rng.randrange(8), rng.randrange(-512, 4096)))
        span = (rng.choice((63, rng.randrange(63, 300), rng.randrange(63, 1 << 32))) if rng.random() < 0.5
                else rng.choice((0, 62, rng.randrange(63))))
    home = f["tile"] if rng.random() < 0.5 else packed_tile(rng, inner(rng), inner(rng))
    return {
        "kind": kind, "codes": codes, "room_data": rng.random() < 0.6, "width": width, "regions": regions,
        "flags": flags, "bounds": (min_x, min_z, max_x, max_z), "cache": rng.randrange(1, 256), "fields": f,
        "flies": rng.random() < 0.5, "object_flags": rng.getrandbits(32) & ~FLIES, "home": home, "range": span,
        "draws": [draw(rng) for _ in range(12)], "adjacent": [rng.getrandbits(1) for _ in range(4)],
        "traverse": [rng.getrandbits(1) for _ in range(4)], "checks": [rng.getrandbits(1) for _ in range(4)],
        "state": rng.randbytes(STATE_SIZE),
    }


def data_block(v: dict) -> bytes:
    block = bytearray(DATA_SIZE)
    for index, cell in enumerate(v["flags"]):
        block[index * 2:index * 2 + 2] = cell.to_bytes(2, "little")
    offset = REGIONS - DATA
    block[offset:offset + len(v["regions"])] = bytes(v["regions"])
    min_x, min_z, max_x, max_z = v["bounds"]
    block[BOUNDS - DATA:BOUNDS - DATA + 4] = bytes((max_x, max_z, min_x, min_z))
    object_flags = v["object_flags"] | (FLIES if v["flies"] else 0)
    block[OBJECT - DATA + 8:OBJECT - DATA + 12] = object_flags.to_bytes(4, "little")
    state = bytearray(v["state"])
    for name, offset, size, _ in FIELDS:
        state[offset:offset + size] = (v["fields"][name] & ((1 << size * 8) - 1)).to_bytes(size, "little")
    state[0x58:0x5C] = OBJECT.to_bytes(4, "little")
    block[STATE - DATA:STATE - DATA + STATE_SIZE] = state
    return bytes(block)


def field(machine: Machine, name: str) -> int:
    offset, size, signed = FIELD[name]
    return machine.load(STATE + offset, size, signed)


def run(machine: Machine, v: dict):
    machine.images.append(Image(data_block(v), DATA))
    machine.map_zero(SCRATCH, 0x400)
    machine.put(TILE_FLAGS, FLAGS)
    machine.put(TERRAIN_REGIONS, REGIONS)
    machine.put(REGION_WIDTH, v["width"], 1)
    machine.put(ROOM_DATA, 0x80600000 if v["room_data"] else 0)
    machine.put(DIRECTION_CACHE, v["cache"], 1)
    machine.put(ROOM_AI_STATE, BOUNDS)
    machine.store(SCRATCH_DIRECTIONS, bytes(v["codes"]))
    machine.put(SCRATCH_DIRECTIONS - 1, v["codes"][7], 1)
    machine.follow.update({CHOOSE_TILE, ARCTANGENT})
    records: list[list[int]] = []
    queues = {name: list(v[name]) for name in ("draws", "adjacent", "traverse", "checks")}

    def pop(name: str) -> int:
        return queues[name].pop(0) if queues[name] else 0

    def record(m: Machine, code: int, arguments: tuple[int, ...], actor: bool) -> None:
        snapshot = [field(m, name) for name in SNAPSHOT] if actor else [0] * len(SNAPSHOT)
        if actor and m.r[4] != STATE:
            code += NOT_THE_ACTOR
        records.append([code, *arguments, *([0] * (3 - len(arguments))), *snapshot])

    def random_masked(m: Machine) -> None:
        record(m, C_RANDOM, (s32(m.r[4]),), False)
        m.r[2] = pop("draws") & ((m.r[4] - 1) & MASK)

    def adjacent(m: Machine) -> None:
        record(m, C_ADJACENT, (m.r[4], s32(m.r[5])), False)
        m.r[2] = pop("adjacent")

    def traverse(m: Machine) -> None:
        record(m, C_TRAVERSE, (m.r[5], m.r[6], s32(m.r[7])), True)
        m.r[2] = pop("traverse")

    def direction_check(code: int):
        def check(m: Machine) -> None:
            record(m, code, (s32(m.r[5]),), True)
            m.r[2] = pop("checks")
        return check

    machine.stub(RANDOM, random_masked)
    machine.stub(ADJACENT, adjacent)
    machine.stub(TRAVERSE, traverse)
    machine.stub(FLY, direction_check(C_FLY))
    machine.stub(WALK, direction_check(C_WALK))
    machine.stub(PROJECT, lambda m: record(m, C_PROJECT, (m.r[5], s32(m.r[6]), m.r[7]), True))
    machine.stub(DISPATCH, lambda m: record(m, C_DISPATCH, (s32(m.r[5]), s32(m.r[6])), True))
    machine.stub(RECOVER, lambda m: record(m, C_RECOVER, (), True))
    machine.r[4:8] = [STATE, v["home"], v["range"], 3]
    machine.execute(CHOOSE_TILE if v["kind"] == K_TILE else UPDATE)
    observation = {
        "result": s32(machine.r[2]) if v["kind"] == K_TILE else 0,
        "fields": [field(machine, name) for name, *_ in FIELDS],
        "cache": machine.load(DIRECTION_CACHE, 1), "records": records, "stores": machine.stores,
    }
    min_x, min_z, max_x, max_z = v["bounds"]
    inputs = [v["kind"], *v["codes"], int(v["room_data"]), v["width"], *v["regions"], *v["flags"],
              min_x, min_z, max_x, max_z, v["cache"], *(v["fields"][name] for name, *_ in FIELDS),
              int(v["flies"]), v["home"], v["range"]]
    for name in ("draws", "adjacent", "traverse", "checks"):
        inputs += [len(v[name]), *v[name]]
    return observation, inputs


def allowed_stores() -> list[tuple[int, int]]:
    spans = [(STATE + offset, STATE + offset + size) for _, offset, size, _ in FIELDS]
    return [*spans, (DIRECTION_CACHE, DIRECTION_CACHE + 1), (0x80400000 - 0x4000, 0x80400000)]


ALLOWED = allowed_stores()
NAMES = [name for name, *_ in FIELDS]


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    problems = []
    result, fields = native[1], native[2:2 + len(FIELDS)]
    cache, count = native[2 + len(FIELDS):4 + len(FIELDS)]
    start = 4 + len(FIELDS)
    records = [native[start + i * RECORD:start + (i + 1) * RECORD] for i in range(count)]
    if result != obs["result"]:
        problems.append(f"tile choice: native {result} original {obs['result']}")
    differing = [f"{NAMES[i]} native {fields[i]} original {obs['fields'][i]}"
                 for i in range(len(FIELDS)) if fields[i] != obs["fields"][i]]
    if differing:
        problems.append("fields: " + ", ".join(differing))
    if cache != obs["cache"]:
        problems.append(f"direction cache: native {cache} original {obs['cache']}")
    if records != obs["records"]:
        first = next((i for i, (a, b) in enumerate(zip(records, obs["records"], strict=False))
                      if a != b),
                     min(len(records), len(obs["records"])))
        problems.append(f"calls differ from {first}: native {records[first:first + 2]} "
                        f"original {obs['records'][first:first + 2]}")
    for address, size in obs["stores"]:
        if not any(low <= address and address + size <= high for low, high in ALLOWED):
            problems.append(f"original stored {size} bytes at {address:#x}")
            break
    return problems


def describe(image: Image, results: list) -> list[str]:
    choices = {-1: 0, 0: 0, 1: 0}
    paths = {"held": 0, "rested": 0, "roamed": 0, "roam restarted": 0, "roam heading before codes": 0,
             "chain started": 0, "chain extended": 0, "goal walked": 0, "goal projected": 0}
    for v, obs, _native in results:
        if v["kind"] == K_TILE:
            choices[obs["result"]] = choices.get(obs["result"], 0) + 1
            continue
        codes = [record[0] for record in obs["records"]]
        dispatches = [record for record in obs["records"] if record[0] == C_DISPATCH]
        if not obs["records"] or codes == [C_RANDOM] and obs["fields"][NAMES.index("behavior_mode")] == 1:
            paths["held"] += 1
        elif dispatches and dispatches[0][1] == 1:
            paths["roamed"] += 1
            paths["roam restarted"] += any(code in (C_FLY, C_WALK) for code in codes)
            paths["roam heading before codes"] += dispatches[0][2] < 0
        elif dispatches:
            paths["goal walked"] += 1
            paths["goal projected"] += C_PROJECT in codes
        elif C_ADJACENT in codes or C_TRAVERSE in codes or codes.count(C_RANDOM) >= 2 and v["range"] < 63:
            paths["chain extended"] += 1
        elif obs["fields"][NAMES.index("count")] == 0 and obs["fields"][NAMES.index("heading")] == -1:
            paths["chain started"] += 1
        else:
            paths["rested"] += 1
    return [f"F01742 tile choices: blocked {choices[-1]}, extended {choices[0]}, complete {choices[1]}",
            "F01745 ticks: " + ", ".join(f"{name} {count}" for name, count in paths.items())]
