"""Original MENUD container indexing/records, using the repository R3000 interpreter."""

import functools
import random

from differential import DEFAULT_STACK, MODULE_BASES, s32, store_problems

MODULES = (r"MENU\MENUD.PRG", r"BATTLE\BATTLE.PRG")
MODULE_BASES.update({MODULES[0]: 0x80102800, MODULES[1]: 0x80068800})
DRIVER = "container_indexing_driver.cpp"
SOURCES = (r"Native\Game\src\Items\ContainerIndexing.cpp",)
CAPACITIES = (32, 64, 64, 32, 64, 192, 256, 2)
OFFSETS = (0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300, 0x3800)
STRIDES = (32, 44, 16, 48, 40, 28, 4)
ID_OFFSETS = (1, 0, 0, 4, 0, 0, 0)
ID_WIDTHS = (1, 1, 2, 1, 1, 2, 2)
MEMORY, CAPACITY, ORDER = 0x80600000, 0x80109944, 0x80700000
MEMORY_BYTES = 0x3C00
RANGES = (
    (0x80102C94, 0x80102CD0), (0x80102CD0, 0x80102D28),
    (0x80102D28, 0x80102D80), (0x80102D80, 0x80102F88),
    (0x80102F88, 0x80103070), (0x80103070, 0x80103110),
    (0x80103110, 0x801031A0),
)
MUTATIONS = (
    {
        "prefix-load": (0x80102CA8, 0x94620002),
        "prefix-add": (0x80102CB0, 0x00E23823),
        "index-element-width": (0x80102CC0, 0x00071080),
        "indices-member-offset": (0x80102CC4, 0x24423BFC),
    },
    {
        "capacity-entry": (0x80102CE0, 0x94820002),
        "one-based-needle": (0x80102CF0, 0x24A50000),
        "halfword-needle": (0x80102CF8, 0x90C20000),
        "match-branch": (0x80102D00, 0x14450007),
        "one-based-row": (0x80102D04, 0x24620000),
        "row-increment": (0x80102D08, 0x24630002),
        "order-stride": (0x80102D14, 0x24C60004),
        "absent-sentinel": (0x80102D1C, 0x24020001),
    },
    {
        "capacity-entry": (0x80102D38, 0x94820002),
        "one-based-token": (0x80102D48, 0x24A50000),
        "vacancy-branch": (0x80102D54, 0x10400003),
        "halfword-store": (0x80102D60, 0xA0C50000),
        "store-value": (0x80102D60, 0xA4C00000),
        "row-increment": (0x80102D58, 0x24630002),
        "order-stride": (0x80102D74, 0x24C60004),
    },
    {
        "weapon-blade-reference": (0x80102DE4, 0x24A50000),
        "weapon-grip-reference": (0x80102DF8, 0x24A50000),
        "weapon-gem-count": (0x80102E20, 0x2A020002),
        "weapon-gem-zero": (0x80102E08, 0x14A00004),
        "weapon-size": (0x80102E34, 0x2405001C),
        "blade-stride": (0x80102E48, 0x00108040),
        "blade-size": (0x80102E5C, 0x24050028),
        "blade-index": (0x80102E68, 0xA200002B),
        "grip-size": (0x80102E80, 0x2405000C),
        "grip-index": (0x80102E8C, 0xA600000E),
        "shield-gem-reference": (0x80102EBC, 0x24A50000),
        "shield-gem-count": (0x80102ECC, 0x2A020002),
        "shield-size": (0x80102EDC, 0x2405002C),
        "parent-index": (0x80102EF0, 0xA2200000),
        "armor-size": (0x80102F10, 0x24050024),
        "armor-index": (0x80102F1C, 0xA2000027),
        "gem-size": (0x80102F3C, 0x24050018),
        "gem-index": (0x80102F48, 0xA600001A),
        "misc-size": (0x80102F60, 0x24050008),
        "misc-index": (0x80102F68, 0xA2000003),
    },
    {
        "last-category": (0x80102F88, 0x2C820006),
        "weapon-field": (0x80102FBC, 0x90470002),
        "unsigned-blade": (0x80102FE0, 0x80470A00),
        "blade-stride": (0x80102FD8, 0x00021040),
        "unsigned-grip": (0x80102FF4, 0x84471500),
        "grip-stride": (0x80102FEC, 0x000510C0),
        "shield-base": (0x80103010, 0x90470400),
        "unsigned-armor": (0x8010302C, 0x80471900),
        "armor-stride": (0x80103024, 0x00021080),
        "unsigned-gem": (0x80103048, 0x84472300),
        "gem-stride": (0x80103040, 0x00021040),
        "unsigned-misc": (0x8010305C, 0x84473800),
        "manage-zero": (0x80102F90, 0x24070001),
    },
    {
        "capacity-entry": (0x801030A8, 0x94620002),
        "occupancy-call": (0x801030C4, 0),
        "occupied-branch": (0x801030CC, 0x14400002),
        "count-increment": (0x801030D4, 0x26310002),
        "slot-increment": (0x801030DC, 0x26100002),
        "count-return": (0x801030EC, 0x02001021),
    },
    {
        "capacity-entry": (0x80103140, 0x94620002),
        "occupancy-call": (0x8010315C, 0),
        "empty-branch": (0x80103164, 0x14400007),
        "slot-increment": (0x80103170, 0x26100002),
        "empty-result": (0x80103168, 0x26020001),
        "full-sentinel": (0x80103180, 0x00001021),
    },
)


