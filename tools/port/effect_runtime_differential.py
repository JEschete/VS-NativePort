"""Shared harness for the effect runtime differentials (BATTLE.PRG 5BF94.c:5047-5070, 5170-5540,
5697-5714, #276).

Each case runs one original under the R3000 interpreter against the typed VS::Effects core that
effect_runtime_driver.cpp drives. The runtime's globals stay at their PS1 addresses; instances 0-6,
renderer slots, timelines, the package, its blocks and the menu reload's disc request are records
at fixed addresses, and both runs print every link as a record number (-1 for null). Every callee
outside the runtime is a stub recording a call [kind, three words]; the phase setters
(func_800CF478, func_800CF484) and the track check (func_800CEEBC) run as the original. The update's
primary destruction stub unlinks the instance, as the original did, so the update's empty-chain
check sees it; the clear's only records it, because the clear walks the links it read beforehand.
Update vectors are weighted to the working phases and finished loads, a fifth end every track so
the action's release runs, and start vectors favour a startable state with the track pending.

Native differences outside the compared domain (covered by EffectRuntimeTests.cpp): a menu reload
the disc queue refuses is not enqueued where the original enqueued the null request, a track outside
the 32-track mask is refused where the original shifted by its low five bits, a slot without a
renderer is refused, and an ended secondary instance's busy bits are read before it is destroyed
where the original read them from the freed record (the stub frees nothing). Vectors therefore get a
disc request, start tracks 0-31 and give every slot a renderer. A secondary spawn without a package
is refused before its token, targets past the request's 16 are not spawned and a refused instance is
skipped, where the original read through the null package, read past the request and wrote through
the null instance: spawn vectors have a package, at most 16 targets and an instance for each. The
secondary instance creation is a stub recording its spawn record, and the source actor effect's
secondary start a stub recording its request; the stack starts as garbage so an unwritten field
shows.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Machine,
    mode_case,
    printed_problems,
    recorded_observation,
    s16,
    s32,
)
from effect_instances_differential import chain, walk

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "effect_runtime_driver.cpp"
SOURCES = (
    r"Native\Game\src\Effects\EffectRuntime.cpp",
    r"Native\Game\src\Effects\PrimaryEffect.cpp",
    r"Native\Game\src\Effects\EffectInstances.cpp",
)

(
    INIT,
    RESET,
    UPDATE_PRIMARY,
    UPDATE_SECONDARY,
    START_TRACK,
    UPDATE_RUNTIME,
    BEGIN_ACTION,
    CLEAR_PRIMARY,
    SPAWN_SECONDARY,
    BEGIN_SECONDARY,
    SPAWN_REQUEST,
    SOURCE_ACTOR,
    CLEAR_MODE,
) = range(13)
SPAWN_MODES = (SPAWN_SECONDARY, BEGIN_SECONDARY, SPAWN_REQUEST)

# Runtime globals.
BUFFER_INDEX = 0x800EC2E4
MENU_SLOT = 0x800F5218
PENDING = 0x800F521C
TRACK_COUNT = 0x800F5224
STARTED = 0x800F5228
VARIANT = 0x800F522C
REQUEST = 0x800F5230
REQUEST_BYTES = 0xE8
ACTING_ACTOR = REQUEST + 8
TOKEN = 0x800F5318
PHASE = 0x800F531C
TOKENS = 0x800F5320
TOKEN_WORDS = 4
CURVE_SAMPLE = 0x800F5330
UNUSED = 0x800F53B4
PRIMARY_HEAD = 0x800F53B8
CURRENT = 0x800F53BC
SECONDARY_REQUEST = 0x800F53C0
LOADED = 0x800F54A8
VARIANT_ARGUMENT = 0x800F54A9
SECONDARY_HEAD = 0x800F54B0
EFFECT_CAMERA = 0x800F54D8
SAVED_CAMERA = 0x800F55A8
PACKAGE_BUFFER = 0x800F567C
PURGE_LOADED = 0x800F5684
PACKAGE_FILE = 0x800F5688
PACKAGE_POINTER = 0x800F569C

# Records.
INSTANCES = 0x80500000
INSTANCE_STRIDE = 0x1000
INSTANCE_RECORDS = 7
CHAINED = 6
SLOTS = 0x80510000
SLOT_STRIDE = 0x10
SLOTS_PER_INSTANCE = 3
RENDERERS = 0x80520000
RENDERER_STRIDE = 0x40
TIMELINES = 0x80530000
TIMELINE_STRIDE = 0x100
PACKAGE = 0x80540000
BLOCK8 = 0x80541000
BLOCK9 = 0x80542000
DISC_SLOT = 0x80550000
REQUEST_RECORD = 0x80560000
BUFFER = 0x80570000

NEXT = 0
BUSY_WORD = 0x14
FRAME = 0x16
TIMELINE = 0xD4C
RENDERER_HEAD = 0xD50
EFFECT_ID = 0xD54
SECONDARY_PHASE = 0xD56
PARAMETER = 0xD58
CURVES = 0x8C
SCRIPT_BLOCK = 0x90
TRACK_BLOCK = 0xB4
TRACK_FRAMES = 0x14
SFX_BANK = 0xC4
STREAM = 0xC8
STREAM_AUX = 0xCC

# Callees.
RESET_INSTANCES = 0x800CE67C
INIT_RESOURCES = 0x800D7814
RESET_POOL_REFERENCES = 0x800D268C
CREATE_PRIMARY = 0x800CE83C
DESTROY_PRIMARY = 0x800CE8F4
UPDATE_PRIMARY_ADDRESS = 0x800CE9B0
UPDATE_SECONDARY_ADDRESS = 0x800CED60
CAN_START = 0x800CEEBC
SET_PHASE = 0x800CF478
SET_SECONDARY_PHASE = 0x800CF484
SPAWN_SECONDARY_ADDRESS = 0x800CF218
BEGIN_SECONDARY_ADDRESS = 0x800CF3F8
ALLOCATE_TOKEN = 0x800CF49C
RELEASE_TOKEN = 0x800CF514
CREATE_SECONDARY = 0x800CF55C
DESTROY_SECONDARY = 0x800CF614
DETACH = 0x800CF70C
SAMPLE = 0x800D1104
RESET_CAMERA = 0x800D1930
CAPTURE = 0x800D1B18
CAMERA_MOTION = 0x800D2560
ACQUIRE_POOL = 0x800D2698
RELEASE_POOL = 0x800D278C
RUN_SCRIPT = 0x800D51D8
SELECT = 0x800D7890
PARTICLES = 0x800D78F0
LOAD_PURGE = 0x800D7BA4
LOAD_PACKAGE = 0x800D7BF8
RELEASE_PACKAGE = 0x800D7C5C
LOAD_TEXTURES = 0x800D7CFC
LOAD_PLUGINS = 0x800D7EF4
BIND = 0x800D8060
MENU_SECTOR = 0x800D8200
WAIT = 0x80042C74
STOP_VIBRATION = 0x800434A4
FREE_HIGH = 0x80043C60
ALLOCATE_DISC = 0x80044B10
RELEASE_DISC = 0x80044B80
ENQUEUE = 0x80044BC4
UNBIND_SFX = 0x80046194
UNBIND_STREAM = 0x80046634
RELEASE_WORKSPACE = 0x8007E1C0
CONFIGURE = 0x800FABB8

POLLS = (LOAD_PURGE, LOAD_TEXTURES, LOAD_PLUGINS, LOAD_PACKAGE)

# Recorded call kinds; effect_runtime_driver.cpp numbers them the same.
(
    SELECT_CALL,
    RESET_CALL,
    RESOURCES_CALL,
    POOL_REFERENCES_CALL,
    POLL_CALL,
    WAIT_CALL,
    CONFIGURE_CALL,
    WORKSPACE_CALL,
    BIND_CALL,
    MENU_SECTOR_CALL,
    ALLOCATE_DISC_CALL,
    ENQUEUE_CALL,
    SAMPLE_CALL,
    PARTICLES_CALL,
    SCRIPT_CALL,
    DESTROY_PRIMARY_CALL,
    UNBIND_SFX_CALL,
    UNBIND_STREAM_CALL,
    VIBRATION_CALL,
    RELEASE_PACKAGE_CALL,
    RELEASE_POOL_CALL,
    RELEASE_TOKEN_CALL,
    RESET_CAMERA_CALL,
    RELEASE_DISC_CALL,
    ACQUIRE_POOL_CALL,
    RENDERER_CALL,
    DETACH_CALL,
    DESTROY_SECONDARY_CALL,
    CREATE_CALL,
    UPDATE_PRIMARY_CALL,
    UPDATE_SECONDARY_CALL,
    CAMERA_MOTION_CALL,
    ALLOCATE_TOKEN_CALL,
    CAPTURE_CALL,
    FREE_CALL,
    SECONDARY_CALL,
    SECONDARY_SCRIPT_CALL,
    SOURCE_CALL,
    SOURCE_ENDPOINT_CALL,
    SOURCE_TARGET_CALL,
) = range(1, 41)

NULL = -1
SLOT_REF = 100
DISC_REF = 300
OTHER = 999
PHASES = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9)


def instance(index: int) -> int:
    return INSTANCES + index * INSTANCE_STRIDE


def slot(index: int) -> int:
    return SLOTS + index * SLOT_STRIDE


def renderer(index: int) -> int:
    return RENDERERS + index * RENDERER_STRIDE


def timeline(index: int) -> int:
    return TIMELINES + index * TIMELINE_STRIDE


SLOT_RECORDS = INSTANCE_RECORDS * SLOTS_PER_INSTANCE
ADDRESSES = {
    **{instance(index): index for index in range(INSTANCE_RECORDS)},
    **{slot(index): SLOT_REF + index for index in range(SLOT_RECORDS)},
    **{timeline(index): index for index in range(INSTANCE_RECORDS)},
    DISC_SLOT: DISC_REF,
}


def ref(address: int) -> int:
    """The record number at `address`: -1 for null, OTHER for an address no record has."""
    return NULL if address == 0 else ADDRESSES.get(address, OTHER)


def instance_address(record: int) -> int:
    return 0 if record == NULL else instance(record)


def call(kind: int, first: int = 0, second: int = 0, third: int = 0) -> list[int]:
    return [kind, first, second, third]


def instance_chain(rng: random.Random, value: dict, minimum: int = 0) -> None:
    value["head"], nexts = chain(rng, CHAINED, minimum)
    value["next"] = [*nexts, rng.choice((NULL, rng.randrange(CHAINED)))]


def signed_word(rng: random.Random, near: int = 0) -> int:
    return s32(
        rng.choice(
            (
                near,
                near - 1,
                near + 1,
                0,
                1,
                -1,
                0x7FFFFFFF,
                -0x80000000,
                rng.randrange(-4, 40),
                rng.randrange(1 << 32),
            )
        )
    )


def track_count_byte(rng: random.Random) -> int:
    return rng.choice(
        (0, 1, 2, 31, 32, 33, 0xFF, rng.randrange(1, 40), rng.randrange(256))
    )


def update_poll(rng: random.Random) -> int:
    """Biased to a finished load so the package and plugin paths run."""
    return rng.choice((0, 0, 0, 0, 0, 0, 1, -1 & 0xFFFFFFFF, rng.randrange(1 << 32)))


def primary_phase(rng: random.Random) -> int:
    return rng.choice(
        (*PHASES, 2, 3, 4, 4, 4, 6, 6, 8, 9, 9, 10, 0xFFFFFFFF, rng.randrange(1 << 32))
    )


def update_phase(rng: random.Random) -> int:
    """Weighted to the update's working phases: textures, package, playing, purge, plugins, reload."""
    return rng.choice(
        (
            *PHASES,
            2,
            3,
            3,
            3,
            3,
            4,
            4,
            4,
            6,
            6,
            8,
            8,
            8,
            9,
            9,
            10,
            0xFFFFFFFF,
            rng.randrange(1 << 32),
        )
    )


