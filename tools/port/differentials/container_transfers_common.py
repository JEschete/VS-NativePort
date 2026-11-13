"""Original MENUD transfers: actual R3000 bodies and real memory/index callees."""

import functools
import random

from differential import DEFAULT_STACK, MODULE_BASES, s32, store_problems

MODULES = (r"MENU\MENUD.PRG", r"BATTLE\BATTLE.PRG")
MODULE_BASES.update({MODULES[0]: 0x80102800, MODULES[1]: 0x80068800})
DRIVER = "container_transfers_driver.cpp"
SOURCES = (
    r"Native\Game\src\Items\ContainerTransfers.cpp",
    r"Native\Game\src\Items\ContainerIndexing.cpp",
)
INCLUDES = (r"Native\Game\src", r"Native\Tests\include")
CAPACITIES = (32, 64, 64, 32, 64, 192, 256, 2)
INVENTORY_CAPACITIES = (8, 16, 16, 8, 16, 48, 64, 2)
OFFSETS = (0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300, 0x3800)
STRIDES = (32, 44, 16, 48, 40, 28, 4)
ID_OFFSETS = (1, 0, 0, 4, 0, 0, 0)
ID_WIDTHS = (1, 1, 2, 1, 1, 2, 2)
STORAGE = 0x80800000
ADDRESSES = (
    0x80600000, 0x80700000, STORAGE + 0x4BB0, STORAGE + 0x87B0,
    STORAGE + 0x105B0, 0x80060168, 0x80980000,
)
SIZES = (0x4180, 0x3C00, 0x3C00, 0x3C00, 0x4180, 0xF00, 0xB0)
CAPACITY, INV_CAPACITY = 0x80109944, 0x801022A0
INV_POINTERS, STORAGE_POINTER, OTHER_POINTER, COUNT = (
    0x801022A8, 0x8010245C, 0x80109A88, 0x80109A82,
)
RANGES = (
    (0x801031A0, 0x80103270), (0x80103270, 0x801032AC),
    (0x801032AC, 0x801037D8), (0x801037D8, 0x801039AC),
    (0x801039AC, 0x80103B20), (0x80103B20, 0x80103C9C),
    (0x80103C9C, 0x80103D14), (0x80103D14, 0x80103D50),
)
MUTATIONS = (
    {
        "preview-copy-extent": (0x801031B0, 0x240603F8),
        "preview-misc-category": (0x801031FC, 0x24140005),
        "preview-token-category": (0x80103204, 0x00061243),
        "preview-copy-mode": (0x8010320C, 0x24040006),
        "preview-last-slot": (0x8010321C, 0x30C6007F),
        "preview-next-token": (0x80103228, 0x26100002),
    },
    {
        "refresh-preview-call": (0x80103278, 0),
        "refresh-count-category": (0x80103288, 0x24040005),
        "refresh-count-call": (0x8010328C, 0),
        "refresh-store-value": (0x8010329C, 0xA6009A82),
    },
    {
        "copy-mode-bit": (0x801032D0, 0x00101143),
        "category-mask": (0x801032D4, 0x32100003),
        "initial-count-call": (0x801032FC, 0),
        "empty-slot-call": (0x8010330C, 0),
        "weapon-capacity": (0x80103354, 0x2402001F),
        "blade-capacity": (0x801034BC, 0x2402003F),
        "grip-capacity": (0x80103504, 0x2402003F),
        "weapon-optional-gem-count": (0x801033A4, 0x2A020002),
        "weapon-gem-capacity": (0x801033C0, 0x284200C0),
        "weapon-zero-size": (0x801033DC, 0x24050004),
        "weapon-mandatory-blade": (0x80103404, 0x24C60000),
        "weapon-mandatory-grip": (0x80103420, 0x24C60000),
        "weapon-name-extent": (0x80103474, 0x24060017),
        "blade-copy-extent": (0x801034D8, 0x24060028),
        "blade-parent": (0x801034E4, 0xA200002A),
        "grip-parent": (0x8010352C, 0xA600000C),
        "shield-capacity": (0x8010355C, 0x2402001F),
        "shield-gem-count": (0x80103588, 0x2A020002),
        "shield-parent-flag": (0x801035DC, 0x36A20000),
        "armor-copy-extent": (0x80103674, 0x24060020),
        "gem-capacity": (0x801036AC, 0x240200BF),
        "gem-parent": (0x801036D4, 0xA6000018),
        "misc-stack-limit": (0x801036E4, 0x24070063),
        "misc-loop-extent": (0x80103764, 0x2A0200FF),
        "misc-quantity": (0x80103738, 0xA0C00002),
        "misc-dry-zero-clamp": (0x80103754, 0x04210002),
        "misc-empty-success": (0x8010377C, 0x24150000),
        "misc-new-identity": (0x801037A0, 0xA4400000),
    },
    {
        "rebuild-prune-occupied": (0x80103848, 0x10400002),
        "rebuild-prune-store": (0x80103850, 0xA6050000),
        "rebuild-live-category": (0x80103838, 0x24040006),
        "rebuild-next-step": (0x801038B4, 0x24A50002),
        "rebuild-shift-value": (0x801038F4, 0xA4600000),
        "rebuild-clear-tail": (0x8010391C, 0xA4620000),
        "rebuild-missing-branch": (0x80103964, 0x10400004),
        "rebuild-append-call": (0x80103970, 0),
    },
    {
        "snapshot-record-clear": (0x801039F0, 0),
        "snapshot-order-clear": (0x80103A08, 0),
        "snapshot-unsigned-token": (0x80103A5C, 0x80420000),
        "snapshot-token-source": (0x80103A5C, 0x90420001),
        "snapshot-category-count": (0x80103A80, 0x2A220006),
        "snapshot-weapons-extent": (0x80103A98, 0x240600F8),
        "snapshot-blades-extent": (0x80103AA8, 0x240602B8),
        "snapshot-grips-extent": (0x80103AB8, 0x240600F8),
        "snapshot-shields-extent": (0x80103AC8, 0x24060178),
        "snapshot-armor-extent": (0x80103AD8, 0x24060278),
        "snapshot-gems-extent": (0x80103AE8, 0x24060538),
        "snapshot-misc-extent": (0x80103AF8, 0x240600F8),
    },
    {
        "commit-rebuild-call": (0x80103B94, 0),
        "commit-rebuild-categories": (0x80103BA0, 0x2A020006),
        "commit-token-byte": (0x80103BD0, 0x90A20001),
        "commit-token-store": (0x80103BD8, 0xA0600000),
        "commit-category-count": (0x80103BF4, 0x2A220006),
        "commit-weapons-extent": (0x80103C0C, 0x240600F8),
        "commit-blades-extent": (0x80103C1C, 0x240602B8),
        "commit-grips-extent": (0x80103C2C, 0x240600F8),
        "commit-shields-extent": (0x80103C3C, 0x24060178),
        "commit-armor-extent": (0x80103C4C, 0x24060278),
        "commit-gems-extent": (0x80103C5C, 0x24060538),
        "commit-misc-extent": (0x80103C6C, 0x240600F8),
    },
    {
        "parent-blade-field": (0x80103CC0, 0x90430A2B),
        "parent-blade-unsigned": (0x80103CC0, 0x80430A2A),
        "parent-grip-field": (0x80103CD8, 0x9443150E),
        "parent-grip-unsigned": (0x80103CD8, 0x8443150C),
        "parent-gem-field": (0x80103CF4, 0x9443231A),
        "parent-shield-bit": (0x80103CFC, 0x30620040),
        "parent-shield-exclusion": (0x80103D00, 0x14400002),
    },
    {
        "shield-parent-category": (0x80103D14, 0x24020004),
        "shield-parent-field": (0x80103D30, 0x9443231A),
        "shield-parent-bit": (0x80103D38, 0x30620040),
        "shield-parent-gate": (0x80103D3C, 0x10400002),
        "shield-parent-mask": (0x80103D4C, 0x306200FF),
    },
)


