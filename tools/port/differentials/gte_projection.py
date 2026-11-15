"""SDK RotTransPers (libgte/SMP_02.s, SLUS_010.40) RTPS projection vs Core_Gte_RotTransPers (#371)."""

from __future__ import annotations

import random

from differential import (
    DQA,
    DQB,
    IR0,
    MAC0,
    OFX,
    OFY,
    Fault,
    H,
    Image,
    Machine,
    s16,
    s32,
)

ID = "RotTransPers"
ENTRY, END = 0x80041814, 0x80041840
MODULES = ("SLUS_010.40",)
DRIVER = "gte_projection_driver.cpp"
SOURCES = (r"Native\Engine\src\Core\Gte.cpp",)
VECTORS, SEED = 8192, 371

VECTOR = 0x80100000
SXY_OUT, CUE_OUT, FLAG_OUT = 0x80100010, 0x80100014, 0x80100018
IDENTITY = (0x1000, 0, 0, 0, 0x1000, 0, 0, 0, 0x1000)
FOG_OFFSET = 20971520
# Issue #371: identity transform, H=256, origin (160,120); (x, z) points, five witnessed as
# differing from ideal rounded division and five previously equal controls.
WITNESSES = ((64, 1024), (-47, 188), (-87, 232), (-77, 308), (-79, 316), (128, 1024), (132, 1024),
             (0, 1024), (85, 170), (-128, 1024))
# (H, Z) depth-cue fixtures with SetFogNear(8192, H): the wide DQA*quotient+DQB sum, not the low
# MAC0 word, decides IR0.
FOG = ((64, 1), (64, 32), (128, 1), (128, 32), (128, 64))
MUTANTS = {
    "xy-not-loaded": {0x80041814: 0x00000000},
    "vz-not-loaded": {0x80041818: 0x00000000},
    "screen-from-mac0": {0x80041824: 0xE8B80000},
    "cue-from-mac0": {0x80041828: 0xE8D80000},
    "flag-dropped": {0x8004182C: 0x00000000},
    "depth-from-mac0": {0x80041830: 0x4802C000},
    "depth-shift-1": {0x8004183C: 0x00021043},
}
NAMES = ("depth", "sxy", "cue", "flag", "mac0")