def configuration(kind):
    entry, end = RANGES[kind]
    return {
        "ID": f"F{2927 + kind:05}", "ENTRY": entry, "END": end,
        "MODULES": MODULES, "DRIVER": DRIVER, "SOURCES": SOURCES,
        "VECTORS": 1024, "SEED": 2927 + kind,
        "MUTANTS": {name: {address: word} for name, (address, word) in MUTATIONS[kind].items()},
        "vector": functools.partial(vector, kind), "run": functools.partial(run, kind),
        "check": check, "describe": describe,
    }


def vector(kind, rng: random.Random, number):
    category = number % 8
    caps = [
        rng.choice((0, 1, maximum, rng.randrange(maximum + 1)))
        for maximum in CAPACITIES
    ]
    if number % 3:
        caps = list(CAPACITIES)
    index = rng.choice((0, CAPACITIES[category] - 1, rng.randrange(CAPACITIES[category])))
    patches = []
    fill = rng.randrange(1, 256)
    order = []
    if kind in (1, 2):
        index = rng.choice((-1, 0, 255, 256, 65534, 65535, 0x7FFFFFFF, -0x80000000,
                            s32(rng.getrandbits(32))))
        order = [rng.choice((0, rng.randrange(1, 65536))) for _ in range(CAPACITIES[category])]
        if number % 5 == 0:
            order = [1] * CAPACITIES[category]
        if number % 5 == 1:
            order = [0] * CAPACITIES[category]
        if number % 5 == 2:
            order = [1] * CAPACITIES[category]
            order[-1] = 0 if kind == 2 else (index + 1) & 0xFFFF
        if number % 5 == 3:
            order[0] = (index + 1) & 0xFFFF
    if kind == 3 and category in (0, 3):
        start = OFFSETS[category] + index * STRIDES[category]
        if category == 0:
            patches += [(start + 1, rng.choice((1, 64, rng.randrange(1, 65))), 1),
                        (start + 2, rng.choice((1, 64, rng.randrange(1, 65))), 1)]
        gems = [rng.choice((0, 1, 128, 192, rng.randrange(1, 193))) for _ in range(4)]
        if number % 4 == 0:
            gems[:3] = [192, 192, 1]
        patches += [(start + (4 if category == 0 else 44) + n, value, 1)
                    for n, value in enumerate(gems)]
    if kind == 4 and category < 7:
        width = ID_WIDTHS[category]
        value = rng.choice((0, 1, 255, 128, 32768, 65535, rng.getrandbits(width * 8)))
        patches = [(OFFSETS[category] + index * STRIDES[category] + ID_OFFSETS[category], value, width)]
    if kind in (5, 6) and category < 7:
        for slot in range(CAPACITIES[category]):
            value = rng.choice((0, 0, 1, 255, 65535))
            if number % 5 == 0:
                value = 1
            elif number % 5 == 1:
                value = 0
            elif number % 5 == 2:
                value = 0 if slot == CAPACITIES[category] - 1 else 1
            patches.append((OFFSETS[category] + slot * STRIDES[category] + ID_OFFSETS[category],
                            value, ID_WIDTHS[category]))
    return {"category": category, "index": index, "caps": caps,
            "fill": fill, "patches": patches, "order": order}


