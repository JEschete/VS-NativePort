"""Four reward dialog continuations; original reward policies are followed."""

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
from differentials.reward_flow import (
    DialogVector,
    callback_args,
    complete_reward_pose,
    dialog_boundaries,
    prepare_dialog_machine,
    record_dialog_call,
    replace_observed_menu,
    selection_states,
    stamp_callback_state,
    state_addresses,
    toggle_word,
)

FUNCTIONS = ("F00196", "F00197", "F00198", "F00224")
ID = "/".join(FUNCTIONS)
ENTRIES = (0x800797BC, 0x8007980C, 0x8007983C, 0x8007B884)
# F00198 ends at its own return, not the next catalogued function.
ENDS = (0x8007980C, 0x8007983C, 0x800798A4, 0x8007B908)
ENTRY, END = ENTRIES[0], ENDS[0]
DRIVER = "reward_dialogs_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\RewardFlow.cpp",)
VECTORS, SEED, MUTANT_VECTORS = 4 * 1024, 196197198224, 4 * 256

GLOBAL, PHASE, FOLLOW, LOOT_OPENED, PAUSED, EFFECTS = (
    0x800F19CC, 0x800F18F0, 0x800F1864, 0x800F1860, 0x800F1968, 0x800F1868)
GLOBALS = (PHASE, FOLLOW, LOOT_OPENED, PAUSED, EFFECTS, GLOBAL)
STATES, MENUS = (0x80500000, 0x80504000), (0x80700000, 0x80700010)
DEFINITIONS, DEFINITION_STRIDE = 0x8004B9DC, 52
GUARDS = (0x800F18F4, 0x800F196C, 0x800F19C8, 0x800F19D0)
UPDATE, MENU_STATE, RELEASE = 0x800C97BC, 0x800CB66C, 0x800CB114
FIELD_RESUME, BEGIN_ABILITY, GRANT_ABILITY = 0x8006C40C, 0x8006F89C, 0x8006FA20
NOTIFICATION, RELEASE_NOTIFICATION = 0x800CB45C, 0x800CB50C
END_PAUSE, RESUME_REWARDS, CHECK_UNLOCKS = 0x8007B470, 0x8007B4C4, 0x8007B508
CLUT, SCENE_COLORS, FREEZE, ROOM, AMBIENCE = (
    0x80048FEC, 0x8007B1B8, 0x8006C250, 0x8008B4BC, 0x8008E480)
IDLE, FLAG, PENDING, POSE, EFFECT, PLATFORMS = (
    0x8009E5C4, 0x800BEBF4, 0x8006F760, 0x8009E070, 0x8006F848, 0x8008D594)
ARG_COUNTS = {
    UPDATE: 0, MENU_STATE: 0, RELEASE: 0, FIELD_RESUME: 0,
    BEGIN_ABILITY: 0, GRANT_ABILITY: 0, NOTIFICATION: 0, RELEASE_NOTIFICATION: 0,
    CLUT: 1, SCENE_COLORS: 5, FREEZE: 0, ROOM: 1, AMBIENCE: 1,
    IDLE: 1, FLAG: 1, PENDING: 0, POSE: 3, EFFECT: 0, PLATFORMS: 1,
}
EVENT_WIDTH = 20
VALUE_NAMES = (
    "phase", "follow", "loot-opened", "paused", "effects", "action-owner",
    "menu-owner", "notification", "pending-source", "flag-reads", "pending-reads",
    "menu0", "menu1", *(f"guard-{i}" for i in range(len(GUARDS))),
    *(f"state-{i}-{field}" for i in range(2)
      for field in ("selection", "primary", "position", "continued", "guard")),
    *(f"definition-{i}" for i in range(256)),
)
MUTANTS = {
    "F00196-no-update": {0x800797C4: 0},
    "F00196-status-byte": {0x800797D4: 0x80430000},
    "F00196-busy-zero": {0x800797D8: 0x24020000},
    "F00196-no-busy-gate": {0x800797DC: 0},
    "F00196-end-not-field-resume": {0x800797E4: 0x0C01ED1C},
    "F00196-no-release": {0x800797EC: 0},
    "F00196-grant-not-begin": {0x800797F4: 0x0C01BE88},
    "F00197-mask-one": {0x80079818: 0x30420001},
    "F00197-mask-two": {0x80079818: 0x30420002},
    "F00197-mask-seven": {0x80079818: 0x30420007},
    "F00197-wrong-flags": {0x80079810: 0x8C421864},
    "F00197-invert-busy": {0x8007981C: 0x10400003},
    "F00197-no-grant": {0x80079824: 0},
    "F00197-begin-not-grant": {0x80079824: 0x0C01BE27},
    "F00198-no-update": {0x80079844: 0},
    "F00198-status-byte": {0x80079854: 0x80430000},
    "F00198-busy-zero": {0x80079858: 0x24020000},
    "F00198-no-busy-gate": {0x8007985C: 0},
    "F00198-wrong-notice-query": {0x80079864: 0x0C032D9B},
    "F00198-invert-notice": {0x8007986C: 0x10400009},
    "F00198-end-not-field-resume": {0x80079874: 0x0C01ED1C},
    "F00198-no-row-release": {0x8007987C: 0},
    "F00198-no-notice-release": {0x80079884: 0},
    "F00198-grant-not-begin": {0x8007988C: 0x0C01BE88},
    "F00224-no-update": {0x8007B88C: 0},
    "F00224-status-byte": {0x8007B89C: 0x80430000},
    "F00224-busy-zero": {0x8007B8A0: 0x24020000},
    "F00224-no-busy-gate": {0x8007B8A4: 0},
    "F00224-follow-half": {0x8007B8AC: 0x84421864},
    "F00224-follow-is-loot": {0x8007B8AC: 0x8C421860},
    "F00224-no-follow-gate": {0x8007B8B4: 0},
    "F00224-no-first-end": {0x8007B8BC: 0},
    "F00224-field-not-reward-end": {0x8007B8BC: 0x0C01B103},
    "F00224-no-row-release": {0x8007B8C4: 0},
    "F00224-loot-half": {0x8007B8D0: 0x84421860},
    "F00224-loot-is-follow": {0x8007B8D0: 0x8C421864},
    "F00224-invert-loot": {0x8007B8D8: 0x14400005},
    "F00224-only-end-not-resume": {0x8007B8E0: 0x0C01ED1C},
    "F00224-resume-not-unlock": {0x8007B8F0: 0x0C01ED31},
}


