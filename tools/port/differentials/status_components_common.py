"""MENU4 component descriptions: real text/conversion code, external display endpoints only."""

from __future__ import annotations

import functools
import hashlib

from differential import DEFAULT_STACK, MODULE_BASES, Fault, Image, R, printed_problems

MODULES = (r"MENU\MENU4.PRG", r"MENU\MAINMENU.PRG", r"BATTLE\BATTLE.PRG", "SLUS_010.40")
BASES = (0x80102800, 0x800F9800, 0x80068800, 0x80010000)
MODULE_BASES.update(dict(zip(MODULES, BASES, strict=True)))
MODULE_HASHES = (
    "afcf4e260b57719b2980e9f52b1dc8a2e568c7f21978df8499ea853f2c0ae7b2",
    "fa66ce05b759c2e1ba2459640b23e966cf8859418112ca4c2c0f9c8fd3a8a1c4",
    "ad6914c81be92a008af8dece6ad394ce39c5aa42360542b5531716ebd00bba12",
    "9f1fce345008c836522e3b76a0046f54c7992ce0c71f41c2080d38d8c51cc41b",
)
BOUNDARIES = (
    (0x80102F64, 0x80103080),
    (0x80103080, 0x80103118),
    (0x80103118, 0x8010317C),
    (0x8010317C, 0x801032C4),
    (0x801032C4, 0x801033A4),
    (0x801033A4, 0x801033D4),
    (0x801033D4, 0x8010341C),
    (0x8010341C, 0x801034BC),
)
SPAN_HASHES = (
    "e649be45af6a77379212d9891b7cda6f632ee916485f1d19f91cf7d92cce7e92",
    "12d46ff356de6d0ea76670eb600c76e926140d453726830a16f5d1607cc91f1a",
    "e7072276e44bf35b1c018a88fe043a350061ae6cc49f86901a94719825083695",
    "856b12f1df5959614234f633eb0115a82140147feb843db5419ae078053ee4ca",
    "1bab8530c6f3e7a1da52050f9b83c3b4d43421a4966a99bdbd6bc718e184ea21",
    "326a8bac3e40e8e82dafdbbabc2a8d9e2f007dff97026ab090e885376d022989",
    "75d561c6e88a570d15004b3ab8c2b146b47e30a76efd4609a4861fd51d9a27e5",
    "ef51965808d20c335e9bf83def01d4db395dc8d4312d2b3a576590b8810454bf",
)
DRIVER = "status_components_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Status\ComponentDescriptions.cpp",
    r"Native\Game\src\Menu\Framework\ItemRows.cpp",
    r"Native\Game\src\Items\EquipmentStats.cpp",
    r"Native\Game\src\Items\EquipmentAffinity.cpp",
    r"Native\Engine\src\Render\EncodedText.cpp",
    r"Native\Engine\src\Core\String\GameText.cpp",
)
INCLUDES = (r"Native\Tests\include",)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
COMPILER_OPTIONS = ("/W4", "/WX", "/Gy", "/Gw", "/MD", "/DNOMINMAX")
ITEM, BUFFER, HELP, NAMES = 0x80410000, 0x80420000, 0x80500000, 0x80600000
ROWS, OUTPUT, ROW_TYPE, HEAP = 0x80430000, 0x80440000, 0x80440020, 0x80450000
CONTEXT, PAGE = 0x800F5168, 0x801024B9
RECORD_SIZES = (0x30, 0x30, 0x30, 0x198, 0x164, 0xBC, 0x60, 0)
ENTRY_BASE, ENTRY_STRIDE, ENTRY_COUNT = 2048, 16, 960
END = 0xE7

