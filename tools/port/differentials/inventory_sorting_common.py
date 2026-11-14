"""Inventory sorting uses both original overlays and real inventory/part/stat instructions."""

import functools

from differential import DEFAULT_STACK, MODULE_BASES, s32, store_problems
from differentials.sorting_vectors import STATS as STATS
from differentials.sorting_vectors import SUPPORTED as SUPPORTED
from differentials.sorting_vectors import (
    populate_sort_order,
    projection_index,
    sort_category,
    sort_stat,
)

MODULES = (r"MENU\MENU3.PRG", r"BATTLE\BATTLE.PRG", "SLUS_010.40", r"MENU\MAINMENU.PRG")
MODULE_BASES.update(
    {
        MODULES[0]: 0x80102800,
        r"MENU\MENUB.PRG": 0x80102800,
        MODULES[1]: 0x80068800,
        MODULES[3]: 0x800F9800,
    }
)
IDS = ("F02583", "F02585", "F02587", "F02589", "F02590")
RANGES = (
    (0x80103CC8, 0x80103E24),
    (0x80103F00, 0x8010406C),
    (0x8010408C, 0x801041CC),
    (0x801042C4, 0x8010439C),
    (0x8010439C, 0x80104530),
)
COMPARATORS = (0x80103BE4, 0x80103E24, 0x8010406C, 0, 0x801041CC)
CAPACITIES = (8, 16, 16, 8, 16, 48, 64, 2)
OFFSETS = (0, 0x280, 0x540, 0x100, 0x640, 0x8C0, 0xE00)
STRIDES = (32, 44, 16, 48, 40, 28, 4)
ORDER_OFFSETS = (0, 8, 24, 40, 48, 64, 112)
INVENTORY, ORDER = 0x80060168, 0x800619D8
MEMORY_BYTES, ORDER_ENTRIES = 0xF00, 176
CAPACITY, ROUTES = 0x801022A0, 0x801022A8
TARGET, TEMP = 0x80800000, 0x80900000
SOURCES = (
    r"Native\Game\src\Menu\Inventory\InventorySorting.cpp",
    r"Native\Game\src\Items\EquipmentStats.cpp",
    r"Native\Game\src\Items\Inventory.cpp",
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
        "stat-argument": (0x80103CD0, 0x0000F021),
        "skip-populated": (0x80103D34, 0x1000000D),
        "weapon-stride": (0x80103D3C, 0x00102980),
        "signed-maximum": (0x80103D5C, 0x0263102B),
        "last-slot-scan": (0x80103D70, 0x2A220007),
        "output-token": (0x80103DC4, 0xA0400000),
        "copy-length": (0x80103DF0, 0x24060004),
    },
    {
        "stat-argument": (0x80103F08, 0x0000F021),
        "skip-populated": (0x80103F6C, 0x1000000F),
        "shield-stride": (0x80103F7C, 0x00052940),
        "signed-maximum": (0x80103F9C, 0x0263102B),
        "last-slot-scan": (0x80103FB0, 0x2A220007),
        "output-token": (0x8010400C, 0xA0400000),
        "copy-length": (0x80104038, 0x24060004),
    },
    {
        "stat-argument": (0x80104094, 0x0000F021),
        "skip-populated": (0x801040F4, 0x1000000A),
        "misc-stride": (0x801040FC, 0x00102840),
        "signed-maximum": (0x80104110, 0x0263102B),
        "last-slot-scan": (0x80104124, 0x2A22003F),
        "output-token": (0x8010416C, 0xA0400000),
        "copy-length": (0x80104198, 0x2406003C),
    },
    {
        "blade-call": (0x80104324, 0),
        "grip-call": (0x80104340, 0),
        "armor-call": (0x80104364, 0),
        "gem-call": (0x80104384, 0),
        "blade-offset": (0x80104320, 0x244203EC),
        "grip-stride": (0x80104334, 0x000610C0),
        "armor-offset": (0x80104360, 0x24A507AC),
        "gem-offset": (0x80104380, 0x24A50A2C),
    },
    {
        "stat-argument": (0x801043E4, 0xAFC00074),
        "skip-populated": (0x80104434, 0x1000000C),
        "record-index": (0x80104444, 0x26060000),
        "signed-maximum": (0x80104458, 0x0263102B),
        "output-token": (0x801044D0, 0xA0400000),
        "copy-length": (0x801044F8, 0x24060004),
        "capacity-entry": (0x801043E8, 0x90540001),
    },
)


