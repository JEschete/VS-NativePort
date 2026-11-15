"""F01195/F01214-F01218 event camera ownership and origin against original BATTLE.PRG."""

import random
from dataclasses import dataclass

from differential import (
    DEFAULT_STACK,
    Image,
    Machine,
    printed_problems,
    recorded_observation,
)

ID = "F01195/F01214-F01218"
FUNCTIONS = ("F01195", "F01214", "F01215", "F01216", "F01217", "F01218")
ENTRIES = (0x800BB38C, 0x800BBBE8, 0x800BBDDC, 0x800BBE04, 0x800BBE10, 0x800BBE94)
# F01218 ends where vs_battle_applyCameraState starts (0x800BBF14).
ENDS = (0x800BB3BC, 0x800BBDDC, 0x800BBE04, 0x800BBE10, 0x800BBE94, 0x800BBF14)
ENTRY, END = ENTRIES[0], ENDS[-1]
MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "event_camera_ownership_driver.cpp"
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
INCLUDES = (
    r"Native\Game\src",
    r"Native\Engine",
    r"Native\Engine\src",
    r"Native\Content",
)
SOURCES = (
    r"Native\Game\src\Event\EventCameraOwnership.cpp",
    r"Native\Game\src\Event\EventCameraOwnershipLive.cpp",
    r"Native\Engine\src\Core\Log.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
VECTORS, MUTANT_VECTORS, SEED = 6144, 1200, 121195

MASK = 0xFFFFFFFF
CAMERA, PREVIOUS = 0x80500000, 0x80510000
CAMERA_SIZE = 0x28C
CAMERA_WORDS = CAMERA_SIZE // 4
CAMERA_GLOBAL, SKIP_GLOBAL = 0x800F4BA4, 0x800F4BA0
DEFAULT_TINT, DEFAULT_TRANSFORM = 0x800E9C08, 0x800E9C0C
INSTRUCTION = 0x80600000
WORLD = 0x2000
ORIGIN_ACTOR, ORIGIN_OFFSET = 0x1E8, 0x1EC
LOOK_AT_POSITION, EYE_POSITION = 0x94, 0x148

ALLOCATE, FREE = 0x80043E3C, 0x80043C60
CAMERA_POSITION, CAMERA_LOOK_AT = 0x8007ABEC, 0x8007AC40
ACTOR_PLACEMENT, RESOLVE_REFERENCE = 0x800A1108, 0x800BFE50
# Leaf helpers run as original code: getShort, _vectorAdd and _copyVector.
FOLLOWED = (0x800BFE00, 0x800BE628, 0x800BE71C, ENTRIES[4], ENTRIES[5])
# The framebuffer effect requests in call order: enable, roll, scale, tint, offset, blend.
SCREEN_REQUESTS = (0x8007DD50, 0x8007DDAC, 0x8007DDB8, 0x8007DDD4, 0x8007DDF8, 0x8007DE2C)
POINTER_REQUESTS = {0x8007DDB8: DEFAULT_TRANSFORM, 0x8007DDD4: DEFAULT_TINT, 0x8007DDF8: DEFAULT_TRANSFORM}

# Call kinds shared with the driver.
CALL_ALLOCATE, CALL_FREE, CALL_POSITION, CALL_LOOK_AT = 1, 2, 3, 4
CALL_SCREEN, CALL_RESOLVE, CALL_PLACEMENT = 10, 20, 21

JR_RA, NOP = 0x03E00008, 0x00000000
MUTANTS = {
    "F01195-early-return": {ENTRIES[0]: JR_RA},
    # The reference reaches the origin unresolved.
    "F01195-unresolved-reference": {0x800BB39C: NOP},
    "F01214-early-return": {ENTRIES[1]: JR_RA},
    "F01214-near-clip-0x41": {0x800BBD7C: 0x24020041},
    "F01215-early-return": {ENTRIES[2]: JR_RA},
    "F01216-early-return": {ENTRIES[3]: JR_RA, ENTRIES[3] + 4: NOP},
    "F01216-stores-zero": {0x800BBE0C: 0xA0404BA0},
    "F01217-early-return": {ENTRIES[4]: JR_RA},
    # beq -> bne: the world-origin switch adds the offset only for the same actor.
    "F01217-adds-for-same-actor": {0x800BBE44: 0x1443000A},
    "F01218-early-return": {ENTRIES[5]: JR_RA},
    "F01218-offset-shift-11": {0x800BBEDC: 0x000212C0},
}


@dataclass(frozen=True)
class Vector:
    function: int
    camera_code: int
    skip: int
    argument: int
    odd: int
    reference: int
    resolved: int
    actor_present: int
    position: tuple[int, int, int]
    facing: int
    field_eye: tuple[int, int, int]
    field_look_at: tuple[int, int, int]
    block: tuple[int, ...]

    def inputs(self) -> list[int]:
        return [
            self.function,
            self.camera_code,
            self.skip,
            self.argument,
            self.odd,
            self.reference,
            self.resolved & MASK,
            self.actor_present,
            *(value & 0xFFFF for value in self.position),
            self.facing,
            *self.field_eye,
            *self.field_look_at,
            *self.block,
        ]


def _actor(rng: random.Random) -> int:
    return rng.choice((WORLD, WORLD, rng.getrandbits(5), 0x1000 | rng.getrandbits(5),
                       rng.getrandbits(16), 0xFFFF, 0))


def _put_half(block: list[int], offset: int, value: int) -> None:
    index, shift = offset // 4, (offset % 4) * 8
    block[index] = (block[index] & ~(0xFFFF << shift) | (value & 0xFFFF) << shift) & MASK


def _put_vector(block: list[int], offset: int, words: tuple[int, int, int]) -> None:
    for axis, value in enumerate(words):
        block[offset // 4 + axis] = value & MASK


def _vector_words(rng: random.Random) -> tuple[int, int, int]:
    pick = rng.randrange(4)
    if pick == 0:
        return tuple(rng.getrandbits(32) for _ in range(3))
    if pick == 1:
        return tuple(rng.randrange(-0x800000, 0x800000) & MASK for _ in range(3))
    if pick == 2:
        return tuple(rng.choice((0, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, 0x7FFF000)) for _ in range(3))
    return (0, 0, 0)


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % len(FUNCTIONS), number // len(FUNCTIONS)
    block = [rng.getrandbits(32) for _ in range(CAMERA_WORDS)]
    current = _actor(rng)
    argument = rng.getrandbits(32)
    resolved = rng.getrandbits(32)
    camera_code = 1
    if function == 0:
        resolved = rng.choice((WORLD, WORLD, 0x12000, -1, current, rng.getrandbits(5),
                               0x1000 | rng.getrandbits(5), rng.getrandbits(32)))
        if lane < 8:
            current, resolved = ((WORLD, WORLD), (7, WORLD), (WORLD, 7), (3, 9))[lane % 4]
    elif function == 1:
        camera_code = rng.choice((0, 2))
    elif function == 2:
        camera_code = (0, 1, 2)[lane % 3]
    elif function == 3:
        argument = rng.choice((0, 1, 0x7F, 0x80, 0xFF, 0x100, 0xFFFFFFFF, rng.getrandbits(32)))
    elif function == 4:
        argument = rng.choice((WORLD, WORLD, current, _actor(rng)))
        if lane < 8:
            current, argument = ((WORLD, WORLD), (7, WORLD), (WORLD, 7), (3, 9))[lane % 4]
        # The original compares and stores only the low half.
        argument |= rng.choice((0, rng.getrandbits(16))) << 16
    _put_half(block, ORIGIN_ACTOR, current)
    _put_vector(block, ORIGIN_OFFSET, _vector_words(rng))
    _put_vector(block, EYE_POSITION, _vector_words(rng))
    _put_vector(block, LOOK_AT_POSITION, _vector_words(rng))
    edge = (0, 1, -1, 0x7FFF, -0x8000)
    return Vector(
        function=function,
        camera_code=camera_code,
        skip=rng.getrandbits(8),
        argument=argument & MASK,
        odd=rng.getrandbits(1),
        reference=rng.choice((WORLD, 0x1000 | rng.getrandbits(5), rng.getrandbits(16))),
        resolved=resolved,
        actor_present=int(rng.randrange(5) != 0),
        position=tuple(rng.choice((*edge, rng.randrange(-0x8000, 0x8000))) for _ in range(3)),
        facing=rng.getrandbits(16),
        field_eye=tuple(rng.getrandbits(32) for _ in range(3)),
        field_look_at=tuple(rng.getrandbits(32) for _ in range(3)),
        block=tuple(block),
    )


def _checksum(data: bytes) -> int:
    value = 0x811C9DC5
    for byte in data:
        value = ((value ^ byte) * 0x01000193) & MASK
    return value


def _pointer_code(address: int) -> int:
    return {0: 0, CAMERA: 1, PREVIOUS: 2}.get(address, 3)


def _install_stubs(machine: Machine, value: Vector, calls: list[tuple[int, int, int]]) -> None:
    def allocate(m: Machine) -> None:
        calls.append((CALL_ALLOCATE, m.r[4], 0))
        m.r[2] = CAMERA

    def release(m: Machine) -> None:
        calls.append((CALL_FREE, _pointer_code(m.r[4]), 0))

    def field_vector(kind: int, words: tuple[int, int, int]):
        def write(m: Machine) -> None:
            calls.append((kind, 0, 0))
            for axis, word in enumerate((*words, 0)):
                m.put(m.r[4] + axis * 4, word)
        return write

    def screen_request(index: int, address: int):
        def record(m: Machine) -> None:
            wanted = POINTER_REQUESTS.get(address)
            if wanted is None:
                argument = m.r[4]
            else:
                argument = 0 if m.r[4] == 0 else (1 if m.r[4] == wanted else 2)
            calls.append((CALL_SCREEN + index, argument, _checksum(m.bytes(CAMERA, CAMERA_SIZE))))
        return record

    def resolve(m: Machine) -> None:
        calls.append((CALL_RESOLVE, m.r[4], 0))
        m.r[2] = value.resolved & MASK

    def placement(m: Machine) -> None:
        calls.append((CALL_PLACEMENT, m.r[4], 0))
        if not value.actor_present:
            m.r[2] = MASK
            return
        out = m.r[5]
        m.put(out, 0x01020304)
        for axis, coordinate in enumerate((*value.position, value.facing)):
            m.put(out + 4 + axis * 2, coordinate, 2)
        m.r[2] = 0

    machine.stub(ALLOCATE, allocate)
    machine.stub(FREE, release)
    machine.stub(CAMERA_POSITION, field_vector(CALL_POSITION, value.field_eye))
    machine.stub(CAMERA_LOOK_AT, field_vector(CALL_LOOK_AT, value.field_look_at))
    for index, address in enumerate(SCREEN_REQUESTS):
        machine.stub(address, screen_request(index, address))
    machine.stub(RESOLVE_REFERENCE, resolve)
    machine.stub(ACTOR_PLACEMENT, placement)
    machine.follow.update(FOLLOWED)


def run(machine: Machine, value: Vector) -> tuple[dict, list[int]]:
    for index, word in enumerate(value.block):
        machine.put(CAMERA + index * 4, word)
    machine.map_zero(PREVIOUS, CAMERA_SIZE)
    machine.map_zero(INSTRUCTION, 8)
    machine.store(INSTRUCTION + value.odd,
                  bytes((0xE8, value.reference & 0xFF, value.reference >> 8)))
    machine.put(CAMERA_GLOBAL, (0, CAMERA, PREVIOUS)[value.camera_code])
    machine.put(SKIP_GLOBAL, value.skip, 1)
    calls: list[tuple[int, int, int]] = []
    _install_stubs(machine, value, calls)
    machine.stores.clear()

    if value.function == 0:
        machine.r[4:6] = [INSTRUCTION + value.odd, 0]
    elif value.function in (3, 4):
        machine.r[4] = value.argument
    machine.execute(ENTRIES[value.function], limit=10000)

    observed = [
        _pointer_code(machine.load(CAMERA_GLOBAL)),
        machine.load(SKIP_GLOBAL, 1),
        machine.r[2] if value.function == 0 else 0,
        *(machine.load(CAMERA + index * 4) for index in range(CAMERA_WORDS)),
    ]
    names = [
        "camera-pointer",
        "skip-field-camera-restore",
        "return",
        *(f"camera-word-{index * 4:#x}" for index in range(CAMERA_WORDS)),
    ]
    return recorded_observation(machine, observed, names, calls), value.inputs()


def check(
    image: Image, value: Vector, observation: dict, native: list[int], reference: dict
) -> list[str]:
    del image, value, reference
    allowed = [
        (DEFAULT_STACK - 0x2000, DEFAULT_STACK + 32),
        (CAMERA, CAMERA + CAMERA_SIZE),
        (CAMERA_GLOBAL, CAMERA_GLOBAL + 4),
        (SKIP_GLOBAL, SKIP_GLOBAL + 1),
    ]
    return printed_problems(observation, native, allowed)


def describe(image: Image, results: list) -> list[str]:
    by_function = [0] * len(FUNCTIONS)
    switches = empty = 0
    for value, _observation, _ in results:
        by_function[value.function] += 1
        empty += value.function in (0, 4, 5) and not value.actor_present
        before = value.block[ORIGIN_ACTOR // 4] & 0xFFFF
        target = (value.resolved if value.function == 0 else value.argument) & 0xFFFF
        switches += value.function in (0, 4) and target == WORLD and before != WORLD
    ranges = ", ".join(
        f"{name} {entry:08X}-{end - 4:08X} ({(end - entry) // 4} words, "
        f"sha256={image.sha256(entry, end)[:16]})"
        for name, entry, end in zip(FUNCTIONS, ENTRIES, ENDS, strict=True)
    )
    return [
        "Functions: " + ranges,
        "Vectors per function: "
        + ", ".join(f"{name}={count}" for name, count in zip(FUNCTIONS, by_function, strict=True)),
        f"World-origin switches from an actor: {switches}; empty actor slots: {empty} "
        "(the harness stack reads zero there, matching the native zero offset; the "
        "original stored stale stack bytes).",
        "Compared the camera pointer, the field camera override, every workspace word, the "
        "call sequence with each framebuffer request's argument and workspace checksum, and "
        "the opcode result.",
    ]
