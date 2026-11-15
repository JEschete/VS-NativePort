"""F01193/F01194/F01196/F01220/F01236-F01238 event camera follow, stop and field returns against original BATTLE.PRG.

The stop, follow and field camera restore opcodes, the field return glide, the follow requests, the stop and
the orbit back to the saved field view run as original code. The camera motion command they start (F01221)
runs for real with the BATTLE vector helpers and the SLUS arctangent and square root it calls followed, so the
synthesized command bytes are compared through the motion they start. The field camera placement and the
script flag query are stubbed with the same deterministic values the native driver returns.
"""

import random
from dataclasses import dataclass

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Image,
    Machine,
    printed_problems,
    recorded_observation,
)
from event_camera_track import FOLLOWED as MOTION_FOLLOWED

ID = "F01193/F01194/F01196/F01220/F01236-F01238"
FUNCTIONS = ("F01193", "F01194", "F01196", "F01220", "F01236", "F01237", "F01238")
ENTRIES = (0x800BB33C, 0x800BB35C, 0x800BB3BC, 0x800BC1CC, 0x800BE36C, 0x800BE3A0, 0x800BE3D0)
# Verified against the symbol order: F01195 (0x800BB38C) follows F01194, F01197 (0x800BB41C) follows F01196,
# F01221 (0x800BC2E8) follows F01220 and F01239 (0x800BE53C) follows F01238.
ENDS = (0x800BB35C, 0x800BB38C, 0x800BB41C, 0x800BC2E8, 0x800BE3A0, 0x800BE3D0, 0x800BE53C)
ENTRY, END = ENTRIES[0], ENDS[-1]
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "event_camera_control_driver.cpp"
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
INCLUDES = (
    r"Native\Game\src",
    r"Native\Engine",
    r"Native\Engine\src",
    r"Native\Content",
)
SOURCES = (
    r"Native\Game\src\Event\EventCameraControl.cpp",
    r"Native\Game\src\Event\EventCameraControlLive.cpp",
    r"Native\Game\src\Event\EventCameraTrack.cpp",
    r"Native\Game\src\Event\EventCameraTrackLive.cpp",
    r"Native\Engine\src\Core\Log.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
)
VECTORS, MUTANT_VECTORS, SEED = 7 * 1024, 7 * 96, 121193

MASK = 0xFFFFFFFF
CAMERA = 0x80500000
CAMERA_SIZE = 0x28C
CAMERA_WORDS = CAMERA_SIZE // 4
EYE = 0xB4
CAMERA_GLOBAL = 0x800F4BA4
# Event_skipNextFieldCameraRestore (byte) and Event_isFieldCameraRestoreScripted (halfword).
SKIP, SCRIPTED = 0x800F4BA0, 0x800F4BB6
# D_800E9C1C: the field return's look-at and eye commands, whose duration bytes the original rewrites.
RETURN_COMMANDS = 0x800E9C1C
INSTRUCTION = 0x80600000
SCRATCH, SCRATCH_SIZE = 0x1F800000, 0x400

# Track fields (Event_CameraTrack, Event/Camera.h).
DURATION, DESTINATION, CURRENT, PARTNER = 5, 0x24, 0x94, 0xA4

SPHERICAL, FLAG = 0x8007CD70, 0x800BEBF4
FACING_FLAG, DISTANCE_FLAG = 0xAF, 0xB0
# Call kinds shared with the driver.
CALL_SPHERICAL, CALL_FLAG = 60, 70

