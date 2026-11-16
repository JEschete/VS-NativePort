"""Shared R3000 vectors and native driver interface for B52-01 text boxes."""

from __future__ import annotations

import random

from differential import DEFAULT_STACK, MASK, Machine, R, s16, s32, store_problems

BOXES = 0x800F4FE0
BOX_SIZE = 0x30
BOX_COUNT = 8
BOX_WORDS = BOX_SIZE // 4
SELECTOR = 0x800F4EA8
SELECTOR_SIZE = 0xD
ANCHORS = 0x800EC284
ANCHOR_WORDS = 32
BUTTONS = 0x8005E1D0
DEFAULT_CHOICE = 0x800615A9
RESULT = 0x80500000
SELECT_SOUND = 0x800C02F0
CHANGE_SOUND = 0x800C02F8
SPEAKER = 0x800AAD4C
INIT_TEXT_BOX = 0x800CCF08
DISMISS_TEXT_BOX = 0x800CD064
ANCHOR_MASK = 0x3FFFFFFF
FRAME = 0x100

# (operation, entry, end) with ends taken from the following symbol.
SPECS = {
    "F01329": (0, 0x800C6BF0, 0x800C6C8C),
    "F01330": (1, 0x800C6C8C, 0x800C7010),
    "F01343": (2, 0x800C8E04, 0x800C8E48),
    "F01378": (3, 0x800CB268, 0x800CB2B8),
    "F01409": (4, 0x800CCDF4, 0x800CCE10),
    "F01410": (5, 0x800CCE10, 0x800CCF08),
    "F01411": (6, 0x800CCF08, 0x800CD064),
    "F01412": (7, 0x800CD064, 0x800CD0FC),
}
NAMES = {
    "F01329": "vs_battle_setTextBox (5BF94.c:1536)",
    "F01330": "vs_battle_selectTextBox (5BF94.c:1559)",
    "F01343": "vs_battle_initInformationTextBox (5BF94.c:2653)",
    "F01378": "_dismissAllTextBoxes (5BF94.c:3757)",
    "F01409": "vs_battle_getTextBox (5BF94.c:4544)",
    "F01410": "func_800CCE10 text anchor (5BF94.c:4546)",
    "F01411": "vs_battle_initTextBox (5BF94.c:4567)",
    "F01412": "vs_battle_dismissTextBox (5BF94.c:4613)",
}
MUTATIONS = {
    "F01329": {
        "F01329-start-speed-two": {0x800C6C20: 0x24110002},
        "F01329-keep-opaque4": {0x800C6C2C: 0x00000000},
        "F01329-speaker-bit-0x10": {0x800C6C34: 0x30420010},
        "F01329-keep-previous-chunks": {0x800C6C3C: 0x00000000},
        "F01329-speaker-command-zero": {0x800C6C48: 0x24070000},
        "F01329-speaker-same-timeline": {0x800C6C50: 0x24C60000},
        "F01329-no-speaker-pause": {0x800C6C58: 0x00000000},
        "F01329-finished-speed-fffe": {0x800C6C60: 0x3402FFFE},
        "F01329-pending-returns-one": {0x800C6C70: 0x24020001},
    },
    "F01330": {
        "F01330-sort-unsigned-y": {0x800C6D34: 0x0043102B},
        "F01330-sort-descending-y": {0x800C6D34: 0x0062102A},
        "F01330-default-not-one-based": {0x800C6D88: 0x24500000},
        "F01330-default-bound-signed": {0x800C6D8C: 0x0211102A},
        "F01330-start-keeps-selected": {0x800C6DA0: 0x00000000},
        "F01330-confirm-mask-0x7f": {0x800C6DBC: 0x3062007F},
        "F01330-up-button-0x2000": {0x800C6DC4: 0x30622000},
        "F01330-confirm-keeps-count": {0x800C6DD0: 0x00000000},
        "F01330-down-button-0x8000": {0x800C6DE8: 0x30628000},
        "F01330-no-change-sound": {0x800C6E18: 0x00000000},
        "F01330-top-reversed": {0x800C6E48: 0x0062102B},
        "F01330-no-order-swap": {0x800C6EA0: 0x00000000},
        "F01330-selected-brightness-0x81": {0x800C6FB0: 0x24060081},
        "F01330-other-brightness-0x41": {0x800C6FB4: 0x24040041},
    },
    "F01343": {
        "F01343-kind-five": {0x800C8E08: 0x24020005},
        "F01343-header-shift-nine": {0x800C8E0C: 0x00042A40},
        "F01343-box-six": {0x800C8E10: 0x24040006},
        "F01343-y-zero": {0x800C8E1C: 0x24070000},
        "F01343-zero-lines": {0x800C8E28: 0xAFA00014},
    },
    "F01378": {
        "F01378-default-open": {0x800CB270: 0x24110000},
        "F01378-start-at-one": {0x800CB278: 0x24100001},
        "F01378-ignores-open-boxes": {0x800CB290: 0x00000000},
        "F01378-skips-last-box": {0x800CB298: 0x2A020007},
    },
    "F01409": {
        "F01409-stride-0x50": {0x800CCDF4: 0x00041880},
        "F01409-stride-0x60": {0x800CCDFC: 0x00031940},
        "F01409-base-plus-four": {0x800CCE04: 0x24424FE4},
    },
    "F01410": {
        "F01410-facing-shift-three": {0x800CCE24: 0x000630C3},
        "F01410-three-secondary-facings": {0x800CCE30: 0x2C420003},
        "F01410-no-secondary": {0x800CCE3C: 0x24A50000},
        "F01410-no-mirror": {0x800CCE68: 0x00000000},
        "F01410-mirror-alignment-one": {0x800CCE6C: 0x39290001},
        "F01410-column-scale-twenty": {0x800CCE78: 0x00071080},
        "F01410-column-margin-twelve": {0x800CCE84: 0x2442000C},
        "F01410-line-margin-five": {0x800CCEB4: 0x24420005},
        "F01410-frame-part-three-bits": {0x800CCEF0: 0x3C023800},
    },
    "F01411": {
        "F01411-speaker-0x10": {0x800CCF24: 0x3C021000},
        "F01411-x-indent-six": {0x800CCF30: 0x24C60006},
        "F01411-y-indent-four": {0x800CCF38: 0x24E70004},
        "F01411-keep-speaker-animation": {0x800CCF54: 0x00000000},
        "F01411-brightness-0x40": {0x800CCF6C: 0x24020040},
        "F01411-durations-45a6": {0x800CCF74: 0x240245A6},
        "F01411-closed-state-stays": {0x800CCFA0: 0x04610003},
        "F01411-open-state-two": {0x800CCFA8: 0x24020002},
        "F01411-alias-kind-six": {0x800CCFF4: 0x34420006},
        "F01411-information-cpl-0x19": {0x800CD008: 0x24020019},
        "F01411-information-center-x-0xa1": {0x800CD010: 0x240200A1},
        "F01411-information-bottom-0xf3": {0x800CD038: 0x240300F3},
        "F01411-second-alias-shape-one": {0x800CD058: 0x34420100},
    },
    "F01412": {
        "F01412-speaker-bit-0x10": {0x800CD094: 0x30420010},
        "F01412-speaker-timeline-one": {0x800CD0A0: 0x24060001},
        "F01412-close-duration-nine": {0x800CD0C0: 0x24020009},
        "F01412-closing-state-minus-two": {0x800CD0C8: 0x2402FFFE},
        "F01412-keeps-open-style-bit": {0x800CD0D4: 0x2403FFEF},
        "F01412-closed-returns-one": {0x800CD0E8: 0x24020001},
    },
}
MAX_PROTOCOL = 2 + BOX_COUNT * BOX_WORDS + 4 + 1
CONFIRM, UP, DOWN = 0xFF, 0x1000, 0x4000


