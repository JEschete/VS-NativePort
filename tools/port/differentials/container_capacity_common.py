"""MENUD capacity/staging proofs over actual original count/copy/clear instructions."""

from __future__ import annotations

import functools
import random

from differential import DEFAULT_STACK, MODULE_BASES, s32, store_problems
from differentials.container_transfers_common import (
    CAPACITIES,
    INVENTORY_CAPACITIES,
    OFFSETS,
    STRIDES,
    block,
    identity,
    materialize,
    patch,
    record_bytes,
)

MODULES = (r"MENU\MENUD.PRG", r"BATTLE\BATTLE.PRG")
MODULE_BASES.update({MODULES[0]: 0x80102800, MODULES[1]: 0x80068800})
DRIVER = "container_capacity_driver.cpp"
SOURCES = (
    r"Native\Game\src\Items\ContainerGemCounts.cpp",
    r"Native\Game\src\Menu\Container\ContainerCapacity.cpp",
    r"Native\Game\src\Items\ContainerIndexing.cpp",
    r"Native\Game\src\Items\ContainerTransfers.cpp",
)
INCLUDES = (r"Native\Game\src", r"Native\Tests\include")
STORAGE = 0x80800000
BLOCK_BYTES = 0x3C00
ADDRESSES = (
    0x80600000,
    STORAGE + 0xC430,
    STORAGE + 0x87B0,
    STORAGE + 0x105B0,
)
CAPACITY = 0x80109944
INVENTORY_LIMIT = 0x801022A0
STORAGE_POINTER = 0x8010245C
ACTIVE_POINTER = 0x80109A8C
OTHER_POINTER = 0x80109A88
DIRECTION = 0x80109A7A
PENDING = STORAGE + 0xC3B0
RANGES = (
    (0x80106464, 0x80106504),
    (0x80106504, 0x80106624),
    (0x80106624, 0x80106684),
    (0x80106684, 0x801066DC),
    (0x801066DC, 0x80106784),
    (0x80106784, 0x801068BC),
    (0x801068BC, 0x80106948),
    (0x80106948, 0x801069B0),
)
MUTATIONS = (
    {
        "capacity-draw-call": (0x80106498, 0),
        "used-separation": (0x801064A8, 0x2610FFE1),
        "slash-draw-call": (0x801064B8, 0),
        "half-doubling": (0x801064C0, 0x00131080),
        "half-equality-direction": (0x801064C8, 0x14400003),
        "highlight-color": (0x801064D4, 0x36524021),
        "occupied-draw-call": (0x801064E0, 0),
    },
    {
        "heading-count": (0x80106554, 0x2A020001),
        "heading-separation": (0x80106548, 0x26310040),
        "bag-capacity-byte": (0x801065A0, 0x90640001),
        "bag-unsigned-limit": (0x801065A0, 0x80640000),
        "bag-count-position": (0x80106598, 0x36250071),
        "category-label-stride": (0x801065BC, 0x26520000),
        "container-count-position": (0x801065D8, 0x362500F9),
        "all-seven-categories": (0x801065F4, 0x2A020006),
    },
    {
        "weapon-stride": (0x8010662C, 0x00042100),
        "mandatory-grip-one-based": (0x8010663C, 0x24420000),
        "grip-stride": (0x80106640, 0x000210C0),
        "grip-slot-field": (0x80106648, 0x90621505),
        "zero-slots-branch": (0x80106650, 0x1440000A),
        "reference-byte": (0x80106660, 0x90420005),
        "nonzero-reference": (0x80106668, 0x2C420001),
        "terminal-delay-result": (0x80106680, 0x00801021),
    },
    {
        "shield-stride": (0x80106694, 0x000318C0),
        "shield-base": (0x8010669C, 0x246303FC),
        "shield-slot-field": (0x801066A4, 0x90C40011),
        "shield-zero-slots": (0x801066AC, 0x14800009),
        "shield-reference-byte": (0x801066B8, 0x9042002D),
        "shield-nonzero-reference": (0x801066C0, 0x2C420001),
        "shield-terminal-result": (0x801066D8, 0x00A01021),
    },
    {
        "destination-count-call": (0x801066FC, 0),
        "staged-count-call": (0x80106718, 0),
        "combine-staged-count": (0x80106720, 0x02028023),
        "receiving-direction": (0x80106730, 0x10400007),
        "additional-count": (0x80106734, 0x02308823),
        "bag-unsigned-capacity": (0x80106744, 0x80420000),
        "signed-capacity-comparison": (0x80106768, 0x0051102B),
    },
    {
        "pending-all-64": (0x801067C8, 0x2A02003F),
        "weapon-two-parts": (0x801067E4, 0x26030001),
        "shield-gem-reservation": (0x80106800, 0x02008021),
        "base-category-capacity": (0x80106818, 0),
        "blade-category-capacity": (0x80106830, 0x24040002),
        "grip-additional-count": (0x8010684C, 0x24050000),
        "weapon-gem-category": (0x80106860, 0x24040004),
        "shield-gem-category": (0x80106888, 0x24040004),
        "fits-result": (0x801068A0, 0x24020001),
    },
    {
        "selected-token-stride": (0x801068C8, 0x00042080),
        "selected-category": (0x80106900, 0x00109243),
        "selected-slot-mask": (0x8010690C, 0x3210007F),
        "undo-copy-mode": (0x80106904, 0x36440000),
        "undo-copy-call": (0x80106910, 0),
        "undo-clear-call": (0x80106924, 0),
        "undo-clear-selected-slot": (0x80106920, 0x26050001),
    },
    {
        "stage-copy-mode": (0x8010695C, 0x36240000),
        "stage-copy-target": (0x80106978, 0x340387AC),
        "stage-copy-call": (0x80106980, 0),
        "stage-clear-category": (0x80106988, 0x00002021),
        "stage-clear-call": (0x80106990, 0),
        "stage-clear-slot": (0x80106994, 0x26450001),
    },
)


