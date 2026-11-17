"""B48-09 limb controllers: actual nested MIPS/core policies and bounded external UI services.

The original sprite builder, status flags, limb condition, limb count, dead-zone
mapping, formatter and all seven packet functions execute, not endpoint doubles.
Only text drawing, preview projection, comparison stats, sound, information display
and cursor rendering are doubled. Observations include complete submitted packets,
chain heads/cursor, all controller fields, sequential halfword stores, formatted
bytes and text-pointer bank/offset/content relations. Metadata imports perform no
image reads and do not import optional Capstone.
"""

from __future__ import annotations

import hashlib
import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MASK,
    MODULE_BASES,
    Fault,
    Machine,
    R,
    mode_case,
    recorded_observation,
    s32,
)

MODULE_BASES["MENU/MENU4.PRG"] = 0x80102800
MODULES = ("MENU/MENU4.PRG", "BATTLE/BATTLE.PRG")
DRIVER = "status_limbs_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Status\LimbControllers.cpp",
    r"Native\Game\src\Menu\Status\ActorSwitching.cpp",
    r"Native\Game\src\Battle\HudState.cpp",
    r"Native\Engine\src\Input\Controller.cpp",
    r"Native\Engine\src\Render\EncodedText.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
)
SPECS = {
    2662: (0, 0x80103E58, 0x80103EF8),
    2663: (1, 0x80103EF8, 0x80103FEC),
    2664: (2, 0x80103FEC, 0x80104134),
    2665: (3, 0x80104134, 0x80104514),
    2668: (4, 0x801045B8, 0x80104AEC),
    2680: (5, 0x80106308, 0x801063F8),
    2681: (6, 0x801063F8, 0x801064D4),
}
INDICATOR, LEADER, ICONS, NAVIGATION, PANEL, CONDITION, DEFENSE = range(7)
MENU_BASE = 0x80102800
MENU_SHA1 = "6326f6a8a5b6b14dbfc9d05e5f6de788fc4e950b"
BATTLE_BASE = 0x80068800
NAV_ANIMATION, PANEL_PHASE, PANEL_ACTOR = 0x801080C4, 0x801080C5, 0x801080C6
ACTIVE, NAV_SELECTED, NAV_COUNT, NAV_TARGET = (
    0x80108138,
    0x80108158,
    0x80108159,
    0x8010815C,
)
PANEL_STEPS = 0x80108168
SELECTED_ACTOR, SELECTED_ELEMENT = 0x801080A8, 0x801081ED
YAW, PITCH, FADE, BACKGROUND = 0x8010809C, 0x801080A0, 0x801080B0, 0x801080AC
CURSOR_COLOR, HIGHLIGHT = 0x801022D5, 0x800F4F6A
PRESSED, HELD, REPEAT, STICKS = 0x8005E1D0, 0x8005E238, 0x8005DFDC, 0x8005DFC8
ACTOR_TABLE, ACTORS, STATS = 0x800F1928, 0x80500000, 0x80600000
ACTOR_STRIDE, STATS_STRIDE, LIMB_OFFSET, LIMB_STRIDE = 0x100, 0x1000, 0x398, 0xDC
PALETTE, TEXTURES, UPPER_ICONS, ROW_OFFSETS = (
    0x800EBC54,
    0x800EBC14,
    0x800EBC68,
    0x800EBBC8,
)
# The original palette's last five words alias the upper-icon mapping.
PALETTE_WORDS, UPPER_HALVES = 10, 8
EQUIPMENT, TEXT_CONTEXT, BUFFER_POINTER = 0x801024C0, 0x800F5168, 0x800F4E8C
TEXT_BUFFER, TEXT_BYTES = 0x80900000, 96
STATUS_TEXT, BATTLE_TEXT = 0x80107448, 0x800EA868
NAMES, CONDITIONS = 0x800EA620, 0x80108084
SCRATCH, PACKETS, SCENE, OVERLAY = 0x1F800000, 0x80700000, 0x80800100, 0x80800200
CHAIN_ADDRESSES = (OVERLAY, SCENE - 4, SCENE - 8)
PACKET_BYTES = 0x10000
FORMAT, FLAGS, LIMB_CONDITION, LIMB_COUNT = (
    0x800C685C,
    0x800C9E08,
    0x800C9E5C,
    0x80104514,
)
SPRITE, DEAD_ZONE = 0x800C0224, 0x800C4708
RAW_TEXT, COLOR_TEXT, PROJECT = 0x800C6828, 0x800C6540, 0x800A13EC
COMPARISON, SOUND, INFORMATION, CURSOR = 0x800FBD80, 0x800C02F8, 0x800FFC04, 0x800FFCDC
(
    NAME_EVENT,
    CONDITION_EVENT,
    PROJECT_EVENT,
    COMPARISON_EVENT,
    SOUND_EVENT,
    INFO_EVENT,
    CURSOR_EVENT,
) = range(7)
WORD_EDGES = (0, 1, -1, 31, 32, 63, 64, 128, 192, 255, 256, 0x7FFFFFFF, -0x80000000)
_verified = False


