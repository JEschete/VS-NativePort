"""Five action-sequence gates and recovery passes, including callback owner/count changes."""

import random
from dataclasses import dataclass

from differential import (
    DEFAULT_STACK,
    MASK,
    Fault,
    Image,
    Machine,
    field_problems,
    store_problems,
)

FUNCTIONS = ("F00187", "F00188", "F00192", "F00193", "F00195")
ID = "/".join(FUNCTIONS)
ENTRIES = (0x80078748, 0x800787F0, 0x80079050, 0x800790BC, 0x800793C0)
ENDS = (0x800787F0, 0x80078828, 0x800790BC, 0x800792E4, 0x800797BC)
ENTRY, END = ENTRIES[0], ENDS[0]
DRIVER = "action_sequence_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\ActionSequence.cpp",)
VECTORS, MUTANT_VECTORS, SEED = 5 * 1024, 5 * 256, 187188192193195
STATES, GLOBAL = (0x80500000, 0x80504000), 0x800F19CC
PROJECTION, DISTANCE, MODE, BUSY_FLAGS, CONTROL = (
    0x8005E248, 0x800F19D8, 0x800F19EC, 0x800F1868, 0x800F196C)
GLOBALS = (GLOBAL, PROJECTION, DISTANCE, MODE, BUSY_FLAGS, CONTROL, 0x800F1A08)
GUARDS = (0x8005E244, 0x8005E24C, 0x800F1864, 0x800F186C, 0x800F1968, 0x800F1970)
ACTORS, TARGETS, EVENT_WIDTH = (0, 1, 40), 30, 15
NOTIFICATION, FLAGS, ASSET, CAN_START, ADVANCE, ANIMATION, TRACK = (
    0x800CB45C, 0x800A0BE0, 0x800995B0, 0x800CEEBC, 0x800704B0, 0x80070278, 0x800CEF74)
PLANS, RELEASE, FADE, BUSY, DEFEAT, RECOVERY = (
    0x800DEC88, 0x80069D14, 0x8006FCBC, 0x8009E4B0, 0x80074A20, 0x8006FBCC)
APPLY_PROJECTION, LOOK, RESUME, HUD, IDLE, RESOLVE, EVENT, CUTSCENE, LOOT = (
    0x80041534, 0x80077DF0, 0x8006C40C, 0x800CB654, 0x8009E5C4,
    0x800792E4, 0x800BEC58, 0x80073898, 0x8006FB48)
ARG_COUNTS = {
    NOTIFICATION: 0, FLAGS: 1, ASSET: 0, CAN_START: 0, ADVANCE: 0,
    ANIMATION: 0, TRACK: 1, PLANS: 1, RELEASE: 0, FADE: 0, BUSY: 1,
    DEFEAT: 1, RECOVERY: 1, APPLY_PROJECTION: 1, LOOK: 0, RESUME: 0,
    HUD: 1, IDLE: 1, RESOLVE: 3, EVENT: 4, CUTSCENE: 0, LOOT: 0,
}
VALUE_NAMES = (
    "global-state", "projection", "distance", "camera-mode", "busy-flags", "control", "unrelated",
    *(f"guard-{i}" for i in range(len(GUARDS))),
    *(f"state-{s}-{field}" for s in range(2)
      for field in ("index", "chain", "selection-word", "preferred-word", "guard")),
    *(f"state-{s}-queue-{q}-{field}" for s in range(2) for q in range(4)
      for field in ("guard", "primary-word", "primary-position", "animation-count", "focus-x-y",
                    "focus-z-tag", *(f"target-{t}-{kind}" for t in range(TARGETS)
                                    for kind in ("word", "position")))),
)