def configuration(kind, shift=0):
    """Publish static metadata only; importing and listing never reads game data."""
    entry, end = (address + shift for address in RANGES[kind])
    return {
        "ID": IDS[kind],
        "ENTRY": entry,
        "END": end,
        "MODULES": (r"MENU\MENUB.PRG", *MODULES[1:]) if shift else MODULES,
        "DRIVER": "inventory_sorting_driver.cpp",
        "SOURCES": SOURCES,
        "INCLUDES": (
            r"Native\Game\src",
            r"Native\Tests\include",
            r"Native\Engine\src",
            r"Native\Content\include",
            r"tools\port\differentials",
        ),
        "VECTORS": 256,
        "MUTANT_VECTORS": 48,
        "SEED": int(IDS[kind][1:]),
        "MUTANTS": {
            name: {address + shift: word}
            for name, (address, word) in MUTATIONS[kind].items()
        },
        "vector": functools.partial(vector, kind),
        "run": functools.partial(run, kind, shift=shift),
        "check": check,
        "describe": describe,
    }


def data(rng):
    """Populate real-width records, with valid part references and tied last records."""
    block = bytearray(rng.randbytes(MEMORY_BYTES))
    for row in range(8):
        weapon = row * 32
        block[weapon] = row + 1
        block[weapon + 1 : weapon + 3] = bytes(rng.choice((0, 1, 16)) for _ in range(2))
        block[weapon + 4 : weapon + 7] = bytes(rng.choice((0, 1, 48)) for _ in range(3))
        shield = 0x100 + row * 48
        block[shield] = row + 1
        block[shield + 44 : shield + 47] = bytes(
            rng.choice((0, 1, 48, 129, 176)) for _ in range(3)
        )
    for category in range(7):
        start, stride = OFFSETS[category], STRIDES[category]
        second, last = start + stride, start + (CAPACITIES[category] - 1) * stride
        block[last : last + stride] = block[second : second + stride]
    block[0xE00:0xE02] = b"\0\0"
    block[0xE04:0xE06] = b"\xff\xff"
    return bytes(block)


