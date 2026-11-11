"""F01721-F01728 (771A8.c:459-756) route gate links and area routing vs AreaRoute.cpp."""

from __future__ import annotations

import random

from differential import Image, Machine, s16, s32

ID = "F01721-F01728"
# func_800DF9A8, func_800DFA54, func_800DFAF8 and func_800DFBCC with its inline helpers.
ENTRY, END = 0x800DF9A8, 0x800E02B4
# The arctangent (0x8004137C) is in the executable.
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "area_route_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\AreaRoute.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1721
MUTANT_VECTORS = 2048

ROUTE_GATE_EXIT = 0x800DF9A8
NEAREST_ENTRY = 0x800DFA54
GATE_AT_TILE = 0x800DFAF8
AREA_ROUTE = 0x800DFBCC
# Host boundaries, stubbed and traced.
COLLECT_PLATFORMS = 0x8008D2C0
PLATFORM_DWELL = 0x8008E320
CONTACT_OCCUPANCY = 0x800E4BE0
DESTINATION_OCCUPANCY = 0x800D954C
ARCTANGENT = 0x8004137C

DIRECTION_CODES = 0x800F16EC
STEP_DIRECTIONS = 0x800F175C
ROOM_AI_STATE = 0x800F58BC
ROUTE_GATES = 0x800F5910
SCRATCH = 0x1F800000
ACTOR_TABLE = 0x1F80037C
HEIGHT_MAP = 0x1F8003BC
TILE_FLAGS = 0x1F8003C0
TERRAIN_REGIONS = 0x1F8003C4
REGION_WIDTH = 0x1F8003DC
SCRATCH_DIRECTIONS = 0x1F8003EC
ROOM_DATA = 0x1F8003F4

# One synthetic data block per vector.
DATA = 0x80500000
ROUTES = DATA
FLAGS = DATA + 0x400
HEIGHTS = DATA + 0xC00
REGIONS = DATA + 0x1400
LINKS = DATA + 0x1C00
ROOM_AI = DATA + 0x1C40
TILES = DATA + 0x1C80
POSITIONS = DATA + 0x1D00
ACTORS, ACTOR_STRIDE = DATA + 0x1E00, 0x80
STATE, STATE_SIZE = DATA + 0x2600, 0x1A0
DATA_SIZE = 0x2800
GRID = 32
SLOTS = 16
NO_TARGET = 16
UNREPORTED = -0x8000

# AreaRouteActor in declaration order: (state offset, size, signed).
FIELDS = [
    (0x05, 1, False), (0x34, 4, False), (0x4C, 2, True), (0x50, 2, True), (0x88, 1, False),
    (0x13A, 2, False), (0x13C, 2, False), (0x154, 1, False), (0x15C, 1, False), (0x168, 4, False),
    (0x170, 4, False), (0x17C, 4, True), (0x180, 4, True), (0x188, 2, True), (0x18A, 2, True),
    (0x19B, 1, False), (0x19C, 4, False),
]
REFUSED = -2
K_EXIT, K_ENTRY, K_GATE, K_ROUTE = range(4)
INTENTS = {4: "turn", 5: "move-direction", 6: "move-destination", 14: "step-to-tile", REFUSED: "refused"}

