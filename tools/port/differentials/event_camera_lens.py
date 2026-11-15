"""F01232-F01235 event camera lens tweens and shake against original BATTLE.PRG."""

import random
from dataclasses import dataclass

from differential import (
    DEFAULT_STACK,
    Image,
    Machine,
    printed_problems,
    recorded_observation,
)

ID = "F01232-F01235"
FUNCTIONS = ("F01232", "F01233", "F01234", "F01235")
ENTRIES = (0x800BDC9C, 0x800BDF6C, 0x800BE01C, 0x800BE180)
# F01235 ends where func_800BE36C starts.
ENDS = (0x800BDF6C, 0x800BE01C, 0x800BE180, 0x800BE36C)
ENTRY, END = ENTRIES[0], ENDS[-1]
MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "event_camera_lens_driver.cpp"
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
INCLUDES = (
    r"Native\Game\src",
    r"Native\Engine",
    r"Native\Engine\src",
    r"Native\Content",
)
SOURCES = (
    r"Native\Game\src\Event\EventCameraLens.cpp",
    r"Native\Game\src\Event\EventCameraLensLive.cpp",
    r"Native\Engine\src\Core\Log.cpp",
)
VECTORS, MUTANT_VECTORS, SEED = 4800, 1200, 121232

MASK = 0xFFFFFFFF
CAMERA = 0x80500000
CAMERA_SIZE = 0x28C
CAMERA_WORDS = CAMERA_SIZE // 4
CAMERA_GLOBAL = 0x800F4BA4
TICK_SPEED = 0x8005E24C
SCRATCH = 0x1F800000
ENVELOPE_SCRATCH = 0x88
# cameraShakeScratch's eye, look-at and axis vectors.
SHAKE_SCRATCH, SHAKE_SCRATCH_WORDS = 0xD8, 6
INSTRUCTION = 0x80600000
LENS_TWEENS = (0x180, 0x18C, 0x198)
OSCILLATORS = (0x1A6, 0x1B2)
EASE_COUNT, SINE_COUNT = 1, 4
OPCODES = (0xEA, 0xEB, 0xEC, 0xEF)

EASE, SINE = 0x800BFEBC, 0x80040F28
# Call kinds shared with the driver.
CALL_EASE, CALL_SINE = 1, 2

OBSERVED_NAMES = (
    "return",
    "scratch-envelope",
    *(f"scratch-shake-word-{SHAKE_SCRATCH + index * 4:#x}" for index in range(SHAKE_SCRATCH_WORDS)),
    *(f"camera-word-{index * 4:#x}" for index in range(CAMERA_WORDS)),
)
# The original may store only to its stack, the workspace and the two scratch spans.
ALLOWED_STORES = (
    (DEFAULT_STACK - 0x2000, DEFAULT_STACK + 32),
    (CAMERA, CAMERA + CAMERA_SIZE),
    (SCRATCH + ENVELOPE_SCRATCH, SCRATCH + ENVELOPE_SCRATCH + 4),
    (SCRATCH + SHAKE_SCRATCH, SCRATCH + SHAKE_SCRATCH + SHAKE_SCRATCH_WORDS * 4),
)

NOP = 0x00000000
MUTANTS = {
    "F01232-projection-shift-3": {0x800BDD18: 0x000210C0},
    "F01232-near-default-0x41": {0x800BDDBC: 0x24040041},
    "F01232-far-shift-6": {0x800BDE60: 0x00021180},
    "F01232-elapsed-kept": {0x800BDF2C: NOP},
    "F01232-first-slot-only": {0x800BDF58: 0x28420001},
    "F01233-quarter-tick": {0x800BDF94: 0x00031883},
    "F01233-step-shift-11": {0x800BDFF0: 0x000722C3},
    "F01233-duration-kept": {0x800BE000: NOP},
    "F01234-phase-shift-11": {0x800BE038: 0x00028AC0},
    "F01234-flat-envelope-0x800": {0x800BE090: 0x24020800},
    "F01234-fade-out-offset-0x200": {0x800BE0CC: 0x24840200},
    "F01234-displacement-shift-11": {0x800BE158: 0x000612C3},
    "F01235-x-axis-shift-3": {0x800BE1F4: 0x000210C2},
    "F01235-look-at-uses-eye-bit": {0x800BE288: 0x30420001},
    "F01235-count-not-stored": {0x800BE2F8: NOP},
}


