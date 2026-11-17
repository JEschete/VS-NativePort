"""MENUB loot terminals against their actual R3000 instructions.

Callees are endpoint doubles, shared with the native test host. The original
160-row allocation is compared byte-for-byte; native 176-row storage, paging,
ownership refusals and the #412 correction have separate Catch2 evidence.
"""

from __future__ import annotations

import functools
import struct
from enum import IntEnum

from differential import DEFAULT_STACK, MODULE_BASES, Fault, s32, store_problems

MODULE = r"MENU\MENUB.PRG"
MODULE_BASES[MODULE] = 0x80102800
IDS = ("F02843", "F02844", "F02845", "F02864", "F02865")
RANGES = ((0x801066CC, 0x80106B80), (0x80106B80, 0x801073E0),
          (0x801073E0, 0x80107C54), (0x80109750, 0x80109E90),
          (0x80109E90, 0x8010A24C))
CAPACITIES = (8, 16, 16, 8, 16, 48, 64)
INV_BYTES, ORIGINAL_ROWS, NATIVE_ROWS = 0xF00, 160, 176
ASHLEY, PRESSED, REPEAT, CATEGORY = 0x80060168, 0x8005E1D0, 0x8005DFDC, 0x800F4F22
HUD, HUD_OFFSET, SETTINGS = 0x800EB9AF, 0x800EB9B0, 0x80060028
INFO_DONE, RESULT, STRING, ANIMATION = 0x800F5130, 0x800F4E9A, 0x800F5190, 0x800EBBC8
CAPACITY_TABLE, NAMES_PTR, MENU_TEXT = 0x801022A0, 0x8010229C, 0x8010A280
MENU_STRINGS, ROOM_LOOT = 0x800EB9DC, 0x800EB9C4
HEAP, MENU_ROWS, NAMES, STEP_PTR = 0x80A00000, 0x80A10000, 0x80A20000, 0x80A30000
INVENTORY, INVENTORY_BACKUP = HEAP, HEAP + INV_BYTES
LOOT, BACKUP = HEAP + 0x1E00, HEAP + 0x2080
NAME_ITEM, NAME_WEAPON = 10000, 20000
STATE_FIELDS = (
    (0x8010A680, 1), (0x8010A681, 1),
    (0x8010A682, 1), (0x8010A683, 1), (0x8010A684, 1),
    (0x8010A685, 1), (0x8010A686, 1), (0x8010A5C9, 1),
    (0x8010A688, 2), (0x8010A68A, 2), (0x8010A68C, 2),
    (0x8010A68E, 1), (0x8010A68F, 1), (0x8010A690, 1),
    (0x8010A5CA, 1), (0x8010A69C, 1), (0x8010A69D, 1),
    (0x8010A5EA, 1), (STEP_PTR, 1))
RUNTIME_FIELDS = (
    (0x8010A6B6, 1), (0x8010A6B5, 1), (0x8010A6B4, 1),
    *((0x8010A6B9 + index, 1) for index in range(7)), (0x8010A514, 4),
    (CATEGORY, 1), (0x8010A6B0, 1), (0x8010A6B1, 1),
    (0x8010A6B2, 1), (0x8010A6B3, 1), (0x8010A505, 1),
    (0x8010A6B7, 1), (0x8010A6B8, 1), (SETTINGS, 1), (HUD, 1),
    (HUD_OFFSET, 4), (RESULT, 2))


class Runtime(IntEnum):
    COUNT, OFFSET, SELECTED = range(3)
    SECONDARY, ICONS, STATUS, SECONDARY_STEP, ICONS_STEP, STATUS_STEP = range(3, 9)
    ICON_ON_TOP, ASHLEY_LAYOUT, CATEGORY = range(9, 12)
    BAG_SHOWN, BAG_STEP, LOOT_SHOWN, LOOT_STEP = range(12, 16)
    CONTROLS, PREVIEW, CLAIMED, INFORMATION, HUD, HUD_OFFSET, RESULT = range(16, 23)


# Match Native/Tests/include/Menu/LootTerminalWorld.hpp.
(READY, ROW, SET_ROW, INIT_INFO, INFORMATION, DISMISS_INFO, FLY_OUT, SOUND,
 PROMPT_CURSOR, LOOT_CURSOR, BACKGROUND, COMMAND, HIDE_PANELS, CLEAR_MENU,
 FIELD, SET_HUD, ASHLEY_DRAW, INVENTORY_MENU, COUNT, ICONS, BUTTON_BACKGROUND,
 BUTTON, STATUS_TEXT, REFRESH, DESCRIPTION, CLAIM, KEY, LEAVE, ORGANIZE,
 TRANSITION, DISASSEMBLE, CONSOLIDATE, BASE_STATS, REBUILD, ACQUIRE, RELEASE,
 STAGE, CLEAR_ROOM, LOAD_TEXT, UNLOAD_TEXT, DISMISS_TEXT, SELECTION,
 CAPACITIES_DRAW, LOOT_DRAW) = range(1, 45)

