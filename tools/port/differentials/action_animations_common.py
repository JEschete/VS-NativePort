"""Shared R3000 cases for the owned action-animation and reaction policies."""

from __future__ import annotations

import random
from pathlib import Path

from differential import (
    DEFAULT_STACK,
    MASK,
    ROOT,
    Fault,
    Machine,
    pack_fields,
    s16,
    s32,
)

STAGE = Path(__file__).resolve().parents[3]
DRIVER = "action_animations_driver.cpp"
INCLUDES = (
    str(STAGE / r"Native\Game\include"),
    str(STAGE / r"Native\Tests\include"),
)
SOURCES = (
    str(STAGE / r"Native\Game\src\Battle\ActionAnimations.cpp"),
    *(str(ROOT / path) for path in (
        r"Native\Game\src\Actors\ActorMotion.cpp",
        r"Native\Game\src\Actors\Skeleton.cpp",
        r"Native\Game\src\Actors\AnimationPose.cpp",
        r"Native\Game\src\Actors\ModelRendering.cpp",
        r"Native\Game\src\Actors\ModelDrawing.cpp",
        r"Native\Game\src\Actors\SequencePlayback.cpp",
        r"Native\Engine\src\Core\Math\Matrix.cpp",
        r"Native\Engine\src\Core\Math\Vector.cpp",
        r"Native\Engine\src\Core\Math\ScalarMath.cpp",
        r"Native\Engine\src\Core\Math\Distance.cpp",
    )),
)
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
TABLE, SIDE, SPECIAL_PITCH = 0x800F4538, 0x800F49D8, 0x800F49E4
GROUPS, SHARED_SLOT, WORK_BANK, CLIP_INDEX = (
    0x800E8E80, 0x800E8F2C, 0x800F4768, 0x800F49DC
)
ACTORS = (0x80500000, 0x80502000, 0x80504000)
BANKS = (0x80600000, 0x80602000, 0x80604000)
TARGET, RECORD_BYTES = 0x80700000, 0x800
REACTIONS = (819, 832, 891)
POSE_BYTES, BLEND_POSE, CURRENT_POSE = 0x550, 0x704, 0xC54
TURN, IMPACT, COPY = 0x800A9EB4, 0x8006C84C, 0x800490B0
FACING, PITCH, HEADER = 0x8009E180, 0x8009E228, 0x800AD494
PLAY, RESET, BONE, QUEUE = 0x800A0204, 0x8009FD38, 0x800A1AF8, 0x800A01C8
ATAN, ROOT_HELPER, SPECIAL = 0x8004137C, 0x80041B94, 0x8009EFEC
SPANS = {
    817: (0x8009DD00, 0x8009DDF4),
    818: (0x8009DDF4, 0x8009DF3C),
    819: (0x8009DF3C, 0x8009E070),
    823: (0x8009E2E0, 0x8009E480),
    831: (0x8009EC9C, 0x8009EE9C),
    832: (0x8009EE9C, 0x8009EFEC),
    833: (0x8009EFEC, 0x8009F298),
    834: (0x8009F298, 0x8009F314),
    891: (0x800A2574, 0x800A25EC),
}
# Native state order. The final map_present field describes the loaded bank.
FIELDS = (
    ("flags", 8, 4), ("slot", 0xF, 1), ("jump", 0x11, 1), ("turn", 0x18, 2),
    ("x", 0x1C, 2), ("y", 0x1E, 2), ("z", 0x20, 2), ("w", 0x22, 2),
    ("facing", 0x26, 2), ("delta", 0x3E, 2), ("radius", 0x63C, 2),
    ("model", 0x6E6, 2), ("bones", 0, 1), ("sequence_model", 0x5B1, 1),
    ("common_entries", 0x5B4, 1), ("battle_entries", 0x5B5, 1),
    ("special_entries", 0x5B6, 1),
)
FIELD_COUNT = len(FIELDS) + 1
PITCHES = (-229, -228, 171, 172, 768, 769, -2048, 2048)
POINT_HEIGHTS = (-377, -375, 274, 276, 2476, 2490)