def reset_vector(rng: random.Random, value: dict) -> None:
    value["globals"] = [
        primary_phase(rng),
        signed_word(rng),
        signed_word(rng),
        rng.randrange(2),
        rng.randrange(256),
        rng.choice((NULL, rng.randrange(INSTANCE_RECORDS))),
        rng.choice((NULL, rng.randrange(INSTANCE_RECORDS))),
        signed_word(rng),
        rng.choice((NULL, rng.randrange(INSTANCE_RECORDS))),
        signed_word(rng),
        rng.randrange(256),
        rng.choice((NULL, DISC_REF)),
        *(rng.randrange(1 << 32) for _ in range(TOKEN_WORDS)),
        rng.randrange(1 << 32),
    ]


def primary_vector(rng: random.Random, value: dict) -> None:
    instance_chain(rng, value)
    count = rng.randrange(256)
    value.update(
        phase=update_phase(rng),
        track_count=signed_word(rng, count),
        started=signed_word(rng, count),
        pending=rng.choice((0, 0, 1, 0x80000000, 0xFFFFFFFF, rng.randrange(1 << 32))),
        loaded=rng.randrange(2),
        token=rng.randrange(256),
        menu=rng.choice((NULL, DISC_REF, DISC_REF)),
        disc_state=s16(rng.choice((4, 4, 0, 1, 3, 5, -1, rng.randrange(1 << 16)))),
        purge=rng.choice((0, 0, 1, rng.randrange(1 << 32))),
        effect=rng.choice((0, 1, 0xFFFF, rng.randrange(1 << 16))),
        lba=rng.choice((0, 0, 1, rng.randrange(1 << 16))),
        tracks=track_count_byte(rng),
        polls=[update_poll(rng) for _ in POLLS],
        sector=rng.randrange(1 << 32),
        current=rng.choice((NULL, rng.randrange(INSTANCE_RECORDS))),
        frames=[
            rng.choice((0, 0xFFFF, rng.randrange(1 << 16)))
            for _ in range(INSTANCE_RECORDS)
        ],
        scripts=[
            rng.choice((0, 1, 1, rng.randrange(1 << 32)))
            for _ in range(INSTANCE_RECORDS)
        ],
        curves=rng.choice((0, BUFFER)),
        sfx=rng.choice((0, rng.randrange(1, 1 << 32))),
        stream=rng.choice((0, rng.randrange(1, 1 << 32))),
        aux=rng.randrange(1 << 32),
    )
    if rng.randrange(5) == 0:
        # Every track's script ends with the started tracks at (or one short of) the count: the action's release.
        value.update(
            phase=4,
            scripts=[0] * INSTANCE_RECORDS,
            started=s32(value["track_count"] + rng.choice((0, 0, 1, -1))),
        )


