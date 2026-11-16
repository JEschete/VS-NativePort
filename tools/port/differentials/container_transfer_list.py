"""Recorded execution of MENUD func_80107490, the container item list (F02985).

Every step of the list runs against a recorded world: item records and display order live in
mapped memory and the original's own container helpers run on them, while menus, dialogs and
transfer bookkeeping are recorded calls answered from per-vector queues."""

from __future__ import annotations

import random

from differential import DEFAULT_STACK, MASK, s32
from differentials import container_transfer_common as common

ENTRY, END = common.RANGES[common.LIST]
STEP_STATE = 0x80109A68
EMPTY_STATE = 0x80109A69
FIRST_STATE = 0x80109A6A
SELECTED_STATE = 0x80109A6C
ACTION_IDS = 0x80109A60
MENU_MEMORY = 0x800F4F88
MENU_MEMORY_BYTES = 0x12
FREEZE_TAB_ARROWS = 0x801023E3
ROW_TYPES_POINTER = 0x800F4E84
ROW_TYPES = 0x80B10000
HEAP = 0x80C00000
HEAP_BYTES = 0x6C00
HEAP_TYPES, HEAP_TEXTS = HEAP + 0x6000, HEAP + 0x6400
HELP_ENTRIES = 663
INDICES = common.ACTIVE + 0x3C00
INDEX_BYTES = 704 * 2
STAGED = common.STORAGE + common.STORAGE_OFFSETS["staged"]
FOLLOW = (0x80102C94, 0x80103070, 0x80102F88, 0x80103C9C, 0x80103D14, 0x80102CD0, 0x80102D80)

# Call codes beyond the shared ones; the driver's enum continues with the same numbers.
(CLEAR_MEMORY, CLEAR_WORDS, ACQUIRE, RELEASE, BUILD_ROWS, MARK_ROW, CREATE_ROWS, RESTORE_ROWS,
 SELECTED_ROW, CLOSE_ROWS, DETAILS, UNDO, STAGE, CAPACITY_ERROR, PREVIEW_MISC, COPY, COPY_SOURCE,
 REBUILD_PENDING, ACTION_ROWS, ACTION_TEXT, ACTION_TYPES, HIGHLIGHTED, SHOW_ACTION,
 UPDATE_ACTIONS, ACTION_RESULT, DISMISS, DISCARD, SORT, CONFIRM) = range(28, 57)

# Item ids and links that reach every branch of the action rows.
MISC_IDS = (1, 1, 1, 5, 7, 0x1C9, 0x1CA, 0x1D0)
GEM_LINKS = (0, 0, 1, 2, 0x81, 0x82)
STEPS = ("init", 0, 1, 2, 2, 2, 2, 3, 3, 4, 5, 6, 6, 6, 7, 8, 8, 8, 8, 9, 9, 9, 10, 10, 11, 12,
         13, 14, 15, 15, 16, 17)
ITEM_STEPS = (6, 8, 9, 10)
TITLE_X = (0, 0xB4, 0xB5, 0xCC, 0xCD, 0x100, 0xFFF0)
BUTTONS = (0, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80, 0xA0, 0x1000, 0x2000, 0x8000, 0x8004, 0x2008,
           0xA00C, 0x2004)


# ----------------------------------------------------------------- records
def write(entries: list, owner: int, offset: int, value: int, size: int) -> None:
    for byte in range(size):
        entries.append((owner, offset + byte, value >> 8 * byte & 0xFF))


def record_fields(rng: random.Random, category: int,
                  misc_ids: tuple = MISC_IDS) -> list[tuple[int, int, int]]:
    """(offset, value, size) of one present record of a category."""
    if category == 0:
        return [(1, rng.randrange(1, 65), 1), (2, rng.randrange(1, 65), 1),
                (3, rng.randrange(2), 1)]
    if category == 1:
        return [(0, rng.randrange(1, 256), 1), (42, rng.choice((0, 0, 1, 2, 3)), 1)]
    if category == 2:
        return [(0, rng.randrange(1, 256), 2), (12, rng.choice((0, 0, 1, 2, 3)), 2)]
    if category == 3:
        return [(1, rng.randrange(2), 1), (4, rng.randrange(1, 256), 1)]
    if category == 4:
        return [(0, rng.randrange(1, 256), 1), (38, rng.choice((0, 0, 1, 2)), 1)]
    if category == 5:
        return [(0, rng.randrange(1, 256), 2), (24, rng.choice(GEM_LINKS), 2)]
    return [(0, rng.choice(misc_ids), 2), (2, rng.choice((1, 2, 30, 50, 60, 99)), 1)]