def halves(low: int, high: int) -> int:
    return (low & 0xFFFF) | (high & 0xFFFF) << 16


def flags(rng: random.Random, number: int) -> int:
    word = rng.getrandbits(32)
    kinds = (0, 1, 2, 3, 4, 5, 6, 7, 8, 15)
    word = (word & ~0xF) | kinds[(number + rng.randrange(3)) % len(kinds)]
    if rng.randrange(2):
        word ^= 0x20000000  # Speaker absent/present is bit 29.
    return word & MASK


def box_words(rng: random.Random, number: int, y_values=None) -> list[int]:
    words = []
    for index in range(BOX_COUNT):
        state = rng.choice((-1, 0, 1, 2, -2, 0x7FFF, -0x8000, s16(rng.getrandbits(16))))
        speed = rng.choice((0, 1, 0xFFFF, 0xFFFE, rng.getrandbits(16)))
        y = (
            y_values[index]
            if y_values is not None
            else rng.choice((0, 1, -1, 0x7FFF, -0x8000, s16(rng.getrandbits(16))))
        )
        words.extend(
            [
                flags(rng, number + index),
                rng.getrandbits(32),
                rng.choice((0, 0x80100000 + rng.getrandbits(16) * 4, rng.getrandbits(32))),
                halves(speed, rng.getrandbits(16)),
                rng.getrandbits(32),
                halves(rng.getrandbits(16), y),
                rng.getrandbits(32),
                halves(state, rng.getrandbits(16)),
                rng.getrandbits(32),
                rng.getrandbits(32),
                rng.getrandbits(32),
                rng.getrandbits(32),
            ]
        )
    return words