def start_vector(rng: random.Random, value: dict) -> None:
    count = rng.randrange(40)
    track = rng.randrange(32)
    bit = 1 << track
    value.update(
        track=track,
        phase=rng.choice((4, 5, 4, 5, 4, 5, *PHASES, rng.randrange(1 << 32))),
        track_count=signed_word(rng, count),
        started=rng.choice(
            (signed_word(rng, count), s32(count - rng.choice((1, 1, 2, 0))))
        ),
        pending=rng.choice(
            (0xFFFFFFFF, bit, bit | rng.randrange(1 << 32), rng.randrange(1 << 32), 0)
        ),
        token=rng.randrange(256),
    )
    if rng.randrange(2) == 0:
        value["track_count"] = count


def secondary_vector(rng: random.Random, value: dict) -> None:
    instance_chain(rng, value)
    value.update(
        current=rng.choice((NULL, rng.randrange(INSTANCE_RECORDS))),
        curves=rng.choice((0, BUFFER)),
        phases=[
            s16(rng.choice((3, 4, 4, 2, 0, 5, -1, rng.randrange(1 << 16))))
            for _ in range(INSTANCE_RECORDS)
        ],
        busy=[rng.randrange(1 << 32) for _ in range(INSTANCE_RECORDS)],
        frames=[
            rng.choice((0, 0xFFFF, rng.randrange(1 << 16)))
            for _ in range(INSTANCE_RECORDS)
        ],
        scripts=[
            rng.choice((0, 1, 1, rng.randrange(1 << 32)))
            for _ in range(INSTANCE_RECORDS)
        ],
        slots=[rng.randrange(SLOTS_PER_INSTANCE + 1) for _ in range(INSTANCE_RECORDS)],
        updates=[
            rng.choice((0, 1, 1, rng.randrange(1 << 32))) for _ in range(SLOT_RECORDS)
        ],
    )


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode, "number": number}
    if mode == RESET:
        reset_vector(rng, value)
    elif mode == UPDATE_PRIMARY:
        primary_vector(rng, value)
    elif mode == UPDATE_SECONDARY:
        secondary_vector(rng, value)
    elif mode == START_TRACK:
        start_vector(rng, value)
    elif mode == UPDATE_RUNTIME:
        value.update(
            buffer=rng.choice((0, 1, 2, 0xFF, rng.randrange(256))),
            primary=rng.randrange(2),
            phase=rng.choice((4, 4, *PHASES, rng.randrange(1 << 32))),
            secondary=rng.randrange(8),
            old_phase=rng.choice((4, primary_phase(rng))),
        )
    elif mode == BEGIN_ACTION:
        value.update(
            request=list(rng.randbytes(REQUEST_BYTES)),
            variant=s32(
                rng.choice((0, 0, 1, 0xFF, 0x100, 0x1FF, -1, rng.randrange(1 << 32)))
            ),
            argument=s32(rng.choice((0, 1, 0xFF, 0x1234, -1, rng.randrange(1 << 32)))),
            source=rng.choice((4, 4, 0, 3, 5, 0x104, rng.randrange(1 << 16))),
            token=rng.randrange(256),
            old=[rng.randrange(256), rng.randrange(256), primary_phase(rng)],
        )
        value["request"][4:6] = value["source"].to_bytes(2, "little")
    elif mode == CLEAR_PRIMARY:
        instance_chain(rng, value)
        value["timelines"] = [rng.randrange(2) for _ in range(INSTANCE_RECORDS)]
        value["moved"] = rng.choice((NULL, rng.randrange(INSTANCE_RECORDS)))
    elif mode in SPAWN_MODES:
        spawn_vector(rng, value)
    elif mode == SOURCE_ACTOR:
        value.update(
            actor=rng.randrange(256),
            beside=list(rng.randbytes(3)),
            stack=list(rng.randbytes(SOURCE_FRAME)),
        )
    elif mode == CLEAR_MODE:
        value.update(
            variant=rng.choice((0, 1, 0xFF, rng.randrange(1, 256))),
            beside=rng.randrange(256),
        )
    return value


