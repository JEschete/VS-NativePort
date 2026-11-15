"""F01298-F01302/F01310 sphere target label lifecycle against original BATTLE.PRG."""

import random
from dataclasses import dataclass

from differential import DEFAULT_STACK, Image, Machine, store_problems

ID = "F01298-F01302/F01310"
FUNCTIONS = ("F01298", "F01299", "F01300", "F01301", "F01302", "F01310")
ENTRIES = (0x800C16FC, 0x800C1A40, 0x800C1D84, 0x800C1DC4, 0x800C20B4, 0x800C4650)
# F01310 ends at 0x800C4704; MapStickDeadZone (F01311) starts at 0x800C4708.
ENDS = (0x800C1A40, 0x800C1D84, 0x800C1DC4, 0x800C20B4, 0x800C2254, 0x800C4708)
ENTRY, END = ENTRIES[0], ENDS[-1]
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "sphere_target_labels_driver.cpp"
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
INCLUDES = (
    r"Native\Game\src",
    r"Native\Engine",
    r"Native\Engine\src",
    r"Native\Content",
)
SOURCES = (
    r"Native\Game\src\Battle\SphereTargetLabels.cpp",
    r"Native\Game\src\Battle\SphereTargetLabelsLive.cpp",
    r"Native\Engine\src\Core\Log.cpp",
)
VECTORS, MUTANT_VECTORS, SEED = 3072, 384, 121298

MASK = 0xFFFFFFFF
LABEL_COUNT, LABEL_STRIDE, LABELS = 24, 0x14, 0x990
STEP_COUNT, RECORD_COUNT, RECORD_STRIDE = 16, 8, 0x18
SPHERE, SPHERE_SIZE = 0x80500000, 0x4B1C
SPHERE_GLOBAL = 0x800EB9B8
COLUMN_TOP_GLOBAL = 0x800F4CB4
STEPS_GLOBAL = 0x800EBBC8
SCRATCH = 0x1F800000  # word 0: primitive cursor; word 2: overlay ordering table
ORDERING_SLOTS, ORDERING_ORIGIN = 512, 300
ORDERING = 0x80600000
OT = ORDERING + ORDERING_ORIGIN * 4
PRIMITIVES, PRIMITIVE_SPACE = 0x80700000, 0x38 * 32
TEXTS, TEXT_STRIDE = 0x80800000, 0x20
RECORDS = 0x80900000

ADD_TILE, INSERT_TPAGE = 0x800CCCB8, 0x800CCD00
GRADIENT_STOP, DRAW_TEXT, LOAD_VIEW = 0x800C8FAC, 0x800C6540, 0x800C02A8
VOLUME_TEST, ENABLE_PULSE, DISABLE_PULSE = 0x800C1564, 0x8009FD5C, 0x8009FE74
RTPS_PC, OTZ_STORE_PC, BOX_DONE_PC = 0x800C1FD0, 0x800C1FE0, 0x800C1FA4

# Events, each printed as kind followed by its fixed fields.
LINE, TPAGE, GRADIENT, TEXT, BOX, VIEW, PROJECT, VOLUME, ENABLE, DISABLE = range(1, 11)
BOX_WORDS = 14

JR_RA = 0x03E00008
# Each function returning at its first instruction, plus one changed decision per function.
MUTANTS = {
    **{f"{function}-early-return": {entry: JR_RA} for function, entry in zip(FUNCTIONS, ENTRIES, strict=True)},
    "F01298-gray-scale-128": {0x800C17E0: 0x00042140},  # sll $a0, $a0, 5
    "F01299-diagonal-rightward": {0x800C1CDC: 0x02A31821},  # addu $v1, $s5, $v1
    "F01300-sliding-left-settled": {0x800C1D9C: 0x2C420003},  # sltiu $v0, $v0, 3
    "F01301-right-text-inset-7": {0x800C1E80: 0x26100007},  # addiu $s0, $s0, 7
    "F01302-signed-slide-steps": {0x800C2134: 0x80420000},  # lb $v0, ($v0)
    "F01302-visible-right-319": {0x800C2204: 0x2C4201BD},  # sltiu $v0, $v0, 0x1bd
    "F01310-position-shift-5": {0x800C4690: 0x00101143},  # sra $v0, $s0, 5
}


def gradient_stop(gradient: int, color_index: int, brightness: int) -> int:
    """The stubbed vs_battle_getGradientStop result; the driver computes the same word."""
    value = (gradient * 0x01000193) ^ (color_index * 0x9E3779B1) ^ (brightness << 7) ^ 0x2C5A9E11
    return value & MASK


