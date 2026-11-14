"""Container sorting: original MENUD and real BATTLE/stat/memory instructions."""

import functools
import struct

from differential import DEFAULT_STACK, MODULE_BASES, s32, store_problems
from differentials.sorting_vectors import STATS as STATS
from differentials.sorting_vectors import SUPPORTED as SUPPORTED
from differentials.sorting_vectors import (
    populate_sort_order,
    projection_index,
    sort_category,
    sort_stat,
)

MODULES = (r"MENU\MENUD.PRG", r"BATTLE\BATTLE.PRG", "SLUS_010.40")
MODULE_BASES.update({MODULES[0]: 0x80102800, MODULES[1]: 0x80068800})
IDS = ("F02961", "F02963", "F02965", "F02967", "F02968")
RANGES = (
    (0x801051F8, 0x80105378),
    (0x80105454, 0x801055D0),
    (0x801055F0, 0x8010574C),
    (0x80105844, 0x8010592C),
    (0x8010592C, 0x80105ACC),
)
COMPARATORS = (0x80105114, 0x80105378, 0x801055D0, 0, 0x8010574C)
CAPACITIES = (32, 64, 64, 32, 64, 192, 256, 2)
OFFSETS = (0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300, 0x3800)
STRIDES = (32, 44, 16, 48, 40, 28, 4)
OWNERS, MEMORY_BYTES, ORDER_ENTRIES = (0x80600000, 0x80610000), 0x3C00, 704
TARGET, TEMP = 0x80800000, 0x80900000
CURRENT, CAPACITY = 0x80109A8C, 0x80109944
SOURCES = (
    r"Native\Game\src\Menu\Container\ContainerSorting.cpp",
    r"Native\Game\src\Menu\Container\ContainerDetailHelpers.cpp",
    r"Native\Game\src\Items\ContainerIndexing.cpp",
    r"Native\Game\src\Items\EquipmentStats.cpp",
    r"Native\Game\src\Menu\Framework\Framework.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Render\GameFont.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
    r"Native\Engine\src\Render\UiPackets.cpp",
    r"Native\Engine\src\Render\EncodedText.cpp",
)
MUTATIONS = (
    {
        "stat-argument": (0x801051FC, 0xAFA00218),
        "skip-populated": (0x80105270, 0x1000000E),
        "weapon-stride": (0x80105278, 0x00102900),
        "signed-maximum": (0x8010529C, 0x0283102B),
        "last-slot-scan": (0x801052B0, 0x2A22001F),
        "output-token": (0x80105310, 0xA6600000),
        "copy-length": (0x80105344, 0x2406003C),
    },
    {
        "stat-argument": (0x8010545C, 0x0000F021),
        "skip-populated": (0x801054C4, 0x10000010),
        "shield-stride": (0x801054D8, 0x000528C0),
        "signed-maximum": (0x801054F8, 0x0263102B),
        "last-slot-scan": (0x8010550C, 0x2A22001F),
        "output-token": (0x80105570, 0xA4400178),
        "copy-length": (0x8010559C, 0x2406003C),
    },
    {
        "stat-argument": (0x801055F8, 0x0000F021),
        "skip-populated": (0x80105664, 0x1000000A),
        "misc-stride": (0x8010566C, 0x00102840),
        "signed-maximum": (0x80105680, 0x0283102B),
        "last-slot-scan": (0x80105694, 0x2A2200FF),
        "output-token": (0x801056E4, 0xA6600000),
        "copy-length": (0x80105718, 0x240601FC),
    },
    {
        "blade-call": (0x801058A8, 0),
        "grip-call": (0x801058C8, 0),
        "armor-call": (0x801058F0, 0),
        "gem-call": (0x80105914, 0),
        "blade-offset": (0x801058A4, 0x24A50A04),
        "grip-stride": (0x801058BC, 0x000628C0),
        "armor-offset": (0x801058EC, 0x24A51908),
        "gem-offset": (0x80105910, 0x24A52304),
    },
    {
        "stat-argument": (0x8010597C, 0xAFC00074),
        "skip-populated": (0x801059CC, 0x1000000C),
        "record-index": (0x801059DC, 0x26060000),
        "signed-maximum": (0x801059F0, 0x0283102B),
        "output-token": (0x80105A64, 0xA6600000),
        "copy-length": (0x80105A94, 0x00153000),
        "capacity-entry": (0x80105974, 0x94750002),
    },
)


