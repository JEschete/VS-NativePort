"""Original notification ownership, formatting, polling and release spans."""

from __future__ import annotations

import functools

from differential import DEFAULT_STACK, DEFAULT_STACK_SIZE, printed_problems, s32

POINTER = 0x800EB9D8
TIMER, TIMED = 0x800F4E68, 0x800F4E69
CONTEXT = 0x800F5190
BOX_FLAGS = 0x800F5130
TICK = 0x8005E24C
ACTION_NAME = 0x8004B9F8
ACTION_STRIDE = 52
ITEM_NAMES = 0x800EBF58
MENU_TEXT = 0x800EB9DC
DOOR_TEXT = 0x800EBDDC
BUFFER = 0x80500000
CAPACITY = 112
END_TEXT = 0xE7
SPANS = (
    (0x800CB2B8, 0x800CB45C, "225b96d0b292c8eaaa08ccbe8c32f9ad17754cf2fbe083d42ff5c5b3fdd1e8b8"),
    (0x800CB45C, 0x800CB50C, "9bde5a58f7ba131acd969c1de5a93695169a8037785c4ea8e52373c9af864be4"),
    (0x800CB50C, 0x800CB550, "35b46d7680a83d1f629f033a71c45d2c1e7e5f5b316882e76dfc42445ec236bc"),
)
MUTATIONS = (
    {
        "allocation-size": (0x800CB2D4, 0x2404006F),
        "timed-publication": (0x800CB2F0, 0),
        "timed-narrowing": (0x800CB2F4, 0x3210FFFF),
        "initial-timer": (0x800CB300, 0x24020077),
        "timer-publication": (0x800CB304, 0),
        "initial-box-id": (0x800CB308, 0x24040006),
        "initial-layout": (0x800CB30C, 0x24050204),
        "initial-lines": (0x800CB310, 0x24020004),
        "initialize-call": (0x800CB328, 0),
        "prefix-high": (0x800CB330, 0x3C0303FB),
        "prefix-low": (0x800CB338, 0x346300F9),
        "action-name-stride": (0x800CB358, 0x000528C0),
        "mastered-discriminant": (0x800CB36C, 0x2402000D),
        "action-discriminant": (0x800CB374, 0x2402000A),
        "mastered-prefix": (0x800CB388, 0x24840000),
        "action-frame-shape": (0x800CB3A4, 0),
        "action-type-mask": (0x800CB3C0, 0x30420003),
        "action-message-index": (0x800CB3C4, 0x24420017),
        "action-prefix": (0x800CB3D4, 0x24840000),
        "item-name-bias": (0x800CB3E8, 0x2622FE37),
        "item-name-stride": (0x800CB3F4, 0x00031880),
        "notification-index-stride": (0x800CB410, 0x00131080),
        "notification-text-stride": (0x800CB420, 0x00021000),
        "format-call": (0x800CB428, 0),
        "text-publication-call": (0x800CB438, 0),
        "text-box-id": (0x800CB43C, 0x24040006),
    },
    {
        "absent-owner-gate": (0x800CB468, 0),
        "done-bit": (0x800CB47C, 0x000217C2),
        "unfinished-result": (0x800CB490, 0x24020000),
        "timed-gate": (0x800CB49C, 0),
        "timer-reads-next-byte": (0x800CB4A4, 0x90834E69),
        "stop-threshold": (0x800CB4AC, 0x2C620005),
        "expired-timer-publication": (0x800CB4BC, 0),
        "timer-adds": (0x800CB4C8, 0x00621021),
        "timer-subtraction-publication": (0x800CB4CC, 0),
        "expired-poll-gate": (0x800CB4DC, 0),
        "poll-box": (0x800CB4E4, 0x24040006),
        "poll-call": (0x800CB4E8, 0),
        "poll-negation": (0x800CB4F4, 0x00021021),
    },
    {
        "empty-release-gate": (0x800CB520, 0),
        "dismiss-call": (0x800CB528, 0),
        "dismiss-box": (0x800CB52C, 0x24040006),
        "release-call": (0x800CB534, 0),
        "clear-owner": (0x800CB53C, 0),
    },
)


@functools.cache
def verify(image):
    for entry, end, expected in SPANS:
        if image.sha256(entry, end) != expected or image.read(end - 8) != 0x03E00008:
            raise RuntimeError(f"notification original span changed: {entry:08X}-{end:08X}")


def text(seed, index):
    return bytes((1 + (seed + index * 13) % 200, 0xFF, 0,
                  1 + (seed + index * 17) % 200, END_TEXT))


def name(seed, size):
    result = bytearray(1 + (seed + index * 7) % 200 for index in range(size))
    result[1 + seed % (size - 1)] = END_TEXT
    return bytes(result)