MUTATIONS = (
    {
        "initialize-category": (0x801066E8, 0),
        "title-selection": (0x801067B0, 0),
        "reset-secondary-step": (0x801067C8, 0),
        "hide-field": (0x801067F0, 0),
        "loot-navigation-entry": (0x80106830, 0),
        "loot-navigation-poll": (0x80106844, 0),
        "restore-hud": (0x80106880, 0),
        "show-loot-on-return": (0x801068EC, 0),
        "category-count": (0x80106A0C, 0),
        "icon-animation-bound": (0x80106A34, 0x2C620003),
    },
    {
        "backup-row-copy": (0x80106BA8, 0),
        "initialize-target": (0x80106BC4, 0),
        "preview-root-state": (0x80106CC4, 0),
        "weapon-blade-row": (0x80106DD0, 0),
        "prompt-target": (0x80106F40, 0),
        "cancel-mask": (0x801070A4, 0x30420000),
        "confirmation-cursor": (0x8010713C, 0),
        "detach-blade-owner": (0x80107244, 0),
        "detach-gem-owner": (0x801072B8, 0),
        "restore-row-copy": (0x80107380, 0),
        "refresh-claimability": (0x80107388, 0),
    },
    {
        "backup-inventory-copy": (0x8010742C, 0),
        "clear-matching-record": (0x801075B8, 0),
        "preview-slot": (0x801076BC, 0),
        "matching-room-bound": (0x801076EC, 0x2C420001),
        "whole-stack-count": (0x8010776C, 0),
        "remainder-count": (0x80107790, 0),
        "confirmation-mask": (0x801078E8, 0x30420040),
        "cancel-mask": (0x8010791C, 0x30420000),
        "apply-record-id": (0x80107B64, 0),
        "apply-record-count": (0x80107B88, 0),
        "restore-inventory-copy": (0x80107BF0, 0),
    },
    {
        "initial-selection-reset": (0x8010977C, 0),
        "cancel-priority": (0x801097F8, 0x30620000),
        "take-all-key-record": (0x801098A4, 0),
        "take-all-removal-mark": (0x801098B0, 0),
        "single-claim-delay": (0x801099FC, 0),
        "short-list-boundary": (0x80109A3C, 0x2C620008),
        "fresh-up-wrap-mask": (0x80109A68, 0x31020000),
        "down-scroll-offset": (0x80109B54, 0),
        "selection-description": (0x80109B8C, 0),
        "organization-disassembly": (0x80109C74, 0),
        "removal-row-count": (0x80109DA0, 0),
        "delayed-preview-mask": (0x80109E48, 0x304200FF),
    },
    {
        "base-stat-actor": (0x80109EE8, 0x24040011),
        "rebuild-category-bound": (0x80109EF8, 0x2A020006),
        "reset-loot-offset": (0x80109F50, 0),
        "load-text-entry": (0x80109F58, 0),
        "empty-chest-state": (0x80109FE0, 0x24020003),
        "selection-entry": (0x8010A020, 0),
        "selection-poll": (0x8010A030, 0),
        "screen-result-kind": (0x8010A044, 0x3A030002),
        "information-completion": (0x8010A064, 0x30420000),
        "release-workspace": (0x8010A0C0, 0),
        "bag-render-boundary": (0x8010A168, 0x2C420009),
        "loot-row-selection": (0x8010A230, 0),
    },
)


def configuration(kind: int) -> dict:
    entry, end = RANGES[kind]
    return {
        "ID": IDS[kind], "ENTRY": entry, "END": end, "MODULES": (MODULE,),
        "DRIVER": "loot_terminal_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\Loot\LootTerminal.cpp",),
        "INCLUDES": (r"Native\Tests\include",), "CXX_STANDARD": "c++latest",
        "VECTORS": 1024, "MUTANT_VECTORS": 256, "SEED": 2843 + kind * 71,
        "MUTANTS": {name: {address: replacement}
                    for name, (address, replacement) in MUTATIONS[kind].items()},
        "vector": functools.partial(vector, kind),
        "run": functools.partial(run, kind), "check": check,
        "describe": functools.partial(describe, kind),
    }


def words(block):
    return list(struct.unpack(f"<{len(block) // 4}I", bytes(block)))


def put_item(block, slot, item_id, count, index):
    struct.pack_into("<HBB", block, 0xE00 + slot * 4, item_id, count, index)