TARGETS = 16
TARGET_TYPES = 0x10
TARGET_STRIDE = 0xC
SOURCE_ENDPOINT = REQUEST_RECORD + 4
SOURCE_FRAME = 0x100
# Effects either side of each EmitsTowardSource range, 54's busy bits and the source actor effect.
EFFECT_IDS = (22, 23, 24, 25, 26, 39, 40, 41, 42, 43, 44, 49, 52, 53, 54, 54, 0x38)
TOWARD_SOURCE = {23, 25, 40, 41, *range(44, 54)}
BUSIEST = 54


def spawn_vector(rng: random.Random, value: dict) -> None:
    effect = rng.choice((*EFFECT_IDS, rng.randrange(1 << 16)))
    count = rng.choice((0, 1, 1, 2, 3, TARGETS, rng.randrange(TARGETS + 1)))
    request = list(rng.randbytes(REQUEST_BYTES))
    request[0:2] = effect.to_bytes(2, "little")
    request[2] = count
    value.update(
        request=request,
        effect=effect,
        count=count,
        parameter=s32(rng.randrange(1 << 32)),
        unused=rng.randrange(1 << 32),
        token=rng.choice((rng.randrange(256), rng.randrange(1 << 32))),
        types=[
            int.from_bytes(request[at : at + 2], "little")
            for at in range(TARGET_TYPES, TARGET_TYPES + TARGETS * TARGET_STRIDE, TARGET_STRIDE)
        ],
        created=[rng.randrange(INSTANCE_RECORDS) for _ in range(TARGETS)],
        busy=[rng.randrange(1 << 32) for _ in range(INSTANCE_RECORDS)],
        parameters=[s32(rng.randrange(1 << 32)) for _ in range(INSTANCE_RECORDS)],
        effects=[rng.randrange(1 << 16) for _ in range(INSTANCE_RECORDS)],
        phases=[rng.randrange(1 << 16) for _ in range(INSTANCE_RECORDS)],
    )


class Stubs:
    """The stubbed callees and the calls they record."""

    def __init__(self, machine: Machine) -> None:
        self.machine = machine
        self.calls: list[list[int]] = []

    def record(
        self, address: int, kind: int, *, result=None, arguments=(), constants=()
    ) -> None:
        """Stubs `address`: records [kind, constants..., each converted argument register...] and
        returns `result` (a value, or a function of the machine) in $v0."""

        def handler(machine: Machine) -> None:
            words = [convert(machine.r[register]) for register, convert in arguments]
            self.calls.append(call(kind, *constants, *words))
            if result is not None:
                machine.r[2] = result(machine) if callable(result) else result

        self.machine.stub(address, handler)

    def unlinking(self, address: int, kind: int, head: int) -> None:
        """Stubs a destruction: records the instance and unlinks it from the chain at `head`."""

        def handler(machine: Machine) -> None:
            target = machine.r[4]
            self.calls.append(call(kind, ref(target)))
            link, at = head, machine.load(head)
            while at not in (0, target):
                link, at = at + NEXT, machine.load(at + NEXT)
            if at == target:
                machine.put(link, machine.load(target + NEXT))

        self.machine.stub(address, handler)


def put_chain(machine: Machine, value: dict, head: int) -> None:
    machine.map_zero(INSTANCES, INSTANCE_RECORDS * INSTANCE_STRIDE)
    machine.put(head, instance_address(value["head"]))
    for index in range(INSTANCE_RECORDS):
        machine.put(instance(index) + NEXT, instance_address(value["next"][index]))


def put_package(machine: Machine, value: dict) -> None:
    machine.map_zero(PACKAGE, 0x100)
    machine.map_zero(BLOCK9, 0x100)
    machine.put(PACKAGE_POINTER, PACKAGE)
    machine.put(PACKAGE + CURVES, value.get("curves", 0))
    machine.put(PACKAGE + SCRIPT_BLOCK, BLOCK8)
    machine.put(PACKAGE + TRACK_BLOCK, BLOCK9)
    machine.put(BLOCK9 + TRACK_FRAMES, value.get("tracks", 0), 1)
    machine.put(PACKAGE + SFX_BANK, value.get("sfx", 0))
    machine.put(PACKAGE + STREAM, value.get("stream", 0))
    machine.put(PACKAGE + STREAM_AUX, value.get("aux", 0))


def frames(machine: Machine) -> list[int]:
    return [
        machine.load(instance(index) + FRAME, 2) for index in range(INSTANCE_RECORDS)
    ]


INSTANCE_FRAMES = [f"instance {index} frame" for index in range(INSTANCE_RECORDS)]


def run_init(machine: Machine, value: dict, entry: int, stubs: Stubs) -> tuple:
    stubs.record(RESET_INSTANCES, RESET_CALL)
    stubs.record(INIT_RESOURCES, RESOURCES_CALL)
    stubs.record(RESET_POOL_REFERENCES, POOL_REFERENCES_CALL)
    machine.execute(entry)
    return [], [], []


RESET_FIELDS = (
    (PHASE, 4, "phase"),
    (STARTED, 4, "started"),
    (TRACK_COUNT, 4, "track count"),
    (LOADED, 1, "loaded"),
    (TOKEN, 1, "token"),
    (SECONDARY_HEAD, 4, "secondary first"),
    (CURRENT, 4, "current"),
    (UNUSED, 4, "unused word"),
    (PRIMARY_HEAD, 4, "primary first"),
    (CURVE_SAMPLE, 4, "first curve sample"),
    (VARIANT, 1, "variant"),
    (MENU_SLOT, 4, "menu reload"),
    *((TOKENS + word * 4, 4, f"token word {word}") for word in range(TOKEN_WORDS)),
    (PENDING, 4, "pending"),
)
RESET_POINTERS = {SECONDARY_HEAD, CURRENT, PRIMARY_HEAD, MENU_SLOT}


def run_reset(machine: Machine, value: dict, entry: int, stubs: Stubs) -> tuple:
    for (address, size, _), word in zip(RESET_FIELDS, value["globals"], strict=True):
        if address in RESET_POINTERS:
            word = DISC_SLOT if word == DISC_REF else instance_address(word)
        machine.put(address, word, size)
    machine.follow.add(SET_PHASE)
    machine.execute(entry)
    values = []
    for address, size, _ in RESET_FIELDS:
        word = machine.load(address, size)
        values.append(
            ref(word) if address in RESET_POINTERS else s32(word) if size == 4 else word
        )
    return values, [name for _, _, name in RESET_FIELDS], value["globals"]


