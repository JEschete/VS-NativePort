"""Asset-free case metadata and complete original MENUD dialog execution fixtures."""

from __future__ import annotations

import random
from functools import partial

from differential import (
    DEFAULT_STACK,
    DISC,
    MASK,
    MODULE_BASES,
    Fault,
    s16,
    s32,
    store_problems,
)

MODULES = (r"MENU\MENUD.PRG", r"BATTLE\BATTLE.PRG")
MODULE_BASES.update({MODULES[0]: 0x80102800, MODULES[1]: 0x80068800})
RANGES = {
    2969: (0x80105ACC, 0x80105D24),
    2970: (0x80105D24, 0x80106464),
    2983: (0x801071D8, 0x801072B0),
    2984: (0x801072B0, 0x80107490),
}
FIELDS = (
    "command",
    "sort_step",
    "sort_category",
    "discard_step",
    "category",
    "selected",
    "kind",
    "place",
    "quantity",
    "maximum",
    "transfer_step",
    "transfer_result",
    "direction",
    "ready",
    "done",
    "action",
    "pressed",
    "repeated",
    "cursor_result",
    "list_row",
    "stack_count",
    "controls",
    "frozen",
    "animation",
)
CAPACITIES = (32, 64, 64, 32, 64, 192, 256)
CRITERIA_COUNTS = (24, 24, 7, 24, 23, 16, 2)
STATE = 0x80109A4C
TEMPLATE = 0x801099F8
OWNER = 0x80700000
HELP = 0x80800000
MENU_TEXT = 0x8010952C
ROWS = 0x80600000
MEMORY = 0x800F4EE8
LIST_ROW = 0x801023D0
CONTROLS = 0x80109A7D
FROZEN = 0x801023E3
PRESSED = 0x8005E1D0
REPEATED = 0x8005DFDC
ROW_BYTES = 64
MEMORY_BYTES = 242
FIELD_BYTES = (0, 0x14, 0x18, 6, 7)
RECORD_OFFSETS = (0, 0xA00, 0x1500, 0x400, 0x1900, 0x2300, 0x3800)
RECORD_SIZES = (32, 44, 16, 48, 40, 28, 4)
# Fixed maps audited against original-assembly.log. Import/configure never reads
# an image; CI's case list runs with Image.load explicitly denied (#367).
MUTANTS = {
    2969: {
        "entry-not-minus-one": {0x80105AE4: 0x24840000},
        "entry-stage-not-cleared": {0x80105AF4: 0xA0649A4C},
        "name-table-next-entry": {0x80105B88: 0x94420370},
        "description-next-entry": {0x80105BAC: 0x94420372},
        "wrong-menu-memory": {0x80105BF0: 0x24A50061},
        "wrong-display-stage": {0x80105BF8: 0x24020002},
        "waiting-action-not-ignored": {0x80105C1C: 0},
        "select-sound-is-leave": {0x80105C2C: 0x0C0300BA},
        "weapon-route-is-misc": {0x80105C80: 0x0C04157C},
        "shield-route-is-weapon": {0x80105CA0: 0x0C04147E},
        "misc-route-is-shield": {0x80105CC0: 0x0C041515},
        "equipment-category-is-zero": {0x80105CD0: 0x24040000},
    },
    2970: {
        "entry-selected-not-minus-one": {0x80105D68: 0x24A20000},
        "entry-category-mask": {0x80105D5C: 0x30420003},
        "discard-text-next-entry": {0x80105D88: 0x94440456},
        "information-done-wrong-bit": {0x80105DEC: 0x000217C2},
        "single-multiple-kind-bit": {0x80105E14: 0x30420002},
        "wrong-discard-menu": {0x80105E9C: 0x24050005},
        "maximum-reads-provenance-byte": {0x80105ED4: 0x90633803},
        "maximum-units-wrong-slot": {0x80105F00: 0xA2020007},
        "wrong-quantity-row": {0x80105F34: 0x24040023},
        "quantity-row-wrong-state": {0x80105F58: 0x24030002},
        "ascii-space-instead-of-glyph": {0x80105FF0: 0x24030020},
        "quantity-confirm-only": {0x80106048: 0x30420020},
        "triangle-loses-cancel-priority": {0x801060B0: 0x30420040},
        "zero-quantity-accepted": {0x801060E8: 0x10000005},
        "quantity-up-ten-not-one": {0x80106160: 0x90420001},
        "quantity-upper-limit-missing": {0x801061A4: 0},
        "left-max-at-wrong-place": {0x801061F0: 0x24020001},
        "wrong-quantity-font": {0x80106284: 0x24040004},
        "units-x-shifted": {0x801062A0: 0x24050041},
        "cursor-y-shifted": {0x80106318: 0x3C020073},
        "fixed-confirmation-text-shifted": {0x80106354: 0x250A5C94},
        "full-stack-equality-inverted": {0x8010640C: 0x10620007},
        "quantity-subtraction-is-addition": {0x80106410: 0x00621021},
    },
    2983: {
        "row-construction-stops-early": {0x80107218: 0x2C42000F},
        "first-row-off-by-one": {0x80107224: 0x24100015},
        "wrong-slide-state": {0x80107228: 0x24130003},
        "wrong-restored-state": {0x8010722C: 0x24120000},
        "cursor-memory-previous-menu": {0x80107238: 0x26220050},
        "wrong-restored-x-member": {0x80107260: 0x94620016},
        "selected-row-offset": {0x80107274: 0x24420013},
        "last-row-omitted": {0x80107288: 0x2A020027},
        "selected-stored-into-disabled": {0x80107284: 0xA0620007},
    },
    2984: {
        "wrong-controls-mode": {0x801072D4: 0x24030004},
        "freeze-not-released": {0x801072E0: 0},
        "selected-result-only-byte": {0x801072E8: 0xA0609A58},
        "wrong-transfer-description": {0x8010735C: 0x2442000C},
        "wrong-first-answer": {0x80107348: 0x24050012},
        "wrong-transfer-menu": {0x801073B4: 0x24050004},
        "wrong-input-stage": {0x801073C8: 0x24020002},
        "result-plus-two": {0x801073E4: 0x24420002},
        "second-flyout-wrong-row": {0x801073FC: 0x24040002},
        "no-remap-is-wrong-result": {0x8010743C: 0x24020003},
        "waiting-result-requires-not-ready": {0x80107464: 0x14400004},
    },
}