def immediate(op: int, target: int, source: int, value: int) -> int:
    return op << 26 | source << 21 | target << 16 | (value & 0xFFFF)


def mutants(function: int) -> dict:
    return {
        817: {
            "radius-128": {0x8009DD54: immediate(11, 2, 2, 128)},
            "pitch-low-boundary": {0x8009DD78: immediate(10, 2, 3, -227)},
            "pitch-high-boundary": {0x8009DD84: immediate(10, 2, 3, 173)},
            "special-pitch-770": {0x8009DD90: immediate(10, 2, 3, 770)},
            "ignore-full-parameter": {0x8009DD9C: 0},
            "omit-head-reset": {0x8009DDCC: 0},
        },
        818: {
            "ignore-drawn": {0x8009DE34: 0},
            "ignore-air-state": {0x8009DE50: 0},
            "radius-129": {0x8009DE78: immediate(11, 2, 2, 129)},
            "cleanup-nonplayer": {0x8009DEF0: 0},
            "clear-move-three": {0x8009DF04: 0},
            "keep-jump-countdown": {0x8009DF18: 0},
            "always-blend-eight": {0x8009DEDC: immediate(9, 7, 0, 8)},
            "omit-head-reset": {0x8009DEE0: 0},
        },
        819: {
            "truncate-full-mode-byte": {0x8009DF4C: immediate(12, 19, 5, 255)},
            "ignore-instance-player": {0x8009DF80: 0},
            "idle-animation-seven": {0x8009DFC8: immediate(9, 16, 0, 7)},
            "ready-animation-c8": {0x8009DFD0: immediate(9, 16, 0, 0xC8)},
            "alternate-animation-c6": {0x8009DFD8: immediate(9, 16, 0, 0xC6)},
            "ignore-unrelated-current-animation": {0x8009DFEC: 0},
            "finish-without-increment": {0x8009DFF4: 0},
            "omit-header-query": {0x8009E000: 0},
            "fallback-blend-eight": {0x8009E01C: immediate(9, 17, 0, 8)},
            "copy-only-modes-below-two": {0x8009E034: immediate(10, 2, 19, 2)},
            "unsigned-copy-mode": {0x8009E034: immediate(11, 2, 19, 3)},
            "omit-full-pose-copy": {0x8009E044: 0},
            "short-pose-copy": {0x8009E048: immediate(9, 6, 0, 0x540)},
        },
        823: {
            "turn-phase-one": {0x8009E344: 0},
            "radius-128": {0x8009E3D4: immediate(11, 2, 2, 128)},
            "omit-positive-yaw-wrap": {0x8009E3B4: 0},
            "fallback-55": {0x8009E314: immediate(9, 2, 0, 0x55)},
            "omit-fresh-drawn-read": {0x8009E428: 0},
            "omit-head-reset": {0x8009E454: 0},
        },
        831: {
            "signed-group-byte": {0x8009ECF0: immediate(32, 3, 2, 0)},
            "boolean-side-mask": {0x8009EDC8: immediate(12, 2, 2, 1)},
            "reset-side-any-sequence": {0x8009EDB0: 0},
            "omit-current-heading": {0x8009EE64: 0},
            "omit-wrapped-square-negation": {0x8009ED38: 0},
            "pitch-high-boundary": {0x8009ED94: immediate(10, 2, 3, 173)},
            "blend-six": {0x8009EE60: immediate(9, 7, 0, 6)},
        },
        832: {
            "ignore-drawn": {0x8009EEDC: 0},
            "ignore-air-state": {0x8009EEF8: 0},
            "radius-129": {0x8009EF20: immediate(11, 2, 2, 129)},
            "pitch-low-boundary": {0x8009EF5C: immediate(10, 2, 3, -227)},
            "pitch-high-boundary": {0x8009EF68: immediate(10, 2, 3, 173)},
            "omit-head-reset": {0x8009EF88: 0},
            "cleanup-nonplayer": {0x8009EF98: 0},
            "clear-move-three": {0x8009EFAC: 0},
            "keep-jump-countdown": {0x8009EFC0: 0},
            "omit-unconditional-impact-tail": {0x8009EFC4: 0},
            "impact-tail-slot-zero": {0x8009EFC8: 0x00002021},
        },
        833: {
            "mode-zero-radius-129": {0x8009F0AC: immediate(11, 2, 2, 129)},
            "always-fallback": {0x8009F128: 0},
            "retain-old-fallback-owner": {0x8009F130: 0x02208021},
            "fallback-radius-128": {0x8009F154: immediate(11, 2, 2, 128)},
            "pitch-reads-target-x": {0x8009F21C: immediate(33, 2, 18, 0)},
            "high-special-pitch-one": {0x8009F258: immediate(9, 2, 0, 1)},
            "omit-mode-zero-head-reset": {0x8009F0F8: 0},
        },
        834: {
            "omit-actual-special-policy": {0x8009F2AC: 0},
            "query-fe-for-other-modes": {0x8009F2B8: 0},
            "follow-up-slot-zero": {0x8009F2C8: 0x00001880},
            "queue-blend-six": {0x8009F2F0: immediate(9, 6, 0, 6)},
            "queue-start-one": {0x8009F2F8: immediate(9, 7, 0, 1)},
        },
        891: {
            "unsigned-short-facing": {0x800A2584: 0x00052C02},
            "turn-rate-four": {0x800A25A8: immediate(9, 6, 0, 4)},
            "omit-real-combat-pose-policy": {0x800A25B0: 0},
            "combat-pose-mode-two": {0x800A25B4: immediate(9, 5, 0, 2)},
            "clear-bit31": {0x800A25B8: immediate(15, 3, 0, 0x0FFF)},
            "leave-pose-bit28": {0x800A25B8: immediate(15, 3, 0, 0x9FFF)},
            "finish-state-seven": {0x800A25C8: immediate(15, 3, 0, 0x7000)},
            "omit-pose-state-store": {0x800A25D0: 0},
        },
    }[function]


