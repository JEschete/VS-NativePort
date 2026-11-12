"""Four presentation policies against original MIPS in the unaffected domain.

Services are controlled and callback mutations are explicit. The two discarded
bone-query results are not events: the real query is read-only. Only initialized
effect fields are compared, not original unspecified record padding. #356's
intentional corrections have separate production-builder tests and controls.
"""

import random
import struct

from differential import DEFAULT_STACK, MASK, Image, Machine, store_problems

ID = "F00100/F00101/F00108/F00109"
ENTRY, END = 0x8007138C, 0x800719DC
ENTRIES = (0x80070278, 0x800704B0, 0x80070F28, ENTRY)
ENDS = (0x800704B0, 0x800704D8, 0x8007138C, END)
FUNCTIONS = ("F00100", "F00101", "F00108", "F00109")
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "action_presentation_driver.cpp"
INCLUDES = (r"Native\Tests\include",)
SOURCES = (r"Native\Game\src\Battle\ActionPresentation.cpp",
           r"Native\Game\src\Battle\ActionPreview.cpp",
           r"Native\Engine\src\Core\Math\ScalarMath.cpp",
           r"Native\Engine\src\Core\Math\Distance.cpp")
VECTORS, MUTANT_VECTORS, SEED = 12288, 2048, 100109
STATE, ACTOR, STATS, MENU = 0x80500000, 0x80600000, 0x80700000, 0x80800000
GLOBAL, ACTORS, DEFINITIONS = 0x800F19CC, 0x800F1928, 0x8004B9DC
FIELDS = (0x2C02, 0x2C03, 0x2C04, 0x2C05, 0x2C06, 0x2C07, 0x2C08, 0x2C0A)
(F, LANE, ACTION, ENABLED, SOURCE, SOURCE_KIND, INDEX, SLOT, MAPPING,
 ANIMATION, FRAME, PREVIEW, MODE, KIND, ITEM, CATEGORY, ATTACK, HEAL,
 DISTANCE, ZOOM, STATUS, EFFECT, VARIANT, INFO, MUTATE, SHAPE) = range(26)
MUTANTS = {
    "F00100-no-window-reset": {0x80070294: 0},
    "F00100-no-timer-reset": {0x80070298: 0},
    "F00100-queue-mask": {0x8007029C: 0x30630001},
    "F00100-chain-range": {0x800702D0: 0x2C42001F},
    "F00100-no-primary-kind": {0x800702E4: 0},
    "F00100-no-primary-player": {0x800702F4: 0},
    "F00100-flag": {0x800702F8: 0x240400B7},
    "F00100-melee-parameter": {0x80070358: 0x34060000},
    "F00100-break-boundary": {0x8007037C: 0x2C42008E},
    "F00100-special-boundary": {0x80070384: 0x2C4200B9},
    "F00100-stage": {0x80070398: 0x24060000},
    "F00100-speed": {0x800703E0: 0x24020003},
    "F00100-defense-boundary": {0x800703F8: 0x2C420027},
    "F00100-chain-offset": {0x80070420: 0x24C6FFEB},
    "F00100-half-window": {0x80070450: 0},
    "F00100-stop-order": {0x8007046C: 0x24050034},
    "F00100-no-slot-clear": {0x80070490: 0},
    "F00100-phase": {0x800704A0: 0x24020006},
    "F00101-phase": {0x800704B8: 0x24020005},
    "F00101-no-callback": {0x800704C0: 0},
    "F00108-no-prepare": {0x80070F3C: 0},
    "F00108-no-primary-volume": {0x80070F5C: 0},
    "F00108-cancel-preview": {0x80070F7C: 0x240200FE},
    "F00108-no-reaction": {0x80070FB4: 0},
    "F00108-no-menu-cancel": {0x80071018: 0},
    "F00108-no-resolve": {0x800710E4: 0},
    "F00108-no-attack-score": {0x800711A0: 0},
    "F00108-score-saturation": {0x80071190: 0x3402FFFD},
    "F00108-distance-boundary": {0x800711AC: 0x284201C0},
    "F00108-zoom-mask": {0x80071234: 0x30420001},
    "F00108-no-buttons-reset": {0x80071248: 0},
    "F00108-no-input-reset": {0x80071254: 0},
    "F00108-no-continued-reset": {0x80071264: 0},
    "F00108-spell-boundary": {0x80071278: 0x2C420038},
    "F00108-no-heal-score": {0x800712EC: 0},
    "F00108-heal-limit": {0x800712E0: 0x2C6203E7},
    "F00108-no-distance": {0x800711A4: 0},
    "F00108-no-idle": {0x8007131C: 0},
    "F00108-no-palette": {0x8007134C: 0},
    "F00108-no-freeze": {0x80071354: 0},
    "F00109-no-placement": {0x80071488: 0},
    "F00109-no-success-reset": {0x800713BC: 0},
    "F00109-attempt-mask": {0x800713DC: 0x30424000},
    "F00109-spell-sound": {0x800714D0: 0x24050035},
    "F00109-source-kind": {0x800715CC: 0x24020005},
    "F00109-source-event-slot": {0x800715E4: 0},
    "F00109-no-source-part": {0x800715F8: 0},
    "F00109-no-duplicate-aggregation": {0x80071668: 0xA0820016},
    "F00109-wrong-default-limb": {0x800716E0: 0x24050003},
    "F00109-no-final-target-part": {0x80071700: 0},
    "F00109-target-event-slot": {0x80071694: 0},
    "F00109-world-tail-zero": {0x80071724: 0xBA000018},
    "F00109-other-kind": {0x80071778: 0x2C420001},
    "F00109-no-source-exclusion": {0x800717EC: 0},
    "F00109-only-fifteen-others": {0x80071820: 0x2A42000F},
    "F00109-no-focus-z": {0x8007184C: 0},
    "F00109-ground-effect": {0x80071868: 0x24020023},
    "F00109-variant-zero": {0x80071874: 0x24060000},
    "F00109-no-dispatch": {0x80071878: 0},
    "F00109-no-asset": {0x80071974: 0},
    "F00109-model-nibble": {0x800718FC: 0x000210C2},
    "F00109-phase": {0x800719A8: 0x24020004},
    "F00109-no-hud": {0x8007197C: 0},
    "F00109-no-message": {0x8007199C: 0},
}


