"""Shared harness for the bone trail differentials (BATTLE.PRG 5BF94.c:4898-5760 and 146C.c:1923-1925,
#276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects cores that
bone_trail_driver.cpp drives:

  draw         func_800CDCBC  DrawTrailSegment; RotAverage4 (func_80041840) is a stub recording the corners
                              and answering the vector's screens and depth; the framebuffer mapping and its
                              clamp are followed. The packet is compared word by word, less the bytes the
                              original leaves (each corner's spare code byte, the halfwords after the third
                              and fourth uvs and, in a distortion quad, the CLUT), with its ordering entry
                              and the primitive cursor
  update       func_800CE174  UpdateBoneTrail; the bone sampler, the heap, the view loads and the segment
                              draw are stubs recording their arguments; the Lagrange spline (func_800D1390,
                              InterpolateLagrangeVector) is followed
  spawn        func_800CF7A8  MakeBoneTrailRequest; the secondary effect start (func_800CF3F8) is a stub
                              recording the request and its parameter
  fade         func_800CF830  FadeBoneTrails over fixture instance and renderer chains; the trail renderer
                              is a stub recording each fade, which may unlink the slot it fades
  clamp, map   func_800CF92C, func_800CF988  ClampTrailTexturePoint and MapTrailFramebufferTexture
  weapon spawn func_8006C480  MakeWeaponTrailRequest, following the spawn
  weapon fade  func_8006C4A4  FadeWeaponTrails, following the fade

Native differences outside the compared domain (covered by BoneTrailTests.cpp): a tick or fade of a slot
without a trail and a start the heap has no room for are refused, where the original wrote through the
null block; and a colour number past both palettes (additive 15, distortion 7-15) draws nothing, where the
original read past its tables. A new trail is value-initialized where the original left the block as the
heap had it; the history is written before it is read, so start vectors begin from a zeroed block.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Machine,
    Records,
    mode_case,
    s32,
)

MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "bone_trail_driver.cpp"
SOURCES = (
    r"Native\Game\src\Effects\BoneTrail.cpp",
    r"Native\Game\src\Effects\Templates\RingCore.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Math\Interpolation.cpp",
)

DRAW, UPDATE, SPAWN, FADE, CLAMP, MAP, WEAPON_SPAWN, WEAPON_FADE = range(8)

# Globals: the primitive cursor and ordering table (scratchpad), the view matrix, the buffer being
# drawn, the two palettes and the secondary effect instances.
CURSOR_WORD, ORDERING_WORD, VIEW_MATRIX = 0x1F800000, 0x1F800004, 0x1F800014
FRAME_BUFFER = 0x8005E210
ADDITIVE_PALETTE, DISTORTION_PALETTE = 0x800EC2E8, 0x800EC308
PALETTE_ENTRIES = 15  # eight additive colours, then seven distortion colours
SECONDARY_INSTANCES = 0x800F54B0

# Callees.
ROT_AVERAGE4, SET_ROTATION, SET_TRANSLATION = 0x80041840, 0x80041598, 0x80041578
SAMPLE_BONE, ALLOCATE, FREE = 0x800A1AF8, 0x80043E3C, 0x80043C60
DRAW_SEGMENT, TRAIL_RENDERER, LAGRANGE = 0x800CDCBC, 0x800CE174, 0x800D1390
BEGIN_SECONDARY, SPAWN_TRAIL, FADE_TRAILS = 0x800CF3F8, 0x800CF7A8, 0x800CF830
CLAMP_POINT, MAP_TEXTURE = 0x800CF92C, 0x800CF988

# Fixtures.
TRAIL = 0x80500000
SLOT = 0x80510000
PACKET = 0x80520000
ORDERING = 0x80530000
UV = 0x80540000
INSTANCES = Records(0x80600000, 0x1000, 4)
SLOTS = Records(0x80700000, 0x10, 12)
OTHER_RENDERERS = (0x800CDCBC, 0x800D0000, 0x80041840)

# Trail state (effectTrailState).
CONFIG, NEWEST, AGE, BASES, TIPS = 0x00, 0x04, 0x08, 0x0C, 0x4C
PREVIOUS_BASE, CURRENT_BASE, PREVIOUS_TIP, CURRENT_TIP = 0x8C, 0x94, 0x9C, 0xA4
TRAIL_BYTES = 0xB4
HISTORY = 8
SLOT_STATE = 0x08
FIRST_SLOT = 0xD50
SLOT_RENDERER = 0x04

# Trail packets.
PACKET_WORDS = 15
PACKET_BYTES = PACKET_WORDS * 4
SCREEN_OFFSETS = (0x0C, 0x18, 0x24, 0x30)
UV_OFFSETS = (0x10, 0x1C, 0x28, 0x34)
TPAGE = 0x1E
ORDERING_ENTRIES = 0x800
NEAR_DEPTH, FAR_DEPTH = 17, 0x800
DISTORTION_TEXTURE = 6
DISTORTION_COLORS = 7
ADDITIVE_COLORS = PALETTE_ENTRIES  # additive numbers 8-14 read the distortion colours
# The bytes of each packet word the original writes; textured_mask(False) also drops a distortion
# quad's CLUT. bone_trail_driver.cpp applies the same masks.
CODE_BYTE_SPARE, HIGH_HALF_SPARE = 0x00FFFFFF, 0x0000FFFF

CALL_WORDS = 16
PROJECT_CALL, SAMPLE_CALL, LOAD_VIEW_CALL, DRAW_CALL, ALLOCATE_CALL, FREE_CALL, BEGIN_CALL, FADE_CALL = range(8)
START, TICK, FADE_COMMAND, STOP = 1, 2, 3, 4


def packet_mask(distortion: bool) -> list[int]:
    mask = [0xFFFFFFFF] * PACKET_WORDS
    for index in (5, 8, 11):
        mask[index] = CODE_BYTE_SPARE
    for index in (10, 13):
        mask[index] = HIGH_HALF_SPARE
    if distortion:
        mask[4] = HIGH_HALF_SPARE
    return mask


def call(kind: int, *words: int) -> list[int]:
    return [kind, *words, *([0] * (CALL_WORDS - 1 - len(words)))]


def word(rng: random.Random) -> int:
    return s32(rng.choice((0, 1, -1, 2, 0x7F, 0x80, 0xFF, 0x7FFF, 0x8000, 0xFFFF, 0x10000, 0x7FFFFFFF,
                           -0x80000000, rng.getrandbits(32))))


def coordinate(rng: random.Random) -> int:
    return rng.choice((rng.randint(-0x400, 0x400), rng.randint(-0x8000, 0x7FFF), 0, -1, 0x7FFF, -0x8000))


def point(rng: random.Random) -> list[int]:
    return [coordinate(rng) for _ in range(3)]


def config_word(rng: random.Random, *, distortion: bool | None = None) -> int:
    if distortion is None:
        distortion = rng.random() < 0.3
    texture = DISTORTION_TEXTURE if distortion else rng.choice([t for t in range(16) if t != DISTORTION_TEXTURE])
    color = rng.randrange(DISTORTION_COLORS if distortion else ADDITIVE_COLORS)
    life = rng.choice((0xFF, 0, 1, 2, 8, rng.getrandbits(8)))
    return rng.getrandbits(8) | rng.getrandbits(8) << 8 | color << 16 | texture << 20 | life << 24


def weight(rng: random.Random) -> int:
    return rng.choice((rng.randint(0, 12), rng.randint(-0x8000, 0x7FFF), word(rng)))


def screen(rng: random.Random) -> int:
    x = rng.choice((rng.randint(-0x80, 0x1C0), rng.randint(0x100, 0x180), rng.randint(-0x8000, 0x7FFF)))
    y = rng.choice((rng.randint(-0x80, 0x150), rng.randint(-0x8000, 0x7FFF)))
    return (x & 0xFFFF) | (y & 0xFFFF) << 16


def offset(rng: random.Random) -> int:
    return rng.choice((rng.randint(-0x140, 0x140), rng.randint(-4, 4), word(rng)))


def draw_vector(rng: random.Random, value: dict) -> None:
    value["config"] = config_word(rng)
    value["corners"] = [point(rng) for _ in range(4)]
    value["weights"] = [weight(rng), weight(rng)]
    value["screens"] = [screen(rng) for _ in range(4)]
    value["depth"] = rng.choice((NEAR_DEPTH - 1, NEAR_DEPTH, FAR_DEPTH - 1, FAR_DEPTH, 0, -1,
                                 *([rng.randint(NEAR_DEPTH, FAR_DEPTH - 1)] * 6), word(rng)))
    value["frame"] = rng.choice((0, 1, word(rng)))
    value["palette"] = [rng.getrandbits(8) for _ in range(PALETTE_ENTRIES * 4)]
    value["fill"] = [rng.getrandbits(32) for _ in range(PACKET_WORDS)]
    value["link"] = rng.getrandbits(32)


def update_vector(rng: random.Random, value: dict) -> None:
    command = rng.choice((TICK, TICK, TICK, TICK, TICK, START, FADE_COMMAND, STOP, 0, 5, word(rng) & 0xFFFFFFFF))
    value["command"] = command
    value["parameter"] = word(rng)
    # A tick or fade needs a trail (the original wrote through the null block).
    value["held"] = command in (TICK, FADE_COMMAND) or rng.random() < 0.5
    value["config"] = config_word(rng)
    value["newest"] = rng.choice((*range(HISTORY), word(rng) & 0xFFFFFFFF))
    value["age"] = rng.choice((*range(13), 3, 4, 8, 9, 0x7FFFFFFF, word(rng)))
    started = command == START
    value["history"] = [[0, 0, 0] if started else point(rng) for _ in range(2 * HISTORY)]
    value["edges"] = [[0, 0, 0] if started else point(rng) for _ in range(4)]
    value["samples"] = [[int(rng.random() < 0.1), *point(rng)] for _ in range(2)]


def request_vector(rng: random.Random, value: dict, arguments: int) -> None:
    value["arguments"] = [rng.choice((rng.getrandbits(8), rng.getrandbits(4), word(rng))) for _ in range(arguments)]


def fade_vector(rng: random.Random, value: dict) -> None:
    instances = rng.sample(range(INSTANCES.count), rng.randint(0, INSTANCES.count))
    slots = list(range(SLOTS.count))
    rng.shuffle(slots)
    value["arguments"] = [word(rng), word(rng)]
    value["first"] = instances[0] if instances else -1
    value["instance_next"] = [-1] * INSTANCES.count
    value["first_slot"] = [-1] * INSTANCES.count
    value["slot_next"] = [-1] * SLOTS.count
    for here, there in zip(instances, instances[1:], strict=False):
        value["instance_next"][here] = there
    for instance in instances:
        chain = [slots.pop() for _ in range(rng.randint(0, min(4, len(slots))))]
        value["first_slot"][instance] = chain[0] if chain else -1
        for here, there in zip(chain, chain[1:], strict=False):
            value["slot_next"][here] = there
    value["trail"] = [int(rng.random() < 0.6) for _ in range(SLOTS.count)]
    value["unlink"] = [int(rng.random() < 0.2) for _ in range(SLOTS.count)]
    value["renderers"] = [rng.choice(OTHER_RENDERERS) for _ in range(SLOTS.count)]


def map_vector(rng: random.Random, value: dict) -> None:
    value["screens"] = [screen(rng) for _ in range(4)]
    value["offsets"] = [offset(rng), offset(rng)]
    value["flags"] = rng.choice((0, rng.getrandbits(16), word(rng)))
    value["frame"] = rng.choice((0, 1, word(rng)))
    value["fill"] = [rng.getrandbits(32) for _ in range(PACKET_WORDS)]


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode, "number": number}
    if mode == DRAW:
        draw_vector(rng, value)
    elif mode == UPDATE:
        update_vector(rng, value)
    elif mode == SPAWN:
        request_vector(rng, value, 4)
    elif mode == WEAPON_SPAWN:
        request_vector(rng, value, 2)
    elif mode in (FADE, WEAPON_FADE):
        fade_vector(rng, value)
    elif mode == CLAMP:
        value["offsets"] = [offset(rng), offset(rng)]
        value["screen"] = screen(rng)
    else:
        map_vector(rng, value)
    return value


# ---------------------------------------------------------------- runs
def load_point(machine: Machine, address: int) -> list[int]:
    return [machine.load(address + 2 * axis, 2, signed=True) for axis in range(3)]


def put_point(machine: Machine, address: int, coordinates: list[int]) -> None:
    for axis, item in enumerate(coordinates):
        machine.put(address + 2 * axis, item, 2)


def put_palette(machine: Machine, palette: list[int]) -> None:
    machine.store(ADDITIVE_PALETTE, bytes(palette))


def driver_palette(palette: list[int]) -> list[int]:
    """The red, green and blue of each entry; the fourth byte is never read."""
    return [palette[4 * entry + channel] for entry in range(PALETTE_ENTRIES) for channel in range(3)]


def put_packet(machine: Machine, fill: list[int]) -> None:
    machine.map_zero(PACKET, PACKET_BYTES)
    for index, item in enumerate(fill):
        machine.put(PACKET + 4 * index, item)


def run_draw(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    def project(machine: Machine) -> None:
        sp = machine.r[29]
        pointed = [machine.r[4 + index] for index in range(4)]
        expected = [TRAIL + PREVIOUS_BASE, TRAIL + CURRENT_BASE, TRAIL + PREVIOUS_TIP, TRAIL + CURRENT_TIP]
        corners = [item for address in pointed for item in load_point(machine, address)]
        calls.append(call(PROJECT_CALL, int(pointed == expected), *corners))
        for index, item in enumerate(value["screens"]):
            machine.put(machine.load(sp + 0x10 + 4 * index), item)
        machine.put(machine.load(sp + 0x20), 0)
        machine.put(machine.load(sp + 0x24), 0)
        machine.r[2] = value["depth"] & 0xFFFFFFFF

    machine.map_zero(TRAIL, TRAIL_BYTES)
    machine.put(TRAIL + CONFIG, value["config"])
    for at, corner in zip((PREVIOUS_BASE, CURRENT_BASE, PREVIOUS_TIP, CURRENT_TIP), value["corners"], strict=True):
        put_point(machine, TRAIL + at, corner)
    put_packet(machine, value["fill"])
    machine.map_zero(ORDERING, ORDERING_ENTRIES * 4)
    entry_address = ORDERING + (value["depth"] & (ORDERING_ENTRIES - 1)) * 4
    machine.put(entry_address, value["link"])
    machine.put(CURSOR_WORD, PACKET)
    machine.put(ORDERING_WORD, ORDERING)
    machine.put(FRAME_BUFFER, value["frame"])
    put_palette(machine, value["palette"])
    machine.stub(ROT_AVERAGE4, project)
    machine.follow.update({MAP_TEXTURE, CLAMP_POINT})
    machine.r[4], machine.r[5], machine.r[6] = TRAIL, value["weights"][0] & 0xFFFFFFFF, value["weights"][1] & 0xFFFFFFFF
    machine.execute(entry)
    distortion = (value["config"] >> 20) & 0xF == DISTORTION_TEXTURE
    mask = packet_mask(distortion)
    values = [machine.load(PACKET + 4 * index) & mask[index] for index in range(PACKET_WORDS)]
    advance = s32(machine.load(CURSOR_WORD) - PACKET)
    values += [machine.load(entry_address), advance, value["depth"] if advance else -1]
    names = [f"packet word {index}" for index in range(PACKET_WORDS)] + ["ordering entry", "cursor advance",
                                                                          "ordering depth"]
    inputs = [value["config"], *(item for corner in value["corners"] for item in corner), *value["weights"],
              *value["screens"], value["depth"], value["frame"], *driver_palette(value["palette"]),
              *value["fill"], value["link"]]
    return values, names, inputs


def trail_values(machine: Machine) -> list[int]:
    values = [machine.load(TRAIL + CONFIG), machine.load(TRAIL + NEWEST), s32(machine.load(TRAIL + AGE))]
    for base in (BASES, TIPS):
        for entry in range(HISTORY):
            values += load_point(machine, TRAIL + base + 8 * entry)
    for at in (PREVIOUS_BASE, CURRENT_BASE, PREVIOUS_TIP, CURRENT_TIP):
        values += load_point(machine, TRAIL + at)
    return values


TRAIL_NAMES = ["config", "newest", "age",
               *(f"{side} {entry} {axis}" for side in ("base", "tip") for entry in range(HISTORY) for axis in "xyz"),
               *(f"{edge} {axis}" for edge in ("previous base", "current base", "previous tip", "current tip")
                 for axis in "xyz")]


def held(address: int) -> int:
    return {0: 0, TRAIL: 1}.get(address, 2)


def run_update(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    samples = iter(value["samples"])

    def sample(machine: Machine) -> None:
        destination = machine.r[6]
        calls.append(call(SAMPLE_CALL, machine.r[4], machine.r[5], machine.r[7], s32(destination - TRAIL)))
        missing, *coordinates = next(samples, [1, 0, 0, 0])
        if not missing:
            put_point(machine, destination, coordinates)

    def set_rotation(machine: Machine) -> None:
        calls.append(call(LOAD_VIEW_CALL, int(machine.r[4] == VIEW_MATRIX), 0))

    def set_translation(machine: Machine) -> None:
        last = calls[-1] if calls else None
        if last is not None and last[0] == LOAD_VIEW_CALL and last[2] == 0:
            last[2] = int(machine.r[4] == VIEW_MATRIX)
        else:
            calls.append(call(LOAD_VIEW_CALL, 0, 2))

    def draw(machine: Machine) -> None:
        edges = [item for at in (PREVIOUS_BASE, CURRENT_BASE, PREVIOUS_TIP, CURRENT_TIP)
                 for item in load_point(machine, TRAIL + at)]
        calls.append(call(DRAW_CALL, int(machine.r[4] == TRAIL), s32(machine.r[5]), s32(machine.r[6]), *edges))

    def allocate(machine: Machine) -> None:
        calls.append(call(ALLOCATE_CALL, machine.r[4]))
        machine.r[2] = TRAIL

    machine.map_zero(SLOT, 0x10)
    machine.put(SLOT + SLOT_STATE, TRAIL if value["held"] else 0)
    machine.map_zero(TRAIL, TRAIL_BYTES)
    machine.put(TRAIL + CONFIG, value["config"])
    machine.put(TRAIL + NEWEST, value["newest"])
    machine.put(TRAIL + AGE, value["age"])
    for index, coordinates in enumerate(value["history"]):
        base = BASES if index < HISTORY else TIPS
        put_point(machine, TRAIL + base + 8 * (index % HISTORY), coordinates)
    for at, coordinates in zip((PREVIOUS_BASE, CURRENT_BASE, PREVIOUS_TIP, CURRENT_TIP), value["edges"], strict=True):
        put_point(machine, TRAIL + at, coordinates)
    machine.stub(SAMPLE_BONE, sample)
    machine.stub(SET_ROTATION, set_rotation)
    machine.stub(SET_TRANSLATION, set_translation)
    machine.stub(DRAW_SEGMENT, draw)
    machine.stub(ALLOCATE, allocate)
    machine.stub(FREE, lambda m: calls.append(call(FREE_CALL, held(m.r[4]))))
    machine.follow.add(LAGRANGE)
    machine.r[4], machine.r[5], machine.r[6] = SLOT, value["command"], value["parameter"] & 0xFFFFFFFF
    machine.execute(entry)
    values = [s32(machine.r[2]), held(machine.load(SLOT + SLOT_STATE)), *trail_values(machine)]
    names = ["result", "held", *TRAIL_NAMES]
    inputs = [value["command"], value["parameter"], int(value["held"]), value["config"], value["newest"],
              value["age"], *(item for coordinates in value["history"] for item in coordinates),
              *(item for coordinates in value["edges"] for item in coordinates),
              *(item for sample in value["samples"] for item in sample)]
    return values, names, inputs


def begin_stub(calls: list[list[int]]):
    def begin(machine: Machine) -> None:
        request = machine.r[4]
        calls.append(call(
            BEGIN_CALL, machine.load(request, 2), machine.load(request + 2, 1), machine.load(request + 3, 1),
            machine.load(request + 4, 2), *load_point(machine, request + 8), machine.load(request + 0x10, 2),
            *load_point(machine, request + 0x14), s32(machine.r[5])))
    return begin


def run_spawn(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    machine.stub(BEGIN_SECONDARY, begin_stub(calls))
    machine.follow.add(SPAWN_TRAIL)
    for index, argument in enumerate(value["arguments"]):
        machine.r[4 + index] = argument & 0xFFFFFFFF
    machine.execute(entry)
    return [], [], list(value["arguments"])


def run_fade(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    def fade(machine: Machine) -> None:
        slot = SLOTS.number(machine.r[4])
        calls.append(call(FADE_CALL, slot, s32(machine.r[5]), s32(machine.r[6])))
        if 0 <= slot < SLOTS.count and value["unlink"][slot]:
            machine.put(SLOTS.at(slot), 0)

    for index in range(INSTANCES.count):
        machine.map_zero(INSTANCES.at(index), FIRST_SLOT + 4)
        machine.put(INSTANCES.at(index), INSTANCES.address(value["instance_next"][index]))
        machine.put(INSTANCES.at(index) + FIRST_SLOT, SLOTS.address(value["first_slot"][index]))
    machine.map_zero(SLOTS.base, SLOTS.count * SLOTS.stride)
    for index in range(SLOTS.count):
        renderer = TRAIL_RENDERER if value["trail"][index] else value["renderers"][index]
        machine.put(SLOTS.at(index), SLOTS.address(value["slot_next"][index]))
        machine.put(SLOTS.at(index) + SLOT_RENDERER, renderer)
    machine.put(SECONDARY_INSTANCES, INSTANCES.address(value["first"]))
    machine.stub(TRAIL_RENDERER, fade)
    machine.follow.add(FADE_TRAILS)
    machine.r[4], machine.r[5] = (argument & 0xFFFFFFFF for argument in value["arguments"])
    machine.execute(entry)
    inputs = [*value["arguments"], value["first"], *value["instance_next"], *value["first_slot"],
              *value["slot_next"], *value["trail"], *value["unlink"]]
    return [], [], inputs


def run_clamp(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    machine.map_zero(UV, 4)
    machine.put(UV, 0xA5A5A5A5)
    machine.r[4], machine.r[5] = (item & 0xFFFFFFFF for item in value["offsets"])
    machine.r[6], machine.r[7] = value["screen"], UV
    machine.execute(entry)
    return [machine.load(UV, 2)], ["uv"], [*value["offsets"], value["screen"]]


def run_map(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    put_packet(machine, value["fill"])
    for at, item in zip(SCREEN_OFFSETS, value["screens"], strict=True):
        machine.put(PACKET + at, item)
    machine.put(FRAME_BUFFER, value["frame"])
    machine.follow.add(CLAMP_POINT)
    machine.r[4] = PACKET
    machine.r[5], machine.r[6] = (item & 0xFFFFFFFF for item in value["offsets"])
    machine.r[7] = value["flags"] & 0xFFFFFFFF
    machine.execute(entry)
    values = [machine.load(PACKET + TPAGE, 2), *(machine.load(PACKET + at, 2) for at in UV_OFFSETS)]
    return values, ["tpage", "uv 0", "uv 1", "uv 2", "uv 3"], [*value["screens"], *value["offsets"],
                                                               value["flags"], value["frame"]]


RUNNERS = {
    DRAW: run_draw,
    UPDATE: run_update,
    SPAWN: run_spawn,
    WEAPON_SPAWN: run_spawn,
    FADE: run_fade,
    WEAPON_FADE: run_fade,
    CLAMP: run_clamp,
    MAP: run_map,
}


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    """The spans the original may store to: the stack and the mode's records."""
    spans = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    mode = value["mode"]
    if mode == DRAW:
        spans += [(PACKET, PACKET + PACKET_BYTES), (ORDERING, ORDERING + ORDERING_ENTRIES * 4),
                  (CURSOR_WORD, CURSOR_WORD + 4)]
    elif mode == UPDATE:
        spans += [(TRAIL, TRAIL + TRAIL_BYTES), (SLOT + SLOT_STATE, SLOT + SLOT_STATE + 4)]
    elif mode == CLAMP:
        spans.append((UV, UV + 2))
    elif mode == MAP:
        spans += [(PACKET + at, PACKET + at + 2) for at in (*UV_OFFSETS, TPAGE)]
    return spans


def describe(image, results) -> list[str]:
    vectors = [value for value, _, _ in results]
    modes = {value["mode"] for value in vectors}
    if modes == {DRAW}:
        drawn = [value for value, observation, _ in results if observation["values"][PACKET_WORDS + 1]]
        distorted = sum(1 for value in drawn if (value["config"] >> 20) & 0xF == DISTORTION_TEXTURE)
        return [f"{len(drawn)} of {len(vectors)} vectors drew a quad, {distorted} of them distortions"]
    if modes == {UPDATE}:
        draws = sum(1 for _, observation, _ in results for item in observation["calls"] if item[0] == DRAW_CALL)
        return [f"{draws} segments drawn"]
    if modes & {FADE, WEAPON_FADE}:
        fades = sum(len(observation["calls"]) for _, observation, _ in results)
        return [f"{fades} trails faded"]
    return []


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
