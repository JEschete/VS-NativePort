"""Shared harness for the particle motion and texture animation differentials (BATTLE.PRG
5BF94.c:5770, 6915, 7658, 7830 and 8501-8521, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects cores that
particle_motion_driver.cpp drives:

  step point   func_800CFAAC  StepScreenPointToward, a line's start (+0x10) toward its end (+0x0C)
  spawn        func_800D2A38  PlaceParticleSpawn; ApplyRotMatrix (func_80041508) is a stub both
                              sides answer with the same fake rotation, recording its arguments
  tick         func_800D4C18  TickEffectParticles; the integrator, draw, emit and release are stubs
                              recording their arguments, and an emit may rewrite its particle's
                              lifetime, end event and texture timer (the reread order)
  integrate    func_800D52A4  IntegrateParticleMotion; VectorNormal (func_80041A94) and its GTE
                              helper are followed, so the native NormalizeVector is compared too
  clear, init, advance        func_800D6CCC, func_800D6CF0, func_800D6D24 on the animation's
                              value fields; init's program is ParticleAnimationData's entry and
                              advance's frame is the texture table index its frame pointer names

Particles 0-7 are 0x78-byte records at fixed addresses and links print as record numbers (-1 for
null). The clear's program and frame pointers are cleared by the Live binding, outside the core,
and are not compared; the original's stores are confined to the record.

Native differences outside the compared domain (covered by ParticleMotionTests.cpp): the advance
refuses an unknown command, a second restart in one step and a word past the program, where the
original looped forever or read past the table; an init index past the table's count is refused;
and a frame of a texture table past the bound ones selects no frame. Vectors therefore build
programs that reach a frame or stop word within the program, initialize counted entries and
advance animations on texture tables 0-5. VectorNormal's helper sums the squares of the 16-bit
direction with trapping adds, so the original traps on a squared length of 2**31 or more, which
native NormalizeVector computes unsigned; a dry run of the original captures each attracted
particle's direction after its step, and out-of-domain targets are re-aimed inside it.
"""

from __future__ import annotations

import functools
import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    NULL_RECORD,
    Image,
    Machine,
    R,
    Records,
    mode_case,
    s16,
    s32,
)

MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "particle_motion_driver.cpp"
SOURCES = (
    r"Native\Game\src\Effects\ParticleMotion.cpp",
    r"Native\Game\src\Effects\ParticleTextureAnimation.cpp",
    r"Native\Game\src\Effects\EffectResources.cpp",
    r"Native\Game\src\Effects\PackageExtents.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Math\Interpolation.cpp",
)

(
    STEP_POINT,
    SPAWN,
    TICK,
    INTEGRATE,
    CLEAR,
    INIT,
    ADVANCE,
) = range(7)

# Globals: the current effect package and the particle forces.
PACKAGE_POINTER = 0x800F569C
DRAG_FORCE = 0x800F5618
GRAVITY_FORCE = 0x800F5620

# Fixtures.
LINE = 0x80500000
SCRATCH = 0x80510000
PARTICLES = 0x80520000
INSTANCE = 0x80530000
ANIMATION = 0x80540000
PROGRAM = 0x80550000
TABLE = 0x80560000
PACKAGE = 0x80570000
TEXTURE_BASE = 0x40000000
TEXTURE_STRIDE = 0x100000

# Line endpoints.
LINE_END, LINE_START = 0x0C, 0x10
# Emitter scratch.
FLAGS, OFFSET, AIM, ORIGIN = 0x24, 0x2C, 0x34, 0x98
ROTATE_FLAG = 0x40000
# Particle records.
PARTICLE_BYTES = 0x78
RECORDS = 8
NEXT = 0x04
MASS, GRAVITY_SCALE, POSITION, VELOCITY, FORCE, ACCELERATION = 0x08, 0x0A, 0x0C, 0x18, 0x24, 0x30
TEXTURE_TIMER = 0x42
TARGET, LIFETIME, END_EVENT, TICK_EVENT, ATTRACTION, AGE = 0x62, 0x68, 0x6C, 0x6D, 0x70, 0x77
POSITION_WORDS = 9  # position, velocity and force
# Integrate vector items: mass, gravity scale, position, velocity, force, acceleration, target, attraction.
TARGET_ITEM = 14
SQUARE_LIMIT = 1 << 31
INSTANCE_FIRST = 0x18
# Texture animation records.
TIMER, FRAME_FLAGS, OFFSET_X, OFFSET_Y, CURSOR, TEXTURE_INDEX, PROGRAM_POINTER, FRAME_POINTER = (
    0x06, 0x07, 0x08, 0x0A, 0x0E, 0x16, 0x18, 0x1C)
