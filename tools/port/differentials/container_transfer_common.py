"""Asset-free metadata and recorded MENUD execution for the container list transfer functions."""

from __future__ import annotations

import functools
import random

from differential import DEFAULT_STACK, MASK, s32, store_problems
from differentials import container_pending_rows_common as rows_common

MODULES = rows_common.MODULES
FUNCTIONS = (2951, 2952, 2985, 2986, 2987)
RANGES = (
    (0x801041E0, 0x8010425C),
    (0x8010425C, 0x801042D0),
    (0x80107490, 0x801089BC),
    (0x801089BC, 0x80108C6C),
    (0x80108C6C, 0x8010952C),
)
WEAPON, SHIELD, LIST, DIRECTION, SCREEN = range(5)

# Weapon/shield rows: the row-builder fixture of the pending-row cases.
DESCRIPTION_BYTES = rows_common.DESCRIPTION_BYTES
BUFFER, TYPES, TEXTS = rows_common.BUFFER, rows_common.TYPES, rows_common.TEXTS
OWNER = rows_common.OWNERS[0]
ROW_FOLLOW = rows_common.FOLLOW
TYPE_FILL = 0xA5A5A5A5
TEXT_FILL = (0x50000000, 0x50000004)

# Recorded-world addresses.
MENU_TEXT = 0x8010952C
MENU_TEXT_ENTRIES = 48
HELP_POINTER = 0x80102540
HELP = rows_common.HELP
HELP_BYTES = 28672
STORAGE_POINTER = 0x8010245C
STORAGE = 0x80600000
STORAGE_BYTES = 0x14730
ACTIVE_POINTER, OTHER_POINTER = 0x80109A8C, 0x80109A88
ACTIVE, OTHER = 0x80700000, 0x80710000
CONTAINER_BYTES = 0x3C00 + 704 * 2
STORAGE_OFFSETS = {"staged": 0x87B0, "preview": 0x4BB0, "container": 0xC430,
                   "bag": 0x105B0, "stored": 0xFB0}
PENDING = STORAGE + 0xC3B0
ROWS = 0x80B00000
ROW_STRIDE = 0x44
ROW_COUNT = 128
ROW_FIELDS = ((0, 1), (6, 1), (0xD, 1), (0x10, 2), (0x12, 2), (0x14, 2), (0x18, 2))
SCRATCH = 0x1F800000
CHAIN = 0x00012340
FLAGS = 0x80109A7A
SETTINGS = 0x80060020
SELECTION = 0x800F4F86
TOKEN = 0x800F4F88
PRESSED, HELD, REPEATED = 0x8005E1D0, 0x8005E238, 0x8005DFDC
MENU_STATE = 0x800F51C0
ANIMATION = 0x800EBBC8
CONTAINER_CAPACITIES = 0x80109944
INVENTORY_CAPACITIES = 0x801022A0
TRANSFERRED_MISC = 0x80109A82
PUBLISHED = (0x80102470, 0x80102464, 0x80102460, 0x8010246C, 0x80102468, 0x80102458)
PUBLISHED_OFFSETS = (0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300)
DIRECTION_STATE = (0x80109A70, 0x80109A2C, 0x80109A2D, 0x80109A2E)
SCREEN_STATE = ((0x80109A74, 4), (0x80109A78, 1), (0x80109A79, 1))
CATEGORY_INDICES = (0, 32, 96, 160, 192, 256, 448)
CAPACITY = (32, 64, 64, 32, 64, 192, 256)
RECORD_OFFSETS = (0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300, 0x3800)
STRIDES = (32, 44, 16, 48, 40, 28, 4)
ID_OFFSETS = (1, 0, 0, 4, 0, 0, 0)