@dataclass(frozen=True)
class Vector(DialogVector):
    flags: tuple[int, ...]

    def inputs(self) -> list[int]:
        return [*self.prefix_inputs(), *self.flags, *self.object_inputs()]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 4, number // 4
    scenario, mutation = lane % 16, lane // 16 % 16
    menus, effects, follow, loot, notification = dialog_boundaries(rng, lane)
    pending = (0, 1, 0x10000, 0x10001)[scenario // 2 % 4]
    flags = ((0, 99, 99), (1, 2, 99), (2, 1, 3), (1, 1, 1))[scenario % 4]
    if mutation == 12:
        flags = (0, 99, 99)
    states = selection_states(rng, scenario)
    menu = 0 if mutation == 4 else MENUS[0]
    globals_ = (rng.getrandbits(32), follow, loot, rng.getrandbits(32), effects, STATES[0])
    return Vector(function=function, lane=lane, mutation=mutation,
                  seed=rng.getrandbits(32), pending=pending, notification=notification,
                  menu=menu, changed_id=(0, 1, 255)[scenario % 3], globals=globals_,
                  flags=flags, menus=menus, states=tuple(states),
                  guards=tuple(rng.getrandbits(32) for _ in GUARDS),
                  unlocked=int(scenario % 8 == 3))


def run(m: Machine, v: Vector) -> tuple[dict, list[int]]:
    dynamic = prepare_dialog_machine(m, v, GLOBALS, GUARDS)
    counts = [0, 0]
    calls = []

    def mutate(address: int, result: int) -> None:
        if address == UPDATE:
            if v.mutation in (1, 3, 4, 13):
                dynamic["menu"] = MENUS[1]
            if v.mutation in (1, 2, 13):
                toggle_word(m, FOLLOW)
                toggle_word(m, LOOT_OPENED)
            if v.mutation == 2:
                m.put(MENUS[0], 1 if v.lane % 2 else 0xFFFF, 2)
            if v.mutation == 3:
                dynamic["notice"] = 0 if v.notification else 0x80000000
            if v.mutation == 4:
                dynamic["notice"] = 0
            if v.mutation == 13:
                m.put(GLOBAL, STATES[1])
        if v.mutation == 5 and address == FIELD_RESUME:
            m.put(GLOBAL, STATES[1])
            m.put(STATES[1] + 8, v.changed_id, 2)
        if v.mutation in (6, 9, 12) and address == RELEASE:
            if v.mutation == 6:
                m.put(GLOBAL, STATES[1])
                toggle_word(m, FOLLOW)
                toggle_word(m, LOOT_OPENED)
                dynamic["notice"] ^= MASK
            if v.mutation == 9:
                dynamic["menu"] = MENUS[1]
            if v.mutation == 12:
                m.put(GLOBAL, 0)
                dynamic["pending"] = 1
        if v.mutation == 7 and address == RELEASE_NOTIFICATION:
            m.put(GLOBAL, STATES[1])
            m.put(STATES[1] + 8, v.changed_id, 2)
            dynamic["pending"] ^= 0x10001
        if v.mutation == 8 and address == AMBIENCE:
            toggle_word(m, FOLLOW)
            toggle_word(m, LOOT_OPENED)
        if v.mutation == 10 and address == NOTIFICATION:
            dynamic["notice"] ^= MASK
            m.put(PHASE, 0xFACEB00C)
        if v.mutation == 11 and address == MENU_STATE:
            replace_observed_menu(m, dynamic, result, v.lane)
        if v.mutation == 14:
            ordinal = len(calls) // EVENT_WIDTH
            stamp_callback_state(m, GUARDS[0], ordinal)
        if v.mutation == 15 and address == POSE:
            complete_reward_pose(m)

    def stub(address: int):
        def invoke(machine: Machine) -> None:
            count = ARG_COUNTS[address]
            args = callback_args(machine, count)
            result = 0
            if address == FLAG:
                result = v.flags[min(counts[0], 2)] if args[0] == 0xB6 else 0
                counts[0] += 1
            elif address == PENDING:
                result = dynamic["pending"]
                counts[1] += 1
            elif address == MENU_STATE:
                result = dynamic["menu"]
            elif address == NOTIFICATION:
                result = dynamic["notice"]
            record_dialog_call(machine, calls, address, args, result, GLOBALS, dynamic)
            mutate(address, result)
            for register in (*range(2, 16), 24, 25):
                machine.r[register] = 0xCC000000 | len(calls) // EVENT_WIDTH << 8 | register
            if address in (FLAG, PENDING, MENU_STATE, NOTIFICATION):
                machine.r[2] = result
        return invoke

    for address in ARG_COUNTS:
        m.stub(address, stub(address))
    m.follow.update((END_PAUSE, RESUME_REWARDS, CHECK_UNLOCKS))
    m.execute(ENTRIES[v.function], limit=2048)
    values = [*(m.load(a) for a in GLOBALS), dynamic["menu"], dynamic["notice"],
              dynamic["pending"], *counts, *(m.load(a) for a in MENUS),
              *(m.load(a) for a in GUARDS)]
    values.extend(m.load(address) for base in STATES for address in state_addresses(base))
    values.extend(m.load(DEFINITIONS + index * DEFINITION_STRIDE + 12) for index in range(256))
    writes = []
    for address, size in m.stores:
        if DEFAULT_STACK - 0x80 <= address < DEFAULT_STACK:
            continue
        tag = 1 if address == PHASE else 2 if address == PAUSED else address
        writes.extend((tag, size))
    return {"values": values, "calls": calls, "writes": writes, "stores": list(m.stores)}, v.inputs()


def check(image: Image, v: Vector, observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, v, reference
    values_end = len(VALUE_NAMES) + 1
    if len(native) < values_end + 2:
        return [f"driver printed only {len(native)} values"]
    metadata = native[values_end:]
    write_size = metadata[0]
    if write_size < 0 or len(metadata) < write_size + 2:
        return ["malformed write/call output"]
    writes = metadata[1:write_size + 1]
    call_size, *call_words = metadata[write_size + 1:]
    if call_size != len(call_words) or call_size % EVENT_WIDTH:
        return ["malformed event output"]
    problems = field_problems(("native-diagnostics", *VALUE_NAMES),
                              native[:values_end], [0, *observation["values"]])
    if writes != observation["writes"]:
        problems.append(f"writes native {writes} original {observation['writes']}")
    return [
        *problems,
        *call_problems(
            [call_words[i:i + EVENT_WIDTH] for i in range(0, call_size, EVENT_WIDTH)],
            [observation["calls"][i:i + EVENT_WIDTH]
             for i in range(0, len(observation["calls"]), EVENT_WIDTH)]),
        *store_problems(observation["stores"], [
            (DEFAULT_STACK - 0x80, DEFAULT_STACK), (PHASE, PHASE + 4), (PAUSED, PAUSED + 4)]),
    ]


def describe(image: Image, results: list) -> list[str]:
    lines = [
        "VOID0 continuations; all own instructions execute in the shared R3000 interpreter.",
        "F00224 follows actual original EndRewardPause/ResumeAfterRewards/CheckRewardUnlocks.",
        "Field resume, older ability rewards, menu/notice and lower presentation/query services are bounded stubs.",
        "No gameplay claim: real Live/helper/module integration is a separate central Catch2 gate.",
        "No independent actor41 record, invalid used owners or native layout extensions in original vectors.",
    ]
    for function, (name, entry, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, o, n) for v, o, n in results if v.function == function]
        mismatches = sum(bool(check(image, v, o, n, o)) for v, o, n in selected)
        callbacks = sorted({v.mutation for v, _, _ in selected})
        mutants = sum(name.startswith(FUNCTIONS[function] + "-") for name in MUTANTS)
        lines.append(f"{name} [{entry:08X},{end:08X}) {(end - entry) // 4} instructions "
                     f"sha256={image.sha256(entry, end)} vectors={len(selected)} "
                     f"mismatches={mismatches} callbacks={callbacks} own-original-mutants={mutants}")
    return lines