MUTATIONS = (
    {
        "material-signed-load": (0x80102F88, 0x80830013),
        "material-name-row": (0x80102FA0, 0x244217D0),
        "category-help": (0x80102FBC, 0x9442031E),
        "damage-help": (0x80102FE0, 0x94420332),
        "damage-signed-load": (0x80102FD0, 0x80820010),
        "hand-category-index": (0x80103004, 0x90620000),
        "hand-help": (0x80103014, 0x9442033A),
        "base-description": (0x80103038, 0x94450000),
        "first-format-call": (0x80103044, 0),
        "second-format-call": (0x80103054, 0),
        "description-output-slot": (0x80103064, 0xAE420000),
    },
    {
        "category-signed-load": (0x8010309C, 0x80820004),
        "category-help": (0x801030AC, 0x94420502),
        "context-slot": (0x801030BC, 0xACA251A8),
        "id-bias": (0x801030D0, 0x9445FFF6),
        "first-format-call": (0x801030DC, 0),
        "template-offset": (0x801030F0, 0x24A5680C),
        "description-output-slot": (0x801030FC, 0xAE420000),
    },
    {
        "copy-length": (0x80103130, 0x24060050),
        "gem-id-bias": (0x80103148, 0x9445FEEA),
        "copy-call": (0x80103154, 0),
        "description-output-slot": (0x80103164, 0xAE220000),
    },
    {
        "dispatch-upper-bound": (0x801031A4, 0x2E420005),
        "no-gems-pointer": (0x8010319C, 0x2442681E),
        "weapon-format-call": (0x801031E0, 0),
        "blade-description-call": (0x801031F8, 0),
        "blade-stats-call": (0x80103200, 0),
        "grip-part-offset": (0x80103210, 0x26300018),
        "grip-stats-call": (0x80103224, 0),
        "gem-slot-address": (0x80103248, 0x946200A8),
        "gem-description-call": (0x80103270, 0),
        "gem-stats-call": (0x80103278, 0),
        "empty-bars": (0x8010328C, 0x24040009),
        "empty-reset-call": (0x80103290, 0),
        "highlight-row": (0x8010329C, 0x26440015),
        "highlight-value": (0x801032A0, 0x24030000),
        "returned-description": (0x801032A8, 0x8FA20010),
    },
    {
        "shield-format-call": (0x801032FC, 0),
        "negative-row-boundary": (0x8010330C, 0x0621001B),
        "gem-upper-bound": (0x80103310, 0x2A220003),
        "gem-slot-address": (0x8010332C, 0x94620078),
        "gem-description-call": (0x80103354, 0),
        "gem-stats-call": (0x8010335C, 0),
        "empty-bars": (0x80103370, 0x24040009),
        "empty-reset-call": (0x80103374, 0),
        "highlight-row": (0x80103380, 0x26240015),
        "highlight-value": (0x80103384, 0x24030000),
        "returned-description": (0x8010338C, 0x8FA20010),
    },
    {
        "row-text-output": (0x801033AC, 0x27A50014),
        "armor-format-call": (0x801033B8, 0),
        "returned-description": (0x801033C0, 0x8FA20010),
    },
    {
        "accessory-conversion-call": (0x801033E8, 0),
        "inventory-format-call": (0x80103400, 0),
        "row-text-output": (0x801033F4, 0x27A50014),
        "returned-description": (0x80103408, 0x8FA20010),
    },
    {
        "element-upper-bound": (0x8010341C, 0x2C820014),
        "pp-bit": (0x80103448, 0x00051183),
        "dp-bit": (0x80103454, 0x00051143),
        "type-page": (0x8010346C, 0x38420001),
        "affinity-page": (0x80103484, 0x38420002),
        "first-component": (0x80103490, 0x2483FFF6),
        "maximum-mask": (0x80103494, 0x30A20007),
        "component-order": (0x80103498, 0x0062102A),
        "range-risk-bit": (0x801034A4, 0x00051143),
        "always-selectable": (0x801034B0, 0x24030000),
    },
)


@functools.cache
def verify_original(images: tuple[Image, ...]):
    """Optional decoder and disc reads belong to proof execution, never case listing."""
    from capstone import CS_ARCH_MIPS, CS_MODE_LITTLE_ENDIAN, CS_MODE_MIPS32, Cs

    decoder = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_LITTLE_ENDIAN)
    for module, image, base, expected in zip(MODULES, images, BASES, MODULE_HASHES, strict=True):
        if image.base != base or hashlib.sha256(image.data).hexdigest() != expected:
            raise RuntimeError(f"{module}: original image identity changed")
    image = images[0]
    for (entry, end), expected in zip(BOUNDARIES, SPAN_HASHES, strict=True):
        if not image.contains(entry, end - entry) or image.sha256(entry, end) != expected:
            raise RuntimeError(f"original span {entry:08X}-{end:08X} changed")
        instructions = list(decoder.disasm(image.data[entry - image.base:end - image.base], entry))
        if len(instructions) * 4 != end - entry or image.read(end - 8) != 0x03E00008:
            raise RuntimeError(f"original span {entry:08X} is not a complete decoded return boundary")
    return images