def selector_words(rng: random.Random, number: int) -> list[int]:
    count = (0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xFF)[number % 12]
    if number % 23 == 7:
        count = rng.choice((9, 0x10, 0xFF))
    order = rng.sample(range(BOX_COUNT), BOX_COUNT)
    if number % 17 == 5:
        order[rng.randrange(BOX_COUNT)] = rng.choice((8, 9, 0x40, 0xFF))
    if count == 0:
        order[0] = rng.randrange(BOX_COUNT)  # The original self-swaps it.
    selected = rng.choice((0, 1, count - 1 if count else 0, count, rng.getrandbits(8)))
    reserved = rng.getrandbits(16)
    tail = rng.getrandbits(32)
    return [
        sum(order[i] << i * 8 for i in range(4)),
        sum(order[4 + i] << i * 8 for i in range(4)),
        reserved | (selected & 0xFF) << 16 | (count & 0xFF) << 24,
        tail,
    ]


def selection_y(rng: random.Random, number: int) -> list[int]:
    pattern = number % 4
    if pattern == 0:
        return [rng.randrange(0, 4) for _ in range(BOX_COUNT)]  # Ties.
    if pattern == 1:
        return [rng.choice((0x7FFF, 5, -5, 0)) for _ in range(BOX_COUNT)]
    if pattern == 2:
        return [s16(rng.getrandbits(16)) for _ in range(BOX_COUNT)]
    return [40 + 13 * i for i in rng.sample(range(BOX_COUNT), BOX_COUNT)]


