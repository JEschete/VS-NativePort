"""Original MENUD container detail helpers; shared repository R3000 execution."""

import functools
import random

from differential import DEFAULT_STACK, MODULE_BASES, s32, store_problems

MODULES = (r"MENU\MENUD.PRG", r"BATTLE\BATTLE.PRG", "SLUS_010.40")
MODULE_BASES.update({MODULES[0]: 0x80102800, MODULES[1]: 0x80068800})
DRIVER = "container_detail_helpers_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Container\ContainerDetailHelpers.cpp",
    r"Native\Game\src\Items\ContainerIndexing.cpp",
    r"Native\Game\src\Items\EquipmentStats.cpp",
)
IDS = ("F02925", "F02926", "F02942", "F02943",
       "F02945", "F02947", "F02949", "F02950")
RANGES = ((0x80102A34, 0x80102BB0), (0x80102BB0, 0x80102C94),
          (0x80103D50, 0x80103D6C), (0x80103D6C, 0x80103E24),
          (0x80103F64, 0x80103FD4), (0x80104034, 0x80104078),
          (0x80104114, 0x80104170), (0x80104170, 0x801041E0))
CAPACITIES = (32, 64, 64, 32, 64, 192, 256, 2)
MEMORY, MEMORY_BYTES, SOURCE, TARGET, TEMP = (
    0x80600000, 0x3C00, 0x80700000, 0x80800000, 0x80900000
)
STATE = (0x80109A7B, 0x80109A7C, 0x80109A7D, 0x80109A81, 0x80109A84)
SIZES = (1, 1, 1, 1, 4)
CURSORS, CURSOR_BYTES, CAPACITY, CONTAINER = 0x800F4EE8, 0xF2, 0x80109944, 0x80109A8C
ROWS, ROW_BYTES, ROW_COUNT = (0x80500000, 0x80510000), 64, 40
HELD, REPEAT, PRESSED = 0x8005E238, 0x8005DFDC, 0x8005E1D0
COUNT, INVALID, CLEAR, ROW, CONTINUE, PAGES, PRIMARY, LEAVE, DISMISS, BARS = range(1, 11)
CALLEES = {
    COUNT: 0x80103070, INVALID: 0x800C02E0, CLEAR: 0x800FA8E0,
    ROW: 0x800C8E48, CONTINUE: 0x80103FD4, PAGES: 0x800FBBD4,
    PRIMARY: 0x800FBEA4, LEAVE: 0x800C02E8, DISMISS: 0x800FA810,
    BARS: 0x800FC268,
}
MUTATIONS = (
    {
        "index-store": (0x80102A88, 0),
        "blade-copy": (0x80102AB4, 0),
        "blade-material": (0x80102AE8, 0),
        "grip-copy": (0x80102B08, 0),
        "gem-copy": (0x80102B3C, 0),
        "gem-count": (0x80102B48, 0x2A020002),
        "gem-stride": (0x80102B50, 0x26310000),
        "name-first-byte": (0x80102B60, 0x90420007),
        "name-length": (0x80102B6C, 0x2A020017),
        "apply-stats": (0x80102B7C, 0),
    },
    {
        "shield-index": (0x80102C0C, 0),
        "armor-copy": (0x80102C08, 0),
        "shield-material": (0x80102C18, 0),
        "gem-zero-branch": (0x80102C28, 0x14400008),
        "gem-slot-mask": (0x80102C30, 0x304200FF),
        "gem-copy": (0x80102C44, 0),
        "gem-count": (0x80102C50, 0x2A020002),
        "gem-stride": (0x80102C58, 0x26310000),
        "apply-stats": (0x80102C60, 0),
    },
    {
        "title-store": (0x80103D54, 0),
        "icons-store": (0x80103D5C, 0),
        "controls-delay-store": (0x80103D68, 0),
    },
    {
        "held-mask": (0x80103D88, 0x30420008),
        "count-call": (0x80103D9C, 0),
        "left-mask": (0x80103DB4, 0x30620001),
        "left-step": (0x80103DC0, 0x26020000),
        "right-mask": (0x80103DC8, 0x30620002),
        "right-step": (0x80103DD4, 0x26100002),
        "signed-wrap": (0x80103DD0, 0x0204102B),
        "wrap-subtract": (0x80103DE4, 0x02008021),
        "pressed-mask": (0x80103DF0, 0x30420004),
        "invalid-sound": (0x80103E04, 0),
    },
    {
        "clear-call": (0x80103F74, 0),
        "clear-except": (0x80103F78, 0x24040027),
        "header-row": (0x80103F80, 0x2404001E),
        "header-state": (0x80103F8C, 0x24020002),
        "header-y": (0x80103F94, 0x24020013),
        "selected-state": (0x80103FA4, 0x24020003),
        "selected-x": (0x80103FAC, 0x2402009A),
        "selected-flag": (0x80103FB4, 0x24020000),
        "subtext-clear": (0x80103FBC, 0),
        "selectability-clear": (0x80103FC0, 0),
    },
    {
        "icons-store": (0x80104044, 0),
        "controls-delay-store": (0x80104054, 0),
        "continue-call": (0x80104050, 0),
        "pages-call": (0x80104058, 0),
        "pages-argument": (0x8010405C, 0x00002021),
        "primary-open": (0x80104064, 0x24040002),
    },
    {
        "cursor-category": (0x80104128, 0x2482001E),
        "cursor-stride": (0x8010412C, 0x00021080),
        "cursor-row": (0x80104140, 0),
        "cursor-page": (0x80104144, 0),
        "item-halfword-stride": (0x80104154, 0x00108080),
        "signed-item-token": (0x8010415C, 0x86020000),
    },
    {
        "leave-sound": (0x8010417C, 0),
        "clear-call": (0x80104184, 0),
        "command-row": (0x80104190, 0x24040000),
        "pages-close": (0x80104198, 0x24040000),
        "primary-close": (0x801041A0, 0x24040001),
        "return-phase": (0x801041A8, 0x24020001),
        "phase-store": (0x801041AC, 0),
        "triangle-mask": (0x801041BC, 0x30420020),
        "optional-bars": (0x801041C0, 0x16000003),
        "return-mask-store": (0x801041C4, 0),
        "bars-call": (0x801041C8, 0),
        "bars-command": (0x801041CC, 0x24040000),
    },
)


