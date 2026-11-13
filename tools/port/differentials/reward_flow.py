"""Eight reward policies, following their real original cluster calls."""

import random
from dataclasses import dataclass

from differential import (
    DEFAULT_STACK,
    MASK,
    Image,
    Machine,
    call_problems,
    field_problems,
    store_problems,
)

FUNCTIONS = ("F04123", "F04124", "F04125", "F04126", "F00220", "F00221", "F00222", "F00223")
ID = "/".join(FUNCTIONS)
ENTRIES = (0x8007B410, 0x8007B470, 0x8007B4C4, 0x8007B508,
           0x8007B63C, 0x8007B764, 0x8007B7DC, 0x8007B834)
ENDS = (*ENTRIES[1:], 0x8007B884)
ENTRY, END = ENTRIES[0], ENDS[0]
DRIVER, SOURCES = "reward_flow_driver.cpp", (r"Native\Game\src\Battle\RewardFlow.cpp",)
VECTORS, SEED, MUTANT_VECTORS = 8 * 1024, 41234126220223, 8 * 128

GLOBAL, PHASE, FOLLOW, PAUSED, LOOT = (
    0x800F19CC, 0x800F18F0, 0x800F1864, 0x800F1968, 0x800F1900)
DEFINITIONS, DEFINITION_STRIDE = 0x8004B9DC, 52
STATES, MENUS = (0x80500000, 0x80504000), (0x80700000, 0x80700010)
HEADS = (0, 0x80800000, 0x80800040)
GUARDS = (0x800F18EC, 0x800F18F4, 0x800F1860, 0x800F196C, 0x800F19C8, 0x800F19D0)
ROOM_PALETTE, CLUT, UNFREEZE, ROOM, MINIMAP, AMBIENCE = (
    0x8007B10C, 0x80048FEC, 0x8006C2A0, 0x8008B4BC, 0x8008B4C8, 0x8008E480)
SCENE_COLORS, FREEZE, IDLE, FLAG, PENDING, POSE, EFFECT, PLATFORMS = (
    0x8007B1B8, 0x8006C250, 0x8009E5C4, 0x800BEBF4,
    0x8006F760, 0x8009E070, 0x8006F848, 0x8008D594)
NOTICE, ABILITIES, LOOT_MENU, RESULTS, UPDATE, MENU_STATE, RELEASE = (
    0x800CB2B8, 0x800CB208, 0x800CB158, 0x800CB23C,
    0x800C97BC, 0x800CB66C, 0x800CB114)
ARG_COUNTS = {
    ROOM_PALETTE: 5, CLUT: 1, UNFREEZE: 0, ROOM: 1, MINIMAP: 1, AMBIENCE: 1,
    SCENE_COLORS: 5, FREEZE: 0, IDLE: 1, FLAG: 1, PENDING: 0, POSE: 3,
    EFFECT: 0, PLATFORMS: 1, NOTICE: 3, ABILITIES: 2, LOOT_MENU: 1,
    RESULTS: 0, UPDATE: 0, MENU_STATE: 0, RELEASE: 0,
}
EVENT_WIDTH = 17
VALUE_NAMES = (
    "phase", "follow", "paused", "loot", "global-state", "menu-owner", "pending-source",
    "B6-reads", "AB-reads", "pending-reads", "menu0", "menu1",
    *(f"guard-{i}" for i in range(len(GUARDS))),
    *(f"state-{i}-{field}" for i in range(2)
      for field in ("selection", "primary", "position", "continued", "guard")),
    *(f"definition-{i}" for i in range(256)),
)

