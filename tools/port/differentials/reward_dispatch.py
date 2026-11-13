"""Five reward continuations: real original reward cluster calls are followed."""

import random
from dataclasses import dataclass
from pathlib import Path

import differential
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
    ARG_COUNTS as REWARD_SERVICES,
)
from differentials.reward_flow import (
    ENTRIES as REWARD_ENTRIES,
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

FUNCTIONS = ("F00225", "F00226", "F00227", "F00228", "F00229")
ID = "/".join(FUNCTIONS)
ENTRIES = (0x8007B908, 0x8007B938, 0x8007B9A0, 0x8007B9CC, 0x8007B9FC)
ENDS = (*ENTRIES[1:], 0x8007BA98)
ENTRY, END = ENTRIES[0], ENDS[-1]
DRIVER = "reward_dispatch_driver.cpp"
STAGE = Path(__file__).resolve().parents[3]
SOURCES = (str(STAGE / r"Native\Game\src\Battle\RewardFlow.cpp"),)
SOURCE_ROOT = Path(differential.__file__).resolve().parents[2]
INCLUDES = tuple(str(SOURCE_ROOT / path) for path in (
    r"Native\Game\include", r"Native\Game\src", r"Native\Engine\include",
    r"Native\Engine", r"Native\Engine\src", r"Native\Content",
    r"Native\Content\include", r"Native\Content\src",
))
VECTORS, SEED, MUTANT_VECTORS = 5 * 1024, 225226227228229, 5 * 256

GLOBAL, PHASE, FOLLOW, LOOT_OPENED, PAUSED, EFFECTS, LOOT_HEAD = (
    0x800F19CC, 0x800F18F0, 0x800F1864, 0x800F1860, 0x800F1968,
    0x800F1868, 0x800F1900,
)
GLOBALS = (PHASE, FOLLOW, LOOT_OPENED, PAUSED, EFFECTS, GLOBAL, LOOT_HEAD)
STATES, MENUS = (0x80500000, 0x80504000), (0x80700000, 0x80700010)
HEADS = (0, 0x80800000, 0x80800040)
DEFINITIONS, DEFINITION_STRIDE = 0x8004B9DC, 52
GUARDS = (0x800F18F4, 0x800F196C, 0x800F19C8, 0x800F19D0)
UPDATE, MENU_STATE, RELEASE = 0x800C97BC, 0x800CB66C, 0x800CB114
NOTIFICATION, RELEASE_NOTIFICATION = 0x800CB45C, 0x800CB50C
FIELD_RESUME, BEGIN_ABILITY, GRANT_ABILITY = 0x8006C40C, 0x8006F89C, 0x8006FA20
ROOM_PALETTE, AMBIENCE, NOTICE, LOOT_MENU = (
    0x8007B10C, 0x8008E480, 0x800CB2B8, 0x800CB158,
)
FLAG, PENDING, POSE = 0x800BEBF4, 0x8006F760, 0x8009E070
ARG_COUNTS = {
    **REWARD_SERVICES,
    NOTIFICATION: 0, RELEASE_NOTIFICATION: 0,
    FIELD_RESUME: 0, BEGIN_ABILITY: 0, GRANT_ABILITY: 0,
}
EVENT_WIDTH = 21
VALUE_NAMES = (
    "int-result", "phase", "follow", "loot-opened", "paused", "effects",
    "action-owner", "loot-head", "menu-owner", "notification", "pending-source",
    "B6-reads", "AB-reads", "pending-reads", "menu0", "menu1",
    *(f"guard-{i}" for i in range(len(GUARDS))),
    *(f"state-{i}-{field}" for i in range(2)
      for field in ("selection", "primary", "position", "continued", "guard")),
    *(f"definition-{i}" for i in range(256)),
)
PHASES = (3, 4, 5, 6, 0, 1, 2, 7, 0x10003, 0x10004, 0x10005, 0x10006,
          0x80000003, MASK, 0x80000000, 0x7FFFFFFF)
MUTANTS = {
    "F00225-mask-one": {0x8007B914: 0x30420001},
    "F00225-mask-two": {0x8007B914: 0x30420002},
    "F00225-mask-seven": {0x8007B914: 0x30420007},
    "F00225-wrong-effects-word": {0x8007B90C: 0x8C421864},
    "F00225-invert-busy": {0x8007B918: 0x10400003},
    "F00225-no-show": {0x8007B920: 0},
    "F00225-old-grant-not-new-show": {0x8007B920: 0x0C01BE88},
    "F00226-no-update": {0x8007B940: 0},
    "F00226-status-byte": {0x8007B950: 0x80430000},
    "F00226-busy-zero": {0x8007B954: 0x24020000},
    "F00226-no-menu-gate": {0x8007B958: 0},
    "F00226-no-notice-query": {0x8007B960: 0},
    "F00226-invert-notice": {0x8007B968: 0x10400009},
    "F00226-field-resume-not-reward-end": {0x8007B970: 0x0C01B103},
    "F00226-no-reward-end": {0x8007B970: 0},
    "F00226-rows-before-end": {0x8007B970: 0x0C032C45},
    "F00226-no-rows": {0x8007B978: 0},
    "F00226-no-notice-release": {0x8007B980: 0},
    "F00226-old-begin-not-check": {0x8007B988: 0x0C01BE27},
    "F00226-no-check": {0x8007B988: 0},
    "F00227-no-loot-reset": {0x8007B9A8: 0},
    "F00227-half-loot-reset": {0x8007B9A8: 0xA4401860},
    "F00227-loot-reset-wrong-word": {0x8007B9A8: 0xAC401864},
    "F00227-no-follow-reset": {0x8007B9B8: 0},
    "F00227-half-follow-reset": {0x8007B9B8: 0xA4401864},
    "F00227-loot-not-results": {0x8007B9B4: 0x0C01EDD9},
    "F00228-no-loot-reset": {0x8007B9D8: 0},
    "F00228-half-loot-reset": {0x8007B9D8: 0xA4401860},
    "F00228-follow-zero": {0x8007B9DC: 0x24020000},
    "F00228-follow-two": {0x8007B9DC: 0x24020002},
    "F00228-no-follow-reset": {0x8007B9E8: 0},
    "F00228-half-follow-reset": {0x8007B9E8: 0xA4621864},
    "F00228-results-not-loot": {0x8007B9E4: 0x0C01EDF7},
    "F00229-phase-halfword": {0x8007BA04: 0x844318F0},
    "F00229-phase-wrong-word": {0x8007BA04: 0x8C431864},
    "F00229-case-four-is-three": {0x8007BA08: 0x24020003},
    "F00229-case-three-is-two": {0x8007BA1C: 0x24020002},
    "F00229-case-five-is-seven": {0x8007BA30: 0x24020007},
    "F00229-case-six-is-eight": {0x8007BA38: 0x24020008},
    "F00229-three-to-loot": {0x8007BA4C: 0x0C01EE21},
    "F00229-four-to-results": {0x8007BA5C: 0x0C01EE0D},
    "F00229-five-to-old-delay": {0x8007BA6C: 0x0C01E603},
    "F00229-six-to-old-results": {0x8007BA7C: 0x0C01E60F},
    "F00229-three-return-zero": {0x8007BA58: 0x24020000},
    "F00229-four-return-zero": {0x8007BA68: 0x24020000},
    "F00229-five-return-zero": {0x8007BA78: 0x24020000},
    "F00229-six-return-zero": {0x8007BA84: 0x24020000},
    "F00229-low-default-return-one": {0x8007BA24: 0x24020001},
    "F00229-high-default-return-one": {0x8007BA40: 0x24020001},
}


@dataclass(frozen=True)
class Vector(DialogVector):
    b6: tuple[int, ...]
    ab: tuple[int, ...]

    def inputs(self) -> list[int]:
        return [*self.prefix_inputs(), *self.b6, *self.ab, *self.object_inputs()]


def vector(rng: random.Random, number: int) -> Vector:
    function, lane = number % 5, number // 5
    scenario, mutation = lane % 16, lane // 16 % 16
    menus, effects, follow, loot, notification = dialog_boundaries(rng, lane)
    pending = (0, 1, 0x10000, 0x10001, MASK, 0x80000000)[scenario // 2 % 6]
    b6 = ((0, 99, 99), (1, 2, 99), (2, 1, 3), (1, 1, 1))[scenario % 4]
    ab = ((0, 2), (1, 3), (2, 255), (3, 0), (255, 3), (0x10003, 0))[scenario % 6]
    if mutation == 12:
        b6 = (0, 99, 99)
    phase = PHASES[(lane + lane // 16) % len(PHASES)]
    pointer = STATES[0]
    if (function == 0 and effects & 3) or (function == 4 and phase not in (3, 4, 5, 6)):
        pointer = 0
    menu = 0 if mutation == 8 else MENUS[0]
    if function == 4 and phase not in (3, 4, 5, 6):
        menu = 0
    globals_ = (phase, follow, loot, rng.getrandbits(32), effects, pointer,
                HEADS[scenario // 2 % len(HEADS)])
    return Vector(function=function, lane=lane, mutation=mutation,
                  seed=rng.getrandbits(32), pending=pending, notification=notification,
                  menu=menu, changed_id=(0, 1, 255)[scenario % 3], globals=globals_,
                  b6=b6, ab=ab, menus=menus, states=selection_states(rng, scenario),
                  guards=tuple(rng.getrandbits(32) for _ in GUARDS),
                  unlocked=int(scenario % 8 == 3))


def run(m: Machine, v: Vector) -> tuple[dict, list[int]]:
    dynamic = prepare_dialog_machine(m, v, GLOBALS, GUARDS)
    counts = [0, 0, 0]
    calls = []

    def mutate(address: int, result: int) -> None:
        ordinal = len(calls) // EVENT_WIDTH
        if v.mutation in (1, 14):
            stamp_callback_state(m, GUARDS[0], ordinal)
            if v.mutation == 1:
                toggle_word(m, FOLLOW)
                toggle_word(m, LOOT_OPENED)
        if v.mutation == 2 and address == AMBIENCE:
            toggle_word(m, FOLLOW)
            toggle_word(m, LOOT_OPENED)
        if v.mutation in (3, 5) and address == ROOM_PALETTE:
            m.put(GLOBAL, STATES[1])
            m.put(STATES[1] + 8, v.changed_id, 2)
            if v.mutation == 5:
                dynamic["pending"] ^= MASK
        if v.mutation in (4, 5) and address == NOTICE:
            target = STATES[0] if v.mutation == 5 else STATES[1]
            m.put(GLOBAL, target)
            m.put(target + 8, v.changed_id, 2)
        if v.mutation == 6 and address == RELEASE:
            m.put(GLOBAL, STATES[1])
            toggle_word(m, FOLLOW)
            toggle_word(m, LOOT_OPENED)
            dynamic["notice"] ^= MASK
        if v.mutation == 7 and address == RELEASE_NOTIFICATION:
            m.put(GLOBAL, STATES[1])
            m.put(STATES[1] + 8, v.changed_id, 2)
            dynamic["pending"] ^= 0x10001
        if v.mutation == 8 and address == UPDATE:
            dynamic["menu"] = MENUS[1]
            dynamic["notice"] = 0
        if v.mutation == 9 and address == LOOT_MENU:
            m.put(LOOT_HEAD, HEADS[2] if m.load(LOOT_HEAD) != HEADS[2] else HEADS[1])
            m.put(PHASE, 0xFACEB00C)
        if v.mutation == 10 and address == NOTIFICATION:
            dynamic["notice"] ^= MASK
            dynamic["menu"] = 0
            m.put(PHASE, 0xFACEB00C)
        if v.mutation == 11 and address == MENU_STATE:
            replace_observed_menu(m, dynamic, result, v.lane)
        if v.mutation == 12 and address == RELEASE:
            m.put(GLOBAL, 0)
            dynamic["pending"] = 1
        if v.mutation == 13 and address == UPDATE:
            m.put(GLOBAL, STATES[1])
            toggle_word(m, FOLLOW)
            toggle_word(m, LOOT_OPENED)
        if v.mutation == 15 and address == POSE:
            complete_reward_pose(m)

    def stub(address: int):
        def invoke(machine: Machine) -> None:
            count = ARG_COUNTS[address]
            args = callback_args(machine, count)
            result = 0
            if address == FLAG:
                slot = 0 if args[0] == 0xB6 else 1
                values = v.b6 if slot == 0 else v.ab
                if args[0] not in (0xB6, 0xAB):
                    raise ValueError(f"unexpected flag {args[0]:#x}")
                result = values[min(counts[slot], len(values) - 1)]
                counts[slot] += 1
            elif address == PENDING:
                result = dynamic["pending"]
                counts[2] += 1
            elif address == MENU_STATE:
                result = dynamic["menu"]
            elif address == NOTIFICATION:
                result = dynamic["notice"]
            record_dialog_call(machine, calls, address, args, result, GLOBALS, dynamic)
            mutate(address, result)
            for register in (*range(2, 16), 24, 25):
                machine.r[register] = 0xCC000000 | ordinal_register(len(calls), register)
            if address in (FLAG, PENDING, MENU_STATE, NOTIFICATION):
                machine.r[2] = result
        return invoke

    for address in ARG_COUNTS:
        m.stub(address, stub(address))
    m.follow.update((*REWARD_ENTRIES, 0x8007B884, *ENTRIES,
                     0x800797BC, 0x8007980C, 0x8007983C))
    m.execute(ENTRIES[v.function], limit=4096)
    result = m.r[2] if v.function == 4 else 0
    values = [result, *(m.load(a) for a in GLOBALS), dynamic["menu"],
              dynamic["notice"], dynamic["pending"], *counts,
              *(m.load(a) for a in MENUS), *(m.load(a) for a in GUARDS)]
    values.extend(m.load(address) for base in STATES for address in state_addresses(base))
    values.extend(m.load(DEFINITIONS + index * DEFINITION_STRIDE + 12) for index in range(256))
    writes = []
    tags = {PHASE: 1, PAUSED: 2, LOOT_OPENED: 4, FOLLOW: 5}
    for address, size in m.stores:
        if DEFAULT_STACK - 0x100 <= address < DEFAULT_STACK:
            continue
        if address in tags:
            tag = (tags[address], 0)
        elif (address - DEFINITIONS - 12) % DEFINITION_STRIDE == 0:
            tag = (3, (address - DEFINITIONS - 12) // DEFINITION_STRIDE)
        else:
            tag = (address, 0)
        writes.extend((*tag, size))
    return {"values": values, "calls": calls, "writes": writes,
            "stores": list(m.stores)}, v.inputs()


def ordinal_register(call_words: int, register: int) -> int:
    return call_words // EVENT_WIDTH << 8 | register


def check(image: Image, v: Vector, observation: dict,
          native: list[int], reference: dict) -> list[str]:
    del image, v, reference
    base = len(VALUE_NAMES) + 1
    if len(native) < base + 2:
        return [f"driver printed only {len(native)} values"]
    write_end = base + 1 + native[base]
    if write_end >= len(native) or write_end + 1 + native[write_end] != len(native):
        return ["malformed write/call output"]
    problems = field_problems(("native-diagnostics", *VALUE_NAMES),
                              native[:base], [0, *observation["values"]])
    native_calls, original_calls = native[write_end + 1:], observation["calls"]
    if len(native_calls) % EVENT_WIDTH:
        return ["malformed event width"]
    problems.extend(call_problems(
        [native_calls[i:i + EVENT_WIDTH] for i in range(0, len(native_calls), EVENT_WIDTH)],
        [original_calls[i:i + EVENT_WIDTH] for i in range(0, len(original_calls), EVENT_WIDTH)]))
    if native[base + 1:write_end] != observation["writes"]:
        problems.append(f"writes native {native[base + 1:write_end]} original {observation['writes']}")
    allowed = [(DEFAULT_STACK - 0x100, DEFAULT_STACK),
               *((address, address + 4) for address in (PHASE, PAUSED, LOOT_OPENED, FOLLOW))]
    allowed.extend((DEFINITIONS + i * DEFINITION_STRIDE + 12,
                    DEFINITIONS + i * DEFINITION_STRIDE + 16) for i in range(256))
    return problems + store_problems(observation["stores"], allowed)


def describe(image: Image, results: list) -> list[str]:
    lines = [
        "100 own original instructions; four VOID0 entries and the distinct INT0 dispatcher.",
        "All actual reward cluster calls are followed; new reward math/policies are never stubbed.",
        "Bounded externals: presentation, menu/notice, flags, ability queries and actor/effect/platform services.",
        "Older field/ability services are bounded comparison controls reached only by deliberate wrong-call mutants.",
        "Raw high AB return words are masking controls outside the production U8 flag service domain.",
        "Original ActionState/actor41 slot alias is preserved; no independent actor41 or native extensions.",
        "No gameplay, EC retirement, F00830/#296 or row30 completion claim.",
    ]
    for function, (name, entry, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, o, n) for v, o, n in results if v.function == function]
        mismatches = sum(bool(check(image, v, o, n, o)) for v, o, n in selected)
        callbacks = sorted({v.mutation for v, _, _ in selected})
        phases = sorted({v.globals[0] for v, _, _ in selected}) if function == 4 else []
        mutants = sum(label.startswith(name + "-") for label in MUTANTS)
        lines.append(f"{name} [{entry:08X},{end:08X}) {(end - entry) // 4} instructions "
                     f"sha256={image.sha256(entry, end)} vectors={len(selected)} "
                     f"mismatches={mismatches} callbacks={callbacks} "
                     f"own-original-mutants={mutants} phases={phases}")
    return lines