JR_RA, NOP = 0x03E00008, 0x00000000
MUTANTS = {
    "F01193-early-return": {ENTRIES[0]: JR_RA},
    "F01193-returns-1": {0x800BB350: 0x24020001},
    "F01194-early-return": {ENTRIES[1]: JR_RA},
    "F01194-eye-bit-0": {0x800BB36C: 0x30850001},
    "F01194-look-at-bit-1": {0x800BB370: 0x30840002},
    # The eye request keeps the raw bit 2 instead of 1.
    "F01194-eye-unnormalized": {0x800BB378: NOP},
    "F01196-no-half-turn": {0x800BB3DC: NOP},
    "F01196-facing-mask-15": {0x800BB3E0: 0x3043000F},
    "F01196-keeps-distance": {0x800BB3F4: NOP},
    "F01196-not-scripted": {0x800BB40C: NOP},
    "F01220-keeps-override": {0x800BC1F4: NOP},
    "F01220-half-duration": {0x800BC298: 0x00081443},
    # Tests the quotient's low byte instead of its low halfword: a 256-frame return starts nothing.
    "F01220-tests-low-byte": {0x800BC2A4: 0x00021600},
    "F01220-no-eye-command": {0x800BC2CC: NOP},
    "F01220-swapped-destinations": {0x800BC228: 0x24840024, 0x800BC238: 0x248400D8},
    # Measures the eye's distance as destination less current, which floors the other way.
    "F01220-eye-distance-reversed": {0x800BC248: 0x24C500D8, 0x800BC250: 0x24C60148},
    "F01236-early-return": {ENTRIES[4]: JR_RA},
    "F01236-keeps-look-at-following": {0x800BE384: NOP},
    "F01236-eye-gets-look-at": {0x800BE390: 0xA04400BA},
    "F01237-early-return": {ENTRIES[5]: JR_RA},
    "F01237-keeps-eye-mode": {0x800BE3AC: NOP},
    "F01237-mode-14": {0x800BE3A8: 0x2402000E},
    "F01237-keeps-look-at-request": {0x800BE3CC: NOP},
    "F01238-facing-plus-one": {0x800BE414: 0x26660001},
    "F01238-side-not-doubled": {0x800BE41C: NOP},
    "F01238-lift-shift-11": {0x800BE460: 0x000212C0},
    "F01238-look-at-not-lifted": {0x800BE478: NOP},
    "F01238-distance-plus-two": {0x800BE42C: 0x26140002},
    "F01238-no-look-at-command": {0x800BE514: NOP},
}


@dataclass(frozen=True)
class Vector:
    function: int
    argument0: int
    argument1: int
    instruction: tuple[int, int, int]
    skip: int
    scripted: int
    flags: tuple[int, int]
    flag_high: tuple[int, int]
    placements: tuple[int, ...]
    block: tuple[int, ...]

    def inputs(self) -> list[int]:
        return [
            self.function,
            self.argument0 & MASK,
            self.argument1 & MASK,
            *self.instruction,
            self.skip,
            self.scripted,
            *self.flags,
            *(word & MASK for word in self.placements),
            *self.block,
        ]


def _position(rng: random.Random, center: list[int]) -> list[int]:
    spread = rng.choice((0, 0x1000, 0x10000, 0x100000, 0x800000))
    return [axis + rng.randint(-spread, spread) for axis in center]


def _near(rng: random.Random, words: list[int]) -> list[int]:
    return [word + rng.randint(-0x10000, 0x10000) for word in words]