def primary_stubs(machine: Machine, value: dict, stubs: Stubs) -> None:
    for which, (address, busy) in enumerate(zip(POLLS, value["polls"], strict=True)):
        stubs.record(address, POLL_CALL, result=busy, constants=(which,))
    stubs.record(WAIT, WAIT_CALL)
    stubs.record(CONFIGURE, CONFIGURE_CALL)
    stubs.record(RELEASE_WORKSPACE, WORKSPACE_CALL, arguments=((4, s32),))
    stubs.record(BIND, BIND_CALL, arguments=((4, lambda word: int(word == BUFFER)),))
    stubs.record(MENU_SECTOR, MENU_SECTOR_CALL, result=value["sector"])

    def allocate_disc(machine: Machine) -> None:
        file = machine.r[4]
        named = machine.load(file) == value["sector"] and machine.load(file + 4) == 0
        stubs.calls.append(call(ALLOCATE_DISC_CALL, int(named)))
        machine.r[2] = DISC_SLOT

    machine.stub(ALLOCATE_DISC, allocate_disc)
    stubs.record(ENQUEUE, ENQUEUE_CALL, arguments=((4, ref), (5, s32)))
    stubs.record(SAMPLE, SAMPLE_CALL, arguments=((4, lambda word: word & 0xFFFF),))
    stubs.record(PARTICLES, PARTICLES_CALL)
    stubs.record(
        RUN_SCRIPT,
        SCRIPT_CALL,
        result=lambda machine: value["scripts"][ref(machine.r[4])],
        arguments=((4, ref),),
    )
    stubs.unlinking(DESTROY_PRIMARY, DESTROY_PRIMARY_CALL, PRIMARY_HEAD)
    stubs.record(UNBIND_SFX, UNBIND_SFX_CALL)
    stubs.record(UNBIND_STREAM, UNBIND_STREAM_CALL)
    stubs.record(STOP_VIBRATION, VIBRATION_CALL, arguments=((4, s32), (5, s32)))
    stubs.record(RELEASE_PACKAGE, RELEASE_PACKAGE_CALL)
    stubs.record(RELEASE_POOL, RELEASE_POOL_CALL)
    stubs.record(RELEASE_TOKEN, RELEASE_TOKEN_CALL, arguments=((4, s32),))
    stubs.record(RESET_CAMERA, RESET_CAMERA_CALL)
    stubs.record(RELEASE_DISC, RELEASE_DISC_CALL, arguments=((4, ref),))
    machine.follow.add(SET_PHASE)


PRIMARY_NAMES = [
    "result",
    "phase",
    "track count",
    "started",
    "pending",
    "loaded",
    "token",
    "menu reload",
    "purge overlay",
    "first",
    "current",
    *INSTANCE_FRAMES,
    "sfx bank",
    "stream",
    "stream aux",
]


def run_update_primary(
    machine: Machine, value: dict, entry: int, stubs: Stubs
) -> tuple:
    put_chain(machine, value, PRIMARY_HEAD)
    put_package(machine, value)
    machine.map_zero(DISC_SLOT, 0x10)
    machine.put(PHASE, value["phase"])
    machine.put(TRACK_COUNT, value["track_count"])
    machine.put(STARTED, value["started"])
    machine.put(PENDING, value["pending"])
    machine.put(LOADED, value["loaded"], 1)
    machine.put(TOKEN, value["token"], 1)
    machine.put(MENU_SLOT, DISC_SLOT if value["menu"] == DISC_REF else 0)
    machine.put(DISC_SLOT, value["disc_state"], 2)
    machine.put(PURGE_LOADED, value["purge"])
    machine.put(REQUEST, value["effect"], 2)
    machine.put(PACKAGE_FILE, value["lba"], 2)
    machine.put(PACKAGE_BUFFER, BUFFER)
    machine.put(CURRENT, instance_address(value["current"]))
    for index in range(INSTANCE_RECORDS):
        machine.put(instance(index) + FRAME, value["frames"][index], 2)
    primary_stubs(machine, value, stubs)
    machine.execute(entry)
    values = [
        s32(machine.r[2]),
        machine.load(PHASE),
        s32(machine.load(TRACK_COUNT)),
        s32(machine.load(STARTED)),
        machine.load(PENDING),
        machine.load(LOADED, 1),
        machine.load(TOKEN, 1),
        ref(machine.load(MENU_SLOT)),
        machine.load(PURGE_LOADED),
        ref(machine.load(PRIMARY_HEAD)),
        ref(machine.load(CURRENT)),
        *frames(machine),
        machine.load(PACKAGE + SFX_BANK),
        machine.load(PACKAGE + STREAM),
        machine.load(PACKAGE + STREAM_AUX),
    ]
    inputs = [
        value["phase"],
        value["track_count"],
        value["started"],
        value["pending"],
        value["loaded"],
        value["token"],
        value["menu"],
        value["disc_state"],
        value["purge"],
        value["effect"],
        value["lba"],
        value["tracks"],
        *value["polls"],
        value["head"],
        *value["next"],
        value["current"],
        *value["frames"],
        *(int(script != 0) for script in value["scripts"]),
        int(value["curves"] != 0),
        value["sfx"],
        value["stream"],
        value["aux"],
    ]
    return values, PRIMARY_NAMES, inputs


def slot_index(owner: int, position: int) -> int:
    return owner * SLOTS_PER_INSTANCE + position


