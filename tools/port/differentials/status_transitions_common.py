"""MENU4 status transitions: original instructions, shared component APIs and real formatting."""

from __future__ import annotations

import functools
import hashlib

from differential import DEFAULT_STACK, Fault, R, printed_problems
from status_components_common import (
    BASES,
    BUFFER,
    CONTEXT,
    HELP,
    ITEM,
    MODULE_HASHES,
    MODULES,
    NAMES,
    PAGE,
    ROWS,
)
from status_components_common import (
    SOURCES as COMPONENT_SOURCES,
)
from status_components_common import (
    fixture as component_fixture,
)

IDS = ("F02655", "F02669", "F02670", "F02671", "F02672", "F02673", "F02674")
BOUNDARIES = (
    (0x801034BC, 0x80103608),
    (0x80104AEC, 0x80104B38),
    (0x80104B38, 0x80104C0C),
    (0x80104C0C, 0x80104C40),
    (0x80104C40, 0x80104DFC),
    (0x80104DFC, 0x80104F2C),
    (0x80104F2C, 0x80104F80),
)
SPAN_HASHES = (
    "129dbdaa60d27dd76c25a9b420b98886ece22ebac9d6a6c3c24b3aef24955f75",
    "e0c67dd9997ca64a13f60aa7b8d5102d68f99129a326bce26d8467ffa62aebe0",
    "f0d2c84bca0d45fc4d90ddee53890324d6cdd1fce47e359fa7da86da77cd1795",
    "c6fdde41464deb8abd38fb28ff4a027ac0548ed67ccea99ccbb258db4b40dd1c",
    "2f7c78ce3919307da620113f674adf0fdbecaaffcae5e39d040ad6c309baee04",
    "ea9cb23841ae59adaea9425b0fd77a95b98e3fa79c927091c4041635b391315b",
    "a8de32a3cedce5ffdb585b5b238f15152c71d234d041b4167c4c850389e29939",
)
STACK_RELEASES = (0x28, 0x18, 0x20, 0x18, 0xA0, 0x60, 0x18)
DRIVER = "status_transitions_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\Status\DetailTransitions.cpp", *COMPONENT_SOURCES)
INCLUDES = (r"Native\Tests\include",)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
COMPILER_OPTIONS = ("/W4", "/WX", "/Gy", "/Gw", "/MD", "/DNOMINMAX")
NAVIGATION, REPEAT, STATUS, SUBTEXT = 0x8010214C, 0x8005DFDC, 0x80107448, 0x80460000
ROW_BYTES, ROW_COUNT, STATUS_BYTES = 64, 40, 3520
RECORD_SIZES = (0, 0, 0, 0, 0x198, 0x164, 0)
END = 0xE7