MUTANTS = {
    # func_800DF9A8
    "exit-partner-always-next": {0x800DF9EC: 0x24C60001},
    "exit-x-mask-0x0f": {0x800DFA08: 0x3042000F},
    "exit-platform-shift-2": {0x800DFA20: 0x00061083},
    "exit-seven-links": {0x800DFA3C: 0x28C20007},
    # func_800DFA54
    "entry-point-shift-18": {0x800DFA58: 0x00052C83},
    "entry-fallback-odd-link": {0x800DFAB0: 0x2549FFFF},
    "entry-z-mask-0x0f": {0x800DFAEC: 0x3042000F},
    # func_800DFAF8
    "gate-ends-swapped": {0x800DFB3C: 0x12200002},
    "gate-contact-shift-8": {0x800DFB80: 0x00021603},
    "gate-report-quarter": {0x800DFB40: 0x00061883},
    "gate-missing-zero": {0x800DFBB4: 0x24020000},
    # func_800DFBCC
    "defined-mask-with-gate": {0x800DFC18: 0x30421FE0},
    "second-defined-mask-with-gate": {0x800DFC20: 0x3C031FE0},
    "closed-endpoint-tried": {0x800DFC98: 0x00000000},
    "dwell-ignored": {0x800DFCBC: 0x00000000},
    "gate-tag-base-1": {0x800DFCC8: 0x26220001},
    "contact-occupancy-always": {0x800DFD10: 0x00000000},
    "long-step-filter-kept": {0x800DFD90: 0x00000000},
    "long-step-length-plus-2": {0x800DFDB8: 0x24840002},
    "turn-centre-0x20": {0x800DFE94: 0x24420020},
    "turn-angle-mask-0x7ff": {0x800DFECC: 0x304307FF},
    "turn-timer-kept": {0x800DFEE0: 0x00000000},
    "long-step-blocked-accepts": {0x800DFEE4: 0x00000000},
    "step-layer-bits-0x0c00": {0x800DFF98: 0x30420C00},
    "step-region-ignored": {0x800DFFE0: 0x00000000},
    "first-closed-gate-bit-only": {0x800E000C: 0x30830400},
    "raised-bits-0x1800": {0x800E00D4: 0x30421800},
    "distance-choice-inverted": {0x800E0220: 0x0062102A},
}

_battle: Image | None = None


def battle() -> Image:
    global _battle
    if _battle is None:
        _battle = Image.load("BATTLE/BATTLE.PRG")
    return _battle


def endpoint(rng: random.Random, x: int, z: int) -> int:
    value = z | x << 5
    value |= (rng.random() < 0.25) << 10  # gate
    value |= rng.randrange(4) << 11  # step kind
    if rng.random() < 0.4:
        value |= rng.randrange(1, 4) << 13  # long step
    value |= (rng.random() < 0.3) << 15  # closed
    return value


def gate_end(rng: random.Random, x: int, z: int) -> int:
    return x | z << 5 | rng.getrandbits(6) << 10


def tile_word(rng: random.Random, x: int, z: int) -> int:
    return x | rng.getrandbits(8) << 8 | z << 16 | rng.getrandbits(8) << 24


def coordinate(rng: random.Random) -> int:
    return rng.randrange(2, GRID - 2) if rng.random() < 0.8 else rng.randrange(GRID)


def gate_ends(rng: random.Random) -> list[int]:
    return [gate_end(rng, rng.randrange(GRID), rng.randrange(GRID)) for _ in range(16)]


def contacts(rng: random.Random, ends: list[int]) -> tuple[int, list[list[int]]]:
    """Up to four reported platform positions, some on a link's platform end."""
    count = rng.choice([0, 1, 2, 3, 4, 4, 4])
    reported = []
    for report in range(4):
        if report >= count:
            reported.append([UNREPORTED, 0, UNREPORTED, 0])
            continue
        if rng.random() < 0.6:
            end = ends[(report * 2 + rng.randrange(2)) * 2 + 1]
            x = (end & 31) << 7 | rng.randrange(128)
            z = (end >> 5 & 31) << 7 | rng.randrange(128)
        else:
            x, z = s16(rng.getrandbits(16)), s16(rng.getrandbits(16))
        reported.append([x, s16(rng.getrandbits(16)), z, rng.randrange(4)])
    return count, reported


def directions(rng: random.Random) -> tuple[list[int], list[int]]:
    image = battle()
    codes = ([image.read(DIRECTION_CODES + i, 1) for i in range(8)] if rng.random() < 0.6
             else [rng.getrandbits(8) for _ in range(8)])
    steps = ([image.read(STEP_DIRECTIONS + i, 1) for i in range(4)] if rng.random() < 0.6
             else [rng.randrange(8) for _ in range(4)])
    return codes, steps