def run_update_secondary(
    machine: Machine, value: dict, entry: int, stubs: Stubs
) -> tuple:
    put_chain(machine, value, SECONDARY_HEAD)
    put_package(machine, value)
    machine.map_zero(SLOTS, SLOT_RECORDS * SLOT_STRIDE)
    machine.put(CURRENT, instance_address(value["current"]))
    for index in range(INSTANCE_RECORDS):
        base = instance(index)
        machine.put(base + BUSY_WORD, value["busy"][index])
        machine.put(base + FRAME, value["frames"][index], 2)
        machine.put(base + SECONDARY_PHASE, value["phases"][index], 2)
        held = value["slots"][index]
        machine.put(base + RENDERER_HEAD, slot(slot_index(index, 0)) if held else 0)
        for position in range(held):
            at = slot(slot_index(index, position))
            machine.put(
                at + NEXT,
                slot(slot_index(index, position + 1)) if position + 1 < held else 0,
            )
            machine.put(at + 4, renderer(slot_index(index, position)))
    for index in range(SLOT_RECORDS):
        update = value["updates"][index]
        stubs.record(
            renderer(index),
            RENDERER_CALL,
            result=update,
            arguments=((4, ref), (5, s32), (6, s32)),
        )
    stubs.record(ACQUIRE_POOL, ACQUIRE_POOL_CALL, arguments=((4, s32),))
    stubs.record(SAMPLE, SAMPLE_CALL, arguments=((4, lambda word: word & 0xFFFF),))
    stubs.record(PARTICLES, PARTICLES_CALL)
    stubs.record(DETACH, DETACH_CALL, arguments=((4, ref), (5, ref)))
    stubs.record(
        RUN_SCRIPT,
        SCRIPT_CALL,
        result=lambda machine: value["scripts"][ref(machine.r[4])],
        arguments=((4, ref),),
    )
    stubs.record(DESTROY_SECONDARY, DESTROY_SECONDARY_CALL, arguments=((4, ref),))
    stubs.record(RELEASE_POOL, RELEASE_POOL_CALL)
    machine.follow.add(SET_SECONDARY_PHASE)
    machine.execute(entry)
    values = [
        s32(machine.r[2]),
        ref(machine.load(CURRENT)),
        *(
            machine.load(instance(index) + SECONDARY_PHASE, 2, True)
            for index in range(INSTANCE_RECORDS)
        ),
        *frames(machine),
    ]
    names = [
        "result",
        "current",
        *(f"instance {index} phase" for index in range(INSTANCE_RECORDS)),
        *INSTANCE_FRAMES,
    ]
    inputs = [
        value["head"],
        *value["next"],
        value["current"],
        int(value["curves"] != 0),
        *value["phases"],
        *((value["busy"][index] >> 8) & 7 for index in range(INSTANCE_RECORDS)),
        *value["frames"],
        *(int(script != 0) for script in value["scripts"]),
        *value["slots"],
        *(int(update != 0) for update in value["updates"]),
    ]
    return values, names, inputs


SPAWN_WORDS = {
    0x00: REQUEST + 4,
    0x04: REQUEST + 4,
    0x08: REQUEST + 0x10,
    0x0C: REQUEST + 0x10,
    0x10: 0,
    0x14: BLOCK8,
    0x1C: 0,
}


def run_start_track(machine: Machine, value: dict, entry: int, stubs: Stubs) -> tuple:
    put_package(machine, value)
    machine.put(PHASE, value["phase"])
    machine.put(TRACK_COUNT, value["track_count"])
    machine.put(STARTED, value["started"])
    machine.put(PENDING, value["pending"])
    machine.put(TOKEN, value["token"], 1)
    stubs.record(SELECT, SELECT_CALL, arguments=((4, s32),))

    def create(machine: Machine) -> None:
        spawn = machine.r[4]
        shaped = all(
            machine.load(spawn + offset) == word for offset, word in SPAWN_WORDS.items()
        )
        stubs.calls.append(
            call(
                CREATE_CALL,
                s32(machine.load(spawn + 0x18)),
                machine.load(spawn + 0x20, 1),
                int(shaped),
            )
        )
        machine.r[2] = instance(0)

    machine.stub(CREATE_PRIMARY, create)
    machine.follow.update((CAN_START, SET_PHASE))
    machine.r[4] = value["track"]
    machine.execute(entry)
    values = [
        s32(machine.r[2]),
        machine.load(PHASE),
        s32(machine.load(STARTED)),
        machine.load(PENDING),
    ]
    inputs = [
        value["track"],
        value["phase"],
        value["track_count"],
        value["started"],
        value["pending"],
        value["token"],
    ]
    return values, ["result", "phase", "started", "pending"], inputs


def run_update_runtime(
    machine: Machine, value: dict, entry: int, stubs: Stubs
) -> tuple:
    machine.put(BUFFER_INDEX, value["buffer"], 1)
    machine.put(PHASE, value["old_phase"])
    stubs.record(SELECT, SELECT_CALL, arguments=((4, s32),))

    def update_primary(machine: Machine) -> None:
        stubs.calls.append(call(UPDATE_PRIMARY_CALL))
        machine.put(PHASE, value["phase"])
        machine.r[2] = value["primary"]

    machine.stub(UPDATE_PRIMARY_ADDRESS, update_primary)
    stubs.record(
        UPDATE_SECONDARY_ADDRESS, UPDATE_SECONDARY_CALL, result=value["secondary"]
    )
    stubs.record(CAMERA_MOTION, CAMERA_MOTION_CALL)
    machine.execute(entry)
    values = [s32(machine.r[2]), machine.load(BUFFER_INDEX, 1)]
    inputs = [value["buffer"], value["primary"], value["phase"], value["secondary"]]
    return values, ["result", "buffer index"], inputs


def run_begin_action(machine: Machine, value: dict, entry: int, stubs: Stubs) -> tuple:
    machine.map_zero(REQUEST_RECORD, 0x100)
    machine.store(REQUEST_RECORD, bytes(value["request"]))
    variant, argument, phase = value["old"]
    machine.put(VARIANT, variant, 1)
    machine.put(VARIANT_ARGUMENT, argument, 1)
    machine.put(PHASE, phase)
    stubs.record(ALLOCATE_TOKEN, ALLOCATE_TOKEN_CALL, result=value["token"])
    stubs.record(SELECT, SELECT_CALL, arguments=((4, s32),))
    cameras = {SAVED_CAMERA: 0, EFFECT_CAMERA: 1}
    stubs.record(
        CAPTURE, CAPTURE_CALL, arguments=((4, lambda word: cameras.get(word, OTHER)),)
    )
    machine.follow.add(SET_PHASE)
    machine.r[4], machine.r[5], machine.r[6] = (
        REQUEST_RECORD,
        value["variant"],
        value["argument"],
    )
    machine.execute(entry)
    stored = machine.bytes(REQUEST, REQUEST_BYTES) == bytes(value["request"])
    values = [
        s32(machine.r[2]),
        int(stored),
        machine.load(VARIANT, 1),
        machine.load(VARIANT_ARGUMENT, 1),
        machine.load(TOKEN, 1),
        machine.load(PHASE),
    ]
    names = ["result", "stored", "variant", "variant argument", "token", "phase"]
    inputs = [
        value["variant"],
        value["argument"],
        value["source"],
        value["token"],
        *value["old"],
    ]
    return values, names, inputs