@dataclass(frozen=True)
class Vector:
    function: int
    odd: int
    target: int
    tick_speed: int
    scratch: tuple[int, ...]
    eases: tuple[int, ...]
    sines: tuple[int, ...]
    instruction: tuple[int, ...]
    block: tuple[int, ...]

    def inputs(self) -> list[int]:
        return [
            self.function,
            self.odd,
            self.target,
            self.tick_speed & MASK,
            *self.scratch,
            *(value & MASK for value in self.eases),
            *(value & MASK for value in self.sines),
            *self.instruction,
            *self.block,
        ]


def _put(block: list[int], offset: int, value: int, size: int) -> None:
    for index in range(size):
        word, shift = (offset + index) // 4, ((offset + index) % 4) * 8
        byte = (value >> (index * 8)) & 0xFF
        block[word] = (block[word] & ~(0xFF << shift) | byte << shift) & MASK


def _byte(rng: random.Random, *common: int) -> int:
    return rng.choice((*common, rng.getrandbits(8), rng.getrandbits(8)))


def _tween(rng: random.Random, block: list[int], offset: int) -> None:
    _put(block, offset + 1, _byte(rng, 0, 1, 2, 3), 1)
    _put(block, offset + 8, _byte(rng, 0, 0xFE, 0xFF), 1)
    _put(block, offset + 9, _byte(rng, 0, 0, 1, 2, 0xFF), 1)


def _oscillator(rng: random.Random, block: list[int], offset: int) -> None:
    _put(block, offset + 1, _byte(rng, 0, 1, 2, 3, 4), 1)
    _put(block, offset + 4, rng.choice((0, 1, 8, 0x7FFF, 0x8000, rng.getrandbits(16))), 2)
    _put(block, offset + 6, rng.choice((0, 0x100, 0x7FFF, 0x8000, rng.getrandbits(16))), 2)
    _put(block, offset + 8, _byte(rng, 0, 0xFE, 0xFF), 1)
    _put(block, offset + 9, _byte(rng, 0, 0, 1, 2, 0xFF), 1)


def _sine(rng: random.Random) -> int:
    return rng.choice((0, 0x1000, -0x1000, rng.randrange(-0x1000, 0x1001),
                       rng.getrandbits(32)))


def vector(rng: random.Random, number: int) -> Vector:
    function = number % len(FUNCTIONS)
    block = [rng.getrandbits(32) for _ in range(CAMERA_WORDS)]
    for offset in LENS_TWEENS:
        _tween(rng, block, offset)
    for offset in OSCILLATORS:
        _oscillator(rng, block, offset)
    target = 0
    if function == 1:
        target = rng.randrange(len(LENS_TWEENS))
    elif function == 2:
        target = rng.randrange(len(OSCILLATORS))
    opcode = rng.choice((*OPCODES, *OPCODES, 0xEE, rng.getrandbits(8)))
    instruction = [opcode, *(_byte(rng, 0, 0, 1, 0xFF) for _ in range(5))]
    return Vector(
        function=function,
        odd=rng.randrange(4),
        target=target,
        tick_speed=rng.choice((2, 2, 4, 4, 0, 1, 0x1FF, -2, rng.getrandbits(32))),
        scratch=tuple(rng.getrandbits(32) for _ in range(1 + SHAKE_SCRATCH_WORDS)),
        eases=tuple(rng.choice((0, 0x800, 0x1000, rng.randrange(-0x2000, 0x2000),
                                rng.getrandbits(32))) for _ in range(EASE_COUNT)),
        sines=tuple(_sine(rng) for _ in range(SINE_COUNT)),
        instruction=tuple(instruction),
        block=tuple(block),
    )