def verify_originals(machine: Machine) -> None:
    """Proof-only byte identity, exact boundaries and contiguous disassembly."""
    global _verified
    if _verified:
        return
    from capstone import CS_ARCH_MIPS, CS_MODE_LITTLE_ENDIAN, CS_MODE_MIPS32, Cs

    image = machine.images[0]
    if image.base != MENU_BASE or len(image.data) != 0x5A08:
        raise AssertionError("wrong MENU4 load image/extent")
    if hashlib.sha1(image.data).hexdigest() != MENU_SHA1:
        raise AssertionError("MENU4 does not match the read-only splat source")
    disassembler = Cs(CS_ARCH_MIPS, CS_MODE_MIPS32 | CS_MODE_LITTLE_ENDIAN)
    for identifier, (_, start, end) in SPECS.items():
        code = image.data[start - MENU_BASE : end - MENU_BASE]
        instructions = list(disassembler.disasm(code, start))
        if [item.address for item in instructions] != list(range(start, end, 4)):
            raise AssertionError(f"F{identifier:05}: incomplete original disassembly")
        if image.read(end - 8) != 0x03E00008:
            raise AssertionError(f"F{identifier:05}: boundary excludes JR/delay slot")
    _verified = True


def word(rng: random.Random) -> int:
    return rng.choice((*WORD_EDGES, s32(rng.getrandbits(32))))