def place(entries: list, owner: int, rng: random.Random, category: int, slot: int,
          misc_ids: tuple = MISC_IDS) -> None:
    base = common.RECORD_OFFSETS[category] + slot * common.STRIDES[category]
    for offset, value, size in record_fields(rng, category, misc_ids):
        write(entries, owner, base + offset, value, size)


def active_records(rng: random.Random, number: int, chosen: int, chosen_ids: tuple = ()):
    """Present records and their display order for every active category; the chosen category
    always has records and alternate rounds stack a single misc item so rows merge."""
    entries, indices, counts = [], [], []
    misc_ids = (1,) if number // len(STEPS) % 2 == 0 else MISC_IDS
    for category in range(7):
        count = rng.choice((0, 1, 3, 6, 12) if category else (0, 2, 9, 12, 20, 24, 30))
        if (number + category) % 9 == 0:
            count = 0
        if category == chosen:
            count = max(count, 2)
        slots = rng.sample(range(common.CAPACITY[category]), count)
        for row, slot in enumerate(slots):
            place(entries, 0, rng, category, slot,
                  chosen_ids if category == chosen and chosen_ids else misc_ids)
            indices.append((common.CATEGORY_INDICES[category] + row, slot + 1))
        counts.append(count)
    return entries, indices, counts


def staged_records(rng: random.Random, entries: list, stacks: int | None = None) -> list[int]:
    """Staged items: sometimes 63 or exactly the 64-item limit of misc stacks; returns pending
    tokens, whose records are staged unless they would overwrite a stack. A forced stack count
    stages nothing else, and 63 forced full stacks keep the listed item transferable."""
    forced = stacks
    tokens_count = rng.choice((0, 1, 3)) if stacks is None else 0
    stacks = rng.choice((0, 63, 64)) if stacks is None else stacks
    if stacks:
        for slot in range(stacks):
            base = common.RECORD_OFFSETS[6] + slot * 4
            write(entries, 2, base, rng.choice(MISC_IDS), 2)
            write(entries, 2, base + 2, 100 if forced == 63 else rng.randrange(1, 101), 1)
    tokens = []
    for _ in range(tokens_count):
        category = rng.randrange(1, 7)
        slot = rng.randrange(min(common.CAPACITY[category], 255))
        if category != 6 or slot >= stacks:
            place(entries, 2, rng, category, slot)
        tokens.append(category << 8 | slot + 1)
    return tokens


