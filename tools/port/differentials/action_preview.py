"""Six action preview/reset/resolution policies against 861 original instructions.

The preview builders' source limb/focus are unspecified stack residue (#353).
Their comparison projects those bytes out, and services do not consume that limb.
Catch2 separately runs the fixed fresh-request contract through real chain dispatch.
All other bytes in the original 30-target request are compared; native tests own
the expanded 255-target reset/copy. Services model callback mutation explicitly.
"""

import random
import struct

from differential import DEFAULT_STACK, MASK, Image, Machine, s16, store_problems

ID = "F00508/F00509/F00513/F00514/F00515/F00516"
ENTRY, END = 0x80085B10, 0x8008631C
ENTRIES = (0x800856F8, 0x80085718, 0x80085A34, ENTRY, 0x8008631C, 0x800863A4)
ENDS = (0x80085718, 0x8008574C, ENTRY, END, 0x800863A4, 0x80086754)
FUNCTIONS = ("F00508", "F00509", "F00513", "F00514", "F00515", "F00516")
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
DRIVER = "action_preview_driver.cpp"
SOURCES = (r"Native\Game\src\Battle\ActionPreview.cpp",
           r"Native\Engine\src\Core\Math\ScalarMath.cpp",
           r"Native\Engine\src\Core\Math\Distance.cpp")
VECTORS, MUTANT_VECTORS, SEED = 12288, 1536, 508516
RESULT, SOURCE, STATS, ACTOR, STATE, FROM, TO = (
    0x80500000, 0x80510000, 0x80600000, 0x80700000, 0x80800000, 0x80900000, 0x80900010)
ACTORS, DEFINITIONS = 0x800F1928, 0x8004B9DC
SIZE = 0x84C
(F, LANE, SEED_BYTE, EXECUTE, SOURCE_KIND, COUNT, CATEGORY, PRE0, PRE1, STAT,
 DAMAGE, AFFINITY, DRAWN, BLADE, WEAPON_DAMAGE, CANCEL, SPAWN, QUEUE, TRACE,
 CHECK, ROLL, MUTATE, TARGET, LIMB, EFFECTS, THRESHOLD, IMPACT, FLAGS,
 X0, Y0, Z0, T0, X1, Y1, Z1, T1) = range(36)
