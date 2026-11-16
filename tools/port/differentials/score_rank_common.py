"""Shared R3000 vectors and native driver protocol for B50-02 score rank policies.

MENUF.PRG _determineRank/_calculateScore (3B8.c:2774, 2801) and MENU9.PRG _determineCharacterRank/_setTitleFlags/
_calculateScore/_calculateProgress (238.c:2633, 2662, 2842, 418).
"""

from __future__ import annotations

import random

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MASK,
    MODULE_BASES,
    Machine,
    store_problems,
)

MENUF = "MENU/MENUF.PRG"
MENU9 = "MENU/MENU9.PRG"
MODULE_BASES[MENUF] = 0x80102800
MODULE_BASES[MENU9] = 0x80102800

SCOREDATA = 0x8005FEA0  # vs_main_scoredata
SCOREDATA_SIZE = 0x114
STATE_FLAGS = 0x80061598  # vs_main_stateFlags
STATE_FLAGS_SIZE = 0x400
ACTIONS = 0x8004B9DC  # vs_main_actions
ACTION_STRIDE = 0x34
ROOM_FLAGS = 0x8005FFD8  # vs_main_mapStatus.roomFlags
COUNTABLE_MASKS = 0x800E8508  # _countableRoomMask
SETTINGS = 0x80060000  # covers vs_main_settings.cursorMemory (0x80060021)
CURSOR_MEMORY = 0x80060021
MENU9_CURSOR_MEMORY = 0x800F1BF0  # vs_battle_menu9CursorMemory
BZERO = 0x80049110
MEMCPY = 0x800490B0
CAMERA_POSITION = 0x1F800034
CAMERA_ANGLES = 0x1F800054

# MENUF.PRG results statics.
RESULTS_RANK = 0x8010987C
RESULTS_CLEAR = 0x80109880
RESULTS_SCORE = 0x80109884
# MENU9.PRG score screen statics.
MENU_STATE = 0x8010A43C
HIDE_UI = 0x8010A45C
SCORE = 0x8010A460
CHARACTER_RANK = 0x8010A464
CLEAR_COUNT = 0x8010A466
OPENED_CHESTS = 0x8010A468
MAP_COMPLETION = 0x8010A46A
POSITION_BACKUP = 0x8010A480
ANGLES_BACKUP = 0x8010A490

STACK = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)
TITLES = SCOREDATA

# The flat vector, in driver protocol order.
LAYOUT = (
    ("mode", 1), ("titles", 1), ("kills", 6), ("attacks", 10), ("trials", 8), ("chain", 1), ("time", 1),
    ("map", 1), ("chests", 1), ("streak", 1), ("misc", 1), ("clear", 1), ("score", 1), ("rank", 1),
    ("bosses", 6), ("gold", 1), ("chest_key", 1), ("early", 1), ("late_low", 1), ("late_high", 1),
    ("rooms", 16), ("masks", 16), ("opened", 64), ("clear_byte", 1), ("cursor", 1),
)
SLICES = {}
_offset = 0
for _name, _count in LAYOUT:
    SLICES[_name] = slice(_offset, _offset + _count)
    _offset += _count
VECTOR_SIZE = _offset

# identifier: (mode, module, entry, end (exclusive), name)
SPECS = {
    "F03060": (0, MENUF, 0x80108A0C, 0x80108AB0, "MENUF.PRG _determineRank (3B8.c:2774)"),
    "F03061": (1, MENUF, 0x80108AB0, 0x80108E38, "MENUF.PRG _calculateScore (3B8.c:2801)"),
    "F02832": (2, MENU9, 0x80107600, 0x801076A4, "MENU9.PRG _determineCharacterRank (238.c:2633)"),
    "F02833": (3, MENU9, 0x801076A4, 0x80107A98, "MENU9.PRG _setTitleFlags (238.c:2662)"),
    "F02835": (4, MENU9, 0x80107C34, 0x80107FBC, "MENU9.PRG _calculateScore (238.c:2842)"),
    "F04157": (5, MENU9, 0x801031BC, 0x8010337C, "MENU9.PRG _calculateProgress (238.c:418)"),
}
OUTPUT_NAMES = {
    0: ("rank",),
    1: ("score",),
    2: ("rank",),
    3: ("titles",),
    4: ("score",),
    5: ("host order", "room flags word 1", "map completion", "opened chests", "clear count", "record map",
        "record chests", "titles", "score", "rank"),
}


