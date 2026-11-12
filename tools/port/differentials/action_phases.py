"""Seven action-phase policies: state writes, service arguments and callback ownership."""

import random
from dataclasses import dataclass

from differential import (
    DEFAULT_STACK,
    MASK,
    Image,
    Machine,
    field_problems,
    store_problems,
)

FUNCTIONS = ("F00001", "F00089", "F00096", "F00097", "F00098", "F00114", "F00121")
ID = "/".join(FUNCTIONS)
ENTRIES = (0x80069D14, 0x8006F5FC, 0x8006FBCC, 0x8006FCBC, 0x8006FD0C, 0x80072B9C, 0x800734C0)
ENDS = (0x80069D78, 0x8006F630, 0x8006FCBC, 0x8006FD0C, 0x8006FD1C, 0x80072BA8, 0x8007350C)
ENTRY, END = ENTRIES[0], ENDS[0]
DRIVER = "action_phases_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\ActionPhases.cpp",)
VECTORS, SEED = 7 * 1024, 1969798114121

ACTORS, GLOBAL = 0x800F1928, 0x800F19CC
PHASE, REWARD_MODE, TICK, CONTROL, UNRELATED = (
    0x800F18F0, 0x800F18FC, 0x8005E24C, 0x800F196C, 0x800F1A08)
STATES, ACTOR_RECORDS = (0x80500000, 0x80504000), 0x80600000
GUARDS = (0x800F18EC, 0x800F18F4, 0x800F18F8, 0x800F1900, 0x800F1970,
          0x800F19C8, 0x800F19D0, 0x8005E248, 0x8005E250)
SLOTS = 17  # Slot 16 is an untouched sentinel; slot 17 aliases Field_controlMode.
IDLE, FLAGS, PALETTE, FREEZE, CLUT, ROOM, MARKERS, BONE, FADE, WEAPON = (
    0x8009E5C4, 0x800A0BE0, 0x8007B10C, 0x8006C250, 0x80048FEC,
    0x8008B4BC, 0x80095B70, 0x800A152C, 0x8006C4A4, 0x8007C0AC)
ARG_COUNTS = {IDLE: 1, FLAGS: 1, PALETTE: 5, FREEZE: 0, CLUT: 1,
              ROOM: 1, MARKERS: 1, BONE: 3, FADE: 2, WEAPON: 2}
EVENT_WIDTH = 15
VALUE_NAMES = (
    "phase", "reward", "tick", "control", "global-state", "unrelated",
    *(f"guard-{i}" for i in range(len(GUARDS))),
    *(f"state-{s}-{field}" for s in range(2)
      for field in ("index", "primary-word", "queue0-word", "indexed-queue-word", "guard")),
    *(f"actor-{slot}-word-{word}" for slot in range(SLOTS) for word in range(8)),
    *(f"flag-reads-{slot}" for slot in range(SLOTS)),
)