def configuration(kind):
    entry, end = RANGES[kind]
    return {
        "ID": IDS[kind],
        "ENTRY": entry,
        "END": end,
        "MODULES": MODULES,
        "DRIVER": "container_sorting_driver.cpp",
        "SOURCES": SOURCES,
        "INCLUDES": (
            r"Native\Game\src",
            r"Native\Tests\include",
            r"Native\Engine\src",
            r"Native\Content\include",
        ),
        "VECTORS": 256,
        "MUTANT_VECTORS": 48,
        "SEED": int(IDS[kind][1:]),
        "MUTANTS": {
            name: {address: word} for name, (address, word) in MUTATIONS[kind].items()
        },
        "vector": functools.partial(vector, kind),
        "run": functools.partial(run, kind),
        "check": check,
        "describe": describe,
    }


def data(rng):
    block = bytearray(rng.randbytes(MEMORY_BYTES))
    for row in range(32):
        weapon = row * 32
        block[weapon] = row + 1
        block[weapon + 1 : weapon + 3] = bytes(rng.choice((0, 1, 64)) for _ in range(2))
        block[weapon + 4 : weapon + 7] = bytes(
            rng.choice((0, 1, 192)) for _ in range(3)
        )
        shield = 0x400 + row * 48
        block[shield] = row + 1
        block[shield + 44 : shield + 47] = bytes(
            rng.choice((0, 1, 127, 129, 255)) for _ in range(3)
        )
    for category in range(7):
        start, stride = OFFSETS[category], STRIDES[category]
        second, last = start + stride, start + (CAPACITIES[category] - 1) * stride
        block[last : last + stride] = block[second : second + stride]
    return bytes(block)