def configuration(kind):
    entry, end = RANGES[kind]
    return {
        "ID": IDS[kind], "ENTRY": entry, "END": end, "MODULES": MODULES,
        "DRIVER": DRIVER, "SOURCES": SOURCES, "VECTORS": 1024,
        "MUTANT_VECTORS": 128, "SEED": int(IDS[kind][1:]),
        "MUTANTS": {name: {address: word}
                    for name, (address, word) in MUTATIONS[kind].items()},
        "vector": functools.partial(vector, kind), "run": functools.partial(run, kind),
        "check": check, "describe": functools.partial(describe, kind),
    }


def vector(kind, rng: random.Random, number):
    if kind < 2:
        return stats_vector(kind, rng, number)
    count = rng.choice((0, 1, 2, 32, 64, 127, 192, 256))
    category = number % 7
    page = rng.choice((0, count - 1, count, count * 2, -1,
                       0x7FFFFFFF, -0x80000000, s32(rng.getrandbits(32))))
    selected = (0, 31, 39, 20, rng.randrange(40))[number % 5]
    held = rng.getrandbits(32) & ~12 | (number % 4) << 2
    first, second = {
        2: (rng.choice((0, 1, 2, 128, 255, -1)), 0),
        3: (-1 if number % 10 == 0 and held & 12 == 12 else category, page),
        4: (selected, 0), 5: (selected, (3, 4, 7, -1, 0, 255)[number % 6]),
        6: (category, 0), 7: ((0, 1, -1, -2)[number % 4], 0),
    }[kind]
    capacities = list(CAPACITIES)
    if kind == 6 and number % 3 == 0:
        capacities[:category] = [rng.randrange(cap + 1) for cap in CAPACITIES[:category]]
    offset = sum(capacities[:category])
    physical_end = 0
    for capacity in CAPACITIES[:-1]:
        physical_end += capacity
        if physical_end > offset:
            break
    if kind == 6:
        second = rng.choice((0, physical_end-offset-1, rng.randrange(physical_end-offset)))
    return {
        "kind": kind, "args": [first, second], "count": count, "held": held,
        "repeat": rng.getrandbits(32), "pressed": rng.getrandbits(32),
        "after_repeat": rng.getrandbits(32), "after_pressed": rng.getrandbits(32),
        "mode": number % 3,
        "state": [*(rng.randrange(256) for _ in range(4)), s32(rng.getrandbits(32))],
        "memory": rng.randbytes(CURSOR_BYTES) if kind == 6 else b"",
        "capacities": capacities,
        "order": [rng.choice((0, 1, 255, 256, 32768, 65535, rng.randrange(65536)))
                  for _ in range(704)] if kind == 6 else [],
        "rows": rng.randbytes(ROW_BYTES * ROW_COUNT * 2) if kind == 4 else b"",
    }


