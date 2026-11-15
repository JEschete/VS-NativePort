"""F01221/F01223-F01225/F01239/F01240/F01246 event camera track motion against original BATTLE.PRG.

The camera motion command, the per-frame track update, the terrain glide step, the ground clearance probe,
the distance-scaled durations and the script fixed-point vector run as original code with the BATTLE vector
helpers and the SLUS rotation, arctangent, square root and GTE transform routines followed. The floor height,
easing curve, actor reference, actor placement and tagged bone queries are stubbed with the same
deterministic values the native driver returns.
"""

import random
from dataclasses import dataclass, replace

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Fault,
    Image,
    Machine,
    printed_problems,
    recorded_observation,
)

ID = "F01221/F01223-F01225/F01239/F01240/F01246"
FUNCTIONS = ("F01221", "F01223", "F01224", "F01225", "F01239", "F01240", "F01246")
ENTRIES = (0x800BC2E8, 0x800BCA8C, 0x800BCFB4, 0x800BD2B8, 0x800BE53C, 0x800BE5A4, 0x800BE7B8)
# Verified against the symbol order: F01222 (0x800BC9E0) follows F01221, F01225 ends at func_800BD444,
# F01240 ends at the vector add helper and F01246 at the short-vector conversion helper.
ENDS = (0x800BC9E0, 0x800BCFB4, 0x800BD2B8, 0x800BD444, 0x800BE5A4, 0x800BE628, 0x800BE824)
ENTRY, END = ENTRIES[0], ENDS[-1]
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "event_camera_track_driver.cpp"
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
INCLUDES = (
    r"Native\Game\src",
    r"Native\Engine",
    r"Native\Engine\src",
    r"Native\Content",
)
SOURCES = (
    r"Native\Game\src\Event\EventCameraTrack.cpp",
    r"Native\Game\src\Event\EventCameraTrackLive.cpp",
    r"Native\Engine\src\Core\Log.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
)
VECTORS, MUTANT_VECTORS, SEED = 7 * 1024, 7 * 150, 121221

MASK = 0xFFFFFFFF
CAMERA = 0x80500000
CAMERA_SIZE = 0x28C
CAMERA_WORDS = CAMERA_SIZE // 4
EYE = 0xB4
CAMERA_GLOBAL, TICK_SPEED = 0x800F4BA4, 0x8005E24C
INSTRUCTION, INSTRUCTION_BYTES = 0x80600000, 12
MATRIX, DEST = 0x80520000, 0x80520100
SCRATCH, SCRATCH_SIZE = 0x1F800000, 0x400
WORLD = 0x2000
UNSET = 0x80000000
# The original's epilogue address, which it returned for operation 6.
EPILOGUE = 0x800BC9C4