MUTANTS = {
    "F00508-short-clear": {0x80085704: 0x24050848},
    "F00509-short-clear": {0x8008572C: 0x24050040},
    "F00509-impact-five": {0x80085730: 0x24020005},
    "F00513-position-gate": {0x80085A3C: 0},
    "F00513-no-threshold-reset": {0x80085A54: 0},
    "F00513-ignore-impact": {0x80085A60: 0},
    "F00513-effect2": {0x80085A84: 0x24080004},
    "F00513-effect1": {0x80085AC8: 0xA0870003},
    "F00513-effect3": {0x80085AD0: 0xA0880003},
    "F00513-eight-lanes": {0x80085AD4: 0x28C20008},
    "F00513-ignore-inflict": {0x80085AF4: 0x00401025},
    "F00514-action-id": {0x80085B84: 0xA6C00000},
    "F00514-source-limb": {0x80085BBC: 0xA2C00005},
    "F00514-origin-mode": {0x80085BD4: 0x24070001},
    "F00514-five-position-kinds": {0x80085C4C: 0x2C420005},
    "F00514-target-limb": {0x80085C48: 0xA200004D},
    "F00514-no-focus-tag": {0x80085CC0: 0xA6C0084A},
    "F00514-no-explicit-affinity-offset": {0x80085CDC: 0x24460000},
    "F00514-affinity-owner-slot": {0x80085CF0: 0x24020000},
    "F00514-affinity-drawn-bit": {0x80085D10: 0x30420002},
    "F00514-affinity-six": {0x80085D58: 0x2A620006},
    "F00514-no-default-physical": {0x80085DBC: 0},
    "F00514-stat24": {0x80085D8C: 0x24020018},
    "F00514-no-last-reset": {0x80085DEC: 0},
    "F00514-no-previous-reset": {0x80085DF0: 0},
    "F00514-no-prerequisite-reset": {0x80085DF4: 0},
    "F00514-direct-count-gated": {0x80085E50: 0},
    "F00514-self-no-prerequisite": {0x80085EA8: 0},
    "F00514-self-no-cancel": {0x80085F0C: 0},
    "F00514-ignore-redirect": {0x80085F5C: 0x30420000},
    "F00514-terrain-mp": {0x80085FE0: 0x24020040},
    "F00514-tag-limit": {0x80086014: 0x2C420003},
    "F00514-no-redirect-limb": {0x80086064: 0},
    "F00514-no-room-cancel": {0x80086160: 0},
    "F00514-no-execute-cancel": {0x800861A4: 0},
    "F00514-no-failed-threshold": {0x800861D4: 0},
    "F00514-one-hit": {0x800861F4: 0x2AA20001},
    "F00514-no-copy-primary": {0x80086280: 0},
    "F00514-no-recovery": {0x800862D4: 0},
    "F00514-cost-preview": {0x800862E8: 0x24060000},
    "F00515-no-target": {0x80086348: 0xA7A0005A},
    "F00515-no-limb": {0x80086350: 0xA3A0005D},
    "F00515-execute": {0x80086360: 0x24070001},
    "F00515-cost-update": {0x80086384: 0x24060001},
    "F00515-no-cost-mask": {0x80086398: 0},
    "F00516-shape-mask": {0x8008647C: 0x30630003},
    "F00516-pitch-bias": {0x8008651C: 0x2442003F},
    "F00516-area-mask": {0x800865AC: 0x304200F0},
    "F00516-bone-mode": {0x80086650: 0x24070001},
    "F00516-failed-bone-not-skipped": {0x80086654: 0},
    "F00516-five-limbs": {0x80086684: 0x2A220005},
    "F00516-ignore-contains": {0x80086664: 0},
    "F00516-explicit-limb": {0x800866A4: 0xA3A0005D},
    "F00516-default-limb": {0x800866E4: 0xA3A0005D},
    "F00516-cost-update": {0x8008671C: 0x24060001},
    "F00516-no-mask": {0x80086748: 0},
}