@functools.lru_cache(maxsize=16)
def fixture(seed):
    help_text = bytearray([END]) * 28672
    names = bytearray([END]) * 12288
    for entry in range(ENTRY_COUNT):
        start = ENTRY_BASE + entry * ENTRY_STRIDE
        help_text[entry * 2:entry * 2 + 2] = (start // 2).to_bytes(2, "little")
        for byte in range(ENTRY_STRIDE):
            help_text[start + byte] = 1 + (seed + entry * 13 + byte * 7) % 220
        help_text[start + 3 + entry % 4] = END
    for row in range(len(names) // 24):
        names[row * 24:row * 24 + 2] = bytes((1 + (row + seed) % 220, 1 + row // 220))
    for halfword, text in (
        (13307, (0xFF, 9, 0x10, 0xFF, 8, 0x11, 0xFF, 7, 0x12, 0xFF, 6, END)),
        (13317, (0x19, 0xFF, 5, END)),
        (13301, (0xFF, 0, 0x13, 0xFF, 1, END)),
        (13319, (0xFF, 8, 0x14, 0xFF, 9, END)),
        (13326, (0x31, 0x32, END)),
    ):
        help_text[halfword * 2:halfword * 2 + len(text)] = bytes(text)
    memory = {HELP + index: byte for index, byte in enumerate(help_text)}
    memory.update({NAMES + index: byte for index, byte in enumerate(names)})
    return memory


def vector(rng, number, kind):
    record = bytearray(rng.getrandbits(8) for _ in range(RECORD_SIZES[kind]))

    def part(offset, identifier, category, damage=1):
        record[offset:offset + 2] = identifier.to_bytes(2, "little")
        record[offset + 4] = category
        record[offset + 16] = damage

    if kind == 0:
        part(0, rng.randrange(1, 960), 1 + number % 10, (number * 53) % 256)
    elif kind == 1:
        part(0, 6 + number % 960, (number * 37) % 256)
    elif kind == 2:
        part(0, 140 + number % 960, 0)
    elif kind == 3:
        part(24, rng.randrange(1, 97), 1 + number % 10, number % 256)
        part(72, 96 + number % 16, 1 + number % 4)
        for gem in range(3):
            part(120 + gem * 48, 0 if (number + gem) % 3 == 0 else 261 + (number + gem) % 62, 0)
    elif kind == 4:
        part(24, 97 + number % 16, 1)
        for gem in range(4):
            part(72 + gem * 48, 0 if (number + gem) % 3 == 0 else 261 + (number + gem) % 62, 0)
    elif kind in (5, 6):
        category = (1, 6, 7, 128, 255)[number % 5]
        part(0, 223 + number % 32 if category == 7 else 145 + number % 78, category)
    row = ((-20, -1, 0, 1, 2, 3, 4, 5, 6, 19)[number % 10]
           if kind == 3 else (-20, -1, 0, 1, 2, 3, 4, 19)[number % 8])
    if kind == 7:
        row = (*range(-2, 23), -0x80000000, 0x7FFFFFFF)[number % 27]
    return {
        "kind": kind, "seed": (number % 16) * 17, "record": bytes(record), "row": row,
        "flags": rng.getrandbits(32), "page": (0, 1, 2, 3, 127, 128, 255)[number % 7],
    }


def run(machine, value, entry):
    verify_original(tuple(machine.images))
    kind, seed, record = value["kind"], value["seed"], value["record"]
    machine.overlay.update(fixture(seed))
    machine.store(ITEM, record)
    machine.store(BUFFER, bytes((seed + index * 3) & 255 for index in range(96)))
    machine.store(0x80102140, bytes((seed + index * 29) & 255 for index in range(10)))
    machine.put(0x8010229C, NAMES)
    machine.put(0x80102540, HELP)
    machine.put(0x800F4E8C, BUFFER)
    for slot in range(10):
        machine.put(CONTEXT + slot * 4, 0)
        machine.put(CONTEXT + 40 + slot * 4, HELP + ENTRY_BASE + (900 + slot) * ENTRY_STRIDE)
    for index in range(40):
        machine.store(ROWS + index * 64, bytes([0xA5]) * 64)
        machine.put(ROWS + index * 64 + 6, seed + index * 5, 1)
    machine.put(OUTPUT, 0x81234560)
    machine.put(OUTPUT + 4, 0x81234564)
    machine.put(ROW_TYPE, 0xA5B6C7D8)
    machine.put(PAGE, value["page"], 1)
    events = []
    allocations = []
    # All in-packet functions, the text expansion/copy, item formatters, title
    # ranking and accessory inventory conversions execute original instructions.
    machine.follow.update({
        *(pair[0] for pair in BOUNDARIES), 0x800C685C, 0x800C01E0, 0x800FA598,
        0x800FC85C, 0x800FCCE8, 0x800FCECC, 0x800FD084, 0x8006BADC,
        0x8006B7BC, 0x8006B110, 0x8006ACFC, 0x8006A9F0, 0x800490B0, 0x80049110,
    })

    def arguments():
        return [machine.load(CONTEXT + 40 + slot * 4) for slot in range(10)]

    def display(active, event, size):
        address = active.r[R["a0"]]
        events.extend((event, address, size))
        events.extend(active.bytes(address, size))
        events.extend(active.bytes(BUFFER, 96))
        events.extend(arguments())

    def menu_row(active):
        row = active.r[R["a0"]]
        if row >= 40:
            raise Fault(f"row endpoint outside 40 records: {row}")
        events.extend((6, row))
        active.r[R["v0"]] = ROWS + row * 64

    def allocate(active):
        size = active.r[R["a0"]]
        if allocations or size != 0x34:
            raise Fault(f"unexpected armor intermediate allocation: {size}")
        allocations.append(HEAP)
        active.store(HEAP, bytes([0xAB]) * size)
        active.r[R["v0"]] = HEAP

    def free(active):
        if allocations != [active.r[R["a0"]]]:
            raise Fault("unexpected armor intermediate release")
        allocations.clear()

    machine.stub(0x80102D64, lambda active: display(active, 1, 0x198))
    machine.stub(0x80102E3C, lambda active: display(active, 2, 0x30))
    machine.stub(0x80102EC0, lambda active: display(active, 3, 0x30))
    machine.stub(0x800FC268, lambda active: events.extend((4, active.r[R["a0"]])))
    machine.stub(0x800FD220, lambda active: events.append(5))
    machine.stub(0x800C8E48, menu_row)
    machine.stub(0x80043E3C, allocate)
    machine.stub(0x80043C60, free)
    if kind <= 2:
        machine.r[R["a0"]], machine.r[R["a1"]], machine.r[R["a2"]] = ITEM, OUTPUT, ROW_TYPE
    elif kind in (3, 4):
        machine.r[R["a0"]], machine.r[R["a1"]] = value["row"] & 0xFFFFFFFF, ITEM
    elif kind in (5, 6):
        machine.r[R["a0"]] = ITEM
    else:
        machine.r[R["a0"]], machine.r[R["a1"]] = value["row"] & 0xFFFFFFFF, value["flags"]
    machine.execute(entry)
    if allocations:
        raise Fault("armor intermediate leaked")
    result = machine.load(OUTPUT + 4) if kind <= 2 else machine.r[R["v0"]]
    values = [
        result, machine.load(OUTPUT), machine.load(ROW_TYPE),
        *machine.bytes(BUFFER, 96), *arguments(),
        *(machine.load(ROWS + index * 64 + 6, 1) for index in range(40)),
        *machine.bytes(ITEM, len(record)), len(events), *events,
    ]
    names = [
        "result", "untouched-name", "untouched-row-type",
        *(f"buffer[{index}]" for index in range(96)),
        *(f"argument[{index}]" for index in range(10)),
        *(f"selected[{index}]" for index in range(40)),
        *(f"source[{index}]" for index in range(len(record))),
        "event-words", *(f"event[{index}]" for index in range(len(events))),
    ]
    observation = {"values": values, "names": names, "stores": list(machine.stores)}
    inputs = [kind, seed, value["row"], value["flags"], value["page"], *record]
    return observation, inputs


def allowed_stores(value):
    allowed = [(DEFAULT_STACK - 4096, DEFAULT_STACK), (BUFFER, BUFFER + 96),
               (CONTEXT + 40, CONTEXT + 80), (HEAP, HEAP + 0x34)]
    if value["kind"] <= 2:
        allowed.append((OUTPUT + 4, OUTPUT + 8))
    if value["kind"] in (3, 4):
        allowed.extend((ROWS + index * 64 + 6, ROWS + index * 64 + 7) for index in range(40))
    return allowed


def check(image, value, observation, native, reference):
    del image, reference
    if not native or native[0]:
        return ["native refused a valid original vector"]
    return printed_problems(observation, native[1:], allowed_stores(value))


def describe(image, results):
    del image
    return [
        "Independent named return spans and all four original image identities verified.",
        "Original text format/copy, row formatters, title rank and accessory conversion executed.",
        "Doubles: external stat displays, bar/reset UI, menu-row lookup, temporary heap.",
        f"Compared full buffers, string pointers, row selections, input bytes and endpoint content/order "
        f"for {len(results)} vectors; uninitialized accessory id=0 is separately refused (#418).",
    ]


def case(kind):
    entry, end = BOUNDARIES[kind]
    return {
        "ID": f"F{2647 + kind:05}", "ENTRY": entry, "END": end,
        "MODULES": MODULES, "DRIVER": DRIVER, "SOURCES": SOURCES, "INCLUDES": INCLUDES,
        "COMPILER": COMPILER, "CXX_STANDARD": CXX_STANDARD, "COMPILER_OPTIONS": COMPILER_OPTIONS,
        "VECTORS": 1024, "SEED": 2647 + kind, "MUTANT_VECTORS": 128,
        "MUTANTS": {name: {address: word} for name, (address, word) in MUTATIONS[kind].items()},
        "vector": lambda rng, number: vector(rng, number, kind),
        "run": lambda machine, value: run(machine, value, entry),
        "check": check, "describe": describe,
    }
