"""Original HUD instructions with bounded, explicit external-service contracts."""

from __future__ import annotations

from functools import partial

from differential import DEFAULT_STACK, MASK, Machine, s32, store_problems

SPECS = {
    "F01361": (0, 0x800C9FE8, 0x800CA2DC),
    "F01362": (1, 0x800CA2DC, 0x800CA97C),
    "F01384": (2, 0x800CB56C, 0x800CB654),
    "F00729": (3, 0x80095B70, 0x80095B7C),
    "F00730": (4, 0x80095B7C, 0x80095C18),
    "F00743": (5, 0x8009722C, 0x8009723C),
}
MUTATIONS = {
    "F00729": {
        "byte-instead-of-halfword-command": {0x80095B78: 0xA0442270},
        "wrong-command-source": {0x80095B78: 0xA4402270},
    },
    "F00730": {
        "pin-mask": {0x80095B88: 0x30A20020},
        "pin-priority": {0x80095B8C: 0x14000008},
        "unpin-mask": {0x80095BB0: 0x30A20020},
        "unpin-preserve-bits": {0x80095BC8: 0x3042003F},
        "kind-retains-pin": {0x80095BE8: 0x30630000},
        "kind-low-bit": {0x80095BE4: 0x30A4007E},
        "bulk-extent": {0x80095C04: 0x2406000F},
    },
    "F00743": {
        "suppression-command": {0x80097230: 0x24020000},
        "byte-instead-of-halfword-suppression": {0x80097238: 0xA062227A},
    },
    "F01384": {
        "inverted-hour-saturation": {0x800CB5A0: 0x14600006},
        "saturated-minutes-and-seconds": {0x800CB5A8: 0x2402003A},
        "saturated-hundredths": {0x800CB5B4: 0x24020062},
        "unsigned-clock-components": {0x800CB5D0: 0x80440000},
        "tens-character": {0x800CB5E0: 0x24630031},
        "last-digit-pair": {0x800CB5EC: 0x24840005},
        "ones-character": {0x800CB5FC: 0x24420031},
        "all-three-pairs": {0x800CB60C: 0x2A220002},
    },
    "F01361": {
        "opening-direction": {0x800CA02C: 0x24420001},
        "hidden-step": {0x800CA060: 0x24020009},
        "panel-direction": {0x800CA09C: 0x00609821},
        "ability-shortcut-selection": {0x800CA0A0: 0x26500000},
        "marker-packing": {0x800CA0CC: 0x30637FFF},
        "marker-height-packing": {0x800CA0D0: 0x000423C0},
        "icon-draw-mode": {0x800CA114: 0x34420016},
        "unsigned-icon-texture": {0x800CA12C: 0x84420000},
        "badge-size": {0x800CA164: 0x34C6001D},
        "opposite-panel-origin": {0x800CA188: 0x2403009F},
        "spell-icon-inset": {0x800CA208: 0x24A5FFFF},
        "reverse-digit-order": {0x800CA25C: 0x24100002},
        "consume-request": {0x800CA2D0: 0},
    },
    "F01362": {
        "shortcut-before-early-return": {0x800CA31C: 0},
        "only-exact-armed-mode": {0x800CA32C: 0x24020003},
        "indicator-step-bound": {0x800CA344: 0x2C620003},
        "unsigned-indicator-step": {0x800CA378: 0x8062B9CD},
        "indicator-closing-direction": {0x800CA36C: 0x24420001},
        "indicator-position": {0x800CA3B4: 0x34A500ED},
        "indicator-phase-mask": {0x800CA3DC: 0x30840FFF},
        "indicator-sine-scale": {0x800CA3F0: 0x00021103},
        "indicator-phase-rate": {0x800CA408: 0x00021140},
        "hud-visibility": {0x800CA420: 0x14400009},
        "fully-hidden-return": {0x800CA42C: 0x3C020018},
        "outward-slide-step": {0x800CA434: 0x3C020004},
        "menu-background-limb-gate": {0x800CA45C: 0x30420001},
        "limb-layout-mode": {0x800CA4B4: 0x30630002},
        "all-five-limbs": {0x800CA534: 0x2A420004},
        "signed-hp": {0x800CA55C: 0x96C50018},
        "unsigned-risk": {0x800CA57C: 0x86D40020},
        "status-hidden-command": {0x800CA5D8: 0x104000DC},
        "carousel-stale-icon-mask": {0x800CA658: 0x30420002},
        "carousel-rotation-step": {0x800CA67C: 0x2402000B},
        "carousel-middle-store": {0x800CA6A4: 0xA0A20000},
        "carousel-fast-tick": {0x800CA7B8: 0x24020002},
        "first-icon-slot": {0x800CA7D4: 0x24060000},
        "count-dot-origin": {0x800CA850: 0x24100095},
        "count-dot-size": {0x800CA860: 0x34C60003},
        "selection-row-bit": {0x800CA8A0: 0x32C20010},
        "count-row-width": {0x800CA8E0: 0x24120018},
        "count-row-texture": {0x800CA920: 0x34630C01},
    },
}
STATE = (
    0x800EB9AF,
    0x800EB9CC,
    0x800F4E90,
    0x800EBC78,
    0x800EBCE8,
    0x800EB9CD,
    0x800EBD64,
    0x800EB9AE,
    0x800EB9B0,
)
CAROUSEL = 0x800EB9D0
ACTOR, STATS, NEXT_STATS = 0x80500000, 0x80510000, 0x80520000
PACKETS, TEXT, TIME = 0x80600000, 0x80530000, 0x80540000
SCRATCH = 0x1F800000
MARKERS, HIDDEN, SUPPRESSED = 0x800F2260, 0x800F2270, 0x800F227A
ICON_COORDS = (0x800EBCA0, 0x800EBCC8)
ICON_SIZES = (0x800EBCA8, 0x800EBCD0)
ICON_TEXTURES = (0x800EBCB8, 0x800EBCE0)