def component(code: int) -> int:
    value = code & 3
    return value - 4 if value & 2 else value


def leaves_grid(v: dict) -> bool:
    """Whether a single step from an endpoint at the actor's tile reads a height outside the grid.

    The original then read past the map; the native core refuses. Vectors avoid it."""
    x, z = v["state_tile"] & 0xFF, v["state_tile"] >> 16 & 0xFF
    for value in v["route"]:
        if value & 0x400 or value & 0x6000 or (value >> 5 & 31, value & 31) != (x, z):
            continue
        code = v["codes"][v["steps"][value >> 11 & 3]]
        nx, nz = x + component(code), z + component(code >> 4)
        if not (0 <= nx < GRID and 0 <= nz < GRID):
            return True
    return False


def place_contact(rng: random.Random, ends: list[int], reported: list[list[int]], link: int) -> int:
    """Reports the link's platform at its platform end; returns the reported count."""
    end = ends[link * 2 + 1]
    reported[link >> 1] = [(end & 31) << 7 | rng.randrange(128), s16(rng.getrandbits(16)),
                           (end >> 5 & 31) << 7 | rng.randrange(128), rng.randrange(4)]
    for report in range(link >> 1):
        if reported[report][0] == UNREPORTED:
            reported[report] = [s16(rng.getrandbits(16)) for _ in range(4)]
    return max((link >> 1) + 1, rng.randrange((link >> 1) + 1, 5))


def route_vector(rng: random.Random) -> dict:
    ends = gate_ends(rng)
    codes, steps = directions(rng)
    route = [endpoint(rng, coordinate(rng), coordinate(rng)) for _ in range(2)]
    if rng.random() < 0.1:
        route[0] &= ~0x1BE0  # no route
    if rng.random() < 0.3:
        route[1] &= ~0x1BE0  # one endpoint
    roll = rng.random()
    if roll < 0.5:
        x, z = route[0] >> 5 & 31, route[0] & 31
    elif roll < 0.75:
        x, z = route[1] >> 5 & 31, route[1] & 31
    else:
        x, z = rng.randrange(GRID), rng.randrange(GRID)
    tile = tile_word(rng, x, z)
    count, reported = contacts(rng, ends)
    dwell = [rng.choice([-1, 0, 1, 0x40, 0x7FFF]) if rng.random() < 0.5 else -1 - rng.randrange(4)
             if rng.random() < 0.3 else rng.randrange(0x10000) for _ in range(4)]
    closed = rng.getrandbits(8) if rng.random() < 0.5 else 0
    if rng.random() < 0.6:  # a link from the actor's tile
        link = rng.randrange(8)
        ends[link * 2] = gate_end(rng, x, z)
        if rng.random() < 0.6:  # a platform gate endpoint there, its platform waiting
            chosen = 0 if roll < 0.5 or route[1] & 0x1BE0 == 0 else 1
            route[chosen] = (route[chosen] & ~0x83FF) | 0x400 | x << 5 | z
            route[chosen] |= (rng.random() < 0.15) << 15
            if rng.random() < 0.8:
                count = place_contact(rng, ends, reported, link)
            if rng.random() < 0.8:
                dwell[link >> 1] = rng.randrange(0x10000)
            if rng.random() < 0.7:
                closed &= ~(1 << ((link >> 1) + 2))
    width = rng.randrange(GRID, 41)
    heights = [rng.getrandbits(16) & (0xE3FF if rng.random() < 0.7 else 0xFFFF) for _ in range(GRID * GRID)]
    for value in route:
        cell = (value & 31) * GRID + (value >> 5 & 31)
        heights[cell] = rng.getrandbits(16) | 0x400 if rng.random() < 0.4 else heights[cell] & 0xE3FF
    state = bytearray(rng.randbytes(STATE_SIZE))
    target = NO_TARGET if rng.random() < 0.4 else rng.randrange(SLOTS)
    state[0x34:0x38] = tile.to_bytes(4, "little")
    state[0x88] = 0 if rng.random() < 0.3 else rng.randrange(1, 256)
    state[0x15C] = target
    if rng.random() < 0.3:  # the destination already at the tile
        state[0x168:0x16C] = (tile & 0xFF00FF | rng.getrandbits(8) << 8).to_bytes(4, "little")
    return {
        "kind": K_ROUTE, "ends": ends, "count": count, "contacts": reported,
        "area": rng.randrange(16), "index": rng.randrange(16),
        "routes": [rng.getrandbits(32) for _ in range(256)], "route": route,
        "flags": [rng.getrandbits(16) for _ in range(GRID * GRID)], "heights": heights,
        "width": width, "regions": list(rng.randbytes(width * GRID)), "codes": codes, "steps": steps,
        "dwell": dwell, "closed": closed,
        "actors": [rng.random() < 0.6 for _ in range(SLOTS)],
        "positions": [[s16(rng.getrandbits(16)) for _ in range(2)] for _ in range(SLOTS)],
        "occupancy": [rng.choice([-1, 0, 1, -0x10000, 0x7FFFFFFF, -3, 5]) for _ in range(2)],
        "state": bytes(state), "state_tile": tile,
    }