def in_volume(x: int, y: int, z: int) -> int:
    """The stubbed volume test: whether the halfword point has an odd sum."""
    return (x ^ y ^ z) & 1


def chain_seed(slot: int) -> int:
    return 0x00A00000 + slot * 0x1111 if 0 <= slot <= 2 else 0


def s16(value: int) -> int:
    value &= 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


@dataclass(frozen=True)
class Vector:
    function: int
    anchor: int
    ring: int
    pulse_head: int
    column_top: int
    project_dx: int
    project_dy: int
    label_index: int
    count: int
    steps: tuple[int, ...]
    labels: tuple[tuple[int, ...], ...]  # state, depth, gradient, ring, x, offset_y, fade, wx, wy, wz
    records: tuple[tuple[int, ...], ...]  # x, y, z, actor, limb

    def inputs(self) -> list[int]:
        return [
            self.function, self.anchor, self.ring, self.pulse_head, self.column_top,
            self.project_dx, self.project_dy, self.label_index, self.count,
            *self.steps,
            *(field for label in self.labels for field in label),
            *(field for record in self.records for field in record),
        ]


def _point(rng: random.Random, wide: bool) -> int:
    if wide:
        return rng.getrandbits(32)
    x = rng.randint(-60, 380) & 0xFFFF
    y = rng.randint(-60, 300) & 0xFFFF
    return x | y << 16


def _pulse_head(rng: random.Random, wide: bool) -> int:
    if wide:
        return rng.choice((-0x80000000, -1, 0x7FFFFFFF, rng.getrandbits(32) - 0x80000000))
    return rng.randint(-20, 48)


def _screen_x(rng: random.Random) -> int:
    pick = rng.randrange(6)
    if pick == 0:
        return rng.randint(-0x8000, 0x7FFF)
    if pick == 1:
        return rng.choice((-127, -126, -125, 158, 159, 160, 161, 247, 248, 249, 318, 319, 320, 321))
    if pick == 2:
        return rng.randint(-160, -60)
    if pick == 3:
        return rng.randint(260, 360)
    return rng.randint(-20, 300)


def _label(rng: random.Random, selected: bool, wide: bool) -> tuple[int, ...]:
    state = rng.choice((0, 1, 2, 3)) if not wide else rng.getrandbits(8)
    depth = (1 if rng.randrange(4) else rng.choice((2, 0x80, 0xFF))) if selected else 0
    gradient = rng.randint(0, 9) if not wide else rng.getrandbits(8)
    ring = rng.randint(0, 31) if not wide else rng.getrandbits(8)
    world = (rng.randint(-200, 400), rng.randint(-200, 300), rng.randint(-0x8000, 0x7FFF))
    if wide:
        world = tuple(rng.randint(-0x8000, 0x7FFF) for _ in range(3))
    return (state, depth, gradient, ring, _screen_x(rng), rng.getrandbits(8), rng.getrandbits(8), *world)


def _steps(rng: random.Random, lane: int) -> tuple[int, ...]:
    if lane % 3 == 0:
        return tuple(sorted((rng.getrandbits(8) for _ in range(STEP_COUNT)), reverse=True))
    if lane % 3 == 1:
        start = rng.randint(80, 255)
        return tuple(max(0, start - index * rng.randint(4, 16)) for index in range(STEP_COUNT))
    return tuple(rng.getrandbits(8) for _ in range(STEP_COUNT))


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % len(FUNCTIONS), number // len(FUNCTIONS)
    wide = lane % 8 == 7
    labels = [_label(rng, False, wide) for _ in range(LABEL_COUNT)]
    label_index = rng.randrange(LABEL_COUNT)
    if function == 2 and lane % 2 == 0:
        labels = [(rng.choice((0, 1)), *label[1:]) for label in labels]
        if lane % 4 == 2:
            labels[label_index] = (rng.choice((2, 3, 0xFF)), *labels[label_index][1:])
    if function == 3 or (function == 4 and lane % 2 == 0):
        labels[label_index] = _label(rng, lane % 5 != 4, wide)
        if function == 4:
            labels[label_index] = (rng.choice((1, 2, 3)), *labels[label_index][1:])
    records = []
    for _ in range(RECORD_COUNT):
        actor = (rng.randint(0, 15), rng.randint(0, 15), rng.randint(16, 31), rng.getrandbits(8))[rng.randrange(4)]
        records.append((rng.getrandbits(16), rng.getrandbits(16), rng.getrandbits(16), actor,
                        rng.randint(-128, 127)))
    count = rng.randint(1, RECORD_COUNT) if lane % 6 else rng.choice((-2, -1, 0))
    return Vector(
        function=function,
        anchor=_point(rng, wide),
        ring=_point(rng, wide),
        pulse_head=_pulse_head(rng, wide),
        column_top=rng.getrandbits(8),
        project_dx=rng.randint(-100, 200) if not wide else rng.randint(-0x8000, 0x7FFF),
        project_dy=rng.randint(-100, 200) if not wide else rng.randint(-0x8000, 0x7FFF),
        label_index=label_index,
        count=count,
        steps=_steps(rng, lane),
        labels=tuple(labels),
        records=tuple(records),
    )