# Recorded call codes; the native driver's world records the same words.
(SOUND, CLEAR_MENU, FLY_OUT, ROWS_IDLE, MENU_ITEM, MENU_ITEM_TEXT, TEXT, TEXT_STYLE, CURSOR,
 INFORMATION, CAPACITIES, LOAD_TEXT, INIT_INFORMATION, PANEL, STORE_SNAPSHOT, SNAPSHOT_BAG,
 COMMIT_BAG, REBUILD_INDICES, UNLOAD_TEXT, ITEM_LIST, DIRECTION_MENU, ICONS, REFRESH_MISC,
 PROMPT, PROMPT_SIZE, BUTTON, CAPACITY_COUNT) = range(1, 28)
CODES = 64
SOUNDS = {0x800C02E8: 0, 0x800C02E0: 1, 0x800C02F0: 2, 0x800C02F8: 3}
COUNT_FOLLOW = (0x80103070, 0x80102F88, 0x80102C94)


WEAPON_MUTANTS = {
    "weapon-stride-16": {0x80104214: 0x00021100},
    "name-at-record-start": {0x80104238: 0x26100000},
    "name-store-omitted": {0x8010423C: 0},
    "stats-build-skipped": {0x8010421C: 0},
    "types-alias-texts": {0x8010422C: 0x02203021},
}
SHIELD_MUTANTS = {
    "index-times-four": {0x8010428C: 0x00022880},
    "shield-stride-24": {0x80104294: 0x000528C0},
    "table-offset-0x3c0": {0x80104298: 0x24A503C0},
    "stats-build-skipped": {0x8010429C: 0},
    "buffer-alias-texts": {0x801042B4: 0x02203821},
}
DIRECTION_MUTANTS = {
    "reset-selection-omitted": {0x801089F8: 0},
    "init-blink-kept": {0x80108A04: 0},
    "row-y-base-0x20": {0x80108A50: 0x24C60020},
    "row-target-0xc3": {0x80108A88: 0x240400C3},
    "selected-store-omitted": {0x80108AD0: 0},
    "triangle-mask-circle": {0x80108AEC: 0x30620020},
    "cross-result-exit": {0x80108B24: 0x2402FFFE},
    "circle-result-zero-based": {0x80108B3C: 0x26220000},
    "blink-step-8": {0x80108B48: 0x24420008},
    "brightness-base-0x41": {0x80108B70: 0x24420041},
    "cursor-gap-mask-2": {0x80108BA0: 0x30420002},
    "cursor-animation-store-omitted": {0x80108BC4: 0},
    "wrap-by-two": {0x80108BF0: 0x2631FFFE},
    "selection-store-omitted": {0x80108C0C: 0},
    "help-index-12": {0x80108C28: 0x94630018},
    "capacities-call-omitted": {0x80108C4C: 0},
}
SCREEN_MUTANTS = {
    "published-blade-offset": {0x80108CAC: 0x24620A01},
    "controls-slide-reset-omitted": {0x80108D78: 0},
    "reopen-delay-not-decremented": {0x80108DB4: 0x24420000},
    "direction-limit-2": {0x80108DE8: 0x2C420002},
    "direction-name-index-0xe": {0x80108E1C: 0x2442000E},
    "title-row-unselected": {0x80108E40: 0x24030000},
    "list-step-4": {0x80108E70: 0x24020004},
    "reopen-delay-4": {0x80108EE4: 0x24020004},
    "empty-flag-bit-3": {0x80108F84: 0x000210C0},
    "empty-when-six-scanned": {0x80108F7C: 0x3A420006},
    "return-state-plus-1": {0x80108FAC: 0x24630001},
    "title-slide-limit-9": {0x80108FDC: 0x2C620009},
    "pending-count-limit-63": {0x80109114: 0x2A42003F},
    "icon-slide-limit-3": {0x801091A0: 0x2C620003},
    "refresh-misc-omitted": {0x80109208: 0},
    "prompt-base-x-0x11": {0x80109284: 0x24020011},
    "status-color-0x202021": {0x80109300: 0x34422021},
    "end-color-shift-1": {0x80109364: 0x00063042},
    "misc-row-at-index-5": {0x8010944C: 0x24020005},
    "gadget-misc-limit-0x101": {0x801094E0: 0x24040101},
}
LIST_MUTANTS = {
    "first-entry-store-omitted": {0x801074CC: 0},
    "clear-staged-bytes-0x3800": {0x801074F8: 0x24053800},
    "rows-allocation-0x6000": {0x801075F4: 0x24046000},
    "freeze-store-omitted": {0x8010760C: 0},
    "empty-store-omitted": {0x80107660: 0},
    "rows-layout-0x19300": {0x801076DC: 0x34429300},
    "rows-base-0x52": {0x801076E4: 0x26850052},
    "empty-cross-mask-circle": {0x80107760: 0x30620020},
    "picked-row-flag-4": {0x801078F4: 0x34420004},
    "misc-overflow-base-99": {0x80107BE8: 0x2482FF9D},
    "misc-stack-limit-99": {0x80107BF4: 0x24030063},
    "merged-misc-clear-omitted": {0x80107C04: 0},
    "title-minimum-x-0xb6": {0x80107C84: 0x28A200B6},
    "title-overflow-x-0xce": {0x80107C8C: 0x28A200CE},
    "title-shift-0x10": {0x80107C98: 0x2482FFF0},
    "title-target-0xb5": {0x80107CA8: 0x240200B5},
    "transfer-key-item-0x1cb": {0x80107DA8: 0x2C4201CB},
    "staged-limit-0x3f": {0x80107F0C: 0x2402003F},
    "transfer-name-0x1e": {0x80107FD4: 0x2482001E},
    "transfer-help-0x21": {0x8010800C: 0x24820021},
    "discard-help-entry-664": {0x8010826C: 0x94A20530},
    "action-menu-base-0x5a": {0x8010838C: 0x2685005A},
    "discard-category-shift-11": {0x801084A8: 0x001412C0},
    "jump-cursor-base-0x52": {0x80108670: 0x26820052},
    "pending-rows-layout-0x19300": {0x80108784: 0x34429300},
    "commit-clear-omitted": {0x8010888C: 0},
}
MUTANTS: tuple[dict, ...] = (WEAPON_MUTANTS, SHIELD_MUTANTS, LIST_MUTANTS, DIRECTION_MUTANTS,
                             SCREEN_MUTANTS)


