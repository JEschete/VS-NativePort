"""F01630 (6FA6C.s func_800D8260) temporary awareness marker vs Engagement.cpp.

func_800D8260 stores the marker's low halfword at AI state 0x138 and the frame count's low byte
at 0x12A. Vectors draw both arguments from the full 32-bit range, small counts and the values the
callers pass (7 frames of 0x28 from the target approach); the state around the two fields is
random, so a store of the wrong width or to the wrong offset shows in the compared fields or the
store check.
"""

from __future__ import annotations

import random

from differential import Image, Machine, field_problems, s32, store_problems

ID = "F01630"
ENTRY, END = 0x800D8260, 0x800D826C
MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "temporary_awareness_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\Ai\Engagement.cpp",)
VECTORS, SEED = 1024, 1630
MUTANT_VECTORS = 256

STATE, STATE_SIZE = 0x80500000, 0x200
# TemporaryAwareness in declaration order: name, offset, size, signed.
FIELDS = (("frames_remaining", 0x12A, 1, False), ("marker", 0x138, 2, False))
NAMES = [name for name, *_ in FIELDS]
# The neighbouring bytes a wider store would overwrite.
NEIGHBOURS = (("before_frames", 0x129, 1, False), ("after_frames", 0x12B, 1, False),
              ("before_marker", 0x136, 2, False), ("after_marker", 0x13A, 2, False))
COMPARED = FIELDS + NEIGHBOURS

MUTANTS = {
    "marker-byte": {0x800D8260: 0xA0850138},
    "frames-halfword": {0x800D8268: 0xA486012A},
    "marker-not-stored": {0x800D8260: 0x00000000},
    "frames-not-stored": {0x800D8268: 0x00000000},
    "marker-at-0x13A": {0x800D8260: 0xA485013A},
    "frames-from-marker": {0x800D8268: 0xA085012A},
}


def argument(rng: random.Random) -> int:
    roll = rng.random()
    if roll < 0.4:
        return rng.choice((0, 1, 7, 0x28, 0x7F, 0x80, 0xFF, 0x100, 0x7FFF, 0x8000, 0xFFFF, 0x10000, -1))
    if roll < 0.7:
        return rng.randrange(0x200)
    return s32(rng.getrandbits(32))


def vector(rng: random.Random, number: int) -> dict:
    return {"marker": argument(rng), "frames": argument(rng), "state": rng.randbytes(STATE_SIZE)}


def run(machine: Machine, v: dict):
    machine.images.append(Image(bytes(v["state"]), STATE))
    machine.r[4] = STATE
    machine.r[5] = v["marker"] & 0xFFFFFFFF
    machine.r[6] = v["frames"] & 0xFFFFFFFF
    machine.execute(ENTRY)
    observation = {"fields": machine.load_fields(STATE, COMPARED, [name for name, *_ in COMPARED]),
                   "stores": machine.stores}
    initial = [int.from_bytes(v["state"][offset:offset + size], "little") for _, offset, size, _ in FIELDS]
    return observation, [*initial, v["marker"], v["frames"]]


def check(image: Image, v: dict, obs: dict, native: list[int], reference: dict) -> list[str]:
    neighbours = [int.from_bytes(v["state"][offset:offset + size], "little") for _, offset, size, _ in NEIGHBOURS]
    problems = field_problems([name for name, *_ in COMPARED], [*native, *neighbours], obs["fields"])
    allowed = [(STATE + offset, STATE + offset + size) for _, offset, size, _ in FIELDS]
    return problems + store_problems(obs["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    wide = sum(1 for v, _obs, _native in results if not 0 <= v["marker"] <= 0xFFFF or not 0 <= v["frames"] <= 0xFF)
    return [f"Vectors with a marker or count wider than its field: {wide}"]