def _install_stubs(machine: Machine, value: Vector, calls: list[list[int]]) -> None:
    def ease(m: Machine) -> None:
        index = sum(call[0] == CALL_EASE for call in calls)
        calls.append([CALL_EASE, m.r[4], m.r[5], m.r[6]])
        m.r[2] = value.eases[index] & MASK if index < EASE_COUNT else 0

    def sine(m: Machine) -> None:
        index = sum(call[0] == CALL_SINE for call in calls)
        calls.append([CALL_SINE, m.r[4], 0, 0])
        m.r[2] = value.sines[index] & MASK if index < SINE_COUNT else 0

    machine.stub(EASE, ease)
    machine.stub(SINE, sine)
    machine.follow.add(ENTRIES[2])


def run(machine: Machine, value: Vector) -> tuple[dict, list[int]]:
    for index, word in enumerate(value.block):
        machine.put(CAMERA + index * 4, word)
    machine.put(CAMERA_GLOBAL, CAMERA)
    machine.put(TICK_SPEED, value.tick_speed & MASK)
    machine.map_zero(SCRATCH, 0x400)
    machine.put(SCRATCH + ENVELOPE_SCRATCH, value.scratch[0])
    for index, word in enumerate(value.scratch[1:]):
        machine.put(SCRATCH + SHAKE_SCRATCH + index * 4, word)
    machine.map_zero(INSTRUCTION, 16)
    machine.store(INSTRUCTION + value.odd, bytes(value.instruction))
    calls: list[list[int]] = []
    _install_stubs(machine, value, calls)
    machine.stores.clear()

    if value.function == 0:
        machine.r[4:6] = [INSTRUCTION + value.odd, 0]
    elif value.function == 1:
        machine.r[4] = CAMERA + LENS_TWEENS[value.target]
    elif value.function == 2:
        machine.r[4] = CAMERA + OSCILLATORS[value.target]
    machine.execute(ENTRIES[value.function], limit=10000)

    observed = [
        machine.r[2] if value.function in (0, 2) else 0,
        machine.load(SCRATCH + ENVELOPE_SCRATCH),
        *(machine.load(SCRATCH + SHAKE_SCRATCH + index * 4) for index in range(SHAKE_SCRATCH_WORDS)),
        *(machine.load(CAMERA + index * 4) for index in range(CAMERA_WORDS)),
    ]
    return recorded_observation(machine, observed, list(OBSERVED_NAMES), calls), value.inputs()


def check(image: Image, value: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, value, reference
    return printed_problems(observation, native, list(ALLOWED_STORES))


def describe(image: Image, results: list) -> list[str]:
    by_function = [0] * len(FUNCTIONS)
    served = unserved = active_lens = active_shake = shake_calls = 0
    for value, observation, _ in results:
        by_function[value.function] += 1
        calls = observation["calls"]
        if value.function == 0:
            if value.instruction[0] in OPCODES:
                served += 1
            else:
                unserved += 1
        elif value.function == 1:
            active_lens += bool(calls)
        elif value.function == 2:
            active_shake += bool(calls)
        else:
            shake_calls += len(calls)
    ranges = ", ".join(
        f"{name} {entry:08X}-{end - 4:08X} ({(end - entry) // 4} words, "
        f"sha256={image.sha256(entry, end)[:16]})"
        for name, entry, end in zip(FUNCTIONS, ENTRIES, ENDS, strict=True)
    )
    return [
        "Functions: " + ranges,
        "Vectors per function: "
        + ", ".join(f"{name}={count}" for name, count in zip(FUNCTIONS, by_function, strict=True)),
        f"F01232 served opcodes {served}, fall-through opcodes {unserved}; active F01233 tweens "
        f"{active_lens}; active F01234 oscillators {active_shake}; F01235 sine calls {shake_calls}.",
        "Compared the opcode or oscillator result, the shared envelope scratch word, the shake "
        "scratch vectors, every workspace word, and the ease and sine calls with their arguments.",
    ]
