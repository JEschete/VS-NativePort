"""F01279-F01284/F01297/F01312 sphere lifecycle against original BATTLE.PRG."""

import random
from dataclasses import dataclass

from differential import DEFAULT_STACK, Image, Machine, store_problems

ID = "F01279-F01284/F01297/F01312"
FUNCTIONS = (
    "F01279",
    "F01280",
    "F01281",
    "F01282",
    "F01283",
    "F01284",
    "F01297",
    "F01312",
)
ENTRIES = (
    0x800C031C,
    0x800C05B4,
    0x800C05EC,
    0x800C06E0,
    0x800C0700,
    0x800C0738,
    0x800C16DC,
    0x800C4734,
)
ENDS = (
    0x800C05B4,
    0x800C05EC,
    0x800C06E0,
    0x800C0700,
    0x800C0738,
    0x800C0758,
    0x800C16FC,
    0x800C4794,
)
ENTRY, END = ENTRIES[0], ENDS[-1]
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "sphere_lifecycle_driver.cpp"
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
INCLUDES = (
    r"Native\Game\src",
    r"Native\Engine",
    r"Native\Engine\src",
    r"Native\Content",
)
SOURCES = (
    r"Native\Game\src\Battle\SphereLifecycle.cpp",
    r"Native\Game\src\Battle\SphereLifecycleLive.cpp",
    r"Native\Game\src\Battle\SphereReleaseLive.cpp",
    r"Native\Engine\src\Core\Log.cpp",
    r"Native\Engine\src\Core\Memory\Copy.cpp",
    r"Native\Engine\src\Core\Memory\CopyLive.cpp",
    r"Native\Engine\src\Core\Memory\WordCopyLive.cpp",
)
VECTORS, MUTANT_VECTORS, SEED = 8192, 1024, 120279

SPHERE = 0x80500000
SPHERE_SIZE = 0x4B1C
SPHERE_GLOBAL = 0x800EB9B8
REUSE_GLOBAL = 0x800EB9AC
BUTTONS_GLOBAL = 0x8005E238
PRIMARY, SECONDARY, PREVIEWS = 0x80600000, 0x80600100, 0x80600200
WIRE_EDGES = 0x44B4
WIRE_EDGE_CAPACITY = 0x302
ALLOCATE, FREE, ZERO = 0x80043EC4, 0x80043D50, 0x800CCC1C

# Returning at each function's first instruction removes its state transition.
MUTANTS = {
    f"{function}-early-return": {entry: 0x03E00008}
    for function, entry in zip(FUNCTIONS, ENTRIES, strict=True)
}


@dataclass(frozen=True)
class Vector:
    function: int
    sphere_exists: int
    reuse: int
    secondary_present: int
    preview_argument_present: int
    preview_already_bound: int
    sphere_mode: int
    buttons: int
    phase_a: int
    phase_b: int
    phase_c: int
    wire_shape: int
    target_count: int
    selected_target: int
    initial_mode: int
    reserved_38: int
    reserved_3c: int
    primary_words: tuple[int, int, int, int]
    secondary_words: tuple[int, int, int, int]

    def inputs(self) -> list[int]:
        return [
            self.function,
            self.sphere_exists,
            self.reuse,
            self.secondary_present,
            self.preview_argument_present,
            self.preview_already_bound,
            self.sphere_mode,
            self.buttons,
            self.phase_a,
            self.phase_b,
            self.phase_c,
            self.wire_shape,
            self.target_count,
            self.selected_target,
            self.initial_mode,
            self.reserved_38,
            self.reserved_3c,
            *self.primary_words,
            *self.secondary_words,
        ]