def configuration(kind):
    entry, end = RANGES[kind]
    return {
        "ID": f"F{2934 + kind:05}", "ENTRY": entry, "END": end,
        "MODULES": MODULES, "DRIVER": DRIVER, "SOURCES": SOURCES, "INCLUDES": INCLUDES,
        "VECTORS": 256 if kind != 5 else 32, "SEED": 2934 + kind,
        "MUTANT_VECTORS": 128 if kind == 2 else 16 if kind == 5 else 64,
        "MUTANTS": {name: {address: word} for name, (address, word) in MUTATIONS[kind].items()},
        "vector": functools.partial(vector, kind), "run": functools.partial(run, kind),
        "check": check, "describe": describe,
    }


def block(fill=0):
    return {"fill": fill, "patches": []}


def patch(target, offset, value, size=1):
    target["patches"].append((offset, value, size))


def identity(target, category, slot, value):
    patch(target, OFFSETS[category] + slot * STRIDES[category] + ID_OFFSETS[category],
          value, ID_WIDTHS[category])


def record_bytes(target, category, slot, rng):
    start = OFFSETS[category] + slot * STRIDES[category]
    for offset in range(STRIDES[category]):
        patch(target, start + offset, rng.randrange(256))


def order_patch(target, caps, category, values):
    start = 0x3C00 + sum(caps[:category]) * 2
    for row, token in enumerate(values):
        patch(target, start + row * 2, token, 2)