def vector(rng: random.Random, number: int, mode: int) -> dict:
    selected = rng.randrange(1, 16)
    count = rng.randrange(1, 33)
    names = [rng.randrange(1, 20) for _ in range(6)]
    limbs = rng.randrange(7)
    if limbs < 6:
        names[limbs] = 0
    palette = [rng.getrandbits(32) for _ in range(PALETTE_WORDS)]
    # Native data has separate arrays; keep the original overlapping bytes
    # equal so the proof exercises that source alias rather than erasing it.
    upper = [part for item in palette[5:9] for part in (item & 0xFFFF, item >> 16)]
    value = {
        "mode": mode,
        "args": [word(rng) for _ in range(4)],
        "globals": [
            selected,
            rng.randrange(6),
            word(rng),
            word(rng),
            rng.choice((0, 0, word(rng))),
            rng.choice((0, 0, word(rng))),
            rng.randrange(256),
        ],
        "nav": [rng.randrange(256), rng.randrange(count), count, rng.randrange(-1, 15)],
        "active": rng.sample(range(32), count)
        + [rng.randrange(256) for _ in range(32 - count)],
        "panel": [rng.choice((0, 1, 1, 2, 2, 3, 3, 255)), rng.randrange(15)],
        "steps": [rng.choice((0, 0, 1, 7, 8, 12, 17, 35)) for _ in range(6)],
        "highlight": rng.choice((0, 1, 5, 6, 127, 128, 129, 133, 134, 255)),
        "input": [
            rng.choice((0, 0, 0x10, 0x40, 0x50)),
            rng.choice((0, 0, 0x80 | rng.randrange(16) << 12)),
            rng.randrange(16) << 12,
            word(rng),
            word(rng),
        ],
        "cursor": word(rng),
        "statuses": rng.getrandbits(32),
        "upper": upper,
        "hp": [
            rng.choice((-32768, -1, 0, 1, 2, 24, 25, 74, 75, 99, 100, 32767))
            for _ in range(6)
        ],
        "max_hp": [rng.choice((-32768, -1, 0, 1, 2, 100, 32767)) for _ in range(6)],
        "names": names,
        "bones": [rng.randrange(256) for _ in range(6)],
        "projections": [s32(rng.getrandbits(32)) for _ in range(256)],
        "palette": palette,
        "textures": [rng.randrange(0x10000) for _ in range(32)],
        "rows": [rng.randrange(256) for _ in range(36)],
        "equipment": [rng.randrange(0x10000) for _ in range(64)],
        "affinities": [rng.randrange(0x10000) for _ in range(6 * 8)],
        "types": [rng.randrange(0x10000) for _ in range(6 * 4)],
        "output": [rng.randrange(256) for _ in range(TEXT_BYTES)],
        "context": [rng.randrange(1, 20), rng.randrange(5)],
        "alias": 0,
        "heads": [rng.getrandbits(32) for _ in CHAIN_ADDRESSES],
    }
    if number % 8 == 0:
        value["statuses"] = 0
    if mode == INDICATOR:
        value["args"][0] = number % PALETTE_WORDS
    elif mode == LEADER:
        value["args"][0] = number % 6
        value["args"][3] = rng.choice((0, 1, 31, 32, word(rng)))
    elif mode == ICONS:
        value["args"][0] = selected - 1
    elif mode == NAVIGATION:
        value["args"][0] = int(number % 3 != 0)
        value["args"][1] = number % 2
        value["globals"][1] = rng.choice((0, 6, 11, 12, 255))
        if number % 3 == 0:
            value["input"][0] = 0 if number % 4 else value["input"][0]
            value["input"][1] = 0 if number % 5 else value["input"][1]
        if number % 11 == 0:
            value["input"][3:5] = [128, 128]
        if number % 16 < 2:
            # Centered sticks must take the digital path; exercise both a
            # visible down step and an upward step beyond the pitch clamp.
            value["args"][0] = 0
            value["input"] = [0, 0x4080 if number % 16 == 0 else 0x1080, 0, 128, 128]
            value["globals"][3] = 448 if number % 16 == 0 else 896
        elif number % 16 < 4:
            value["args"][0] = 0
            value["input"][0] = 0x40 if number % 16 == 2 else 0x50
            value["input"][1] = 0x80  # Both pressed exits precede model rotation.
    elif mode == PANEL:
        value["args"][0] = rng.choice(
            (0, 0, 0, 0, -1, -2, -3, -9, 1, 129, 0x182, 0x7FFFFFFF)
        )
        if value["panel"][0] == 1 and value["args"][0] == 0 and not names[0]:
            names[0] = 1  # Undefined original empty-entering input is a Catch2 refusal.
        if value["panel"][0] == 3:
            value["steps"] = [
                rng.choice((0, 1, 7, 8, 9, -1, -0x80000000)) for _ in range(6)
            ]
        if value["args"][0] < 0 and value["args"][0] not in (-2, -3):
            value["globals"][0] = selected | (rng.getrandbits(24) << 8)
        if number % 16 == 4:
            value["args"][0] = 0
            value["panel"][0] = 2
            value["highlight"] = 0
            value["globals"][4:6] = [1, 0]
            names[0] = names[0] or 1
        elif number % 16 in (5, 6):
            value["args"][0] = 0
            names[:] = [name or 1 for name in names]
            if number % 16 == 5:
                value["panel"][0] = 1
                value["steps"] = [7, 3, 2, 1, 1, 0]
                value["rows"][7] = 255
                value["bones"][0] = 128
            else:
                value["panel"][0] = 3
                value["steps"] = [7, 8, 1, 0, -1, 0]
    elif mode == DEFENSE and number % 3 == 0:
        value["alias"] = rng.choice((4, 12, 15, 16, 20, 28, 32, 40, 56))
        value["panel"][1] = (
            selected % 15
        )  # Selected alias never used for the panel's count.
    return value


def text(machine: Machine, address: int, *, encoded: bool) -> list[int]:
    result = []
    for _ in range(0x2000):
        byte = machine.load(address, 1)
        address += 1
        result.append(byte)
        if byte == (0xE7 if encoded else 0):
            return result
        if encoded and byte >= 0xEC:
            result.append(machine.load(address, 1))
            address += 1
    raise Fault("unterminated external text")