def random_rows(rng):
    return [bytes((category, rng.randrange(CAPACITIES[category]),
                   rng.randrange(3), rng.randrange(2)))
            for category in rng.choices(range(7), k=NATIVE_ROWS)]


def random_panels(rng):
    return [rng.randrange(2), rng.randrange(2), rng.randrange(3),
            rng.randrange(11), rng.randrange(5), rng.randrange(11),
            rng.randrange(2), rng.randrange(4)]


def services(rng):
    return {
        "ready": rng.choices((0, 1), k=4), "cursor": rng.randrange(256),
        "inventory": rng.choice((-1, 0, 1)), "leave": rng.choice((-1, 0, 1, 2)),
        "organize": rng.choice((-1, 0, 1, 2, 3)),
        "transition": rng.choice((-1, 0, 1)),
        "disassembly": rng.choice((-1, 0, 1)),
        "consolidation": rng.choice((-1, 0, 1)),
        "selection": rng.choice((-1, 0, 1, 2)),
        "staged_count": rng.choice((0, 1, 8, 160)),
        "selection_preview": rng.choice((0, 1, 4, 8, 128)),
        "information_done": rng.randrange(2), "text_loaded": rng.randrange(2),
        "text_unloaded": rng.randrange(2), "dismiss_pending": rng.randrange(2),
        "room_loot": rng.randrange(2), "active": 1,
        "change_ui": rng.randrange(2), "nested_category": rng.randrange(8),
        "nested_ui": random_panels(rng), "refresh": 0, "refreshed_cursor": [0, 0, 0],
        "animation": rng.choices(range(256), k=11),
        "counts": [rng.randrange(capacity + 1) for capacity in CAPACITIES],
        "claims": rng.choices((0, 1), k=7 * 64),
    }


def initial_vector(rng, number):
    count = rng.choice((1, 2, 7, 8, 9, 16, 64, 159, 160))
    item = {
        "number": number, "initialize": 0,
        "states": [number % 4, rng.randrange(2),
                   number % 8, rng.randrange(2), count, 0, 0, rng.randrange(256),
                   rng.randrange(6401), rng.randrange(6401), 323,
                   number % 8, rng.randrange(2), count, rng.randrange(256),
                   number % 10, rng.choice((0, 1, 3, 255)), rng.randrange(256),
                   number % 6],
        "runtime": [count, 0, rng.randrange(min(8, count)), *random_panels(rng),
                    rng.randrange(8), rng.randrange(2), rng.randrange(11),
                    rng.randrange(2), rng.randrange(11), rng.randrange(2),
                    rng.choice((0, 1, 8, 128)), rng.randrange(256),
                    rng.randrange(2), rng.randrange(2), rng.randrange(-1000, 1001),
                    rng.choice((-2, 0, 1, 32767))],
        "pressed": rng.choice((0, 0x20, 0x40, 0x60, 0x80, 0xC0, 0xA0,
                               0x1000, 0x4000, 0x5000, 0x10000)),
        "repeat": rng.choice((0, 0x1000, 0x4000, 0x5000)),
        "name": 0, "services": services(rng),
        "inventory": bytearray(rng.randbytes(INV_BYTES)),
        "inventory_backup": bytearray(rng.randbytes(INV_BYTES)),
        "ashley": bytearray(rng.randbytes(INV_BYTES)),
        "rows": random_rows(rng), "backup": random_rows(rng),
        "menu_rows": [rng.randbytes(60) for _ in range(40)],
    }
    for slot in range(64):
        item_id = rng.choice((0, 323, 457, 458, 500))
        put_item(item["inventory"], slot, item_id, rng.randrange(1, 101), slot + 1)
        put_item(item["ashley"], slot, item_id, rng.randrange(1, 101), slot + 1)
    return item


def transition_vector(item, rng, number):
    step = number % 5
    if step == 4:
        item["initialize"] = rng.randrange(1, 8)
    else:
        item["states"][0] = step
    if step == 2:
        item["runtime"][Runtime.HUD] = 1
    return item