ANIMATION_BYTES = 0x20
# Package fields.
TEXTURE_TABLES, ANIMATION_TABLE = 0x98, 0xB0
BOUND_TABLES = 6

ROTATE = 0x80041508
NORMALIZE, NORMALIZE_HELPER = 0x80041A94, 0x80041AF4
INTEGRATE_CALL_ADDRESS, DRAW, EMIT, RELEASE = 0x800D52A4, 0x800D6E24, 0x800D2ADC, 0x800D2888

NULL = NULL_RECORD
ROTATE_CALL = 0
INTEGRATE_CALL, DRAW_CALL, EMIT_CALL, RELEASE_CALL = range(4)
ANIMATION_NAMES = ["timer", "frame flags", "offset x", "offset y", "cursor", "texture index"]
FRAME_COMMAND, OFFSET_COMMAND, RESTART_COMMAND, STOP_COMMAND = range(4)


PARTICLE_RECORDS = Records(PARTICLES, PARTICLE_BYTES, RECORDS)
particle, ref, address_of = PARTICLE_RECORDS.at, PARTICLE_RECORDS.number, PARTICLE_RECORDS.address


def texture_table(index: int) -> int:
    return TEXTURE_BASE + index * TEXTURE_STRIDE


def word(rng: random.Random) -> int:
    return s32(rng.choice((0, 1, -1, 2, 0x7F, 0x80, 0xFFFF, 0x10000, 0x7FFFFFFF, -0x80000000, rng.getrandbits(32))))


def half(rng: random.Random) -> int:
    return s16(rng.choice((0, 1, -1, 0x7FFF, 0x8000, rng.getrandbits(16))))


def spread(rng: random.Random, bits: int) -> int:
    """Usually a value within +-2**bits, sometimes any word."""
    return rng.randrange(-(1 << bits), 1 << bits) if rng.random() < 0.85 else word(rng)


def ordered_list(rng: random.Random, value: dict, minimum: int) -> None:
    """A random acyclic list of `minimum`-8 records, linked by value["next"]; others keep stale links."""
    records = rng.sample(range(RECORDS), rng.randrange(minimum, RECORDS + 1))
    value["next"] = [rng.choice((NULL, rng.randrange(RECORDS))) for _ in range(RECORDS)]
    for at, record in enumerate(records):
        value["next"][record] = records[at + 1] if at + 1 < len(records) else NULL
    value["first"] = records[0] if records else NULL
    value["listed"] = records


# ---------------------------------------------------------------- vectors
def point_vector(rng: random.Random, value: dict) -> None:
    target = rng.getrandbits(32)
    roll = rng.random()
    if roll < 0.1:
        point = target
    elif roll < 0.7:
        dx, dy = rng.randrange(-8, 9), rng.randrange(-8, 9)
        point = ((target + dx) & 0xFFFF) | (((target >> 16) + dy) & 0xFFFF) << 16
    elif roll < 0.85:
        # Exact ratios: the minor axis at, just under or just over half the major.
        minor = rng.randrange(1, 64)
        major = 2 * minor + rng.choice((-1, 0, 1))
        dx, dy = (major, minor) if rng.random() < 0.5 else (minor, major)
        dx, dy = dx * rng.choice((1, -1)), dy * rng.choice((1, -1))
        point = ((target + dx) & 0xFFFF) | (((target >> 16) + dy) & 0xFFFF) << 16
    else:
        point = rng.getrandbits(32)
    value["point"], value["target"] = point, target