def stats_vector(kind, rng, number):
    data = rng.randbytes(MEMORY_BYTES)
    source = bytearray(rng.randbytes((32, 48)[kind]))
    present = kind == 0 or number % 5 != 0
    if kind == 0:
        source[1:3] = bytes(rng.choice((0, 1, 64, rng.randrange(1, 65))) for _ in range(2))
        source[4:7] = bytes(rng.choice((0, 1, 128, 192, rng.randrange(1, 193))) for _ in range(3))
    else:
        source[44:47] = bytes(
            rng.choice((0, 1, 127, 129, 255, rng.randrange(1, 128))) for _ in range(3)
        )
    alias = number % 3
    target_size = (0x198, 0x164)[kind]
    target = bytearray(rng.randbytes(target_size))
    if alias == 1 and present:
        target[:len(source)] = source
    if alias == 2:
        target = bytearray(data[0xA00:0xA00 + target_size])
    return {"kind": kind, "present": int(present), "alias": alias,
            "target": bytes(target), "source": bytes(source), "data": data}


def run(kind, machine, item):
    if kind < 2:
        return run_stats(kind, machine, item)
    for address, value, size in zip(STATE, item["state"], SIZES, strict=True):
        machine.put(address, value, size)
    for address, value in ((HELD, item["held"]), (REPEAT, item["repeat"]),
                           (PRESSED, item["pressed"])):
        machine.put(address, value)
    machine.put(CONTAINER, MEMORY)
    owner, reads = 0, 0
    if kind == 6:
        machine.store(CURSORS, item["memory"])
        for n, value in enumerate(item["capacities"]):
            machine.put(CAPACITY + n * 2, value, 2)
        for n, value in enumerate(item["order"]):
            machine.put(MEMORY + MEMORY_BYTES + n * 2, value, 2)
        machine.follow.add(0x80102C94)
    if kind == 4:
        for n, address in enumerate(ROWS):
            start = n * ROW_BYTES * ROW_COUNT
            machine.store(address, item["rows"][start:start + ROW_BYTES * ROW_COUNT])

    def state():
        return [machine.load(address, size, size == 4)
                for address, size in zip(STATE, SIZES, strict=True)]

    def callback(m, operation):
        nonlocal owner, reads
        argument = 0 if operation in (INVALID, LEAVE) else s32(m.r[4])
        extra = int(m.r[5] == MEMORY) if operation == COUNT else 0
        m.trace.append([operation, argument, extra, *state()])
        if operation == COUNT:
            m.r[2] = item["count"] & 0xFFFFFFFF
            m.put(REPEAT, item["after_repeat"])
            m.put(PRESSED, item["after_pressed"])
        elif operation == CLEAR and kind == 4 and item["mode"] == 1:
            owner = 1
        elif operation == ROW:
            m.r[2] = ROWS[owner] + argument * ROW_BYTES
            reads += 1
            if item["mode"] == 2 and reads == 1:
                owner = 1
        elif operation == PRIMARY:
            if item["mode"] == 1:
                m.put(PRESSED, item["after_pressed"])
            elif item["mode"] == 2:
                m.put(STATE[1], 0x93, 1)
                m.put(STATE[2], 0xA7, 1)

    for operation, address in CALLEES.items():
        machine.stub(address, functools.partial(callback, operation=operation))
    machine.r[4], machine.r[5] = (value & 0xFFFFFFFF for value in item["args"])
    machine.execute(RANGES[kind][0])
    memory = machine.bytes(CURSORS, CURSOR_BYTES) if kind == 6 else b""
    rows = b"".join(machine.bytes(address, ROW_BYTES * ROW_COUNT) for address in ROWS) if kind == 4 else b""
    result = s32(machine.r[2]) if kind == 3 else machine.r[2] if kind == 6 else 0
    allowed = [(DEFAULT_STACK - 0x80, DEFAULT_STACK),
               *((address, address + size) for address, size in zip(STATE, SIZES, strict=True))]
    if kind == 6:
        allowed.append((CURSORS, CURSORS + CURSOR_BYTES))
    if kind == 4:
        allowed += [(address, address + ROW_BYTES * ROW_COUNT) for address in ROWS]
    observation = {"values": [result, *state()], "memory": memory, "rows": rows,
                   "calls": machine.trace, "stores": machine.stores, "allowed": allowed}
    inputs = [kind, *item["args"], item["count"], item["held"], item["repeat"],
              item["pressed"], item["after_repeat"], item["after_pressed"], item["mode"],
              *item["state"], *item["memory"],
              *(item["capacities"] if kind == 6 else []), *item["order"], *item["rows"]]
    return observation, inputs