# Track fields (Event_CameraTrack, Event/Camera.h).
MODE, ELAPSED, DURATION, FOLLOW_REQUEST, FOLLOWING = 0, 4, 5, 6, 7
SCALAR, SCALAR_DELTA, ORBIT_ANGLE, ORBIT_DELTA = 8, 0xC, 0x10, 0x12
FIRST_VECTOR, VECTOR_COUNT, DESTINATION, COMPUTED, CURRENT = 0x14, 10, 0x24, 0x84, 0x94
# Workspace fields (Event_CameraState).
PLACEMENT, FACING = 0x1D0, 0x1DA
ORIGIN_ACTOR, ORIGIN_OFFSET, LAST_TERRAIN = 0x1E8, 0x1EC, 0x27C
# The actor placement query's output words the native workspace leaves alone.
SKIPPED_WORDS = (PLACEMENT // 4, PLACEMENT // 4 + 1, PLACEMENT // 4 + 2)
COMPARED_WORDS = tuple(index for index in range(CAMERA_WORDS) if index not in SKIPPED_WORDS)

FLOOR, EASE, RESOLVE, PLACE, BONE = 0x8008DC7C, 0x800BFEBC, 0x800BFE50, 0x800A1108, 0x800A190C
# getShort, the vector add/subtract/scale/copy/magnitude/conversion/multiply/arctangent/direction/normalize
# helpers and the SLUS cosine, sine, RotMatrix, ratan2, SetTransMatrix, SetRotMatrix, ApplyMatrixLV,
# RotTrans, VectorNormal and square root.
FOLLOWED = (
    *ENTRIES,
    0x800BFE00, 0x800BE628, 0x800BE66C, 0x800BE6B0, 0x800BE71C, 0x800BE740, 0x800BE824, 0x800BE858,
    0x800BE878, 0x800BE89C, 0x800BE940,
    0x80040F24, 0x80040F28, 0x80041200, 0x8004137C, 0x80041578, 0x80041598, 0x800415C8, 0x800417EC,
    0x80041A94, 0x80041AF4, 0x80041B94,
)

# Call kinds shared with the driver; the bone query adds its endpoint selector.
CALL_RESOLVE, CALL_PLACEMENT, CALL_BONE, CALL_FLOOR, CALL_EASE = 20, 21, 30, 40, 50
FLOOR_EDGES = (0x8000, 0x7FFF, 0x4000, 0xC000, 0x3FFF, 0)
EASE_SALT = 0x5A5A

MODES = (2, 3, 4, 5, 8, 9, 11, 0xF, 0, 1, 6, 7, 10)
# SLUS VectorNormal's trapping adds (sum of the squared int16 whole units).
NORMALIZE_TRAPS = (0x80041B18, 0x80041B1C)
NARROW_SCALAR = 0x3FFFF

JR_RA, NOP = 0x03E00008, 0x00000000
MUTANTS = {
    "F01221-early-return": {ENTRIES[0]: JR_RA},
    # The terrain glide no longer resets the last position's x.
    "F01221-keeps-terrain-x": {0x800BC9C0: NOP},
    "F01221-partner-offset-mode-10": {0x800BC7CC: 0x2402000A},
    "F01223-early-return": {ENTRIES[1]: JR_RA},
    "F01223-no-elapsed-clamp": {0x800BCAE8: NOP},
    "F01223-never-finishes": {0x800BCF6C: NOP},
    "F01223-tick-quarter": {0x800BCAC8: 0x00031883},
    "F01224-early-return": {ENTRIES[2]: JR_RA},
    "F01224-step-cap-9": {0x800BD104: 0x24020009},
    "F01224-repeat-keeps-elapsed": {0x800BD278: NOP},
    "F01224-min-steps-3": {0x800BD080: 0x28620003},
    # slt v1, a2, v1 -> slt v1, v1, a2: climbs when the candidate is below the floor.
    "F01224-climb-inverted": {0x800BD1E4: 0x0066182A},
    "F01225-early-return": {ENTRIES[3]: JR_RA},
    "F01225-clearance-199": {0x800BD33C: 0x2610FF39},
    "F01225-probe-depth-151": {0x800BD304: 0x24160097},
    "F01225-result-shift-3": {0x800BD438: 0x000210C3},
    "F01239-early-return": {ENTRIES[4]: JR_RA},
    "F01239-divisor-shift-5": {0x800BE588: 0x00031943},
    "F01239-keeps-duration": {0x800BE590: NOP},
    "F01240-early-return": {ENTRIES[5]: JR_RA},
    "F01240-mean-quarter": {0x800BE5F0: 0x00021083},
    "F01240-both-selector-3": {0x800BE5BC: 0x24020003},
    "F01240-always-look-at": {0x800BE600: 0x10000002},
    "F01246-early-return": {ENTRIES[6]: JR_RA},
    "F01246-shift-5": {0x800BE7E0: 0x00021143},
    "F01246-returns-operands": {0x800BE804: 0x02001021},
    "F01246-keeps-z": {0x800BE80C: NOP},
}


@dataclass(frozen=True)
class Vector:
    function: int
    odd: int
    eye: int
    repeat: int
    tick: int
    selector: int
    seed: int
    floor_mode: int
    floor_base: int
    ease_mode: int
    resolved: int
    placement_present: int
    placement: tuple[int, int, int, int]
    bone_present: int
    bone: tuple[int, int, int]
    instruction: tuple[int, ...]
    matrix: tuple[int, ...]
    dest: tuple[int, int, int, int]
    block: tuple[int, ...]
    narrowed: int = 0

    def inputs(self) -> list[int]:
        return [
            self.function,
            self.odd,
            self.eye,
            self.tick & MASK,
            self.selector & MASK,
            self.seed,
            self.floor_mode,
            self.floor_base & MASK,
            self.ease_mode,
            self.resolved & MASK,
            self.placement_present,
            *(value & 0xFFFF for value in self.placement),
            self.bone_present,
            *(value & 0xFFFF for value in self.bone),
            *self.instruction,
            *self.matrix,
            *self.dest,
            *self.block,
        ]


def mix(x: int, z: int, seed: int) -> int:
    """The stubs' shared hash; the driver's Mix."""
    value = (x * 0x9E3779B1 + z * 0x85EBCA77 + seed) & MASK
    value ^= value >> 15
    value = (value * 0x2C1B3C6D) & MASK
    return value ^ value >> 12


def floor_height(value: Vector, x: int, z: int) -> int:
    """The floor stub's return word: the height in the low half and hash bits above it."""
    hashed = mix(x & MASK, z & MASK, value.seed)
    if value.floor_mode == 0:
        height = value.floor_base
    elif value.floor_mode == 1:
        height = value.floor_base + (hashed & 0xFF) - 0x80
    elif value.floor_mode == 2:
        height = hashed
    else:
        height = FLOOR_EDGES[hashed % len(FLOOR_EDGES)]
    return (hashed >> 16 << 16 | height & 0xFFFF) & MASK


def ease(value: Vector, curve: int, frame: int, duration: int) -> int:
    hashed = mix(curve | frame << 8 | duration << 16, EASE_SALT, value.seed)
    if value.ease_mode == 0:
        return 4096 if duration == 0 else frame * 4096 // duration
    if value.ease_mode == 1:
        return hashed & 0x1FFF
    if value.ease_mode == 2:
        return hashed
    return (0, 2048, 4096)[hashed % 3]


def _put_byte(block: list[int], offset: int, value: int) -> None:
    index, shift = offset // 4, (offset % 4) * 8
    block[index] = (block[index] & ~(0xFF << shift) | (value & 0xFF) << shift) & MASK


def _put_half(block: list[int], offset: int, value: int) -> None:
    index, shift = offset // 4, (offset % 4) * 8
    block[index] = (block[index] & ~(0xFFFF << shift) | (value & 0xFFFF) << shift) & MASK


def _put_vector(block: list[int], offset: int, words) -> None:
    for axis, word in enumerate(words):
        block[offset // 4 + axis] = word & MASK


def _frames(rng: random.Random) -> tuple[int, int]:
    pick = rng.randrange(6)
    if pick == 0:
        return rng.randrange(41), rng.randrange(41)
    if pick == 1:
        frames = rng.randrange(41)
        return frames, frames
    if pick == 2:
        return rng.randrange(41), 0
    if pick == 3:
        return rng.randrange(1, 41), rng.randrange(256)
    if pick == 4:
        return rng.getrandbits(8), rng.getrandbits(8)
    return 0, rng.randrange(1, 41)


def _scalar(rng: random.Random) -> int:
    return rng.choice((0, 1, rng.randrange(1, 0x2000), -rng.randrange(1, 0x40000),
                       rng.randrange(0x1000, 0x100000), rng.randrange(0x1000, 0x100000), rng.getrandbits(32),
                       -1)) & MASK


def _spread(rng: random.Random) -> int:
    return rng.choice((0, 0x1000, 0x10000, 0x100000, 0x800000))


def _positions(rng: random.Random, wide: bool) -> list[tuple[int, int, int]]:
    """Ten track vectors around one centre, some equal, some full words when `wide`."""
    center = [rng.randrange(-0x800000, 0x800000) for _ in range(3)]
    vectors: list[tuple[int, int, int]] = []
    for _ in range(VECTOR_COUNT):
        if wide and rng.randrange(3) == 0:
            vectors.append(tuple(rng.getrandbits(32) for _ in range(3)))
        elif vectors and rng.randrange(6) == 0:
            vectors.append(rng.choice(vectors))
        else:
            spread = _spread(rng)
            vectors.append(tuple((axis + rng.randint(-spread, spread)) & MASK for axis in center))
    return vectors


def _signed(word: int) -> int:
    return word - (1 << 32) if word & 0x80000000 else word


def _squared_units(words) -> int:
    """The magnitude helper's wrapped sum of squared whole units, as the signed word its square root reads."""
    return _signed(sum((_signed(word) >> 12) ** 2 for word in words) & MASK)


def _bound_distance(block: list[int], base: int) -> None:
    """Keep the duration distance's wrapped sum of squares non-negative.

    The SLUS square root indexes before its table for a negative word; Core::Math::SquareRootInteger defines
    the root of a negative word as zero instead (tested natively), so those distances are outside the
    compared domain. Positive wrapped sums are kept.
    """
    current = (base + CURRENT) // 4
    destination = (base + DESTINATION) // 4
    for axis in range(3):
        difference = [
            (block[current + index] - block[destination + index]) & MASK for index in range(3)
        ]
        if _squared_units(difference) >= 0:
            return
        block[destination + axis] = block[current + axis]


def _shape_track(rng: random.Random, block: list[int], base: int, mode: int, wide: bool) -> None:
    _put_byte(block, base + MODE, mode)
    elapsed, duration = _frames(rng)
    _put_byte(block, base + ELAPSED, elapsed)
    _put_byte(block, base + DURATION, duration)
    _put_byte(block, base + FOLLOW_REQUEST, rng.choice((0, 0, 1, rng.getrandbits(8))))
    _put_byte(block, base + FOLLOWING, rng.choice((0, 1, 1, rng.getrandbits(8))))
    block[(base + SCALAR) // 4] = _scalar(rng)
    block[(base + SCALAR_DELTA) // 4] = rng.choice((rng.getrandbits(8), rng.getrandbits(32), _scalar(rng)))
    _put_half(block, base + ORBIT_ANGLE, rng.getrandbits(16))
    _put_half(block, base + ORBIT_DELTA, rng.getrandbits(16))
    for index, words in enumerate(_positions(rng, wide)):
        _put_vector(block, base + FIRST_VECTOR + index * 16, words)


def _instruction(rng: random.Random, lane: int, eye: int) -> list[int]:
    operation = lane % 16 if lane < 64 or rng.randrange(2) else rng.choice((*range(12), 6, 12, 15))
    opcode = rng.choice((0xC0 | eye << 4, rng.getrandbits(8) & 0xF0)) | operation
    operands = [rng.getrandbits(8) for _ in range(INSTRUCTION_BYTES - 1)]
    if operation == 4:
        operands[0] = rng.getrandbits(4) << 4 | rng.choice((0xF, 1, 0, rng.getrandbits(4)))
    elif operation == 5:
        height = rng.randrange(-0x800, 0x800) & 0xFFFF
        operands[2], operands[3] = height & 0xFF, height >> 8
    elif operation == 11:
        operands[1] = rng.choice((0, 1, rng.getrandbits(8)))
    return [opcode, *operands]


def _matrix(rng: random.Random) -> list[int]:
    halves = [rng.choice((rng.getrandbits(16), rng.randrange(-0x1000, 0x1001) & 0xFFFF)) for _ in range(10)]
    rotation = [halves[index] | halves[index + 1] << 16 for index in range(0, 10, 2)]
    translation = [rng.choice((rng.randrange(-0x40000000, 0x40000000), rng.randrange(-0x800, 0x800), 0)) & MASK
                   for _ in range(3)]
    return rotation + translation


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % len(FUNCTIONS), number // len(FUNCTIONS)
    eye = rng.getrandbits(1)
    block = [rng.getrandbits(32) for _ in range(CAMERA_WORDS)]
    wide = function in (4, 5)
    modes = [rng.choice(MODES) for _ in range(2)]
    if function == 1:
        modes[eye] = MODES[lane % len(MODES)] if lane < 4 * len(MODES) else rng.choice((*MODES, 11, 11))
    for index, base in enumerate((0, EYE)):
        _shape_track(rng, block, base, modes[index], wide)
        if wide:
            _bound_distance(block, base)
    _put_half(block, ORIGIN_ACTOR, rng.choice((WORLD, WORLD, rng.getrandbits(5), rng.getrandbits(16))))
    _put_vector(block, ORIGIN_OFFSET, [rng.randrange(-0x800000, 0x800000) for _ in range(3)])
    track = EYE if eye else 0
    computed = block[(track + COMPUTED) // 4:(track + COMPUTED) // 4 + 3]
    last = rng.randrange(4)
    if last == 1:
        _put_vector(block, LAST_TERRAIN, (UNSET, UNSET, UNSET))
    elif last == 2:
        _put_vector(block, LAST_TERRAIN, computed)
    repeat = int((function == 2 or (function == 1 and modes[eye] == 11)) and last == 3
                 and rng.randrange(2) == 0)
    if function in (1, 2) and rng.randrange(8) == 0:
        # A zero step returns to the computed position.
        block[(track + SCALAR) // 4] = 0
        _put_vector(block, LAST_TERRAIN, computed)
    placement_present = int(rng.randrange(5) != 0)
    if not placement_present:
        # The native facing of an empty slot is zero; the original read this stale halfword.
        _put_half(block, FACING, 0)
    floor_mode = rng.randrange(4)
    return Vector(
        function=function,
        odd=rng.getrandbits(1),
        eye=eye,
        repeat=repeat,
        tick=rng.choice((2, 2, 4, 0, 1, 3, -2, 0x1FE, rng.getrandbits(32))) & MASK,
        selector=rng.choice((0, 1, 2, 3, -1, 0x10002, 0x20000, 0xFFFF0002, 0xFFFF0000,
                             rng.getrandbits(32))) & MASK if lane >= 3 else lane,
        seed=rng.getrandbits(32),
        floor_mode=floor_mode,
        floor_base=rng.randrange(-0x400, 0x400),
        ease_mode=rng.randrange(4),
        resolved=rng.choice((rng.getrandbits(5), 0x1000 | rng.getrandbits(5), rng.getrandbits(32), MASK)),
        placement_present=placement_present,
        placement=tuple(rng.getrandbits(16) for _ in range(4)),
        bone_present=int(rng.randrange(5) != 0),
        bone=tuple(rng.getrandbits(16) for _ in range(3)),
        instruction=tuple(_instruction(rng, lane, eye)),
        matrix=tuple(_matrix(rng)),
        dest=tuple(rng.getrandbits(32) for _ in range(4)),
        block=tuple(block),
    )


def _install_stubs(machine: Machine, value: Vector, calls: list[tuple[int, int, int]]) -> None:
    def floor(m: Machine) -> None:
        calls.append((CALL_FLOOR, m.r[4], m.r[5]))
        m.r[2] = floor_height(value, m.r[4], m.r[5])

    def curve(m: Machine) -> None:
        mode, frame, duration = (m.r[register] & 0xFFFF for register in (4, 5, 6))
        calls.append((CALL_EASE, mode, frame | duration << 16))
        m.r[2] = ease(value, mode, frame, duration) & MASK

    def resolve(m: Machine) -> None:
        calls.append((CALL_RESOLVE, m.r[4] & 0xFFFF, 0))
        m.r[2] = value.resolved & MASK

    def place(m: Machine) -> None:
        calls.append((CALL_PLACEMENT, m.r[4], 0))
        if not value.placement_present:
            m.r[2] = MASK
            return
        out = m.r[5]
        m.put(out, 0x01020304)
        for axis, half in enumerate(value.placement):
            m.put(out + 4 + axis * 2, half, 2)
        m.r[2] = 0

    def bone(m: Machine) -> None:
        calls.append((CALL_BONE + m.r[7], m.r[4], m.r[5]))
        if not value.bone_present:
            m.r[2] = MASK
            return
        for axis, half in enumerate(value.bone):
            m.put(m.r[6] + axis * 2, half, 2)
        m.r[2] = 0

    machine.stub(FLOOR, floor)
    machine.stub(EASE, curve)
    machine.stub(RESOLVE, resolve)
    machine.stub(PLACE, place)
    machine.stub(BONE, bone)
    machine.follow.update(FOLLOWED)


def _execute(machine: Machine, value: Vector, calls: list[tuple[int, int, int]]) -> None:
    for index, word in enumerate(value.block):
        machine.put(CAMERA + index * 4, word)
    machine.put(CAMERA_GLOBAL, CAMERA)
    machine.put(TICK_SPEED, value.tick)
    machine.map_zero(SCRATCH, SCRATCH_SIZE)
    machine.map_zero(INSTRUCTION, 16)
    machine.store(INSTRUCTION + value.odd, bytes(value.instruction))
    for index, word in enumerate(value.matrix):
        machine.put(MATRIX + index * 4, word)
    for index, word in enumerate(value.dest):
        machine.put(DEST + index * 4, word)
    _install_stubs(machine, value, calls)
    machine.stores.clear()
    track = CAMERA + (EYE if value.eye else 0)
    other = CAMERA + (0 if value.eye else EYE)
    registers = (
        (INSTRUCTION + value.odd, 0),
        (track, other),
        (track, 0),
        (MATRIX, 0),
        (track, 0),
        (value.selector & MASK, 0),
        (DEST, INSTRUCTION + value.odd),
    )[value.function]
    machine.r[4:6] = list(registers)
    machine.execute(ENTRIES[value.function], limit=400000)


_DOMAIN: dict["Vector", "Vector"] = {}


def _narrowed(value: Vector) -> Vector:
    """The vector with a bounded easing progress and step length."""
    index = ((EYE if value.eye else 0) + SCALAR) // 4
    block = list(value.block)
    block[index] &= NARROW_SCALAR
    return replace(value, ease_mode=1 if value.ease_mode == 2 else value.ease_mode, block=tuple(block),
                   narrowed=1)


def _in_domain(images, value: Vector) -> Vector:
    """For the track update and the terrain step, the vector the unpatched original runs without trapping.

    SLUS VectorNormal sums the squared int16 whole units with trapping adds, so a direction whose squares
    reach 2^31 raises an overflow exception on the console. Core::Math::NormalizeVector reads that length as
    unsigned (tested natively), so such lanes are narrowed instead of compared. A repeat lane also gets its
    last terrain position where the original moves the track, so the glide finds it has not moved. Cached so
    the mutant runs use the vector the reference and the native driver used.
    """
    if value.function not in (1, 2):
        return value
    cached = _DOMAIN.get(value)
    if cached is not None:
        return cached
    adjusted = value
    while True:
        probe = Machine(images)
        try:
            _execute(probe, adjusted, [])
            break
        except Fault as error:
            if adjusted.narrowed or not any(f"{address:#x}" in str(error) for address in NORMALIZE_TRAPS):
                raise
            adjusted = _narrowed(adjusted)
    if adjusted.repeat:
        computed = CAMERA + (EYE if value.eye else 0) + COMPUTED
        block = list(adjusted.block)
        _put_vector(block, LAST_TERRAIN, [probe.load(computed + axis * 4) for axis in range(3)])
        adjusted = replace(adjusted, block=tuple(block))
    _DOMAIN[value] = adjusted
    return adjusted


def run(machine: Machine, value: Vector) -> tuple[dict, list[int]]:
    value = _in_domain(machine.images, value)
    calls: list[tuple[int, int, int]] = []
    _execute(machine, value, calls)
    result = 0
    if value.function == 0:
        result = machine.r[2]
        if value.instruction[0] & 0xF == 6 and result == EPILOGUE:
            result = MASK
    elif value.function == 3:
        result = machine.r[2]
    elif value.function == 6:
        result = 1 if machine.r[2] == DEST else 2
    observed = [
        result,
        *(machine.load(CAMERA + index * 4) for index in COMPARED_WORDS),
        *(machine.load(DEST + index * 4) for index in range(4)),
    ]
    names = [
        "return",
        *(f"camera-word-{index * 4:#x}" for index in COMPARED_WORDS),
        *(f"dest-word-{index * 4:#x}" for index in range(4)),
    ]
    return recorded_observation(machine, observed, names, calls), value.inputs()


def check(
    image: Image, value: Vector, observation: dict, native: list[int], reference: dict
) -> list[str]:
    del image, value, reference
    allowed = [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (CAMERA, CAMERA + CAMERA_SIZE),
        (SCRATCH, SCRATCH + SCRATCH_SIZE),
        (DEST, DEST + 16),
    ]
    return printed_problems(observation, native, allowed)


def describe(image: Image, results: list) -> list[str]:
    by_function = [0] * len(FUNCTIONS)
    operations = [0] * 16
    modes: dict[int, int] = {}
    repeats = finished = narrowed = 0
    for value, observation, _ in results:
        by_function[value.function] += 1
        narrowed += _DOMAIN.get(value, value).narrowed
        track = EYE if value.eye else 0
        after = dict(zip(observation["names"], observation["values"], strict=False))
        if value.function == 0:
            operations[value.instruction[0] & 0xF] += 1
        elif value.function == 1:
            mode = value.block[track // 4] & 0xFF
            modes[mode] = modes.get(mode, 0) + 1
        if value.function in (1, 2):
            computed = (track + COMPUTED) // 4
            repeats += value.repeat and all(
                after[f"camera-word-{(computed + axis) * 4:#x}"] == value.block[computed + axis]
                for axis in range(3)
            )
            elapsed_word = after[f"camera-word-{track + ELAPSED:#x}"]
            finished += (elapsed_word & 0xFF) == (elapsed_word >> 8 & 0xFF)
    ranges = ", ".join(
        f"{name} {entry:08X}-{end - 4:08X} ({(end - entry) // 4} words, "
        f"sha256={image.sha256(entry, end)[:16]})"
        for name, entry, end in zip(FUNCTIONS, ENTRIES, ENDS, strict=True)
    )
    return [
        "Functions: " + ranges,
        "Vectors per function: "
        + ", ".join(f"{name}={count}" for name, count in zip(FUNCTIONS, by_function, strict=True)),
        "F01221 operations: " + ", ".join(f"{op}={count}" for op, count in enumerate(operations)),
        "F01223 modes: " + ", ".join(f"{mode:#x}={count}" for mode, count in sorted(modes.items())),
        f"F01223/F01224 terrain glide repeats onto the last start: {repeats}; "
        f"tracks left with elapsed == duration: {finished}",
        "Duration distances whose wrapped sum of squared units is negative are outside the compared domain "
        "(the SLUS square root reads before its table there; the native root is zero).",
        f"Track lanes narrowed to a bounded easing progress and step length because the original's "
        f"VectorNormal overflow trap fired (outside the compared domain; the native normalize is defined): "
        f"{narrowed}",
        "Compared the opcode result (operation 6's epilogue address read as the native -1), every workspace "
        "word except the actor placement query's output, the destination vector, the floor, easing, "
        "reference, placement and bone calls in order with their arguments, and the original's stores.",
    ]
