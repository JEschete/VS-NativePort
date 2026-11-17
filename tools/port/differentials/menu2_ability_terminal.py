"""Shared ability-terminal fixtures; only differential.Machine executes MIPS."""

from __future__ import annotations

import random

import menu2_ability_foundation as foundation
from differential import MODULE_BASES, Fault, Machine, s32

MODULES = (r"MENU\MENU2.PRG", r"BATTLE\BATTLE.PRG")
MODULE_BASES[MODULES[1]] = 0x80068800
BATTLE_SHA256 = "ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12"
SOURCES = (
    r"Native\Game\src\Menu\BattleAbilityAssignments.cpp",
    r"Native\Game\src\Menu\BattleAbilitiesTerminal.cpp",
)
DRIVER = "menu2_ability_terminal_driver.cpp"
SETTINGS = 0x80060020
CHAIN = 0x801050D0
DEFENSE = 0x801050E0
TEXT_POINTER = 0x80104EB4
HIGHLIGHTED = 0x80104EB8
MENU_STATE = 0x80104EBC
TYPE = 0x80104EC0
COUNT = 0x80104EC1
FIRST = 0x80104EC2
CURSOR_ANIMATION = 0x80104EC4
CHAIN_PHASE = 0x80104EC8
DEFENSE_PHASE = 0x80104ED0
ANIMATING = 0x801050C0
POINT_STEP = 0x801050C1
SHORTCUT = 0x800F4E6A
CURRENT_MENU = 0x800F51C0
DISPLAY_COST = 0x801022D4
REPEAT = 0x8005DFDC
ROW_ANIMATION = 0x800EBBC8
CHAIN_ORDER = 0x800EBDBC
DEFENSE_ORDER = 0x800EBDCC
TEXT = 0x80220000
ITEMS = 0x80230000
SCRATCH = 0x80250000
PHASE = 0x80260000
ITEM_STRIDE = 128
ITEM_FIELDS = ((0, 1, False), (4, 1, False), (5, 1, False), (6, 1, False),
               (24, 2, True), (20, 2, True), (22, 2, True), (1, 1, False))


def settings() -> dict:
    return {
        "MODULES": MODULES, "SOURCES": SOURCES, "DRIVER": DRIVER,
        "INCLUDES": (r"Native\Tests\include",), "COMPILER": "clang-cl",
        "CXX_STANDARD": "c++latest", "VECTORS": 1024, "MUTANT_VECTORS": 256,
    }


def setup(machine: Machine, seed: int) -> None:
    foundation.validate(machine)
    machine.map_zero(SETTINGS, 32)
    for index in range(32):
        machine.put(SETTINGS + index, seed + index * 13, 1)
    for index in range(16):
        machine.put(CHAIN + index, seed + index * 17, 1)
        machine.put(DEFENSE + index, seed + 61 + index * 13, 1)


def mapping_values(machine: Machine) -> list[int]:
    return [machine.load(SETTINGS + 12 + index, 1) for index in range(8)]


def sound_stubs(machine: Machine, calls: list[list[int]], side_effect=None) -> None:
    def sound(m: Machine, kind: int) -> None:
        calls.append([kind, m.r[4], m.r[5]] if kind == 2 else [kind])
        if side_effect:
            side_effect(m, kind)

    machine.stub(0x800C02E8, lambda m: sound(m, 1))
    machine.stub(0x80045988, lambda m: sound(m, 2))
    machine.stub(0x800C02F8, lambda m: sound(m, 3))


def assignment_vector(rng: random.Random, number: int) -> list[int]:
    return [24 + number // 4 % 2, number // 8 % 3, number // 24 % 16,
            rng.getrandbits(32), number % 4, int(number % 7 == 0)]


def run_assignment(machine: Machine, value: list[int], entry: int) -> tuple[dict, list[int]]:
    kind, button, selected, seed, mode, nested = value
    setup(machine, seed)
    base = SETTINGS + (12 if kind == 24 else 16)
    action = machine.load((CHAIN if kind == 24 else DEFENSE) + selected, 1)
    for slot in range(3):
        machine.put(base + slot, action if mode & 1 else action + slot + 1, 1)
    if mode & 2:
        machine.put(base + button, action + 73, 1)
    calls = []

    def sound_effect(m: Machine, sound: int) -> None:
        if nested and sound == 2:
            m.put(base + 2, action, 1)

    sound_stubs(machine, calls, sound_effect)
    machine.r[4:7] = [kind, button, selected]
    machine.execute(entry, limit=1024)
    return foundation.observation(
        machine, mapping_values(machine), calls, [(SETTINGS + 12, SETTINGS + 20)]
    ), [0, *value]