# ----------------------------------------------------------------- vectors
def vector(rng: random.Random, number: int) -> dict:
    step = STEPS[number % len(STEPS)]
    scenario = number // len(STEPS)
    category = rng.randrange(8)
    if step in ITEM_STEPS:
        category = min((scenario + number) % 9, 6)
    elif step == 2 and scenario % 4 == 1:
        category = scenario // 4 % 6
    key_items = (0x1CA,) if step == 8 and category == 6 and scenario % 4 == 2 else ()
    entries, indices, counts = active_records(rng, number, category, key_items)
    forced = 63 + scenario % 2 if step == 8 and category == 6 and scenario % 4 < 2 else None
    pending = staged_records(rng, entries, forced)
    if rng.randrange(4) == 0 or forced:
        pending = []
    items = max(counts[category] if category < 7 else 0, 1)
    rows = list(range(items))
    script = common.script(rng, {
        common.ROWS_IDLE: (0, 1, 1),
        BUILD_ROWS: (0, 1, 3, 5),
        SELECTED_ROW: (*(row for row in rows[:4]), 0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFD),
        CLOSE_ROWS: (0, 3, 0x105, 0xA07),
        DETAILS: (0, 1, 1, 0xFFFFFFFE, 0xFFFFFFFF),
        CAPACITY_ERROR: (0, 1, 2, 3),
        COPY: (0, 1, 0x40, 0x41),
        HIGHLIGHTED: (0, 1, 2),
        ACTION_RESULT: (0xFFFFFFFF, 0xFFFFFFFE, 0xFFFFFFFD, 0, 1, 2, 3, 4),
        DISCARD: (0, 0, 1, 0xFFFFFFFE),
        SORT: (0, 0, 1, 0xFFFFFFFE),
        CONFIRM: (0, 1, 0xFFFFFFFF, 0xFFFFFFFE)})
    empty = rng.randrange(2)
    pressed = rng.choice((*BUTTONS, 0x40, 0x80))
    if step == 2 and scenario % 4 == 0:
        empty, pressed = 1, rng.choice((0x10, 0x40, 0xA0))
        if scenario % 8 == 0:
            pressed, pending = 0x40, []
    elif step == 2 and scenario % 4 == 1:
        empty, pressed = 0, 0x80
        script[SELECTED_ROW] = [0xFFFFFFFF]
        if scenario % 8 == 5:
            script[SELECTED_ROW], pending = [0xFFFFFFFE], []
        script[common.ROWS_IDLE] = [1]
    x = rng.choice(TITLE_X)
    if step == 7 and scenario < 2 * len(TITLE_X):
        x = TITLE_X[scenario % len(TITLE_X)]
        script[DETAILS] = [1]
    if step == 8 and category == 6:
        script[COPY] = [0x41 if scenario % 4 == 3 else 1]
    actions = [rng.randrange(7) for _ in range(8)]
    if step == 9 and category == 6 and scenario % 2 == 0:
        script[ACTION_RESULT] = [2]
        actions[3] = 3
    flags = [rng.randrange(2), rng.randrange(3), rng.randrange(2), rng.randrange(3),
             rng.randrange(3), rng.randrange(3), rng.randrange(3), rng.randrange(2)]
    return {
        "kind": common.LIST,
        "direction": rng.choice((1, 2)) if step == "init" else 0,
        "step": 0 if step == "init" else step,
        "empty": empty,
        "first": rng.randrange(2),
        "selected": rng.randrange(1, items + 1),
        "actions": actions,
        "flags": flags,
        "memory": [(category + 1) & 7 if rng.randrange(4) or step in ITEM_STEPS
                   else rng.choice((0, 8, 9))]
                  + [rng.randrange(256) for _ in range(MENU_MEMORY_BYTES - 1)],
        "settings": rng.randrange(1 << 32) & ~0xFF00 | rng.randrange(2) << 8,
        "freeze": rng.randrange(2),
        "pressed": pressed,
        "held": rng.choice(BUTTONS),
        "repeated": rng.choice(BUTTONS),
        "x": x,
        "help": [rng.randrange(0x3400, 0x3700) for _ in range(4)],
        "records": entries,
        "indices": indices,
        "pending": pending,
        "script": script,
    }


# ----------------------------------------------------------------- recorded world
def clear_word(address: int) -> int:
    return 7 << 24 if address == common.PENDING else common.owner_word(address)


def checksum(machine, address: int, size: int) -> int:
    return common.string_hash(bytes(machine.bytes(address, size)))