MUTATIONS = (
    {
        "up-button": (0x801034EC, 0x30622000),
        "up-direction": (0x801034FC, 0x24110004),
        "down-button": (0x80103500, 0x30622000),
        "down-direction": (0x80103510, 0x24110001),
        "left-direction": (0x80103520, 0x24110004),
        "right-direction": (0x8010352C, 0x24110003),
        "initial-map-row": (0x80103538, 0x24632150),
        "selectable-call": (0x8010355C, 0),
        "backward-upper-bound": (0x80103578, 0x2C42000D),
        "backward-step": (0x8010359C, 0x2610FFFE),
        "forward-step": (0x801035BC, 0x26100002),
        "continued-map-column": (0x801035D0, 0x24420000),
        "selected-return": (0x80103568, 0x02401021),
    },
    {
        "flyout-call": (0x80104AFC, 0),
        "flyout-row": (0x80104B00, 0x24040005),
        "equipment-row-base": (0x80104B08, 0x2604000B),
        "horizontal-state": (0x80104B0C, 0x24030003),
        "horizontal-target": (0x80104B14, 0x2403009A),
        "selected-value": (0x80104B1C, 0x24030000),
        "subtext-clear": (0x80104B24, 0),
    },
    {
        "title-background": (0x80104B44, 0x24020007),
        "category-boundary": (0x80104B54, 0x2A020001),
        "weapon-title-index": (0x80104B60, 0x2602000A),
        "armor-title-index": (0x80104B6C, 0x24020014),
        "title-row": (0x80104B74, 0x2404001F),
        "title-x": (0x80104B84, 0x2405FF5D),
        "title-y": (0x80104B88, 0x24060013),
        "title-width": (0x80104B8C, 0x240700A3),
        "armor-icon": (0x80104BA4, 0x2405001B),
        "title-state": (0x80104BA8, 0x24020002),
        "title-target": (0x80104BB0, 0x24020011),
        "title-selected": (0x80104BB8, 0x24020000),
        "weapon-icon": (0x80104BC8, 0x24050019),
        "shield-icon": (0x80104BD8, 0x2405001C),
        "selected-row-base": (0x80104BDC, 0x2604000B),
        "vertical-state": (0x80104BEC, 0x24020002),
        "vertical-target": (0x80104BF4, 0x24020013),
    },
    {
        "transition-call": (0x80104C18, 0),
        "page-call": (0x80104C20, 0),
        "page-command": (0x80104C24, 0x00002021),
        "primary-call": (0x80104C28, 0),
        "primary-command": (0x80104C2C, 0x24040002),
    },
    {
        "blade-selector": (0x80104C5C, 0x24020003),
        "gem-capacity-field": (0x80104C6C, 0x90E3005C),
        "part-width": (0x80104C74, 0x2411009D),
        "blade-conversion": (0x80104C80, 0),
        "blade-format": (0x80104C98, 0),
        "grip-selector": (0x80104CA8, 0x24020001),
        "grip-conversion": (0x80104CB8, 0),
        "grip-format": (0x80104CD0, 0),
        "first-gem-row": (0x80104CE0, 0x2651FFFE),
        "capacity-comparison": (0x80104CE4, 0x0071102A),
        "empty-socket-text": (0x80104CF8, 0x2442681C),
        "gem-stride": (0x80104D08, 0x00022940),
        "gem-conversion": (0x80104D2C, 0),
        "gem-format": (0x80104D44, 0),
        "gem-width": (0x80104D50, 0x24110096),
        "component-row-base": (0x80104D58, 0x26500015),
        "deactivation-call": (0x80104D5C, 0),
        "screen-edge": (0x80104D6C, 0x2413013F),
        "vertical-pitch": (0x80104D78, 0x001230C0),
        "vertical-top": (0x80104D7C, 0x24C60013),
        "init-low-bit": (0x80104D9C, 0x32820002),
        "empty-unselectable": (0x80104DA0, 0x2C630000),
        "slide-state": (0x80104DAC, 0x24020003),
        "slide-target": (0x80104DB8, 0),
        "icon-shift": (0x80104DC4, 0x00021642),
        "material-mask": (0x80104DD4, 0x30420003),
    },
    {
        "gem-capacity-field": (0x80104E20, 0x90E2002C),
        "first-gem-row": (0x80104E24, 0x26500000),
        "capacity-comparison": (0x80104E28, 0x0050102A),
        "empty-socket-text": (0x80104E40, 0x2442681C),
        "gem-stride": (0x80104E50, 0x00022940),
        "gem-conversion": (0x80104E74, 0),
        "gem-format": (0x80104E8C, 0),
        "gem-width": (0x80104E98, 0x24100096),
        "component-row-base": (0x80104EA0, 0x26510015),
        "deactivation-call": (0x80104EA4, 0),
        "screen-edge": (0x80104EB4, 0x2413013F),
        "vertical-pitch": (0x80104EC0, 0x001230C0),
        "vertical-top": (0x80104EC4, 0x24C60013),
        "init-low-bit": (0x80104EE4, 0x32820002),
        "empty-unselectable": (0x80104EE8, 0x2C630000),
        "slide-state": (0x80104EF4, 0x24020003),
        "slide-target": (0x80104F00, 0),
        "fixed-gem-icon": (0x80104F04, 0x24020017),
    },
    {
        "leave-sound": (0x80104F38, 0),
        "clear-call": (0x80104F40, 0),
        "clear-sentinel": (0x80104F44, 0x24040027),
        "flyout-call": (0x80104F48, 0),
        "flyout-row": (0x80104F4C, 0x24040001),
        "page-call": (0x80104F50, 0),
        "page-command": (0x80104F54, 0x24040000),
        "primary-call": (0x80104F58, 0),
        "primary-command": (0x80104F5C, 0x24040001),
        "clear-bars-gate": (0x80104F60, 0x16000003),
        "bars-call": (0x80104F68, 0),
        "bars-command": (0x80104F6C, 0x24040008),
    },
)


