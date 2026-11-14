"""Asset-free metadata and real MENUD pending/row execution with actual formatter/stat callees."""

from __future__ import annotations

import functools
import random

from differential import DEFAULT_STACK, DISC, MASK, MODULE_BASES, s32, store_problems

MODULES = (
    r"MENU\MENUD.PRG",
    r"MENU\MAINMENU.PRG",
    r"BATTLE\BATTLE.PRG",
    "SLUS_010.40",
)
MODULE_BASES.update(
    {MODULES[0]: 0x80102800, MODULES[1]: 0x800F9800, MODULES[2]: 0x80068800}
)
RANGES = (
    (0x801069B0, 0x80106A04),
    (0x80106A04, 0x80106A50),
    (0x80106A50, 0x80106C64),
    (0x80106C64, 0x801071D8),
)
CAPACITIES = (32, 64, 64, 32, 64, 192, 256, 2)
OFFSETS = (0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300, 0x3800)
STRIDES = (32, 44, 16, 48, 40, 28, 4)
BLOCK_BYTES = 0x3C00
STORAGE = 0x80600000
OWNERS = (0x80700000, STORAGE + 0x87B0)
PENDING = STORAGE + 0xC3B0
HELP, NAMES = 0x80800000, 0x80810000
BUFFER, TYPES, TEXTS = 0x80900000, 0x80910000, 0x80920000
TEMP = 0x80A00000
DESCRIPTION_BYTES, MAX_ROWS = 96, 256
CONTEXT = 0x800F5168
CONTEXT_BYTES = 10 * 4 + 10 * 4
FOLLOW = (
    0x80102C94,
    0x80102F88,
    0x80103C9C,
    0x80103D14,
    0x801041E0,
    0x8010425C,
    0x80102A34,
    0x80102BB0,
    0x800FC85C,
    0x800FCAA4,
    0x800FCC0C,
    0x800FCCE8,
    0x800FCECC,
    0x800FD084,
    0x800FD0E0,
    0x800FD17C,
    0x800FA598,
    0x8006A334,
    0x8006A65C,
    0x8006A9F0,
    0x8006AB44,
    0x8006AC74,
    0x8006ACFC,
    0x8006AE0C,
    0x8006B110,
    0x800490B0,
    0x80049110,
    0x800C01E0,
    0x800CCC1C,
    0x800C685C,
    0x801069B0,
    0x80106A04,
)
MUTANTS = (
    {
        "category-shift": {0x801069B8: 0x00042240},
        "slot-plus-two": {0x801069BC: 0x24A50002},
        "signed-halfword": {0x801069D4: 0x844243B0},
        "inverted-match": {0x801069DC: 0x14440007},
        "zero-based-result": {0x801069E0: 0x24620000},
        "last-entry-omitted": {0x801069E8: 0x2862003F},
        "wrong-token-stride": {0x801069F0: 0x24A50004},
        "missing-result-is-one": {0x801069F8: 0x24020001},
    },
    {
        "category-shift": {0x80106A08: 0x00042240},
        "slot-plus-two": {0x80106A0C: 0x24A50002},
        "overwrites-first-occupied": {0x80106A2C: 0x10400003},
        "zero-store": {0x80106A38: 0xA4600000},
        "word-store": {0x80106A38: 0xAC640000},
        "last-entry-omitted": {0x80106A3C: 0x28C2003F},
        "wrong-token-stride": {0x80106A44: 0x24630004},
    },
    {
        "prune-keeps-only-absent": {0x80106AB8: 0x10400002},
        "prune-clear-omitted": {0x80106AC0: 0},
        "prune-slot-mask": {0x80106AAC: 0x30A5007F},
        "prune-last-entry-omitted": {0x80106AC8: 0x2A02003F},
        "compaction-zeroes-live-value": {0x80106B68: 0xA4600000},
        "compaction-tail-not-cleared": {0x80106B90: 0},
        "capacity-narrowed-to-byte": {0x80106BC4: 0x92620000},
        "duplicate-search-call-omitted": {0x80106BFC: 0},
        "missing-append-call": {0x80106C0C: 0},
        "misc-category-omitted": {0x80106C30: 0x2A220006},
    },
    {
        "pending-slot-mask": {0x80106CF4: 0x3231007F},
        "pending-last-row-omitted": {0x80106E88: 0x2BC2003F},
        "pending-shield-attachment-selectable": {0x80106E28: 0x3415CC00},
        "pending-weapon-attachment-selectable": {0x80106E60: 0x3415CC00},
        "pending-description-stride": {0x80106E7C: 0x2673005C},
        "pending-row-type-store-omitted": {0x80106E84: 0},
        "misc-last-row-omitted": {0x80106F1C: 0x2BC200FF},
        "active-record-one-based": {0x80106F78: 0x26310000},
        "weapon-equipped-field": {0x80106FCC: 0x90420000},
        "unworn-weapon-parent-marked-equipped": {0x80107118: 0x3415CA00},
        "unworn-shield-parent-marked-equipped": {0x80107158: 0x3415CA00},
        "active-description-stride": {0x80107164: 0x2673005C},
        "active-row-flags-store-omitted": {0x80107184: 0},
    },
)