def stats_pointer(slot: int) -> int:
    return STATS + slot * STATS_STRIDE


def place(machine: Machine, value: dict) -> None:
    for address, size in (
        (ACTOR_TABLE, 42 * 4),
        (ACTORS, 15 * ACTOR_STRIDE),
        (STATS, 15 * STATS_STRIDE),
        (SCRATCH, 0x10),
        (PACKETS, PACKET_BYTES),
        (SCENE - 8, 0x108),
        (TEXT_BUFFER, TEXT_BYTES),
        (EQUIPMENT, 128),
        (CURSOR_COLOR, 1),
        (PRESSED, 4),
        (HELD, 4),
        (REPEAT, 4),
        (STICKS, 8),
    ):
        machine.map_zero(address, size)
    for address, field in zip(
        (SELECTED_ACTOR, SELECTED_ELEMENT, YAW, PITCH, FADE, BACKGROUND, CURSOR_COLOR),
        value["globals"],
        strict=True,
    ):
        machine.put(
            address, field, 1 if address in (SELECTED_ELEMENT, CURSOR_COLOR) else 4
        )
    for address, field in zip(
        (NAV_ANIMATION, NAV_SELECTED, NAV_COUNT), value["nav"][:3], strict=True
    ):
        machine.put(address, field, 1)
    machine.put(
        NAV_TARGET, 0 if value["nav"][3] < 0 else stats_pointer(value["nav"][3])
    )
    machine.store(ACTIVE, bytes(value["active"]))
    machine.put(PANEL_PHASE, value["panel"][0], 1)
    machine.put(PANEL_ACTOR, value["panel"][1], 1)
    machine.put(HIGHLIGHT, value["highlight"], 1)
    for index, step in enumerate(value["steps"]):
        machine.put(PANEL_STEPS + index * 4, step)
    for address, field in zip(
        (PRESSED, HELD, REPEAT, STICKS, STICKS + 4), value["input"], strict=True
    ):
        machine.put(address, field)
    machine.put(SCRATCH, PACKETS)
    machine.put(SCRATCH + 4, SCENE)
    machine.put(SCRATCH + 8, OVERLAY)
    for address, head in zip(CHAIN_ADDRESSES, value["heads"], strict=True):
        machine.put(address, head)
    for key, address, stride in (
        ("palette", PALETTE, 4),
        ("textures", TEXTURES, 2),
        ("upper", UPPER_ICONS, 2),
        ("rows", ROW_OFFSETS, 1),
        ("equipment", EQUIPMENT, 2),
    ):
        for index, field in enumerate(value[key]):
            machine.put(address + stride * index, field, stride)
    machine.put(BUFFER_POINTER, TEXT_BUFFER)
    machine.store(TEXT_BUFFER, bytes(value["output"]))
    machine.put(
        TEXT_CONTEXT + 40,
        BATTLE_TEXT + 2 * machine.load(BATTLE_TEXT + 2 * value["context"][0], 2),
    )
    machine.put(
        TEXT_CONTEXT + 44,
        STATUS_TEXT + 2 * machine.load(STATUS_TEXT + 2 * (13 + value["context"][1]), 2),
    )
    for slot in range(15):
        actor = ACTORS + slot * ACTOR_STRIDE
        stats = stats_pointer(slot)
        machine.put(ACTOR_TABLE + slot * 4, actor)
        machine.put(actor + 0x3C, stats)
        bits = value["statuses"]
        machine.put(stats + 0x948, ((bits << slot) | (bits >> (32 - slot))) & MASK)
        for limb in range(6):
            start = stats + LIMB_OFFSET + limb * LIMB_STRIDE
            machine.put(start, value["hp"][limb] + slot, 2)
            machine.put(start + 2, value["max_hp"][limb] + slot * 2, 2)
            machine.put(start + 5, value["bones"][limb] + slot, 1)
            name = value["names"][limb]
            machine.put(start + 6, (name + slot - 1) % 19 + 1 if name else 0, 1)
            for index in range(8):
                machine.put(
                    start + 0x10 + index * 2,
                    value["affinities"][limb * 8 + index] + slot * 257,
                    2,
                )
            for index in range(4):
                machine.put(
                    start + 8 + index * 2,
                    value["types"][limb * 4 + index] + slot * 257,
                    2,
                )
    if value["alias"]:
        slot = value["globals"][0] - 1
        limb = value["globals"][1]
        alias_stats = (
            EQUIPMENT + 2 * value["alias"] - (LIMB_OFFSET + limb * LIMB_STRIDE + 0x10)
        )
        machine.put(ACTORS + slot * ACTOR_STRIDE + 0x3C, alias_stats)