@functools.cache
def verify_original(images):
    """Keep all disc reads and the optional decoder out of metadata-only loading."""
    from capstone import CS_ARCH_MIPS, CS_MODE_LITTLE_ENDIAN, CS_MODE_MIPS32, Cs

    decoder = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_LITTLE_ENDIAN)
    for module, image, base, expected in zip(MODULES, images, BASES, MODULE_HASHES, strict=True):
        if image.base != base or hashlib.sha256(image.data).hexdigest() != expected:
            raise RuntimeError(f"{module}: original image identity changed")
    image = images[0]
    for bounds, expected, release in zip(BOUNDARIES, SPAN_HASHES, STACK_RELEASES, strict=True):
        entry, end = bounds
        if not image.contains(entry, end - entry) or image.sha256(entry, end) != expected:
            raise RuntimeError(f"original span {entry:08X}-{end:08X} changed")
        instructions = list(decoder.disasm(image.data[entry - image.base:end - image.base], entry))
        if (len(instructions) * 4 != end - entry or image.read(end - 8) != 0x03E00008
                or image.read(end - 4) != 0x27BD0000 | release):
            raise RuntimeError(f"original span {entry:08X} lost its decoded return and delay slot")
    if image.base + len(image.data) != STATUS + STATUS_BYTES:
        raise RuntimeError("borrowed status strings no longer end at MENU4's file extent")
    return images


@functools.lru_cache(maxsize=16)
def fixture(seed):
    memory = dict(component_fixture(seed))
    for base, length, stride in ((BUFFER, 96, 3), (0x80102140, 10, 29)):
        memory.update((base + index, (seed + index * stride) & 255) for index in range(length))
    pointers = {0x8010229C: NAMES, 0x80102540: HELP, 0x800F4E8C: BUFFER}
    for slot in range(10):
        pointers[CONTEXT + slot * 4] = 0
        pointers[CONTEXT + 40 + slot * 4] = HELP + 2048 + (900 + slot) * 16
    for address, pointer in pointers.items():
        memory.update((address + index, byte)
                      for index, byte in enumerate(pointer.to_bytes(4, "little")))
    for offset in range(STATUS_BYTES):
        memory[STATUS + offset] = END
    for entry in range(12):
        offset = 64 + entry * 8
        memory[STATUS + entry * 2] = offset // 2
        memory[STATUS + entry * 2 + 1] = 0
        memory[STATUS + offset] = 1 + (seed + entry) % 220
        memory[STATUS + offset + 1] = 1 + entry
        memory[STATUS + offset + 2] = END
    for row in range(ROW_COUNT):
        start = ROWS + row * ROW_BYTES
        for offset in range(60):
            memory[start + offset] = (seed + row * 13 + offset * 7) & 255
        for offset, byte in enumerate(SUBTEXT.to_bytes(4, "little")):
            memory[start + 60 + offset] = byte
    return memory


@functools.cache
def original_navigation():
    from differential import Image

    image = Image.load(MODULES[1])
    return image.data[NAVIGATION - image.base:NAVIGATION - image.base + 84]