def corrupt_selector(values: list[int]) -> bool:
    selector = values[SELECTOR_START : SELECTOR_START + 4]
    count = selector[2] >> 24
    order = [(selector[i // 4] >> (i % 4) * 8) & 0xFF for i in range(BOX_COUNT)]
    return (
        count != 0
        and not values[9] & CONFIRM
        and (count > BOX_COUNT or any(entry >= BOX_COUNT for entry in order[:count]))
    )


def refusal_expected(values: list[int]) -> bool:
    operation, arguments = values[0], values[1:9]
    if operation == 1:
        return corrupt_selector(values)
    if operation == 4:
        return not 0 <= s32(arguments[0]) < BOX_COUNT
    if operation == 5:
        return not 0 <= s32(arguments[1]) < ANCHOR_WORDS // 2
    return False


SELECTOR_START = 12 + BOX_COUNT * BOX_WORDS


def vector_for(identifier: str, rng: random.Random, number: int) -> list[int]:
    operation = SPECS[identifier][0]
    arguments = [0] * 8
    pressed = 0
    default_choice = 0
    y_values = None
    box = rng.randrange(BOX_COUNT)
    if operation == 0:
        text = 0 if number % 2 else rng.choice((0x80100000 + rng.getrandbits(12) * 4, rng.getrandbits(32) | 1))
        arguments[:2] = [box, text]
    elif operation == 1:
        masks = (0, 1, 0x80, 0xFF, 0b110, 0x81, 0x18, 0x7E)
        mask = masks[number % len(masks)] if number < 64 else rng.getrandbits(8)
        arguments[0] = mask | rng.getrandbits(24) << 8  # Only the byte counts.
        buttons = (0, CONFIRM, 0x80, 0x01, UP, DOWN, UP | DOWN, UP | CONFIRM, 0x2000, 0x8000, 0x10)
        pressed = buttons[number % len(buttons)] if number % 5 else rng.getrandbits(32)
        default_choice = rng.choice((0, 1, 2, 3, 8, 9, 255, rng.getrandbits(8)))
        y_values = selection_y(rng, number)
    elif operation == 2:
        headers = (0, 1, 2, 7, 15, 0xFF, 0x100, -1, 0x7FFFFFFF)
        arguments[0] = headers[number % len(headers)] if number < 64 else s32(rng.getrandbits(32))
    elif operation == 4:
        ids = (0, 1, 7, 8, -1, 0x7FFFFFFF, -0x80000000, 100)
        arguments[0] = ids[number % len(ids)] if number < 64 else rng.randrange(BOX_COUNT) if number % 4 else s32(rng.getrandbits(32))
    elif operation == 5:
        preset = rng.choice((-1, 16, 17, 0x7FFFFFFF)) if number % 8 == 7 else number % 16 if number < 128 else rng.randrange(16)
        orientation = number // 16 % 16 if number < 256 else s32(rng.getrandbits(32))
        columns = rng.choice((0, 1, 4, 20, 40, -1, 0x7FFFFFFF, s32(rng.getrandbits(32))))
        lines = rng.choice((0, 1, 3, 9, -1, -0x80000000, s32(rng.getrandbits(32))))
        arguments[:5] = [RESULT, preset, orientation, columns, lines]
    elif operation == 6:
        layout_flags = rng.choice((0, 1, 4, 5, 2, 7, 0x14, 0x31, 0x305, 0x704)) | rng.choice((0, rng.getrandbits(16) & ~0xF))
        if number % 3 == 0:
            layout_flags = rng.getrandbits(32)
        small = (0, 1, -1, 4, 12, 0x7F, 0x80, 0xFF, 0x7FFF, 0x8000, -0x8000)
        arguments = [box, s32(layout_flags)] + [
            rng.choice(small) if rng.randrange(2) else s32(rng.getrandbits(32)) for _ in range(6)
        ]
    elif operation == 7:
        arguments[0] = box
    values = [operation, *arguments, pressed, default_choice, rng.getrandbits(32)]
    words = box_words(rng, number, y_values)
    if operation == 3 and number % 4 == 0:
        for index in range(BOX_COUNT):  # Every box closed.
            words[index * BOX_WORDS + 7] &= 0xFFFF0000
    values += words
    values += selector_words(rng, number)
    return [value & MASK if index else value for index, value in enumerate(values)]


def run_original(machine: Machine, values: list[int]) -> tuple[dict, list[int]]:
    operation, arguments = values[0], values[1:9]
    pressed, default_choice, v1 = values[9:12]
    boxes = values[12:SELECTOR_START]
    selector = values[SELECTOR_START : SELECTOR_START + 4]
    anchors = [machine.load(ANCHORS + i * 4) for i in range(ANCHOR_WORDS)]
    inputs = [operation, *[s32(a) for a in arguments], pressed, default_choice, v1, *boxes, *selector, *anchors]
    if refusal_expected(values):
        # The native core refuses; the original read past its tables.
        return {"refused": True}, inputs
    for index, word in enumerate(boxes):
        machine.put(BOXES + index * 4, word)
    for index, word in enumerate(selector):
        machine.put(SELECTOR + index * 4, word)
    machine.put(BUTTONS, pressed)
    machine.put(DEFAULT_CHOICE, default_choice, 1)
    machine.map_zero(RESULT, 4)
    calls = []

    def record(kind: int):
        def handler(m: Machine) -> None:
            if kind:
                calls.append([kind, 0, 0, 0, 0])
            else:
                calls.append([0, m.r[R["a0"]] & 0xFF, m.r[R["a1"]] & MASK, s32(m.r[R["a2"]]), s32(m.r[R["a3"]])])

        return handler

    machine.stub(SPEAKER, record(0))
    machine.stub(SELECT_SOUND, record(1))
    machine.stub(CHANGE_SOUND, record(2))
    machine.follow.update((INIT_TEXT_BOX, DISMISS_TEXT_BOX))
    registers = arguments[:4]
    if operation == 5:
        registers = arguments[:4]
        machine.put(DEFAULT_STACK + 0x10, arguments[4])
    elif operation == 6:
        machine.put(DEFAULT_STACK + 0x10, arguments[4])
        machine.put(DEFAULT_STACK + 0x14, arguments[5])
        machine.put(DEFAULT_STACK + 0x18, arguments[6])
        machine.put(DEFAULT_STACK + 0x1C, arguments[7])
    for name, value in zip(("a0", "a1", "a2", "a3"), registers, strict=True):
        machine.r[R[name]] = value & MASK
    machine.r[R["v1"]] = v1
    machine.execute(SPECS_BY_OPERATION[operation])
    return {
        "refused": False,
        "return": machine.r[R["v0"]] & MASK,
        "boxes": [machine.load(BOXES + i * 4) for i in range(BOX_COUNT * BOX_WORDS)],
        "selector": [machine.load(SELECTOR + i * 4) for i in range(4)],
        "result": machine.load(RESULT),
        "calls": calls,
        "stores": machine.stores,
    }, inputs


SPECS_BY_OPERATION = {operation: entry for operation, entry, _ in SPECS.values()}


def expected_return(operation: int, observation: dict, native: list[int]) -> tuple[int, int]:
    original = observation["return"]
    value = native[1] & MASK
    if operation == 4:
        return value * BOX_SIZE + BOXES, original
    if operation == 5:
        return value & ANCHOR_MASK, observation["result"] & ANCHOR_MASK
    if operation in (2, 6):
        return 0, 0  # Void.
    return value, original


def check_original(_image, values: list[int], observation: dict, native: list[int], _reference: dict) -> list[str]:
    if len(native) < MAX_PROTOCOL or len(native) != MAX_PROTOCOL + native[MAX_PROTOCOL - 1] * 5:
        return ["invalid native protocol"]
    if observation["refused"]:
        return [] if native[0] else ["native accepted an input the original cannot handle"]
    if native[0]:
        return ["native refused a valid original input"]
    operation = values[0]
    problems = []
    got, want = expected_return(operation, observation, native)
    if got != want:
        problems.append(f"return native {got:#x} original {want:#x}")
    if operation == 5 and observation["return"] != RESULT:
        problems.append(f"anchor returned {observation['return']:#x}, not the result pointer")
    boxes = native[2 : 2 + BOX_COUNT * BOX_WORDS]
    for index, (mine, theirs) in enumerate(zip(boxes, observation["boxes"], strict=True)):
        if mine != theirs:
            problems.append(f"box {index // BOX_WORDS} word {index % BOX_WORDS} native {mine:#x} original {theirs:#x}")
            break
    selector = native[2 + BOX_COUNT * BOX_WORDS : MAX_PROTOCOL - 1]
    if selector != observation["selector"]:
        problems.append(f"selector native {selector} original {observation['selector']}")
    count = native[MAX_PROTOCOL - 1]
    calls = [native[MAX_PROTOCOL + i * 5 : MAX_PROTOCOL + i * 5 + 5] for i in range(count)]
    if calls != observation["calls"]:
        problems.append(f"calls native {calls} original {observation['calls']}")
    allowed = [
        (BOXES, BOXES + BOX_COUNT * BOX_SIZE),
        (SELECTOR, SELECTOR + SELECTOR_SIZE),
        (DEFAULT_STACK - FRAME, DEFAULT_STACK),
    ]
    if operation == 5:
        allowed.append((RESULT, RESULT + 4))
    return problems + store_problems(observation["stores"], allowed)


def build_case(identifier: str) -> dict:
    operation, entry, end = SPECS[identifier]

    def vector(rng: random.Random, number: int) -> list[int]:
        return vector_for(identifier, rng, number)

    def describe(_image, results) -> list[str]:
        refused = sum(bool(observation["refused"]) for _, observation, _ in results)
        lines = [
            f"{NAMES[identifier]}: executed the complete function including its return delay slot.",
            "Compared all eight 0x30-byte boxes, the 16-byte selector window, the return, every "
            "speaker/sound call with its arguments and every original store.",
            f"{refused} vectors are documented refusals (out-of-range id/preset or a corrupt selector) "
            "the original cannot execute inside its tables; native must refuse exactly those.",
        ]
        if operation == 5:
            lines.append("Anchor words come from the original D_800EC284 table; bits 30-31 hold the caller's "
                         "stale $v1 in the original and are excluded.")
        if operation in (2, 3):
            lines.append("The nested initTextBox/dismissTextBox calls follow the original instructions.")
        return lines

    return {
        "ID": identifier,
        "ENTRY": entry,
        "END": end,
        "DRIVER": "text_box_state_driver.cpp",
        "SOURCES": (
            r"Native\Game\src\Menu\TextBoxState.cpp",
            r"Native\Engine\src\Core\Math\ScalarMath.cpp",
            r"Native\Engine\src\Core\Math\Distance.cpp",
        ),
        "VECTORS": 1024,
        "SEED": int(identifier[1:]),
        "MUTANT_VECTORS": 256,
        "MUTANTS": MUTATIONS[identifier],
        "vector": vector,
        "run": run_original,
        "check": check_original,
        "describe": describe,
    }