def vector(rng: random.Random, number: int) -> dict:
    roll = rng.random()
    if roll < 0.65:
        while True:
            v = route_vector(rng)
            if not leaves_grid(v):
                v["route_word"] = v["route"][0] | v["route"][1] << 16
                v["routes"][v["area"] * 16 + v["index"]] = v["route_word"]
                return v
    ends = gate_ends(rng)
    if roll < 0.75:
        link = rng.randrange(8)
        x, z = ((ends[link * 2] & 31, ends[link * 2] >> 5 & 31) if rng.random() < 0.7
                else (rng.getrandbits(8), rng.getrandbits(8)))
        return {"kind": K_EXIT, "ends": ends, "entry": tile_word(rng, x, z), "prev": rng.getrandbits(32)}
    if roll < 0.85:
        platform = rng.randrange(4)
        end = ends[(platform * 2 + rng.randrange(2)) * 2 + 1]
        if rng.random() < 0.7:
            world_x = (end & 31) << 19 | rng.getrandbits(19)
            world_z = (end >> 5 & 31) << 19 | rng.getrandbits(19)
        else:
            world_x, world_z = s32(rng.getrandbits(32)), s32(rng.getrandbits(32))
        return {"kind": K_ENTRY, "ends": ends, "platform": platform, "world": [world_x, world_z],
                "prev": rng.getrandbits(32)}
    count, reported = contacts(rng, ends)
    link = rng.randrange(8)
    if rng.random() < 0.4:
        count = place_contact(rng, ends, reported, link)
    end = ends[link * 2 + rng.randrange(2)]
    x, z = (end & 31, end >> 5 & 31) if rng.random() < 0.8 else (rng.getrandbits(8), rng.getrandbits(8))
    return {"kind": K_GATE, "ends": ends, "count": count, "contacts": reported,
            "tile": tile_word(rng, x, z), "match": rng.choice([0, 1, 1, rng.getrandbits(32)])}


def put_word(block: bytearray, address: int, value: int, size: int = 4) -> None:
    offset = address - DATA
    block[offset:offset + size] = (value & ((1 << size * 8) - 1)).to_bytes(size, "little")