def vector(rng: random.Random, number: int) -> list[int]:
    f, lane = number % 4, number // 4
    actions = (1, 21, 22, 39, 40, 53, 54, 55, 77, 96, 140, 141, 183, 184, 223, 224, 227, 228, 232, 235, 236)
    v = [f, lane, actions[lane % len(actions)], lane // len(actions) % 2,
         lane // 3 % 3, lane // 5 % 2, (0, 1, 2, 3, 4, 7, 0x80000005)[lane // 7 % 7],
         lane % 3 + 1, (0, 22, 39, 128, 255)[lane % 5],
         rng.randrange(65536), rng.choice((0, 1, 32768, MASK, rng.getrandbits(32))),
         (0, 1, 254, 255)[lane // 3 % 4], (0, 1, 2, 6, MASK)[lane // 4 % 5],
         (0, 7, 8, -1)[lane // 5 % 4] & MASK, rng.randrange(65536),
         (0, 9, 10)[lane // 6 % 3], (0, 65534, 65535)[lane // 3 % 3],
         (0, 999, 1000, 65535)[lane // 2 % 4],
         (MASK, 448, 449, 32767)[lane // 8 % 4], lane // 3 % 4,
         (0, 0x8000)[lane % 2], (36, 39, 255)[lane // 3 % 3],
         (0, 127, 128, 255)[lane % 4], lane % 2, 0, lane % 6]
    v[MUTATE] = ((0, 1, 2, 3, 4), (0, 5), (0, 6, 7, 8, 9, 10, 11),
                 (0, 2, 12, 13, 14, 15, 16))[f][lane % (5, 2, 7, 7)[f]]
    if f == 3:
        v[ENABLED] = lane // 13 % 5 != 0
    return [int(x) for x in v]


def run(m: Machine, v: list[int]) -> tuple[dict, list[int]]:
    m.map_zero(STATE, 0x8000)
    m.map_zero(ACTOR, 0x1000)
    m.map_zero(STATS, 0x10000)
    m.map_zero(MENU, 16)
    m.store(DEFAULT_STACK - 0x2000, bytes([0xA5]) * 0x2000)
    m.map_zero(DEFAULT_STACK, 32)
    current = 0
    queue = STATE + 0x854 + (v[INDEX] & 3) * 0x84C
    calls = []
    dispatched = []
    asset_calls = []
    m.put(GLOBAL, STATE)
    m.put(0x800F18EC, MENU)
    m.put(MENU + 2, v[KIND], 2)
    m.put(MENU + 6, v[ITEM], 2)
    m.put(0x800F18F0, 0)
    m.put(0x8005E24C, 0)
    m.put(0x8005FFAF, 1, 1)
    m.put(0x8005FFB2, v[HEAL], 2)
    m.put(0x80060028, v[INFO], 1)
    for i in range(3):
        m.put(0x8006002C + i, v[MAPPING], 1)
    for i in range(10):
        m.put(0x8005FEB4 + i * 2, v[ATTACK], 2)
    for action in range(256):
        m.put(DEFINITIONS + action * 52 + 2, v[ENABLED], 1)
        m.put(DEFINITIONS + action * 52 + 1, v[EFFECT], 1)
        m.put(DEFINITIONS + action * 52 + 13, v[ZOOM] << 4, 1)
    for i in range(16):
        m.put(ACTORS + i * 4, ACTOR + i * 0x80)
        m.put(ACTOR + i * 0x80 + 0x3C, STATS + i * 0x1000)
        eligible = i in (0, 3, 15) or (i == 1 and v[SHAPE] in (1, 3)) or (i == 2 and v[SHAPE] in (2, 5))
        m.put(ACTOR + i * 0x80 + 0x40, (1 + i % 2) if eligible else 3)
        m.put(ACTOR + i * 0x80 + 0xC, 0x10002)
        m.put(ACTOR + i * 0x80 + 0x27, 128 if v[LANE] % 11 == 0 else 0, 1)
        m.put(ACTOR + i * 0x80 + 0x25, (v[LANE] + i * 2) % 11, 1)
        m.put(STATS + i * 0x1000 + 0x36, 4, 1)
        m.put(STATS + i * 0x1000 + 0x8E6, ((v[LANE] + i) % 16) << 4, 1)
    m.put(0x800F19FC, ACTOR)
    m.put(ACTOR + 0x25, v[CATEGORY], 1)
    m.put(STATS + 0x948, v[STATUS])
    shapes = {
        0: [],
        1: [(1, v[LANE] % 6, 0)],
        2: [(1, 0, 0), (2, 255, 0)],
        3: [(1, 0, 0), (1, 2, 0)],
        4: [(7, 0, 1)] * 16,
        5: [(1, 0, 0), (7, 0, 1), (2, 255, 0)],
    }
    for state in range(2):
        base = STATE + state * 0x4000
        m.put(base, v[INDEX] if state == 0 else v[INDEX] ^ 1)
        m.put(base + 4, 9)
        m.put(base + 0x298C, v[PREVIEW], 1)
        m.put(base + 0x2C0E, 7, 2)
        for i, offset in enumerate(FIELDS):
            m.put(base + offset, v[SLOT] if i == 1 else 9, 2 if i >= 6 else 1)
        for q in range(5):
            request = base + (8 if q == 0 else 0x854 + (q - 1) * 0x84C)
            m.put(request, v[ACTION] if q in (0, (v[INDEX] & 3) + 1) else 96, 2)
            m.put(request + 2, v[VARIANT], 1)
            m.put(request + 4, v[SOURCE], 1)
            m.put(request + 0x44, v[SOURCE_KIND])
            m.put(request + 0x48, v[ANIMATION], 2)
            m.store(request + 0x844, struct.pack("<4h", 10 + state, -20, 30, 40))
            m.put(request + 0x4A, len(shapes[v[SHAPE]]), 2)
            for i, (actor, limb, kind) in enumerate(shapes[v[SHAPE]]):
                hit = request + 0x4C + i * 68
                m.put(hit, actor, 1)
                m.put(hit + 1, limb, 1)
                m.put(hit + 64, kind)
                if kind:
                    m.store(hit, struct.pack("<4h", actor, -3, 17, -99))
            if v[SHAPE] == 1:
                m.put(request + 0x4C + 68, 1, 1)
            if v[SHAPE] == 2:
                m.put(request + 0x4C + 2 * 68, 2, 1)
            if v[SHAPE] == 0 and v[LANE] // 6 % 2:
                m.put(request + 0x4C + 64, 4)

    def mutate(address):
        nonlocal current
        mutation = v[MUTATE]
        animation = address in (0x8009DD00, 0x8009E2E0, 0x8009F298, 0x8009EFEC, 0x8009EC9C, 0x8009EE9C)
        if ((mutation == 1 and address == 0x800BEC14) or
            (mutation == 2 and animation) or
            (mutation == 3 and address == 0x8009F8DC) or
            (mutation == 4 and address == 0x800CB50C)):
            current = 1
            if mutation == 2:
                m.put(queue + 4, 2, 1)
        if mutation == 5 and address == 0x800CF920:
            m.put(0x800F18F0, 91)
        if mutation == 6 and address == 0x8006EF10:
            current = 1
            m.put(STATE + 0x4000 + 0x298C, 255, 1)
        if mutation == 7 and address == 0x800C16DC:
            m.put(STATE + current * 0x4000 + 0x298C, v[PREVIEW] ^ 255, 1)
        if mutation == 8 and address == 0x800CB114:
            current = 1
            m.put(STATE + 0x4000 + 12, 2, 1)
        if mutation == 9 and address == 0x80085B10:
            current = 1
            base = STATE + 0x4000 + 8
            m.put(base, 184, 2)
            m.put(base + 4, 0, 1)
            m.put(base + 0x4C, 1, 1)
            m.put(base + 0x4C + 64, 0)
        if mutation == 10 and address == 0x80077F70:
            current = 1
            m.put(STATE + 0x4000 + 8, 232, 2)
            m.put(DEFINITIONS + 232 * 52 + 13, 16, 1)
        if mutation == 11 and address == 0x8009E5C4:
            current = 1
            m.put(STATE + 0x4000 + 12, 2, 1)
        if mutation == 12 and address == 0x800A1108:
            m.put(queue, 141, 2)
            m.put(queue + 4, 1, 1)
        if mutation == 13 and animation:
            m.put(queue, 224, 2)
            m.put(queue + 4, 0, 1)
        if mutation == 14 and address == 0x800CF0E8:
            m.put(queue, 223, 2)
            m.put(queue + 4, 0, 1)
            current = 1
        if mutation == 15 and address == 0x800CB654:
            m.put(0x80060028, 1, 1)
            m.put(queue, 222, 2)
        if mutation == 16 and address == 0x800CB2B8:
            m.put(0x800F18F0, 99)
        m.put(GLOBAL, STATE + current * 0x4000)

    def event(address, *args):
        calls.extend([address, *(x & MASK for x in args)])
        mutate(address)

    def plain(address, count):
        def callback(machine):
            event(address, *machine.r[4:4 + count])
        return callback

    zero = (0x800CB50C, 0x8006EF10, 0x800C16DC, 0x800C06E0, 0x800C0738,
            0x800C05B4, 0x800CB114, 0x800704D8, 0x80070CAC, 0x80070B04,
            0x800708EC, 0x8006C250)
    one = (0x800E4C64, 0x8006FBCC, 0x800CCB9C, 0x800D82A8, 0x80048FEC,
           0x80095B70, 0x8008B4C8, 0x8008B4BC, 0x800CB654)
    for address in zero:
        m.stubs[address] = plain(address, 0)
    for address in one:
        m.stubs[address] = plain(address, 1)
    for address in (0x800BEC14, 0x80045D64):
        m.stubs[address] = plain(address, 2)
    m.stubs[0x800CB2B8] = plain(0x800CB2B8, 3)

    def animation(address):
        def callback(machine):
            point = [machine.load(machine.r[5] + i * 2, 2, signed=True) for i in range(4)]
            event(address, machine.r[4], *point, machine.r[6],
                  machine.r[7] if address == 0x8009EC9C else 0)
        return callback

    for address in (0x8009DD00, 0x8009E2E0, 0x8009F298, 0x8009EFEC, 0x8009EC9C, 0x8009EE9C):
        m.stubs[address] = animation(address)

    def window(machine):
        event(0x8009F8DC, machine.r[4])
        machine.r[2] = v[FRAME]

    def distance(machine):
        event(0x80077F70)
        machine.r[2] = v[DISTANCE]

    def placement(machine):
        machine.store(machine.r[5] + 4, struct.pack("<4h", 11, -22, 33, 0))
        event(0x800A1108, machine.r[4])

    def sound(machine):
        event(0x80045AA8, *machine.r[4:6],
              *(machine.load(machine.r[6] + i * 2, 2, signed=True) for i in range(3)))

    def bone(machine):
        actor, limb, kind = machine.r[4:7]
        assert kind == 2
        machine.r[2] = 0 if limb == 255 else (actor * 8 + (limb if limb < 0x80000000 else limb - 2**32)) & MASK

    def target(address):
        kind = m.load(address, 2)
        return ([kind, m.load(address + 4, 1), m.load(address + 5, 1),
                 m.load(address + 6, 1), 0, 0, 0, 0] if kind == 4 else
                [kind, 0, 0, 0, *(m.load(address + 4 + i * 2, 2) for i in range(4))])

    def dispatch(machine):
        address = machine.r[4]
        count, other = machine.load(address + 2, 1), machine.load(address + 3, 1)
        dispatched.extend([machine.load(address, 2), count, other, *target(address + 4)])
        for i in range(count):
            dispatched.extend(target(address + 16 + i * 12))
        dispatched.extend(machine.load(address + 0xD0 + i, 1) for i in range(other))
        dispatched.extend(machine.load(address + 0xE0 + i * 2, 2) for i in range(3))
        event(0x800CF0E8, machine.r[5], machine.r[6])

    def asset(machine):
        address = machine.r[4]
        values = [machine.load(address, 1), machine.load(address + 1, 1),
                  machine.load(address + 16, 1), machine.load(address + 2, 2),
                  machine.load(address + 18, 1)]
        asset_calls.extend(values)
        event(0x800995E8, *values)

    m.stubs[0x8009F8DC] = window
    m.stubs[0x80077F70] = distance
    m.stubs[0x800A1108] = placement
    m.stubs[0x80045AA8] = sound
    m.stubs[0x800A152C] = bone
    m.stubs[0x800CF0E8] = dispatch
    m.stubs[0x800995E8] = asset
    m.stubs[0x800CF920] = lambda _: event(0x800CF920, m.load(0x800F18F0))
    m.stubs[0x80085B10] = lambda machine: event(0x80085B10, machine.r[4], machine.r[7])
    m.stubs[0x8009E5C4] = lambda machine: event(0x8009E5C4, machine.r[4], m.load(0x800F18F0))
    m.stubs[0x8007B1B8] = lambda machine: event(0x8007B1B8, *machine.r[4:8], machine.load(machine.r[29] + 16))
    m.stores.clear()
    m.follow.add(ENTRIES[0])
    m.r[4] = v[MODE]
    m.execute(ENTRIES[v[F]], limit=30000)
    values = [current, m.load(0x800F18F0), m.load(0x8005E24C),
              m.load(MENU + 2, 2), m.load(0x8005FFAF, 1), m.load(0x8005FFB2, 2)]
    for state in range(2):
        base = STATE + state * 0x4000
        values.extend([m.load(base), m.load(base + 4), m.load(base + 0x298C, 1), m.load(base + 0x2C0E, 2)])
        values.extend(m.load(base + offset, 2 if i >= 6 else 1) for i, offset in enumerate(FIELDS))
        for q in range(5):
            request = base + (8 if q == 0 else 0x854 + (q - 1) * 0x84C)
            values.extend([m.load(request, 2), m.load(request + 4, 1)])
    values.extend(m.load(0x8005FEB4 + i * 2, 2) for i in range(10))
    values.extend([len(dispatched), *dispatched, len(asset_calls), *asset_calls, len(calls), *calls])
    stores = [(a, n) for a, n in m.stores if not DEFAULT_STACK - 0x2000 <= a < DEFAULT_STACK + 32]
    return {"values": values, "stores": stores}, v


def check(image: Image, v: list[int], observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, v, reference
    expected = observation["values"]
    differences = [(i, a, b) for i, (a, b) in enumerate(zip(native, expected, strict=False)) if a != b]
    problems = [] if native == expected else [f"state/request/events {differences[:8]}; lengths {len(native)}/{len(expected)}"]
    return problems + store_problems(observation["stores"], [
        (STATE, STATE + 0x8000), (MENU, MENU + 16), (GLOBAL, GLOBAL + 4),
        (DEFINITIONS, DEFINITIONS + 256 * 52), (0x800F18F0, 0x800F18F4),
        (0x8005E24C, 0x8005E250), (0x8005FEB4, 0x8005FEC8),
        (0x8005FFAF, 0x8005FFB4), (0x80060028, 0x80060029)])


def describe(image: Image, results: list) -> list[str]:
    lines = []
    for f, (name, start, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, o, n) for v, o, n in results if v[F] == f]
        lines.append(f"{name}: {len(selected)} vectors, "
                     f"{sum(bool(check(image, v, o, n, {})) for v, o, n in selected)} mismatches; "
                     f"[{start:08X},{end:08X}) {(end-start)//4} instructions SHA256={image.sha256(start,end)}")
    return lines + ["All functions VOID; full defined state/event/request fields compared. "
                    "Read-only bone queries and unspecified effect padding are not event/byte parity. "
                    "#356 corrected default/compaction/bystanders/overflow separately tested through production."]
