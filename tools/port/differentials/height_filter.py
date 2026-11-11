"""F01738-F01739 (771A8.c:1455-1540) height-filtered movement vs AreaRoute.cpp.

func_800E1850 runs with the original ground height (func_800E42EC) over a mapped height map;
func_800E1908 follows it and the arctangent and stubs the directional move it hands over to,
which reports a movement result and a new heading.
"""

from __future__ import annotations

import random

from differential import Image, Machine, s16, s32

ID = "F01738-F01739"
ENTRY, END = 0x800E1850, 0x800E19FC
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "height_filter_driver.cpp"
SOURCES = (
    r"Native\Game\src\Battle\Ai\AreaRoute.cpp",
    r"Native\Game\src\Field\TerrainHeight.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, SEED = 4096, 1738
MUTANT_VECTORS = 2048

FILTER = 0x800E1850
MOVE = 0x800E1908
GROUND_HEIGHT = 0x800E42EC
GROUND_HEIGHT_FLAGS = 0x800E4320
DIRECTIONAL = 0x800E3600
ARCTANGENT = 0x8004137C

DIRECTION_CODES = 0x800F16EC
SCRATCH = 0x1F800000
HEIGHT_MAP = 0x1F8003BC
TERRAIN_REGIONS = 0x1F8003C4
MOVE_RESULT = 0x1F8003C8
REGION_WIDTH = 0x1F8003DC
SCRATCH_DIRECTIONS = 0x1F8003EC

DATA = 0x80500000
HEIGHTS = DATA
REGIONS = DATA + 0x800
ACTOR = DATA + 0xE00
STATE, STATE_SIZE = DATA + 0x1000, 0x190
DATA_SIZE = 0x1200
GRID = 32
K_FILTER, K_MOVE = range(2)
# Command intents: the move results that walk (bits 5 and 7 of 0xA0) and others.
RESULTS = (5, 7, 37, 39, 0, 4, 6, 14)

MUTANTS = {
    # func_800E1850
    "z-from-byte-1": {0x800E186C: 0x00052A02},
    "dz-from-bits-2-3": {0x800E18AC: 0x00052F00},
    "height-compare-reversed": {0x800E18C0: 0x0054102A},
    "clears-bit-0-only": {0x800E18CC: 0x00000000},
    "keeps-only-the-bit": {0x800E18D0: 0x00000000},
    "seven-directions": {0x800E18DC: 0x2A020007},
    # func_800E1908
    "region-row-from-x": {0x800E1924: 0x92430034},
    "filter-height-from-x": {0x800E1944: 0x8646004C},
    "filter-not-stored": {0x800E1968: 0x00000000},
    "angle-mask-0x7ff": {0x800E198C: 0x304507FF},
    "cell-not-restored": {0x800E1998: 0x00000000},
    "walk-results-0x20": {0x800E19A4: 0x24020020},
    "walk-mode-8": {0x800E19B4: 0x24020008},
    "heading-unscaled": {0x800E19D0: 0x00000000},
    "walk-heading-not-stored": {0x800E19DC: 0x00000000},
}

_battle: Image | None = None


def battle() -> Image:
    global _battle
    if _battle is None:
        _battle = Image.load("BATTLE/BATTLE.PRG")
    return _battle


def component(code: int) -> int:
    value = code & 3
    return value - 4 if value & 2 else value


def ground_height(cell: int) -> int:
    """func_800E42EC: the signed 10-bit height times 64 less 128 per slope layer, as 15 bits."""
    height = s16((cell << 6) & 0xFFFF) - ((cell & 0x1C00) >> 3)
    return s32((height << 17) & 0xFFFFFFFF) >> 17


def neighbours_in_grid(mask: int, x: int, z: int, codes: list[int]) -> bool:
    return all(0 <= x + component(codes[i]) < GRID and 0 <= z + component(codes[i] >> 4) < GRID
               for i in range(8) if mask >> i & 1)


def vector(rng: random.Random, number: int) -> dict:
    image = battle()
    codes = ([image.read(DIRECTION_CODES + i, 1) for i in range(8)] if rng.random() < 0.7
             else [rng.getrandbits(8) for _ in range(8)])
    heights = [rng.getrandbits(16) if rng.random() < 0.5 else rng.getrandbits(16) & 0x1FFF
               for _ in range(GRID * GRID)]
    width = rng.randrange(GRID, 41)
    regions = list(rng.randbytes(width * GRID))
    while True:
        x, z = rng.randrange(GRID), rng.randrange(GRID)
        mask = rng.getrandbits(8)
        if neighbours_in_grid(mask, x, z, codes):
            break
    tile = rng.getrandbits(8) << 24 | z << 16 | rng.getrandbits(8) << 8 | x
    set_bits = [i for i in range(8) if mask >> i & 1]
    if set_bits and rng.random() < 0.8:  # near a neighbour's height
        i = rng.choice(set_bits)
        cell = heights[(z + component(codes[i] >> 4)) * GRID + x + component(codes[i])]
        limit = ground_height(cell) + rng.choice([-1, 0, 1, -64, 64])
    else:
        limit = s16(rng.getrandbits(16))
    v = {"codes": codes, "heights": heights, "width": width, "regions": regions, "tile": tile,
         "mask": mask, "limit": limit}
    if rng.random() < 0.4:
        v["kind"] = K_FILTER
        # The caller loads the mask as a byte; the original ignores the bits above it.
        v["mask_word"] = mask | (rng.getrandbits(24) << 8 if rng.random() < 0.3 else 0)
        if rng.random() < 0.1:
            v["limit"] = s32(rng.getrandbits(32))
        return v
    v["kind"] = K_MOVE
    regions[z * width + x] = mask
    state = bytearray(rng.randbytes(STATE_SIZE))
    state[0x34:0x38] = tile.to_bytes(4, "little")
    state[0x4E:0x50] = (limit & 0xFFFF).to_bytes(2, "little")
    v["limit"] = s16(limit & 0xFFFF)
    state[0x54:0x58] = ACTOR.to_bytes(4, "little")
    for offset in (0x17C, 0x180):
        point = rng.randrange(-0x40000, 0x40000) if rng.random() < 0.8 else rng.randrange(GRID) << 7 | 64
        state[offset:offset + 4] = (point & 0xFFFFFFFF).to_bytes(4, "little")
    v["state"] = bytes(state)
    v["actor"] = bytes(rng.randbytes(16))
    v["result"] = rng.choice(RESULTS) if rng.random() < 0.9 else rng.getrandbits(32)
    v["heading"] = rng.getrandbits(16)
    return v


def data_block(v: dict) -> bytes:
    block = bytearray(DATA_SIZE)
    for index, cell in enumerate(v["heights"]):
        block[index * 2:index * 2 + 2] = cell.to_bytes(2, "little")
    offset = REGIONS - DATA
    block[offset:offset + len(v["regions"])] = bytes(v["regions"])
    if v["kind"] == K_MOVE:
        block[ACTOR - DATA:ACTOR - DATA + 16] = v["actor"]
        block[STATE - DATA:STATE - DATA + STATE_SIZE] = v["state"]
    return bytes(block)


def world_input(v: dict) -> list[int]:
    return [*v["codes"], *v["heights"], v["width"], *v["regions"]]


def run(machine: Machine, v: dict):
    machine.images.append(Image(data_block(v), DATA))
    machine.map_zero(SCRATCH, 0x400)
    machine.put(HEIGHT_MAP, HEIGHTS)
    machine.put(TERRAIN_REGIONS, REGIONS)
    machine.put(REGION_WIDTH, v["width"], 1)
    machine.store(SCRATCH_DIRECTIONS, bytes(v["codes"]))
    machine.follow.update({GROUND_HEIGHT, GROUND_HEIGHT_FLAGS, FILTER, ARCTANGENT})
    if v["kind"] == K_FILTER:
        machine.r[4:7] = [v["mask_word"], v["tile"], v["limit"] & 0xFFFFFFFF]
        machine.execute(FILTER)
        return ({"out": [machine.r[2]], "stores": machine.stores},
                [K_FILTER, *world_input(v), v["mask"], v["tile"], v["limit"]])
    x, z = v["tile"] & 0xFF, v["tile"] >> 16 & 0xFF
    cell_address = REGIONS + z * v["width"] + x
    calls = []

    def directional(m: Machine) -> None:
        calls.append([s32(m.r[5]), s32(m.r[6]), m.load(cell_address, 1), int(m.r[4] == STATE)])
        m.put(MOVE_RESULT, v["result"])
        m.put(STATE + 0x188, v["heading"], 2)

    machine.stub(DIRECTIONAL, directional)
    machine.r[4] = STATE
    machine.execute(MOVE)
    state = v["state"]
    inputs = [K_MOVE, *world_input(v), v["tile"], *(s16(int.from_bytes(state[o:o + 2], "little")) for o in
                                                   (0x4C, 0x4E, 0x50)),
              *(s32(int.from_bytes(state[o:o + 4], "little")) for o in (0x17C, 0x180)),
              s32(v["result"]), v["heading"]]
    observation = {
        "calls": calls, "cell": machine.load(cell_address, 1),
        "mode": machine.load(ACTOR + 9, 1), "angle": machine.load(ACTOR + 0xC, 2), "stores": machine.stores,
    }
    return observation, inputs


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    if native[0]:
        return ["native refused"]
    stack_low = 0x80400000 - 0x4000
    allowed: list[tuple[int, int]] = []
    problems = []
    if v["kind"] == K_FILTER:
        if obs["out"] != native[1:2]:
            problems.append(f"filtered mask: native {native[1]:#x} original {obs['out'][0]:#x}")
    else:
        walks, heading = native[1:3]
        count = native[3]
        calls = [[native[4 + i * 2], 0, native[5 + i * 2], 1] for i in range(count)]
        cell = native[4 + count * 2]
        if calls != obs["calls"]:
            problems.append(f"moves: native {calls} original {obs['calls']}")
        if cell != obs["cell"]:
            problems.append(f"restored cell: native {cell:#x} original {obs['cell']:#x}")
        mode, angle = (9, heading) if walks else (v["actor"][9], int.from_bytes(v["actor"][12:14], "little"))
        if (mode, angle) != (obs["mode"], obs["angle"]):
            problems.append(f"walk: native {mode} {angle:#x} original {obs['mode']} {obs['angle']:#x}")
        x, z = v["tile"] & 0xFF, v["tile"] >> 16 & 0xFF
        cell_address = REGIONS + z * v["width"] + x
        allowed = [(cell_address, cell_address + 1), (ACTOR + 9, ACTOR + 10), (ACTOR + 0xC, ACTOR + 0xE)]
    for address, size in obs["stores"]:
        if stack_low <= address < 0x80400000:
            continue
        if not any(start <= address and address + size <= end for start, end in allowed):
            problems.append(f"original stored {size} bytes at {address:#x}")
            break
    return problems


def describe(image: Image, results: list) -> list[str]:
    kinds = [0, 0]
    cleared = kept = walks = scaled = 0
    for v, _obs, native in results:
        kinds[v["kind"]] += 1
        if v["kind"] == K_FILTER:
            cleared += native[1] != v["mask"]
            kept += native[1] == v["mask"] and v["mask"] != 0
        else:
            walks += native[1]
            scaled += native[1] and v["result"] == 5
    return [f"Vectors: F01738 filter {kinds[K_FILTER]}, F01739 move {kinds[K_MOVE]}",
            f"F01738 masks with a direction cleared {cleared}, kept whole {kept}",
            f"F01739 walks {walks} (heading scaled {scaled})"]
