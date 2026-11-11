"""F01710 (74088.c:957) nearest active room waypoint vs Waypoints.cpp."""

from __future__ import annotations

import random

from differential import Image, Machine, s16

ID = "F01710"
ENTRY, END = 0x800DD7CC, 0x800DD918
DRIVER = "waypoint_search_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\Waypoints.cpp",
    r"Native\Game\src\Field\TerrainHeight.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1710

# Followed: the ground height at a world position (and the tile query it calls) and the GTE
# squared length; both read what the search swapped into the frame's height-map slot.
GROUND_HEIGHT = 0x800E42D4
GROUND_HEIGHT_FLAGS = 0x800E4320
SQUARED_LENGTH = 0x800E4660

ROOM_WAYPOINTS = 0x800F58C8
SPECIAL_HEIGHTS = 0x800F58D0
HEIGHT_MAP = 0x1F8003BC
SCRATCH = 0x1F800000

DATA = 0x80500000
WAYPOINTS = DATA
TILE = DATA + 0x20
STATE = DATA + 0x40
SPECIAL = DATA + 0x100
ROOM = DATA + 0x900
DATA_SIZE = 0x1100
GRID = 32

MUTANTS = {
    "special-heights-not-swapped": {0x800DD820: 0x00000000},
    "inactive-waypoints-searched": {0x800DD840: 0x00000000},
    "x-mask-0x0f": {0x800DD844: 0x3084000F},
    "z-mask-0x780": {0x800DD854: 0x30A50780},
    "centre-0x20": {0x800DD84C: 0x24840020},
    "height-ignored": {0x800DD880: 0x00A02821},
    # sltu v0, s4, v1 with bnez: an equally near later waypoint replaces the first.
    "ties-take-the-last": {0x800DD888: 0x0283102B, 0x800DD88C: 0x14400010},
    "closest-not-kept": {0x800DD8C0: 0x00000000},
    "seven-waypoints": {0x800DD8D4: 0x2A620007},
    "height-map-not-restored": {0x800DD8E8: 0x00000000},
}


def waypoint(rng: random.Random, x: int, z: int, active: bool) -> int:
    return active << 15 | rng.getrandbits(5) << 10 | z << 5 | x


def vector(rng: random.Random, number: int) -> dict:
    roll = rng.random()
    special = [rng.getrandbits(16) for _ in range(GRID * GRID)]
    room = [rng.getrandbits(16) for _ in range(GRID * GRID)]
    waypoints = [waypoint(rng, rng.randrange(GRID), rng.randrange(GRID), rng.random() < 0.6)
                 for _ in range(8)]
    x, z = rng.randrange(-64, GRID * 128 + 64), rng.randrange(-64, GRID * 128 + 64)
    y = rng.randrange(-0x800, 0x800)
    if roll < 0.1:  # nothing active
        waypoints = [value & 0x7FFF for value in waypoints]
    elif roll < 0.35:  # two active waypoints equally near: the first one wins
        tx, tz = rng.randrange(1, GRID - 1), rng.randrange(1, GRID - 1)
        step = rng.randrange(1, min(tx, GRID - 1 - tx) + 1)
        first, second = sorted(rng.sample(range(8), 2))
        a, b = (tx - step, tz), (tx + step, tz)
        if rng.random() < 0.5:
            a, b = b, a
        waypoints[first] = waypoint(rng, *a, True)
        waypoints[second] = waypoint(rng, *b, True)
        cell = special[a[1] * GRID + a[0]]
        special[b[1] * GRID + b[0]] = cell
        for index in range(8):  # nothing nearer between them
            if index not in (first, second):
                waypoints[index] &= 0x7FFF
        x, z = (tx << 7) + 64, (tz << 7) + 64
    elif roll < 0.5:  # standing on an active waypoint
        index = rng.randrange(8)
        waypoints[index] |= 0x8000
        x = ((waypoints[index] & 31) << 7) + rng.randrange(128)
        z = ((waypoints[index] >> 5 & 31) << 7) + rng.randrange(128)
    return {"waypoints": waypoints, "special": special, "room": room, "position": [x, y, z],
            "tile": rng.getrandbits(32)}


def put(block: bytearray, address: int, value: int, size: int) -> None:
    offset = address - DATA
    block[offset:offset + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")


def run(machine: Machine, v: dict):
    block = bytearray(DATA_SIZE)
    for index, value in enumerate(v["waypoints"]):
        put(block, WAYPOINTS + index * 2, value, 2)
    for index in range(GRID * GRID):
        put(block, SPECIAL + index * 2, v["special"][index], 2)
        put(block, ROOM + index * 2, v["room"][index], 2)
    for offset, value in zip((0x4C, 0x4E, 0x50), v["position"], strict=True):
        put(block, STATE + offset, value, 2)
    put(block, TILE, v["tile"], 4)
    machine.images.append(Image(bytes(block), DATA))
    machine.map_zero(SCRATCH, 0x400)
    machine.put(HEIGHT_MAP, ROOM)
    machine.put(ROOM_WAYPOINTS, WAYPOINTS)
    machine.put(SPECIAL_HEIGHTS, SPECIAL)
    machine.follow.update({GROUND_HEIGHT, GROUND_HEIGHT_FLAGS, SQUARED_LENGTH})
    machine.r[4:6] = [STATE, TILE]
    machine.execute(ENTRY)
    observation = {"tile": machine.load(TILE), "height_map": machine.load(HEIGHT_MAP),
                   "stores": machine.stores}
    inputs = [*v["waypoints"], *v["position"], *v["special"], v["tile"]]
    return observation, inputs


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    found, x, z, distance, tile = native
    problems = []
    if tile != obs["tile"]:
        problems.append(f"tile: native {tile:#010x} original {obs['tile']:#010x}")
    if obs["height_map"] != ROOM:
        problems.append(f"the original left the height-map slot at {obs['height_map']:#x}")
    if found:
        px, py, pz = (s16(value) for value in v["position"])
        cx, cz = (x << 7) + 64, (z << 7) + 64
        if not distance >= 0 or (px - cx) ** 2 + (pz - cz) ** 2 > distance:
            problems.append(f"squared distance {distance} is below the horizontal distance")
    stack_low = 0x80400000 - 0x4000
    for address, size in obs["stores"]:
        if stack_low <= address < 0x80400000 or address == HEIGHT_MAP and size == 4:
            continue
        if address not in (TILE, TILE + 2) or size != 1:
            problems.append(f"original stored {size} bytes at {address:#x}")
            break
    return problems


def describe(image: Image, results: list) -> list[str]:
    found = sum(native[0] for _, _, native in results)
    ties = sum(1 for v, _, native in results if native[0] and sum(value >> 15 for value in v["waypoints"]) == 2)
    return [f"waypoints found {found}, none active {len(results) - found}, two-candidate searches {ties}"]