# Each replacement changes one original instruction, never a native fixture.
MUTANTS = {
    "F04123-mode": {0x8007B414: 0x2404000C},
    "F04123-duration": {0x8007B418: 0x24050005},
    "F04123-red": {0x8007B41C: 0x2406FFFD},
    "F04123-green": {0x8007B420: 0x00003821},
    "F04123-blue-stack": {0x8007B42C: 0xAFA60010},
    "F04123-clut": {0x8007B434: 0x24040001},
    "F04123-freeze-not-unfreeze": {0x8007B438: 0x0C01B094},
    "F04123-room": {0x8007B444: 0x24040000},
    "F04123-minimap": {0x8007B44C: 0x24040001},
    "F04123-ambience": {0x8007B454: 0x24040000},
    "F04123-no-store": {0x8007B464: 0},
    "F04123-half-store": {0x8007B464: 0xA4621968},
    "F04124-clut": {0x8007B47C: 0x24040000},
    "F04124-mode": {0x8007B480: 0x24040009},
    "F04124-duration": {0x8007B484: 0x24050005},
    "F04124-red": {0x8007B488: 0x24060001},
    "F04124-green": {0x8007B48C: 0x24070001},
    "F04124-blue-stack": {0x8007B494: 0xAFA40010},
    "F04124-room-not-scene": {0x8007B490: 0x0C01EC43},
    "F04124-unfreeze": {0x8007B498: 0x0C01B0A8},
    "F04124-room": {0x8007B4A4: 0x24040001},
    "F04124-ambience": {0x8007B4AC: 0x24040001},
    "F04124-no-store": {0x8007B4B8: 0},
    "F04124-half-store": {0x8007B4B8: 0xA4401968},
    "F04125-phase": {0x8007B4CC: 0x24020003},
    "F04125-begin-not-end": {0x8007B4D4: 0x0C01ED04},
    "F04125-no-phase-store": {0x8007B4D8: 0},
    "F04125-wrong-follow-word": {0x8007B4E0: 0x8C421860},
    "F04125-ignore-follow": {0x8007B4E8: 0},
    "F04125-idle-actor": {0x8007B4F4: 0x24040001},
    "F04126-flag-id": {0x8007B514: 0x240400B5},
    "F04126-first-mask": {0x8007B518: 0x3042007F},
    "F04126-second-mask": {0x8007B52C: 0x30420001},
    "F04126-third-mask": {0x8007B544: 0x30420001},
    "F04126-second-compare": {0x8007B530: 0x24030001},
    "F04126-third-compare": {0x8007B548: 0x24030002},
    "F04126-pending-low-half": {0x8007B55C: 0x3042FFFF},
    "F04126-no-id-guard": {0x8007B578: 0},
    "F04126-wrong-unlocked-bit": {0x8007B5A8: 0x00021382},
    "F04126-no-unlocked-guard": {0x8007B5B0: 0},
    "F04126-no-position-guard": {0x8007B5C0: 0},
    "F04126-no-continued-guard": {0x8007B5D0: 0},
    "F04126-no-source-guard": {0x8007B5E0: 0},
    "F04126-phase": {0x8007B5F0: 0x24020004},
    "F04126-no-follow-guard": {0x8007B5F4: 0},
    "F04126-pose-actor": {0x8007B5FC: 0x24040001},
    "F04126-pose-id": {0x8007B608: 0x24060005},
    "F04126-no-effect": {0x8007B60C: 0},
    "F04126-platforms": {0x8007B618: 0x24040000},
    "F04126-fallback-only-end": {0x8007B624: 0x0C01ED1C},
    "F00220-no-id-guard": {0x8007B65C: 0},
    "F00220-wrong-unlocked-bit": {0x8007B68C: 0x00021382},
    "F00220-no-unlocked-guard": {0x8007B694: 0},
    "F00220-no-position-guard": {0x8007B6A4: 0},
    "F00220-no-continued-guard": {0x8007B6B4: 0},
    "F00220-no-source-guard": {0x8007B6C4: 0},
    "F00220-action-phase": {0x8007B6CC: 0x24020005},
    "F00220-no-pause-selection-reload": {0x8007B6D8: 0},
    "F00220-notice-kind": {0x8007B6DC: 0x2404000B},
    "F00220-notice-flag": {0x8007B6E8: 0x24060000},
    "F00220-no-notice-selection-reload": {0x8007B6EC: 0},
    "F00220-wrong-unlock-bit": {0x8007B71C: 0x34634000},
    "F00220-half-unlock-store": {0x8007B724: 0xA443000C},
    "F00220-ability-phase": {0x8007B734: 0x24040005},
    "F00220-art-shift": {0x8007B744: 0x00102202},
    "F00220-ability-mask": {0x8007B74C: 0x320500FF},
    "F00221-stage-flag": {0x8007B770: 0x240400AA},
    "F00221-stage-mask": {0x8007B774: 0x3042007F},
    "F00221-stage-threshold": {0x8007B778: 0x2C420002},
    "F00221-no-head-guard": {0x8007B78C: 0},
    "F00221-no-loot-open": {0x8007B794: 0},
    "F00221-phase": {0x8007B7A0: 0x24020003},
    "F00221-no-pause": {0x8007B7A4: 0},
    "F00221-empty-only-resume": {0x8007B7B4: 0x0C01ED31},
    "F00221-low-only-end": {0x8007B7C4: 0x0C01ED1C},
    "F00222-stage-flag": {0x8007B7E8: 0x240400AA},
    "F00222-stage-mask": {0x8007B7EC: 0x3042007F},
    "F00222-stage-threshold": {0x8007B7F0: 0x2C420002},
    "F00222-no-results-open": {0x8007B7FC: 0},
    "F00222-phase": {0x8007B808: 0x24020004},
    "F00222-no-pause": {0x8007B80C: 0},
    "F00222-low-only-resume": {0x8007B81C: 0x0C01ED31},
    "F00223-no-update": {0x8007B83C: 0},
    "F00223-status-byte": {0x8007B84C: 0x80430000},
    "F00223-busy-value": {0x8007B850: 0x24020000},
    "F00223-no-busy-guard": {0x8007B854: 0},
    "F00223-begin-not-end": {0x8007B85C: 0x0C01ED04},
    "F00223-no-release": {0x8007B864: 0},
    "F00223-tail-only-resume": {0x8007B86C: 0x0C01ED31},
}