def vector_for(identifier, rng, number):
    operation = SPECS[identifier][0]
    scalar = [
        operation,
        rng.getrandbits(32),
        number % 16,
        rng.getrandbits(16),
        rng.getrandbits(16),
        rng.choice((0, 0, 1, 255)),
        (0, 1, 2, 3, 129, 255)[number % 6],
        rng.randrange(4),
        (0, 1, 2, 255)[number % 4],
        number % 12,
        (0, 1, 3, 4, 127, 128, 255)[number % 7],
        rng.getrandbits(16),
        128 if number % 13 == 0 else 0,
        (0, 0x80000, 0x180000, 0x200000, 1, -1, 0x7FFFFFFF, -0x80000000)[number % 8],
        *[rng.choice((0, 1, 7, 8, 15, 30, 31, 32, 127, 255)) for _ in range(3)],
        (0, 1, 2, 8, 9, 10, 11, 12, 13, 14, 15, 255)[number % 12],
        (0, 1, 2, 4, -1, 0x7FFFFFFF)[number % 6],
        rng.choice((-4096, -33, -1, 0, 31, 4096, rng.getrandbits(32))),
        (0, 1, 100, 32767, 32768, 65535)[number % 6],
        (0, 1, 3, 7, 255, 511, 65535, MASK, 0x80000000, 0x80000001)[number % 10],
        rng.choice((0, 1, 4, -1, -32768)),
        rng.choice((0, 3, 127, 255)),
        0x80700100,
        0x80702084,
        0x80710100,
        0x80712084,
        number % 64,
        rng.getrandbits(24),
        rng.getrandbits(8),
        ord(":"),
        ord(":"),
        0,
        *[rng.getrandbits(16) for _ in range(4)],
    ]
    if number % 3 == 0:
        scalar[29] |= rng.choice((1, 127, 128, 255)) << 24
    if operation == 3:
        scalar[1] = (0, 1, 2, -1, 0x10000, 0x12348001)[number % 6]
    if operation == 4:
        scalar[1] = (
            0,
            1,
            7,
            63,
            64,
            65,
            127,
            128,
            129,
            192,
            255,
            -1,
            rng.getrandbits(32),
        )[number % 13]
    if operation == 0:
        scalar[9] = number % 12
        if scalar[8] != 0 and scalar[9] == 0:
            scalar[9] = 0
    if operation == 1 and number < 256:
        scalar[28] = number % 4
        scalar[6] = (0, 1, 2, 3)[number % 4]
        scalar[10] = (0, 1, 3, 4, 255)[number % 5]
        scalar[13] = (0, 0x80000, 0x180000, 0x200000)[(number // 4) % 4]
        scalar[5] = (number // 16) % 2
        scalar[7] = 2 if number % 7 == 0 else 0
        scalar[22] = 0
        scalar[17] = (number // 2) % 16
        if number % 8 == 0:
            scalar[14:17] = [0, 0, 0]
        elif number % 8 == 1:
            scalar[14:17] = [0, 1, 0]
    tables = [
        *[rng.getrandbits(8) for _ in range(16)],
        *[min(index + 1, 10) for index in range(12)],
        *[rng.getrandbits(8) for _ in range(20)],
        *[rng.choice((0, 0x140, 0x180)) for _ in range(8)],
        *[rng.getrandbits(8) for _ in range(32)],
        *[rng.getrandbits(32) for _ in range(8)],
        *[rng.getrandbits(16) for _ in range(8)],
        *[rng.getrandbits(32) for _ in range(34)],
        *[rng.getrandbits(32) for _ in range(5)],
        *[rng.randrange(5) for _ in range(5)],
    ]
    result = [(value & MASK) for value in (*scalar, *tables)]
    assert len(result) == 186, len(result)
    return result


def run_for(identifier, machine: Machine, vector):
    operation, entry, _ = SPECS[identifier]
    v = vector
    for address, size in (
        (0x800F0000, 0x10000),
        (0x80061000, 0x1000),
        (SCRATCH, 0x100),
        (ACTOR, 0x1000),
        (STATS, 0x1000),
        (NEXT_STATS, 0x1000),
        (PACKETS, 0x10000),
        (TEXT, 0x100),
        (TIME, 0x100),
    ):
        machine.map_zero(address, size)
    for index, address in enumerate(STATE):
        size = 2 if index == 6 else (4 if index == 8 else 1)
        machine.put(address, v[5 + index], size)
    for index in range(4):
        machine.put(CAROUSEL + index, v[14 + index], 1)
    machine.put(HIDDEN, v[3], 2)
    machine.put(SUPPRESSED, v[4], 2)
    machine.put(0x8005E24C, v[18])
    machine.put(0x800F19FC, ACTOR)
    machine.put(ACTOR + 0x3C, STATS)
    machine.put(SCRATCH + 4, v[24])
    machine.put(SCRATCH + 8, v[25])
    machine.put(0x800F514C, v[22], 2)
    machine.put(0x800F5152, v[23], 1)
    machine.put(STATS + 0x20, v[20], 2)
    for index in range(4):
        machine.put(STATS + 0x18 + index * 2, v[34 + index], 2)
    machine.put(0x800EC258, TEXT)
    machine.put(TIME, v[29])
    machine.put(0x80061638, v[30], 1)
    text = bytearray(b"00:00:00\0")
    text[2], text[5], text[8] = v[31:34]
    machine.store(TEXT, text)
    blocks = [
        (MARKERS, 38, 16, 1),
        (0x800EBC7C, 54, 12, 1),
        (0x800EBBC8, 66, 20, 1),
        (0x800EBC98, 94, 8, 1),
        (0x800EBCC0, 102, 8, 1),
        (ICON_COORDS[0], 110, 8, 1),
        (ICON_COORDS[1], 118, 8, 1),
        (ICON_SIZES[0], 126, 4, 4),
        (ICON_SIZES[1], 130, 4, 4),
        (ICON_TEXTURES[0], 134, 4, 2),
        (ICON_TEXTURES[1], 138, 4, 2),
        (0x800EBC88, 142, 4, 4),
        (0x800EBCEC, 146, 10, 4),
        (0x800EBD14, 156, 10, 4),
        (0x800EBD3C, 166, 10, 4),
        (0x800EBC54, 176, 5, 4),
    ]
    for address, start, count, size in blocks:
        for index in range(count):
            machine.put(address + index * size, v[start + index], size)
    conditions = list(v[181:186])
    events, packets = [], []

    def mutate(event):
        flags = v[28]
        tag = event[0]
        if flags & 1 and tag == 4:
            machine.put(SCRATCH + 4, v[26])
            machine.put(SCRATCH + 8, v[27])
            machine.put(ACTOR + 0x3C, NEXT_STATS)
        if flags & 2 and tag == 3:
            machine.put(STATE[3], 77, 1)
            machine.put(STATE[1], machine.load(STATE[1], 1) ^ 1, 1)
            for base in ICON_TEXTURES:
                for index in range(4):
                    address = base + index * 2
                    machine.put(address, machine.load(address, 2) ^ 0x101, 2)
            for index in range(5):
                conditions[index] = (conditions[index] + 1) % 5
            machine.put(STATE[8], machine.load(STATE[8]) + 0x10000)
        if flags & 4 and tag == 2:
            for row, base in enumerate(ICON_COORDS):
                for index in range(8):
                    machine.put(
                        base + index,
                        machine.load(base + index, 1) + (7 if index < 4 else 1),
                        1,
                    )
                for index in range(4):
                    address = ICON_SIZES[row] + index * 4
                    machine.put(address, machine.load(address) ^ 0x10001)
        if flags & 8 and tag == 5:
            address = packets[-1] + 8
            machine.put(address, machine.load(address) ^ 0xFFFF)
            machine.put(STATE[6], machine.load(STATE[6], 2) + 0x1234, 2)
            machine.put(0x8005E24C, 4)
        if flags & 16 and tag == 6:
            machine.put(STATS + 0x1C, machine.load(STATS + 0x1C, 2) ^ 0x4000, 2)
            machine.put(STATS + 0x20, machine.load(STATS + 0x20, 2) ^ 0xFFFF, 2)
        if flags & 32 and tag == 10:
            machine.put(CAROUSEL, machine.load(CAROUSEL, 1) ^ 1, 1)
            machine.put(CAROUSEL + 1, machine.load(CAROUSEL + 1, 1) ^ 2, 1)
            machine.put(SCRATCH + 4, v[26])
            machine.put(STATE[8], machine.load(STATE[8]) + 0x10000)

    def emit(*values):
        event = [value & MASK for value in (*values, *([0] * (6 - len(values))))]
        events.append(event)
        mutate(event)

    def sprite(m, page):
        brightness, position, size, chain = m.r[4:8]
        pointer = PACKETS + len(packets) * 24
        packets.append(pointer)
        color = ((0x64 | (2 if brightness & 0x100 else 0)) << 24) | (
            (brightness & 255) * 0x10101
        )
        for index, word in enumerate(
            (0, 0xE100000C if page else 0, color, position, 0, size)
        ):
            m.put(pointer + index * 4, word)
        emit(3, page, brightness, position, size, chain)
        m.r[2] = pointer

    def tone(m):
        shortcut = m.r[4]
        emit(1, shortcut)
        m.r[2] = v[86 + shortcut] if shortcut < 8 else 0

    def limb(m):
        owner = m.load(ACTOR + 0x3C)
        index = (m.r[4] - owner - 0x398) // 0xDC
        emit(9, index)
        m.r[2] = conditions[index] if 0 <= index < 5 else 0xFFFFFFFF

    def gauge(m):
        emit(6, m.r[4], m.r[5], m.r[6])

    def sine(m):
        emit(5, m.r[4])
        m.r[2] = v[19]

    def status(m):
        emit(8, 0 if m.r[4] == STATS else 1)
        m.r[2] = v[21]

    def text_draw(m):
        if operation != 2:
            emit(7, m.r[5], m.r[6])

    def memset(m):
        address, value, count = m.r[4:7]
        for offset in range(count):
            m.stores.append((address + offset, 1))
            m.put(address + offset, value, 1)

    machine.stub(0x800C0224, partial(sprite, page=1))
    machine.stub(0x800C0230, partial(sprite, page=0))
    machine.stub(0x800C9EB8, tone)
    machine.stub(0x800C9F98, lambda m: emit(2, m.r[4], m.r[5]))
    machine.stub(0x800C9FE8, lambda _m: emit(4))
    machine.stub(0x80040F28, sine)
    machine.stub(0x800C9E5C, limb)
    machine.stub(0x800C9B38, gauge)
    machine.stub(0x800C9E08, status)
    machine.stub(0x800C9CB4, lambda m: emit(10, *m.r[4:7]))
    machine.stub(0x800C6828, text_draw)
    machine.stub(0x80026EB4, memset)
    machine.follow.add(0x800CCC54)
    if operation in (3, 4):
        machine.r[4] = v[1] if operation == 3 else v[2]
        machine.r[5] = v[1]
    elif operation == 2:
        machine.r[4] = TIME
    machine.stores.clear()
    machine.execute(entry)
    output = [
        0,
        s32(machine.load(HIDDEN, 2, signed=True)) & MASK,
        s32(machine.load(SUPPRESSED, 2, signed=True)) & MASK,
    ]
    output.extend(machine.load(MARKERS + index, 1) for index in range(16))
    output.extend(
        machine.load(address, 2 if index == 6 else (4 if index == 8 else 1))
        for index, address in enumerate(STATE)
    )
    output.extend(machine.load(CAROUSEL + index, 1) for index in range(4))
    output.extend(
        (
            machine.load(0x8005E24C),
            machine.load(STATS + 0x20, 2),
            machine.load(SCRATCH + 4),
            machine.load(SCRATCH + 8),
            len(events),
        )
    )
    for event in events:
        output.extend(event)
    output.append(len(packets))
    for pointer in packets:
        output.extend(machine.load(pointer + index * 4) for index in range(1, 6))
    output.extend(machine.load(TEXT + index, 1) for index in range(9))
    return {
        "output": output,
        "stores": machine.stores,
        "events": events,
        "sprite_count": len(packets),
    }, vector


def check_for(_identifier, _image, _vector, observation, native, _reference):
    expected = observation["output"]
    problems = []
    if native != expected:
        differing = next(
            (
                index
                for index, (left, right) in enumerate(
                    zip(native, expected, strict=False)
                )
                if left != right
            ),
            min(len(native), len(expected)),
        )
        problems.append(
            f"word {differing}: native {native[differing : differing + 8]} "
            f"original {expected[differing : differing + 8]} "
            f"lengths {len(native)}/{len(expected)}"
        )
    allowed = [
        (DEFAULT_STACK - 0x38, DEFAULT_STACK),
        (MARKERS, MARKERS + 16),
        (HIDDEN, HIDDEN + 2),
        (SUPPRESSED, SUPPRESSED + 2),
        (CAROUSEL, CAROUSEL + 4),
        (PACKETS, PACKETS + observation["sprite_count"] * 24),
        (TEXT, TEXT + 9),
    ]
    allowed.extend(
        (address, address + (2 if index == 6 else (4 if index == 8 else 1)))
        for index, address in enumerate(STATE)
    )
    return problems + store_problems(observation["stores"], allowed)