MUTANTS = {
    "F00187-index-mask-one": {0x80078764: 0x30630001},
    "F00187-ignore-notification": {0x80078790: 0},
    "F00187-primary-neighbor": {0x80078798: 0x92040005},
    "F00187-wrong-high-ready-bit": {0x800787A4: 0x3C030400},
    "F00187-wrong-low-ready-bit": {0x800787A8: 0x34630200},
    "F00187-ignore-flags": {0x800787B0: 0},
    "F00187-ignore-asset": {0x800787C0: 0},
    "F00187-ignore-effect": {0x800787D0: 0},
    "F00187-no-phase-advance": {0x800787D8: 0},
    "F00188-busy-bit-one": {0x800787FC: 0x30420001},
    "F00188-ignore-busy": {0x80078800: 0},
    "F00188-no-animation": {0x80078808: 0},
    "F00188-no-track": {0x80078810: 0},
    "F00188-track-one": {0x80078814: 0x24040001},
    "F00192-index-mask-one": {0x80079068: 0x30420001},
    "F00192-request-neighbor": {0x80079088: 0x24840858},
    "F00192-inverted-result": {0x80079094: 0x10400005},
    "F00192-no-release": {0x8007909C: 0},
    "F00192-no-fade": {0x800790A4: 0},
    "F00193-first-count-index": {0x800790E8: 0x8C420000},
    "F00193-first-ring-divisor": {0x8007910C: 0x00031840},
    "F00193-ignore-world-target": {0x80079158: 0},
    "F00193-no-busy-query": {0x80079164: 0},
    "F00193-busy-without-increment": {0x8007916C: 0x24420000},
    "F00193-signed-busy-test": {0x80079170: 0x28420002},
    "F00193-forget-busy": {0x8007917C: 0x24140001},
    "F00193-first-target-only": {0x8007918C: 0},
    "F00193-first-outer-index": {0x800791A0: 0x8C420000},
    "F00193-first-request-only": {0x800791AC: 0},
    "F00193-second-ring-divisor": {0x800791E8: 0x00031840},
    "F00193-ignore-world-primary": {0x80079220: 0},
    "F00193-no-primary-defeat": {0x8007922C: 0},
    "F00193-ignore-world-defeat": {0x80079250: 0},
    "F00193-no-target-defeat": {0x8007925C: 0},
    "F00193-ignore-defeat-result": {0x80079264: 0},
    "F00193-reward-three": {0x8007926C: 0x24140003},
    "F00193-second-target-only": {0x8007927C: 0},
    "F00193-second-outer-index": {0x80079290: 0x8C420000},
    "F00193-second-request-only": {0x8007929C: 0},
    "F00193-final-primary-neighbor": {0x800792A4: 0x92620005},
    "F00193-always-no-reward": {0x800792AC: 0},
    "F00193-no-recovery": {0x800792B8: 0},
    "F00195-camera-mode-two": {0x800793CC: 0x24020002},
    "F00195-alternate-mode-four": {0x800793F0: 0x24020004},
    "F00195-restore-all-modes": {0x800793F4: 0},
    "F00195-projection-subtract63": {0x80079414: 0x2462FFC1},
    "F00195-projection-add63": {0x80079434: 0x2462003F},
    "F00195-projection-threshold514": {0x80079408: 0x28620202},
    "F00195-projection-cap511": {0x80079424: 0x240201FF},
    "F00195-no-projection-service": {0x80079450: 0},
    "F00195-unsigned-preferred": {0x8007946C: 0x94A42C0C},
    "F00195-distance-add191": {0x8007947C: 0x246200BF},
    "F00195-distance-subtract191": {0x80079498: 0x2462FF41},
    "F00195-always-distance-clamp": {0x800794AC: 0},
    "F00195-ignore-projection-gate": {0x800794C4: 0},
    "F00195-ignore-distance-gate": {0x800794E0: 0},
    "F00195-ignore-look-gate": {0x800794F0: 0},
    "F00195-busy-without-increment": {0x80079584: 0x24420000},
    "F00195-signed-busy-test": {0x80079588: 0x28420002},
    "F00195-forget-busy": {0x80079594: 0x24140001},
    "F00195-first-ready-target-only": {0x800795A4: 0},
    "F00195-no-field-resume": {0x800795D0: 0},
    "F00195-hide-hud": {0x800795DC: 0x24040001},
    "F00195-selected-neighbor": {0x800795E8: 0x9044000D},
    "F00195-no-idle": {0x800795EC: 0},
    "F00195-target-neighbor": {0x80079678: 0x9205004D},
    "F00195-kill-count-two": {0x80079680: 0x24060002},
    "F00195-no-target-resolution": {0x8007967C: 0},
    "F00195-source-count-index": {0x800796BC: 0x8C420000},
    "F00195-no-source-resolution": {0x80079724: 0},
    "F00195-source-target-zero": {0x80079728: 0x00002821},
    "F00195-control-four": {0x80079750: 0x24020004},
    "F00195-trigger-thirteen": {0x80079758: 0x2404000D},
    "F00195-event-argument-one": {0x8007975C: 0x24050001},
    "F00195-no-event-low16": {0x80079770: 0},
    "F00195-always-cutscene": {0x80079778: 0},
    "F00195-no-cutscene": {0x80079780: 0},
    "F00195-cutscene-falls-through": {0x80079788: 0},
    "F00195-no-loot": {0x80079790: 0},
}


