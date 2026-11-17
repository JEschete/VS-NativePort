"""Six terminal controllers: original instructions, complete packets and endpoint protocols.

The original six controllers, limb/equipment counts, resident trig and dead-zone
mapping execute. Already-delivered component/transition/limb, text formatting,
menu creation, audio, asynchronous loads and rendering endpoints are protocol
doubles here; actual production Live adapters have separate lifecycle tests.
Importing CASE metadata reads no images and imports no optional disassembler.
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

MODULE_BASES[r"MENU\MENU4.PRG"] = 0x80102800
MODULE_BASES[r"BATTLE\BATTLE.PRG"] = 0x80068800
MODULES = (r"MENU\MENU4.PRG", r"BATTLE\BATTLE.PRG", "SLUS_010.40")
DRIVER = "status_terminal_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Status\TerminalEquipment.cpp",
    r"Native\Game\src\Menu\Status\TerminalRendering.cpp",
    r"Native\Game\src\Menu\Status\TerminalController.cpp",
    r"Native\Game\src\Menu\Status\ActorSwitching.cpp",
    r"Native\Engine\src\Input\Controller.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
INCLUDES = (r"Native\Tests\include",)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
COMPILER_OPTIONS = ("/W4", "/WX", "/Gy", "/Gw", "/MD", "/DNOMINMAX")
SPECS = {
    2675: (0, 0x80104F80, 0x80105970),
    2676: (1, 0x80105970, 0x80105E94),
    2677: (2, 0x80105E94, 0x80105F60),
    2678: (3, 0x80105F60, 0x80106150),
    2679: (4, 0x80106150, 0x80106308),
    2682: (5, 0x801064D4, 0x80107448),
}
SPAN_HASHES = (
    "806e5d1c6da1a877bed4fcb2ada2548e6e23de88ab01a37b5db3713ee3a85a6d",
    "19b97fa708d6916698632b915003ac50162df9702014305eca156a1d742ff940",
    "f7414809ea23a3a2fe52051524c8af061aa9f818ca72a21378eac17beb1a7e25",
    "b1744ecd69e12dd1457cc60e10184e615b9e34fb398523ee65a9b02b63755fe5",
    "e14d97e3d398a483ecbcb2a9fe636ecbcd6f9eedd9f75ddb10d18fe363a2b866",
    "f4d43176f087f3987184b285915c847402e905f124e1dfbf7e604e97ede8a90d",
)
MENU_SHA1 = "6326f6a8a5b6b14dbfc9d05e5f6de788fc4e950b"
MENU_BASE, STATUS_TEXT = 0x80102800, 0x80107448
SCRATCH, PACKETS, SCENE = 0x1F800000, 0x80700000, 0x80800100
HEADS = (SCENE - 4, SCENE + 0x1FF4, SCENE - 8)
ACTOR_TABLE, ACTORS, STATS = 0x800F1928, 0x80500000, 0x80600000
MODEL, STATE, ROWS, BUFFER = 0x80510000, 0x80410000, 0x80420000, 0x80430000
HELP, NAMES, COMPONENTS = 0x80900000, 0x80910000, 0x80911000
FRAME = (
    (0x801080A8, 4),
    (0x801080A4, 4),
    (0x801080B4, 4),
    (0x8010809C, 4),
    (0x801080A0, 4),
    (0x801080AC, 4),
    (0x801080B0, 4),
    (0x801080B8, 1),
    (0x801080B9, 1),
    (0x801080BB, 1),
    (0x801080BC, 1),
    (0x801081ED, 1),
    (0x801081EE, 1),
    (0x801022D5, 1),
    (0x801023E3, 1),
    (0x801024B9, 1),
    (0x80102544, 1),
    (0x800F4F6C, 1),
    (0x80060021, 1),
    (0x800EB9AE, 1),
    (0x800F51C0, 1),
    (0x8005E1D0, 4),
    (0x8005E238, 4),
    (0x8005DFDC, 4),
    (0x8005DFC8, 4),
    (0x8005DFCC, 4),
)
PRIVATE = (
    (0x801080C7, 1),
    (0x80108180, 1),
    (0x80108181, 1),
    (0x80108182, 1),
    (0x80108183, 1),
    (0x80108184, 1),
    (0x80108185, 1),
    (0x8010812C, 4),
    (0x80108130, 4),
    (0x80108134, 4),
    (0x80108188, 4),
    (0x801081EC, 1),
)
ROW_OFFSETS, ANIMATION, CURSORS = 0x800EBBC8, 0x800EBC7C, 0x801021A0
PROJECTION, FRAMEBUFFER = 0x8005E248, 0x8005E210
WORD_EDGES = (0, 1, -1, 127, 128, 255, 256, 4095, 4096, 65535, 0x7FFFFFFF, -0x80000000)
_verified = False


def verify_originals(machine: Machine) -> None:
    global _verified
    if _verified:
        return
    image = machine.images[0]
    if image.base != MENU_BASE or len(image.data) != 0x5A08:
        raise AssertionError("incorrect MENU4 load-image extent")
    if hashlib.sha1(image.data).hexdigest() != MENU_SHA1:
        raise AssertionError("incorrect read-only MENU4 image")
    for (_, start, end), expected in zip(SPECS.values(), SPAN_HASHES, strict=True):
        body = image.data[start - MENU_BASE : end - MENU_BASE]
        if (
            hashlib.sha256(body).hexdigest() != expected
            or image.read(end - 8) != 0x03E00008
        ):
            raise AssertionError(f"boundary/fingerprint mismatch at {start:#x}")
    _verified = True


def text(seed: int, index: int) -> bytes:
    return bytes(((seed + index) % 220 + 1, index // 220 + 1, 0xE7, 0xE7))


def actors(rng: random.Random) -> list[list[int]]:
    result = []
    for _ in range(15):
        count = rng.randrange(7)
        names = [rng.randrange(1, 20) if i < count else 0 for i in range(6)]
        result.append(
            [
                *names,
                *(rng.choice((0, 1, 128, 255)) for _ in range(8)),
                rng.choice((0, 1, 255)),
                rng.randrange(4),
                rng.randrange(4),
                rng.randrange(2),
                rng.choice((0, 1, 0x80000000, MASK)),
            ]
        )
    return result


def vectors(rng: random.Random, count: int, mode: int, start: int = 0):
    for index in range(start, start + count):
        actor_data = actors(rng)
        selected = rng.randrange(1, 16)
        actor = actor_data[selected - 1]
        limbs = next((i for i, name in enumerate(actor[:6]) if not name), 6)
        rows = limbs + 2 + bool(actor[14])
        request = 0
        menu_state = (index % 12) + 3
        frame = [
            selected,
            rng.randrange(16),
            rng.choice(WORD_EDGES),
            rng.choice(WORD_EDGES),
            rng.choice(WORD_EDGES),
            rng.randrange(2),
            rng.randrange(2),
            rng.randrange(256),
            rng.randrange(256),
            rng.randrange(16),
            rng.choice((0, 1, 127, 255)),
            rng.randrange(max(limbs, 1)),
            rng.randrange(256),
            rng.randrange(256),
            rng.randrange(256),
            rng.randrange(3),
            rng.randrange(256),
            rng.randrange(256),
            rng.randrange(256),
            rng.randrange(2),
            rng.randrange(256),
            rng.choice((0, 4, 8, 12, 16, 32, 64, 80, 96, 128)),
            rng.choice((0, 1, 4, 8, 12, 0x3001, 0x6001, 0xC001)),
            rng.choice((0, 4, 8, 12, 0x1000, 0x4000, 0x8000, 0x2000, 0xF000)),
            rng.choice((0, 1, 95, 96, 127, 128, 159, 160, 255, -1)),
            rng.choice((0, 127, 128, 159, 255, 0x7FFFFFFF)),
        ]
        detail = [
            rng.randrange(256),
            index % 4,
            rng.randrange(rows),
            rng.randrange(11),
            rng.randrange(21),
        ]
        listing = [index % 7, rng.randrange(11)]
        screen = [
            0,
            rng.randrange(2),
            rng.choice(WORD_EDGES),
            rng.choice(WORD_EDGES),
            rng.randrange(11),
        ]
        options = [
            rng.randrange(2),
            selected,
            rng.randrange(2),
            -1,
            rng.randrange(21),
            rng.choice((0, 1, 2, 3)),
            rng.randrange(2),
            rng.randrange(2),
            rng.randrange(2),
            rng.choice(WORD_EDGES),
            rng.randrange(2),
            rng.randrange(2),
            rng.randrange(2),
            rng.choice(WORD_EDGES),
            rng.choice((0, 1, 32767, 32768, 65535)),
            rng.choice((0, 1, 32768, 65535)),
        ]
        if mode == 0:
            if index % 16 == 0:
                request = rng.choice((1, 257, -255))
            detail[1] = (index // 4) % 4
        if mode in (1, 5):
            options[3] = rng.choice((-3, -2, -1, -1, -1, 0, rows - 1))
            if mode == 1 and index % 16 == 0:
                request = rng.choice((-1, 1, 255, 256, 0x7FFFFFFF))
            if mode == 1 and listing[0] == 5:
                detail[1] = 2
                frame[22], frame[23] = 12, 0
                frame[21] = 16 if (index // 7) % 2 else 64
        if mode == 5:
            options[6] = rng.randrange(4)
            if menu_state in (4, 5, 6, 7):
                screen[0] = rng.randrange(2)
            if menu_state in (9, 10):
                screen[0] = rng.choice((0, 1, 10))
            if menu_state in (12, 14):
                screen[0] = rng.choice((-1, 0, 1, 8))
            if menu_state in (5, 6, 9):
                # The delivered limb refusal covers zero-limb entering. Keep
                # terminal differential vectors inside the original valid UI.
                if limbs == 0:
                    actor[0] = 1
                    limbs = 1
                frame[11] = (
                    rng.choice((*range(limbs), 6, 7, 8, 9, 10, 11))
                    if menu_state == 5
                    else rng.randrange(limbs)
                )
                frame[4] = rng.randrange(897)
            if menu_state == 5 and index % 24 == 2:
                screen[0], frame[11], frame[21], frame[22], frame[23] = 0, 0, 0, 0, 0
                options[5], options[7] = 2, 1
            if menu_state == 6 and index % 24 == 3:
                screen[0], options[5], options[6] = 0, 2, 2
            if menu_state == 8:
                listing[0] = index // 12 % 6
            options[0] = 0
        yield {
            "mode": mode,
            "seed": index + 1,
            "request": request,
            "menu": menu_state,
            "frame": frame,
            "private": detail + listing + screen,
            "view": [rng.getrandbits(32) for _ in range(21)],
            "saved": [rng.getrandbits(32) for _ in range(21)],
            "options": options,
            "actors": actor_data,
            "flags": [rng.getrandbits(32) for _ in range(9)],
            "heads": [rng.getrandbits(24) for _ in range(3)],
            "offsets": [rng.randrange(256) for _ in range(36)],
            "animation": [rng.randrange(11) for _ in range(244)],
            "cursors": [rng.getrandbits(32) for _ in range(21)],
        }


def vector(rng: random.Random, number: int, mode: int) -> dict:
    return next(vectors(rng, 1, mode, number))


def stats_pointer(slot: int) -> int:
    return STATS + slot * 0x1000


def place(machine: Machine, value: dict) -> None:
    machine.map_zero(SCRATCH, 0x1000)
    machine.map_zero(ROWS, 40 * 64)
    machine.map_zero(MODEL, 0x700)
    machine.map_zero(PACKETS, 0x10000)
    machine.put(SCRATCH, PACKETS)
    machine.put(SCRATCH + 4, SCENE)
    machine.put(STATE, value["menu"], 1)
    for layout, fields in ((FRAME, value["frame"]), (PRIVATE, value["private"])):
        for (address, size), field in zip(layout, fields, strict=True):
            machine.put(address, field, size)
    for start, key in ((SCRATCH + 0x14, "view"), (0x80108198, "saved")):
        for index, word in enumerate(value[key]):
            machine.put(start + index * 4, word)
    for address, word in zip(HEADS, value["heads"], strict=True):
        machine.put(address, word)
    for key, base, size in (
        ("offsets", ROW_OFFSETS, 1),
        ("animation", ANIMATION, 1),
        ("cursors", CURSORS, 4),
    ):
        for index, word in enumerate(value[key]):
            machine.put(base + index * size, word, size)
    machine.put(0x800F453C, MODEL)
    machine.put(MODEL + 0x656, value["options"][14], 2)
    machine.put(MODEL + 0x63E, value["options"][15], 2)
    machine.put(PROJECTION, value["options"][13])
    machine.put(FRAMEBUFFER, value["options"][12])
    machine.put(0x8010229C, NAMES if value["options"][11] else 0)
    machine.put(0x80102540, HELP)
    machine.put(0x800F4E8C, BUFFER)
    machine.store(BUFFER, bytes([0xAA]) * 96)
    for row in range(40):
        machine.store(
            ROWS + row * 64,
            bytes((value["seed"] + row * 13 + byte * 7) & 255 for byte in range(60)),
        )
    for index in range(61):
        machine.put(STATUS_TEXT + index * 2, (512 + index * 4) // 2, 2)
        machine.store(STATUS_TEXT + 512 + index * 4, text(value["seed"], index))
    for index in (230, 233):
        machine.store(STATUS_TEXT + index * 2, text(value["seed"], index))
    for index in range(700):
        machine.put(HELP + index * 2, (2048 + index * 4) // 2, 2)
        machine.store(HELP + 2048 + index * 4, text(value["seed"] + 11, index))
    for index in range(9):
        machine.store(NAMES + index * 4, text(value["seed"] + 21, index))
    for index in range(63):
        machine.store(COMPONENTS + index * 4, text(value["seed"] + 51, index))
    for slot, actor in enumerate(value["actors"]):
        stats = stats_pointer(slot)
        machine.map_zero(stats, 0x1000)
        machine.put(ACTOR_TABLE + slot * 4, ACTORS + slot * 0x100)
        machine.put(ACTORS + slot * 0x100 + 0x3C, stats)
        machine.store(stats + 0x3C, text(value["seed"] + 31, slot))
        machine.put(stats + 0x54, actor[6], 1)
        machine.put(stats + 0x1EC, actor[7], 1)
        machine.put(stats + 0x338, actor[14], 1)
        machine.put(stats + 0x99, actor[15], 1)
        machine.put(stats + 0x201, actor[16], 1)
        machine.put(stats + 0x954, actor[17] << 17)
        machine.put(stats + 0x948, actor[18])
        for limb in range(6):
            base = stats + 0x398 + limb * 0xDC
            machine.put(base + 6, actor[limb], 1)
            machine.put(base + 0x20, actor[8 + limb], 1)


def encoded(cpu: Machine, address: int) -> list[int]:
    result = []
    for index in range(96):
        byte = cpu.byte(address + index)
        result.append(byte)
        if byte == 0xE7:
            return [len(result), *result]
    raise Fault("unterminated endpoint text")


def services(machine: Machine, value: dict, calls: list[list[int]]) -> None:
    def event(kind: int, *args: int) -> None:
        calls.append([kind, *(arg & MASK for arg in args)])

    def a(cpu: Machine, index: int = 0) -> int:
        return cpu.r[R["a0"] + index]

    def selected(cpu: Machine) -> int:
        return cpu.load(FRAME[0][0]) - 1

    def record(kind: int, arguments: int = 0, result: int | None = None):
        def call(cpu: Machine) -> None:
            event(kind, *(a(cpu, index) for index in range(arguments)))
            if result is not None:
                cpu.r[R["v0"]] = result & MASK

        return call

    def row_pointer(cpu: Machine) -> None:
        row = s32(a(cpu))
        if not 0 <= row < 40:
            raise Fault("row borrow outside BATTLE owner")
        cpu.r[R["v0"]] = ROWS + row * 64

    def create_row(cpu: Machine) -> None:
        event(9, *(a(cpu, i) for i in range(4)), cpu.load(cpu.r[R["sp"]] + 16))
        calls[-1].extend(encoded(cpu, cpu.load(cpu.r[R["sp"]] + 20)))
        row_pointer(cpu)

    def format_row(cpu: Machine, row: int) -> None:
        event(8, row)
        cpu.store(a(cpu, 3), text(value["seed"] + 41, row)[:3])
        cpu.put(a(cpu, 1), NAMES + row * 4)
        cpu.put(a(cpu, 1) + 4, a(cpu, 3))
        cpu.put(a(cpu, 2), value["flags"][row])

    def armor_row(cpu: Machine) -> int:
        offset = a(cpu) - stats_pointer(selected(cpu)) - 0x3B8
        if offset < 0 or offset % 0xDC or offset // 0xDC >= 6:
            raise Fault("armor pointer outside selected actor")
        return 2 + offset // 0xDC

    def accessory_row(cpu: Machine) -> int:
        names = value["actors"][selected(cpu)][:6]
        return 2 + next((i for i, name in enumerate(names) if not name), 6)

    def create_list(cpu: Machine) -> None:
        event(10, a(cpu), a(cpu, 1), cpu.load(FRAME[18][0], 1))
        for row in range(a(cpu)):
            calls[-1].append(cpu.load(a(cpu, 3) + row * 4))
            calls[-1].extend(encoded(cpu, cpu.load(a(cpu, 2) + row * 8)))
            calls[-1].extend(encoded(cpu, cpu.load(a(cpu, 2) + row * 8 + 4)))

    def describe(cpu: Machine, row: int, component: int) -> None:
        event(12, row, component)
        cpu.r[R["v0"]] = COMPONENTS + (row * 7 + component) * 4

    def next_actor(cpu: Machine) -> None:
        event(21, a(cpu), a(cpu, 1))
        cpu.r[R["v0"]] = (a(cpu) + a(cpu, 1) + (value["seed"] & 1)) % 15

    def switch(cpu: Machine) -> None:
        event(22, a(cpu))
        if a(cpu):
            cpu.put(FRAME[0][0], a(cpu) & 15)
        elif value["options"][0]:
            cpu.put(FRAME[0][0], value["options"][1])
        cpu.r[R["v0"]] = value["options"][2]

    def camera(cpu: Machine) -> None:
        event(43, *(cpu.load(SCRATCH + 0x34 + index * 4) for index in range(13)))

    def information(cpu: Machine) -> None:
        event(28)
        calls[-1].extend(encoded(cpu, a(cpu)))

    def effects(cpu: Machine) -> None:
        pointer = a(cpu)
        event(39, (pointer - STATS) // 0x1000 if pointer else -1, a(cpu, 1))
        cpu.r[R["v0"]] = value["options"][6]

    def projection(cpu: Machine) -> None:
        event(33, a(cpu))
        cpu.put(PROJECTION, a(cpu))

    simple = {
        0x800CB654: (1, 1, None),
        0x80104AEC: (2, 1, None),
        0x80104C0C: (3, 2, None),
        0x800FC268: (4, 1, None),
        0x800FFB90: (7, 1, None),
        0x801013F8: (13, 1, None),
        0x80104F2C: (14, 1, None),
        0x800FBD80: (15, 1, None),
        0x801008C8: (16, 0, value["options"][3]),
        0x80100814: (17, 0, None),
        0x800FA9D0: (18, 0, value["options"][10]),
        0x800FFCDC: (20, 2, value["options"][9]),
        0x80103608: (23, 1, None),
        0x800FBEA4: (24, 1, None),
        0x800FBBD4: (25, 1, None),
        0x800FFA88: (26, 1, None),
        0x800FA8E0: (27, 1, None),
        0x800FE5CC: (29, 1, value["options"][7]),
        0x800FE694: (30, 0, value["options"][8]),
        0x8008A4DC: (31, 1, None),
        0x80100414: (32, 2, None),
        0x800FFBC8: (34, 0, None),
        0x800FFBA8: (35, 0, None),
        0x80106308: (36, 0, None),
        0x801063F8: (37, 0, None),
        0x801045B8: (38, 1, value["options"][5]),
        0x80103AC8: (40, 0, None),
        0x800F9E0C: (41, 0, None),
        0x800FFB68: (42, 1, None),
        0x800F9EB8: (44, 0, None),
        0x80100344: (45, 4, None),
        0x800FFC68: (46, 4, None),
    }
    for address, args in simple.items():
        machine.stub(address, record(*args))
    for address, sound in (
        (0x800C02F8, 0),
        (0x800C02E8, 1),
        (0x800C02F0, 2),
        (0x800C02E0, 3),
    ):
        machine.stub(address, lambda _, sound=sound: event(19, sound))
    machine.stub(0x800C8E48, row_pointer)
    machine.stub(0x800C8E5C, create_row)
    machine.stub(0x800FC85C, lambda cpu: format_row(cpu, 0))
    machine.stub(0x800FCCE8, lambda cpu: format_row(cpu, 1))
    machine.stub(0x800FCECC, lambda cpu: format_row(cpu, armor_row(cpu)))
    machine.stub(0x800FD084, lambda cpu: format_row(cpu, accessory_row(cpu)))
    machine.stub(0x8006BADC)
    for address, equipment in ((0x80102920, 0), (0x80102A64, 1)):
        machine.stub(address, lambda _, row=equipment: event(5, row))
    machine.stub(0x80102B70, lambda cpu: event(5, armor_row(cpu)))
    machine.stub(0x80102CAC, lambda cpu: event(5, accessory_row(cpu)))
    for address, equipment in ((0x80104C40, 0), (0x80104DFC, 1)):
        machine.stub(
            address,
            lambda cpu, equipment=equipment: event(6, equipment, a(cpu), a(cpu, 2)),
        )
    machine.stub(0x801005E0, create_list)
    machine.stub(0x801034BC, record(11, 2, value["options"][4]))
    machine.stub(0x8010317C, lambda cpu: describe(cpu, 0, a(cpu)))
    machine.stub(0x801032C4, lambda cpu: describe(cpu, 1, a(cpu)))
    machine.stub(
        0x801033A4,
        lambda cpu: describe(cpu, armor_row(cpu), cpu.load(PRIVATE[4][0], 1) - 9),
    )
    machine.stub(
        0x801033D4,
        lambda cpu: describe(cpu, accessory_row(cpu), cpu.load(PRIVATE[4][0], 1) - 9),
    )
    machine.stub(0x80103688, next_actor)
    machine.stub(0x80103744, switch)
    machine.stub(0x8007ACB0, camera)
    machine.stub(0x800FFC04, information)
    machine.stub(0x80104134, effects)
    machine.stub(0x8007CCF0, projection)
    machine.stub(
        0x800C9E08, lambda cpu: cpu.r.__setitem__(R["v0"], cpu.load(a(cpu) + 0x948))
    )
    machine.stub(0x800C6540, lambda cpu: event(47, a(cpu, 1), a(cpu, 2), a(cpu, 3)))
    machine.follow.update(
        (
            0x80104514,
            0x8010455C,
            0x800C4708,
            0x80040F24,
            0x80040F28,
            *(entry for _, entry, _ in SPECS.values()),
        )
    )


def flatten_input(value: dict) -> list[int]:
    return [
        value["mode"],
        value["seed"],
        value["request"],
        value["menu"],
        *value["frame"],
        *value["private"],
        *value["view"],
        *value["saved"],
        *value["options"],
        *(field for actor in value["actors"] for field in actor),
        *value["flags"],
        *value["heads"],
        *value["offsets"],
        *value["animation"],
        *value["cursors"],
    ]


def run(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    verify_originals(machine)
    place(machine, value)
    calls: list[list[int]] = []
    services(machine, value, calls)
    machine.r[R["a0"]] = STATE if value["mode"] == 5 else value["request"] & MASK
    machine.stores.clear()
    machine.execute(entry)
    result = machine.r[R["v0"]] if value["mode"] in (0, 1, 5) else 0
    values = [
        result,
        machine.load(STATE, 1),
        *(machine.load(address, size) for address, size in (*FRAME, *PRIVATE)),
        *(machine.load(SCRATCH + 0x14 + index * 4) for index in range(21)),
        *(machine.load(0x80108198 + index * 4) for index in range(21)),
        machine.load(PROJECTION),
        *machine.bytes(BUFFER, 96),
        *(byte for row in range(40) for byte in machine.bytes(ROWS + row * 64, 60)),
    ]
    words = (machine.load(SCRATCH) - PACKETS) // 4
    if not 0 <= words < 0x4000:
        raise Fault("invalid terminal packet cursor")
    values.extend(
        [
            words,
            *(machine.load(address) for address in HEADS),
            *(machine.load(PACKETS + i * 4) for i in range(words)),
        ]
    )
    return recorded_observation(
        machine, values, [f"word {i}" for i in range(len(values))], calls
    ), flatten_input(value)


def allowed_stores(_: dict) -> list[tuple[int, int]]:
    return [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (SCRATCH, SCRATCH + 0x100),
        (PACKETS, PACKETS + 0x10000),
        (ROWS, ROWS + 40 * 64),
        (BUFFER, BUFFER + 96),
        (STATE, STATE + 1),
        *((address, address + size) for address, size in (*FRAME, *PRIVATE)),
        *((address, address + 4) for address in HEADS),
        (0x80108198, 0x801081EC),
    ]


def describe(image, _) -> list[str]:
    return [
        f"MENU4 sha1={hashlib.sha1(image.data).hexdigest()}; six exact JR/delay-slot boundaries pinned",
        "Original counts, resident trig and dead-zone execute; external endpoints are protocol doubles",
        "Full state, saved/current view, row bytes, description bytes, packet words/heads/cursor and callback order compared",
    ]


def configure(identifier: int, mutants: dict) -> dict:
    mode, entry, end = SPECS[identifier]
    patches = {name: {address: word} for name, (address, word) in mutants.items()}
    result = mode_case(
        identifier, mode, entry, end, patches, harness=sys.modules[__name__]
    )
    result.update(
        VECTORS=1024,
        MUTANT_VECTORS=128,
        INCLUDES=INCLUDES,
        COMPILER=COMPILER,
        CXX_STANDARD=CXX_STANDARD,
        COMPILER_OPTIONS=COMPILER_OPTIONS,
    )
    return result