def vector(rng: random.Random, number: int) -> list[int]:
    f, lane = number % 6, number // 6
    v = [0] * 36
    v[:8] = [f, lane, rng.randrange(1, 256), lane % 2, (0, 0, 3)[lane % 3],
             (0, 1, 2, 3, 30)[lane % 5], lane % 16, (0, 1, 3, 6, 7, 8, 9)[lane // 2 % 7]]
    v[PRE1] = (0, 1, 3, 8)[lane // 7 % 4]
    v[STAT] = (0, 2, 25)[lane // 3 % 3]
    v[DAMAGE], v[AFFINITY] = lane // 5 % 4, lane // 8 % 8
    v[DRAWN], v[BLADE], v[WEAPON_DAMAGE] = lane // 2 % 4, lane % 2, lane // 3 % 4
    v[CANCEL], v[SPAWN], v[QUEUE] = lane // 3 % 2, (0, 128)[lane // 4 % 2], lane // 2 % 2
    v[TRACE] = (MASK, 0, 2, 3, 16, 17, 65535)[lane // 2 % 7]
    v[CHECK], v[ROLL], v[MUTATE] = lane // 3 % 2, lane // 5 % 2, lane // 7 % 7
    v[TARGET], v[LIMB] = (1, 2, 257)[lane // 4 % 3], (0, 5, 255, 259)[lane % 4]
    v[EFFECTS] = (0, 1, 2, 3, 1 << 16, 2 << 16, 3 << 16, 1 << 18, 1 << 31)[lane % 9]
    v[THRESHOLD], v[IMPACT], v[FLAGS] = (0, 99, 255)[lane // 3 % 3], (0, 4, 6)[lane // 9 % 3], lane % 2
    v[X0:T1 + 1] = [(x & 65535) for x in
                    rng.choice(((0, 0, 0, 7, 64, 64, 0, 9),
                                (25000, 32767, 25000, -1, -25000, -32768, -25000, 2),
                                (0, 3000, 0, 1, 317, -3000, 281, 3),
                                tuple(s16(rng.getrandbits(16)) for _ in range(8))))]
    dx, dz = s16(v[X1]) - s16(v[X0]), s16(v[Z1]) - s16(v[Z0])
    if (dx * dx + dz * dz) & 0x80000000:
        # The original root reads before its lookup table for negative words.
        # The shared native helper deliberately returns zero outside that domain.
        v[X0], v[Z0], v[X1], v[Z1] = 0, 0, 317, 281
    if f >= 4:
        v[SOURCE_KIND], v[EXECUTE] = 0, 0
    if f == 2:
        v[SOURCE_KIND] = (0, 0, 3)[lane // 27 % 3]
    if v[SOURCE_KIND] != 0:
        v[STAT], v[FLAGS] = 0, 0
    if f == 5 and v[CATEGORY] in (1, 2, 3, 4, 6, 8, 11, 12):
        v[TARGET] %= 256
    return v


def hash_bytes(data: bytes) -> int:
    result = 2166136261
    for value in data:
        result = ((result ^ value) * 16777619) & MASK
    return result


def projection(data: bytes, preview: bool) -> int:
    value = bytearray(data)
    if preview:
        value[5] = 0
        value[0x844:0x84C] = bytes(8)
    return hash_bytes(value)


def run(m: Machine, v: list[int]) -> tuple[dict, list[int]]:
    m.store(RESULT, bytes([v[SEED_BYTE]]) * SIZE)
    m.store(SOURCE, bytes([v[SEED_BYTE]]) * SIZE)
    m.store(DEFAULT_STACK - 0x1000, bytes([v[SEED_BYTE]]) * 0x1000)
    m.map_zero(DEFAULT_STACK, 32)
    m.map_zero(STATS, 0x4000)
    m.map_zero(ACTOR, 0x1000)
    m.map_zero(STATE, 16)
    m.put(0x800F19CC, STATE)
    m.put(STATE, v[QUEUE])
    for i in range(16):
        m.put(ACTORS + i * 4, ACTOR + i * 0x100)
        m.put(ACTOR + i * 0x100 + 0x3C, STATS + min(i, 2) * 0x1000)
        m.put(ACTOR + i * 0x100 + 0x20, v[DRAWN] if i == 2 else v[DRAWN] ^ 1)
        m.put(ACTOR + i * 0x100 + 0x27, v[SPAWN], 1)
    for i in range(3):
        base = STATS + i * 0x1000
        m.put(base + 0x957, 2, 1)
        m.put(base + 0x36, i + 2, 1)
        m.put(base + 0x956, v[CANCEL] if i != 2 else v[CANCEL] ^ 1, 1)
        m.put(base + 0x54, v[BLADE], 2)
        m.put(base + 0x14A, v[WEAPON_DAMAGE], 1)
        for j in range(7):
            value = (0, -1, 4, 4, -32768, 2, 9)[(j + v[LANE]) % 7]
            m.put(base + 0x184 + j * 2, value, 2)
    definition = DEFINITIONS + 23 * 52
    m.put(definition, v[CATEGORY] << 20)
    m.put(definition + 4, 0xFF8040FF)
    m.put(definition + 8, 0xFF090807)
    m.put(definition + 12, v[FLAGS] << 14)
    for hit in range(2):
        m.put(definition + 20 + 4 * hit, v[PRE0 + hit] << 7 | v[STAT] << 16 |
              v[DAMAGE] << 27 | v[AFFINITY] << 29)
    m.put(SOURCE + 4, 0, 1)
    m.put(SOURCE + 5, v[LIMB], 1)
    m.put(SOURCE + 0x44, v[SOURCE_KIND])
    m.put(SOURCE + 0x4A, v[COUNT], 2)
    for i in range(30):
        hit = SOURCE + 0x4C + i * 68
        m.put(hit, i % 2 + 1, 1)
        m.put(hit + 1, (i + 3) % 6, 1)
        m.put(hit + 64, (0, 1, 5, 6, MASK)[i % 5])
    m.put(RESULT + 4 + 64, v[SOURCE_KIND])
    m.put(RESULT + 4 + 2, v[THRESHOLD], 1)
    m.put(RESULT + 4 + 3, v[IMPACT], 1)
    m.put(RESULT + 4 + 20, v[LANE] % 2)
    m.put(RESULT + 4 + 24, v[LANE] // 2 % 2)
    m.put(RESULT + 4 + 28, v[EFFECTS])
    m.store(FROM, struct.pack("<4H", *v[X0:T0 + 1]))
    m.store(TO, struct.pack("<4H", *v[X1:T1 + 1]))
    for address, value in ((0x800F1A00, 456), (0x800F1A04, 123), (0x800F1A08, 789)):
        m.put(address, value)
    calls = []
    current_source = SOURCE

    def event(kind, *values):
        calls.extend([kind, *(x & MASK for x in values)])

    def swap():
        for slot in (0, 1):
            m.put(ACTOR + slot * 0x100 + 0x3C, STATS + 0x2000)

    def origin(machine):
        event(1, machine.r[4], machine.r[5], machine.r[7])
        machine.store(machine.r[6], struct.pack("<4h", 1, 2, 3, 4))
        if v[MUTATE] == 1 and v[F] == 3:
            machine.put(current_source + 0x4A, 2, 2)
        machine.r[2] = 0

    def position(machine):
        event(2, machine.r[4], machine.r[5])
        machine.store(machine.r[6], machine.bytes(TO, 8))

    def trace(machine):
        event(3, machine.r[7])
        if v[TRACE] != MASK:
            machine.store(machine.r[6], struct.pack("<4H", 11, 22, 33, v[TRACE]))
        machine.r[2] = int(v[TRACE] != MASK)

    def check(machine):
        hit = machine.r[6]
        event(40 + hit, machine.load(machine.r[5], 1))
        if v[MUTATE] == 4:
            machine.put(RESULT + 0x4A, 0, 2)
        machine.r[2] = v[CHECK]

    def roll(machine):
        target, hit = machine.r[6], machine.r[7]
        event(50 + hit, machine.load(target, 1), machine.load(0x800F1A00),
              machine.load(0x800F1A04), machine.load(machine.r[29] + 16))
        machine.put(target + 2, 77, 1)
        machine.r[2] = v[ROLL]

    def effect(machine):
        target, hit = machine.r[6], machine.load(machine.r[29] + 16)
        event(60 + hit, machine.load(target, 1), machine.r[7],
              machine.load(0x800F1A00), machine.load(0x800F1A04),
              machine.load(0x800F1A08))
        machine.put(target + 4, 10 + hit, 2)
        machine.put(target + 28, machine.load(target + 28) | 1)
        machine.put(0x800F1A08, 100 + hit)
        if v[MUTATE] == 2 and hit == 0:
            machine.put(RESULT + 0x4A, 1, 2)
            machine.put(definition + 24, 3 << 7)
            swap()
        if v[MUTATE] == 3:
            machine.put(target, 2, 1)
            machine.put(target + 1, 5, 1)
            swap()
        if v[MUTATE] == 5:
            machine.put(STATE, 0)

    def injuries(machine):
        event(70, machine.load(machine.r[4], 1),
              machine.load(machine.r[4] + 3, 1))

    def recovery(machine):
        event(80, machine.r[4], (machine.r[5] - STATS) // 0x1000, machine.r[6])
        if v[MUTATE] == 6:
            swap()

    def cost(machine):
        event(90, machine.r[4], (machine.r[5] - STATS) // 0x1000, machine.r[6])
        machine.r[2] = 0xAB123456

    def limb(machine):
        index = machine.r[5]
        event(20, machine.r[4], index, machine.r[7])
        machine.store(machine.r[6], struct.pack("<4H", index, 0, 0, 0))
        machine.r[2] = MASK if (index + v[LANE]) % 3 == 1 else 0

    def contains(machine):
        volume = machine.bytes(machine.r[4], 16)
        index = machine.load(machine.r[5], 2)
        event(21, *struct.unpack("<4I", volume), index)
        machine.r[2] = int((index + v[LANE]) % 4 != 2)

    original_call = m.call

    def observe(address):
        nonlocal current_source
        if address == ENTRY:
            current_source = m.r[6]
            event(100, m.r[4], m.r[7], m.load(current_source + 0x4A, 2))
        return original_call(address)

    m.call = observe
    for address, service in ((0x800A1AF8, origin), (0x80077F14, position),
                             (0x800E6C34, trace), (0x8007FDD8, check),
                             (0x8007F230, roll), (0x80085390, effect),
                             (0x80085008, injuries), (0x800859B4, recovery),
                             (0x8008574C, cost), (0x800A1B9C, limb),
                             (0x800C1564, contains)):
        m.stub(address, service)
    m.follow.update((*ENTRIES[:4], 0x80049110, 0x8004137C, 0x80041B94))
    if v[F] < 3:
        m.r[4] = RESULT if v[F] == 0 else RESULT + 4
    elif v[F] == 3:
        m.r[4:8] = [23, RESULT, SOURCE, v[EXECUTE]]
    else:
        m.r[4:8] = [23, 0, v[TARGET], v[LIMB]]
        for i, arg in enumerate((RESULT,) if v[F] == 4 else (FROM, TO, RESULT)):
            m.put(DEFAULT_STACK + 16 + i * 4, arg)
    m.execute(ENTRIES[v[F]], limit=30000)
    stores = [(a, n) for a, n in m.stores if not DEFAULT_STACK - 0x1000 <= a < DEFAULT_STACK + 32]
    result = m.r[2] if v[F] >= 4 else 0
    values = [result, projection(m.bytes(RESULT, SIZE), v[F] >= 4),
              projection(m.bytes(SOURCE, SIZE), False),
              *(m.load(a) for a in (0x800F1A00, 0x800F1A04, 0x800F1A08)),
              len(calls), *calls]
    return {"values": values, "stores": stores}, v


def check(image: Image, v: list[int], observation: dict, native: list[int], reference: dict) -> list[str]:
    del image, v, reference
    expected = observation["values"]
    problems = [] if native == expected else [f"projection/state/callbacks differ: {native[:40]} != {expected[:40]}"]
    return problems + store_problems(observation["stores"],
                                    [(RESULT, RESULT + SIZE), (SOURCE, SOURCE + SIZE),
                                     (0x800F1A00, 0x800F1A0C)])


def describe(image: Image, results: list) -> list[str]:
    lines = []
    for index, (name, start, end) in enumerate(zip(FUNCTIONS, ENTRIES, ENDS, strict=True)):
        selected = [(v, o, n) for v, o, n in results if v[F] == index]
        lines.append(f"{name}: {len(selected)} vectors "
                     f"{sum(bool(check(image, v, o, n, {})) for v, o, n in selected)} mismatches "
                     f"[{start:08X},{end:08X}) {(end-start)//4} instructions SHA256={image.sha256(start, end)}")
    return lines + ["F00515/516 exclude formerly unspecified primary limb and focus from byte projection; "
                    "real original F00514/508/509/513 and bzero/atan2/sqrt followed. "
                    "Roll/prerequisite/effect/injury/geometry/cost services controlled, not gameplay. "
                    "Geometry compares nonnegative low32 squared lengths, including positive overflow; "
                    "negative words read before the original root table and are excluded."]