def assignments_vector(rng: random.Random, number: int) -> list[int]:
    # Each navigation gate occurs inside the first mutant sample for both kinds.
    cases = (
        (1, 0, 0, 0, 0, 0), (8, 0, 0, 7, 0, 0),
        (16, 0, 0, 7, 0, 0), (16, 2, 1, 0, 0, 0),
        (8, 0, 3, 16, 0, 0), (9, 0, 4, 16, 0, 0),
        (9, 0, 3, 10, 0, 0), (11, 2, 3, 17, 0xF0, 0x5000),
        (8, 0, 1, 17, 0x10, 0), (16, 3, 2, 17, 0x80, 0),
        (1, 0, 0, 17, 0, 0x1000), (8, 0, 0, 17, 0, 0x1000),
        (8, 0, 7, 17, 0, 0x4000), (8, 0, 3, 17, 0, 0x5000),
        (16, 0, 0, 17, 0, 0x1000), (16, 0, 0, 17, 0x1000, 0x1000),
        (16, 8, 7, 17, 0, 0x4000), (16, 8, 7, 17, 0x4000, 0x4000),
        (16, 2, 1, 17, 0, 0x1000), (16, 2, 3, 17, 0, 0x1000),
        (16, 0, 4, 17, 0, 0x1000), (16, 2, 6, 17, 0, 0x4000),
        (16, 8, 3, 17, 0, 0x4000), (16, 0, 3, 17, 0, 0x4000),
        (9, 0, 6, 17, 0x20, 0x4000), (8, 0, 2, 17, 0xB0, 0),
        (7, 0, 4, 17, 0x90, 0), (16, 1, 1, 17, 0, 0x5000),
        (9, 1, 7, 17, 0x5000, 0x5000), (16, 7, 6, 17, 0, 0x5000),
        (5, 0, 3, 17, 0x40, 0x5000), (16, 8, 2, 18, 0x20, 0),
    )
    count, first, highlighted, phase, pressed, repeat = cases[number % len(cases)]
    pressed |= rng.getrandbits(16) << 16
    return [count, 24 + number // 32 % 2, first, highlighted, phase, pressed,
            repeat, int(number % 32 != 4), rng.getrandbits(32),
            rng.getrandbits(32), int(number % 11 == 0)]


def setup_items(machine: Machine, seed: int) -> None:
    machine.map_zero(ITEMS, 8 * ITEM_STRIDE)
    for row in range(8):
        item = ITEMS + row * ITEM_STRIDE
        values = [seed + row, seed + row * 19, row % 3, row % 2,
                  700 + row, 100 + row, 200 + row, 90 + row]
        for (offset, size, _), value in zip(ITEM_FIELDS, values, strict=True):
            machine.put(item + offset, value, size)
        machine.put(item + 32, 500 + row)


def item_values(machine: Machine) -> list[int]:
    values = []
    for row in range(8):
        item = ITEMS + row * ITEM_STRIDE
        values.extend(machine.load(item + offset, size, signed)
                      for offset, size, signed in ITEM_FIELDS)
        values.append(machine.load(item + 32))
    return values