def fog_coefficient(near: int, h: int) -> int:
    return max(-0x8000, min(0x7FFF, -near * 320 // h))


def base(h: int = 256, ofx: int = 160, ofy: int = 120, dqa: int = 0, dqb: int = 0) -> dict:
    return {"rot": list(IDENTITY), "trans": [0, 0, 0], "ofx": ofx, "ofy": ofy, "h": h, "dqa": dqa,
            "dqb": dqb, "v": [0, 0, 0], "pad": 91}


def edge16(rng: random.Random) -> int:
    return rng.choice((-0x8000, -0x7FFF, -1, 0, 1, 0x7FFF)) if rng.random() < 0.2 else rng.randrange(-0x8000, 0x8000)


def vector(rng: random.Random, number: int) -> dict:
    if number < len(WITNESSES):
        value = base()
        value["v"] = [WITNESSES[number][0], 0, WITNESSES[number][1]]
        return value
    number -= len(WITNESSES)
    if number < len(FOG):
        h, z = FOG[number]
        value = base(h=h, dqa=fog_coefficient(8192, h), dqb=FOG_OFFSET)
        value["v"] = [0, 0, z]
        return value
    kind = rng.randrange(10)
    if kind < 2:  # Identity transform near the divide limit H < 2 * SZ3.
        h = rng.choice((0, 1, 2, 0xFFFF, 0x8000)) if rng.random() < 0.2 else rng.randrange(0x10000)
        z = max(-0x8000, min(0x7FFF, h // 2 + rng.randrange(-2, 3)))
        value = base(h=h, ofx=edge16(rng), ofy=edge16(rng), dqa=edge16(rng), dqb=s32(rng.getrandbits(32)))
        value["v"] = [edge16(rng), edge16(rng), z]
        return value
    if kind < 6:  # Identity transform, ordinary camera ranges, signed fractional coordinates.
        h, near = rng.choice((256, 320, 512, 128, 64)), rng.randrange(1, 8193)
        value = base(h=h, dqa=fog_coefficient(near, h),
                     dqb=FOG_OFFSET if rng.random() < 0.8 else s32(rng.getrandbits(32)))
        z = max(1, min(0x7FFF, int(near * rng.uniform(0.5, 6)))) if rng.random() < 0.7 else rng.randrange(1, 4096)
        value["v"] = [rng.randrange(-1024, 1025), rng.randrange(-1024, 1025), z]
        return value
    # Random rotation, translation and inputs, including signed44, word and halfword overflow.
    value = base(h=rng.getrandbits(16), ofx=edge16(rng), ofy=edge16(rng), dqa=edge16(rng),
                 dqb=s32(rng.getrandbits(32)))
    value["rot"] = [edge16(rng) for _ in range(9)]
    span = rng.choice((0x100, 0x10000, 0x1000000, 0x80000000))
    value["trans"] = [rng.randrange(-span, span) for _ in range(3)]
    if kind == 9:  # Translations at the word limits push the signed44 sums past either end.
        value["trans"] = [rng.choice((-0x80000000 + rng.randrange(16), 0x7FFFFFFF - rng.randrange(16)))
                          for _ in range(3)]
    value["v"] = [edge16(rng) for _ in range(3)]
    value["pad"] = edge16(rng)
    return value


def run(machine: Machine, value: dict) -> tuple:
    rot = value["rot"]
    for word in range(4):
        machine.gte_rotation[word] = (rot[2 * word] & 0xFFFF) | (rot[2 * word + 1] & 0xFFFF) << 16
    machine.gte_rotation[4] = s16(rot[8])
    machine.gte_translation[:] = [s32(t) for t in value["trans"]]
    machine.gte_projection[OFX] = s32(value["ofx"] << 16)
    machine.gte_projection[OFY] = s32(value["ofy"] << 16)
    machine.gte_projection[H] = value["h"] & 0xFFFF
    machine.gte_projection[DQA] = s16(value["dqa"])
    machine.gte_projection[DQB] = s32(value["dqb"])
    x, y, z = value["v"]
    machine.put(VECTOR, (x & 0xFFFF) | (y & 0xFFFF) << 16)
    machine.put(VECTOR + 4, (z & 0xFFFF) | (value["pad"] & 0xFFFF) << 16)
    for address in (SXY_OUT, CUE_OUT, FLAG_OUT):
        machine.put(address, 0xDEADBEEF)
    machine.r[4:8] = [VECTOR, SXY_OUT, CUE_OUT, FLAG_OUT]
    machine.execute(ENTRY)
    for address, size in machine.stores:
        if address not in (SXY_OUT, CUE_OUT, FLAG_OUT) or size != 4:
            raise Fault(f"store outside the outputs {address:#x}")
    observation = (machine.r[2], machine.load(SXY_OUT), machine.load(CUE_OUT), machine.load(FLAG_OUT),
                   machine.gte[MAC0], machine.gte[IR0])
    inputs = [*rot, *value["trans"], value["ofx"], value["ofy"], value["h"], value["dqa"], value["dqb"],
              x, y, z, value["pad"]]
    return observation, inputs


def check(image: Image, value: dict, observation: tuple, native: list[int], reference: tuple) -> list[str]:
    if len(native) != len(NAMES) + 1:
        return [f"driver printed {len(native)} values"]
    problems = [f"{name} native {got & 0xFFFFFFFF:#x} != original {want & 0xFFFFFFFF:#x}"
                for name, got, want in zip(NAMES, native, observation, strict=False)
                if (got ^ want) & 0xFFFFFFFF]
    if native[5] != value["pad"]:
        problems.append("native input vector changed")
    return problems


def ideal_quotient(h: int, z: int) -> int:
    return min(((h * 0x20000 // z) + 1) // 2, 0x1FFFF) if z > 0 and h < 2 * z else 0x1FFFF


def describe(image: Image, results: list) -> list[str]:
    counts = dict.fromkeys(("unr-differs", "divide-overflow", "sz-limit", "screen-limit", "mac0-overflow",
                            "cue-limit", "cue-wide-differs", "ir-limit", "mac-overflow"), 0)
    for value, observation, _ in results:
        flag = observation[3]
        counts["divide-overflow"] += bool(flag & 1 << 17)
        counts["sz-limit"] += bool(flag & 1 << 18)
        counts["screen-limit"] += bool(flag & 3 << 13)
        counts["mac0-overflow"] += bool(flag & 3 << 15)
        counts["cue-limit"] += bool(flag & 1 << 12)
        counts["ir-limit"] += bool(flag & 7 << 22)
        counts["mac-overflow"] += bool(flag & 0x3F << 25)
        if value["rot"] == list(IDENTITY) and value["trans"] == [0, 0, 0] and value["v"][2] > 0:
            h, z = value["h"] & 0xFFFF, value["v"][2]
            if h < 2 * z:
                counts["unr-differs"] += Machine.gte_divide(h, z)[0] != ideal_quotient(h, z)
        cue_low = s32(observation[4]) >> 12
        counts["cue-wide-differs"] += max(0, min(0x1000, cue_low)) != observation[5]
    return [
        ", ".join(f"{count} {name}" for name, count in counts.items()),
        f"{len(WITNESSES)} issue #371 vectors (identity, H=256, origin 160,120) and {len(FOG)} SetFogNear(8192, H) "
        "depth-cue fixtures lead the vectors; compared the returned SZ3/4, stored SXY2, IR0 and FLAG, MAC0 "
        "and the unchanged input vector.",
    ]