def imm(word: int, value: int) -> int:
    return (word & 0xFFFF0000) | (value & 0xFFFF)


def slt(word: int) -> int:
    return word & ~1  # sltu (funct 0x2B) -> slt (0x2A)


def opcode(word: int, op: int) -> int:
    return (word & 0x03FFFFFF) | (op << 26)


LW, LH, LHU, LB, LBU, SB, SH, SW, BEQ, BNE = 0x23, 0x21, 0x25, 0x20, 0x24, 0x28, 0x29, 0x2B, 0x04, 0x05

# name: (address, original word, edit) per ID; describe() checks each original word against the image.
EDITS = {
    "F03060": {
        "F03060-counts-17-title-bits": (0x80108A38, 0x28620010, lambda w: imm(w, 0x11)),
        "F03060-signed-score-compare": (0x80108A80, 0x00E2102B, slt),
        "F03060-title-compare-reversed": (0x80108A6C, 0x00C2102A, lambda w: 0x0046102A),
        "F03060-stores-halfword": (0x80108A90, 0xAD03987C, lambda w: opcode(w, SH)),
        "F03060-skips-last-row": (0x80108A9C, 0x28620010, lambda w: imm(w, 0x0F)),
    },
    "F03061": {
        "F03061-chain-tier-6-inclusive": (0x80108C3C, 0x2C620006, lambda w: imm(w, 7)),
        "F03061-fast-clear-at-600": (0x80108DB8, 0x2C820258, lambda w: imm(w, 0x259)),
        "F03061-quarter-bonus-from-541": (0x80108DC0, 0x2C82021C, lambda w: imm(w, 0x21D)),
        "F03061-signed-clamp": (0x80108E20, 0x00C2102B, slt),
        "F03061-trial-points-20001": (0x80108C14, 0x24424E20, lambda w: imm(w, 0x4E21)),
        "F03061-kills-sign-extended": (0x80108B60, 0x94A30004, lambda w: opcode(w, LH)),
        "F03061-misc-not-added": (0x80108E18, 0x00431021, lambda w: 0x00401021),
        "F03061-clear-count-halfword": (0x80108D70, 0x8C449880, lambda w: opcode(w, LHU)),
    },
    "F02832": {
        "F02832-counts-17-title-bits": (0x8010762C, 0x28620010, lambda w: imm(w, 0x11)),
        "F02832-signed-score-compare": (0x80107674, 0x00E2102B, slt),
        "F02832-title-compare-reversed": (0x80107660, 0x00C2102A, lambda w: 0x0046102A),
        "F02832-stores-byte": (0x80107684, 0xA503A464, lambda w: opcode(w, SB)),
        "F02832-skips-last-row": (0x80107690, 0x28620010, lambda w: imm(w, 0x0F)),
    },
    "F02833": {
        "F02833-chests-from-51": (0x801076D8, 0x2C420034, lambda w: imm(w, 0x33)),
        "F02833-map-from-360": (0x801076FC, 0x2C420169, lambda w: imm(w, 0x168)),
        "F02833-asura-reads-ogre": (0x801077C0, 0x906200D1, lambda w: imm(w, 0xD0)),
        "F02833-trial-full-word": (0x801077E4, 0x3C0600FF, lambda w: imm(w, 0xFFFF)),
        "F02833-skips-action-34": (0x801078F8, 0x24080021, lambda w: imm(w, 0x22)),
        "F02833-late-actions-end-223": (0x801078C8, 0x286200E0, lambda w: imm(w, 0xDF)),
        "F02833-mastery-31-from-501": (0x80107A74, 0x2C4201F4, lambda w: imm(w, 0x1F5)),
        "F02833-veteran-kills-4999": (0x80107990, 0x2C421388, lambda w: imm(w, 0x1387)),
        "F02833-clear-title-ignored": (0x801076B0, 0x10400005, lambda w: 0x10000005),
    },
    "F02835": {
        "F02835-chain-tier-6-inclusive": (0x80107DC0, 0x2C620006, lambda w: imm(w, 7)),
        "F02835-fast-clear-at-600": (0x80107F3C, 0x2C820258, lambda w: imm(w, 0x259)),
        "F02835-quarter-bonus-from-541": (0x80107F44, 0x2C82021C, lambda w: imm(w, 0x21D)),
        "F02835-signed-clamp": (0x80107FA4, 0x00C2102B, slt),
        "F02835-trial-points-20001": (0x80107D98, 0x24424E20, lambda w: imm(w, 0x4E21)),
        "F02835-kills-sign-extended": (0x80107CE4, 0x94A30004, lambda w: opcode(w, LH)),
        "F02835-misc-not-added": (0x80107F9C, 0x00431021, lambda w: 0x00401021),
    },
    "F04157": {
        "F04157-aliases-bit-21": (0x801031E0, 0x3C020040, lambda w: imm(w, 0x20)),
        "F04157-cursor-clear-inverted": (0x80103200, 0x14600005, lambda w: opcode(w, BEQ)),
        "F04157-counts-31-room-bits": (0x8010326C, 0x28820020, lambda w: imm(w, 0x1F)),
        "F04157-counts-63-chests": (0x801032C8, 0x28A20040, lambda w: imm(w, 0x3F)),
        "F04157-signed-map-record": (0x801032F4, 0x0044102B, slt),
        "F04157-chest-record-not-raised": (0x80103320, 0xACC30098, lambda w: 0),
        "F04157-skips-rank": (0x80103360, 0x0C041D80, lambda w: 0),
        "F04157-clear-sign-extended": (0x801032E4, 0x90651598, lambda w: opcode(w, LB)),
    },
}