def run_stats(kind, machine, item):
    machine.store(MEMORY, item["data"])
    machine.store(SOURCE, item["target"] if item["alias"] == 1 else bytes(0x200))
    machine.store(SOURCE, item["source"])
    target = SOURCE if item["alias"] == 1 else MEMORY + 0xA00 if item["alias"] == 2 else TARGET
    machine.store(target, item["target"])
    machine.store(TEMP, bytes([0xA5]) * 0x200)
    machine.stub(0x80043E3C, lambda m: m.r.__setitem__(2, TEMP))
    machine.stub(0x80049110, lambda m: m.store(m.r[4], bytes(m.r[5])))
    machine.stub(0x80043C60, lambda m: None)
    machine.follow.update((0x8006A334, 0x8006A65C, 0x8006AB44, 0x8006AC74,
                           0x8006ACFC, 0x8006AE0C, 0x800490B0))
    machine.r[4:7] = [target, SOURCE if item["present"] else 0, MEMORY]
    machine.execute(RANGES[kind][0])
    observation = {
        "bytes": machine.bytes(target, len(item["target"])),
        "stores": machine.stores,
        "allowed": [(target, target + len(item["target"])),
                    (TEMP, TEMP + 0x10C), (DEFAULT_STACK - 0x100, DEFAULT_STACK)],
    }
    return observation, [kind, item["present"], item["alias"],
                         *item["target"], *item["source"], *item["data"]]


def check(image, item, observation, native, reference):
    if item["kind"] < 2:
        problems = [] if bytes(native) == observation["bytes"] else ["complete native loadout image differs"]
    else:
        memory_end = 6 + len(observation["memory"])
        rows_end = memory_end + len(observation["rows"])
        calls = [native[rows_end + 1 + n * 8:rows_end + 1 + (n + 1) * 8]
                 for n in range(native[rows_end])]
        problems = []
        if native[:6] != observation["values"]:
            problems.append(f"result/live scalar values differ: {native[:6]} != {observation['values']}")
        if bytes(native[6:memory_end]) != observation["memory"]:
            problems.append("complete cursor memory differs")
        if bytes(native[memory_end:rows_end]) != observation["rows"]:
            problems.append("complete borrowed/current row-owner images differ")
        if calls != observation["calls"]:
            problems.append(f"ordered callback arguments/state snapshots differ: {calls[:3]}")
    return problems + store_problems(observation["stores"], observation["allowed"])


def describe(kind, image, results):
    if kind < 2:
        return [
            "Real BATTLE part readers and weapon/shield builders plus actual SLUS memcpy instructions followed.",
            "Only allocation/bzero/free are bounded scratch callbacks; native stack values replace PS1 temporary management.",
            "Compared all 0x198 weapon/0x164 shield bytes, including retained reserved/tail fields; "
            "biased zero/first/last/duplicate/flagged references, signed stats and target/source/container aliases.",
            "Undefined null weapon name and out-of-table used references are separate checked Live refusals, not parity vectors.",
        ]
    return [
        "Actual MENUD body; container CategoryIndices instructions followed for selection.",
        "Bounded production-service callbacks: count, sounds, clear/get row, existing continuation, "
        "stat pages, primary stats, flyout and DP/PP bars; all used arguments/order and five live-state snapshots compared.",
        "Full 0xF2-byte cursor memory, unsigned-halfword display tokens and both complete 40-row PS1 images compared.",
        "Biased simultaneous shoulders, overflow/negative pages, zero/one/full counts, owner changes after clear/get-row, "
        "header/selected aliases, callback-written input/panels and optional bars; no inventory transition/return fields invented.",
    ]
