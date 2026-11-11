"""F01740 (771A8.c:1540) area movement vs AreaRoute.cpp.

The world, data block and routing host calls are area_route's (F01721-F01728); this case follows
func_800DFBCC and the arctangent and stubs the movement primitives the function hands over to.
"""

from __future__ import annotations

import importlib.util
import random
from pathlib import Path

from differential import MASK, Image, Machine

_spec = importlib.util.spec_from_file_location("differential_area_route_world",
                                               Path(__file__).with_name("area_route.py"))
route = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(route)

ID = "F01740"
ENTRY, END = 0x800E19FC, 0x800E1BB8
MODULES = route.MODULES
DRIVER = "area_movement_driver.cpp"
SOURCES = route.SOURCES
VECTORS, SEED = 4096, 1740
MUTANT_VECTORS = 2048

HEIGHT_FILTERED = 0x800E1908
MOVEMENT_COOLDOWN = 0x800E4BB8
GROUND_MOVEMENT = 0x800E4B70
ISSUE_COMMAND = 0x800DEEFC
DIRECTIONAL = 0x800E3600
# The arguments each stubbed movement primitive takes.
ARITY = {HEIGHT_FILTERED: 1, MOVEMENT_COOLDOWN: 1, GROUND_MOVEMENT: 2, ISSUE_COMMAND: 2, DIRECTIONAL: 3}
DIRECTION_CACHE = 0x1F8003D0
STATE = route.STATE
# AreaMovementActor's own fields: is_stuck, near_target_detour, height_filtered_movement, area_lock.
EXTRA = (0x02, 0x04, 0x1A, 0x29)
GRID = route.GRID
H_FILTERED, H_GROUND, H_COMMAND, H_DIRECTIONAL = range(4)
ROUTE_FIELDS = len(route.FIELDS)

MUTANTS = {
    "detour-kept": {0x800E1A20: 0x00000000},
    "height-filter-ignored": {0x800E1A24: 0x10000005},
    "height-filter-always": {0x800E1A24: 0x00000000},
    "filter-not-set": {0x800E1A28: 0x00000000},
    "area-z-stride-32": {0x800E1A58: 0x00021140},
    "area-mask-0x7": {0x800E1A80: 0x30450007},
    "same-area-routed": {0x800E1A88: 0x00000000},
    "lock-ignored": {0x800E1A98: 0x00000000},
    "move-mode-7": {0x800E1AAC: 0x24020007},
    "move-centre-0x20": {0x800E1ACC: 0x24420020},
    "move-z-not-stored": {0x800E1AE4: 0x00000000},
    "recheck-ignored": {0x800E1B1C: 0x10000014},
    "recheck-stuck-not-marked": {0x800E1B28: 0x00000000},
    "cooldown-skipped": {0x800E1B2C: 0x00000000},
    "ground-angle-shift-8": {0x800E1B48: 0x00052A00},
    "command-intent-zero": {0x800E1B58: 0x00002821},
    "stuck-not-marked": {0x800E1B64: 0x00000000},
    "cache-kept": {0x800E1B6C: 0x00000000},
    "x-delta-reversed": {0x800E1B80: 0x00832023},
    "angle-mask-0x7ff": {0x800E1B90: 0x304507FF},
    "directional-third-argument-1": {0x800E1B98: 0x24060001},
}


def set_area(flags: list[int], x: int, z: int, area: int) -> None:
    cell = z * GRID + x
    flags[cell] = flags[cell] & ~15 | area


def vector(rng: random.Random, number: int) -> dict:
    while True:
        v = route.route_vector(rng)
        if not route.leaves_grid(v):
            break
    state = bytearray(v["state"])
    tile = v["state_tile"]
    tx, tz = tile & 0xFF, tile >> 16 & 0xFF
    roll = rng.random()
    if roll < 0.2:  # the destination is the actor's tile
        dx, dz = tx, tz
    else:
        dx, dz = rng.randrange(GRID), rng.randrange(GRID)
    state[0x168], state[0x16A] = dx, dz
    # Movement points stay within 2^22 of the actor: there the original's ratan2 wrapped and read
    # outside its table, where Arctangent2 reads the true ratio (ScalarMath.cpp).
    for offset in (0x17C, 0x180):
        point = rng.randrange(-0x40000, 0x40000) if rng.random() < 0.8 else rng.randrange(GRID) << 7 | 64
        state[offset:offset + 4] = (point & MASK).to_bytes(4, "little")
    current = rng.randrange(16)
    following = current if rng.random() < 0.25 else rng.randrange(16)
    if rng.random() < 0.5:  # route endpoints in the actor's area: moving there continues
        for value in v["route"]:
            set_area(v["flags"], value >> 5 & 31, value & 31, current)
    set_area(v["flags"], dx, dz, following)
    set_area(v["flags"], tx, tz, current)
    following = v["flags"][dz * GRID + dx] & 15
    v["index"], v["area"] = current, following
    v["routes"][following * 16 + current] = v["route"][0] | v["route"][1] << 16
    state[0x1A] = 0 if rng.random() < 0.9 else rng.randrange(1, 256)
    state[0x29] = 0 if rng.random() < 0.35 else rng.randrange(1, 256)
    v["state"] = bytes(state)
    v["cache"] = rng.randrange(1, 256)
    return v