def mutants(identifier: str) -> dict:
    # Case discovery must not read disc images (#367): the patches derive from the recorded words.
    return {name: {address: edit(original) & MASK} for name, (address, original, edit) in EDITS[identifier].items()}


def verify_original_words(identifier: str, image) -> None:
    for name, (address, original, _edit) in EDITS[identifier].items():
        found = image.read(address)
        if found != original:
            raise SystemExit(f"{name}: {address:#x} holds {found:#010x}, expected {original:#010x}")


# ---------------------------------------------------------------------------------------------- vectors
def pick(rng: random.Random, edges: tuple[int, ...], bits: int = 32) -> int:
    choice = rng.randrange(4)
    if choice < 2:
        return rng.choice(edges)
    if choice == 2:
        return max(0, rng.choice(edges) + rng.randrange(-3, 4)) & ((1 << bits) - 1)
    return rng.getrandbits(bits)


def counts(rng: random.Random, size: int) -> list[int]:
    edges = (0, 1, 499, 500, 501, 4999, 5000, 5001, 0x7FFF, 0x8000, 0xFFFF)
    if rng.randrange(3) == 0:
        base = rng.choice((5000, 500, 0xFFFF, 4999))
        values = [base + rng.randrange(0, 3) for _ in range(size)]
        if rng.randrange(2):
            values[rng.randrange(size)] = rng.choice((4999, 499, 0))
        return [value & 0xFFFF for value in values]
    return [pick(rng, edges, 16) for _ in range(size)]


def trials(rng: random.Random) -> list[int]:
    cleared_all = rng.randrange(3) == 0
    words = []
    for _ in range(8):
        high = rng.getrandbits(8) << 24
        if cleared_all or rng.randrange(2):
            time = rng.choice((0, 1, 0x7FFFFF, 0x800001, 0xFFFFFF, rng.getrandbits(24)))
        else:
            time = 0x800000
        words.append(high | time)
    return words


def titles_word(rng: random.Random) -> int:
    count = rng.choice((0, 3, 7, 8, 11, 12, 15, 16, rng.randrange(17)))
    low = 0
    for bit in rng.sample(range(16), count):
        low |= 1 << bit
    return (rng.getrandbits(16) << 16 if rng.randrange(2) else 0) | low


RANK_SCORES = (100_000_000, 75_000_000, 60_000_000, 40_000_000, 32_000_000, 24_000_000, 16_000_000, 12_000_000,
               8_000_000, 5_000_000, 4_000_000, 3_000_000, 2_000_000, 1_000_000, 500_000, 0, 999_999_999,
               0x80000000, 0xFFFFFFFF)