def configuration(kind: int) -> dict:
    """Supply one original ID with fixed seeds, cases and detected-instruction targets."""
    entry, end = RANGES[kind]
    return {
        "ID": f"F{2971 + kind:05}",
        "ENTRY": entry,
        "END": end,
        "MODULES": MODULES,
        "DRIVER": DRIVER,
        "SOURCES": SOURCES,
        "INCLUDES": INCLUDES,
        "VECTORS": (2048, 256, 1024, 1024, 512, 512, 256, 256)[kind],
        "SEED": 2971 + kind,
        "MUTANT_VECTORS": 256 if kind == 5 else 128,
        "MUTANTS": {
            name: {address: word} for name, (address, word) in MUTATIONS[kind].items()
        },
        "vector": functools.partial(vector, kind),
        "run": functools.partial(run, kind),
        "check": check,
        "describe": functools.partial(describe, kind),
    }


def fill(target: dict, category: int, count: int) -> None:
    """Populate actual category identities rather than substituting a count oracle."""
    for slot in range(count):
        identity(target, category, slot, 1)


def attach(target: dict, category: int, slot: int, rng: random.Random) -> None:
    """Build real last-slot equipment records with mandatory and optional references."""
    record_bytes(target, category, slot, rng)
    if category == 0:
        identity(target, 0, slot, 64)
        patch(target, slot * STRIDES[0] + 2, 64)
        for part in (1, 2):
            record_bytes(target, part, 63, rng)
            identity(target, part, 63, 1)
        patch(target, OFFSETS[2] + 63 * STRIDES[2] + 4, 3)
        start = slot * STRIDES[0] + 4
    elif category == 3:
        identity(target, 3, slot, 1)
        patch(target, OFFSETS[3] + slot * STRIDES[3] + 16, 3)
        start = OFFSETS[3] + slot * STRIDES[3] + 44
    else:
        identity(target, category, slot, 21 if category != 6 else 12)
        if category == 6:
            patch(target, OFFSETS[6] + slot * STRIDES[6] + 2, 17)
        return
    for number, reference in enumerate((192, 0, 1, 233)):
        patch(target, start + number, reference)
    for gem in (0, 191):
        record_bytes(target, 5, gem, rng)
        identity(target, 5, gem, 300 + gem)