def run(machine: Machine, v: dict):
    log = route.map_world(machine, v)
    route.map_route_world(machine, v, log)
    machine.follow.add(route.AREA_ROUTE)
    machine.put(DIRECTION_CACHE, v["cache"], 1)
    calls = []
    for address, arity in ARITY.items():
        machine.stub(address, lambda m, address=address, arity=arity: calls.append(
            (address, *(value & MASK for value in m.r[4:4 + arity]))))
    initial = route.initial_fields(v)
    extra = [v["state"][offset] for offset in EXTRA]
    machine.r[4] = STATE
    machine.execute(ENTRY)
    observation = {
        "fields": route.actor_fields(machine.load),
        "extra": [machine.load(STATE + offset, 1) for offset in EXTRA],
        "routing": route.routing_calls(log), "calls": calls,
        "cache": machine.load(DIRECTION_CACHE, 1), "stores": machine.stores,
    }
    inputs = [*route.ends_input(v), *route.contacts_input(v), *route.maps_input(v), *initial, *extra]
    return observation, inputs


def expected_calls(action: int, value: int) -> list[tuple]:
    """The calls UpdateLiveNavigationAreaMovement makes for a movement step."""
    if action == H_FILTERED:
        return [(HEIGHT_FILTERED, STATE)]
    if action == H_GROUND:
        return [(MOVEMENT_COOLDOWN, STATE), (GROUND_MOVEMENT, STATE, value & MASK)]
    if action == H_COMMAND:
        return [(MOVEMENT_COOLDOWN, STATE), (ISSUE_COMMAND, STATE, value & MASK)]
    return [(DIRECTIONAL, STATE, value & MASK, 0)]


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    refused, action, value, clear = native[:4]
    fields = native[4:4 + ROUTE_FIELDS]
    extra = native[4 + ROUTE_FIELDS:8 + ROUTE_FIELDS]
    routing = native[8 + ROUTE_FIELDS:]
    if refused:
        return ["native refused"]
    problems = []
    if obs["calls"] != expected_calls(action, value):
        problems.append(f"calls: native step {action} {value:#x} original {obs['calls']}")
    cache = 0 if action == H_DIRECTIONAL and clear else v["cache"]
    if obs["cache"] != cache:
        problems.append(f"direction cache: native {cache} original {obs['cache']}")
    if fields != obs["fields"]:
        problems.append(f"routing fields: native {fields} original {obs['fields']}")
    if extra != obs["extra"]:
        problems.append(f"movement fields: native {extra} original {obs['extra']}")
    if routing != obs["routing"]:
        problems.append(f"routing calls: native {routing} original {obs['routing']}")
    allowed = [(STATE + offset, STATE + offset + size) for offset, size, _ in route.FIELDS]
    allowed += [(STATE + 2, STATE + 3), (STATE + 4, STATE + 5), (DIRECTION_CACHE, DIRECTION_CACHE + 1)]
    stack_low = 0x80400000 - 0x4000
    for address, size in obs["stores"]:
        if stack_low <= address < 0x80400000:
            continue
        if not any(start <= address and address + size <= end for start, end in allowed):
            problems.append(f"original stored {size} bytes at {address:#x} outside the movement fields")
            break
    return problems


def describe(image: Image, results: list) -> list[str]:
    counts: dict[str, int] = {}
    for v, _obs, native in results:
        action, clear = native[1], native[3]
        state = v["state"]
        if action == H_FILTERED:
            name = "height-filtered"
        elif action == H_GROUND:
            name = "routed: ground movement"
        elif action == H_COMMAND:
            name = f"routed: command {native[2]}"
        elif v["index"] == v["area"]:
            name = "same area"
        elif state[0x29] == 0:
            name = "stuck without area lock"
        else:
            name = "routed to destination: " + ("stuck" if clear else "continues")
        counts[name] = counts.get(name, 0) + 1
    return ["Outcomes: " + ", ".join(f"{name} {count}" for name, count in sorted(counts.items()))]