def _slot(chain: int) -> int:
    offset = (chain - OT) & MASK
    if offset & 0x80000000:
        offset -= 1 << 32
    return offset // 4 if offset % 4 == 0 else 0x7FFF


def _text_index(pointer: int) -> int:
    offset = pointer - TEXTS
    if 0 <= offset < LABEL_COUNT * TEXT_STRIDE and offset % TEXT_STRIDE == 0:
        return offset // TEXT_STRIDE
    return -1


def _write_labels(machine: Machine, value: Vector) -> None:
    for index, label in enumerate(value.labels):
        state, depth, gradient, ring, x, offset_y, fade, wx, wy, wz = label
        base = SPHERE + LABELS + index * LABEL_STRIDE
        machine.store(base, bytes((state, depth, gradient, ring)))
        machine.put(base + 4, x, 2)
        machine.store(base + 6, bytes((offset_y, fade)))
        machine.put(base + 8, wx, 2)
        machine.put(base + 10, wy, 2)
        machine.put(base + 12, wz, 2)
        machine.put(base + 14, 0, 2)
        machine.put(base + 16, TEXTS + index * TEXT_STRIDE)


def _label_values(machine: Machine) -> list[int]:
    values = []
    for index in range(LABEL_COUNT):
        base = SPHERE + LABELS + index * LABEL_STRIDE
        values += [machine.load(base, 1), machine.load(base + 1, 1), machine.load(base + 2, 1),
                   machine.load(base + 3, 1), machine.load(base + 4, 2, signed=True),
                   machine.load(base + 6, 1), machine.load(base + 7, 1),
                   machine.load(base + 8, 2, signed=True), machine.load(base + 10, 2, signed=True),
                   machine.load(base + 12, 2, signed=True), _text_index(machine.load(base + 16))]
    return values