def text_word(pointer: int) -> int:
    """A text pointer as its table and halfword offset, independent of native pointer width."""
    if MENU_TEXT <= pointer < MENU_TEXT + 0x20000:
        return (pointer - MENU_TEXT) // 2 | (pointer - MENU_TEXT) % 2 << 28
    if HELP <= pointer < HELP + HELP_BYTES:
        return 1 << 24 | (pointer - HELP) // 2 | (pointer - HELP) % 2 << 28
    return 0x7F000000 | pointer & 0xFFFFFF


def string_hash(data: bytes) -> int:
    value = 0x811C9DC5
    for byte in data:
        value = (value ^ byte) * 0x01000193 & MASK
    return value & 0x7FFFFFFF


def configuration(kind: int) -> dict:
    """Provide one original function's independent deterministic proof configuration."""
    entry, end = RANGES[kind]
    return {
        "ID": f"F{FUNCTIONS[kind]:05}",
        "ENTRY": entry,
        "END": end,
        "MODULES": MODULES,
        "DRIVER": "container_transfer_driver.cpp",
        "INCLUDES": (r"Native\Game\src", r"Native\Tests\include"),
        "SOURCES": (
            r"Native\Game\src\Menu\Container\ContainerListTransfer.cpp",
            r"Native\Game\src\Menu\Container\ContainerPendingRows.cpp",
            r"Native\Game\src\Items\ContainerIndexing.cpp",
            r"Native\Game\src\Items\ContainerTransfers.cpp",
            r"Native\Game\src\Menu\Container\ContainerDetailHelpers.cpp",
            r"Native\Game\src\Menu\Framework\ItemRows.cpp",
            r"Native\Game\src\Items\EquipmentStats.cpp",
            r"Native\Engine\src\Core\Math\ScalarMath.cpp",
            r"Native\Engine\src\Core\Math\Distance.cpp",
            r"Native\Engine\src\Render\EncodedText.cpp",
        ),
        "VECTORS": (192, 192, 1024, 512, 512)[kind],
        "MUTANT_VECTORS": (64, 64, 1024, 512, 512)[kind],
        "SEED": FUNCTIONS[kind],
        "MUTANTS": MUTANTS[kind],
        "vector": functools.partial(vector, kind),
        "run": functools.partial(run, kind),
        "check": check,
        "describe": describe,
    }