def _volume_words(rng: random.Random, kind: int) -> tuple[int, int, int, int]:
    words = [rng.getrandbits(32) for _ in range(4)]
    words[0] = (words[0] & ~0xFF) | kind
    return tuple(words)


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % len(FUNCTIONS), number // len(FUNCTIONS)
    if lane < 32:
        primary_kind = (1, 0, 2, 0x80, 0xFF)[lane % 5]
        secondary_kind = (0, 1, 0x7F, 0x80, 0xFF)[(lane // 2) % 5]
    else:
        primary_kind = rng.choice((0, 1, 2, 0x7F, 0x80, 0xFF))
        secondary_kind = rng.choice((0, 1, 2, 0x7F, 0x80, 0xFF))

    sphere_exists = int(rng.getrandbits(1) != 0)
    reuse = 0
    secondary_present = int(rng.getrandbits(1) != 0)
    if function == 2:
        sphere_exists = 1
        reuse = int(lane % 3 == 0 or rng.getrandbits(1) != 0)
        if reuse:
            secondary_present = 1
    if function == 4:
        sphere_exists = 1
        secondary_present = 1

    if function == 7 and lane < 16:
        wire_shape = (0, 1, 0, 15)[lane % 4]
        input_locked = (0, 1, 0x8000, 0xFFFF)[(lane // 4) % 4]
        buttons = (0, 0x80, 0x100, 0xFFFFFFFF)[(lane // 2) % 4]
        reserved_3c = rng.getrandbits(16) | input_locked << 16
    else:
        wire_shape = rng.choice((0, 1, 15, 0x7FFF, 0x8000, 0xFFFF))
        reserved_3c = rng.getrandbits(32)
        buttons = rng.choice((0, 0x80, 0x100, 0xFFFFFFFF))

    return Vector(
        function=function,
        sphere_exists=sphere_exists,
        reuse=reuse,
        secondary_present=secondary_present,
        preview_argument_present=int(rng.getrandbits(1) != 0),
        preview_already_bound=int(rng.getrandbits(1) != 0),
        sphere_mode=rng.getrandbits(8),
        buttons=buttons,
        phase_a=rng.choice((0, 1, 0x7FFF, 0x8000, 0xFFFF)),
        phase_b=rng.choice((0, 1, 0x7FFF, 0x8000, 0xFFFF)),
        phase_c=rng.choice((0, 1, 0x7FFF, 0x8000, 0xFFFF)),
        wire_shape=wire_shape,
        target_count=rng.getrandbits(8),
        selected_target=rng.getrandbits(8),
        initial_mode=rng.getrandbits(8),
        reserved_38=rng.getrandbits(32),
        reserved_3c=reserved_3c,
        primary_words=_volume_words(rng, primary_kind),
        secondary_words=_volume_words(rng, secondary_kind),
    )


def _put_words(machine: Machine, address: int, words: tuple[int, ...]) -> None:
    for index, value in enumerate(words):
        machine.put(address + index * 4, value)


def _state_values(machine: Machine, function: int) -> tuple[list[int], list[str]]:
    primary = [machine.load(SPHERE + offset, 4) for offset in range(0, 16, 4)]
    secondary = [machine.load(SPHERE + 0x10 + offset, 4) for offset in range(0, 16, 4)]
    values = [
        int(machine.load(SPHERE_GLOBAL, 4) != 0),
        machine.load(REUSE_GLOBAL, 1),
        *primary,
        *secondary,
        machine.load(SPHERE + 0x24, 2),
        machine.load(SPHERE + 0x26, 2),
        machine.load(SPHERE + 0x28, 2),
        machine.load(SPHERE + 0x2A, 2),
        machine.load(SPHERE + 0x2C, 1),
        machine.load(SPHERE + 0x2D, 1),
        machine.load(SPHERE + 0x2E, 1),
        machine.load(SPHERE + 0x38, 4),
        machine.load(SPHERE + 0x3C, 4),
        int(machine.load(SPHERE + 0x40, 4) == PREVIEWS),
        int(machine.load(SPHERE + 0x44, 4) == PREVIEWS),
    ]
    names = [
        "sphere-active",
        "reuse",
        *(f"primary-word-{index}" for index in range(4)),
        *(f"secondary-word-{index}" for index in range(4)),
        "phase-a",
        "phase-b",
        "phase-c",
        "wire-shape",
        "target-count",
        "selected-target",
        "sphere-mode",
        "reserved-38",
        "reserved-3c-input-lock",
        "target-list-0-bound",
        "target-list-1-bound",
    ]
    if function == 0:
        edge_values = [
            machine.load(SPHERE + WIRE_EDGES + index * 2, 2)
            for index in range(WIRE_EDGE_CAPACITY)
        ]
        values.extend(edge_values)
        names.extend(f"wire-edge-{index}" for index in range(WIRE_EDGE_CAPACITY))
        opaque_offsets = (0x30, 0x80, 0x100, SPHERE_SIZE - 4)
        values.extend(machine.load(SPHERE + offset, 4) for offset in opaque_offsets)
        names.extend(f"cleared-word-{offset:#x}" for offset in opaque_offsets)
    return values, names


def run(machine: Machine, value: Vector) -> tuple[dict, list[int]]:
    machine.map_zero(SPHERE, SPHERE_SIZE)
    machine.store(SPHERE, bytes([0xA5]) * SPHERE_SIZE)
    _put_words(machine, SPHERE, value.primary_words)
    _put_words(machine, SPHERE + 0x10, value.secondary_words)
    machine.put(SPHERE + 0x24, value.phase_a, 2)
    machine.put(SPHERE + 0x26, value.phase_b, 2)
    machine.put(SPHERE + 0x28, value.phase_c, 2)
    machine.put(SPHERE + 0x2A, value.wire_shape, 2)
    machine.put(SPHERE + 0x2C, value.target_count, 1)
    machine.put(SPHERE + 0x2D, value.selected_target, 1)
    machine.put(SPHERE + 0x2E, value.initial_mode, 1)
    machine.put(SPHERE + 0x38, value.reserved_38)
    machine.put(SPHERE + 0x3C, value.reserved_3c)
    if value.preview_already_bound:
        machine.put(SPHERE + 0x44, PREVIEWS)
    _put_words(machine, PRIMARY, value.primary_words)
    _put_words(machine, SECONDARY, value.secondary_words)
    machine.put(SPHERE_GLOBAL, SPHERE if value.sphere_exists else 0)
    machine.put(REUSE_GLOBAL, value.reuse, 1)
    machine.put(BUTTONS_GLOBAL, value.buttons)
    calls: list[int] = []

    def allocate(m: Machine) -> None:
        calls.append(1)
        m.r[2] = SPHERE

    def release(_machine: Machine) -> None:
        calls.append(3)

    def zero(m: Machine) -> None:
        calls.append(2)
        address, size = m.r[4], m.r[5]
        if address != SPHERE or not 0 < size <= SPHERE_SIZE:
            raise ValueError(f"unexpected sphere clear {address:#x}+{size:#x}")
        m.store(address, bytes(size))

    machine.stub(ALLOCATE, allocate)
    machine.stub(FREE, release)
    machine.stub(ZERO, zero)
    machine.stores.clear()

    if value.function == 2:
        machine.r[4:8] = [
            PRIMARY,
            SECONDARY if value.secondary_present else 0,
            value.sphere_mode,
            PREVIEWS if value.preview_argument_present else 0,
        ]
    elif value.function == 4:
        machine.r[4] = SECONDARY
    machine.execute(ENTRIES[value.function], limit=10000)

    observed, names = _state_values(machine, value.function)
    if value.function == 7:
        observed.append(machine.r[2])
        names.append("interaction-mode")
    observed.extend((len(calls), *calls))
    names.extend(("call-count", *(f"call-{index}" for index in range(len(calls)))))
    stores = [
        (address, size)
        for address, size in machine.stores
        if not DEFAULT_STACK - 0x2000 <= address < DEFAULT_STACK + 32
    ]
    return {"values": observed, "names": names, "stores": stores}, value.inputs()


def check(
    image: Image, value: Vector, observation: dict, native: list[int], reference: dict
) -> list[str]:
    del image, value, reference
    expected = observation["values"]
    if native == expected:
        problems = []
    else:
        differences = [
            (name, actual, wanted)
            for name, actual, wanted in zip(
                observation["names"], native, expected, strict=False
            )
            if actual != wanted
        ]
        problems = [f"lifecycle fields differ: {differences[:6]}"]
        if len(native) != len(expected):
            problems.append(f"output lengths differ: {len(native)} != {len(expected)}")
    allowed = [
        (SPHERE, SPHERE + SPHERE_SIZE),
        (SPHERE_GLOBAL, SPHERE_GLOBAL + 4),
        (REUSE_GLOBAL, REUSE_GLOBAL + 1),
    ]
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    del image
    by_function = [0] * len(FUNCTIONS)
    for value, _, _ in results:
        by_function[value.function] += 1
    return [
        "Vectors per function: "
        + ", ".join(f"{name}={count}" for name, count in zip(FUNCTIONS, by_function, strict=True)),
        "Compared sphere lifecycle fields, raw target volume records, edge-list words, "
        "conditional heap calls, pointer state, and interaction mode.",
    ]