def spawn_vector(rng: random.Random, value: dict) -> None:
    value["flags"] = rng.getrandbits(32) & ~ROTATE_FLAG | (ROTATE_FLAG if rng.random() < 0.5 else 0)
    value["offset"] = [half(rng) if rng.random() < 0.3 else rng.randrange(-2048, 2048) for _ in range(3)]
    value["origin"] = [spread(rng, 26) for _ in range(3)]
    value["old"] = [word(rng) for _ in range(6)]


def tick_vector(rng: random.Random, value: dict) -> None:
    ordered_list(rng, value, 0)
    value["age"] = [rng.randrange(256) for _ in range(RECORDS)]
    value["lifetime"] = [rng.choice((-1, -1, 0, 1, 1, 2, 3, 0x7FFF, -0x8000, half(rng))) for _ in range(RECORDS)]
    value["end"] = [rng.choice((0, 0, 1, 2, 0xFF, rng.randrange(256))) for _ in range(RECORDS)]
    value["tick"] = [rng.choice((0, 0, 1, 3, 0xFF, rng.randrange(256))) for _ in range(RECORDS)]
    value["timer"] = [rng.choice((0, 0, 1, 2, rng.randrange(256))) for _ in range(RECORDS)]
    # An emit may rewrite its particle's lifetime, end event and texture timer.
    value["poke"] = [
        [1, rng.choice((-1, 0, 1, 2, half(rng))), rng.choice((0, 1, rng.randrange(256))), rng.choice((0, 2, 5))]
        if rng.random() < 0.25 else [0, 0, 0, 0]
        for _ in range(RECORDS)
    ]


def mass(rng: random.Random) -> int:
    return rng.choice((1, 2, 16, 64, 256, 4096, 4096, rng.randrange(1, 0x8000), half(rng), 0, -1))


def attraction_direction(direction: list[int]) -> list[int]:
    """The 16-bit direction VectorNormal reads from the integrator's 32-bit one (the IR moves)."""
    return [s16(word & 0xFFFF) for word in direction]


@functools.cache
def module_images() -> tuple[Image, ...]:
    return tuple(Image.load(module) for module in MODULES)


def captured_directions(value: dict) -> dict[int, list[int]]:
    """Dry-runs the original with VectorNormal stubbed: each attracted particle's 32-bit direction.

    The direction is target - (position >> 12) after the step moved the position, so only the
    original's own step can say which targets stay inside VectorNormal's domain.
    """
    machine = Machine(module_images())
    directions: dict[int, list[int]] = {}

    def capture(machine: Machine) -> None:
        record = (machine.r[R["s2"]] - PARTICLES) // PARTICLE_BYTES
        directions[record] = [s32(machine.load(machine.r[R["a0"]] + 4 * axis)) for axis in range(3)]

    put_integrate(machine, value)
    machine.stub(NORMALIZE, capture)
    machine.r[R["a0"]] = address_of(value["first"])
    machine.execute(INTEGRATE_CALL_ADDRESS)
    return directions


def fit_attraction(rng: random.Random, value: dict) -> None:
    """Keep attracted particles inside the original's domain.

    VectorNormal's helper (0x80041AF4) sums the GTE SQR squares of the 16-bit direction with
    trapping `add`s, so the original requires x*x + y*y + z*z < 2**31. A target that leaves the
    domain, and a few others, is re-aimed so the truncated direction becomes a chosen delta: a
    nearby point, one just inside the limit, or zero (a skip, or a 32-bit direction whose 16-bit
    truncation is zero).
    """
    value["directions"] = captured_directions(value)
    for record, direction in value["directions"].items():
        motion = value["motion"][record]
        if sum(d * d for d in attraction_direction(direction)) < SQUARE_LIMIT and rng.random() >= 0.2:
            continue
        delta = rng.choice((
            [rng.randrange(-4096, 4096) for _ in range(3)],
            [32767, 32767, 362], [-32767, 32767, -362], [-32768, 32767, 0], [-32768, 0, 0], [0, 0, 0],
        ))
        for axis in range(3):
            # The step's position is target - direction; aim the target at position + delta.
            target = motion[TARGET_ITEM + axis]
            moved = s16((target - direction[axis] + delta[axis]) & 0xFFFF)
            motion[TARGET_ITEM + axis] = moved
            direction[axis] = s32(direction[axis] + moved - target)