def install(machine, recorder: common.Recorder) -> None:
    common.install_common(machine, recorder)
    machine.follow.update(FOLLOW)
    hook = recorder.hook

    def from_list(m) -> bool:
        return ENTRY <= m.r[31] < END

    def memset(m):
        for offset in range(m.r[6]):
            m.put(m.r[4] + offset, m.r[5], 1)
        recorder.record(CLEAR_MEMORY, m.r[6])
    machine.stub(0x80026EB4, memset)

    def zero_words(m):
        for offset in range(0, m.r[5], 4):
            m.put(m.r[4] + offset, 0)
        if from_list(m) and m.r[4] != ROW_TYPES:
            recorder.record(CLEAR_WORDS, clear_word(m.r[4]), m.r[5])
    machine.stub(0x800CCC1C, zero_words)

    def acquire(m):
        machine.store(HEAP, bytes(HEAP_BYTES))
        recorder.record(ACQUIRE, m.r[4])
        m.r[2] = HEAP
    machine.stub(0x80043E3C, acquire)
    machine.stub(0x80043C60, lambda m: recorder.record(RELEASE, int(m.r[4] == HEAP)))

    def build(m):
        recorder.record(BUILD_ROWS, m.r[4], int(m.r[5] == HEAP_TEXTS)
                        | int(m.r[6] == HEAP_TYPES) << 1 | int(m.r[7] == HEAP) << 2)
        m.r[2] = recorder.answer(BUILD_ROWS)
    machine.stub(0x80106C64, build)

    def create(m):
        for row in range(256):
            value = m.load(HEAP_TYPES + row * 4)
            if value:
                recorder.record(MARK_ROW, row, value)
        heap = int(m.r[6] == HEAP_TEXTS and m.r[7] == HEAP_TYPES)
        recorder.record(CREATE_ROWS, m.r[4], m.r[5], m.load(common.SETTINGS + 1, 1) | heap << 8)
    machine.stub(0x801005E0, create)
    hook(0x801071D8, RESTORE_ROWS, 4)
    hook(0x801008C8, SELECTED_ROW, returns=True)
    hook(0x80100814, CLOSE_ROWS, returns=True)
    hook(0x80105008, DETAILS, 4, returns=True)
    hook(0x801068BC, UNDO, 4)
    hook(0x80106948, STAGE, 4, 5)
    hook(0x80106784, CAPACITY_ERROR, 4, 5, returns=True)
    hook(0x801031A0, PREVIEW_MISC)
    machine.stub(0x801037D8, lambda m: recorder.record(
        common.REBUILD_INDICES, common.owner_word(m.r[5]), m.r[4]))

    def copy(m):
        recorder.record(COPY, m.r[4], common.owner_word(m.r[5]), m.r[6])
        recorder.record(COPY_SOURCE, common.owner_word(m.r[7]))
        m.r[2] = recorder.answer(COPY)
    machine.stub(0x801032AC, copy)

    def rebuild_pending(m):
        recorder.record(REBUILD_PENDING)
        values = [m.load(common.PENDING + index * 2, 2) for index in range(1, 64)] + [0]
        for index, value in enumerate(values):
            m.put(common.PENDING + index * 2, value, 2)
    machine.stub(0x80106A50, rebuild_pending)

    def action_rows(m):
        recorder.record(ACTION_ROWS, m.r[4], m.r[5])
        for text in range(m.r[4] * 2):
            recorder.record(ACTION_TEXT, common.text_word(m.load(m.r[6] + text * 4)))
        types = [m.load(m.r[7] + index * 4) for index in range(4)]
        recorder.record(ACTION_TYPES, *types[:3])
        recorder.record(ACTION_TYPES, types[3])
    machine.stub(0x800FF0EC, action_rows)
    hook(0x800FF348, HIGHLIGHTED, returns=True)
    hook(0x800FF9E4, SHOW_ACTION, 4)
    hook(0x800FF43C, UPDATE_ACTIONS)
    hook(0x800FF360, ACTION_RESULT, returns=True)
    hook(0x800FFBA8, DISMISS)
    hook(0x80105D24, DISCARD, 4, returns=True)
    hook(0x80105ACC, SORT, 4, returns=True)
    hook(0x801072B0, CONFIRM, 4, returns=True)