MUTANTS = {
    "F00001-queued-127": {0x80069D18: 0x2408007F},
    "F00001-opcode-25": {0x80069D1C: 0x24070019},
    "F00001-release-9": {0x80069D20: 0x24060009},
    "F00001-ignore-state": {0x80069D44: 0},
    "F00001-ignore-opcode": {0x80069D54: 0},
    "F00001-opcode-store": {0x80069D5C: 0xA0660009},
    "F00001-wide-store": {0x80069D5C: 0xAC660008},
    "F00001-fifteen-slots": {0x80069D64: 0x28A2000F},
    "F00001-seventeen-slots": {0x80069D64: 0x28A20011},
    "F00089-mode-11": {0x8006F604: 0x2403000B},
    "F00089-wrong-control-word": {0x8006F608: 0xAC431970},
    "F00089-idle-actor-1": {0x8006F610: 0x24040001},
    "F00089-no-idle": {0x8006F618: 0},
    "F00089-no-phase-store": {0x8006F61C: 0},
    "F00096-selection-neighbor": {0x8006FBFC: 0x9042000D},
    "F00096-no-source-exclusion": {0x8006FC04: 0},
    "F00096-wrong-animation-bit": {0x8006FBEC: 0x3C110080},
    "F00096-ignore-animation": {0x8006FC18: 0},
    "F00096-no-second-flags-read": {0x8006FC20: 0},
    "F00096-wrong-presence-bit": {0x8006FC28: 0x30420002},
    "F00096-ignore-presence": {0x8006FC2C: 0},
    "F00096-no-idle": {0x8006FC34: 0},
    "F00096-fifteen-slots": {0x8006FC40: 0x2A02000F},
    "F00096-seventeen-slots": {0x8006FC40: 0x2A020011},
    "F00096-palette-mode-9": {0x8006FC48: 0x24040009},
    "F00096-palette-duration-5": {0x8006FC4C: 0x24050005},
    "F00096-palette-red-1": {0x8006FC50: 0x24060001},
    "F00096-palette-stack-reward": {0x8006FC5C: 0xAFB30010},
    "F00096-no-freeze": {0x8006FC60: 0},
    "F00096-service-order": {0x8006FC68: 0x0C022D2F},
    "F00096-clut-disabled": {0x8006FC6C: 0x24040000},
    "F00096-room-paused": {0x8006FC74: 0x24040001},
    "F00096-markers-hidden": {0x8006FC7C: 0x24040001},
    "F00096-reward-truncated": {0x8006FC88: 0xA45318FC},
    "F00096-tick-3": {0x8006FC8C: 0x24020003},
    "F00096-wrong-tick-word": {0x8006FC90: 0xAC62E250},
    "F00096-phase-10": {0x8006FCAC: 0x2402000A},
    "F00097-bone-tag-239": {0x8006FCC0: 0x240500EF},
    "F00097-phase-9": {0x8006FCD4: 0x24020009},
    "F00097-selection-not-queue0": {0x8006FCE0: 0x26100008},
    "F00097-first-actor-neighbor": {0x8006FCE4: 0x92040005},
    "F00097-bone-mode-1": {0x8006FCEC: 0x24060001},
    "F00097-no-actor-reread": {0x8006FCF0: 0},
    "F00097-bone-byte-only": {0x8006FCF8: 0x304500FF},
    "F00097-no-fade": {0x8006FCF4: 0},
    "F00098-phase-8": {0x8006FD10: 0x24020008},
    "F00098-no-store": {0x8006FD18: 0},
    "F00098-wrong-word": {0x8006FD18: 0xAC6218F4},
    "F00114-no-store": {0x80072BA4: 0},
    "F00114-wrong-word": {0x80072BA4: 0xAC4018F4},
    "F00114-halfword-only": {0x80072BA4: 0xA44018F0},
    "F00121-no-idle": {0x800734D0: 0},
    "F00121-fifteen-slots": {0x800734DC: 0x2A02000F},
    "F00121-seventeen-slots": {0x800734DC: 0x2A020011},
    "F00121-wrong-actor": {0x800734E4: 0x24040001},
    "F00121-mode-3": {0x800734EC: 0x24050003},
    "F00121-no-weapon-mode": {0x800734E8: 0},
    "F00121-control-11": {0x800734FC: 0x2402000B},
}