def run(machine: Machine, value: Vector) -> tuple[dict, list[int]]:
    machine.map_zero(SPHERE, SPHERE_SIZE)
    machine.map_zero(SCRATCH, 0x400)
    machine.map_zero(ORDERING, ORDERING_SLOTS * 4)
    machine.map_zero(PRIMITIVES, PRIMITIVE_SPACE)
    machine.map_zero(RECORDS, RECORD_COUNT * RECORD_STRIDE)
    machine.put(SPHERE_GLOBAL, SPHERE)
    machine.put(COLUMN_TOP_GLOBAL, value.column_top, 1)
    machine.store(STEPS_GLOBAL, bytes(value.steps))
    machine.put(SCRATCH, PRIMITIVES)
    machine.put(SCRATCH + 8, OT)
    for slot in range(3):
        machine.put(OT + slot * 4, chain_seed(slot))
    _write_labels(machine, value)
    for index, (x, y, z, actor, limb) in enumerate(value.records):
        base = RECORDS + index * RECORD_STRIDE
        machine.put(base, x, 2)
        machine.put(base + 2, y, 2)
        machine.put(base + 4, z, 2)
        machine.put(base + 9, actor, 1)
        machine.put(base + 10, limb, 1)

    events: list[int] = []

    def add_tile(m: Machine) -> None:
        events.extend((LINE, _slot(m.r[4]), m.r[5] & MASK, m.r[6] & MASK, m.r[7] & MASK))

    def insert_tpage(m: Machine) -> None:
        events.extend((TPAGE, _slot(m.r[5]), m.r[4] & MASK))

    def gradient(m: Machine) -> None:
        a0, a1, a2 = m.r[4] & MASK, m.r[5] & MASK, m.r[6] & MASK
        events.extend((GRADIENT, a0, a1, a2))
        m.r[2] = gradient_stop(a0, a1, a2)

    def text(m: Machine) -> None:
        events.extend((TEXT, _slot(m.r[7]), _text_index(m.r[4]), m.r[5] & MASK, m.r[6] & MASK))

    def view(_m: Machine) -> None:
        events.append(VIEW)

    def volume(m: Machine) -> None:
        offset = (m.r[4] - SPHERE) & MASK
        point = [m.load(m.r[5] + axis * 2, 2) for axis in range(3)]
        events.extend((VOLUME, offset // 0x10 if offset % 0x10 == 0 else 0x7FFF, *point))
        m.r[2] = in_volume(*point)

    def enable(m: Machine) -> None:
        events.extend((ENABLE, m.r[4] & MASK, m.r[5] & MASK, m.r[6] & MASK))

    def disable(m: Machine) -> None:
        events.extend((DISABLE, m.r[4] & MASK, m.r[5] & MASK))

    for address, handler in ((ADD_TILE, add_tile), (INSERT_TPAGE, insert_tpage),
                             (GRADIENT_STOP, gradient), (DRAW_TEXT, text), (LOAD_VIEW, view),
                             (VOLUME_TEST, volume), (ENABLE_PULSE, enable),
                             (DISABLE_PULSE, disable)):
        machine.stub(address, handler)
    machine.follow.update((ENTRIES[0], ENTRIES[1], ENTRIES[3]))

    original_step = machine.step

    def step(pc: int):
        if pc == RTPS_PC:
            # The projection is stubbed: SXY2 is the world x/y moved by the vector's offset.
            vx, vy, vz = s16(machine.gte[0]), s16(machine.gte[0] >> 16), s16(machine.gte[1])
            events.extend((PROJECT, vx, vy, vz))
            machine.gte[14] = ((vx + value.project_dx) & 0xFFFF) | ((vy + value.project_dy) & 0xFFFF) << 16
            return None
        if pc == OTZ_STORE_PC:
            return None  # the OTZ the original keeps on its stack is never read
        if pc == BOX_DONE_PC:
            packet = machine.r[7] - BOX_WORDS * 4
            events.extend((BOX, _slot(machine.r[22]),
                           *(machine.load(packet + word * 4) for word in range(BOX_WORDS))))
        return original_step(pc)

    machine.step = step
    machine.stores.clear()
    if value.function in (0, 1):
        machine.r[4:7] = [value.anchor, value.ring, value.pulse_head & MASK]
    elif value.function == 3:
        machine.r[4] = SPHERE + LABELS + value.label_index * LABEL_STRIDE
    elif value.function == 5:
        machine.r[4:6] = [RECORDS, value.count & MASK]
    machine.execute(ENTRIES[value.function], limit=200_000)

    values = [len(events), *events]
    if value.function == 2:
        values.append(machine.r[2] & MASK)
    if value.function in (2, 3, 4):
        values += _label_values(machine)
    stores = [(address, size) for address, size in machine.stores
              if not DEFAULT_STACK - 0x2000 <= address < DEFAULT_STACK + 32]
    return {"values": values, "stores": stores}, value.inputs()


def check(image: Image, value: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    expected = observation["values"]
    problems = []
    if native != expected:
        first = next((index for index, (ours, theirs) in enumerate(zip(native, expected, strict=False))
                      if ours != theirs), min(len(native), len(expected)))
        problems.append(f"{FUNCTIONS[value.function]} output differs at value {first}: native "
                        f"{native[first:first + 6]} original {expected[first:first + 6]} "
                        f"(lengths {len(native)}, {len(expected)})")
    labels = SPHERE + LABELS
    allowed = [(labels, labels + LABEL_COUNT * LABEL_STRIDE), (ORDERING, ORDERING + ORDERING_SLOTS * 4),
               (PRIMITIVES, PRIMITIVES + PRIMITIVE_SPACE), (SCRATCH, SCRATCH + 4)]
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    del image
    by_function = [0] * len(FUNCTIONS)
    events = [0] * len(FUNCTIONS)
    for value, observation, _ in results:
        by_function[value.function] += 1
        events[value.function] += observation["values"][0]
    return [
        "Vectors per function: "
        + ", ".join(f"{name}={count}" for name, count in zip(FUNCTIONS, by_function, strict=True)),
        "Event words per function: "
        + ", ".join(f"{name}={count}" for name, count in zip(FUNCTIONS, events, strict=True)),
        "Compared in order every link line, draw mode, gradient stop, text, label box packet, view "
        "load, projected point, volume test and palette pulse call, the settled result and every "
        "label field afterwards; original stores outside the labels, ordering table, primitive "
        "buffer and primitive cursor fail.",
    ]