def vector(rng, number, operation):
    kind = number % 13
    subject = (number * 37) % 256 if kind >= 11 else (
        0 if number % 2 == 0 else 0x1CA + number % 33)
    flags = (rng.getrandbits(32) & ~0x40000000) | ((number // 2) % 2) << 30
    return {
        "operation": operation, "kind": kind, "subject": subject,
        "timed_arg": (0, 1, 0x100, 0x101, -1, 128, 255)[number % 7],
        "active": operation != 0 and number % 4 != 0,
        "timer": (0, 1, 2, 3, 4, 5, 120, 127, 128, 255)[number % 10],
        "timed": (0, 1, 128, 255)[(number // 4) % 4],
        "flags": flags, "context": 0x80600000 + number * 4,
        "poll": (0, -1, 1, -0x80000000, 0x7FFFFFFF, s32(rng.getrandbits(32)))[number % 6],
        "tick": (0, 1, 2, 3, 4, 120, 255, -1, s32(rng.getrandbits(32)))[number % 9],
        "seed": number % 211, "type": (number * 3) % 8,
    }


def run(machine, value):
    verify(machine.images[0])
    operation, seed = value["operation"], value["seed"]
    machine.map_zero(MENU_TEXT, 246 * 2)
    machine.map_zero(DOOR_TEXT, 190 * 2)
    for index in range(11):
        offset = 40 + index * 12
        machine.put(DOOR_TEXT + index * 2, offset, 2)
        machine.store(DOOR_TEXT + offset * 2, text(seed, index))
    for index in range(8):
        offset = 64 + index * 12
        machine.put(MENU_TEXT + (22 + index) * 2, offset, 2)
        machine.store(MENU_TEXT + offset * 2, text(seed, 20 + index))
    machine.store(MENU_TEXT + 224 * 2, text(seed, 40))
    action_address = ACTION_NAME + value["subject"] * ACTION_STRIDE
    machine.store(action_address, name(seed, 24))
    machine.put(action_address - 28, value["type"] << 17)
    if value["subject"] >= 0x1CA:
        machine.store(ITEM_NAMES + (value["subject"] - 0x1CA) * 24, name(seed + 31, 12))
    machine.store(BUFFER, bytes((seed + index * 5) & 255 for index in range(CAPACITY)))
    machine.put(POINTER, BUFFER if value["active"] else 0)
    machine.put(TIMER, value["timer"], 1)
    machine.put(TIMED, value["timed"], 1)
    machine.put(CONTEXT, value["context"])
    machine.put(BOX_FLAGS, value["flags"])
    machine.put(TICK, value["tick"], 1)
    events = []

    def record(code, arguments):
        events.extend((code, len(arguments), *arguments,
                       machine.load(POINTER), machine.load(TIMER, 1),
                       machine.load(TIMED, 1), machine.load(CONTEXT), machine.load(BOX_FLAGS)))

    def allocate(current):
        record(0, [current.r[4]])
        current.r[2] = BUFFER

    def initialize(current):
        record(1, [*current.r[4:8], *(current.load(current.r[29] + offset)
                                     for offset in (16, 20, 24, 28))])

    def set_text(current):
        record(2, current.r[4:6])
        current.r[2] = value["poll"] & 0xFFFFFFFF

    machine.stub(0x80043E3C, allocate)
    machine.stub(0x800CCF08, initialize)
    machine.stub(0x800C6BF0, set_text)
    machine.stub(0x800CD064, lambda current: record(3, [current.r[4]]))
    machine.stub(0x80043C60, lambda current: record(4, [current.r[4]]))
    machine.follow.update((0x800C685C, 0x800C01E0, 0x800490B0, 0x80049110))
    machine.r[4:7] = [value["kind"], value["subject"], value["timed_arg"] & 0xFFFFFFFF]
    machine.execute(SPANS[operation][0])
    result = s32(machine.r[2]) if operation == 1 else 0
    values = [result, machine.load(POINTER), machine.load(TIMER, 1),
              machine.load(TIMED, 1), machine.load(CONTEXT), machine.load(BOX_FLAGS),
              *machine.bytes(BUFFER, CAPACITY), len(events), *events]
    names = ["return", "owner", "timer", "timed", "subject", "flags",
             *(f"buffer[{index}]" for index in range(CAPACITY)),
             "event-words", *(f"event[{index}]" for index in range(len(events)))]
    inputs = [operation, value["kind"], value["subject"], value["timed_arg"],
              int(value["active"]), value["timer"], value["timed"], value["flags"],
              value["context"], value["poll"], value["tick"], seed, value["type"]]
    return {"values": values, "names": names, "stores": list(machine.stores)}, inputs


def check(_image, _value, observation, native, _reference):
    if not native or native[0]:
        return ["native refused a defined notification vector"]
    return printed_problems(observation, native[1:], [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (BUFFER, BUFFER + CAPACITY), (POINTER, POINTER + 4), (TIMER, TIMED + 1),
        (CONTEXT, CONTEXT + 4), (BOX_FLAGS, BOX_FLAGS + 4),
    ])


def case(operation):
    entry, end, digest = SPANS[operation]
    return {
        "ID": f"F{1379 + operation:05}", "ENTRY": entry, "END": end,
        "MODULES": ("BATTLE/BATTLE.PRG", "SLUS_010.40"),
        "DRIVER": "scene_notifications_driver.cpp",
        "SOURCES": (r"Native\Game\src\Menu\SceneNotifications.cpp",
                    r"Native\Engine\src\Render\EncodedText.cpp",
                    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
                    r"Native\Engine\src\Core\Math\Distance.cpp"),
        "CXX_STANDARD": "c++latest", "VECTORS": 1024, "SEED": 1379 + operation,
        "MUTANT_VECTORS": 256,
        "MUTANTS": {name: {address: word}
                    for name, (address, word) in MUTATIONS[operation].items()},
        "vector": lambda rng, number: vector(rng, number, operation),
        "run": run, "check": check,
        "describe": lambda _image, results: [
            f"F{1379 + operation:05} {entry:08X}-{end:08X} exclusive; sha256={digest}",
            f"{len(results)} full 112-byte buffers, state publications and ordered callbacks compared.",
            "Original encoded-text expansion executes; allocator and text-box service boundaries are doubled.",
            "Busy, malformed, undersized and invalid heap bindings are separate native refusal tests.",
        ],
    }