def configuration(kind: int) -> dict:
    """Provide one original function's independent deterministic proof configuration."""
    entry, end = RANGES[kind]
    return {
        "ID": f"F{2979 + kind:05}",
        "ENTRY": entry,
        "END": end,
        "MODULES": MODULES,
        "DRIVER": "container_pending_rows_driver.cpp",
        "INCLUDES": (r"Native\Game\src", r"Native\Tests\include"),
        "SOURCES": (
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
        "VECTORS": (512, 512, 128, 256)[kind],
        "MUTANT_VECTORS": (128, 128, 32, 64)[kind],
        "SEED": 2979 + kind,
        "MUTANTS": MUTANTS[kind],
        "vector": functools.partial(vector, kind),
        "run": functools.partial(run, kind),
        "check": check,
        "describe": describe,
    }


def put(block: bytearray, offset: int, value: int, size: int = 1) -> None:
    """Build input record bytes using audited disk widths, not native pointers."""
    block[offset : offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(
        size, "little"
    )


def records(rng: random.Random, number: int) -> bytes:
    """Construct coherent first/last parts with real stats and bounded attachments."""
    block = bytearray(BLOCK_BYTES)
    for slot in (0, 31):
        weapon = slot * STRIDES[0]
        block[weapon : weapon + STRIDES[0]] = rng.randbytes(STRIDES[0])
        block[weapon : weapon + 8] = bytes(
            (slot + 1, 64, 64, number // 3 % 2, 192, 1, 0, 0)
        )
        block[weapon + 8 : weapon + 32] = bytes((1 + number % 20, 0xE7)) + bytes(22)
        shield = OFFSETS[3] + slot * STRIDES[3]
        block[shield : shield + STRIDES[3]] = rng.randbytes(STRIDES[3])
        put(block, shield, slot + 1)
        put(block, shield + 1, number // 2 % 2)
        put(block, shield + 4, 112 + number % 16)
        put(block, shield + 7, 1)
        put(block, shield + 16, 3)
        put(block, shield + 40, 1, 2)
        put(block, shield + 42, 0)
        block[shield + 44 : shield + 48] = bytes((1, 127, 0, 0))
    for slot in (0, 63):
        blade = OFFSETS[1] + slot * STRIDES[1]
        block[blade : blade + STRIDES[1]] = rng.randbytes(STRIDES[1])
        put(block, blade, 1 + number % 12)
        put(block, blade + 3, 1 + number % 10)
        put(block, blade + 16, 1 + number % 3)
        put(block, blade + 40, 1 + number % 7, 2)
        put(block, blade + 42, (0, 1, 32)[number % 3])
        grip = OFFSETS[2] + slot * STRIDES[2]
        block[grip : grip + STRIDES[2]] = rng.randbytes(STRIDES[2])
        put(block, grip, 97 + number % 15, 2)
        put(block, grip + 3, 1 + number % 3)
        put(block, grip + 4, 3)
        put(block, grip + 12, (0, 1, 32)[number // 2 % 3], 2)
        armor = OFFSETS[4] + slot * STRIDES[4]
        block[armor : armor + STRIDES[4]] = rng.randbytes(STRIDES[4])
        accessory = number % 5 == 0
        put(block, armor, 223 + number % 32 if accessory else 145 + number % 16)
        put(block, armor + 3, 7 if accessory else 1 + number % 6)
        put(block, armor + 36, 1 + number % 7, 2)
        put(block, armor + 38, number % 2)
    for slot in (0, 126, 191):
        gem = OFFSETS[5] + slot * STRIDES[5]
        block[gem : gem + STRIDES[5]] = rng.randbytes(STRIDES[5])
        put(block, gem, 261 + number % 62, 2)
        put(block, gem + 24, (0, 1, 32, 0x81, 0xA0)[number % 5], 2)
    for slot in (0, 255):
        misc = OFFSETS[6] + slot * STRIDES[6]
        put(block, misc, 323 + number % 20, 2)
        put(block, misc + 2, (0, 1, 99, 100, 128, 255)[number % 6])
        put(block, misc + 3, slot + 1)
    return bytes(block)


def vector(kind: int, rng: random.Random, number: int) -> dict:
    """Bias holes, full lists, word wrap, attachments, aliasing and every final slot."""
    category = number % (8 if kind == 3 else 7)
    slot = (0, CAPACITIES[category] - 1)[number // 7 % 2]
    item = {
        "kind": kind,
        "category": category,
        "slot": slot,
        "staged": number % 2,
        "active": number // 2 % 2,
        "fill": 0xA5,
        "caps": list(CAPACITIES),
        "pending": [0] * 64,
        "records": [],
        "order": [],
        "types": [],
    }
    if kind < 2:
        key_categories = (0, 1, 5, 6, 0xFF, 0x100, 0x10000, 0x1000000, -1)
        key_slots = (0, 1, 191, 255, -1, 0xFFFF, 0x10000, 0x7FFFFFFF, -0x80000000)
        item["category"] = key_categories[number % len(key_categories)]
        item["slot"] = key_slots[number // len(key_categories) % len(key_slots)]
        item["pending"] = [rng.randrange(1, 65536) for _ in range(64)]
        word = ((item["category"] << 8) | ((item["slot"] + 1) & MASK)) & MASK
        position = (0, 1, 31, 63)[number // 3 % 4]
        if number % 4 == 0:
            item["pending"][position] = word & 0xFFFF
        elif number % 4 != 3:
            item["pending"][position] = 0
        if number % 16 == 0:
            item["pending"] = [0] * 64
            item["pending"][-1] = word & 0xFFFF
        return item
    item["records"] = [records(rng, number), records(rng, number + 1)]
    if kind == 2:
        choices = [
            1,
            32,
            0x101,
            0x140,
            0x201,
            0x240,
            0x301,
            0x320,
            0x401,
            0x440,
            0x501,
            0x57F,
            0x5C0,
            0x601,
            0x700,
            0xFF01,
            0x102,
            0x202,
            0x402,
            0x502,
            0x602,
        ]
        item["pending"] = [
            rng.choice(choices) if rng.random() < 0.6 else 0 for _ in range(64)
        ]
        if number % 8 == 0:
            item["pending"] = [0] * 63 + [0x102]
        elif number % 8 == 1:
            item["pending"] = [0x601] * 64
        elif number % 8 == 2:
            item["pending"] = [0, 0x5C0, 0, 0x101] + [0] * 60
        return item
    category = item["category"]
    item["types"] = [0xA5000000 | row for row in range(MAX_ROWS)]
    item["order"] = [0] * sum(CAPACITIES[:-1])
    if category == 7:
        mode = number // 8 % 8
        tokens = [1, 0x140, 0x240, 0x320, 0x440, 0x5C0, 0x601]
        if mode == 1:
            tokens = [0xFF01, 0x601]
        elif mode == 2:
            tokens = [0x700, 0x601]
        elif mode == 3:
            tokens = [0x601] * 64
        elif mode == 4:
            tokens = [0x5C0, 0x501, 0x140, 0x240]
        item["pending"] = tokens + [0] * (64 - len(tokens))
    else:
        offset = sum(CAPACITIES[:category])
        tokens = [1, CAPACITIES[category]]
        if number == 6:
            tokens = list(range(1, 257))
            block = bytearray(item["records"][item["active"]])
            for record in range(256):
                put(block, OFFSETS[6] + record * 4, 323 + record % 20, 2)
                put(block, OFFSETS[6] + record * 4 + 2, record)
            item["records"][item["active"]] = bytes(block)
        elif category != 6 and number // 8 % 4 == 1:
            item["caps"][category] = 1
        item["order"][offset : offset + len(tokens)] = tokens
    return item


@functools.cache
def texts() -> tuple[bytes, bytes]:
    """Require real item text assets only when executing a vector, never when importing/listing."""
    return (
        (DISC / "MENU" / "ITEMHELP.BIN").read_bytes(),
        (DISC / "MENU" / "ITEMNAME.BIN").read_bytes(),
    )


def normalize_pointer(pointer: int) -> tuple[int, int]:
    """Compare actual text ownership and offsets independently of native pointer width."""
    for kind, base, size in (
        (2, HELP, 28672),
        (3, NAMES, 12288),
        (4, BUFFER, DESCRIPTION_BYTES * MAX_ROWS),
        (5, OWNERS[0], BLOCK_BYTES),
        (6, OWNERS[1], BLOCK_BYTES),
    ):
        if base <= pointer < base + size:
            return kind, pointer - base
    return 0, pointer


def run(kind: int, machine, item: dict) -> tuple[dict, list[int]]:
    """Execute the complete entry and actual count/copy/clear/stats/text instruction bodies."""
    machine.map_zero(STORAGE, 0x14730)
    machine.map_zero(OWNERS[0], 0x4180)
    machine.put(0x8010245C, STORAGE)
    for row, token in enumerate(item["pending"]):
        machine.put(PENDING + row * 2, token, 2)
    for category, capacity in enumerate(item["caps"]):
        machine.put(0x80109944 + category * 2, capacity, 2)
    allowed = [(DEFAULT_STACK - 0x1000, DEFAULT_STACK), (PENDING, PENDING + 128)]
    machine.follow.update(FOLLOW)
    inputs = [
        kind,
        item["category"],
        item["slot"],
        item["staged"],
        item["active"],
        item["fill"],
        *item["caps"],
        *item["pending"],
    ]
    if kind >= 2:
        staged_bytes = item["records"][item["staged"]]
        other_bytes = item["records"][1 - item["staged"]]
        machine.store(OWNERS[1], staged_bytes)
        machine.store(OWNERS[0], other_bytes)
        # Native identities 0 and 1 retain their original vector ownership.
        mapped = [other_bytes, staged_bytes]
        inputs += [*item["records"][0], *item["records"][1]]
    if kind == 3:
        machine.put(
            0x80109A8C,
            OWNERS[1] if item["active"] == item["staged"] else OWNERS[0],
        )
        machine.map_zero(CONTEXT, CONTEXT_BYTES)
        machine.map_zero(TEMP, 0x1000)
        help_text, names = texts()
        machine.store(HELP, help_text)
        machine.store(NAMES, names)
        machine.put(0x80102540, HELP)
        machine.put(0x8010229C, NAMES)
        active = OWNERS[1] if item["active"] == item["staged"] else OWNERS[0]
        if item["category"] != 7:
            for row, token in enumerate(item["order"]):
                machine.put(active + BLOCK_BYTES + row * 2, token, 2)
        machine.store(BUFFER, bytes([item["fill"]]) * (DESCRIPTION_BYTES * MAX_ROWS))
        for row, value in enumerate(item["types"]):
            machine.put(TYPES + row * 4, value)
            machine.put(TEXTS + row * 8, 0x50000000 + row * 8)
            machine.put(TEXTS + row * 8 + 4, 0x50000000 + row * 8 + 4)
        # Allocation and release are the only opaque service boundaries.
        # Zeroing, copies, stat math, ranking and printf execute original code.
        machine.stub(0x80043E3C, lambda m: m.r.__setitem__(2, TEMP))
        machine.stub(0x80043C60)
        machine.r[4:8] = [item["category"], TEXTS, TYPES, BUFFER]
        allowed += [
            (BUFFER, BUFFER + DESCRIPTION_BYTES * MAX_ROWS),
            (TYPES, TYPES + MAX_ROWS * 4),
            (TEXTS, TEXTS + MAX_ROWS * 8),
            (CONTEXT, CONTEXT + CONTEXT_BYTES),
            (TEMP, TEMP + 0x1000),
        ]
        inputs += [*item["order"], *item["types"]]
    else:
        machine.r[4:6] = [item["category"] & MASK, item["slot"] & MASK]
    machine.execute(RANGES[kind][0])
    values = [
        s32(machine.r[2]) if kind in (0, 3) else 0,
        *(machine.load(PENDING + row * 2, 2) for row in range(64)),
    ]
    if kind == 3:
        values += [machine.load(TYPES + row * 4) for row in range(MAX_ROWS)]
        for row in range(MAX_ROWS):
            for text in range(2):
                source, offset = normalize_pointer(
                    machine.load(TEXTS + (row * 2 + text) * 4)
                )
                if source in (5, 6):
                    source = 5 + (item["staged"] if source == 6 else 1 - item["staged"])
                values += [source, offset]
        values += machine.bytes(BUFFER, DESCRIPTION_BYTES * MAX_ROWS)
        if any(
            machine.bytes(base, BLOCK_BYTES) != data
            for base, data in zip(OWNERS, mapped, strict=True)
        ):
            raise RuntimeError("An original row helper changed its source records")
    observation = {
        "values": values,
        "stores": machine.stores,
        "allowed": allowed,
        "steps": machine.steps,
    }
    return observation, inputs


def check(
    image, item: dict, observation: dict, native: list[int], reference
) -> list[str]:
    """Compare complete outputs/owners/text bytes and bound every original store."""
    expected = observation["values"]
    if len(native) != len(expected):
        return [f"output length {len(native)} != original {len(expected)}"]
    first = next(
        (
            index
            for index, (left, right) in enumerate(zip(native, expected, strict=True))
            if left & MASK != right & MASK
        ),
        None,
    )
    problems = (
        []
        if first is None
        else [
            f"output[{first}] native {native[first] & MASK:#x} "
            f"original {expected[first] & MASK:#x}"
        ]
    )
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(image, results: list) -> list[str]:
    """Record actual instruction counts and the separately bounded renderer evidence."""
    return [
        f"Complete original and real helper execution: {sum(item[1]['steps'] for item in results)} steps",
        "Original count/copy/clear/stat/ranking/printf helpers are followed, never opaque math/owner stubs",
        "Physical current-native row/font/packet/OT output is separate production-linked Catch2 evidence",
    ]