def vector(kind, rng, number):
    category = sort_category(kind, number)
    stat = sort_stat(category, number)
    capacities = list(CAPACITIES)
    if kind == 4:
        maximum = CAPACITIES[category]
        # Original zero/copy scratch helpers require positive four-token groups.
        # Empty item orders still run against the real, nonzero capacity.
        capacities[category] = (
            4,
            8,
            12,
            maximum,
            maximum - 4,
            rng.randrange(1, maximum // 4 + 1) * 4,
        )[number // 4 % 6]
    count = (
        capacities[category] if kind == 4 else CAPACITIES[category] if kind != 3 else 0
    )
    order = [rng.randrange(65536) for _ in range(ORDER_ENTRIES)]
    if kind != 3:
        first = sum(capacities[:category])
        populate_sort_order(order, first, count, CAPACITIES[category], number)
    index = projection_index(CAPACITIES, category, number)
    return {
        "kind": kind,
        "category": category,
        "stat": stat,
        "index": index,
        "mode": number % 4 if kind != 3 else 0,
        "count": count,
        "capacities": capacities,
        "data": [data(rng), data(rng)],
        "order": [order, [rng.randrange(65536) for _ in range(ORDER_ENTRIES)]],
        "target": rng.randbytes(48),
    }


def checksum(raw):
    return sum((n + 1) * value for n, value in enumerate(raw)) & 0xFFFFFFFF


def run(kind, machine, item):
    for owner, memory, order in zip(OWNERS, item["data"], item["order"], strict=True):
        machine.store(owner, memory)
        machine.store(owner + MEMORY_BYTES, struct.pack("<704H", *order))
    machine.store(TARGET, item["target"])
    machine.map_zero(TEMP, 0x200)
    machine.put(CURRENT, OWNERS[0])
    for row, value in enumerate(item["capacities"]):
        machine.put(CAPACITY + row * 2, value, 2)
    machine.follow.update(
        (
            0x80102A34,
            0x80102BB0,
            0x80105844,
            0x8006A334,
            0x8006A65C,
            0x8006AB44,
            0x8006AC74,
            0x8006ACFC,
            0x8006AE0C,
            0x800490B0,
            0x80049110,
            0x800CCC1C,
            0x800C01E0,
        )
    )
    machine.stub(0x80043E3C, lambda m: m.r.__setitem__(2, TEMP))
    machine.stub(0x80043C60, lambda m: None)
    trace, order_base = [], OWNERS[0] + MEMORY_BYTES

    def actual(m, entry):
        saved = m.r[31]
        m.execute(entry, limit=2_000_000)
        m.r[31] = saved

    def order_call(m):
        nonlocal order_base
        actual(m, 0x80102C94)
        order_base = m.r[2]
        if item["mode"] == 2:
            m.put(CURRENT, OWNERS[1])
        if item["mode"] == 3:
            m.put(CAPACITY + item["category"] * 2, 1, 2)

    def key_call(m):
        stat = s32(m.r[4])
        actual(m, COMPARATORS[kind])
        hashed = (
            sum((n + 1) * m.load(order_base + n * 2, 2) for n in range(item["count"]))
            & 0xFFFFFFFF
        )
        owner = OWNERS.index(m.load(CURRENT))
        trace.append([kind, stat, s32(m.r[2]), hashed, owner])
        if item["mode"] == 1 and len(trace) == 1:
            m.put(CURRENT, OWNERS[1])

    if kind != 3:
        machine.stub(0x80102C94, order_call)
        machine.stub(COMPARATORS[kind], key_call)
    machine.r[4:7] = [TARGET, item["category"] & 0xFFFFFFFF, item["index"]]
    if kind != 3:
        machine.r[4] = item["category"] if kind == 4 else item["stat"] & 0xFFFFFFFF
        if kind == 4:
            machine.r[5] = item["stat"] & 0xFFFFFFFF
    machine.execute(RANGES[kind][0], limit=2_000_000)
    orders = [
        [
            machine.load(owner + MEMORY_BYTES + row * 2, 2)
            for row in range(ORDER_ENTRIES)
        ]
        for owner in OWNERS
    ]
    values = [
        OWNERS.index(machine.load(CURRENT)),
        *(machine.load(CAPACITY + n * 2, 2) for n in range(8)),
        *(checksum(machine.bytes(owner, MEMORY_BYTES)) for owner in OWNERS),
        *machine.bytes(TARGET, 48),
        *orders[0],
        *orders[1],
        len(trace),
        *(value for call in trace for value in call),
    ]
    allowed = [(DEFAULT_STACK - 0x1000, DEFAULT_STACK + 0x10), (TEMP, TEMP + 0x10C)]
    allowed += (
        [(TARGET, TARGET + 48)]
        if kind == 3
        else [
            (owner + MEMORY_BYTES, owner + MEMORY_BYTES + ORDER_ENTRIES * 2)
            for owner in OWNERS
        ]
    )
    observation = {"values": values, "stores": machine.stores, "allowed": allowed}
    inputs = [
        kind,
        item["category"],
        item["stat"],
        item["index"],
        item["mode"],
        item["count"],
        *item["capacities"],
        *(
            value
            for memory, order in zip(item["data"], item["order"], strict=True)
            for value in (*memory, *order)
        ),
        *item["target"],
    ]
    return observation, inputs


def check(image, item, observation, native, reference):
    problems = []
    if native != observation["values"]:
        first = next(
            (
                n
                for n, pair in enumerate(
                    zip(native, observation["values"], strict=False)
                )
                if pair[0] != pair[1]
            ),
            None,
        )
        problems.append(
            f"complete owners/capacities/records/output/orders/key trace differ at {first}; "
            f"lengths {len(native)}/{len(observation['values'])}"
        )
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(image, results):
    return [
        "Defined typed production domain: F02967 independent output and current container "
        "record; static caller/domain audit covers both F02968 stack-output calls and 89 images.",
        "Original comparator and offset callbacks execute their actual instructions on the same "
        "shared Machine, restoring only the caller return address; no key/offset math is stubbed.",
        "Real BATTLE part/loadout calculations and SLUS/BATTLE copy/zero instructions followed. "
        "Only high-end temporary allocation/free are bounded scratch services.",
        "Compared complete 704-halfword orders for both owners, all 48 output bytes, all capacities, "
        "full-record checksums, and ordered key/stat/value/order-mutation/current-owner observations.",
        "Zero/single/full/sentinel tails, duplicate tokens/equal-key records, first/last rows, "
        "signed bytes/halfwords, folded key families, capacity capture and current-owner callbacks.",
        "Preserved raw byte-overlap oracle still has 215/256 FAIL; it is not passed, removed or weakened.",
        "Zero/small non-four-multiple synthetic capacities are separate native checked-policy "
        "cases, not original-defined-domain passes: the actual scratch helpers do not return "
        "normally for those byte lengths. Their initial failing probes remain preserved.",
    ]
