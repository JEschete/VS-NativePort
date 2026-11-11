"""F01747 (771A8.c:1863) waypoint patrol step vs Waypoints.cpp."""

from __future__ import annotations

import random

from differential import MASK, Fault, Image, Machine

ID = "F01747"
ENTRY, END = 0x800E23AC, 0x800E24EC
DRIVER = "waypoint_patrol_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\Waypoints.cpp",
    r"Native\Game\src\Field\TerrainHeight.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1747

ACTOR_RANDOM = 0x800E45D4
LOCOMOTION = 0x800E50A0
PATROL_ROUTES = 0x800F58D8

DATA = 0x80500000
ROUTES = DATA
STATE = DATA + 0x100
DATA_SIZE = 0x300
ROWS, PER_ROW = 4, 8
FIELDS = (0x100, 0x101, 0x102, 0x103, 0x104)
DESTINATION = (0x168, 0x16A)
# Every step of a cycle with no active waypoint runs ten instructions; the original never leaves.
STEP_LIMIT = 4000

MUTANTS = {
    "row-stride-8": {0x800E23D0: 0x000318C0},
    "arrival-checks-x-only": {0x800E23FC: 0x10000014},
    "cooldown-not-counted-down": {0x800E2414: 0x00000000},
    "range-is-the-rate": {0x800E2424: 0x00402021},
    "draw-ignored": {0x800E2428: 0x00000000},
    "cooldown-ignored": {0x800E2438: 0x00000000},
    "direction-kept": {0x800E2448: 0x00000000},
    "step-plus-one": {0x800E2458: 0x24430009},
    "index-mask-15": {0x800E2468: 0x3042000F},
    "inactive-waypoints-accepted": {0x800E2488: 0x00000000},
    "arrival-wait-9": {0x800E248C: 0x24020009},
    "destination-x-mask-15": {0x800E24B8: 0x3042000F},
    "destination-z-not-stored": {0x800E24D4: 0x00000000},
}


def waypoint(rng: random.Random, active: bool) -> int:
    return active << 15 | rng.getrandbits(5) << 10 | rng.getrandbits(10)


def vector(rng: random.Random, number: int) -> dict:
    routes = [waypoint(rng, rng.random() < 0.55) for _ in range(ROWS * PER_ROW)]
    row, index = rng.randrange(ROWS), rng.randrange(PER_ROW)
    direction = rng.choice((1, 0xFF)) if rng.random() < 0.7 else rng.getrandbits(8)
    cooldown = rng.randrange(4) if rng.random() < 0.7 else rng.getrandbits(8)
    raw = 0 if rng.random() < 0.4 else rng.getrandbits(32)
    current = routes[row * PER_ROW + index]
    roll = rng.random()
    if roll < 0.05:  # a row without an active waypoint: the original never returns
        for slot in range(PER_ROW):
            routes[row * PER_ROW + slot] &= 0x7FFF
    if roll < 0.45:  # standing on the current waypoint
        tile = [current & 31, current >> 5 & 31]
    elif roll < 0.55:  # only x matches
        tile = [current & 31, (current >> 5 & 31) ^ (1 + rng.randrange(31))]
    else:
        tile = [rng.randrange(32), rng.randrange(32)] if rng.random() < 0.8 else [
            rng.getrandbits(8), rng.getrandbits(8)]
    return {"routes": routes, "cursor": [index, row, rng.getrandbits(8), direction, cooldown],
            "tile": tile, "raw": raw}


def run(machine: Machine, v: dict):
    block = bytearray(DATA_SIZE)
    for slot, value in enumerate(v["routes"]):
        block[slot * 2:slot * 2 + 2] = value.to_bytes(2, "little")
    for offset, value in zip(FIELDS, v["cursor"], strict=True):
        block[STATE - DATA + offset] = value
    block[STATE - DATA + 0x34] = v["tile"][0]
    block[STATE - DATA + 0x36] = v["tile"][1]
    machine.images.append(Image(bytes(block), DATA))
    machine.put(PATROL_ROUTES, ROUTES)
    ranges, calls = [], []

    def draw(m: Machine) -> None:
        ranges.append(m.r[4] & MASK)
        m.r[2] = v["raw"] & (m.r[4] - 1) & MASK

    machine.stub(ACTOR_RANDOM, draw)
    machine.stub(LOCOMOTION, lambda m: calls.append(tuple(m.r[4:7])))
    machine.r[4] = STATE
    runaway = False
    try:
        machine.execute(ENTRY, limit=STEP_LIMIT)
    except Fault as error:
        if str(error) != "runaway original":
            raise
        runaway = True
    observation = {
        "runaway": runaway,
        "cursor": [machine.load(STATE + offset, 1) for offset in FIELDS],
        "destination": [machine.load(STATE + offset, 1) for offset in DESTINATION],
        "ranges": ranges, "calls": calls, "stores": machine.stores,
    }
    inputs = [*v["cursor"], *v["tile"], v["raw"], *v["routes"]]
    return observation, inputs


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    refused, cursor, destination, draws, last_range = native[0], native[1:6], native[6:8], native[8], native[9]
    problems = []
    if bool(refused) != obs["runaway"]:
        problems.append(f"native refused={refused}, original runaway={obs['runaway']}")
    if draws != len(obs["ranges"]) or (draws and last_range & MASK != obs["ranges"][-1]):
        problems.append(f"draws: native {draws} of {last_range & MASK:#x}, original {obs['ranges']}")
    if obs["runaway"]:
        if cursor != v["cursor"]:
            problems.append(f"refused native changed the cursor to {cursor}")
        return problems
    if cursor != obs["cursor"]:
        problems.append(f"cursor: native {cursor} original {obs['cursor']}")
    if destination != obs["destination"]:
        problems.append(f"destination: native {destination} original {obs['destination']}")
    if obs["calls"] != [(STATE, 0, 0)]:
        problems.append(f"original locomotion calls {obs['calls']}")
    stack_low = 0x80400000 - 0x4000
    allowed = {STATE + offset for offset in (0x100, 0x103, 0x104, *DESTINATION)}
    for address, size in obs["stores"]:
        if stack_low <= address < 0x80400000:
            continue
        if address not in allowed or size != 1:
            problems.append(f"original stored {size} bytes at {address:#x}")
            break
    return problems


def describe(image: Image, results: list) -> list[str]:
    runaway = sum(obs["runaway"] for _, obs, _ in results)
    stepped = sum(1 for v, obs, _ in results if not obs["runaway"] and obs["cursor"][0] != v["cursor"][0])
    reversed_ = sum(1 for v, obs, _ in results if not obs["runaway"] and obs["cursor"][3] != v["cursor"][3])
    waiting = sum(1 for v, obs, _ in results if not obs["runaway"] and obs["cursor"][4] == 10)
    return [f"stepped {stepped}, reversed {reversed_}, arrival waits {waiting}, "
            f"runaway rows refused {runaway} of {len(results)}"]