def action_masks(rng: random.Random) -> tuple[int, int, int]:
    early = 0xFFFFFFFF if rng.randrange(4) else rng.getrandbits(32)
    choice = rng.randrange(4)
    if choice == 0:
        early &= ~(1 << (rng.choice((33, 36, 38, 39)) - 22))
    elif choice == 1:
        early &= ~(1 << rng.randrange(32))
    late = [0xFFFFFFFF, 0xFF] if rng.randrange(3) else [rng.getrandbits(32), rng.getrandbits(8)]
    if rng.randrange(3) == 0:
        index = rng.randrange(40)
        late[index // 32] &= ~(1 << (index % 32))
    return early & MASK, late[0] & MASK, late[1] & 0xFF


def vector_for(mode: int, rng: random.Random, number: int) -> list[int]:
    values = [0] * VECTOR_SIZE

    def put(name: str, items) -> None:
        values[SLICES[name]] = list(items)

    put("mode", [mode])
    put("titles", [titles_word(rng)])
    put("kills", counts(rng, 6))
    put("attacks", counts(rng, 10))
    put("trials", trials(rng))
    put("chain", [pick(rng, (0, 5, 6, 10, 11, 15, 16, 20, 21, 25, 26, 29, 30, 31, 50, 51, 0xFFFF), 16)])
    put("time", [pick(rng, (0, 299, 300, 539, 540, 599, 600, 0x80000000))])
    put("map", [pick(rng, (0, 5, 359, 360, 361, 362, 400, 512, 0x80000000, 0xFFFFFFFF))])
    put("chests", [pick(rng, (0, 3, 50, 51, 52, 53, 64, 0x80000000, 0xFFFFFFFF))])
    put("streak", [pick(rng, (0, 1, 999_999_999, 0x7FFFFFFF, 0xFFFFFFFF))])
    put("misc", [pick(rng, (0, 1, 999_999_999, 0x80000000))])
    if mode == 1:
        clear = pick(rng, (0, 1, 2, 42950, 0xFFFF, 0x10000, 0x10001, 0xFFFFFFFF, 0x80000000))
    else:
        clear = pick(rng, (0, 1, 2, 42950, 0xFFFF), 16)
    if number % 8 == 0:
        clear = 0
    put("clear", [clear])
    put("score", [pick(rng, RANK_SCORES)])
    put("rank", [rng.getrandbits(32) if mode == 0 else rng.getrandbits(16)])
    put("bosses", [rng.choice((0, 0, 1, 0x80, 0xFF)) for _ in range(6)])
    put("gold", [rng.choice((0, 1, 0x80))])
    put("chest_key", [rng.choice((0, 1, 0xFF))])
    put("early", [action_masks(rng)[0]])
    _, late_low, late_high = action_masks(rng)
    put("late_low", [late_low])
    put("late_high", [late_high])
    rooms = [rng.getrandbits(32) for _ in range(16)]
    if rng.randrange(2):
        rooms[1] |= 0x800000
    put("rooms", rooms)
    put("masks", [rng.choice((0xFFFFFFFF, rng.getrandbits(32))) for _ in range(16)])
    put("opened", [0 if rng.randrange(3) else rng.choice((1, 0x80, 0xFF)) for _ in range(64)])
    if rng.randrange(4) == 0:
        put("opened", [rng.choice((1, 0x80)) for _ in range(64)])
    put("clear_byte", [rng.choice((0, 1, 2, 0x7F, 0x80, 0xFF, rng.getrandbits(8)))])
    put("cursor", [rng.choice((0, 0, 1, 0x80))])
    return values


def field(values: list[int], name: str) -> list[int]:
    return values[SLICES[name]]


def one(values: list[int], name: str) -> int:
    return values[SLICES[name]][0]


def action_unlocked(values: list[int], index: int) -> bool:
    if 22 <= index < 54:
        return bool(one(values, "early") >> (index - 22) & 1)
    if 184 <= index < 216:
        return bool(one(values, "late_low") >> (index - 184) & 1)
    if 216 <= index < 224:
        return bool(one(values, "late_high") >> (index - 216) & 1)
    return False


# ---------------------------------------------------------------------------------------------- original
def place_world(machine: Machine, values: list[int], rng: random.Random) -> None:
    machine.map_zero(SCOREDATA, SCOREDATA_SIZE)
    machine.map_zero(ROOM_FLAGS, 0x48)
    machine.map_zero(STATE_FLAGS, STATE_FLAGS_SIZE)
    machine.map_zero(ACTIONS, 256 * ACTION_STRIDE)
    machine.map_zero(COUNTABLE_MASKS, 0x40)
    machine.map_zero(SETTINGS, 0x40)
    machine.store(SCOREDATA, bytes(rng.getrandbits(8) for _ in range(SCOREDATA_SIZE)))
    machine.put(TITLES, one(values, "titles"))
    for index, kills in enumerate(field(values, "kills")):
        machine.put(SCOREDATA + 4 + 2 * index, kills, 2)
    for index, attacks in enumerate(field(values, "attacks")):
        machine.put(SCOREDATA + 0x14 + 2 * index, attacks, 2)
    for index, word in enumerate(field(values, "trials")):
        machine.put(SCOREDATA + 0x28 + 0xC * index, word)
    machine.put(SCOREDATA + 0x88, one(values, "chain"), 2)
    machine.put(SCOREDATA + 0x90, one(values, "time"))
    machine.put(SCOREDATA + 0x94, one(values, "map"))
    machine.put(SCOREDATA + 0x98, one(values, "chests"))
    machine.put(SCOREDATA + 0x104, one(values, "streak"))
    machine.put(SCOREDATA + 0x108, one(values, "misc"))
    machine.store(STATE_FLAGS, bytes(rng.getrandbits(8) for _ in range(STATE_FLAGS_SIZE)))
    machine.store(STATE_FLAGS + 0xCC, bytes(field(values, "bosses")))
    machine.put(STATE_FLAGS + 0x383, one(values, "gold"), 1)
    machine.put(STATE_FLAGS + 0x387, one(values, "chest_key"), 1)
    machine.store(STATE_FLAGS + 0x3C0, bytes(field(values, "opened")))
    machine.put(STATE_FLAGS, one(values, "clear_byte"), 1)
    for index in range(224):
        word = rng.getrandbits(32) & ~0x8000 | (0x8000 if action_unlocked(values, index) else 0)
        machine.put(ACTIONS + ACTION_STRIDE * index + 0xC, word)
    for index, (room, mask) in enumerate(zip(field(values, "rooms"), field(values, "masks"), strict=True)):
        machine.put(ROOM_FLAGS + 4 * index, room)
        machine.put(COUNTABLE_MASKS + 4 * index, mask)
    machine.put(CURSOR_MEMORY, one(values, "cursor"), 1)


def run_original(machine: Machine, values: list[int]) -> tuple[dict, list[int]]:
    mode = one(values, "mode")
    identifier = next(key for key, spec in SPECS.items() if spec[0] == mode)
    entry = SPECS[identifier][2]
    rng = random.Random(hash(tuple(values)) & MASK)
    place_world(machine, values, rng)
    clear, score, rank = one(values, "clear"), one(values, "score"), one(values, "rank")
    if mode in (0, 1):
        machine.put(RESULTS_CLEAR, clear)
        machine.put(RESULTS_SCORE, score)
        machine.put(RESULTS_RANK, rank)
    else:
        machine.put(CLEAR_COUNT, clear, 2)
        machine.put(SCORE, score)
        machine.put(CHARACTER_RANK, rank, 2)
        machine.put(MENU_STATE, 7)
        machine.put(HIDE_UI, 1)
        machine.put(MAP_COMPLETION, 0xBEEF, 2)
        machine.put(OPENED_CHESTS, 0xCAFE, 2)
    calls: list[tuple] = []

    def bzero(m: Machine) -> None:
        calls.append(("bzero", m.r[4], m.r[5], len(m.stores)))

    def memcpy(m: Machine) -> None:
        calls.append(("memcpy", m.r[4], m.r[5], m.r[6], len(m.stores)))

    machine.stub(BZERO, bzero)
    machine.stub(MEMCPY, memcpy)
    machine.follow.update({0x801076A4, 0x80107C34, 0x80107600})
    machine.execute(entry)
    if mode == 0:
        outputs = [machine.load(RESULTS_RANK, 4, signed=True)]
    elif mode == 1:
        outputs = [machine.load(RESULTS_SCORE)]
    elif mode == 2:
        outputs = [machine.load(CHARACTER_RANK, 2)]
    elif mode == 3:
        outputs = [machine.load(TITLES)]
    elif mode == 4:
        outputs = [machine.load(SCORE)]
    else:
        outputs = [0, machine.load(ROOM_FLAGS + 4), machine.load(MAP_COMPLETION, 2), machine.load(OPENED_CHESTS, 2),
                   machine.load(CLEAR_COUNT, 2), machine.load(SCOREDATA + 0x94), machine.load(SCOREDATA + 0x98),
                   machine.load(TITLES), machine.load(SCORE), machine.load(CHARACTER_RANK, 2)]
    return {
        "outputs": outputs,
        "calls": calls,
        "reset": (machine.load(MENU_STATE), machine.load(HIDE_UI)) if mode == 5 else None,
        "stores": list(machine.stores),
    }, values


def progress_problems(values: list[int], observation: dict, native_order: int) -> list[str]:
    cursor_clear = one(values, "cursor") == 0
    problems = []
    if observation["reset"] != (0, 0):
        problems.append(f"original left menuState/hideUi {observation['reset']}")
    reset_stores = {MENU_STATE, HIDE_UI}
    expected_calls = []
    if cursor_clear:
        expected_calls.append(("bzero", MENU9_CURSOR_MEMORY, 8))
    expected_calls += [("memcpy", POSITION_BACKUP, CAMERA_POSITION, 16), ("memcpy", ANGLES_BACKUP, CAMERA_ANGLES, 16)]
    calls = observation["calls"]
    if [call[:-1] for call in calls] != expected_calls:
        problems.append(f"original calls {calls}")
    elif cursor_clear:
        before = {address for address, _ in observation["stores"][:calls[0][-1]]}
        if not reset_stores <= before:
            problems.append("original cleared the cursor memory before resetting the screen")
    expected_order = 123 if cursor_clear else 13
    if native_order != expected_order:
        problems.append(f"native host order {native_order}, expected {expected_order}")
    return problems


def allowed_stores(mode: int) -> list[tuple[int, int]]:
    if mode == 0:
        return [(RESULTS_RANK, RESULTS_RANK + 4)]
    if mode == 1:
        return [(RESULTS_SCORE, RESULTS_SCORE + 4), STACK]
    if mode == 2:
        return [(CHARACTER_RANK, CHARACTER_RANK + 2)]
    if mode == 3:
        return [(TITLES, TITLES + 4)]
    if mode == 4:
        return [(SCORE, SCORE + 4), STACK]
    return [(TITLES, TITLES + 4), (SCOREDATA + 0x94, SCOREDATA + 0x9C), (ROOM_FLAGS + 4, ROOM_FLAGS + 8),
            (MENU_STATE, MENU_STATE + 4), (HIDE_UI, HIDE_UI + 4), (SCORE, SCORE + 4),
            (CHARACTER_RANK, OPENED_CHESTS + 2), (MAP_COMPLETION, MAP_COMPLETION + 2), STACK]


def check_original(_image, values: list[int], observation: dict, native: list[int], _reference: dict) -> list[str]:
    mode = one(values, "mode")
    names = OUTPUT_NAMES[mode]
    if len(native) != len(names):
        return [f"invalid native protocol {native}"]
    problems = []
    original = observation["outputs"]
    for index, (name, mine, theirs) in enumerate(zip(names, native, original, strict=True)):
        if mode == 5 and index == 0:
            continue
        if mine != theirs:
            problems.append(f"{name}: native {mine:#x} original {theirs:#x}")
    if mode == 5:
        problems += progress_problems(values, observation, native[0])
    return problems + store_problems(observation["stores"], allowed_stores(mode))


def build_case(identifier: str) -> dict:
    mode, module, entry, end, name = SPECS[identifier]

    def vector(rng: random.Random, number: int) -> list[int]:
        return vector_for(mode, rng, number)

    def describe(image, results) -> list[str]:
        # The harness describes the unpatched image before any mutant runs.
        verify_original_words(identifier, image)
        lines = [f"{name}: executed {(end - entry) // 4} original instructions including the return delay slot."]
        if mode in (0, 2):
            rows = sorted({observation["outputs"][0] for _, observation, _ in results})
            lines.append(f"Compared the stored rank (prior rank preset); ranks seen: {rows}.")
        elif mode in (1, 4):
            clamped = sum(observation["outputs"][0] == 999_999_999 for _, observation, _ in results)
            lines.append(f"Compared the stored 32-bit score; {clamped} vectors clamped to 999999999.")
        elif mode == 3:
            bits = 0
            for _, observation, _ in results:
                bits |= observation["outputs"][0]
            lines.append(f"Compared the titles word; union of resulting bits {bits:#010x}.")
        else:
            cleared = sum(one(vector, "cursor") == 0 for vector, _, _ in results)
            lines.append("Compared room flag word 1, the map/chest/clear statics, the raised record, titles, score "
                         "and rank (following _setTitleFlags, _calculateScore and _determineCharacterRank), the "
                         f"bzero/memcpy arguments and host order; {cleared} vectors cleared the cursor memory.")
        lines.append("Every original store outside the function's outputs (and the stack) fails the vector.")
        return lines

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "MODULES": (module,),
        "DRIVER": "score_rank_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\Score\ScoreRank.cpp",),
        "VECTORS": 1024,
        "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 1024,
        "MUTANTS": mutants(identifier),
        "vector": vector,
        "run": run_original,
        "check": check_original,
        "describe": describe,
    }