def integrate_vector(rng: random.Random, value: dict) -> None:
    ordered_list(rng, value, 1)
    value["drag"] = rng.choice((0, 1, 16, 256, rng.randrange(-64, 4096), word(rng)))
    value["gravity"] = [rng.choice((0, rng.randrange(-256, 256), word(rng))) for _ in range(3)]
    value["motion"] = []
    for _ in range(RECORDS):
        attraction = rng.choice((0, 0, 4096, rng.randrange(-(1 << 16), 1 << 16), word(rng)))
        value["motion"].append([
            mass(rng), rng.choice((0, 4096, half(rng))),
            *(spread(rng, 26) for _ in range(3)), *(spread(rng, 16) for _ in range(3)),
            *(spread(rng, 12) for _ in range(3)), *(spread(rng, 8) for _ in range(3)),
            *(rng.choice((half(rng), rng.randrange(-4096, 4096))) for _ in range(3)), attraction,
        ])
    fit_attraction(rng, value)


def animation_fields(rng: random.Random) -> list[int]:
    return [rng.randrange(256), rng.randrange(256), half(rng), half(rng), rng.getrandbits(16), rng.randrange(256)]


def frame_word(rng: random.Random) -> int:
    return rng.randrange(16) << 24 | rng.choice((0, 1, 2, 3, rng.randrange(256))) << 16 | rng.getrandbits(16)


def command_word(rng: random.Random, command: int) -> int:
    if command == FRAME_COMMAND:
        return frame_word(rng)
    return command << 28 | rng.getrandbits(28)


def program(rng: random.Random) -> list[int]:
    """Words whose run from any word reaches a frame or a stop: offsets lead to the first terminal
    from word 0, and a restart may follow it."""
    lead = rng.randrange(0, 4)
    words = [command_word(rng, OFFSET_COMMAND) for _ in range(lead)]
    words.append(command_word(rng, rng.choice((FRAME_COMMAND, FRAME_COMMAND, STOP_COMMAND))))
    for _ in range(rng.randrange(0, 6)):
        words.append(command_word(rng, rng.choice((FRAME_COMMAND, OFFSET_COMMAND, RESTART_COMMAND, STOP_COMMAND))))
    words.append(command_word(rng, rng.choice((FRAME_COMMAND, RESTART_COMMAND, STOP_COMMAND))))
    return words


def animation_vector(rng: random.Random, value: dict, mode: int) -> None:
    value["bytes"] = [rng.randrange(256) for _ in range(ANIMATION_BYTES)]
    value["fields"] = animation_fields(rng)
    if mode == INIT:
        count = rng.randrange(1, 9)
        size = 4 + 2 * count + rng.randrange(0, 64)
        value["texture"] = word(rng) if rng.random() < 0.2 else rng.randrange(BOUND_TABLES)
        value["index"] = rng.randrange(count)
        value["table"] = [size, count, *(rng.randrange(size + 1) for _ in range(count))]
    elif mode == ADVANCE:
        words = program(rng)
        value["fields"][0] = rng.choice((0, 0, 1, 2, 2, 4, rng.randrange(256)))
        value["fields"][4] = rng.randrange(len(words)) * 4
        value["fields"][5] = rng.randrange(BOUND_TABLES)
        value["program"] = words


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode, "number": number}
    if mode == STEP_POINT:
        point_vector(rng, value)
    elif mode == SPAWN:
        spawn_vector(rng, value)
    elif mode == TICK:
        tick_vector(rng, value)
    elif mode == INTEGRATE:
        integrate_vector(rng, value)
    else:
        animation_vector(rng, value, mode)
    return value