def actor_values(rng: random.Random) -> list[int]:
    flags = rng.getrandbits(32)
    flags = (flags & ~0x00E70000) | rng.choice((0, 0x00800000))
    flags |= rng.randrange(8) << 16 | rng.choice((0, 0, 0x00200000, 0x00400000))
    return [
        flags, rng.choice((0, 1, 3, 16)), rng.randrange(1, 256),
        rng.getrandbits(16), *(s16(rng.getrandbits(16)) for _ in range(4)),
        rng.choice((-32768, -4096, -2049, -2048, 0, 2048, 4096, 32767)),
        s16(rng.getrandbits(16)), rng.choice((0, 127, 128, 129, 65535)),
        rng.choice((0, 2, 40, 65535)), rng.randrange(1, 6),
        rng.choice((0, 1, 2, 7)), rng.choice((0, 85, 88, 100)),
        rng.choice((0, 91, 92, 102)), rng.choice((0, 3, 31, 32, 64)),
        rng.randrange(2),
    ]


def safe_horizontal(point: list[int], actor: list[int]) -> bool:
    squares = [s32((point[axis] - actor[4 + axis]) ** 2) for axis in (0, 2)]
    return s32(sum((-value if value < 0 else value) & MASK for value in squares)) >= 0


def vector(function: int, rng: random.Random, number: int) -> dict:
    if function in REACTIONS:
        return reaction_vector(function, rng, number)
    actors = [actor_values(rng) for _ in ACTORS]
    parameter = {
        817: (0, 1, 0x10000, -1)[number % 4],
        818: 0,
        823: number % 2,
        831: number % 18,
        833: (0, 1, 1, 0, 2, -1, 0x10000, 0x10001)[number % 8],
        834: (0, 1, 1, 0, 2, -1, 0x10000, 0x10001)[number % 8],
    }[function]
    point = [s16(rng.getrandbits(16)) for _ in range(4)]
    if number < 384:
        for index, actor in enumerate(actors):
            actor[4:8] = [0, 0, 0, 0]
            actor[0] = (actor[0] & ~0x00E70000) | 0x00800000
            actor[1] = (number // 8 + index) % 4
            actor[10] = (127, 128, 129)[(number // 4 + index) % 3]
            actor[11] = 40
            actor[13:17] = [1, 100, 102, 64]
        point = [1024, POINT_HEIGHTS[(number // 2) % 6], 0, 0]
    if number % 13 == 0:
        # Both squares wrap negative individually, but their wrapped sum is valid.
        for actor in actors:
            actor[4], actor[6] = -32768, -32768
        point[0], point[2] = 32767, 32767
    preset = (number // 8) % 12
    if preset == 5 and function == 834 and parameter == 1:
        preset = 4  # A null FE owner is a separate refusal, not original parity.
    mutated_actor = list(actors[0])
    mutated_actor[4], mutated_actor[6] = -700, 400
    if not all(safe_horizontal(point, actor) for actor in actors) or (
        preset == 4 and not safe_horizontal(point, mutated_actor)
    ):
        for actor in actors:
            actor[4], actor[6] = rng.randrange(-12000, 12001), rng.randrange(-12000, 12001)
        point[0], point[2] = rng.randrange(-12000, 12001), rng.randrange(-12000, 12001)
    optional_actor = function in (817, 823, 831) or (
        function in (833, 834) and parameter not in (0, 1)
    )
    owner = -1 if optional_actor and number % 17 == 0 else 0
    if owner < 0 and function in (823, 831):
        parameter = (0x10000, -1, 0x7FFFFFFF)[number % 3]
    return {
        "function": function, "slot": (0, 1, 16)[number % 3],
        "parameter": parameter,
        "sequence": (0, 1, 2, -1, 0x10001, -0x80000000)[number % 6],
        "owner": owner, "point_present": number % 11 != 0,
        "controlled": number // 96 % 3 == 0,
        "helper_facing": (0x1FFFF, -32769, 2048, -2048, 32767)[number % 5],
        "helper_pitch": PITCHES[(number // 3) % len(PITCHES)],
        "loaded": number // 24 % 2 == 0,
        "headers": [bool((number // 8 + number // 96) % 2), bool(number // 16 % 2)],
        "group": (0, 1, 2, 127, 128, 255)[number % 6],
        "side": (0, 1, 2, 3, 0x80000000, 0xFFFFFFFF, rng.getrandbits(32))[number % 7],
        "special_pitch": (0, 1, 2)[number % 3],
        "bone_y": 0 if number < 384 else s16(rng.getrandbits(16)),
        "bone_success": True, "preset": preset, "point": point, "actors": actors,
        "raw": [rng.randbytes(RECORD_BYTES) for _ in ACTORS],
    }


def reaction_vector(function: int, rng: random.Random, number: int) -> dict:
    value = vector(818, rng, number)
    value["function"], value["owner"] = function, 0
    value["animations"] = [(0xC6, 0xC8, 0xC7, 0, 255)[
        (number // 3 + index) % 5] for index in range(3)]
    value["raw"] = [rng.randbytes(0x1800) for _ in ACTORS]
    if function == 819:
        modes = (-0x80000000, -1, 0, 1, 2, 3, 3, 3, 4, 0x10000, 0x10003, 0x7FFFFFFF)
        value["parameter"] = modes[number % len(modes)]
        value["preset"] = (0, 12, 14, 15, 0)[number // 12 % 5]
        value["actors"][0][1] = (1, 0, 16, 255)[number // 12 % 4]
        if number % 12 in (5, 6, 7):
            value["animations"][0] = (0xC6, 0xC8, 0xC7)[number % 12 - 5]
    elif function == 832:
        value["parameter"] = (0, -1, -0x80000000, 0x10000, 0x7FFFFFFF)[number % 5]
        actor = value["actors"][0]
        actor[0] = (actor[0] & ~0x00E70000) | (
            0, 0x00A00000, 0x00C00000, 0x00820000, 0x00830000, 0x00800000
        )[number // 4 % 6]
        actor[1] = (0, 1, 16)[number // 24 % 3]
    else:
        value["parameter"] = (
            -0x80000000, 0x7FFFFFFF, 0x18000, 0xFFFF, -32769, 32768, 1024, 0x10000
        )[number % 8]
        value["preset"] = (0, 13, 14, 15, 16, 17, 18, 0)[number // 8 % 8]
        value["actors"][0][1] = (1, 0, 16)[number // 64 % 3]
    return value


def pose_words(raw: bytes, offset: int) -> list[int]:
    return [int.from_bytes(raw[at:at + 4], "little")
            for at in range(offset, offset + POSE_BYTES, 4)]


def inputs(v: dict) -> list[int]:
    result = [
        v["function"], v["slot"], v["parameter"], v["sequence"], v["owner"],
        int(v["point_present"]), int(v["controlled"]), v["helper_facing"],
        v["helper_pitch"], int(v["loaded"]), *map(int, v["headers"]),
        v["group"], v["side"], v["special_pitch"], v["bone_y"],
        int(v["bone_success"]), v["preset"], *v["point"],
        *(value for actor in v["actors"] for value in actor),
    ]
    if v["function"] in REACTIONS:
        for animation, raw in zip(v["animations"], v["raw"], strict=True):
            result.extend((animation, *pose_words(raw, BLEND_POSE), *pose_words(raw, CURRENT_POSE)))
    return result


def original(machine: Machine, address: int) -> None:
    saved_ra = machine.r[31]
    machine.execute(address)
    machine.r[31] = saved_ra


def run(machine: Machine, v: dict) -> tuple[dict, list[int]]:
    machine.zero.append((TABLE, TABLE + 17 * 4))
    machine.put(TABLE + v["slot"] * 4, ACTORS[v["owner"]] if v["owner"] >= 0 else 0)
    for index, (base, bank, raw, actor) in enumerate(
        zip(ACTORS, BANKS, v["raw"], v["actors"], strict=True)
    ):
        values = {name: actor[index] for index, (name, _, _) in enumerate(FIELDS)}
        block = pack_fields(raw, FIELDS, values)
        machine.store(base, block)
        if v["function"] in REACTIONS:
            machine.put(base + 0x5B8, v["animations"][index], 1)
        for offset in (0x5D0, 0x5D4, 0x5D8, 0x5DC, 0x5E0, 0x5E4, 0x5E8):
            machine.put(base + offset, bank)
        data = bytearray(2048)
        # The PS1 loader rebases this map pointer; the native bank retains an
        # offset (Banks in the driver), with identical map/header bytes.
        data[4:8] = (bank + 1024).to_bytes(4, "little")
        data[1024:1280] = bytes([0 if actor[-1] else 255]) * 256
        machine.store(bank, data)
    machine.put(SHARED_SLOT, 0)
    machine.put(WORK_BANK, BANKS[0])
    machine.put(SIDE, v["side"])
    machine.put(SPECIAL_PITCH, v["special_pitch"], 1)
    machine.put(CLIP_INDEX, 0)
    machine.store(GROUPS, bytes([v["group"]]) * 168)
    machine.store(TARGET, b"".join((value & 0xFFFF).to_bytes(2, "little") for value in v["point"]))
    record_bytes = len(v["raw"][0])
    before = [machine.bytes(base, record_bytes) for base in ACTORS]
    queries, roots, angles, lookups = 0, [], [], []

    def owner(address: int) -> int:
        if address not in ACTORS:
            raise Fault(f"service used an absent/unknown actor {address:#x}")
        return ACTORS.index(address)

    def field(index: int, offset: int, value: int, size: int = 2) -> None:
        machine.put(ACTORS[index] + offset, value, size)

    def table(index: int) -> None:
        machine.put(TABLE + v["slot"] * 4, ACTORS[index] if index >= 0 else 0)

    def preset(event: list[int]) -> None:
        kind, index, *_ = event
        mode = v["preset"]
        if mode == 1 and kind == 1:
            field(index, 0x63C, 129)
            field(index, 0x6E6, 40)
            field(index, 8, machine.load(ACTORS[index] + 8) ^ 0x100, 4)
            table(1)
        elif mode == 2 and kind == 2:
            field(index, 0x6E6, 40)
            field(index, 8, machine.load(ACTORS[index] + 8) ^ 0x100, 4)
            table(1)
        elif mode in (3, 4, 5, 10) and kind == 3 and queries == 1:
            if mode in (3, 10):
                field(index, 8, machine.load(ACTORS[index] + 8) ^ 0x00800000, 4)
            if mode == 4:
                field(index, 0xF, 5, 1)
                field(index, 0x1C, -700)
                field(index, 0x20, 400)
                field(index, 0x26, 30000)
                if v["point_present"]:
                    machine.put(TARGET + 2, 1900, 2)
            if mode in (3, 4):
                table(1)
            if mode == 5:
                table(-1)
        elif mode == 6 and kind == 4:
            table(2)
            for index in range(3):
                field(index, 0xF, 0, 1)
                flags = machine.load(ACTORS[index] + 8)
                field(index, 8, flags & ~0x00070000 | 0x00020000, 4)
                field(index, 0x11, 199, 1)
        elif mode in (7, 8) and kind == 5:
            field(index, 0xF, 0, 1)
            movement = 0x00020000 if mode == 7 else 0x00030000
            field(index, 8, machine.load(ACTORS[index] + 8) & ~0x00070000 | movement, 4)
            field(index, 0x11, 199, 1)
            table(2)
        elif mode == 9 and kind == 6:
            for index in range(3):
                field(index, 0x1C, -1234)
                field(index, 0x20, 345)
                field(index, 0x26, 31500)
            if v["point_present"]:
                for offset, value in ((0, 999), (2, 1200), (4, -744)):
                    machine.put(TARGET + offset, value, 2)
        elif mode == 11 and kind == 1:
            field(index, 0x63C, 127)
            field(index, 0xF, 0, 1)
            field(index, 8, machine.load(ACTORS[index] + 8) & ~0x00070000 | 0x00020000, 4)
        elif mode == 12 and kind == 4:
            table(1)
            for index in range(3):
                for word in range(POSE_BYTES // 4):
                    address = ACTORS[index] + CURRENT_POSE + word * 4
                    machine.put(address, machine.load(address) ^ (0xA5C30000 + 0x100 * index + word))
        elif mode in (13, 18) and kind == 10:
            table(1)
            field(1, 0xF, 1, 1)
            field(1, 0x5B8, 0xC8, 1)
            field(0, 8, machine.load(ACTORS[0] + 8) ^ 0x8F123456, 4)
        elif mode in (14, 18) and kind == 3:
            field(index, 0x5B8, 0, 1)
            table(2)
            field(0, 8, machine.load(ACTORS[0] + 8) ^ 0x8F123456, 4)
        elif mode == 15 and kind == 4:
            table(2)
            field(0, 8, machine.load(ACTORS[0] + 8) ^ 0x8F123456, 4)
        elif mode == 16 and kind == 10:
            table(1)
            field(1, 0xF, 0, 1)
        elif mode == 17 and kind == 10:
            field(0, 0x5B8, 0xC6, 1)
            field(0, 8, machine.load(ACTORS[0] + 8) ^ 0x8F123456, 4)

    def record(event: list[int]) -> None:
        machine.trace.append(event)
        preset(event)

    def helper(m: Machine, kind: int, address: int, selected: int) -> None:
        record([kind, owner(m.r[4]), int(m.r[5] != 0), 0, 0])
        if v["controlled"]:
            m.r[2] = (selected if m.r[5] else 0) & MASK
        else:
            original(m, address)

    def bone(m: Machine) -> None:
        record([6, m.r[4], m.r[5], m.r[7], 0])
        m.put(m.r[6], 0, 2)
        m.put(m.r[6] + 2, v["bone_y"], 2)
        m.put(m.r[6] + 4, 0, 2)
        m.put(m.r[6] + 6, 0, 2)
        m.r[2] = 0

    def header(m: Machine) -> None:
        nonlocal queries
        index, animation, output = owner(m.r[4]), s32(m.r[5]), m.r[6]
        if v["loaded"]:
            original(m, HEADER)
        else:
            m.put(output, BANKS[index] if v["headers"][min(queries, 1)] else 0)
            m.r[2] = BANKS[index]
        present = m.load(output) != 0
        queries += 1
        lookups.append([index, animation, present, v["loaded"]])
        record([3, index, animation & MASK, 0, 0])

    def root(m: Machine) -> None:
        argument = s32(m.r[4])
        roots.append(argument)
        if argument < 0:
            raise Fault(f"negative root argument {argument}: separate unsupported domain")
        original(m, ROOT_HELPER)

    def atan(m: Machine) -> None:
        y, x = s32(m.r[4]), s32(m.r[5])
        original(m, ATAN)
        angles.append([y, x, s32(m.r[2])])

    write = machine.write

    def observed_store(address: int, value: int, size: int) -> None:
        write(address, value, size)
        if address == SIDE:
            record([8, value & MASK, 0, 0, 0])
        elif address == SPECIAL_PITCH:
            record([9, value & 255, 0, 0, 0])

    machine.write = observed_store
    machine.stub(FACING, lambda m: helper(m, 1, FACING, v["helper_facing"]))
    machine.stub(PITCH, lambda m: helper(m, 2, PITCH, v["helper_pitch"]))
    machine.stub(HEADER, header)
    machine.stub(ROOT_HELPER, root)
    machine.stub(ATAN, atan)
    machine.stub(BONE, bone)
    machine.stub(RESET, lambda m: record([5, owner(m.r[4]), 0, 0, 0]))
    machine.stub(PLAY, lambda m: record([4, m.r[4], m.r[5], m.r[6], m.r[7]]))
    machine.stub(QUEUE, lambda m: record([7, m.r[4], m.r[5], m.r[6], m.r[7]]))
    machine.stub(TURN, lambda m: record([10, m.r[4], m.r[5], m.r[6], 0]))
    machine.stub(IMPACT, lambda m: record([11, m.r[4], 0, 0, 0]))
    machine.follow.add(SPECIAL)
    machine.follow.update((SPANS[819][0], COPY))
    machine.r[4:8] = [
        v["slot"], TARGET if v["point_present"] else 0,
        v["parameter"] & MASK, v["sequence"] & MASK,
    ]
    if v["function"] in (819, 891):
        machine.r[5] = v["parameter"] & MASK
    machine.stores.clear()
    machine.execute(SPANS[v["function"]][0])
    address = machine.load(TABLE + v["slot"] * 4)
    observation = {
        "before": before,
        "after": [machine.bytes(base, record_bytes) for base in ACTORS],
        "owner": 0 if address == 0 else owner(address) + 1,
        "side": machine.load(SIDE),
        "special": machine.load(SPECIAL_PITCH, 1),
        "target": machine.bytes(TARGET, 8),
        "events": machine.trace, "roots": roots, "angles": angles, "lookups": lookups,
        "stores": machine.stores,
    }
    return observation, inputs(v)


def check(_image, v: dict, observation: dict, native: list[int], reference: dict) -> list[str]:
    if not native or native[0] != 0:
        return [f"native refused with result {native[0] if native else 'missing'}"]
    problems = []
    if native[1:4] != [observation["owner"], observation["side"], observation["special"]]:
        problems.append(f"owner/side/pitch {native[1:4]} != "
                        f"{[observation['owner'], observation['side'], observation['special']]}")
    start = 4 + 3 * FIELD_COUNT
    count = native[start + 4]
    tail = start + 5 + count * 5
    reaction_fields = 1 + POSE_BYTES // 2 if v["function"] in REACTIONS else 0
    for index in range(3):
        start = 4 + index * FIELD_COUNT
        values = {name: native[start + i] for i, (name, _, _) in enumerate(FIELDS)}
        expected = pack_fields(reference["before"][index], FIELDS, values)
        if reaction_fields:
            reaction = native[tail + index * reaction_fields:tail + (index + 1) * reaction_fields]
            expected[0x5B8] = reaction[0] & 255
            for offset, words in (
                (BLEND_POSE, reaction[1:1 + POSE_BYTES // 4]),
                (CURRENT_POSE, reaction[1 + POSE_BYTES // 4:]),
            ):
                expected[offset:offset + POSE_BYTES] = b"".join(
                    (word & MASK).to_bytes(4, "little") for word in words
                )
        if expected != observation["after"][index]:
            differing = [hex(at) for at in range(len(expected))
                         if expected[at] != observation["after"][index][at]]
            problems.append(f"actor {index} bytes {','.join(differing[:16])}")
        if native[start + FIELD_COUNT - 1] != v["actors"][index][-1]:
            problems.append(f"actor {index} changed its loaded bank")
    start = 4 + 3 * FIELD_COUNT
    target = b"".join((value & 0xFFFF).to_bytes(2, "little") for value in native[start:start + 4])
    if target != observation["target"]:
        problems.append("target fields")
    count = native[start + 4]
    events = [native[start + 5 + i * 5:start + 10 + i * 5] for i in range(count)]
    if len(native) != tail + 3 * reaction_fields:
        problems.append("driver output length")
    if events != observation["events"]:
        problems.append(f"calls {events} != {observation['events']}")
    allowed = [
        (DEFAULT_STACK - 0x1000, DEFAULT_STACK),
        *((base, base + len(v["raw"][0])) for base in ACTORS),
        (SIDE, SIDE + 4), (SPECIAL_PITCH, SPECIAL_PITCH + 1),
        (CLIP_INDEX, CLIP_INDEX + 4),
    ]
    for address, size in observation["stores"]:
        if not any(low <= address and address + size <= high for low, high in allowed):
            problems.append(f"unexpected original store {address:#x}+{size}")
            break
    return problems


def describe(_image, results: list) -> list[str]:
    counts = {
        "null_actor": 0, "null_point": 0, "controlled_helpers": 0,
        "real_helper_math": 0, "real_header_queries": 0, "controlled_header_queries": 0,
        "negative_wrapped_square": 0, "bone_callback_mutation": 0,
        "full_nonboolean_side": 0, "replacement_callbacks": 0,
        "post_play_pose_mutation": 0, "turn_owner_replacement": 0,
        "turn_and_header_replacement": 0, "impact_tail": 0,
    }
    for v, observation, _ in results:
        counts["null_actor"] += v["owner"] < 0
        counts["null_point"] += not v["point_present"]
        counts["controlled_helpers" if v["controlled"] else "real_helper_math"] += 1
        counts["real_header_queries"] += sum(row[3] for row in observation["lookups"])
        counts["controlled_header_queries"] += sum(not row[3] for row in observation["lookups"])
        counts["bone_callback_mutation"] += v["preset"] == 9 and any(
            event[0] == 6 for event in observation["events"])
        counts["full_nonboolean_side"] += v["side"] not in (0, 1)
        counts["replacement_callbacks"] += v["preset"] in (1, 2, 3, 4, 6, 7, 8)
        counts["post_play_pose_mutation"] += v["preset"] == 12 and any(
            event[0] == 4 for event in observation["events"])
        counts["turn_owner_replacement"] += v["preset"] in (13, 16, 18)
        counts["turn_and_header_replacement"] += v["preset"] == 18 and any(
            event[0] == 3 for event in observation["events"])
        counts["impact_tail"] += any(event[0] == 11 for event in observation["events"])
        counts["negative_wrapped_square"] += any(
            s32((v["point"][axis] - actor[4 + axis]) ** 2) < 0
            for actor in v["actors"] for axis in (0, 2)
        )
    return [
        "Coverage: " + ", ".join(f"{key}={value}" for key, value in counts.items()),
        "Original facing/pitch, atan2, approximate integer root and loaded-bank "
        "header lookup instructions execute in real-helper vectors. Selected helper "
        "returns, root-bone output, Play/Queue/Reset and post-service mutations are "
        "explicit controls; reaction cases follow the real F00819 instructions and "
        "original memcpy, while Turn/impact effects are controlled. This is policy "
        "parity, not animation playback/gameplay.",
    ]


def configure(function: int) -> dict:
    entry, end = SPANS[function]
    return {
        "ID": f"F{function:05}", "ENTRY": entry, "END": end,
        "DRIVER": DRIVER, "INCLUDES": INCLUDES, "SOURCES": SOURCES, "MODULES": MODULES,
        "VECTORS": 1024 if function in REACTIONS else 4096,
        "SEED": function, "MUTANT_VECTORS": 768,
        "MUTANTS": mutants(function),
        "vector": lambda rng, number: vector(function, rng, number),
        "run": run, "check": check, "describe": describe,
    }