def vector(kind, rng, number):
    """Exercise all key families, full byte orders, duplicates, live mutation and aliases."""
    category = sort_category(kind, number)
    stat = sort_stat(category, number)
    capacities = list(CAPACITIES)
    if kind == 4:
        maximum = CAPACITIES[category]
        capacities[category] = (8, 8, maximum, maximum - 8)[number // 4 % 4]
    routes = list(range(7))
    if kind != 3 and number % 8 == 3:
        routes[category] = 6
    captured = routes[category] if kind == 4 else category
    count = capacities[category] if kind != 3 else 0
    order = [rng.randrange(256) for _ in range(ORDER_ENTRIES)]
    if kind != 3:
        first = ORDER_OFFSETS[captured]
        populate_sort_order(order, first, count, CAPACITIES[category], number)
    index = projection_index(CAPACITIES, category, number)
    return {
        "kind": kind,
        "category": category,
        "stat": stat,
        "index": index,
        "mode": number % 5 if kind != 3 else 0,
        "count": count,
        "capacities": capacities,
        "data": [data(rng), data(rng)],
        "order": order,
        "routes": routes,
        "target": rng.randbytes(48),
    }


def checksum(raw):
    """Weight every source byte so unrelated writes cannot disappear behind selected fields."""
    return sum((n + 1) * value for n, value in enumerate(raw)) & 0xFFFFFFFF


def run(kind, machine, item, shift=0):
    """Execute real bodies and helpers; only allocator/free are bounded endpoint doubles."""
    machine.store(INVENTORY, item["data"][0])
    machine.store(ORDER, bytes(item["order"]))
    machine.store(TARGET, item["target"])
    machine.map_zero(TEMP, 0x200)
    for row, value in enumerate(item["capacities"]):
        machine.put(CAPACITY + row, value, 1)
    for row, category in enumerate(item["routes"]):
        machine.put(ROUTES + row * 4, ORDER + ORDER_OFFSETS[category])
    machine.follow.update(
        (
            0x801042C4 + shift,
            0x8006AEAC,
            0x8006B02C,
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
    category = item["category"]
    route = item["routes"][category] if kind == 4 else category
    order_base = ORDER + ORDER_OFFSETS[route] if kind != 3 else ORDER
    trace, version = [], 0

    def key_call(m):
        nonlocal version
        saved, stat = m.r[31], s32(m.r[4])
        m.execute(COMPARATORS[kind] + shift, limit=2_000_000)
        m.r[31] = saved
        hashed = (
            sum((n + 1) * m.load(order_base + n, 1) for n in range(item["count"]))
            & 0xFFFFFFFF
        )
        trace.append([kind, stat, s32(m.r[2]), hashed, version])
        if len(trace) == 1:
            if item["mode"] == 1:
                m.store(INVENTORY, item["data"][1])
                version = 1
            elif item["mode"] == 2:
                m.put(ROUTES + category * 4, ORDER + ORDER_OFFSETS[6])
            elif item["mode"] == 3:
                m.put(CAPACITY + category, 4, 1)
            elif item["mode"] == 4:
                m.put(order_base + item["count"] - 1, 0, 1)

    if kind != 3:
        machine.stub(COMPARATORS[kind] + shift, key_call)
    machine.r[4:7] = [TARGET, category & 0xFFFFFFFF, item["index"]]
    if kind != 3:
        machine.r[4] = category if kind == 4 else item["stat"] & 0xFFFFFFFF
        if kind == 4:
            machine.r[5] = item["stat"] & 0xFFFFFFFF
    machine.execute(RANGES[kind][0] + shift, limit=2_000_000)
    values = [
        version,
        *(machine.load(CAPACITY + row, 1) for row in range(8)),
        checksum(machine.bytes(INVENTORY, MEMORY_BYTES)),
        *machine.bytes(TARGET, 48),
        *machine.bytes(ORDER, ORDER_ENTRIES),
        *(
            ORDER_OFFSETS.index(machine.load(ROUTES + row * 4) - ORDER)
            for row in range(7)
        ),
        len(trace),
        *(value for call in trace for value in call),
    ]
    allowed = [
        (DEFAULT_STACK - 0x1000, DEFAULT_STACK + 0x10),
        (TEMP, TEMP + 0x10C),
        (INVENTORY, INVENTORY + MEMORY_BYTES),
        (ORDER, ORDER + ORDER_ENTRIES),
        (TARGET, TARGET + 48),
        (CAPACITY, CAPACITY + 8),
        (ROUTES, ROUTES + 28),
    ]
    inputs = [
        kind,
        category,
        item["stat"],
        item["index"],
        item["mode"],
        item["count"],
        *item["capacities"],
        *item["routes"],
        *(value for memory in item["data"] for value in memory),
        *item["order"],
        *item["target"],
    ]
    return {"values": values, "stores": machine.stores, "allowed": allowed}, inputs


def check(image, item, observation, native, reference):
    """Compare every order, source checksum, output byte, route, capacity and key event."""
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
            f"full inventory/orders/output/capacities/routes/key trace differ "
            f"at {first}; lengths {len(native)}/{len(observation['values'])}"
        )
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(image, results):
    """State the exact original/native domain, endpoint doubles and non-gameplay limits."""
    return [
        "Real MENU3/MENUB key functions, BATTLE inventory/loadout/part math and original copy/zero "
        "instructions execute in the unchanged maintained shared R3000 interpreter.",
        "Allocator/free alone are bounded synthetic endpoints; callback observation runs "
        "the original key instructions before controlled source/order/route/capacity changes.",
        "Inventory is a fixed global object, not a replaceable owner pointer. Every projection "
        "rereads it. Fixed 8/8/64 byte orders differ from container 32/32/256 halfword orders.",
        "Independent F02589 stack output matches both actual callers. Unsupported categories "
        "are no-ops; no arbitrary foreign byte-overlap or global reader-bridge parity claimed.",
        "All complete byte orders, output bytes, source checksum, pointer routes, capacities "
        "and ordered live key/stat/value/mutation observations are compared.",
        "Synthetic callbacks/capacity changes are explicit ordering controls, not physical "
        "menu execution, actual allocator behavior, gameplay, EC retirement or M8 parity.",
        "Prior container same-address raw215/256 FAIL remains explicit and unchanged.",
        "Original scratch helpers require positive eight-byte copy groups. The rejected "
        "four-byte synthetic probe is retained; zero/small capacity tests are native-policy "
        "controls, not original-defined-domain passes or skipped actual caller cases.",
    ]