def run(kind, machine, item):
    category, index = item["category"], item["index"]
    for n, value in enumerate(item["caps"]):
        machine.put(CAPACITY + n * 2, value, 2)
    before = bytearray([item["fill"]]) * MEMORY_BYTES if kind >= 3 else bytearray()
    for offset, value, size in item["patches"]:
        before[offset:offset + size] = (value & ((1 << (size * 8)) - 1)).to_bytes(size, "little")
    if before:
        machine.store(MEMORY, before)
    for n, value in enumerate(item["order"]):
        machine.put(ORDER + n * 2, value, 2)
    machine.r[4] = category
    machine.r[5] = MEMORY if kind in (0, 5, 6) else index & 0xFFFFFFFF
    machine.r[6] = ORDER if kind in (1, 2) else MEMORY
    machine.follow.update((RANGES[3][0], RANGES[4][0], 0x800CCC1C))
    machine.execute(RANGES[kind][0])
    value = 0 if kind in (2, 3) else machine.r[2]
    if kind == 0:
        value -= MEMORY + MEMORY_BYTES
    order = [machine.load(ORDER + n * 2, 2) for n in range(len(item["order"]))] if kind == 2 else []
    after = machine.bytes(MEMORY, MEMORY_BYTES) if kind == 3 else b""
    changes = [(offset, value) for offset, value in enumerate(after) if value != before[offset]]
    allowed = [(DEFAULT_STACK - 0x4000, DEFAULT_STACK)]
    if kind == 3:
        allowed.append((MEMORY, MEMORY + MEMORY_BYTES))
    if kind == 2:
        allowed.append((ORDER, ORDER + len(order) * 2))
    observation = {"value": value, "order": order, "changes": changes,
                   "stores": machine.stores, "allowed": allowed}
    inputs = [kind, category, index, item["fill"], *item["caps"], len(item["patches"]),
              *(value for patch in item["patches"] for value in patch),
              len(item["order"]), *item["order"]]
    return observation, inputs


def check(image, item, observation, native, reference):
    order_end = 2 + native[1]
    order = native[2:order_end]
    changes = [tuple(native[order_end + 1 + n * 2:order_end + 3 + n * 2])
               for n in range(native[order_end])]
    problems = []
    if native[0] != observation["value"]:
        problems.append(f"result native={native[0]} original={observation['value']}")
    if order != observation["order"]:
        problems.append("complete display order differs")
    if changes != observation["changes"]:
        problems.append("complete container byte changes differ")
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(image, results):
    return [
        "No original callees stubbed: recursive MENUD ClearItem, MENUD ItemId and real BATTLE rMemzero followed.",
        "Compared complete mutated display order/container byte deltas against synthetic pre-state; "
        "all original stores must stay within the supplied table and stack spans.",
        "Biased all eight categories, exact physical limits, variable/zero capacities, first/last rows, "
        "full/empty tables, duplicate/128/192 gem references and signed-word/halfword token boundaries.",
        "Invalid record references are refused separately; original undefined out-of-table reads/writes are not parity inputs.",
    ]