def data_block(v: dict) -> bytearray:
    block = bytearray(DATA_SIZE)
    for index, value in enumerate(v["ends"]):
        put_word(block, LINKS + index * 2, value, 2)
    if v["kind"] in (K_EXIT, K_ENTRY):
        put_word(block, TILES, v.get("entry", 0))
        put_word(block, TILES + 4, v["prev"])
    if v["kind"] != K_ROUTE:
        return block
    for index, value in enumerate(v["routes"]):
        put_word(block, ROUTES + index * 4, value)
    for index in range(GRID * GRID):
        put_word(block, FLAGS + index * 2, v["flags"][index], 2)
        put_word(block, HEIGHTS + index * 2, v["heights"][index], 2)
    offset = REGIONS - DATA
    block[offset:offset + len(v["regions"])] = bytes(v["regions"])
    put_word(block, ROOM_AI + 0xE, v["closed"], 1)
    for slot, (x, z) in enumerate(v["positions"]):
        put_word(block, POSITIONS + slot * 8, x, 2)
        put_word(block, POSITIONS + slot * 8 + 4, z, 2)
        put_word(block, ACTORS + slot * ACTOR_STRIDE + 0x60, POSITIONS + slot * 8)
    offset = STATE - DATA
    block[offset:offset + STATE_SIZE] = v["state"]
    return block


def actor_fields(read) -> list[int]:
    return [read(STATE + offset, size, signed) for offset, size, signed in FIELDS]


def ends_input(v: dict) -> list[int]:
    return list(v["ends"])


def contacts_input(v: dict) -> list[int]:
    return [v["count"], *(value for contact in v["contacts"] for value in contact)]


def run(machine: Machine, v: dict):
    machine.images.append(Image(bytes(data_block(v)), DATA))
    machine.put(ROUTE_GATES, LINKS)
    machine.follow.update({ROUTE_GATE_EXIT, GATE_AT_TILE, ARCTANGENT})
    log = {"collect": 0, "dwell": [], "contact": [], "occupancy": []}

    def collect(m: Machine) -> None:
        log["collect"] += 1
        for report, contact in enumerate(v["contacts"]):
            for field, value in enumerate(contact):
                m.put(m.r[4] + report * 8 + field * 2, value, 2)
        m.r[2] = v["count"]

    def dwell(m: Machine) -> None:
        platform = s32(m.r[4])
        log["dwell"].append(platform)
        m.r[2] = v["dwell"][platform] & 0xFFFFFFFF if 0 <= platform < 4 else 0xFFFFFFFF

    def contact(m: Machine) -> None:
        log["contact"].append(s32(m.r[4]))

    def occupancy(m: Machine) -> None:
        state = m.r[4]
        log["occupancy"].append([m.r[5], m.load(state + 0x154, 1), m.load(state + 5, 1)])
        m.r[2] = v["occupancy"][min(len(log["occupancy"]) - 1, 1)] & 0xFFFFFFFF

    machine.stub(COLLECT_PLATFORMS, collect)
    if v["kind"] == K_EXIT:
        machine.r[4:6] = [TILES, TILES + 4]
        machine.execute(ROUTE_GATE_EXIT)
        return {"out": [machine.load(TILES + 4)], "stores": machine.stores}, [K_EXIT, *ends_input(v), v["entry"],
                                                                             v["prev"]]
    if v["kind"] == K_ENTRY:
        machine.r[4:8] = [v["platform"], v["world"][0] & 0xFFFFFFFF, v["world"][1] & 0xFFFFFFFF, TILES + 4]
        machine.execute(NEAREST_ENTRY)
        return ({"out": [machine.load(TILES + 4)], "stores": machine.stores},
                [K_ENTRY, *ends_input(v), v["platform"], *v["world"], v["prev"]])
    if v["kind"] == K_GATE:
        machine.r[4:7] = [STATE, v["tile"], v["match"]]
        machine.execute(GATE_AT_TILE)
        return ({"out": [s32(machine.r[2]), log["collect"]], "stores": machine.stores},
                [K_GATE, *ends_input(v), *contacts_input(v), v["tile"], v["match"]])

    machine.map_zero(SCRATCH, 0x400)
    for slot in range(SLOTS):
        machine.put(ACTOR_TABLE + slot * 4, ACTORS + slot * ACTOR_STRIDE if v["actors"][slot] else 0)
    machine.put(HEIGHT_MAP, HEIGHTS)
    machine.put(TILE_FLAGS, FLAGS)
    machine.put(TERRAIN_REGIONS, REGIONS)
    machine.put(REGION_WIDTH, v["width"], 1)
    machine.store(SCRATCH_DIRECTIONS, bytes(v["codes"]))
    machine.put(ROOM_DATA, ROUTES)
    machine.put(ROOM_AI_STATE, ROOM_AI)
    machine.store(STEP_DIRECTIONS, bytes(v["steps"]))
    machine.stub(PLATFORM_DWELL, dwell)
    machine.stub(CONTACT_OCCUPANCY, contact)
    machine.stub(DESTINATION_OCCUPANCY, occupancy)
    initial = actor_fields(lambda address, size, signed: int.from_bytes(
        v["state"][address - STATE:address - STATE + size], "little", signed=signed))
    machine.r[4:7] = [STATE, v["index"], v["area"]]
    machine.execute(AREA_ROUTE)
    out = [s32(machine.r[2]), *actor_fields(machine.load)]
    out += [len(log["occupancy"]), *(value for call in log["occupancy"] for value in call)]
    out += [len(log["contact"]), *log["contact"], len(log["dwell"]), *log["dwell"], log["collect"]]
    inputs = [K_ROUTE, *ends_input(v), *contacts_input(v), v["area"], v["index"], *v["routes"], *v["flags"],
              *v["heights"], v["width"], *v["regions"], *v["codes"], *v["steps"], *v["dwell"], v["closed"],
              *(int(present) for present in v["actors"]), *(value for pos in v["positions"] for value in pos),
              *v["occupancy"], *initial]
    return {"out": out, "stores": machine.stores}, inputs