@dataclass(frozen=True)
class Vector:
    function: int
    lane: int
    mutation: int
    reward: int
    bone: int
    globals: tuple[int, ...]
    guards: tuple[int, ...]
    states: tuple[tuple[int, ...], ...]
    actors: tuple[tuple[int, ...], ...]

    def inputs(self) -> list[int]:
        return [self.function, self.lane, self.mutation, self.reward, self.bone,
                *self.globals, *self.guards, *(x for state in self.states for x in state),
                *(x for actor in self.actors for x in actor)]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % len(FUNCTIONS), number // len(FUNCTIONS)
    mutations = ((0,), (0, 1, 2), tuple(range(9)), tuple(range(9)),
                 (0,), (0,), (0, 1, 2, 3))
    mutation = mutations[function][lane % len(mutations[function])]
    boundary = (0, 1, 255, 256, MASK, 0x80000000, 0x7FFFFFFF, 0xFFFF8000)
    reward = boundary[lane % len(boundary)] if lane < 128 else rng.getrandbits(32)
    bone = boundary[(lane // 3) % len(boundary)] if lane < 128 else rng.getrandbits(32)
    phase = (0, 9, 10, 11, MASK, 0x80000000, rng.getrandbits(32))[lane % 7]
    global_state = STATES[0] if function in (2, 3) else (0, STATES[0], 0xDEAD0020)[lane % 3]
    globals_ = (phase, rng.getrandbits(32), rng.getrandbits(32),
                rng.getrandbits(32), global_state, rng.getrandbits(32))
    states = []
    for state in range(2):
        primary = (0, 1, 14, 15, 16, 127, 128, 255)[(lane + state * 3) % 8]
        queued = (0, 15, 16, 127, 128, 255)[(lane + state * 2) % 6]
        states.append((lane % 3 + 1, rng.getrandbits(24) << 8 | primary,
                       rng.getrandbits(24) << 8 | queued,
                       rng.getrandbits(24) << 8 | ((queued + 37) & 255), rng.getrandbits(32)))
    actors = []
    flag_pairs = ((0, 1), (1, 0), (0x01000000, 1), (0, 0),
                  (1, 0x01000001), (0x80000000, 0x80000001),
                  (0x01000001, MASK), (0x02000000, 1))
    command_pairs = ((0x80, 0x18), (0x7F, 0x18), (0x81, 0x18), (0x80, 0x17),
                     (0x80, 0x19), (0, 0x18), (0x08, 0x18), (0x80, 0x18))
    for slot in range(SLOTS):
        words = [rng.getrandbits(32) for _ in range(8)]
        state, opcode = command_pairs[(lane + slot) % len(command_pairs)]
        present = int((lane + slot) % 5 != 0)
        first, second = flag_pairs[(lane + slot) % len(flag_pairs)]
        if lane % 16 == 0:
            present, state, opcode = 0, 0x80, 0x18
        elif lane % 16 == 1:
            present, state, opcode = 1, 0x80, 0x18
        elif lane % 16 == 2:
            first, second = 0, 1
        elif lane % 16 == 3:
            first, second = 0x01000000, 1
        elif lane % 16 == 4:
            first, second = 1, 0
        elif lane >= 128 and lane % 4 == 0:
            first = (rng.getrandbits(32) & ~0x01000001) | (first & 0x01000001)
            second = (rng.getrandbits(32) & ~1) | (second & 1)
        if slot in (15, 16) and lane % 3:
            present, state, opcode = 1, 0x80, 0x18
        words[2] = words[2] & 0xFFFF0000 | opcode << 8 | state
        actors.append((present, *words, first, second))
    return Vector(function, lane, mutation, reward, bone, globals_,
                  tuple(rng.getrandbits(32) for _ in GUARDS), tuple(states), tuple(actors))


def state_addresses(base: int, index: int) -> tuple[int, ...]:
    return base, base + 12, base + 0x858, base + 0x858 + index * 0x84C, base + 0x2C10


def run(m: Machine, v: Vector) -> tuple[dict, list[int]]:
    for address, value in zip((PHASE, REWARD_MODE, TICK, CONTROL, GLOBAL, UNRELATED),
                              v.globals, strict=True):
        m.put(address, value)
    for address, value in zip(GUARDS, v.guards, strict=True):
        m.put(address, value)
    for base, state in zip(STATES, v.states, strict=True):
        m.map_zero(base, 0x3000)
        for address, value in zip(state_addresses(base, state[0]), state, strict=True):
            m.put(address, value)
    pointers = []
    for slot, actor in enumerate(v.actors):
        pointer = ACTOR_RECORDS + slot * 32 if actor[0] else 0
        pointers.append(pointer)
        m.put(ACTORS + slot * 4, pointer)
        m.store(ACTOR_RECORDS + slot * 32, b"".join(x.to_bytes(4, "little") for x in actor[1:9]))
    calls, counts = [], [0] * SLOTS

    def current() -> int | None:
        pointer = m.load(GLOBAL)
        return STATES.index(pointer) if pointer in STATES else None

    def primary(slot: int, actor: int) -> None:
        m.put(STATES[slot] + 12, actor & 255, 1)

    def queue(slot: int, actor: int) -> None:
        m.put(STATES[slot] + 0x858, actor & 255, 1)

    def mutate(address: int, args: list[int]) -> None:
        mode = v.mutation
        ordinal = len(calls) // EVENT_WIDTH

        def globals_changed() -> None:
            for target, salt in ((PHASE, 0xA1000000), (REWARD_MODE, 0xB2000000),
                                 (TICK, 0xC3000000), (CONTROL, 0xD4000000),
                                 (UNRELATED, 0xE5000000)):
                m.put(target, salt | ((v.lane & 0xFFFF) << 8) | ordinal)

        if mode == 1:
            globals_changed()
        if v.function == 1 and mode == 2:
            globals_changed()
            m.put(GLOBAL, 0xDEAD0040)
        if v.function == 2:
            slot = args[0] if args else 0
            flag_call = address == FLAGS
            idle_call = address == IDLE
            first_call = flag_call and counts[slot] == 1
            second_call = flag_call and counts[slot] == 2
            if ((mode == 2 and first_call) or (mode == 5 and idle_call) or
                (mode == 6 and flag_call) or (mode == 7 and second_call)):
                m.put(GLOBAL, STATES[1 - current()])
                primary(current(), slot + 1)
            if (mode == 3 and first_call) or (mode == 4 and idle_call):
                primary(current(), slot + 1)
            if mode == 8 and address in (PALETTE, FREEZE, CLUT, ROOM, MARKERS):
                globals_changed()
                m.put(GLOBAL, STATES[1])
                primary(1, 255)
        if v.function == 3:
            changed_actor = (v.states[0][2] + 73) & 255
            if address == BONE:
                if mode in (2, 4, 7):
                    m.put(GLOBAL, STATES[1])
                if mode in (3, 4, 7):
                    queue(0, changed_actor)
                if mode == 5:
                    m.put(STATES[0] + 0x858 + v.states[0][0] * 0x84C, changed_actor, 1)
                if mode == 6:
                    queue(0, 255)
                if mode == 7:
                    queue(1, changed_actor ^ 255)
                    primary(1, changed_actor ^ 127)
            if mode == 8 and address == FADE:
                queue(0, changed_actor)
                m.put(GLOBAL, STATES[1])
                globals_changed()
        if v.function == 6 and ((mode == 2 and address == WEAPON) or
                                (mode == 3 and address == IDLE and args[0] == 7)):
            globals_changed()
            m.put(GLOBAL, 0xDEAD0080)

    def stub(address: int):
        def invoke(machine: Machine) -> None:
            args = machine.r[4:4 + min(ARG_COUNTS[address], 4)]
            if ARG_COUNTS[address] == 5:
                args.append(machine.load(machine.r[29] + 16))
            result = 0
            if address == FLAGS:
                slot = args[0]
                result = v.actors[slot][9 + min(counts[slot], 1)]
                counts[slot] += 1
            elif address == BONE:
                result = v.bone
            active = current()
            calls.extend([address, len(args), *args, *([0] * (5 - len(args))), result,
                          machine.load(GLOBAL), machine.load(PHASE), machine.load(REWARD_MODE),
                          machine.load(TICK), machine.load(CONTROL),
                          MASK if active is None else machine.load(STATES[active] + 12, 1),
                          machine.load(STATES[0] + 0x858, 1)])
            mutate(address, args)
            # Services may destroy every caller-saved register, including old argument values.
            for register in (*range(2, 16), 24, 25):
                machine.r[register] = 0xC1000000 | (len(calls) // EVENT_WIDTH) << 8 | register
            if address in (FLAGS, BONE):
                machine.r[2] = result
        return invoke

    for address in ARG_COUNTS:
        m.stub(address, stub(address))
    m.r[4] = v.reward
    m.execute(ENTRIES[v.function], limit=2048)
    values = [m.load(address) for address in (PHASE, REWARD_MODE, TICK, CONTROL, GLOBAL, UNRELATED)]
    values.extend(m.load(address) for address in GUARDS)
    for base, state in zip(STATES, v.states, strict=True):
        values.extend(m.load(address) for address in state_addresses(base, state[0]))
    values.extend(m.load(ACTOR_RECORDS + slot * 32 + word * 4)
                  for slot in range(SLOTS) for word in range(8))
    values.extend(counts)
    store_tags = {CONTROL: (2, 0), PHASE: (3, 0), REWARD_MODE: (4, 0), TICK: (5, 0)}
    store_tags.update({ACTOR_RECORDS + slot * 32 + 8: (1, slot) for slot in range(SLOTS)})
    writes = []
    for address, size in m.stores:
        if DEFAULT_STACK - 0x30 <= address < DEFAULT_STACK:
            continue
        writes.extend([*store_tags.get(address, (address, 0)), size])
    observation = {"values": values, "calls": calls, "writes": writes, "stores": list(m.stores),
                   "table": all(m.load(ACTORS + slot * 4) == pointer
                                for slot, pointer in enumerate(pointers))}
    return observation, v.inputs()


def check(image: Image, v: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, reference
    base = 1 + len(VALUE_NAMES)
    if len(native) < base + 2:
        return [f"driver printed only {len(native)} values"]
    problems = field_problems(("native-diagnostics", *VALUE_NAMES), native[:base],
                              [0, *observation["values"]])
    write_end = base + 1 + native[base]
    if write_end >= len(native) or write_end + 1 + native[write_end] != len(native):
        return [*problems, "malformed write/call output"]
    if native[base + 1:write_end] != observation["writes"]:
        problems.append(f"writes native {native[base + 1:write_end]} original {observation['writes']}")
    native_calls = native[write_end + 1:]
    if native_calls != observation["calls"]:
        first = next((i for i, (a, b) in enumerate(zip(native_calls, observation["calls"], strict=False))
                      if a != b), min(len(native_calls), len(observation["calls"])))
        first = first // EVENT_WIDTH * EVENT_WIDTH
        problems.append(f"calls at {first // EVENT_WIDTH}: native "
                        f"{native_calls[first:first + EVENT_WIDTH]} original "
                        f"{observation['calls'][first:first + EVENT_WIDTH]}")
    allowed = [(DEFAULT_STACK - 0x30, DEFAULT_STACK)]
    if v.function == 0:
        allowed.extend((ACTOR_RECORDS + slot * 32 + 8, ACTOR_RECORDS + slot * 32 + 9)
                       for slot in range(16))
    else:
        targets = {1: (CONTROL, PHASE), 2: (REWARD_MODE, TICK, PHASE),
                   3: (PHASE,), 4: (PHASE,), 5: (PHASE,), 6: (CONTROL,)}[v.function]
        allowed.extend((address, address + 4) for address in targets)
    problems.extend(store_problems(observation["stores"], allowed))
    if not observation["table"]:
        problems.append("original changed actor table")
    return problems


def describe(image: Image, results: list) -> list[str]:
    lines = ["VOID policies: no original return value is compared."]
    for function, (name, entry, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, o, n) for v, o, n in results if v.function == function]
        mismatches = sum(bool(check(image, v, o, n, o)) for v, o, n in selected)
        mutations = sorted({v.mutation for v, _, _ in selected})
        lines.append(f"{name} [{entry:08X},{end:08X}) {(end - entry) // 4} instructions "
                     f"sha256={image.sha256(entry, end)} vectors={len(selected)} "
                     f"mismatches={mismatches} callbacks={mutations}")
    return lines