def run(machine, item: dict) -> tuple[dict, list[int]]:
    recorder = common.Recorder(machine, item["script"])
    install(machine, recorder)
    common.map_world(machine, item)
    machine.map_zero(HEAP, HEAP_BYTES)
    machine.map_zero(ROW_TYPES, 0x100)
    machine.map_zero(common.HELP, common.HELP_BYTES)
    machine.put(common.HELP_POINTER, common.HELP)
    machine.put(ROW_TYPES_POINTER, ROW_TYPES)
    for index, value in enumerate(item["help"]):
        machine.put(common.HELP + (HELP_ENTRIES + index) * 2, value, 2)
    for index, value in item["indices"]:
        machine.put(INDICES + index * 2, value, 2)
    for index, value in enumerate(item["pending"]):
        machine.put(common.PENDING + index * 2, value, 2)
    machine.put(STEP_STATE, item["step"], 1)
    machine.put(EMPTY_STATE, item["empty"], 1)
    machine.put(FIRST_STATE, item["first"], 1)
    machine.put(SELECTED_STATE, item["selected"])
    for index, value in enumerate(item["actions"]):
        machine.put(ACTION_IDS + index, value, 1)
    for index, value in enumerate(item["flags"]):
        machine.put(common.FLAGS + index, value, 1)
    for index, value in enumerate(item["memory"]):
        machine.put(MENU_MEMORY + index, value, 1)
    machine.put(FREEZE_TAB_ARROWS, item["freeze"], 1)
    machine.put(common.PRESSED, item["pressed"])
    machine.put(common.HELD, item["held"])
    machine.put(common.REPEATED, item["repeated"])
    for row in range(common.ROW_COUNT):
        machine.put(common.ROWS + row * common.ROW_STRIDE + 0xD, 1, 1)
    machine.put(common.ROWS + 0x14, item["x"], 2)
    machine.r[4] = item["direction"]
    machine.execute(ENTRY)
    values = [s32(machine.r[2]), machine.load(STEP_STATE, 1), machine.load(EMPTY_STATE, 1),
              machine.load(FIRST_STATE, 1), s32(machine.load(SELECTED_STATE)),
              *(machine.load(ACTION_IDS + index, 1) for index in range(8)),
              *(machine.load(common.FLAGS + index, 1) for index in range(8)),
              *(machine.load(MENU_MEMORY + index, 1) for index in range(MENU_MEMORY_BYTES)),
              machine.load(common.SETTINGS), machine.load(FREEZE_TAB_ARROWS, 1),
              common.owner_word(machine.load(common.ACTIVE_POINTER)),
              common.owner_word(machine.load(common.OTHER_POINTER)),
              checksum(machine, common.ACTIVE, 0x3C00 + INDEX_BYTES),
              checksum(machine, STAGED, 0x3C00), checksum(machine, common.PENDING, 0x80),
              *common.row_values(machine)]
    allowed = [(DEFAULT_STACK - 0x4000, DEFAULT_STACK), (ACTION_IDS, SELECTED_STATE + 4),
               (common.FLAGS, common.FLAGS + 8), (MENU_MEMORY, MENU_MEMORY + MENU_MEMORY_BYTES),
               (common.SETTINGS, common.SETTINGS + 4), (FREEZE_TAB_ARROWS, FREEZE_TAB_ARROWS + 1),
               (common.OTHER_POINTER, common.ACTIVE_POINTER + 4),
               (common.ACTIVE, INDICES + INDEX_BYTES),
               (common.STORAGE, common.STORAGE + common.STORAGE_BYTES),
               (common.ROWS, common.ROWS + common.ROW_STRIDE * common.ROW_COUNT),
               (HEAP, HEAP + HEAP_BYTES), (ROW_TYPES, ROW_TYPES + 0x10)]
    inputs = [common.LIST, item["direction"], item["step"], item["empty"], item["first"],
              item["selected"], *item["actions"], *item["flags"], *item["memory"],
              item["settings"], item["freeze"], item["pressed"], item["held"], item["repeated"],
              item["x"], *common.menu_text(machine), *item["help"],
              len(item["records"]), *(word for entry in item["records"] for word in entry),
              len(item["indices"]), *(word for entry in item["indices"] for word in entry),
              len(item["pending"]), *item["pending"],
              *common.script_inputs(item["script"])]
    return common.recorded(machine, recorder, values, allowed), inputs


__all__ = ["vector", "run", "MASK"]