def allowed_stores(v: dict) -> list[tuple[int, int]]:
    if v["kind"] in (K_EXIT, K_ENTRY):
        return [(TILES + 4, TILES + 8)]
    if v["kind"] == K_GATE:
        return []
    return [(STATE + offset, STATE + offset + size) for offset, size, _ in FIELDS]


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    problems = []
    if native != obs["out"]:
        problems.append(f"kind {v['kind']}: native {native[:24]} original {obs['out'][:24]}")
    ranges = allowed_stores(v)
    stack_low = 0x80400000 - 0x4000
    for address, size in obs["stores"]:
        if stack_low <= address < 0x80400000:
            continue
        if not any(start <= address and address + size <= end for start, end in ranges):
            problems.append(f"original stored {size} bytes at {address:#x} outside the routing fields")
            break
    return problems


def describe(image: Image, results: list) -> list[str]:
    kinds: dict[int, int] = {}
    intents: dict[str, int] = {}
    gates = boarded = steps = turns = found = exits = 0
    for v, obs, native in results:
        kinds[v["kind"]] = kinds.get(v["kind"], 0) + 1
        if v["kind"] == K_EXIT:
            exits += obs["out"][0] != v["entry"]
        elif v["kind"] == K_GATE:
            found += native[0] >= 0
        elif v["kind"] == K_ROUTE:
            name = INTENTS.get(native[0], str(native[0]))
            intents[name] = intents.get(name, 0) + 1
            occupancy_calls = native[18]
            contact_at = 19 + occupancy_calls * 3
            contacts_made = native[contact_at]
            dwell_calls = native[contact_at + 1 + contacts_made]
            gates += native[-1] > 0
            boarded += native[0] == 14 and dwell_calls > 0
            steps += occupancy_calls > 0
            turns += native[0] == 4
    lines = ["Vectors by kind: " + ", ".join(f"{name} {kinds.get(kind, 0)}" for kind, name in enumerate(
        ["F01721 exit", "F01722 entry", "F01723 gate lookup", "F01728 area route"]))]
    lines.append(f"F01721 exits resolved through a link: {exits}; F01723 links found: {found}")
    lines.append("F01728 commands: " + ", ".join(f"{name} {count}" for name, count in sorted(intents.items())))
    lines.append(f"F01728 gate lookups {gates}, platforms boarded {boarded}, occupancy queries {steps}, "
                 f"turns {turns}")
    return lines