def vector(rng, number, kind):
    record = bytearray(rng.getrandbits(8) for _ in range(RECORD_SIZES[kind]))
    row = number % 21 if kind == 0 else number % 10
    flags = (0, 0x10, 0x20, 0x40, 0x60, 0x70, 0x73, rng.getrandbits(32))[number % 8]
    repeat = ((number // 21) % 16) << 12
    links = bytearray(original_navigation())
    if kind == 0 and number % 4 == 3:
        cycle = list(range(21))
        rng.shuffle(cycle)
        for index, element in enumerate(cycle):
            for direction, step in enumerate((1, -1, 2, -2)):
                links[element * 4 + direction] = cycle[(index + step) % 21]
    if kind in (4, 5):
        row = 1 + number % 6
        capacity = (number // 6) % 4
        if kind == 4:
            record[24:26] = (17 + (number % 79) + (number % 5) * 256).to_bytes(2, "little")
            record[28] = 1 + number % 10
            record[43] = 1 + number % 7
            record[72:74] = (96 + number % 16).to_bytes(2, "little")
            record[76] = 1 + number % 4
            record[93] = capacity
            first_gem = 120
        else:
            record[45] = capacity
            first_gem = 72
        for slot in range(3):
            identifier = 0 if (number // 3 + slot) % 3 == 0 else 261 + (number + slot) % 62
            offset = first_gem + slot * 48
            record[offset:offset + 2] = identifier.to_bytes(2, "little")
    return {
        "kind": kind, "row": row, "flags": flags, "repeat": repeat | (rng.getrandbits(32) & ~0xF000),
        "page": (0, 1, 2, 3, 127, 128, 255)[(number // 8) % 7],
        "command": (0, 1, 2, 3, -1, -2, 7, 255, -0x80000000, 0x7FFFFFFF)[number % 10],
        "seed": (number % 16) * 17, "record": bytes(record), "links": bytes(links),
    }


def run(machine, value, entry):
    verify_original(tuple(machine.images))
    machine.overlay.update(fixture(value["seed"]))
    kind, record = value["kind"], value["record"]
    machine.store(ITEM, record)
    machine.put(REPEAT, value["repeat"])
    machine.put(PAGE, value["page"], 1)
    machine.store(NAVIGATION, value["links"])
    events = []
    machine.follow.update({
        *(pair[0] for pair in BOUNDARIES), 0x8010341C,
        0x8006B57C, 0x8006B6AC, 0x8006B728,
        0x800FCAA4, 0x800FCC0C, 0x800FD0E0,
        0x800C685C, 0x800C01E0, 0x800490B0, 0x80049110,
    })

    def arguments():
        return [machine.load(CONTEXT + 40 + slot * 4) for slot in range(10)]

    def row_pointer(active):
        row = active.r[R["a0"]]
        if row >= ROW_COUNT:
            raise Fault(f"row endpoint outside {ROW_COUNT} records: {row}")
        return ROWS + row * ROW_BYTES

    def menu_row(active):
        active.r[R["v0"]] = row_pointer(active)

    def create(active):
        pointer = row_pointer(active)
        address = active.load(active.r[R["sp"]] + 20)
        text = []
        for index in range(512):
            text.append(active.load(address + index, 1))
            if text[-1] == END:
                break
        else:
            raise Fault("row text has no terminator")
        events.extend((2, *(active.r[R[name]] for name in ("a0", "a1", "a2", "a3")),
                       active.load(active.r[R["sp"]] + 16), address, len(text), *text))
        events.extend(active.bytes(BUFFER, 96))
        events.extend(arguments())
        active.r[R["v0"]] = pointer

    machine.stub(0x800C8E48, menu_row)
    machine.stub(0x800C8E5C, create)
    for address, event in (
        (0x800FA8A0, 1), (0x800FFB90, 3), (0x800FBBD4, 4), (0x800FBEA4, 5),
        (0x800FA8E0, 7), (0x800FA810, 8), (0x800FC268, 9),
    ):
        machine.stub(address, lambda active, code=event: events.extend((code, active.r[R["a0"]])))
    machine.stub(0x800C02E8, lambda active: events.append(6))
    machine.r[R["a0"]] = value["row"]
    machine.r[R["a1"]] = value["flags"] if kind == 0 else value["command"] & 0xFFFFFFFF
    if kind in (4, 5):
        machine.r[R["a1"]] = ITEM
        machine.r[R["a2"]] = value["command"] & 0xFFFFFFFF
    elif kind == 6:
        machine.r[R["a0"]] = value["command"] & 0xFFFFFFFF
    machine.execute(entry)
    values = [
        machine.r[R["v0"]] if kind == 0 else 0,
        *machine.bytes(ROWS, ROW_COUNT * ROW_BYTES),
        *machine.bytes(BUFFER, 96), *arguments(), *machine.bytes(ITEM, len(record)),
        len(events), *events,
    ]
    names = [
        "selection",
        *(f"row[{row}].byte[{byte}]" for row in range(ROW_COUNT) for byte in range(ROW_BYTES)),
        *(f"buffer[{index}]" for index in range(96)),
        *(f"argument[{index}]" for index in range(10)),
        *(f"loadout[{index}]" for index in range(len(record))),
        "event-words", *(f"event[{index}]" for index in range(len(events))),
    ]
    observation = {"values": values, "names": names, "stores": list(machine.stores)}
    inputs = [kind, value["seed"], value["row"], value["flags"], value["repeat"], value["page"],
              value["command"], *value["links"], *record]
    return observation, inputs


def allowed_stores(value):
    allowed = [(DEFAULT_STACK - 4096, DEFAULT_STACK)]
    kind, row = value["kind"], value["row"]

    def fields(index, spans):
        base = ROWS + index * ROW_BYTES
        allowed.extend((base + start, base + end) for start, end in spans)

    if kind == 1:
        fields(row + 10, ((0, 1), (6, 7), (24, 26), (60, 64)))
    elif kind in (2, 3):
        fields(32, ((0, 1), (6, 7), (9, 10), (24, 26)))
        fields(row + 10, ((0, 1), (24, 26)))
    elif kind in (4, 5):
        fields(row + 20, ((0, 1), (7, 8), (9, 10), (20, 22), (24, 26)))
        if kind == 4:
            fields(row + 20, ((12, 13),))
        allowed.extend(((BUFFER, BUFFER + 96), (CONTEXT + 40, CONTEXT + 80)))
    return allowed


def check(image, value, observation, native, reference):
    del image, reference
    if not native or native[0]:
        return ["native refused a valid original vector"]
    return printed_problems(observation, native[1:], allowed_stores(value))


def describe(image, results):
    del image
    return [
        "All seven spans decoded through jr ra and its stack-release delay slot; image/span hashes checked.",
        "The original selectability, inventory conversions and complete item/text formatters execute.",
        "Doubles: row lookup/create/deactivate, flyouts, sound, stat pages, primary panel and DP/PP bars.",
        f"Compared all 40 rows (pointer-normalized), buffers, string arguments, source records and "
        f"ordered callbacks/text for {len(results)} vectors; stack scratch, downstream rendering and "
        "incidental void return registers are excluded.",
        "Navigation includes original links, permuted terminating graphs, page/flag boundaries and "
        "all button priorities; malformed links/cycles and native refusal paths have focused tests.",
        "No private state: borrowed status text is file-backed through MENU4 file end 0x80108208.",
    ]


def case(kind):
    entry, end = BOUNDARIES[kind]
    return {
        "ID": IDS[kind], "ENTRY": entry, "END": end,
        "MODULES": MODULES, "DRIVER": DRIVER, "SOURCES": SOURCES, "INCLUDES": INCLUDES,
        "COMPILER": COMPILER, "CXX_STANDARD": CXX_STANDARD, "COMPILER_OPTIONS": COMPILER_OPTIONS,
        "VECTORS": 1024, "SEED": int(IDS[kind][1:]), "MUTANT_VECTORS": 256,
        "MUTANTS": {name: {address: word} for name, (address, word) in MUTATIONS[kind].items()},
        "vector": lambda rng, number: vector(rng, number, kind),
        "run": lambda machine, value: run(machine, value, entry),
        "check": check, "describe": describe,
    }