@dataclass(frozen=True)
class Vector:
    function: int
    lane: int
    mutation: int
    seed: int
    pending: int
    changed_id: int
    globals: tuple[int, ...]
    b6: tuple[int, ...]
    ab: tuple[int, ...]
    menus: tuple[int, ...]
    states: tuple[tuple[int, ...], ...]
    guards: tuple[int, ...]
    unlocked: int

    def inputs(self) -> list[int]:
        return [self.function, self.lane, self.mutation, self.seed, self.pending,
                self.changed_id, *self.globals, *self.b6, *self.ab, *self.menus,
                *(x for state in self.states for x in state), *self.guards, self.unlocked]


def selection_states(rng: random.Random, scenario: int) -> tuple[tuple[int, ...], ...]:
    states = []
    for owner in range(2):
        action = (0, 1, 255, 1, 1, 1, 1, 255)[scenario % 8] if owner == 0 else 255
        primary = rng.getrandbits(24) << 8 | (255 if scenario % 8 == 6 else 0)
        position = (0x80000000 if scenario % 8 == 4 else 0)
        continued = rng.getrandbits(24) | (255 << 24 if scenario % 8 == 5 else 0)
        states.append((rng.getrandbits(16) << 16 | action, primary, position,
                       continued, rng.getrandbits(32)))
    return tuple(states)


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 8, number // 8
    scenario = lane % 16
    mutation = (lane // 16) % 9
    # High flag words are low-byte masking controls outside LiveScriptFlag's U8 return domain.
    b6 = ((0, 99, 99), (1, 2, 99), (2, 1, 3), (3, 3, 2),
          (128, 128, 128), (0x10000, 99, 99), (255, 0x10002, 99),
          (255, 255, 0x10003))[scenario % 8]
    ab = ((0, 0), (1, 3), (2, 255), (3, 0), (127, 2), (128, 1),
          (255, 0), (0x10002, 0x10003))[scenario % 8]
    pending = (0, 1, 0x10000, 0x10001, 0x80000000, MASK, 0, 0x1234ABCD)[
        (scenario // 2 + lane // 128) % 8]
    if lane >= 128 and lane % 7 == 0:
        pending = rng.getrandbits(32)
    if function == 3 and scenario % 8 == 6:
        pending = 0
    states = selection_states(rng, scenario)
    head = HEADS[(scenario // 2) % 3]
    pointer = STATES[0]
    can_skip_state = function in (0, 1, 2) or (function == 3 and b6[0] & 255 == 0 and pending)
    can_skip_state |= function == 5 and (ab[0] & 255 < 3 or head != 0)
    can_skip_state |= function == 6 and ab[0] & 255 >= 3
    if can_skip_state and lane % 2:
        pointer = 0
    status = (1, 0, 0xFFFF, 0x101, 0x8001, 1, 0xFF01, 0x7FFF)[scenario % 8]
    menus = (rng.getrandbits(16) << 16 | status,
             rng.getrandbits(16) << 16 | (1 if scenario % 2 else 0xFFFF))
    menu = 0 if function == 7 and mutation == 8 else MENUS[0]
    globals_ = (rng.getrandbits(32), (0, 1, 0x10000, MASK, 0x80000000)[scenario % 5],
                rng.getrandbits(32), head, pointer, menu)
    return Vector(function, lane, mutation, rng.getrandbits(32), pending,
                  (0, 1, 255)[scenario % 3], globals_, b6, ab, menus,
                  tuple(states), tuple(rng.getrandbits(32) for _ in GUARDS),
                  int(scenario % 8 == 3))


def definition(seed: int, index: int, first_id: int, unlocked: int) -> int:
    value = (seed ^ (index * 0x1020304)) & MASK
    return (value & ~0x8000 | unlocked << 15) if index == first_id else value


def state_addresses(base: int) -> tuple[int, ...]:
    return base + 8, base + 12, base + 0x4C, base + 0x2C04, base + 0x2C08


def prepare_reward_objects(m: Machine, states: tuple[tuple[int, ...], ...],
                           menus: tuple[int, ...], seed: int, unlocked: int) -> None:
    for base, fields in zip(STATES, states, strict=True):
        m.map_zero(base, 0x3000)
        for address, value in zip(state_addresses(base), fields, strict=True):
            m.put(address, value)
    for address, value in zip(MENUS, menus, strict=True):
        m.put(address, value)
    m.map_zero(DEFINITIONS, 256 * DEFINITION_STRIDE)
    for index in range(256):
        m.put(DEFINITIONS + index * DEFINITION_STRIDE + 12,
              definition(seed, index, states[0][0] & 0xFFFF, unlocked))


def run(m: Machine, v: Vector) -> tuple[dict, list[int]]:
    for address, value in zip((PHASE, FOLLOW, PAUSED, LOOT, GLOBAL), v.globals[:5], strict=True):
        m.put(address, value)
    for address, value in zip(GUARDS, v.guards, strict=True):
        m.put(address, value)
    prepare_reward_objects(m, v.states, v.menus, v.seed, v.unlocked)
    dynamic = {"menu": v.globals[5], "pending": v.pending}
    counts = [0, 0, 0]
    calls = []

    def selected() -> int:
        pointer = m.load(GLOBAL)
        return m.load(pointer + 8, 2) if pointer in STATES else MASK

    def mutate(address: int) -> None:
        ordinal = len(calls) // EVENT_WIDTH
        if v.mutation == 1:
            m.put(PHASE, 0xA1000000 | ordinal)
            m.put(FOLLOW, 0 if (v.lane + ordinal) % 3 == 0 else 0xB2000000 | ordinal)
            m.put(PAUSED, 0xC3000000 | ordinal)
            m.put(GUARDS[0], 0xD4000000 | ordinal)
        if v.mutation == 2 and address == AMBIENCE:
            m.put(FOLLOW, 0 if v.globals[1] else 0x80000000)
        if v.mutation in (3, 5) and address == ROOM_PALETTE:
            m.put(GLOBAL, STATES[1])
            m.put(STATES[1] + 8, v.changed_id, 2)
        if v.mutation in (4, 5) and address == NOTICE:
            target = STATES[0] if v.mutation == 5 else STATES[1]
            m.put(GLOBAL, target)
            m.put(target + 8, v.changed_id, 2)
        if v.mutation in (1, 5) and address == ROOM_PALETTE:
            dynamic["pending"] = v.pending ^ MASK
        if v.mutation == 6 and address == POSE:
            m.put(FOLLOW, 0)
            m.put(PHASE, 0xAABBCCDD)
        if v.mutation == 7 and address == LOOT_MENU:
            m.put(LOOT, HEADS[2] if m.load(LOOT) != HEADS[2] else HEADS[1])
            m.put(PHASE, 0xFACEB00C)
        if v.mutation == 8 and address == UPDATE:
            dynamic["menu"] = MENUS[1]

    def stub(address: int):
        def invoke(machine: Machine) -> None:
            count = ARG_COUNTS[address]
            args = machine.r[4:4 + min(count, 4)]
            if count == 5:
                args.append(machine.load(machine.r[29] + 16))
            result = 0
            if address == FLAG:
                slot = 0 if args[0] == 0xB6 else 1
                values = v.b6 if slot == 0 else v.ab
                result = values[min(counts[slot], len(values) - 1)] if args[0] in (0xB6, 0xAB) else 0
                counts[slot] += 1
            elif address == PENDING:
                result = dynamic["pending"]
                counts[2] += 1
            elif address == MENU_STATE:
                result = dynamic["menu"]
            action = selected()
            status = machine.load(dynamic["menu"], 2) if dynamic["menu"] in MENUS else MASK
            word = (machine.load(DEFINITIONS + action * DEFINITION_STRIDE + 12)
                    if action < 256 else MASK)
            calls.extend([address, count, *args, *([0] * (5 - count)), result,
                          *(machine.load(a) for a in (PHASE, FOLLOW, PAUSED, LOOT, GLOBAL)),
                          dynamic["menu"], action, status, word])
            mutate(address)
            for register in (*range(2, 16), 24, 25):
                machine.r[register] = 0xCC000000 | ordinal_word(len(calls), register)
            if address in (FLAG, PENDING, MENU_STATE):
                machine.r[2] = result
        return invoke

    for address in ARG_COUNTS:
        m.stub(address, stub(address))
    m.follow.update(ENTRIES)
    m.execute(ENTRIES[v.function], limit=2048)
    values = [*(m.load(a) for a in (PHASE, FOLLOW, PAUSED, LOOT, GLOBAL)),
              dynamic["menu"], dynamic["pending"], *counts, *(m.load(a) for a in MENUS),
              *(m.load(a) for a in GUARDS)]
    values.extend(m.load(address) for base in STATES for address in state_addresses(base))
    values.extend(m.load(DEFINITIONS + index * DEFINITION_STRIDE + 12) for index in range(256))
    writes = []
    for address, size in m.stores:
        if DEFAULT_STACK - 0x80 <= address < DEFAULT_STACK:
            continue
        if address == PHASE:
            tag = (1, 0)
        elif address == PAUSED:
            tag = (2, 0)
        elif (address - DEFINITIONS - 12) % DEFINITION_STRIDE == 0:
            tag = (3, (address - DEFINITIONS - 12) // DEFINITION_STRIDE)
        else:
            tag = (address, 0)
        writes.extend([*tag, size])
    return {"values": values, "calls": calls, "writes": writes, "stores": list(m.stores)}, v.inputs()


def ordinal_word(call_words: int, register: int) -> int:
    return call_words // EVENT_WIDTH << 8 | register


def check(image: Image, v: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, v, reference
    base = 1 + len(VALUE_NAMES)
    if len(native) < base + 2:
        return [f"driver printed only {len(native)} values"]
    write_end = base + 1 + native[base]
    if write_end >= len(native) or write_end + 1 + native[write_end] != len(native):
        return ["malformed write/call output"]
    problems = field_problems(("native-diagnostics", *VALUE_NAMES),
                              native[:base], [0, *observation["values"]])
    native_calls = native[write_end + 1:]
    original_calls = observation["calls"]
    problems.extend(call_problems(
        [native_calls[i:i + EVENT_WIDTH] for i in range(0, len(native_calls), EVENT_WIDTH)],
        [original_calls[i:i + EVENT_WIDTH] for i in range(0, len(original_calls), EVENT_WIDTH)]))
    if native[base + 1:write_end] != observation["writes"]:
        problems.append(f"writes native {native[base + 1:write_end]} original {observation['writes']}")
    allowed = [(DEFAULT_STACK - 0x80, DEFAULT_STACK), (PHASE, PHASE + 4), (PAUSED, PAUSED + 4)]
    allowed.extend((DEFINITIONS + i * DEFINITION_STRIDE + 12,
                    DEFINITIONS + i * DEFINITION_STRIDE + 16) for i in range(256))
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    lines = ["VOID0 policies; actual original cluster calls followed, bounded external services stubbed.",
             "Raw high flag words are labelled masking controls outside the production U8 flag service domain.",
             "No independent actor41 data: its original pointer slot aliases ActionState; native extensions excluded."]
    for function, (name, entry, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, o, n) for v, o, n in results if v.function == function]
        mismatches = sum(bool(check(image, v, o, n, o)) for v, o, n in selected)
        outside = sum(any(x > 255 for x in (*v.b6, *v.ab)) for v, _, _ in selected)
        mutations = sorted({v.mutation for v, _, _ in selected})
        lines.append(f"{name} [{entry:08X},{end:08X}) {(end - entry) // 4} instructions "
                     f"sha256={image.sha256(entry, end)} vectors={len(selected)} "
                     f"mismatches={mismatches} callbacks={mutations} outside-U8-controls={outside}")
    return lines