def run_clear_primary(machine: Machine, value: dict, entry: int, stubs: Stubs) -> tuple:
    put_chain(machine, value, PRIMARY_HEAD)
    for index in range(INSTANCE_RECORDS):
        machine.put(
            instance(index) + TIMELINE,
            timeline(index) if value["timelines"][index] else 0,
        )

    def stop_vibration(machine: Machine) -> None:
        stubs.calls.append(call(VIBRATION_CALL, s32(machine.r[4]), s32(machine.r[5])))
        machine.put(PRIMARY_HEAD, instance_address(value["moved"]))

    machine.stub(STOP_VIBRATION, stop_vibration)
    stubs.record(FREE_HIGH, FREE_CALL, arguments=((4, ref),))
    stubs.record(DESTROY_PRIMARY, DESTROY_PRIMARY_CALL, arguments=((4, ref),))
    machine.execute(entry)
    values = [
        ref(machine.load(instance(index) + TIMELINE))
        for index in range(INSTANCE_RECORDS)
    ]
    names = [f"instance {index} timeline" for index in range(INSTANCE_RECORDS)]
    inputs = [value["head"], *value["next"], *value["timelines"], value["moved"]]
    return values, names, inputs


SPAWN_FIELDS = (
    (PARAMETER, 4, True, "parameter"),
    (EFFECT_ID, 2, False, "effect"),
    (BUSY_WORD, 4, False, "busy word"),
    (SECONDARY_PHASE, 2, False, "phase"),
)


def spawned(machine: Machine, spawn: int, value: dict) -> tuple:
    """The two calls recording the spawn record at `spawn` and the instance created for it."""
    target_at = machine.load(spawn + 8)
    offset = target_at - (REQUEST_RECORD + TARGET_TYPES)
    target = (
        offset // TARGET_STRIDE
        if 0 <= offset < TARGETS * TARGET_STRIDE and offset % TARGET_STRIDE == 0
        else OTHER
    )
    endpoints = (machine.load(spawn + 4), machine.load(spawn + 0xC))
    toward = {(target_at, SOURCE_ENDPOINT): 1, (SOURCE_ENDPOINT, target_at): 0}
    shaped = (
        machine.load(spawn) == SOURCE_ENDPOINT
        and machine.load(spawn + 0x14) == BLOCK8
        and machine.load(spawn + 0x1C) == 0
    )
    return (
        call(SECONDARY_CALL, target, toward.get(endpoints, OTHER), machine.load(spawn + 0x10)),
        call(
            SECONDARY_SCRIPT_CALL,
            s32(machine.load(spawn + 0x18)),
            machine.load(spawn + 0x20, 1),
            int(shaped),
        ),
        instance(value["created"][target] if target != OTHER else 0),
    )


def run_spawn(machine: Machine, value: dict, entry: int, stubs: Stubs) -> tuple:
    put_package(machine, value)
    machine.map_zero(REQUEST_RECORD, 0x100)
    machine.store(REQUEST_RECORD, bytes(value["request"]))
    machine.map_zero(INSTANCES, INSTANCE_RECORDS * INSTANCE_STRIDE)
    for index in range(INSTANCE_RECORDS):
        for (offset, size, _, _), initial in zip(
            SPAWN_FIELDS,
            (value["parameters"], value["effects"], value["busy"], value["phases"]),
            strict=True,
        ):
            machine.put(instance(index) + offset, initial[index], size)
    stubs.record(ALLOCATE_TOKEN, ALLOCATE_TOKEN_CALL, result=value["token"])
    stubs.record(SELECT, SELECT_CALL, arguments=((4, s32),))
    cameras = {SAVED_CAMERA: 0, EFFECT_CAMERA: 1}
    stubs.record(
        CAPTURE, CAPTURE_CALL, arguments=((4, lambda word: cameras.get(word, OTHER)),)
    )

    def create(machine: Machine) -> None:
        shape, script, created = spawned(machine, machine.r[4], value)
        stubs.calls += [shape, script]
        machine.r[2] = created

    machine.stub(CREATE_SECONDARY, create)
    machine.follow.update((SPAWN_SECONDARY_ADDRESS, SET_SECONDARY_PHASE))
    machine.r[4] = REQUEST_RECORD
    if value["mode"] == BEGIN_SECONDARY:
        machine.r[5] = value["parameter"]
    else:
        machine.r[5], machine.r[6] = value["unused"], value["parameter"]
    machine.execute(entry)
    values = [
        s32(machine.r[2]),
        int(machine.bytes(SECONDARY_REQUEST, REQUEST_BYTES) == bytes(value["request"])),
    ]
    names = ["result", "stored"]
    for offset, size, signed, name in SPAWN_FIELDS:
        values += [
            machine.load(instance(index) + offset, size, signed)
            for index in range(INSTANCE_RECORDS)
        ]
        names += [f"instance {index} {name}" for index in range(INSTANCE_RECORDS)]
    inputs = [
        value["effect"],
        value["count"],
        value["parameter"],
        value["token"],
        *value["types"],
        *value["created"],
        *value["busy"],
        *value["parameters"],
        *value["effects"],
        *value["phases"],
    ]
    return values, names, inputs


def run_source_actor(machine: Machine, value: dict, entry: int, stubs: Stubs) -> tuple:
    # Stack garbage, so a field the original leaves unwritten shows.
    machine.store(DEFAULT_STACK - SOURCE_FRAME, bytes(value["stack"]))
    machine.put(ACTING_ACTOR, value["actor"], 1)
    machine.store(ACTING_ACTOR + 1, bytes(value["beside"]))

    def begin(machine: Machine) -> None:
        request = machine.r[4]
        stubs.calls += [
            call(
                SOURCE_CALL,
                machine.load(request, 2),
                machine.load(request + 2, 1) | machine.load(request + 3, 1) << 8,
                machine.load(request + 4, 2),
            ),
            call(
                SOURCE_ENDPOINT_CALL,
                machine.load(request + 8, 1) | machine.load(request + 0xA, 1) << 8,
                machine.load(request + TARGET_TYPES, 2),
                s32(machine.r[5]),
            ),
            call(
                SOURCE_TARGET_CALL,
                *(machine.load(request + at, 2, True) for at in (0x14, 0x16, 0x18)),
            ),
        ]

    machine.stub(BEGIN_SECONDARY_ADDRESS, begin)
    machine.execute(entry)
    return [], [], [value["actor"]]