def disassembly_vector(item, rng, number):
    step = number % 9
    category = (number // 9) % 2 * 3
    count = rng.choice((1, 2, 8, 9, 64, 130, 160 if step in (6, 7, 8) else 150))
    target = rng.choice((0, count - 1, rng.randrange(count)))
    slot = rng.choice((0, 7, rng.randrange(8)))
    gems = rng.sample(range(1, 49), 4)
    gem_count = (number // 18) % 5
    gems = [value if rng.randrange(3) else 0 for value in gems]
    block = item["inventory"]
    root = bytes((category, slot, 0, 2))
    if category == 0:
        blade, grip = rng.randrange(1, 17), rng.randrange(1, 17)
        block[slot * 32 + 1:slot * 32 + 3] = bytes((blade, grip))
        block[slot * 32 + 4:slot * 32 + 8] = bytes(gems)
        block[0x540 + (grip - 1) * 16 + 4] = gem_count
        preview = [root, bytes((1, blade - 1, 0, 4)), bytes((2, grip - 1, 0, 4))]
    else:
        block[0x100 + slot * 48 + 16] = gem_count
        block[0x100 + slot * 48 + 44:0x100 + slot * 48 + 48] = bytes(gems)
        preview = [root, bytes((3, slot, 0, 3))]
    preview += [bytes((5, gem - 1, 0, 5)) for gem in gems[:gem_count] if gem]
    item["rows"][target] = bytes((category, slot, 0, 0))
    item["backup"] = item["rows"].copy()
    item["states"][2:7] = [min(step, 7), rng.randrange(2), count, target, category]
    item["runtime"][Runtime.COUNT] = count
    item["runtime"][Runtime.LOOT_STEP] = rng.choice((0, 5, 6, 6, 7, 10))
    if step == 8:
        item["initialize"] = target + 1
        item["backup"] = random_rows(rng)
        item["states"][2:7] = [
            rng.randrange(8), 1, count // 2,
            (target + 1) % count if count > 1 else ORIGINAL_ROWS - 1,
            3 - category]
    elif step != 0:
        item["rows"][:len(preview)] = preview
        item["runtime"][Runtime.COUNT] = len(preview)
    return item


def partition(total, room):
    result = []
    while total:
        amount = min(total, min(room, 100) if room else 100)
        result.append((amount, int(room != 0)))
        total -= amount
        room -= min(room, amount)
    return result


def consolidation_vector(item, rng, number):
    step = number % 9
    item_id = rng.choice((323, 324, 456, 457))
    matching = (2, 8, 20, 63)[:1 + (number // 9) % 4]
    amounts = [rng.choice((1, 5, 20, 50, 99, 100)) for _ in matching]
    total = sum(amounts)
    room = rng.choice((0, 1, min(20, total), min(100, total), total))
    original_room = room
    for name in ("inventory", "ashley"):
        for slot in range(64):
            put_item(item[name], slot, 0, 0, rng.randrange(256))
    for slot, amount in zip(matching, amounts, strict=True):
        put_item(item["inventory"], slot, item_id, amount, slot + 1)
    for slot in range(64):
        if room == 0:
            break
        free = min(room, 100)
        put_item(item["ashley"], slot, item_id, 100 - free, slot + 1)
        room -= free
    count = rng.choice((len(matching) + 1, 8, 32, 64, 128, 155))
    item["rows"] = [bytes((1, index % 16, 0, 1)) for index in range(NATIVE_ROWS)]
    positions = sorted(rng.sample(range(count), len(matching)))
    for position, slot in zip(positions, matching, strict=True):
        item["rows"][position] = bytes((6, slot, 0, 1))
    item["backup"] = item["rows"].copy()
    item["inventory_backup"] = item["inventory"].copy()
    item["states"][10:14] = [item_id, min(step, 7), rng.randrange(2), count]
    item["runtime"][Runtime.COUNT] = count
    item["runtime"][Runtime.LOOT_STEP] = rng.choice((0, 5, 6, 6, 7, 10))
    if step == 8:
        item["initialize"] = positions[0] + 1
        item["backup"] = random_rows(rng)
        item["inventory_backup"] = bytearray(rng.randbytes(INV_BYTES))
    elif step != 0:
        for slot in matching:
            put_item(item["inventory"], slot, 0, 0, 0)
        preview = partition(total, original_room)
        for slot, (amount, claimable) in enumerate(preview):
            put_item(item["inventory"], slot, item_id, amount, 0)
            item["rows"][slot] = bytes((6, slot, 0, claimable))
        item["runtime"][Runtime.COUNT] = len(preview)
    item["healthy_total"], item["healthy_room"] = total, original_room
    return item


def selection_vector(item, rng, number):
    step = number % 11
    item["states"][15] = min(step, 9)
    if step == 10:
        item["initialize"] = 1
    count = item["runtime"][Runtime.COUNT]
    if count > 8:
        last = count - 8
        offset = rng.choice((0, last // 2, last))
        selected = rng.randrange(0 if offset == 0 else 1,
                                8 if offset == last else 7)
        item["runtime"][Runtime.OFFSET:Runtime.SELECTED + 1] = [offset, selected]
    item["runtime"][Runtime.LOOT_STEP] = rng.choice((0, 0, 1, 6, 10))
    for index in range(count):
        category, slot, _, state = item["rows"][index]
        item["rows"][index] = bytes((category, slot, rng.randrange(2) if step == 9 else 0, state))
    if number % 17 == 0:
        item["services"]["claims"] = [0] * (7 * 64)
    selection_edges(item, number % 64)
    return item


def selection_edges(item, edge):
    edges = (
        (1, 8, 0, 0, 0, 0x1000), (1, 9, 0, 0, 0, 0x1000),
        (1, 9, 0, 0, 0x1000, 0x1000), (1, 10, 0, 6, 0, 0x4000),
        (1, 9, 1, 7, 0x4000, 0x4000), (1, 8, 0, 7, 0, 0x4000),
        (1, 1, 0, 0, 0x80, 0), (1, 1, 0, 0, 0xA0, 0),
        (1, 1, 0, 0, 0x20, 0), (1, 1, 0, 0, 0xE0, 0),
        (3, 8, 0, 7, 0, 0), (9, 3, 0, 0, 0, 0),
        (9, 3, 0, 0, 0, 0), (9, 3, 0, 0, 0, 0),
        (1, 8, 0, 7, 0, 0), (7, 8, 0, 7, 0x10000, 0),
    )
    if edge >= len(edges):
        return
    step, count, offset, selected, pressed, repeat = edges[edge]
    item["initialize"] = int(edge == 14)
    item["states"][15] = step
    item["runtime"][:3] = [count, offset, selected]
    item["pressed"], item["repeat"] = pressed, repeat
    if edge in (6, 7, 8):
        item["rows"][0] = bytes((6, 0, 0, 0 if edge == 7 else 1))
        put_item(item["inventory"], 0, 458, 1, 1)
        item["services"]["claims"][6 * 64] = int(edge == 6)
        item["runtime"][Runtime.CLAIMED] = 255
    if edge == 10:
        item["services"]["organize"] = 2
    if edge in (11, 12, 13):
        item["states"][16] = 3 if edge == 13 else 0
        for row in range(count):
            category, slot, _, state = item["rows"][row]
            item["rows"][row] = bytes((category, slot, int(row != 1 or edge == 12), state))


def screen_vector(item, rng, number):
    item["services"]["active"] = int(number % 6 != 0)
    item["runtime"][Runtime.COUNT] = rng.choice((0, 1, 8, 160))
    item["runtime"][Runtime.LOOT_STEP] = rng.choice((0, 0, 1, 6, 10))
    if number % 6 == 0:
        item["runtime"][Runtime.OFFSET] = rng.choice((1, 8, 127, 159))
    return item


def vector(kind, rng, number):
    item = initial_vector(rng, number)
    return (transition_vector, disassembly_vector, consolidation_vector,
            selection_vector, screen_vector)[kind](item, rng, number)


def service_inputs(service):
    names = ("cursor", "inventory", "leave", "organize", "transition", "disassembly",
             "consolidation", "selection", "staged_count", "selection_preview",
             "information_done", "text_loaded", "text_unloaded", "dismiss_pending",
             "room_loot", "active", "change_ui", "nested_category")
    return [*service["ready"], *(service[name] for name in names),
            *service["nested_ui"], service["refresh"], *service["refreshed_cursor"],
            *service["animation"], *service["counts"], *service["claims"]]


def inputs(kind, item):
    return [kind, item["initialize"], *item["states"], *item["runtime"],
            item["pressed"], item["repeat"], item["name"],
            *service_inputs(item["services"]), *words(item["inventory"]),
            *words(item["inventory_backup"]), *words(item["ashley"]),
            *words(b"".join(item["rows"])), *words(b"".join(item["backup"])),
            *words(b"".join(item["menu_rows"]))]


def store_fields(machine, fields, values):
    for (address, width), value in zip(fields, values, strict=True):
        machine.put(address, value, width)


def load_fields(machine, fields):
    return [machine.load(address, width,
                         address in (0x8010A514, HUD_OFFSET, RESULT))
            for address, width in fields]


def prepare(machine, item):
    for address, size in ((HEAP, 0x2300), (MENU_ROWS, 40 * 64),
                          (NAMES, 512 * 24), (STEP_PTR, 4),
                          (ASHLEY, INV_BYTES), (0x8010A670, 0x58),
                          (MENU_STRINGS, 0x200)):
        machine.map_zero(address, size)
    store_fields(machine, STATE_FIELDS, item["states"])
    store_fields(machine, RUNTIME_FIELDS, item["runtime"])
    machine.put(PRESSED, item["pressed"])
    machine.put(REPEAT, item["repeat"])
    for address, value in ((0x8010A6A0, LOOT), (0x8010A6A4, BACKUP),
                           (0x8010A6A8, INVENTORY), (0x8010A6AC, INVENTORY_BACKUP),
                           (NAMES_PTR, NAMES), (STRING, 0),
                           (INFO_DONE, item["services"]["information_done"] << 30),
                           (ROOM_LOOT, 0x80A40000 if item["services"]["room_loot"] else 0)):
        machine.put(address, value)
    machine.put(MENU_STRINGS + 6, 0x80, 2)
    machine.store(ANIMATION, bytes(item["services"]["animation"]))
    machine.store(CAPACITY_TABLE, bytes(CAPACITIES))
    machine.store(INVENTORY, item["inventory"])
    machine.store(INVENTORY_BACKUP, item["inventory_backup"])
    machine.store(ASHLEY, item["ashley"])
    machine.store(LOOT, b"".join(item["rows"][:ORIGINAL_ROWS]))
    machine.store(BACKUP, b"".join(item["backup"][:ORIGINAL_ROWS]))
    for row, raw in enumerate(item["menu_rows"]):
        machine.store(MENU_ROWS + row * 64, raw)
    machine.stores.clear()


class Endpoints:
    def __init__(self, machine, item):
        self.machine = machine
        self.service = item["services"]
        self.calls = []
        self.ready_position = 0
        self.active = self.service["active"]

    def record(self, operation, *arguments):
        self.calls.extend((operation, len(arguments), *arguments))

    def text(self, address):
        if address == MENU_STRINGS + 0x100:
            return 0
        offsets = {138: 32, 142: 48, 448: 64, 464: 80, 68: 96}
        if address - MENU_TEXT in offsets:
            return offsets[address - MENU_TEXT]
        for category in range(8):
            if address == MENU_TEXT + self.machine.load(MENU_TEXT + (23 + category) * 2, 2) * 2:
                return 16 + category
        raise Fault(f"unknown loot text pointer {address:#x}")

    def fixed(self, address, operation, argc=0, result=0):
        def invoke(machine):
            self.record(operation, *(s32(machine.r[4 + index]) for index in range(argc)))
            machine.r[2] = result & 0xFFFFFFFF
        self.machine.stub(address, invoke)

    def ready(self, machine):
        self.record(READY)
        machine.r[2] = self.service["ready"][min(self.ready_position, 3)]
        self.ready_position += 1

    def row(self, machine):
        row = s32(machine.r[4])
        if not 0 <= row < 40:
            raise Fault(f"invalid menu row {row}")
        self.record(ROW, row)
        machine.r[2] = MENU_ROWS + row * 64

    def set_row(self, machine):
        row = s32(machine.r[4])
        if not 0 <= row < 40:
            raise Fault(f"invalid new menu row {row}")
        self.record(SET_ROW, *(s32(machine.r[4 + index]) for index in range(4)),
                    machine.load(machine.r[29] + 16, 4, True),
                    self.text(machine.load(machine.r[29] + 20)))
        machine.r[2] = MENU_ROWS + row * 64

    def information(self, machine):
        self.record(INFORMATION, self.text(machine.r[4]))
        machine.r[2] = 0

    def caution(self, machine):
        if machine.r[4] != 1:
            raise Fault("unexpected loot information header")
        self.record(INIT_INFO, 1)
        machine.r[2] = 0

    def normal_information(self, machine):
        self.record(INIT_INFO, 0)
        machine.r[2] = 0

    def hud(self, machine):
        self.record(SET_HUD, machine.r[4])
        machine.put(HUD, machine.r[4], 1)
        machine.r[2] = 0

    def inventory(self, machine):
        self.record(INVENTORY_MENU, machine.r[4])
        if self.service["change_ui"]:
            store_fields(machine, RUNTIME_FIELDS[3:11], self.service["nested_ui"])
            machine.put(CATEGORY, self.service["nested_category"], 1)
        machine.r[2] = self.service["inventory"] & 0xFFFFFFFF

    def count(self, machine):
        category = machine.r[4]
        if category >= 7:
            raise Fault("count category outside item tables")
        self.record(COUNT, category)
        machine.r[2] = self.service["counts"][category]

    def centered_sound(self, machine):
        if machine.r[4] != 126 or machine.r[5] not in (13, 25):
            raise Fault("unexpected loot sound")
        self.record(SOUND, 5 if machine.r[5] == 13 else 6)
        machine.r[2] = 0

    def sound(self, sound_id):
        def invoke(machine):
            self.record(SOUND, sound_id)
            machine.r[2] = 0
        return invoke

    def status(self, machine):
        if machine.r[4] != 0x80102904 or machine.r[7] != 0:
            raise Fault("unexpected status label or ordering table")
        self.record(STATUS_TEXT, machine.r[5], machine.r[6])
        machine.r[2] = 0

    def refresh(self, machine):
        self.record(REFRESH)
        if self.service["refresh"]:
            store_fields(machine, RUNTIME_FIELDS[:3], self.service["refreshed_cursor"])
        machine.r[2] = 0

    def claim(self, machine):
        category, slot = machine.r[4] ^ 16, machine.r[6]
        if category >= 7 or slot >= CAPACITIES[category] or \
                machine.r[5] != ASHLEY or machine.r[7] != INVENTORY:
            raise Fault("loot claim changed its typed category, slot or inventories")
        self.record(CLAIM, category, slot)
        machine.r[2] = self.service["claims"][category * 64 + slot]

    def organize(self, machine):
        address = machine.r[4]
        if address != 0 and (not LOOT <= address < LOOT + ORIGINAL_ROWS * 4 or
                             (address - LOOT) % 4 != 0):
            raise Fault("organization input is not a staged loot row")
        self.record(ORGANIZE, (address - LOOT) // 4 + 1 if address else 0)
        machine.r[2] = self.service["organize"] & 0xFFFFFFFF

    def acquire(self, machine):
        if self.active or machine.r[4] != 0x2300:
            raise Fault("duplicate or wrongly sized original loot allocation")
        self.record(ACQUIRE)
        self.active = 1
        machine.r[2] = HEAP

    def release(self, machine):
        if not self.active or machine.r[4] != HEAP:
            raise Fault("invalid loot workspace free")
        self.record(RELEASE)
        self.active = 0
        machine.r[2] = 0

    def stage(self, machine):
        self.record(STAGE, int(machine.r[4] != 0))
        machine.put(0x8010A6B6, self.service["staged_count"], 1)
        machine.r[2] = 0

    def selection(self, machine):
        self.record(SELECTION, machine.r[4])
        machine.put(0x8010A6B7, self.service["selection_preview"], 1)
        machine.r[2] = self.service["selection"] & 0xFFFFFFFF

    @staticmethod
    def copy(machine):
        target, source, size = machine.r[4:7]
        if size > 0x2300:
            raise Fault("oversized loot copy")
        machine.store(target, machine.bytes(source, size))
        machine.r[2] = target

    @staticmethod
    def zero(machine):
        target, size = machine.r[4:6]
        if size > 0x2300:
            raise Fault("oversized loot clear")
        machine.store(target, bytes(size))
        machine.r[2] = target

    def install(self, kind):
        fixed_calls = (
            (0x800FFBA8, DISMISS_INFO, 0, 0), (0x800FA8A0, FLY_OUT, 1, 0),
            (0x800CCD40, PROMPT_CURSOR, 2, self.service["cursor"]),
            (0x800FFCDC, LOOT_CURSOR, 2, self.service["cursor"]),
            (0x800FFB68, BACKGROUND, 1, 0), (0x800FFA88, COMMAND, 1, 0),
            (0x80104F98, HIDE_PANELS, 1, 0), (0x800FA8E0, CLEAR_MENU, 1, 0),
            (0x8008A4DC, FIELD, 1, 0), (0x80106274, ASHLEY_DRAW, 1, 0),
            (0x80102B14, ICONS, 2, 0), (0x80100344, BUTTON_BACKGROUND, 4, 0),
            (0x800FFC68, BUTTON, 4, 0), (0x80108E78, DESCRIPTION, 1, 0),
            (0x8010A24C, KEY, 1, 0), (0x801090A4, LEAVE, 1, self.service["leave"]),
            (0x800FBD80, BASE_STATS, 1, 0), (0x800FE8B0, REBUILD, 1, 0),
            (0x8008A6FC, CLEAR_ROOM, 0, 0),
            (0x800FE5CC, LOAD_TEXT, 1, self.service["text_loaded"]),
            (0x800FE694, UNLOAD_TEXT, 0, self.service["text_unloaded"]),
            (0x800CD064, DISMISS_TEXT, 1, self.service["dismiss_pending"]),
            (0x8010837C, CAPACITIES_DRAW, 1, 0), (0x80108938, LOOT_DRAW, 1, 0),
        )
        for values in fixed_calls:
            self.fixed(*values)
        for other, operation, result in (
            (0, TRANSITION, "transition"), (1, DISASSEMBLE, "disassembly"),
            (2, CONSOLIDATE, "consolidation"),
        ):
            if kind != other:
                self.fixed(RANGES[other][0], operation, 1, self.service[result])
        for address, callback in (
            (0x800FA9D0, self.ready), (0x800C8E48, self.row), (0x800C8E5C, self.set_row),
            (0x800FFC04, self.information), (0x800C8E04, self.caution),
            (0x800FFBC8, self.normal_information), (0x800CB654, self.hud),
            (0x80105454, self.inventory), (0x800FEA6C, self.count),
            (0x80045988, self.centered_sound), (0x800C6540, self.status),
            (0x801080F0, self.refresh), (0x800FEB94, self.claim),
            (0x80109444, self.organize),
            (0x80043E3C, self.acquire), (0x80043C60, self.release),
            (0x801088D4, self.stage), (0x800C01E0, self.copy),
            (0x800CCC34, self.copy), (0x800CCC1C, self.zero),
        ):
            self.machine.stub(address, callback)
        for address, sound in ((0x800C02D8, 0), (0x800C02F0, 1), (0x800C02E8, 2),
                               (0x800C02F8, 3), (0x800C02E0, 4)):
            self.machine.stub(address, self.sound(sound))
        if kind != 3:
            self.machine.stub(RANGES[3][0], self.selection)


def name_token(machine):
    address = machine.load(STRING)
    if address == 0:
        return 0
    if NAMES <= address < NAMES + 512 * 24 and (address - NAMES) % 24 == 0:
        return NAME_ITEM + (address - NAMES) // 24
    if INVENTORY + 8 <= address < INVENTORY + 8 * 32 and (address - INVENTORY - 8) % 32 == 0:
        return NAME_WEAPON + (address - INVENTORY - 8) // 32
    raise Fault(f"unknown loot substitution pointer {address:#x}")


def run(kind, machine, item):
    prepare(machine, item)
    endpoints = Endpoints(machine, item)
    endpoints.install(kind)
    machine.r[4] = STEP_PTR if kind == 4 else item["initialize"]
    machine.execute(RANGES[kind][0])
    values = [s32(machine.r[2]), *load_fields(machine, STATE_FIELDS),
              *load_fields(machine, RUNTIME_FIELDS), name_token(machine), endpoints.active,
              *words(machine.bytes(INVENTORY, INV_BYTES)),
              *words(machine.bytes(INVENTORY_BACKUP, INV_BYTES)),
              *words(machine.bytes(LOOT, ORIGINAL_ROWS * 4)),
              *words(machine.bytes(BACKUP, ORIGINAL_ROWS * 4)),
              *words(b"".join(machine.bytes(MENU_ROWS + row * 64, 60) for row in range(40))),
              len(endpoints.calls), *endpoints.calls]
    allowed = [(DEFAULT_STACK - 0x2000, DEFAULT_STACK + 16), (HEAP, HEAP + 0x2300),
               (MENU_ROWS, MENU_ROWS + 40 * 64), (STRING, STRING + 4),
               (0x8010A69E, 0x8010A69F), (0x8010A6A0, 0x8010A6B0)]
    allowed += [(address, address + width) for address, width in STATE_FIELDS + RUNTIME_FIELDS]
    return {"values": values, "stores": machine.stores, "allowed": allowed}, inputs(kind, item)


def check(image, item, observation, native, reference):
    problems = []
    if native != observation["values"]:
        first = next((index for index, (left, right) in
                      enumerate(zip(native, observation["values"], strict=False))
                      if left != right), None)
        problems.append(
            f"loot state/request differs at {first}; lengths {len(native)}/{len(observation['values'])}; "
            f"native={native[first:first+8] if first is not None else []}, "
            f"original={observation['values'][first:first+8] if first is not None else []}")
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(kind, image, results):
    from capstone import CS_ARCH_MIPS, CS_MODE_LITTLE_ENDIAN, CS_MODE_MIPS32, Cs

    entry, end = RANGES[kind]
    body = bytes(image.read(address, 1) for address in range(entry, end))
    decoder = Cs(CS_ARCH_MIPS, CS_MODE_LITTLE_ENDIAN | CS_MODE_MIPS32)
    instructions = list(decoder.disasm(body, entry))
    if len(instructions) != (end - entry) // 4 or instructions[0].mnemonic != "addiu" or \
            image.read(end - 8) != 0x03E00008:
        raise AssertionError("verified MENUB entry/return boundary changed")
    limits = [
        f"MENUB module offsets +{entry-image.base:X}..+{end-image.base:X}; "
        "complete decoded entry, all original instructions and final return delay slot.",
        "All states, initialization, dirty backups, byte/halfword controller state, "
        "complete staged inventories, 160 original rows and requests compare.",
        "Only real callees are endpoint doubles; native pointer layout, 176-row extension, "
        "batch continuation, owned-lifetime refusals and cold reset need native evidence.",
    ]
    if kind == 2:
        if any(item["healthy_room"] > item["healthy_total"] for item, _, _ in results):
            raise AssertionError("original-parity vectors included the known #412 defect domain")
        limits.append("Parity domain: Ashley matching-stack room <= available loot. "
                      "#412 room > loot is corrected and tested separately for conservation/rollback.")
    return limits