def configure(namespace: dict, identifier: int) -> None:
    """Install asset-free per-ID metadata and runtime functions for the shared CLI."""
    namespace.update(
        ID=f"F{identifier:05}",
        ENTRY=RANGES[identifier][0],
        END=RANGES[identifier][1],
        MODULES=MODULES,
        DRIVER="container_menu_dialogs_driver.cpp",
        INCLUDES=(r"Native\Tests\include",),
        SOURCES=(
            r"Native\Game\src\Menu\Container\ContainerMenuDialogs.cpp",
            r"Native\Game\src\Items\ContainerIndexing.cpp",
            r"Native\Game\src\Menu\Framework\ItemRows.cpp",
            r"Native\Game\src\Items\EquipmentStats.cpp",
            r"Native\Engine\src\Core\Math\ScalarMath.cpp",
            r"Native\Engine\src\Core\Math\Distance.cpp",
            r"Native\Engine\src\Render\EncodedText.cpp",
        ),
        VECTORS=1024 if identifier == 2969 else 512,
        MUTANT_VECTORS=1024 if identifier == 2969 else 512,
        SEED=identifier,
        MUTANTS=MUTANTS[identifier],
        vector=partial(vector, identifier),
        run=partial(run, identifier),
        check=check,
    )


def vector(identifier: int, rng: random.Random, number: int) -> list[int]:
    """Generate fixed-seed branch, byte, slot, input-priority and word-wrap boundaries."""
    category = number % 7
    category_word = category
    if identifier == 2983:
        category_word = (0, 7, -81, -80, -1, 39, 0x7FFFFFFF, -0x80000000)[number % 8]
    sort_category = number // 5 % 7
    sort_step = (0, 0, 1, 1, 255)[number % 5]
    discard_step = number % 8
    values = (0, 1, 2, 9, 10, 99, 100, 127, 128, 200, 254, 255)
    selected = (0, CAPACITIES[category] - 1)[number // 7 % 2]
    action = (-1, -2, -3, 0, 1, -0x80000000, 0x7FFFFFFF)[number // 8 % 7]
    if identifier == 2969 and sort_step == 1:
        action = (
            number // 35 % CRITERIA_COUNTS[sort_category]
            if number % 5 == 2
            else (-1, -2, -3, -0x80000000)[number // 35 % 4]
        )
    command = 0
    if number % 11 == 0:
        if identifier == 2969:
            command = (0x7F000000 if number % 22 else 0) | (sort_category + 1)
        elif identifier == 2970:
            command = (number % 3 << 16) | (category << 12) | (selected + 1)
        elif identifier == 2984:
            command = (-1, 1, 0x7FFFFFFF)[number // 11 % 3]
    if identifier == 2983:
        command = category_word
    state = [
        command,
        sort_step,
        sort_category,
        discard_step,
        category,
        selected,
        number % 3,
        number // 8 % 3,
        values[number // 3 % len(values)],
        values[number // 5 % len(values)],
        number % 4,
        s32(rng.getrandbits(32)),
        number % 2,
        number // 2 % 2,
        number // 4 % 2,
        action,
        (0, 0x10, 0x20, 0x40, 0x70, 0x30, 0x60, 0x80)[number // 8 % 8],
        (0, 0x1000, 0x4000, 0x5000, 0x8000, 0x2000, 0xA000, 0x9000, 0x6000)[
            number // 16 % 9
        ],
        s32(rng.getrandbits(32)),
        (0, 14, 15, 16, 255)[number % 5],
        values[number // 7 % len(values)],
        rng.getrandbits(8),
        rng.getrandbits(8),
        rng.getrandbits(8),
    ]
    if identifier == 2970:
        state[FIELDS.index("ready")] = number // 16 % 2
        state[FIELDS.index("done")] = number // 8 % 2
        if number % 64 == 5:
            for name, value in (
                ("place", 0 if number // 64 % 2 == 0 else 2),
                ("quantity", 0),
                ("maximum", 255),
                ("pressed", 0),
                ("repeated", 0x1000 if number // 64 % 2 == 0 else 0x8000),
            ):
                state[FIELDS.index(name)] = value
    rows = [
        value
        for row in range(40)
        for value in (
            row % 6,
            s16(rng.getrandbits(16)),
            s16(rng.getrandbits(16)),
            rng.getrandbits(8),
            rng.getrandbits(8),
        )
    ]
    memory = [rng.getrandbits(8) for _ in range(MEMORY_BYTES)]
    offset = s32(s32(category_word + 81) * 2)
    if 0 <= offset < MEMORY_BYTES:
        memory[offset] = number % 24
    return [*state, *rows, *memory]


def record_locations(v: dict) -> list[tuple[int, int, int]]:
    """Select actual source records and the last attached component records."""
    category, selected = v["category"], v["selected"]
    slots = [
        selected if category == 0 else 0,
        selected if category == 1 else 63,
        selected if category == 2 else 63,
        selected if category == 3 else 0,
        selected if category == 4 else 63,
        selected if category == 5 else 191,
        selected,
    ]
    return [
        (RECORD_OFFSETS[i] + slots[i] * RECORD_SIZES[i], RECORD_SIZES[i], slots[i])
        for i in range(7)
    ]


def initialize_records(machine, v: dict) -> list[tuple[int, int, int]]:
    """Populate real disjoint container tables with valid 1-based last-slot attachments."""
    machine.map_zero(OWNER, 0x3C00)
    locations = record_locations(v)
    for category, (offset, _, slot) in enumerate(locations):
        base = OWNER + offset
        index_offsets = (0, 43, 14, 0, 39, 26, 3)
        index_sizes = (1, 1, 2, 1, 1, 2, 1)
        machine.put(base + index_offsets[category], slot + 1, index_sizes[category])
        if category == 0:
            machine.store(base + 1, bytes((64, 64, 0, 192, 0, 0, 0)))
        elif category == 3:
            machine.put(base + 4, 17, 1)
            machine.put(base + 44, 192, 1)
        else:
            machine.put(
                base,
                321 if category in (5, 6) else 17,
                2 if category in (2, 5, 6) else 1,
            )
        if category == 6:
            machine.put(base + 2, v["stack_count"], 1)
    return locations


def text_identity(pointer: int, help_size: int) -> int:
    """Normalize actual original text pointers without a text-lookup replacement."""
    if HELP <= pointer < HELP + help_size:
        return pointer - HELP
    if MENU_TEXT <= pointer < MENU_TEXT + 1380:
        return -(pointer - MENU_TEXT) - 1
    raise Fault(f"dialog text pointer outside its actual owner: {pointer:#x}")


def install_services(machine, v: dict, events: list, help_size: int) -> None:
    """Observe UI callback boundaries; execute all reached decimal/clear helper instructions."""

    def event(operation: int, first=0, second=0, third=0) -> None:
        """Append the same semantic service operands the typed world receives."""
        events.extend((operation, s32(first), s32(second), s32(third)))

    def simple(operation: int, value=None, first=0):
        """Build a bounded UI callback, optionally returning independent vector input."""

        def callback(m) -> None:
            """Record a service invocation without replacing owner or lookup algorithms."""
            event(operation, first)
            if value is not None:
                m.r[2] = value & MASK

        return callback

    def initialize(m) -> None:
        """Check the original information-header ABI at its actual call."""
        if m.r[4] != 1:
            raise Fault("unexpected information header")
        event(2, 1)

    def information(m) -> None:
        """Observe the actual offset-table-resolved information pointer."""
        event(4, text_identity(m.r[4], help_size))

    def actions(m) -> None:
        """Read both actual stack arrays and validate each original row type."""
        count, menu, text, types = m.r[4:8]
        if count > 24:
            raise Fault("action callback count outside the audited array")
        event(5, menu, count)
        for row in range(count):
            event(
                6,
                text_identity(m.load(text + row * 8), help_size),
                text_identity(m.load(text + row * 8 + 4), help_size),
                m.load(types + row * 4),
            )

    def quantity(m) -> None:
        """Observe exact row placement and original encoded bytes, preserving row identity."""
        row, x, y, width = m.r[4:8]
        stack = m.r[29]
        if row >= 40 or m.load(stack + 16) != 0:
            raise Fault("quantity row outside the real menu workspace")
        event(11, x, row, (y << 16) | width)
        text = m.load(stack + 20)
        for offset in range(0, 12, 4):
            event(12, m.load(text + offset))
        m.r[2] = ROWS + row * ROW_BYTES

    def discard_sound(m) -> None:
        """Validate both full-word sound arguments before the current owner is reloaded."""
        if m.r[4:6] != [126, 28]:
            raise Fault("discard sound ABI mismatch")
        event(9, 3)

    def digit(m) -> None:
        """Check original glyph, position and explicit foreground-chain arguments."""
        if m.r[7] != 0x80900000 - 12:
            raise Fault("quantity digit uses a different original layer base")
        event(14, m.r[4], m.r[5], m.r[6])

    def cursor(m) -> None:
        """Return independent renderer feedback after observing the original cursor command."""
        event(15, m.r[4], m.r[5])
        m.r[2] = v["cursor_result"] & MASK

    def flyout(m) -> None:
        """Retain the exact selected action-row operand."""
        event(17, m.r[4])

    def clear(m) -> None:
        """Refuse an incorrect clear-except operand rather than silently accepting it."""
        if m.r[4] != 0:
            raise Fault("transfer clear-except ABI mismatch")
        event(18)

    def hide(m) -> None:
        """Observe the typed panel-visibility dependency's original zero command."""
        if m.r[4] != 0:
            raise Fault("transfer panel-visibility ABI mismatch")
        event(19)

    def advance(m) -> None:
        """Model independent list-builder progress as the UI service contract."""
        event(20)
        m.put(LIST_ROW, m.load(LIST_ROW, 1) + 1, 1)

    def row(m) -> None:
        """Return the actual indexed menu-row object without changing its fields."""
        index = m.r[4]
        if index >= 40:
            raise Fault("restored row outside the real menu workspace")
        event(21, index)
        m.r[2] = ROWS + index * ROW_BYTES

    machine.stub(0x800FA9D0, simple(1, v["ready"]))
    machine.stub(0x800C8E04, initialize)
    machine.stub(0x800FFC04, information)
    machine.stub(0x800FF0EC, actions)
    machine.stub(0x800FF43C, simple(7))
    machine.stub(0x800FF360, simple(8, v["action"]))
    for address, sound in ((0x800C02F0, 0), (0x800C02E8, 1), (0x800C02F8, 2)):
        machine.stub(address, simple(9, first=sound))
    for address, category in ((0x801051F8, 0), (0x80105454, 3), (0x801055F0, 6)):

        def sort(m, selected_category=category) -> None:
            """Observe a verified sorter dependency's full-word stat selection."""
            event(10, selected_category, m.r[4])

        machine.stub(address, sort)
    machine.stub(0x8010592C, lambda m: event(10, m.r[4], m.r[5]))
    machine.stub(0x800C8E5C, quantity)
    machine.stub(0x80045988, discard_sound)
    machine.stub(0x800C7210, lambda m: event(13, m.r[4]))
    machine.stub(0x800C70F8, digit)
    machine.stub(0x800FFCDC, cursor)
    machine.stub(0x800FA810, flyout)
    machine.stub(0x800FA8E0, clear)
    machine.stub(0x80103D50, hide)
    machine.stub(0x80100A5C, advance)
    machine.stub(0x800C8E48, row)
    machine.follow.update((0x800CCC54, 0x80102D80, 0x800CCC1C))


def run(identifier: int, machine, values: list[int]) -> tuple[dict, list[int]]:
    """Execute the full original body, real decimal helper and recursive item clearing."""
    v = dict(zip(FIELDS, values, strict=False))
    help_bytes = (DISC / r"MENU\ITEMHELP.BIN").read_bytes()
    machine.store(HELP, help_bytes)
    machine.put(0x80102540, HELP)
    machine.put(0x80109A8C, OWNER)
    machine.put(STATE, v["sort_step"], 1)
    machine.put(STATE + 1, v["sort_category"], 1)
    for offset, name in enumerate(
        (
            "discard_step",
            "category",
            "selected",
            "kind",
            "place",
            "quantity",
            "maximum",
        ),
        2,
    ):
        machine.put(STATE + offset, v[name], 1)
    machine.put(STATE + 12, v["transfer_result"])
    machine.put(STATE + 16, v["transfer_step"], 1)
    machine.put(0x80109A7A, v["direction"], 1)
    machine.put(CONTROLS, v["controls"], 1)
    machine.put(FROZEN, v["frozen"], 1)
    machine.put(TEMPLATE + 15, v["animation"], 1)
    machine.put(LIST_ROW, v["list_row"], 1)
    machine.put(PRESSED, v["pressed"])
    machine.put(REPEATED, v["repeated"])
    machine.put(0x800F5130, v["done"] << 30)
    machine.put(0x1F800008, 0x80900000)
    machine.map_zero(ROWS, ROW_BYTES * 40)
    cursor = len(FIELDS)
    for index in range(40):
        for offset, size, value in zip(
            FIELD_BYTES, (1, 2, 2, 1, 1), values[cursor : cursor + 5], strict=True
        ):
            machine.put(ROWS + index * ROW_BYTES + offset, value, size)
        cursor += 5
    machine.store(MEMORY, bytes(values[cursor:]))
    locations = initialize_records(machine, v)
    events = []
    install_services(machine, v, events, len(help_bytes))
    machine.r[4] = v["command"] & MASK
    machine.execute(RANGES[identifier][0])
    output = [0 if identifier == 2983 else s32(machine.r[2])]
    output += [machine.load(STATE + i, 1) for i in range(9)]
    output += [
        machine.load(STATE + 16, 1),
        s32(machine.load(STATE + 12)),
        machine.load(CONTROLS, 1),
        machine.load(FROZEN, 1),
        machine.load(LIST_ROW, 1),
        machine.load(TEMPLATE + 15, 1),
        *machine.bytes(TEMPLATE, 12),
    ]
    for index in range(40):
        output += [
            machine.load(
                ROWS + index * ROW_BYTES + offset, size, signed=offset in (0x14, 0x18)
            )
            for offset, size in zip(FIELD_BYTES, (1, 2, 2, 1, 1), strict=True)
        ]
    for offset, size, _ in locations:
        output.extend(machine.bytes(OWNER + offset, size))
    # InformationDone is a direct read on the original side, not a call.
    output.extend(events)
    return {"output": output, "stores": machine.stores}, [identifier, *values]


def check(
    _image, _vector, observation: dict, native: list[int], _reference
) -> list[str]:
    """Compare every defined result, state byte, real record byte, row field and UI operand."""
    expected = observation["output"]
    problems = []
    if native != expected:
        first = next(
            (
                i
                for i, pair in enumerate(zip(native, expected, strict=False))
                if pair[0] != pair[1]
            ),
            min(len(native), len(expected)),
        )
        problems.append(
            f"output[{first}] native={native[first : first + 6]} "
            f"original={expected[first : first + 6]}; sizes {len(native)}/{len(expected)}"
        )
    return problems + store_problems(
        observation["stores"],
        [
            (DEFAULT_STACK - 0x4000, DEFAULT_STACK),
            (STATE, STATE + 17),
            (TEMPLATE, TEMPLATE + 12),
            (TEMPLATE + 15, TEMPLATE + 16),
            (CONTROLS, CONTROLS + 1),
            (FROZEN, FROZEN + 1),
            (OWNER, OWNER + 0x3C00),
            (ROWS, ROWS + ROW_BYTES * 40),
        ],
    )