# ----------------------------------------------------------------- vectors
def vector(kind: int, rng: random.Random, number: int) -> dict:
    if kind in (WEAPON, SHIELD):
        return {"kind": kind, "index": (0, 31)[number % 2],
                "records": rows_common.records(rng, number // 2)}
    if kind == DIRECTION:
        return direction_vector(rng, number)
    if kind == SCREEN:
        return screen_vector(rng, number)
    from differentials import container_transfer_list

    return container_transfer_list.vector(rng, number)


def script(rng: random.Random, choices: dict[int, tuple]) -> dict[int, list[int]]:
    """Per-code return queues for the recorded callees."""
    return {code: [rng.choice(values) for _ in range(6)] for code, values in choices.items()}


def direction_vector(rng: random.Random, number: int) -> dict:
    buttons = (0, 0x10, 0x20, 0x40, 0x30, 0x60, 0x50, 0x80, 0x1000, 0x4000, 0x5000)
    return {
        "kind": DIRECTION,
        "initialize": int(number % 8 == 0),
        "state": [(0, 1, 2, 3, 3, 3, 4, rng.randrange(256))[number // 8 % 8],
                  rng.randrange(256), rng.randrange(256), rng.randrange(2)],
        "settings": rng.randrange(256),
        "selection": (0, 1, 2, 0, 1, 2, 3, rng.randrange(4, 16))[number // 3 % 8],
        "pressed": rng.choice(buttons) if number % 5 else 0,
        "repeated": rng.choice(buttons) | rng.choice((0, 0x1000, 0x4000, 0x5000)),
        "script": script(rng, {ROWS_IDLE: (0, 1), CURSOR: (0, 1, 7, 255)}),
        "rows": [rng.randrange(256) for _ in range(4)],
    }


def screen_vector(rng: random.Random, number: int) -> dict:
    flags = [rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.choice((0, 1, 2, 3, 4, 5, 7)),
             rng.choice((0, 1, 9, 10)), rng.choice((0, 1, 3, 4, 5)),
             rng.choice((0, 1, 9, 10)), rng.choice((0, 0, 1))]
    return {
        "kind": SCREEN,
        "step": (0, 1, 2, 3, 4, 5, 6, 7)[number % 8],
        "state": [rng.choice((0, 1, 2, 3, 0xFFFFFFFE, 0xFFFFFFFF)), rng.choice((0, 1, 5)),
                  rng.choice((0, 1, 9, 10))],
        "flags": flags,
        "token": rng.choice((0, 1, 2, 3, 4, 5, 6, 7, 8, 9)),
        "settings": rng.randrange(1 << 32),
        "menu_state": rng.randrange(65536),
        "misc": rng.randrange(65536),
        "pending": rng.choice((0, 1, 5, 63, 64)),
        "records": sparse_records(rng, number),
        "script": script(rng, {
            LOAD_TEXT: (0, 1), UNLOAD_TEXT: (0, 1), ROWS_IDLE: (0, 1),
            ITEM_LIST: (0, 0, 0xFFFFFFFE, 0xFFFFFFFF, 1, 2),
            DIRECTION_MENU: (0, 0, 1, 2, 3, 0xFFFFFFFE, 0xFFFFFFFF)}),
    }


def sparse_records(rng: random.Random, number: int) -> list[tuple[int, int, int]]:
    """(owner, byte offset, value) item ids for the active, other, staged and container records;
    every category of an owner is sometimes empty, and the container is sometimes entirely empty
    or holds only its last category."""
    entries = []
    container_mode = number % 7
    for owner in range(4):
        for category in range(7):
            if (number + owner + category) % 3 == 0:
                continue
            if owner == 3 and (container_mode == 0 or (container_mode == 1 and category != 6)):
                continue
            for slot in rng.sample(range(CAPACITY[category]), rng.choice((1, 2, 5))):
                entries.append((owner, RECORD_OFFSETS[category] + slot * STRIDES[category]
                                + ID_OFFSETS[category],
                                rng.randrange(1, 256)))
    return entries


# ----------------------------------------------------------------- recorded world
class Recorder:
    def __init__(self, machine, queues: dict[int, list[int]]) -> None:
        self.machine = machine
        self.queues = {code: list(values) for code, values in queues.items()}
        self.calls: list[list[int]] = []

    def record(self, code: int, *words: int) -> None:
        values = [w & MASK for w in words] + [0] * (3 - len(words))
        self.calls.append([code, *values])

    def answer(self, code: int) -> int:
        queue = self.queues.get(code) or [0]
        return queue.pop(0) if len(queue) > 1 else queue[0]

    def hook(self, address: int, code: int, *registers: int, returns: bool = False,
             value=None) -> None:
        def handler(machine):
            self.record(code, *(machine.r[r] for r in registers))
            if returns:
                machine.r[2] = self.answer(code)
            elif value is not None:
                machine.r[2] = value(machine)
        self.machine.stub(address, handler)

    def string(self, pointer: int) -> int:
        data = bytearray()
        while len(data) < 64:
            byte = self.machine.load(pointer + len(data), 1)
            if byte == 0:
                break
            data.append(byte)
        return string_hash(bytes(data))

    def text(self, chain_register: int):
        def handler(machine):
            r = machine.r
            chain = r[chain_register] if chain_register else 0
            layer = {0: 0, CHAIN - 12: 1}.get(chain, 0x100 | chain & 0xFFFF)
            color = r[6] if chain_register == 7 else MASK
            self.record(TEXT, self.string(r[4]), r[5])
            self.record(TEXT_STYLE, color, layer)
        return handler


def install_common(machine, recorder: Recorder) -> None:
    for address, sound in SOUNDS.items():
        machine.stub(address, lambda m, sound=sound: recorder.record(SOUND, sound))
    recorder.hook(0x800FA8E0, CLEAR_MENU, 4)
    recorder.hook(0x800FA854, FLY_OUT, 4)
    recorder.hook(0x800FA9D0, ROWS_IDLE, returns=True)
    machine.stub(0x800C8E48, lambda m: m.r.__setitem__(2, ROWS + m.r[4] * ROW_STRIDE))

    def menu_item(m):
        sp = m.r[29]
        recorder.record(MENU_ITEM, m.r[4], m.r[5], m.r[6])
        recorder.record(MENU_ITEM_TEXT, m.r[7], m.load(sp + 0x10), text_word(m.load(sp + 0x14)))
        m.r[2] = ROWS + (m.r[4] & 0x7F) * ROW_STRIDE
    machine.stub(0x800C8E5C, menu_item)
    machine.stub(0x800C6540, recorder.text(7))
    machine.stub(0x800C6828, recorder.text(6))
    recorder.hook(0x800FFCDC, CURSOR, 4, 5, returns=True)
    machine.stub(0x800FFC04, lambda m: recorder.record(INFORMATION, text_word(m.r[4])))
    recorder.hook(0x80106504, CAPACITIES)
    recorder.hook(0x800FE5CC, LOAD_TEXT, 4, returns=True)
    recorder.hook(0x800FFBC8, INIT_INFORMATION)
    recorder.hook(0x80103D50, PANEL, 4)
    recorder.hook(0x800FE694, UNLOAD_TEXT, returns=True)
    recorder.hook(0x801039AC, SNAPSHOT_BAG)
    recorder.hook(0x80103B20, COMMIT_BAG)
    recorder.hook(0x80103270, REFRESH_MISC)
    recorder.hook(0x80103E24, ICONS, 4, 5)
    machine.stub(0x80100344, lambda m: (recorder.record(PROMPT, m.r[4], m.r[5]),
                                        recorder.record(PROMPT_SIZE, m.r[6], m.r[7])))
    recorder.hook(0x800FFC68, BUTTON, 4, 5, 6)
    recorder.hook(0x80106464, CAPACITY_COUNT, 4, 5, 6)


def owner_word(address: int) -> int:
    """A record owner as the native TransferStorage plus offset (0 active, 1 other, 2 staged,
    3 preview, 4 container, 5 bag, 6 stored)."""
    for number, base in ((0, ACTIVE), (1, OTHER)):
        if base <= address < base + 0x10000:
            return number << 24 | address - base
    for number, name in enumerate(("staged", "preview", "container", "bag", "stored"), 2):
        base = STORAGE + STORAGE_OFFSETS[name]
        if base <= address < base + 0x3C00:
            return number << 24 | address - base
    return 0x7F000000 | address & 0xFFFFFF


def map_world(machine, item: dict) -> None:
    machine.map_zero(STORAGE, STORAGE_BYTES)
    machine.map_zero(ACTIVE, 0x20000)
    machine.map_zero(ROWS, ROW_STRIDE * ROW_COUNT)
    machine.put(STORAGE_POINTER, STORAGE)
    machine.put(ACTIVE_POINTER, ACTIVE)
    machine.put(OTHER_POINTER, OTHER)
    machine.put(SCRATCH + 4, CHAIN)
    machine.put(SETTINGS, item.get("settings", 0))
    owners = (ACTIVE, OTHER, STORAGE + STORAGE_OFFSETS["staged"],
              STORAGE + STORAGE_OFFSETS["container"])
    for owner, offset, value in item.get("records", ()):
        machine.put(owners[owner] + offset, value, 1)


def row_values(machine) -> list[int]:
    return [machine.load(ROWS + row * ROW_STRIDE + offset, size)
            for row in range(ROW_COUNT) for offset, size in ROW_FIELDS]


def script_inputs(queues: dict[int, list[int]]) -> list[int]:
    words = []
    for code in range(CODES):
        values = queues.get(code, [])
        words += [len(values), *(s32(value) for value in values)]
    return words


# ----------------------------------------------------------------- runs
def run(kind: int, machine, item: dict) -> tuple[dict, list[int]]:
    if kind in (WEAPON, SHIELD):
        return run_row(kind, machine, item)
    if kind == DIRECTION:
        return run_direction(machine, item)
    if kind == SCREEN:
        return run_screen(machine, item)
    from differentials import container_transfer_list

    return container_transfer_list.run(machine, item)


def run_row(kind: int, machine, item: dict) -> tuple[dict, list[int]]:
    """Execute the whole entry with the real stat builders and row formatters."""
    help_text, names = rows_common.texts()
    machine.map_zero(OWNER, 0x4180)
    machine.map_zero(rows_common.CONTEXT, rows_common.CONTEXT_BYTES)
    machine.map_zero(rows_common.TEMP, 0x1000)
    machine.store(OWNER, item["records"])
    machine.store(HELP, help_text)
    machine.store(rows_common.NAMES, names)
    machine.put(HELP_POINTER, HELP)
    machine.put(0x8010229C, rows_common.NAMES)
    machine.put(ACTIVE_POINTER, OWNER)
    machine.store(BUFFER, bytes(DESCRIPTION_BYTES))
    machine.put(TYPES, TYPE_FILL)
    machine.put(TEXTS, TEXT_FILL[0])
    machine.put(TEXTS + 4, TEXT_FILL[1])
    machine.stub(0x80043E3C, lambda m: m.r.__setitem__(2, rows_common.TEMP))
    machine.stub(0x80043C60)
    machine.follow.update(ROW_FOLLOW)
    stack = DEFAULT_STACK - 0x20
    machine.r[29] = stack
    machine.put(stack + 0x10, item["index"])
    machine.r[4:8] = [OWNER, TEXTS, TYPES, BUFFER]
    machine.execute(RANGES[kind][0])
    values = [machine.load(TYPES)]
    for text in range(2):
        source, offset = rows_common.normalize_pointer(machine.load(TEXTS + text * 4))
        values += [source, offset]
    values += machine.bytes(BUFFER, DESCRIPTION_BYTES)
    if machine.bytes(OWNER, 0x3C00) != item["records"]:
        raise RuntimeError("an original row builder changed its records")
    allowed = [(DEFAULT_STACK - 0x4000, DEFAULT_STACK), (BUFFER, BUFFER + DESCRIPTION_BYTES),
               (TYPES, TYPES + 4), (TEXTS, TEXTS + 8),
               (rows_common.CONTEXT, rows_common.CONTEXT + rows_common.CONTEXT_BYTES),
               (rows_common.TEMP, rows_common.TEMP + 0x1000)]
    observation = {"values": values, "stores": machine.stores, "allowed": allowed,
                   "steps": machine.steps}
    return observation, [kind, item["index"], *item["records"]]


def run_direction(machine, item: dict) -> tuple[dict, list[int]]:
    recorder = Recorder(machine, item["script"])
    install_common(machine, recorder)
    map_world(machine, item)
    machine.follow.add(0x80040F28)
    for address, value in zip(DIRECTION_STATE, item["state"], strict=True):
        machine.put(address, value, 1)
    machine.put(SELECTION, item["selection"], 2)
    machine.put(PRESSED, item["pressed"])
    machine.put(REPEATED, item["repeated"])
    for row, value in zip((10, 11), item["rows"][:2], strict=True):
        machine.put(ROWS + row * ROW_STRIDE + 6, value, 1)
    machine.r[4] = item["initialize"]
    machine.execute(RANGES[DIRECTION][0])
    values = [s32(machine.r[2]), *(machine.load(a, 1) for a in DIRECTION_STATE),
              machine.load(SELECTION, 2), *row_values(machine)]
    allowed = [(DEFAULT_STACK - 0x4000, DEFAULT_STACK), (0x80109A2C, 0x80109A2F),
               (0x80109A70, 0x80109A71), (SELECTION, SELECTION + 2),
               (ROWS, ROWS + ROW_STRIDE * ROW_COUNT)]
    inputs = [DIRECTION, item["initialize"], *item["state"], item["settings"] >> 8 & 0xFF,
              item["selection"], item["pressed"], item["repeated"], *item["rows"][:2],
              *menu_text(machine), *script_inputs(item["script"])]
    return recorded(machine, recorder, values, allowed), inputs


def menu_text(machine) -> list[int]:
    return [machine.load(MENU_TEXT + index * 2, 2) for index in range(MENU_TEXT_ENTRIES)]


def common_tables(machine) -> list[int]:
    return [*menu_text(machine),
            *(machine.load(ANIMATION + index, 1) for index in range(11)),
            *(machine.load(INVENTORY_CAPACITIES + index, 1) for index in range(7)),
            *(machine.load(CONTAINER_CAPACITIES + index * 2, 2) for index in range(7))]


def run_screen(machine, item: dict) -> tuple[dict, list[int]]:
    recorder = Recorder(machine, item["script"])
    install_common(machine, recorder)
    map_world(machine, item)
    machine.follow.update(COUNT_FOLLOW)

    def snapshot(m):
        recorder.record(STORE_SNAPSHOT, owner_word(m.r[4]), owner_word(m.r[5]), m.r[6])
    machine.stub(0x800C01E0, snapshot)
    machine.stub(0x801037D8, lambda m: recorder.record(REBUILD_INDICES, owner_word(m.r[5]), m.r[4]))
    recorder.hook(0x80107490, ITEM_LIST, 4, returns=True)
    recorder.hook(0x801089BC, DIRECTION_MENU, 4, returns=True)
    step = DEFAULT_STACK - 0x100
    machine.put(step, item["step"], 1)
    for (address, size), value in zip(SCREEN_STATE, item["state"], strict=True):
        machine.put(address, value, size)
    for index, value in enumerate(item["flags"]):
        machine.put(FLAGS + index, value, 1)
    machine.put(TOKEN, item["token"], 1)
    machine.put(MENU_STATE, item["menu_state"], 2)
    machine.put(TRANSFERRED_MISC, item["misc"], 2)
    for index in range(64):
        machine.put(PENDING + index * 2, 0x101 + index if index < item["pending"] else 0, 2)
    for address in PUBLISHED:
        machine.put(address, 0)
    machine.r[4] = step
    machine.execute(RANGES[SCREEN][0])
    published = all(machine.load(address) == ACTIVE + offset
                    for address, offset in zip(PUBLISHED, PUBLISHED_OFFSETS, strict=True))
    values = [s32(machine.r[2]), machine.load(step, 1),
              *(machine.load(address, size) for address, size in SCREEN_STATE),
              *(machine.load(FLAGS + index, 1) for index in range(8)),
              machine.load(SETTINGS), machine.load(MENU_STATE, 2), int(published),
              *row_values(machine)]
    allowed = [(DEFAULT_STACK - 0x4000, DEFAULT_STACK), (0x80109A74, 0x80109A7A),
               (FLAGS, FLAGS + 8), (SETTINGS, SETTINGS + 4), (MENU_STATE, MENU_STATE + 2),
               (0x80102458, 0x80102474), (ROWS, ROWS + ROW_STRIDE * ROW_COUNT)]
    inputs = [SCREEN, item["step"], *item["state"], *item["flags"], item["token"],
              item["settings"], item["menu_state"], item["misc"], item["pending"],
              *common_tables(machine), len(item["records"]),
              *(word for entry in item["records"] for word in entry),
              *script_inputs(item["script"])]
    return recorded(machine, recorder, values, allowed), inputs


def recorded(machine, recorder: Recorder, values: list[int], allowed) -> dict:
    words = [len(recorder.calls), *(word for call in recorder.calls for word in call)]
    return {"values": values + words, "stores": machine.stores, "allowed": allowed,
            "steps": machine.steps, "calls": len(recorder.calls)}


# ----------------------------------------------------------------- comparison
def check(image, item: dict, observation: dict, native: list[int], reference) -> list[str]:
    expected = observation["values"]
    if len(native) != len(expected):
        first = next((i for i, (a, b) in enumerate(zip(native, expected, strict=False))
                      if a & MASK != b & MASK), min(len(native), len(expected)))
        return [f"output length {len(native)} != original {len(expected)}; first difference "
                f"[{first}] native {native[first:first + 4]} original {expected[first:first + 4]}"]
    first = next((index for index, (left, right) in enumerate(zip(native, expected, strict=True))
                  if left & MASK != right & MASK), None)
    problems = [] if first is None else [
        f"output[{first}] native {native[first] & MASK:#x} original {expected[first] & MASK:#x}"]
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(image, results: list) -> list[str]:
    calls = sum(item[1].get("calls", 0) for item in results)
    return [f"Original instructions executed: {sum(item[1]['steps'] for item in results)} steps; "
            f"{calls} recorded world calls compared word for word"]