def run_assignments(machine: Machine, value: list[int], entry: int) -> tuple[dict, list[int]]:
    count, kind, first, highlighted, phase, pressed, repeat, ready, cursor, seed, nested = value
    setup(machine, seed)
    setup_items(machine, seed)
    machine.map_zero(TEXT, 16 * 128)
    for address, number, size in (
        (TEXT_POINTER, TEXT, 4), (HIGHLIGHTED, highlighted, 4), (MENU_STATE, phase, 4),
        (TYPE, kind, 1), (COUNT, count, 1), (FIRST, first, 1),
        (CURSOR_ANIMATION, cursor, 4), (foundation.INPUT, pressed, 4),
        (REPEAT, repeat, 4), (0x1F800008, SCRATCH, 4),
        (DISPLAY_COST, seed, 1),
    ):
        machine.put(address, number, size)
    machine.map_zero(foundation.MENU_MEMORY, 64)
    for index in range(64):
        machine.put(foundation.MENU_MEMORY + index, seed + index * 7, 1)
    for offset, learned in ((12, CHAIN), (16, DEFENSE)):
        bound_rows = (0, 3, 7) if offset == 12 else (1, 4, 6)
        for button, selected_row in enumerate(bound_rows):
            row = min(selected_row, count - 1)
            action = machine.load(learned + first + row, 1)
            machine.put(SETTINGS + offset + button, action, 1)
    calls = []

    def get_item(m: Machine) -> None:
        row = m.r[4]
        if row < 20 or row >= 28:
            raise Fault("assignment requested a row outside 20..27")
        calls.append([5, row])
        m.r[2] = ITEMS + (row - 20) * ITEM_STRIDE

    def set_item(m: Machine) -> None:
        row, x, y, width = m.r[4:8]
        if row < 20 or row >= 28:
            raise Fault("assignment created a row outside 20..27")
        background, text = m.load(m.r[29] + 16), m.load(m.r[29] + 20)
        if text < TEXT or text >= TEXT + count * 128 or (text - TEXT) % 128:
            raise Fault("assignment name is outside its complete row owner")
        offset = (text - TEXT) // 2
        calls.append([4, row, x, y, width, background, offset])
        item = ITEMS + (row - 20) * ITEM_STRIDE
        for field, size, _ in ITEM_FIELDS:
            m.put(item + field, 0, size)
        for field, number, size in ((0, 1, 1), (20, x, 2), (22, y, 2),
                                     (1, width, 1), (32, offset, 4)):
            m.put(item + field, number, size)
        m.r[2] = item

    def ready_call(m: Machine) -> None:
        calls.append([7])
        m.r[2] = ready

    def draw_cursor(m: Machine) -> None:
        calls.append([10, m.r[4], m.r[5]])
        m.r[2] = (seed ^ 0x81234567) & 0xFFFFFFFF

    def risk(m: Machine) -> None:
        calls.append([11, m.r[4]])
        m.put(DISPLAY_COST, 1, 1)

    def icon(m: Machine) -> None:
        if m.r[7] != SCRATCH + 8:
            raise Fault("assignment button uses the wrong ordering-table offset")
        calls.append([8, *m.r[4:7]])

    def sound_effect(m: Machine, sound: int) -> None:
        if nested and sound == 2:
            m.put(foundation.INPUT, m.load(foundation.INPUT) | 0x40)

    sound_stubs(machine, calls, sound_effect)
    machine.stub(0x800C8E48, get_item)
    machine.stub(0x800C8E5C, set_item)
    machine.stub(0x800FFC04, lambda m: calls.append([6, (m.r[4] - TEXT) // 2]))
    machine.stub(0x800FA9D0, ready_call)
    machine.stub(0x800FFC68, icon)
    machine.stub(0x80043C60, lambda m: calls.append([9, m.r[4]]))
    machine.stub(0x800CCD40, draw_cursor)
    machine.stub(0x80102864, risk)
    machine.follow.add(0x80102B5C)
    machine.execute(entry, limit=10000)
    values = [
        machine.r[2], machine.load(MENU_STATE), machine.load(HIGHLIGHTED),
        machine.load(FIRST, 1), machine.load(CURSOR_ANIMATION),
        int(machine.load(TEXT_POINTER) != 0), machine.load(DISPLAY_COST, 1),
        *mapping_values(machine),
        *(machine.load(foundation.MENU_MEMORY + i, 1) for i in range(64)),
        *item_values(machine),
    ]
    allowed = [(TEXT_POINTER, TEXT_POINTER + 4), (HIGHLIGHTED, HIGHLIGHTED + 4),
               (MENU_STATE, MENU_STATE + 4), (FIRST, FIRST + 1),
               (CURSOR_ANIMATION, CURSOR_ANIMATION + 4), (DISPLAY_COST, DISPLAY_COST + 1),
               (SETTINGS + 12, SETTINGS + 20),
               (foundation.MENU_MEMORY + kind * 2, foundation.MENU_MEMORY + kind * 2 + 2)]
    allowed.extend((ITEMS + row * ITEM_STRIDE, ITEMS + row * ITEM_STRIDE + 7)
                   for row in range(8))
    allowed.extend((ITEMS + row * ITEM_STRIDE + 24, ITEMS + row * ITEM_STRIDE + 26)
                   for row in range(8))
    return foundation.observation(machine, values, calls, allowed), [1, *value]


def list_vector(rng: random.Random, number: int) -> list[int]:
    mode = number % 8
    mask = (0, 1, 1 << 13, 0x3FFF, 0x1555, 0x2AAA, rng.getrandbits(14), 3)[mode]
    return [int(mode == 0), (1, 0, 0, 0, 1, 1, 99, 0)[mode],
            int(mode == 3), int(mode != 7), mask, int(mode == 5),
            rng.getrandbits(32), number % 15]


def run_list(machine: Machine, value: list[int], entry: int, kind: int) -> tuple[dict, list[int]]:
    initialize, phase, shortcut, ready, mask, result, seed, high_slot = value
    setup(machine, seed)
    battle = machine.images[1]
    if battle.sha256(battle.base, battle.base + len(battle.data)) != BATTLE_SHA256:
        raise Fault("learned-action producers are not the reviewed BATTLE image")
    foundation.setup(machine, seed)
    phase_address = CHAIN_PHASE if kind == 0 else DEFENSE_PHASE
    machine.put(phase_address, phase)
    machine.put(SHORTCUT, shortcut, 1)
    order_address = CHAIN_ORDER if kind == 0 else DEFENSE_ORDER
    if high_slot < 14:
        machine.put(order_address + high_slot,
                    machine.load(order_address + high_slot, 1) | 0x80, 1)
    orders = [machine.load(address + index, 1)
              for address in (CHAIN_ORDER, DEFENSE_ORDER) for index in range(16)]
    for index in range(14):
        action = machine.load(order_address + index, 1)
        flags = machine.load(foundation.ACTIONS + action * foundation.ACTION_STRIDE + 12)
        machine.put(foundation.ACTIONS + action * foundation.ACTION_STRIDE + 12,
                    flags | (0x8000 if mask & (1 << index) else 0))
    calls = []

    def ready_call(m: Machine) -> None:
        calls.append([7])
        m.r[2] = ready

    def create(m: Machine) -> None:
        count, menu_type, rows = m.r[4:7]
        if count > 14:
            raise Fault("learned list exceeds the fourteen producer slots")
        call = [21, count, menu_type]
        call.extend(m.load(rows + index * 4) for index in range(count * 2))
        calls.append(call)
        m.put(phase_address, 81)
        m.r[2] = seed & 1

    def assignments(m: Machine) -> None:
        calls.append([22])
        m.r[2] = result

    machine.stub(0x800FA92C, lambda m: calls.append([20, m.r[4], m.r[5]]))
    machine.stub(0x800FA9D0, ready_call)
    machine.stub(0x80102928, create)
    machine.stub(0x80102CAC, assignments)
    machine.stub(0x800FA8E0, lambda m: calls.append([23, m.r[4]]))
    machine.r[4] = initialize
    machine.execute(entry, limit=5000)
    values = [machine.r[2], machine.load(phase_address),
              *(machine.load(address + i, 1) for address in (CHAIN, DEFENSE)
                for i in range(16))]
    return foundation.observation(
        machine, values, calls,
        [(phase_address, phase_address + 4), (CHAIN if kind == 0 else DEFENSE,
                                             (CHAIN if kind == 0 else DEFENSE) + 14)]
    ), [2 + kind, *value, *orders]


def points_vector(rng: random.Random, number: int) -> list[int]:
    boundaries = (0, 1, 9, 10, 99, 100, 999, 1000, 9999, 10000, 32767, 65535)
    requirement = boundaries[number % len(boundaries)]
    kills = (0, max(0, requirement - 1), requirement, 65535)[number // 12 % 4]
    return [number % 23, kills, requirement, s32(rng.getrandbits(32))]


def run_points(machine: Machine, value: list[int], entry: int) -> tuple[dict, list[int]]:
    count, kills, requirement, offset = value
    foundation.setup(machine, 0)
    machine.put(foundation.PROGRESS + 32, count, 2)
    machine.put(foundation.PROGRESS + 34, kills, 2)
    machine.put(foundation.ABILITY_REQUIREMENTS + count * 2, requirement, 2)
    calls = []

    def decimal(m: Machine) -> None:
        value = m.r[4]
        m.r[2] = value % 10 | (value // 10) << 4

    def draw(m: Machine) -> None:
        text = []
        for index in range(16):
            byte = m.load(m.r[4] + index, 1)
            if byte == 0:
                calls.append([40, m.r[5], m.r[6], len(text), *text])
                return
            text.append(byte)
        raise Fault("ability points text is not terminated within sixteen bytes")

    machine.stub(0x800CCC54, decimal)
    machine.stub(0x800C6828, draw)
    machine.r[4] = offset & 0xFFFFFFFF
    machine.execute(entry, limit=2048)
    return foundation.observation(machine, [], calls, []), [4, *value]


def screen_vector(rng: random.Random, number: int) -> list[int]:
    phase = (3, 3, 3, 4, 5, 5, 5, 5, 6, 6, 7, 7, 8, 9, 0, 255)[number % 16]
    shortcut = (0, 7, 8)[number // 16 % 3]
    ready = number // 3 % 4
    selected = (-3, -2, -1, 0, 1, 2)[number // 5 % 6]
    return [phase, number % 2, number // 2 % 11, shortcut, ready, selected,
            number // 16 % 2, number // 7 % 2, number // 11 % 2,
            rng.randrange(256), rng.getrandbits(32), number // 3 % 4]


def run_screen(machine: Machine, value: list[int], entry: int) -> tuple[dict, list[int]]:
    phase, animating, points, shortcut, ready, selected, result, chain, defense, cursor, seed, nested = value
    setup(machine, seed)
    for address, number, size in (
        (PHASE, phase, 1), (ANIMATING, animating, 1), (POINT_STEP, points, 1),
        (SHORTCUT, shortcut, 1), (SETTINGS + 1, cursor, 1),
        (CURRENT_MENU, seed, 1), (ITEMS + 6, seed & 1, 1),
        (MENU_STATE, seed ^ 0x12345678, 4),
    ):
        machine.put(address, number, size)
    for index in range(36):
        machine.put(ROW_ANIMATION + index, seed + index * 13, 1)
    calls = []
    ready_index = 0

    def ready_call(m: Machine) -> None:
        nonlocal ready_index
        calls.append([7])
        m.r[2] = (ready >> min(ready_index, 1)) & 1
        ready_index += 1

    def set_item(m: Machine) -> None:
        calls.append([4, *m.r[4:8], m.load(m.r[29] + 16), m.load(m.r[29] + 20)])
        m.r[2] = ITEMS

    def has_learned(m: Machine) -> None:
        calls.append([31, m.r[4]])
        m.r[2] = defense if m.r[4] else chain

    def create(m: Machine) -> None:
        calls.append([32, m.r[4], m.r[5], m.load(SETTINGS + 1, 1),
                      *(m.load(m.r[6] + index * 4) for index in range(4)),
                      *(m.load(m.r[7] + index * 4) for index in range(2))])
        m.put(SETTINGS + 1, seed + 99, 1)
        m.put(CURRENT_MENU, seed + 77, 1)

    def selection(m: Machine) -> None:
        calls.append([33])
        m.r[2] = selected & 0xFFFFFFFF

    def update_list(m: Machine, kind: int) -> None:
        calls.append([34, kind, m.r[4]])
        if nested:
            m.put(CURRENT_MENU, seed + 19, 1)
            m.put(MENU_STATE, seed ^ 0xABCDEF01)
        if nested & 2:
            m.put(SHORTCUT, (0, 7, 8)[seed % 3], 1)
        m.r[2] = result

    def draw_points(m: Machine) -> None:
        calls.append([37, m.r[4]])
        if nested == 3:
            m.put(POINT_STEP, seed % 11, 1)

    machine.stub(0x800FA9D0, ready_call)
    machine.stub(0x800FFBC8, lambda m: calls.append([30]))
    machine.stub(0x800C8E5C, set_item)
    machine.stub(0x800CAFB4, has_learned)
    machine.stub(0x801005E0, create)
    machine.stub(0x801008C8, selection)
    machine.stub(0x801034FC, lambda m: update_list(m, 0))
    machine.stub(0x80103670, lambda m: update_list(m, 1))
    machine.stub(0x800FA8E0, lambda m: calls.append([23, m.r[4]]))
    machine.stub(0x800FFBA8, lambda m: calls.append([35]))
    machine.stub(0x800FFA88, lambda m: calls.append([36, m.r[4]]))
    machine.stub(0x801037E4, draw_points)
    machine.r[4] = PHASE
    machine.execute(entry, limit=3000)
    values = [machine.r[2], machine.load(PHASE, 1), machine.load(ANIMATING, 1),
              machine.load(POINT_STEP, 1), machine.load(CURRENT_MENU, 1),
              machine.load(SETTINGS + 1, 1), machine.load(SHORTCUT, 1),
              machine.load(ITEMS + 6, 1), machine.load(MENU_STATE)]
    allowed = [(PHASE, PHASE + 1), (ANIMATING, POINT_STEP + 1),
               (CURRENT_MENU, CURRENT_MENU + 1), (SETTINGS + 1, SETTINGS + 2),
               (ITEMS + 6, ITEMS + 7)]
    return foundation.observation(machine, values, calls, allowed), [5, *value]


check = foundation.check


def describe(image, results) -> list[str]:
    calls = {call[0] for _, observed, _ in results for call in observed["calls"]}
    unique = len({tuple(value) for value, _, _ in results})
    return [f"canonical MENU2.PRG sha256={foundation.MODULE_SHA256}",
            f"distinct input vectors={unique}; callback kinds={sorted(calls)}",
            "actual original instructions; external presentation/allocator boundaries stubbed"]