# ---------------------------------------------------------------- runs
def run_point(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    machine.map_zero(LINE, 0x14)
    machine.put(LINE + LINE_END, value["target"])
    machine.put(LINE + LINE_START, value["point"])
    machine.r[4] = LINE
    machine.execute(entry)
    return [machine.load(LINE + LINE_START)], ["point"], [value["point"], value["target"]]


def fake_rotation(x: int, y: int, z: int) -> list[int]:
    """The stand-in rotation both sides apply; particle_motion_driver.cpp FakeRotation matches."""
    return [s32(x * 3 - y), s32(y * 2 + z * 5), s32(z - x * 7)]


def run_spawn(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    def rotate(machine: Machine) -> None:
        source, destination = machine.r[4], machine.r[5]
        offset = [machine.load(source + 2 * axis, 2, signed=True) for axis in range(3)]
        calls.append([ROTATE_CALL, int(source == SCRATCH + OFFSET), int(destination == SCRATCH + AIM), 0, 0])
        for axis, rotated in enumerate(fake_rotation(*offset)):
            machine.put(destination + 4 * axis, rotated)

    machine.map_zero(SCRATCH, 0xA4)
    machine.map_zero(PARTICLES, PARTICLE_BYTES)
    machine.put(SCRATCH + FLAGS, value["flags"])
    for axis in range(3):
        machine.put(SCRATCH + OFFSET + 2 * axis, value["offset"][axis], 2)
        machine.put(SCRATCH + ORIGIN + 4 * axis, value["origin"][axis])
        machine.put(SCRATCH + AIM + 4 * axis, value["old"][axis])
        machine.put(PARTICLES + POSITION + 4 * axis, value["old"][3 + axis])
    machine.stub(ROTATE, rotate)
    machine.r[4], machine.r[5] = SCRATCH, PARTICLES
    machine.execute(entry)
    values = [s32(machine.load(SCRATCH + AIM + 4 * axis)) for axis in range(3)]
    values += [s32(machine.load(PARTICLES + POSITION + 4 * axis)) for axis in range(3)]
    names = [f"aim {axis}" for axis in "xyz"] + [f"position {axis}" for axis in "xyz"]
    return values, names, [value["flags"], *value["offset"], *value["origin"]]


def tick_stubs(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    def emit(machine: Machine) -> None:
        emitter = machine.load(machine.r[29] + 0x10)
        plain = int(machine.r[4] == INSTANCE and machine.r[6] == 0 and machine.r[7] == 0)
        calls.append([EMIT_CALL, s32(machine.r[5]), ref(emitter), plain, 0])
        record = ref(emitter)
        if 0 <= record < RECORDS and value["poke"][record][0]:
            _, lifetime, end, timer = value["poke"][record]
            machine.put(particle(record) + LIFETIME, lifetime, 2)
            machine.put(particle(record) + END_EVENT, end, 1)
            machine.put(particle(record) + TEXTURE_TIMER, timer, 1)

    machine.stub(INTEGRATE_CALL_ADDRESS, lambda m: calls.append([INTEGRATE_CALL, ref(m.r[4]), 0, 0, 0]))
    machine.stub(DRAW, lambda m: calls.append([DRAW_CALL, ref(m.r[4]), 0, 0, 0]))
    machine.stub(EMIT, emit)
    machine.stub(RELEASE, lambda m: calls.append([RELEASE_CALL, ref(m.r[4]), int(m.r[5] == INSTANCE), 0, 0]))


def run_tick(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    machine.map_zero(PARTICLES, RECORDS * PARTICLE_BYTES)
    machine.map_zero(INSTANCE, 0x20)
    machine.put(INSTANCE + INSTANCE_FIRST, address_of(value["first"]))
    for index in range(RECORDS):
        base = particle(index)
        machine.put(base + NEXT, address_of(value["next"][index]))
        machine.put(base + AGE, value["age"][index], 1)
        machine.put(base + LIFETIME, value["lifetime"][index], 2)
        machine.put(base + END_EVENT, value["end"][index], 1)
        machine.put(base + TICK_EVENT, value["tick"][index], 1)
        machine.put(base + TEXTURE_TIMER, value["timer"][index], 1)
    tick_stubs(machine, value, calls)
    machine.r[4] = INSTANCE
    machine.execute(entry)
    values = [s32(machine.r[2])]
    names = ["result"]
    for index in range(RECORDS):
        values += [machine.load(particle(index) + AGE, 1), machine.load(particle(index) + LIFETIME, 2, signed=True)]
        names += [f"particle {index} age", f"particle {index} lifetime"]
    inputs = [value["first"], *value["next"]]
    for index in range(RECORDS):
        inputs += [value["age"][index], value["lifetime"][index], value["end"][index], value["tick"][index],
                   value["timer"][index], *value["poke"][index]]
    return values, names, inputs


def put_integrate(machine: Machine, value: dict) -> None:
    machine.map_zero(PARTICLES, RECORDS * PARTICLE_BYTES)
    machine.put(DRAG_FORCE, value["drag"])
    for axis in range(3):
        machine.put(GRAVITY_FORCE + 4 * axis, value["gravity"][axis])
    offsets = [MASS, GRAVITY_SCALE, *(POSITION + 4 * i for i in range(12)), TARGET, TARGET + 2, TARGET + 4, ATTRACTION]
    sizes = [2, 2, *([4] * 12), 2, 2, 2, 4]
    for index in range(RECORDS):
        machine.put(particle(index) + NEXT, address_of(value["next"][index]))
        for offset, size, item in zip(offsets, sizes, value["motion"][index], strict=True):
            machine.put(particle(index) + offset, item, size)


def run_integrate(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    put_integrate(machine, value)
    machine.follow.update({NORMALIZE, NORMALIZE_HELPER})
    machine.r[4] = address_of(value["first"])
    machine.execute(entry)
    values, names = [], []
    for index in range(RECORDS):
        values += [s32(machine.load(particle(index) + POSITION + 4 * i)) for i in range(POSITION_WORDS)]
        names += [f"particle {index} {field} {axis}" for field in ("position", "velocity", "force") for axis in "xyz"]
    inputs = [value["drag"], *value["gravity"], value["first"], *value["next"]]
    for motion in value["motion"]:
        inputs += motion
    return values, names, inputs


def put_animation(machine: Machine, value: dict) -> None:
    machine.map_zero(ANIMATION, ANIMATION_BYTES)
    machine.store(ANIMATION, bytes(value["bytes"]))
    timer, flags, offset_x, offset_y, cursor, texture = value["fields"]
    machine.put(ANIMATION + TIMER, timer, 1)
    machine.put(ANIMATION + FRAME_FLAGS, flags, 1)
    machine.put(ANIMATION + OFFSET_X, offset_x, 2)
    machine.put(ANIMATION + OFFSET_Y, offset_y, 2)
    machine.put(ANIMATION + CURSOR, cursor, 2)
    machine.put(ANIMATION + TEXTURE_INDEX, texture, 1)


def animation_values(machine: Machine) -> list[int]:
    return [
        machine.load(ANIMATION + TIMER, 1),
        machine.load(ANIMATION + FRAME_FLAGS, 1),
        machine.load(ANIMATION + OFFSET_X, 2, signed=True),
        machine.load(ANIMATION + OFFSET_Y, 2, signed=True),
        machine.load(ANIMATION + CURSOR, 2),
        machine.load(ANIMATION + TEXTURE_INDEX, 1),
    ]


def put_package(machine: Machine) -> None:
    machine.put(PACKAGE_POINTER, PACKAGE)
    machine.map_zero(PACKAGE, 0x100)
    for index in range(BOUND_TABLES):
        machine.put(PACKAGE + TEXTURE_TABLES + 4 * index, texture_table(index))
    machine.put(PACKAGE + ANIMATION_TABLE, TABLE)


def run_animation(machine: Machine, value: dict, entry: int, calls: list[list[int]]) -> tuple:
    mode = value["mode"]
    put_animation(machine, value)
    put_package(machine)
    machine.r[4] = ANIMATION
    inputs = list(value["fields"])
    extra, extra_names = [], []
    if mode == INIT:
        size, count, *offsets = value["table"]
        machine.map_zero(TABLE, size)
        machine.put(TABLE, count)
        for index, offset in enumerate(offsets):
            machine.put(TABLE + 4 + 2 * index, offset, 2)
        machine.r[5], machine.r[6] = value["texture"] & 0xFFFFFFFF, value["index"]
        inputs += [value["texture"], value["index"], *value["table"]]
    elif mode == ADVANCE:
        machine.put(ANIMATION + PROGRAM_POINTER, PROGRAM)
        machine.put(ANIMATION + FRAME_POINTER, 0)
        machine.map_zero(PROGRAM, 4 * len(value["program"]))
        for index, item in enumerate(value["program"]):
            machine.put(PROGRAM + 4 * index, item)
        inputs += [len(value["program"]), *value["program"]]
    machine.execute(entry)
    if mode == INIT:
        extra, extra_names = [s32(machine.load(ANIMATION + PROGRAM_POINTER) - TABLE)], ["program offset"]
    elif mode == ADVANCE:
        pointer = machine.load(ANIMATION + FRAME_POINTER)
        texture = machine.load(ANIMATION + TEXTURE_INDEX, 1)
        extra, extra_names = [NULL if pointer == 0 else s32(pointer - texture_table(texture)) >> 2], ["frame"]
    return [*animation_values(machine), *extra], [*ANIMATION_NAMES, *extra_names], inputs


RUNNERS = {
    STEP_POINT: run_point,
    SPAWN: run_spawn,
    TICK: run_tick,
    INTEGRATE: run_integrate,
    **dict.fromkeys((CLEAR, INIT, ADVANCE), run_animation),
}


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    """The spans the original may store to: the stack and the mode's records."""
    spans = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    mode = value["mode"]
    if mode == STEP_POINT:
        spans.append((LINE + LINE_START, LINE + LINE_START + 4))
    elif mode == SPAWN:
        spans += [(SCRATCH + AIM, SCRATCH + AIM + 12), (PARTICLES + POSITION, PARTICLES + POSITION + 12)]
    elif mode == TICK:
        for index in range(RECORDS):
            spans += [(particle(index) + LIFETIME, particle(index) + LIFETIME + 2),
                      (particle(index) + AGE, particle(index) + AGE + 1)]
    elif mode == INTEGRATE:
        spans += [(particle(index) + POSITION, particle(index) + ACCELERATION) for index in range(RECORDS)]
    elif mode == CLEAR:
        spans.append((ANIMATION, ANIMATION + ANIMATION_BYTES))
    elif mode == INIT:
        spans += [(ANIMATION + TIMER, ANIMATION + TIMER + 1), (ANIMATION + CURSOR, ANIMATION + CURSOR + 2),
                  (ANIMATION + TEXTURE_INDEX, ANIMATION + TEXTURE_INDEX + 1),
                  (ANIMATION + PROGRAM_POINTER, ANIMATION + PROGRAM_POINTER + 4)]
    else:
        spans += [(ANIMATION + TIMER, ANIMATION + OFFSET_Y + 2), (ANIMATION + CURSOR, ANIMATION + CURSOR + 2),
                  (ANIMATION + FRAME_POINTER, ANIMATION + FRAME_POINTER + 4)]
    return spans


def describe(image, results) -> list[str]:
    vectors = [value for value, _, _ in results]
    modes = {value["mode"] for value in vectors}
    if modes == {TICK}:
        released = sum(1 for _, observation, _ in results for call in observation["calls"] if call[0] == RELEASE_CALL)
        emitted = sum(1 for _, observation, _ in results for call in observation["calls"] if call[0] == EMIT_CALL)
        return [f"{emitted} emits and {released} releases recorded"]
    if modes == {INTEGRATE}:
        directions = [attraction_direction(direction) for value in vectors for direction in value["directions"].values()]
        wide = sum(1 for direction in directions if sum(d * d for d in direction) >= 1 << 30)
        truncated_zero = sum(1 for direction in directions if not any(direction))
        return [f"{len(directions)} particles normalized a direction; {wide} had a squared length of at least "
                f"2**30 (the original traps at 2**31) and {truncated_zero} truncated to zero"]
    if modes == {ADVANCE}:
        frames = sum(1 for _, observation, _ in results if observation["values"][6] != NULL)
        return [f"{frames} of {len(vectors)} vectors selected a frame"]
    if modes == {SPAWN}:
        rotated = sum(1 for value in vectors if value["flags"] & ROTATE_FLAG)
        return [f"{rotated} of {len(vectors)} vectors rotated the offset"]
    return []


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