def services(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    def label(cpu: Machine, colored: bool) -> None:
        address = cpu.r[R["a0"]]
        kind, base = (
            (CONDITION_EVENT, MENU_BASE)
            if MENU_BASE <= address < MENU_BASE + 0x5A08
            else (NAME_EVENT, BATTLE_BASE)
        )
        content = text(cpu, address, encoded=False)
        calls.append(
            [
                kind,
                address - base,
                cpu.r[R["a1"]],
                cpu.r[R["a2"]] if colored else -1,
                len(content),
                *content,
            ]
        )

    def project(cpu: Machine) -> None:
        bone = cpu.r[R["a1"]]
        if bone >= len(value["projections"]):
            raise Fault("invalid projected bone")
        point = value["projections"][bone] & MASK
        calls.append([PROJECT_EVENT, cpu.r[R["a0"]], bone, point])
        cpu.write(cpu.r[R["a2"]], point, 4)
        cpu.write(cpu.r[R["a2"]] + 4, 0x1234, 4)
        cpu.r[R["v0"]] = 0

    def information(cpu: Machine) -> None:
        address = cpu.r[R["a0"]]
        bank, base = (
            (1, TEXT_BUFFER)
            if TEXT_BUFFER <= address < TEXT_BUFFER + TEXT_BYTES
            else (0, STATUS_TEXT)
        )
        content = text(cpu, address, encoded=True)
        calls.append([INFO_EVENT, bank, address - base, len(content), *content])

    def cursor(cpu: Machine) -> None:
        calls.append([CURSOR_EVENT, cpu.r[R["a0"]], cpu.r[R["a1"]]])
        cpu.r[R["v0"]] = value["cursor"] & MASK

    machine.stub(RAW_TEXT, lambda cpu: label(cpu, False))
    machine.stub(COLOR_TEXT, lambda cpu: label(cpu, True))
    machine.stub(PROJECT, project)
    machine.stub(
        COMPARISON, lambda cpu: calls.append([COMPARISON_EVENT, cpu.r[R["a0"]]])
    )
    machine.stub(SOUND, lambda _: calls.append([SOUND_EVENT]))
    machine.stub(INFORMATION, information)
    machine.stub(CURSOR, cursor)
    machine.follow.update(
        (
            FLAGS,
            LIMB_CONDITION,
            LIMB_COUNT,
            SPRITE,
            DEAD_ZONE,
            FORMAT,
            *(entry for _, entry, _ in SPECS.values()),
        )
    )


def flatten_input(value: dict) -> list[int]:
    return [
        value["mode"],
        *value["args"],
        *value["globals"],
        *value["nav"],
        *value["active"],
        *value["panel"],
        *value["steps"],
        value["highlight"],
        *value["input"],
        value["cursor"],
        value["statuses"],
        *value["upper"],
        *value["hp"],
        *value["max_hp"],
        *value["names"],
        *value["bones"],
        *value["projections"],
        *value["palette"],
        *value["textures"],
        *value["rows"],
        *value["equipment"],
        *value["affinities"],
        *value["types"],
        *value["output"],
        *value["context"],
        value["alias"],
        *value["heads"],
    ]


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    verify_originals(machine)
    place(machine, value)
    calls: list[list[int]] = []
    services(machine, value, calls)
    mode = value["mode"]
    for index, argument in enumerate(value["args"]):
        machine.r[R["a0"] + index] = int(argument) & MASK
    if mode in (ICONS, NAVIGATION):
        machine.r[R["a0"]] = (
            stats_pointer(value["globals"][0] - 1)
            if mode == ICONS or value["args"][0]
            else 0
        )
    machine.stores.clear()
    machine.execute(entry)
    target = machine.load(NAV_TARGET)
    result = s32(machine.r[R["v0"]]) if mode in (NAVIGATION, PANEL) else 0
    observed = [
        result,
        machine.load(NAV_ANIMATION, 1),
        machine.load(NAV_SELECTED, 1),
        machine.load(NAV_COUNT, 1),
        (target - STATS) // STATS_STRIDE if target else -1,
        *machine.bytes(ACTIVE, 32),
        machine.load(PANEL_PHASE, 1),
        machine.load(PANEL_ACTOR, 1),
        *(s32(machine.load(PANEL_STEPS + index * 4)) for index in range(6)),
        machine.load(SELECTED_ELEMENT, 1),
        s32(machine.load(YAW)),
        s32(machine.load(PITCH)),
        machine.load(CURSOR_COLOR, 1),
        machine.load(HIGHLIGHT, 1),
        s32(machine.load(SELECTED_ACTOR)),
        s32(machine.load(FADE)),
        s32(machine.load(BACKGROUND)),
        *(machine.load(EQUIPMENT + index * 2, 2) for index in range(64)),
        machine.load(TEXT_CONTEXT + 40) - BATTLE_TEXT,
        machine.load(TEXT_CONTEXT + 44) - STATUS_TEXT,
        *machine.bytes(TEXT_BUFFER, TEXT_BYTES),
    ]
    packet_count = (machine.load(SCRATCH) - PACKETS) // 4
    if not 0 <= packet_count <= PACKET_BYTES // 4:
        raise Fault("invalid packet cursor")
    observed.extend(
        [
            packet_count,
            *(machine.load(address) for address in CHAIN_ADDRESSES),
            *(machine.load(PACKETS + index * 4) for index in range(packet_count)),
        ]
    )
    return recorded_observation(
        machine, observed, [f"word {i}" for i in range(len(observed))], calls
    ), flatten_input(value)


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    mode = value["mode"]
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if mode in (INDICATOR, LEADER, ICONS, PANEL):
        allowed += [(PACKETS, PACKETS + PACKET_BYTES), (SCRATCH, SCRATCH + 4)]
        allowed += [(address, address + 4) for address in CHAIN_ADDRESSES]
    if mode == NAVIGATION:
        allowed += [
            (NAV_ANIMATION, NAV_ANIMATION + 1),
            (ACTIVE, ACTIVE + 34),
            (NAV_TARGET, NAV_TARGET + 4),
            (SELECTED_ELEMENT, SELECTED_ELEMENT + 1),
            (YAW, YAW + 8),
            (CURSOR_COLOR, CURSOR_COLOR + 1),
        ]
    if mode == PANEL:
        allowed += [(PANEL_PHASE, PANEL_ACTOR + 1), (PANEL_STEPS, PANEL_STEPS + 24)]
    if mode in (PANEL, CONDITION, DEFENSE):
        allowed += [(HIGHLIGHT, HIGHLIGHT + 1)]
    if mode == CONDITION:
        allowed += [
            (TEXT_CONTEXT + 40, TEXT_CONTEXT + 48),
            (TEXT_BUFFER, TEXT_BUFFER + TEXT_BYTES),
        ]
    if mode == DEFENSE:
        allowed += [(EQUIPMENT + 32, EQUIPMENT + 72)]
    return allowed


def describe(image, results) -> list[str]:
    return [
        f"MENU4 sha1={hashlib.sha1(image.data).hexdigest()}; exact symbol boundaries/disassembly checked",
        "Nested originals executed: flags, limb condition/count, sprite builder, dead-zone, formatter, "
        "and all in-packet callees; external text/cursor/audio/projection/comparison services doubled.",
        "Compared complete packet words, all three chain heads/cursor, text bank offsets/contents, "
        "context pointers, output bytes, all defense halfwords and controller state; stores bounded.",
        f"Defense source/destination alias vectors={sum(bool(v['alias']) for v, _, _ in results)}",
        "Undefined empty-entering original is excluded; Catch2 covers refusal (#419). "
        "No interactive gameplay or compatibility retirement claim.",
    ]


def configure(identifier: int, mutants: dict) -> dict:
    mode, entry, end = SPECS[identifier]
    result = mode_case(
        identifier, mode, entry, end, mutants, harness=sys.modules[__name__]
    )
    result.update(VECTORS=1024, MUTANT_VECTORS=128)
    return result