def _put_vector(block: list[int], offset: int, words) -> None:
    for axis, word in enumerate(words):
        block[offset // 4 + axis] = word & MASK


def _track_position(block: list[int], base: int, field: int) -> list[int]:
    return [block[(base + field) // 4 + axis] - ((block[(base + field) // 4 + axis] & 0x80000000) << 1)
            for axis in range(3)]


def _placements(rng: random.Random, function: int, block: list[int], center: list[int]) -> list[int]:
    """The field camera's two placements (eye, then look-at, each time); the return lanes vary the travel."""
    eye, look_at = _track_position(block, EYE, CURRENT), _track_position(block, 0, CURRENT)
    travel = rng.randrange(5)
    if function not in (2, 3) or travel < 2:
        return [word for _ in range(4) for word in _position(rng, center)]
    if travel == 2:
        # Under a few frames of travel, often none.
        first = _near(rng, eye) + _near(rng, look_at)
    elif travel == 3:
        # About 256 frames: the low halfword is set and the duration byte wraps to 0.
        moved = list(eye)
        moved[0] += rng.choice((1, -1)) * (0x2800000 + rng.randrange(0x28000))
        first = moved + look_at
    else:
        # Exactly the current positions.
        first = eye + look_at
    return first + [word for _ in range(2) for word in _position(rng, center)]


def vector(rng: random.Random, number: int) -> Vector:
    function = number % len(FUNCTIONS)
    block = [rng.getrandbits(32) for _ in range(CAMERA_WORDS)]
    center = [rng.randrange(-0x800000, 0x800000) for _ in range(3)]
    # The motion command's square roots and arctangents stay in the original's domain.
    for base in (0, EYE):
        for field in (DESTINATION, CURRENT, PARTNER):
            _put_vector(block, base + field, _position(rng, center))
    if function in (0, 1, 2):
        argument0, argument1 = INSTRUCTION, 0
    elif function == 6:
        argument0, argument1 = rng.choice((0, 1, rng.getrandbits(8), rng.getrandbits(32))), rng.getrandbits(32)
    else:
        argument0 = rng.choice((0, 1, 2, 7, 0xFFFF, rng.getrandbits(16), rng.getrandbits(32), MASK))
        argument1 = rng.choice((0, 1, 2, 0xFF, 0xFFFF, MASK, rng.getrandbits(32)))
    instruction = (
        (0xDE, 0xDF, 0xE9, 0)[min(function, 3)],
        rng.choice((0xFF, 0xFF, rng.randrange(8), rng.getrandbits(8))),
        rng.choice((0, 0, 1, 2, rng.getrandbits(8))),
    )
    return Vector(
        function=function,
        argument0=argument0,
        argument1=argument1,
        instruction=instruction,
        skip=rng.choice((0, 0, 0, 1, rng.getrandbits(8))),
        scripted=rng.getrandbits(16),
        flags=(rng.choice((rng.randrange(8), rng.randrange(8), rng.getrandbits(8))), rng.getrandbits(1)),
        flag_high=(rng.getrandbits(24), rng.getrandbits(24)),
        placements=tuple(_placements(rng, function, block, center)),
        block=tuple(block),
    )


def _install_stubs(machine: Machine, value: Vector, calls: list[tuple[int, int, int]]) -> None:
    def spherical(m: Machine) -> None:
        index = sum(call[0] == CALL_SPHERICAL for call in calls)
        calls.append((CALL_SPHERICAL, m.r[6], m.r[7]))
        words = value.placements[index * 6:index * 6 + 6] if index < 2 else (0,) * 6
        for axis in range(3):
            m.put(m.r[4] + axis * 4, words[axis] & MASK)
            m.put(m.r[5] + axis * 4, words[3 + axis] & MASK)

    def flag(m: Machine) -> None:
        ident = m.r[4]
        calls.append((CALL_FLAG, ident, 0))
        slot = {FACING_FLAG: 0, DISTANCE_FLAG: 1}.get(ident)
        # The original masks the word to its byte.
        m.r[2] = 0 if slot is None else (value.flag_high[slot] << 8 | value.flags[slot]) & MASK

    machine.stub(SPHERICAL, spherical)
    machine.stub(FLAG, flag)
    machine.follow.update((*ENTRIES, *MOTION_FOLLOWED))


def run(machine: Machine, value: Vector) -> tuple[dict, list[int]]:
    calls: list[tuple[int, int, int]] = []
    for index, word in enumerate(value.block):
        machine.put(CAMERA + index * 4, word)
    machine.put(CAMERA_GLOBAL, CAMERA)
    machine.put(SKIP, value.skip, 1)
    machine.put(SCRIPTED, value.scripted, 2)
    machine.map_zero(SCRATCH, SCRATCH_SIZE)
    machine.map_zero(INSTRUCTION, 16)
    machine.store(INSTRUCTION, bytes(value.instruction))
    _install_stubs(machine, value, calls)
    machine.stores.clear()
    machine.r[4:6] = [value.argument0 & MASK, value.argument1 & MASK]
    machine.execute(ENTRIES[value.function], limit=400000)
    result = machine.r[2] if value.function < 3 else 0
    observed = [
        result,
        *(machine.load(CAMERA + index * 4) for index in range(CAMERA_WORDS)),
        machine.load(SKIP, 1),
        machine.load(SCRIPTED, 2),
    ]
    names = [
        "return",
        *(f"camera-word-{index * 4:#x}" for index in range(CAMERA_WORDS)),
        "skip-next-restore",
        "restore-scripted",
    ]
    observation = recorded_observation(machine, observed, names, calls)
    # Whether the field return rewrote its commands' duration, and the duration byte.
    observation["glide"] = (any(low == RETURN_COMMANDS + 2 for low, _ in machine.stores),
                            machine.load(RETURN_COMMANDS + 2, 1))
    return observation, value.inputs()


def check(
    image: Image, value: Vector, observation: dict, native: list[int], reference: dict
) -> list[str]:
    del image, value, reference
    allowed = [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (CAMERA, CAMERA + CAMERA_SIZE),
        (SCRATCH, SCRATCH + SCRATCH_SIZE),
        (SKIP, SKIP + 1),
        (SCRIPTED, SCRIPTED + 2),
        (RETURN_COMMANDS, RETURN_COMMANDS + 7),
    ]
    return printed_problems(observation, native, allowed)


def describe(image: Image, results: list) -> list[str]:
    by_function = [0] * len(FUNCTIONS)
    skipped = glides = wrapped = still = 0
    facings = kept_facing = kept_distance = 0
    distance_indexes = [0, 0]
    for value, observation, _ in results:
        by_function[value.function] += 1
        restores = value.function in (2, 3)
        if restores and value.skip:
            skipped += 1
        elif restores:
            started, duration = observation["glide"]
            glides += started
            wrapped += started and duration == 0
            still += not started
        if value.function == 2:
            kept_facing += value.instruction[1] == 0xFF
            kept_distance += value.instruction[2] == 0
        if value.function == 6:
            facings += value.flags[0] > 7
            distance_indexes[value.flags[1]] += 1
    ranges = ", ".join(
        f"{name} {entry:08X}-{end - 4:08X} ({(end - entry) // 4} words, "
        f"sha256={image.sha256(entry, end)[:16]})"
        for name, entry, end in zip(FUNCTIONS, ENTRIES, ENDS, strict=True)
    )
    commands = image.data[RETURN_COMMANDS - image.base:RETURN_COMMANDS - image.base + 0x13]
    return [
        "Functions: " + ranges,
        "Vectors per function: "
        + ", ".join(f"{name}={count}" for name, count in zip(FUNCTIONS, by_function, strict=True)),
        f"Module commands and height offsets (D_800E9C1C-D_800E9C2E): {commands.hex(' ')}",
        f"F01196/F01220 restores skipped by the override: {skipped}; glides started: {glides} "
        f"({wrapped} with the duration byte wrapped to 0); returns under one frame of travel: {still}",
        f"F01196 facing kept: {kept_facing}; distance kept: {kept_distance}",
        f"F01238 distance indexes 0/1: {distance_indexes[0]}/{distance_indexes[1]} (the engine sets flag 0xB0 "
        f"to 0 or 1; the original read past D_800E9C24 beyond them); saved facings above 7: {facings}",
        "Compared the opcode result, every workspace word, the override byte, the scripted halfword, the field "
        "camera placement and flag calls in order with their arguments, and the original's stores (the "
        "duration bytes it wrote into D_800E9C1C are allowed; the native builds the commands locally).",
    ]