def run_clear_mode(machine: Machine, value: dict, entry: int, stubs: Stubs) -> tuple:
    machine.put(VARIANT, value["variant"], 1)
    machine.put(VARIANT + 1, value["beside"], 1)
    machine.execute(entry)
    return [machine.load(VARIANT, 1)], ["variant"], [value["variant"]]


RUNNERS = {
    INIT: run_init,
    RESET: run_reset,
    UPDATE_PRIMARY: run_update_primary,
    UPDATE_SECONDARY: run_update_secondary,
    START_TRACK: run_start_track,
    UPDATE_RUNTIME: run_update_runtime,
    BEGIN_ACTION: run_begin_action,
    CLEAR_PRIMARY: run_clear_primary,
    **dict.fromkeys(SPAWN_MODES, run_spawn),
    SOURCE_ACTOR: run_source_actor,
    CLEAR_MODE: run_clear_mode,
}


def run(machine: Machine, value: dict, entry: int) -> tuple:
    stubs = Stubs(machine)
    values, names, inputs = RUNNERS[value["mode"]](machine, value, entry, stubs)
    return recorded_observation(machine, values, names, stubs.calls), [
        value["mode"],
        *inputs,
    ]


def span(address: int, size: int = 4) -> tuple[int, int]:
    return (address, address + size)


def instance_spans(offset: int, size: int) -> list[tuple[int, int]]:
    return [span(instance(index) + offset, size) for index in range(INSTANCE_RECORDS)]


ALLOWED = {
    INIT: [],
    RESET: [
        span(address, size) for address, size, _ in RESET_FIELDS if address != PENDING
    ],
    UPDATE_PRIMARY: [
        span(PHASE),
        span(TRACK_COUNT),
        span(STARTED),
        span(PENDING),
        span(LOADED, 1),
        span(TOKEN, 1),
        span(MENU_SLOT),
        span(PURGE_LOADED),
        span(CURRENT),
        span(PACKAGE + SFX_BANK, 12),
        *instance_spans(FRAME, 2),
    ],
    UPDATE_SECONDARY: [
        span(CURRENT),
        *instance_spans(FRAME, 2),
        *instance_spans(SECONDARY_PHASE, 2),
    ],
    START_TRACK: [span(PHASE), span(STARTED), span(PENDING)],
    UPDATE_RUNTIME: [span(BUFFER_INDEX, 1)],
    BEGIN_ACTION: [
        span(REQUEST, REQUEST_BYTES),
        span(VARIANT, 1),
        span(VARIANT_ARGUMENT, 1),
        span(TOKEN, 1),
        span(PHASE),
    ],
    CLEAR_PRIMARY: instance_spans(TIMELINE, 4),
    **dict.fromkeys(
        SPAWN_MODES,
        [
            span(SECONDARY_REQUEST, REQUEST_BYTES),
            *(
                item
                for offset, size, _, _ in SPAWN_FIELDS
                for item in instance_spans(offset, size)
            ),
        ],
    ),
    SOURCE_ACTOR: [],
    CLEAR_MODE: [span(VARIANT, 1)],
}


def check(
    image, value: dict, observation: dict, native: list[int], reference
) -> list[str]:
    allowed = [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        *ALLOWED[value["mode"]],
    ]
    return printed_problems(observation, native, allowed)


def describe(image, results) -> list[str]:
    if not results:
        return []
    mode = results[0][0]["mode"]
    total = len(results)
    if mode == UPDATE_PRIMARY:
        phases: dict[int, int] = {}
        for value, _, _ in results:
            phase = value["phase"] if value["phase"] < 10 else 10
            phases[phase] = phases.get(phase, 0) + 1
        shown = ", ".join(
            f"{phase if phase < 10 else '>=10'}: {count}"
            for phase, count in sorted(phases.items())
        )
        released = sum(
            1
            for _, observation, _ in results
            if any(item[0] == RELEASE_TOKEN_CALL for item in observation["calls"])
        )
        return [f"phases {shown}; {released} of {total} vectors released the action"]
    if mode == UPDATE_SECONDARY:
        detached = sum(
            sum(1 for item in observation["calls"] if item[0] == DETACH_CALL)
            for _, observation, _ in results
        )
        ended = sum(
            sum(1 for item in observation["calls"] if item[0] == DESTROY_SECONDARY_CALL)
            for _, observation, _ in results
        )
        return [
            f"{detached} renderers detached and {ended} instances ended over {total} vectors"
        ]
    if mode == START_TRACK:
        started = sum(
            1
            for _, observation, _ in results
            if any(item[0] == CREATE_CALL for item in observation["calls"])
        )
        ready = sum(1 for _, observation, _ in results if observation["values"][0])
        return [
            f"{ready} of {total} vectors could start a track; {started} started one"
        ]
    if mode == BEGIN_ACTION:
        kept = sum(
            1
            for value, _, _ in results
            if value["variant"] != 0 and value["source"] == 4
        )
        return [f"{kept} of {total} vectors kept the variant"]
    if mode == CLEAR_PRIMARY:
        cleared = sum(
            len(walk(value["head"], value["next"])) for value, _, _ in results
        )
        return [f"{cleared} instances cleared over {total} vectors"]
    if mode in SPAWN_MODES:
        spawns = sum(
            sum(1 for item in observation["calls"] if item[0] == SECONDARY_CALL)
            for _, observation, _ in results
        )
        toward = sum(
            value["count"] for value, _, _ in results if value["effect"] in TOWARD_SOURCE
        )
        busiest = sum(
            value["count"] for value, _, _ in results if value["effect"] == BUSIEST
        )
        return [
            f"{spawns} instances spawned over {total} vectors; "
            f"{toward} toward the source, {busiest} of effect {BUSIEST}"
        ]
    return []


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