@dataclass(frozen=True)
class Vector:
    function: int
    lane: int
    mutation: int
    services: tuple[int, ...]
    globals: tuple[int, ...]
    guards: tuple[int, ...]
    states: tuple[tuple[int, ...], ...]
    actions: tuple[tuple[int, ...], ...]

    def inputs(self) -> list[int]:
        return [self.function, self.lane, self.mutation, *self.services, *self.globals, *self.guards,
                *(value for state in self.states for value in state),
                *(value for action in self.actions for value in action)]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 5, number // 5
    mutation = (lane // 8) % (6 if function == 0 else 4 if function == 1 else
                             6 if function == 2 else 16)
    boundary = (0, 1, MASK, 0x10000, 0x10001, 0x80000000, 0x7FFFFFFF, 0xFFFFFFFE)
    pending, flags, asset, can = 0, 0x08000100, 0, boundary[1 + lane % 7]
    gate = lane % 8
    if function == 0:
        if gate == 0:
            pending = boundary[1 + lane // 8 % 7]
        elif gate in (1, 2, 3):
            flags = (0, 0x08000000, 0x100)[gate - 1]
        elif gate == 4:
            asset = boundary[1 + lane // 8 % 7]
        elif gate == 5:
            can = 0
        flags |= rng.getrandbits(32) & ~0x08000100
    plan = boundary[lane % len(boundary)]
    busy = [0, MASK, 0]
    if lane % 8 == 0:
        busy = [0xFFFFFFFE, 1, 0x80000000]
    elif lane % 8 == 1:
        busy = [0x7FFFFFFF, 0xFFFFFFFE, 1]
    defeats = [boundary[(lane + i * 3) % 8] for i in range(3)]
    if function == 3 and lane % 16 == 6:
        defeats = [0, 0, 0]
    look = (0, 1, MASK, 0x10000)[lane % 4] if gate < 2 else 1
    event = (0, 1, 0x10001, 0xFFFF0001, 2, MASK, 0x10000, 0x80000000)[lane % 8]
    services = (pending, flags, asset, can, plan, look, event, *busy, *defeats)
    camera_mode = (0, 1, 2, 3, MASK)[lane % 5]
    projection = (511, 512, 513, 447, 448, 576, 577, 0x80000000,
                  0x7FFFFFFF, 512, 512, 512)[lane % 12]
    distance = (-32768, -200, -9, 0, 100, 292, 32767, 0x80000000,
                0x7FFFFFFF)[lane % 9] & MASK
    preferred = (-32768, -9, 0, 100, 32767)[lane % 5] & 0xFFFF
    # Most finish vectors reach the loops; the remaining lanes probe camera boundaries.
    if function == 4 and gate >= 2:
        camera_mode, projection, distance, look = 0, 512, preferred, 1
        if distance & 0x8000:
            distance |= 0xFFFF0000
        services = (*services[:5], look, *services[6:])
    if function == 4 and mutation == 11:
        camera_mode, projection = 1, 513
    pointer = STATES[0]
    if function == 1 or (function == 4 and camera_mode not in (1, 3) and projection != 512):
        pointer = (0, STATES[0], 0xDEAD0020)[lane % 3]
    globals_ = (pointer, projection, distance, camera_mode, boundary[lane % 8],
                (0, 1, 3, 4)[lane % 4], rng.getrandbits(32))
    states, actions = [], []
    for state in range(2):
        chain = (1 + lane % 6) if function == 3 else lane % 7
        if state:
            chain = 1 + (lane + 2) % 6
        index = (0, 1, 2, 3, 0xFFFFFFFE, MASK)[(lane + state) % 6]
        selection = rng.getrandbits(24) << 8 | ACTORS[(lane + state) % 3]
        states.append((index, chain, selection,
                       rng.getrandbits(16) << 16 | preferred, rng.getrandbits(32)))
        for queue in range(4):
            count = (0, 1, 2, 3, 4, 30)[(lane + queue + state) % 6]
            if mutation in (2, 3, 4, 6, 7, 9, 10, 11, 13, 15) and queue == 0:
                count = 2
            actor = 0 if function == 3 and lane % 16 == 6 else ACTORS[(lane + queue + state) % 3]
            position = (0, 0, 0, 1, MASK)[(lane // 3 + queue + state) % 5]
            action = [rng.getrandbits(32), rng.getrandbits(24) << 8 | actor,
                      position, count << 16 | rng.getrandbits(16),
                      rng.getrandbits(32), rng.getrandbits(32)]
            for target in range(TARGETS):
                actor = ACTORS[(lane + target + queue) % 3]
                position = (0, 0, 0, 0, 1, 0x80000000)[(lane + target + state) % 6]
                if target == 0 and mutation != 0:
                    position = 0
                action.extend((rng.getrandbits(24) << 8 | actor, position))
            actions.append(tuple(action))
    return Vector(function, lane, mutation, services, globals_,
                  tuple(rng.getrandbits(32) for _ in GUARDS), tuple(states), tuple(actions))


def request(state: int, queue: int) -> int:
    return STATES[state] + 0x854 + queue * 0x84C


def action_fields(base: int) -> tuple[int, ...]:
    return (base, base + 4, base + 0x44, base + 0x48, base + 0x844, base + 0x848,
            *(base + 0x4C + target * 0x44 + offset
              for target in range(TARGETS) for offset in (0, 0x40)))


def state_fields(base: int) -> tuple[int, ...]:
    return base, base + 4, base + 12, base + 0x2C0C, base + 0x2C10


def run(m: Machine, v: Vector) -> tuple[dict, list[int]]:
    for address, value in zip(GLOBALS, v.globals, strict=True):
        m.put(address, value)
    for address, value in zip(GUARDS, v.guards, strict=True):
        m.put(address, value)
    for state, base in enumerate(STATES):
        m.map_zero(base, 0x3000)
        for address, value in zip(state_fields(base), v.states[state], strict=True):
            m.put(address, value)
        for queue in range(4):
            for address, value in zip(action_fields(request(state, queue)),
                                      v.actions[state * 4 + queue], strict=True):
                m.put(address, value)
    calls, counts = [], dict.fromkeys(ARG_COUNTS, 0)

    def current() -> int:
        return STATES.index(m.load(GLOBAL))

    def owner() -> int:
        return m.r[19 if v.function == 3 else 18]

    def primary(base: int, actor: int) -> None:
        m.put(base + 4, actor, 1)

    def target(base: int, index: int, actor: int) -> None:
        m.put(base + 0x4C + index * 0x44, actor, 1)
        m.put(base + 0x8C + index * 0x44, 0)

    def mutate(address: int) -> None:
        mode, first = v.mutation, counts[address] == 1
        if v.function == 0:
            queued = request(0, v.states[0][0] & 3)
            if address == NOTIFICATION:
                if mode in (1, 2, 3):
                    m.put(GLOBAL, STATES[1])
                if mode in (2, 3):
                    primary(queued, 1 if mode == 2 else 40)
            if mode == 4 and address == FLAGS:
                m.put(GLOBAL, STATES[1])
                primary(queued, 0)
            if mode == 5 and address == ASSET:
                m.put(GLOBAL, 0xDEAD0040)
        if v.function == 1 and address == ANIMATION:
            if mode in (1, 2):
                m.put(BUSY_FLAGS, 2 if mode == 1 else MASK)
            if mode == 3:
                m.put(GLOBAL, 0xDEAD0080)
        if v.function == 2:
            if address == PLANS:
                if mode in (1, 3):
                    m.put(GLOBAL, STATES[1])
                if mode == 2:
                    m.put(STATES[0], v.states[0][0] ^ 3)
                if mode == 3:
                    primary(request(1, 0), 40)
                if mode == 5 and v.services[4] != 0:
                    m.put(GLOBAL, 0xDEAD00C0)
            if mode == 4 and address == RELEASE:
                m.put(GLOBAL, STATES[1])
        if v.function not in (3, 4):
            return
        if address == BUSY and first:
            base = owner()
            if mode == 1:
                m.put(GLOBAL, STATES[1 - current()])
            if mode == 2:
                m.put(base + 0x4A, 0, 2)
            if mode in (3, 10):
                m.put(base + 0x4A, 30 if mode == 10 else 4, 2)
                target(base, 2, 1)
                target(base, 3, 40)
            if mode == 4:
                m.put(GLOBAL, STATES[1])
                m.put(STATES[1] + 4, 0)
                primary(base, 0 if v.lane % 2 == 0 else 40)
                m.put(base + 0x44, 1)
            if mode == 5:
                m.put(STATES[current()] + 4, 6)
            if mode == 11:
                primary(base, 1)
                m.put(base + 0x44, 1)
            if mode == 13:
                m.put(STATES[current()] + 4, 1)
        if address == BUSY and mode == 9:
            m.put(GLOBAL, STATES[1 - current()])
            target(owner(), 1, 40)
        if address in (DEFEAT, RESOLVE) and first:
            base = owner()
            if mode in (6, 12, 15):
                m.put(GLOBAL, STATES[1])
                m.put(base + 0x4A, 3, 2)
                primary(base, 1)
                target(base, 1, 1)
                target(base, 2, 40)
                if mode == 15:
                    m.put(STATES[1] + 4, 6)
            if mode == 7:
                m.put(base + 0x4A, 0, 2)
                primary(base, 40)
                m.put(base + 0x44, 1)
                m.put(STATES[current()] + 4, 0)
            if mode == 13:
                m.put(STATES[current()] + 4, 0)
                m.put(base + 0x4A, 3, 2)
                target(base, 1, 1)
                target(base, 2, 40)
        if address == DEFEAT and mode == 8:
            primary(owner(), 0 if v.lane % 2 == 0 else 40)
            m.put(owner() + 0x44, 1)
        if v.function == 3 and address == RECOVERY and mode == 14:
            m.put(GLOBAL, 0xDEAD0100)
            m.put(CONTROL, 3)
        if v.function == 4:
            if address == APPLY_PROJECTION and mode == 11:
                m.put(GLOBAL, STATES[1])
                m.put(DISTANCE, 300 if v.lane % 2 else -300)
                m.put(STATES[1] + 0x2C0C, 100 if v.lane % 2 else -100, 2)
                m.put(PROJECTION, 511 if v.lane % 2 else 512)
                m.put(MODE, 0)
            if mode == 10 and address == LOOK:
                m.put(GLOBAL, STATES[1])
            if mode == 12 and address == RESUME:
                m.put(GLOBAL, STATES[1])
                m.put(STATES[1] + 12, 40, 1)
            if mode == 12 and address == HUD:
                m.put(STATES[current()] + 12, 1, 1)
            if mode == 14 and address == RESOLVE:
                m.put(CONTROL, 3)
            if mode == 15 and address == EVENT:
                m.put(CONTROL, 3)

    def stub(address: int):
        def invoke(machine: Machine) -> None:
            args = list(machine.r[4:4 + ARG_COUNTS[address]])
            result = 0
            if address in (NOTIFICATION, FLAGS, ASSET, CAN_START, PLANS, LOOK, EVENT):
                result = v.services[(NOTIFICATION, FLAGS, ASSET, CAN_START, PLANS, LOOK, EVENT)
                                    .index(address)]
            elif address in (BUSY, DEFEAT):
                if args[0] not in ACTORS:
                    raise Fault(f"mutated actor argument {args[0]:#x}")
                result = v.services[(7 if address == BUSY else 10) + ACTORS.index(args[0])]
            counts[address] += 1
            calls.extend([address, len(args), *args, *([0] * (4 - len(args))), result,
                          *(machine.load(a) for a in
                            (GLOBAL, PROJECTION, DISTANCE, MODE, CONTROL)),
                          *(machine.load(base + 4) for base in STATES), machine.load(BUSY_FLAGS)])
            mutate(address)
            for register in (*range(2, 16), 24, 25):
                machine.r[register] = 0xC1000000 | counts[address] << 8 | register
            machine.r[2] = result
        return invoke

    for address in ARG_COUNTS:
        m.stub(address, stub(address))
    m.stores.clear()
    m.execute(ENTRIES[v.function], limit=30000)
    values = [m.load(address) for address in (*GLOBALS, *GUARDS)]
    values.extend(m.load(address) for base in STATES for address in state_fields(base))
    values.extend(m.load(address) for state in range(2) for queue in range(4)
                  for address in action_fields(request(state, queue)))
    return {"values": values, "calls": calls, "stores": list(m.stores)}, v.inputs()


def check(image: Image, v: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    fields = len(VALUE_NAMES) + 1
    if len(native) < fields + 1:
        return ["truncated native output"]
    problems = field_problems(("native-diagnostics", *VALUE_NAMES), native[:fields],
                              [0, *observation["values"]])
    if native[fields] != len(native) - fields - 1:
        return [*problems, "malformed native call count"]
    actual, expected = native[fields + 1:], observation["calls"]
    if actual != expected:
        first = next((i for i, (a, b) in enumerate(zip(actual, expected, strict=False)) if a != b),
                     min(len(actual), len(expected)))
        first = first // EVENT_WIDTH * EVENT_WIDTH
        problems.append(f"events at {first // EVENT_WIDTH}: native "
                        f"{actual[first:first + EVENT_WIDTH]} original "
                        f"{expected[first:first + EVENT_WIDTH]}")
    allowed = [(DEFAULT_STACK - 0x30, DEFAULT_STACK)]
    if v.function == 4:
        allowed.extend(((PROJECTION, PROJECTION + 4), (DISTANCE, DISTANCE + 4)))
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    lines = ["VOID0 original ABIs; no incidental return registers compared.",
             "F00193 initially empty undefined domain excluded; native invalid-phase refusal tested separately.",
             "Original30-target layout and actor0/1/40 services; native255-target/42-slot extension separate."]
    for function, (name, entry, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, o, n) for v, o, n in results if v.function == function]
        lines.append(f"{name} [{entry:08X},{end:08X}) {(end - entry) // 4} instructions "
                     f"sha256={image.sha256(entry, end)} vectors={len(selected)} "
                     f"mismatches={sum(bool(check(image, v, o, n, o)) for v, o, n in selected)} "
                     f"callbacks={sorted({v.mutation for v, _, _ in selected})}")
    return lines