def copy_vector(rng, number, item):
    category = number % 7
    mode = number // 7 % 6
    index = rng.choice((0, CAPACITIES[category] - 1, rng.randrange(CAPACITIES[category])))
    item.update(flags=(rng.choice((0, 1, 0x81, 0xFFFF, -1)) << 8) | category
                | (0 if mode == 1 else 0x10), index=index, alias=number % 8 == 0)
    target = block()
    origin = target if item["alias"] else block()
    record_bytes(origin, category, index, rng)
    if category == 0:
        identity(origin, 0, index, 1)
        patch(origin, index * 32 + 2, 64)
        for n, value in enumerate((192, 192, 1, 77)):
            patch(origin, index * 32 + 4 + n, value if mode != 2 else 0)
        for part, slot in ((1, 0), (2, 63), (5, 191), (5, 0)):
            record_bytes(origin, part, slot, rng)
            identity(origin, part, slot, rng.randrange(1, 256))
    elif category == 3:
        identity(origin, 3, index, 0 if item["alias"] and mode == 2 else 8)
        for n, value in enumerate((192, 1, 192, 17)):
            patch(origin, OFFSETS[3] + index * 48 + 44 + n, value if mode != 2 else 0)
        for slot in (0, 191):
            record_bytes(origin, 5, slot, rng)
    elif category == 6:
        identity(origin, 6, index, 0 if mode == 2 else 12)
        patch(origin, OFFSETS[6] + index * 4 + 2, rng.choice((0, 1, 99, 100, 101, 255)))
        for slot, count in ((0, 99), (1, 100), (254, 150), (255, 60)):
            if item["alias"] and slot == index:
                continue
            identity(target, 6, slot, 12)
            patch(target, OFFSETS[6] + slot * 4 + 2, count)
    if mode == 3:
        for slot in range(CAPACITIES[category]):
            identity(target, category, slot, 1)
    if mode == 4 and category in (0, 3):
        for slot in range(CAPACITIES[5] - (2 if number % 2 else 3)):
            identity(target, 5, slot, 1)
    if mode == 5 and category not in (0, 3):
        item["caps"][category] = rng.choice((0, 1, CAPACITIES[category] // 2))
    if category == 0:
        for slot in range(CAPACITIES[category]):
            patch(target, slot * STRIDES[category] + 3, 0x5A)
            patch(target, slot * STRIDES[category] + 7, 0xA5)
    if category == 6 and mode == 1:
        patch(origin, OFFSETS[6] + index * 4 + 2, 0)
        patch(target, OFFSETS[6] + 2, 1)
        for slot in (1, 254, 255):
            identity(target, 6, slot, 14)
    item["blocks"][0] = target
    if not item["alias"]:
        item["blocks"][1] = origin


def vector(kind, rng: random.Random, number):
    item = {
        "flags": 0, "index": 0, "alias": False, "caps": list(CAPACITIES),
        "inv_caps": list(INVENTORY_CAPACITIES), "blocks": [None] * 7, "pending": [],
    }
    if kind == 2:
        copy_vector(rng, number, item)
    elif kind in (0, 1):
        other, preview, staged = block(), block(rng.randrange(1, 256)), block()
        identity(other, 6, 0, 12)
        patch(other, OFFSETS[6] + 2, rng.choice((0, 90, 99, 100, 150)))
        identity(other, 6, 255, 14)
        for slot in (0, 1, 255):
            identity(staged, 6, slot, 12 if slot != 1 else 15)
            patch(staged, OFFSETS[6] + slot * 4 + 2, rng.choice((0, 1, 99, 100, 255)))
        item["pending"] = [0x600, 0x100, 0x601, 0x602, 0] if number % 3 else [0]
        item["blocks"][0], item["blocks"][2], item["blocks"][3] = other, preview, staged
    elif kind == 3:
        category = number % 7
        item["flags"] = category
        target = block(rng.randrange(1, 256))
        for slot in range(CAPACITIES[category]):
            identity(target, category, slot, 0)
        for slot in (0, CAPACITIES[category] - 1, rng.randrange(CAPACITIES[category])):
            identity(target, category, slot, rng.randrange(1, 256))
        tokens = [rng.choice((0, 0, 1, CAPACITIES[category], rng.randrange(1, CAPACITIES[category] + 1)))
                  for _ in range(CAPACITIES[category])]
        order_patch(target, item["caps"], category, tokens)
        item["blocks"][0] = target
    elif kind == 4:
        item["blocks"][4] = block(rng.randrange(1, 256))
        item["blocks"][5] = block(rng.randrange(1, 256))
        item["blocks"][6] = block(rng.randrange(128, 256))
        if number % 4 == 0:
            item["inv_caps"][number % 7] = 0
    elif kind == 5:
        target = block()
        for category in range(7):
            record_bytes(target, category, 0, rng)
            record_bytes(target, category, CAPACITIES[category] - 1, rng)
            identity(target, category, 0, 1)
            identity(target, category, CAPACITIES[category] - 1, 2)
            order_patch(target, item["caps"], category,
                        [CAPACITIES[category], 0, 1, CAPACITIES[category]])
        item["blocks"][4] = target
        item["blocks"][5] = block(rng.randrange(1, 256))
        item["blocks"][6] = block(rng.randrange(1, 256))
    else:
        category = (1, 2, 5, 0, 3, 4, 6, 7, 123)[number % 9]
        item["flags"] = category
        item["index"] = rng.choice((0, CAPACITIES[min(category, 7)] - 1))
        target = block(rng.randrange(1, 256))
        if category in (1, 2, 5):
            offset, width = ((0xA2A, 1) if category == 1 else (0x150C, 2)
                             if category == 2 else (0x2318, 2))
            parent_boundaries = (65535, 32768, 64, 127, 129, 128, 256, 1, 0, 255)
            patch(target, offset + item["index"] * STRIDES[category],
                  parent_boundaries[(number // 9) % len(parent_boundaries)], width)
        item["blocks"][0] = target
    return item


def materialize(specification, size):
    result = bytearray([specification["fill"]]) * size
    for offset, value, width in specification["patches"]:
        result[offset:offset+width] = (value & ((1 << (width * 8)) - 1)).to_bytes(width, "little")
    return bytes(result)


def run(kind, machine, item):
    before = [
        materialize(specification, size) if specification else b""
        for specification, size in zip(item["blocks"], SIZES, strict=True)
    ]
    for address, raw in zip(ADDRESSES, before, strict=True):
        if raw:
            machine.store(address, raw)
    for n, capacity in enumerate(item["caps"]):
        machine.put(CAPACITY + n * 2, capacity, 2)
    for n, capacity in enumerate(item["inv_caps"]):
        machine.put(INV_CAPACITY + n, capacity, 1)
    inventory_offset = 0
    for category, capacity in enumerate(INVENTORY_CAPACITIES[:7]):
        machine.put(INV_POINTERS + category * 4, ADDRESSES[6] + inventory_offset)
        inventory_offset += capacity
    machine.put(STORAGE_POINTER, STORAGE)
    machine.put(OTHER_POINTER, ADDRESSES[0])
    machine.put(COUNT, 49371, 2)
    for row, token in enumerate(item["pending"]):
        machine.put(STORAGE + 0xC3B0 + row * 2, token, 2)
    machine.follow.update((
        0x80102C94, 0x80102CD0, 0x80102D28, 0x80102F88, 0x80103070, 0x80103110,
        *(entry for entry, _ in RANGES), 0x800CCC1C, 0x800CCC34, 0x800C01C0, 0x800C01E0,
    ))
    if kind == 2:
        machine.r[4:8] = [
            item["flags"] & 0xFFFFFFFF, ADDRESSES[0], item["index"] & 0xFFFFFFFF,
            ADDRESSES[0] if item["alias"] else ADDRESSES[1],
        ]
    elif kind == 3:
        machine.r[4:6] = [item["flags"], ADDRESSES[0]]
    elif kind in (6, 7):
        machine.r[4:7] = [item["flags"], item["index"], ADDRESSES[0]]
    machine.execute(RANGES[kind][0])
    value = (machine.load(COUNT, 2) if kind == 1 else s32(machine.r[2])
             if kind in (2, 6, 7) else 0)
    changes = [
        (block_index, offset, value)
        for block_index, (address, raw) in enumerate(zip(ADDRESSES, before, strict=True))
        for offset, value in enumerate(machine.bytes(address, len(raw))) if value != raw[offset]
    ]
    mutable = {0: (2,), 1: (2,), 2: (0,), 3: (0,), 4: (4,), 5: (4, 5, 6), 6: (), 7: ()}[kind]
    allowed = [(DEFAULT_STACK - 0x4000, DEFAULT_STACK)]
    if kind == 2:
        # 80103300 spills a3 at original_sp+12, inside the caller's ABI
        # argument-home area, not outside the supplied argument frame.
        allowed.append((DEFAULT_STACK, DEFAULT_STACK + 16))
    allowed += [(ADDRESSES[n], ADDRESSES[n] + SIZES[n]) for n in mutable]
    if kind == 1:
        allowed.append((COUNT, COUNT + 2))
    inputs = [kind, item["flags"], item["index"], int(item["alias"]),
              *item["caps"], *item["inv_caps"]]
    for specification in item["blocks"]:
        inputs += [int(specification is not None), specification["fill"] if specification else 0,
                   len(specification["patches"]) if specification else 0]
        if specification:
            inputs += [value for patch_value in specification["patches"] for value in patch_value]
    inputs += [len(item["pending"]), *item["pending"]]
    return {"value": value, "changes": changes, "stores": machine.stores, "allowed": allowed}, inputs


def check(image, item, observation, native, reference):
    changes = [tuple(native[2+n*3:5+n*3]) for n in range(native[1])]
    problems = []
    if native[0] != observation["value"]:
        problems.append(f"result native={native[0]} original={observation['value']}")
    if changes != observation["changes"]:
        mismatch = next((n for n, (ours, theirs) in
                         enumerate(zip(changes, observation["changes"], strict=False))
                         if ours != theirs), min(len(changes), len(observation["changes"])))
        problems.append(f"complete record/order byte deltas differ at {mismatch}: "
                        f"native={changes[mismatch:mismatch+1]} "
                        f"original={observation['changes'][mismatch:mismatch+1]}")
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(image, results):
    return [
        "No original callees replaced: actual recursive MENUD CopyItem, CategoryIndices/ItemId/Count/FindEmpty/"
        "FindRow/Append/Rebuild and actual BATTLE copy/zero instructions followed.",
        "Compared complete supplied source/target/preview/staged/bag/inventory/index byte deltas and scalar result; "
        "original stores bounded to actual mutable owners and stack.",
        "Defined cases include shared references, exact source-target alias, valid empty records, "
        "full tables, optional third/fourth gems, overflow quantities, high-bit indices and parent widths.",
        "Missing mandatory parts, invalid out-of-table indices and unterminated pending arrays are diagnosed "
        "by checked native Live probes, not claimed as defined original parity.",
    ]