def projection(item: dict, rng: random.Random, number: int) -> None:
    """Bias actual receiving/staged records around every capacity and equality boundary."""
    category = number % 8
    item["category"] = category
    item["direction"] = (0, 1, 255)[number // 8 % 3]
    if category == 7:
        item["additional"] = (0, 1, 2, 3, -1)[number // 24 % 5]
        return
    limit = (
        item["caps"][category]
        if item["direction"]
        else item["inventory_limits"][category]
    )
    mode = number // 24 % 8
    count = max(0, min(CAPACITIES[category], limit - (mode % 3)))
    fill(item["blocks"][1], category, count)
    fill(item["blocks"][2], category, 0 if mode < 3 else 1)
    item["additional"] = (0, 1, 2, -1, 0x7FFFFFFF, -0x80000000, 1, 3)[mode]
    if number % 13 == 0:
        item["alias"] = 1
        fill(item["blocks"][0], category, count)
    elif number % 13 == 1:
        item["alias"] = 3
    if number % 31 == 0:
        item["inventory_limits"][category] = rng.choice((128, 255))


def capacity_error(item: dict, rng: random.Random, number: int) -> None:
    """Reach each equipment attachment gate and the real 63/64 pending-list edge."""
    category = number % 7
    item["category"] = category
    item["slot"] = CAPACITIES[category] - 1
    item["direction"] = number // 7 % 2
    mode = number // 14 % 9
    attach(item["blocks"][0], category, item["slot"], rng)
    occupied = (0, 59, 60, 61, 62, 63, 64, 1, 0)[mode]
    item["pending"] = [0xFFFF] * occupied + [0] * (64 - occupied)
    if mode in (0, 7, 8):
        blocked = (category, 1, 2, 5)[number // 7 % 4]
        if category == 3:
            blocked = (3, 5)[number // 7 % 2]
        maximum = (
            item["caps"][blocked]
            if item["direction"]
            else item["inventory_limits"][blocked]
        )
        fill(item["blocks"][1], blocked, maximum - (number % 3 == 0))
        fill(item["blocks"][2], blocked, number % 3 == 1)
    if category in (0, 3) and number % 5 == 0:
        slots = number // 5 % 5
        if category == 0:
            patch(item["blocks"][0], OFFSETS[2] + 63 * STRIDES[2] + 4, slots)
        else:
            patch(
                item["blocks"][0],
                OFFSETS[3] + item["slot"] * STRIDES[3] + 16,
                slots,
            )


def staging(item: dict, rng: random.Random, number: int, undo: bool) -> None:
    """Exercise all seven full-size records, real recursive helpers and exact aliases."""
    category = number % 7
    item["category"] = category
    item["slot"] = (
        (0, 254, 255)[number // 7 % 3]
        if undo and category == 6
        else CAPACITIES[category] - 1
    )
    item["row"] = (0, 1, 31, 63)[number // 7 % 4]
    origin = item["blocks"][2] if undo else item["blocks"][0]
    target = item["blocks"][0] if undo else item["blocks"][2]
    attach(origin, category, item["slot"], rng)
    mode = number // 28 % 7
    if mode == 1:
        fill(target, category, CAPACITIES[category])
    elif mode == 2 and category in (0, 3):
        fill(target, 5, CAPACITIES[5])
    elif mode == 3 and category == 0:
        fill(target, 1, CAPACITIES[1])
    elif mode == 4 and category == 0:
        fill(target, 2, CAPACITIES[2])
    elif mode == 5:
        item["alias"] = 2
        item["blocks"][2] = origin
    elif mode == 6 and category == 6:
        for slot, quantity in ((0, 99), (1, 100), (254, 150)):
            identity(target, category, slot, 12)
            patch(target, OFFSETS[6] + slot * STRIDES[6] + 2, quantity)
    item["pending"] = [0xFFFF] * 64
    item["pending"][item["row"]] = category << 8 | (item["slot"] + 1)


def vector(kind: int, rng: random.Random, number: int) -> dict:
    """Generate fixed-seed defined records with explicit thresholds and nonzero canaries."""
    item = {
        "category": 0,
        "slot": 0,
        "row": 0,
        "additional": 0,
        "limit": 0,
        "position": 0,
        "occupied": 0,
        "direction": 0,
        "alias": 0,
        "callback": 0,
        "caps": list(CAPACITIES),
        "inventory_limits": list(INVENTORY_CAPACITIES),
        "blocks": [block() for _ in ADDRESSES],
        "pending": [0] * 64,
    }
    if kind == 0:
        item["limit"] = (0, 1, 2, 8, 32, 64, 192, 256)[number % 8]
        half = item["limit"] // 2
        item["occupied"] = (
            0,
            half - 1,
            half,
            half + 1,
            item["limit"],
            0x7FFFFFFF,
            -0x80000000,
        )[number // 8 % 7]
        item["position"] = rng.choice(
            (0x640000, 0x640070, 0x6400F8, rng.getrandbits(32))
        )
    elif kind == 1:
        for category in range(7):
            maximum = CAPACITIES[category]
            fill(item["blocks"][3], category, number % (maximum + 1))
            fill(item["blocks"][1], category, maximum - number % (maximum + 1))
        if number % 3 == 0:
            item["inventory_limits"][number % 7] = rng.choice((128, 255))
        item["callback"] = number % 5 == 0
    elif kind in (2, 3):
        category = 0 if kind == 2 else 3
        item["slot"] = (0, CAPACITIES[category] - 1)[number % 2]
        slots = number // 2 % 5
        attach(item["blocks"][0], category, item["slot"], rng)
        if kind == 2:
            patch(item["blocks"][0], OFFSETS[2] + 63 * STRIDES[2] + 4, slots)
            start = item["slot"] * STRIDES[0] + 4
        else:
            patch(item["blocks"][0], OFFSETS[3] + item["slot"] * STRIDES[3] + 16, slots)
            start = OFFSETS[3] + item["slot"] * STRIDES[3] + 44
        for offset in range(4):
            patch(
                item["blocks"][0], start + offset, rng.choice((0, 0, 1, 128, 192, 255))
            )
    elif kind == 4:
        projection(item, rng, number)
    elif kind == 5:
        capacity_error(item, rng, number)
    else:
        staging(item, rng, number, kind == 6)
    return item


def string_at(machine, address: int) -> bytes:
    """Read the actual original compact-font ASCII operand through its terminator."""
    result = bytearray()
    for offset in range(256):
        value = machine.load(address + offset, 1)
        if value == 0:
            return bytes(result)
        result.append(value)
    raise ValueError(f"Unterminated original text at {address:#x}")


def render_stubs(machine, item: dict, calls: list[list[int]]) -> None:
    """Observe renderer operands only; original capacity math and all item helpers run."""
    number_calls = 0

    def number(current) -> None:
        """Record integer/position/color and apply the explicit external callback vector."""
        nonlocal number_calls
        calls.append([1, s32(current.r[4]), current.r[5], current.r[6], 0])
        number_calls += 1
        if item["callback"] and number_calls == 1:
            current.put(INVENTORY_LIMIT + 6, 201, 1)
        current.r[2] = current.r[5]

    def label(current) -> None:
        """Record the complete actual ASCII operand, including every space glyph."""
        raw = string_at(current, current.r[4])
        color = current.r[6] if current.r[31] == 0x801064C0 else 0x808080
        calls.append([0, 0, current.r[5], color, len(raw), *raw])
        current.r[2] = 0

    machine.stub(0x800FFD64, number)
    machine.stub(0x800C6540, label)
    machine.stub(0x800C6828, label)


def run(kind: int, machine, item: dict) -> tuple[dict, list[int]]:
    """Execute the original entry with real count/copy/clear/copy-zero callees."""
    before = [
        materialize(specification, BLOCK_BYTES) for specification in item["blocks"]
    ]
    for address, raw in zip(ADDRESSES, before, strict=True):
        machine.store(address, raw)
    for index, value in enumerate(item["caps"]):
        machine.put(CAPACITY + index * 2, value, 2)
    for index, value in enumerate(item["inventory_limits"]):
        machine.put(INVENTORY_LIMIT + index, value, 1)
    machine.put(STORAGE_POINTER, STORAGE)
    machine.put(ACTIVE_POINTER, ADDRESSES[2] if item["alias"] == 2 else ADDRESSES[0])
    receiver = (
        ADDRESSES[0]
        if item["alias"] == 1
        else ADDRESSES[2]
        if item["alias"] == 3
        else ADDRESSES[1]
    )
    machine.put(OTHER_POINTER, receiver)
    machine.put(DIRECTION, item["direction"], 1)
    for row, token in enumerate(item["pending"]):
        machine.put(PENDING + row * 2, token, 2)
    machine.follow.update(
        (
            0x80102C94,
            0x80102CD0,
            0x80102D28,
            0x80102D80,
            0x80102F88,
            0x80103070,
            0x80103110,
            0x801032AC,
            *(entry for entry, _ in RANGES),
            0x800CCC1C,
            0x800CCC34,
            0x800C01C0,
            0x800C01E0,
        )
    )
    calls = []
    render_stubs(machine, item, calls)
    if kind == 0:
        machine.r[4:7] = [
            item["limit"] & 0xFFFFFFFF,
            item["position"],
            item["occupied"] & 0xFFFFFFFF,
        ]
    elif kind in (2, 3):
        machine.r[4] = item["slot"]
    elif kind == 4:
        machine.r[4:6] = [item["category"], item["additional"] & 0xFFFFFFFF]
    elif kind in (5, 7):
        machine.r[4:6] = [item["category"], item["slot"]]
    elif kind == 6:
        machine.r[4] = item["row"]
    machine.execute(RANGES[kind][0])
    result = s32(machine.r[2]) if kind in (2, 3, 4, 5) else 0
    changes = [
        (block_index, offset, value)
        for block_index, (address, raw) in enumerate(
            zip(ADDRESSES, before, strict=True)
        )
        for offset, value in enumerate(machine.bytes(address, len(raw)))
        if value != raw[offset]
    ]
    pending_after = [machine.load(PENDING + row * 2, 2) for row in range(64)]
    allowed = [(DEFAULT_STACK - 0x4000, DEFAULT_STACK)]
    if kind in (6, 7):
        allowed += [(address, address + BLOCK_BYTES) for address in ADDRESSES]
    expected = [
        result,
        len(calls),
        *(value for call in calls for value in call),
        len(changes),
        *(value for change in changes for value in change),
        *pending_after,
    ]
    inputs = [
        kind,
        item["category"],
        item["slot"],
        item["row"],
        item["additional"],
        item["limit"],
        item["position"],
        item["occupied"],
        item["direction"],
        item["alias"],
        int(item["callback"]),
        *item["caps"],
        *item["inventory_limits"],
    ]
    for specification in item["blocks"]:
        inputs += [specification["fill"], len(specification["patches"])]
        inputs += [value for change in specification["patches"] for value in change]
    inputs += item["pending"]
    return {
        "expected": expected,
        "stores": machine.stores,
        "allowed": allowed,
        "calls": calls,
        "result": result,
    }, inputs


def check(image, item, observation: dict, native: list[int], reference) -> list[str]:
    """Compare every renderer operand, complete owner byte deltas and all 64 tokens."""
    expected = observation["expected"]
    problems = []
    if native != expected:
        first = next(
            (
                index
                for index, (ours, theirs) in enumerate(
                    zip(native, expected, strict=False)
                )
                if ours != theirs
            ),
            min(len(native), len(expected)),
        )
        problems.append(
            f"complete output differs at {first}: native={native[first : first + 5]} "
            f"original={expected[first : first + 5]}"
        )
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(kind: int, image, results: list) -> list[str]:
    """State the exact original-helper coverage and renderer-boundary limitation."""
    callees = (
        "Own 40 original instructions; the three integer/slash renderer calls are "
        "observation seams. No item or SDK math result is substituted.",
        "Own 72 original instructions plus actual F02971 and MENUD Count/ItemId "
        "instructions; only final renderer operands are observed at a seam.",
        "Own 24 original instructions read the actual weapon, required grip and "
        "unsigned gem-slot/reference fields; this entry has no original callees.",
        "Own 22 original instructions read the actual shield, unsigned gem-slot "
        "extent and reference bytes; this entry has no original callees.",
        "Own 42 original instructions plus actual MENUD Count/ItemId bodies for "
        "both receiving and staged owners; no owner/count/capacity stubs.",
        "Own 78 original instructions plus actual weapon/shield gem counts, "
        "receiving-capacity, Count and ItemId bodies; no owner/result stubs.",
        "Own 35 original instructions plus actual recursive MENUD Copy/Clear, "
        "Count/ItemId/FindEmpty and BATTLE memory copy/zero bodies.",
        "Own 26 original instructions plus actual recursive MENUD Copy/Clear, "
        "Count/ItemId/FindEmpty and BATTLE memory copy/zero bodies.",
    )
    coverage = (
        "Strict half/equality boundaries, signed doubling wrap, packed-coordinate "
        "borrow and exact draw order/value/color/text operands.",
        "Both actual owners and all seven physical category extents, fixed labels "
        "including the space glyph, byte limits and late external render callback.",
        "First/last weapon slots, required grip slot 64, unsigned references and "
        "zero through four available attachment slots.",
        "First/last shield slots, unsigned reference bytes and zero through four "
        "available attachment slots.",
        "All seven physical categories plus manage, both direction choices, receiving "
        "and staging counts, exact equality, signed wrap and repeated-owner aliases.",
        "All seven item categories, actual per-component thresholds, zero through "
        "four attachment slots and every pending prefix around 59 through 64.",
        "All seven record categories, selected pending rows through 63, 64/192/256 "
        "record extents, exact source/staging aliases and unconditional post-copy clear.",
        "All seven record categories, full 64/192/256 extents, mandatory/optional "
        "components, exact source/staging aliases and unconditional post-copy clear.",
    )
    return [
        callees[kind],
        coverage[kind],
        "Compared complete scalar/render output, all supplied record-owner byte deltas "
        "and all 64 pending halfwords; original stores bounded to the actual owners/stack.",
        "Renderer operand comparisons do not claim original glyph raster parity. Actual "
        "production compact-font/layout/palette/OT and ordinary reset tests are separate; "
        "new central module-binding cases compile only until coordinator integration.",
        f"Observed scalar results: {sorted({observation['result'] for _, observation, _ in results})}",
    ]
