"""Shared harness for the gazette camera and panel differentials (MENU9.PRG 238.c:1402-1717 and 2146, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::Score core
(GazetteCamera.cpp) in gazette_camera_driver.cpp.

The panel modes (BANNER, PANEL, FOOTER, FADE) map a primitive buffer whose cursor starts at a drawn offset
and whose bytes the original must write hold a pattern, and an ordering table of distinct sentinel words
with scratch[1] at entry 16 (manual_page_common's layout). The packets from the start to the advanced
cursor are compared word for word after their tags, each with the slot whose chain holds it, the chains
checked for emission order and for keeping each entry's high byte; the inline _insertTPage packets and
links are compared the same way. The SDK AddPrim runs as a stub with its semantics. Gradients outside the
palette and banners of more than 512 lines are refused natively (the original reads past _bgColorStops or
queues one line per row without a bound) and are not run.

RESTORE follows vs_main_memcpy and compares the frame camera's position and angles VECTORs, all four words
of each, with lookAt and the stores checked. CAMERA follows rsin/rcos, the overlay's yaw and pitch matrix
builders, MulMatrix and ApplyMatrix; it compares the position and angles words the original stores and the
whole matrix, with the pads, lookAt and the matrix pad checked unwritten. OVERVIEW stubs every callee as a
recorder: the edge fade, getGeomOffset (answering a drawn offset), setGeomOffset, the camera (writing a
salted matrix), the actor preview (reading the matrix it is given), sprintf (formatting %ld/%d), the text,
the panels and the footer.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MODULE_BASES,
    Machine,
    mode_case,
    printed_problems,
    recorded_observation,
    s16,
    s32,
    store_problems,
)
from manual_illustration_common import add_prim, compare

MODULE = "MENU/MENU9.PRG"
MODULE_BASES[MODULE] = 0x80102800

MODULES = (MODULE, "SLUS_010.40")
DRIVER = "gazette_camera_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Score\GazetteCamera.cpp",
    r"Native\Game\src\Menu\Framework\Framework.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
    r"Native\Engine\src\Core\Math\Matrix.cpp",
    r"Native\Engine\src\Core\Math\Vector.cpp",
    r"Native\Engine\src\Render\GameFont.cpp",
    r"Native\Engine\src\Render\Sprites.cpp",
    r"Native\Engine\src\Render\UiPackets.cpp",
    r"Native\Engine\src\Render\EncodedText.cpp",
)
INCLUDES: tuple[str, ...] = ()

# Driver modes, shared with gazette_camera_driver.cpp.
RESTORE, CAMERA, BANNER, PANEL, FOOTER, OVERVIEW, FADE = range(7)
PACKET_MODES = (BANNER, PANEL, FOOTER, FADE)
# Native PanelDrawResult values.
DRAWN, REFUSED = 0, 1
EVENT_PACKET = 1
(CALL_FADE, CALL_GET, CALL_SET, CALL_CAMERA, CALL_PREVIEW, CALL_TEXT, CALL_PANEL, CALL_FOOTER) = range(1, 9)

# Original callees and data.
ADD_PRIM = 0x8002B4D4
MEMCPY = 0x800490B0
RCOS, RSIN = 0x80040F24, 0x80040F28
MUL_MATRIX, APPLY_MATRIX = 0x80041C68, 0x8004177C
BUILD_YAW, BUILD_PITCH = 0x80104E90, 0x80104F04
BUILD_CAMERA = 0x80104CBC
GRADIENT_PANEL = 0x801051AC
FOOTER_BACKGROUND = 0x8010539C
EDGE_FADE = 0x80106528
GET_GEOM_OFFSET, SET_GEOM_OFFSET = 0x800994D4, 0x800994F4
ACTOR_PREVIEW = 0x800F9EB8
SPRINTF = 0x80026EF4
RENDER_TEXT = 0x800C6540
SCORE = 0x8010A460
CAMERA_ANGLES = 0x8010A430
CAMERA_DISTANCE = 0x8010A444
CAMERA_HEIGHT = 0x8010A44E
POSITION_BACKUP = 0x8010A480
ANGLES_BACKUP = 0x8010A490

SCRATCH = 0x1F800000
CAMERA_BASE = SCRATCH + 0x34  # Render_Camera: position, lookAt, angles (VECTORs)
CAMERA_WORDS = 12
POSITION, LOOK_AT, ANGLES = 0, 4, 8  # word indices

# Fixture addresses.
PRIMITIVES = 0x80280000
PRIMITIVES_SIZE = 0x8000
UNWRITTEN = 0xA5
ORDERING_TABLE = 0x80300000
ORDERING_ENTRIES = 32
SCENE_ENTRY = 16
MATRIX = 0x80310000
MATRIX_BYTES = 0x20
SENTINEL = 0xC0DE0000

LOW24 = 0x00FFFFFF
HIGH8 = 0xFF000000
MASK = 0xFFFFFFFF

GRADIENTS = 4
MAX_BANNER_LINES = 512
# The bytes each panel draw queues besides its banner lines: the shadow (24) and texture page (12).
LINE_BYTES, SHADED_BYTES, FLAT_BYTES, PAGE_BYTES = 20, 36, 24, 12
# Per packet body length, the body bytes the original never writes: the second, third and fourth
# colour's padding.
UNWRITTEN_BYTES = {4: (11,), 8: (11, 19, 27)}
STACK_ARGUMENT = 0x10
# The frame below the caller's stack pointer and the caller's four argument home slots, which
# _renderEnemyInfoBg spills its width into.
STACK = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK + STACK_ARGUMENT)


def int32(rng: random.Random, low: int, high: int) -> int:
    """Mostly in [low, high], sometimes any word."""
    return rng.randrange(low, high + 1) if rng.randrange(8) else s32(rng.getrandbits(32))


def int16(rng: random.Random) -> int:
    return rng.randrange(-0x8000, 0x8000)


def banner_lines(y: int, height: int) -> int:
    end = s32(y + height - 1)
    return end - y if end > y else 0


def sentinel(entry: int, salt: int) -> int:
    """A distinct ordering-table word per entry, outside the primitive buffer, its high byte salted."""
    return ((0x40 + salt % 0x80) << 24) | (0x00E00000 + entry * 4)


def matrix_marker(salt: int) -> list[int]:
    """The camera stub's matrix: nine signed halfwords, then three signed words; the driver computes the same."""
    rotation = [s16(((salt * (index + 3)) & MASK) ^ (index * 0x1111)) for index in range(9)]
    translation = [s32(salt * (index + 7) + index) for index in range(3)]
    return rotation + translation


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value: dict = {"mode": mode}
    if mode in PACKET_MODES:
        value["start"] = rng.choice((0, 4, rng.randrange(0, 0x100, 4)))
        value["salt"] = rng.randrange(0x100)
    if mode == RESTORE:
        value["backup"] = [rng.getrandbits(32) for _ in range(8)]
        value["camera"] = [rng.getrandbits(32) for _ in range(CAMERA_WORDS)]
    elif mode == CAMERA:
        value["pitch"] = rng.choice((0, 0x400, -0x400, 0x200, rng.randrange(-0x800, 0x800), int16(rng)))
        value["yaw"] = rng.choice((0, 0x800, -0x800, 0x400, int16(rng), int16(rng), -0x8000, 0x7FFF))
        value["roll"] = rng.choice((0, int16(rng)))
        value["distance"] = rng.choice((0, 0x600, 0x900, -0x100, rng.randrange(-0x4000, 0x4000),
                                        rng.randrange(-0x40000, 0x40000), s32(rng.getrandbits(32))))
        value["height"] = rng.choice((0, 0x180, 0x181, -1, -0x181, rng.randrange(-0x400, 0x400), int16(rng)))
        value["camera"] = [rng.getrandbits(32) for _ in range(CAMERA_WORDS)]
    elif mode in (BANNER, PANEL):
        gradient = number % GRADIENTS if number % 8 else rng.choice((-1, 4, 5, int32(rng, -9, 9)))
        height = int32(rng, -3, 40)
        if mode == BANNER and number % 16 == 3:
            height = rng.choice((511, 512, 513, 514, 600))
        value.update(x=int32(rng, -20, 330), y=int32(rng, -20, 240), width=int32(rng, -4, 320),
                     height=height, gradient=gradient)
        if number % 32 == 5:
            # Rows past the word's end: the end wraps below the top.
            value["y"] = rng.randrange(0x7FFFFF00, 0x80000000)
    elif mode == FOOTER:
        value["top"] = rng.choice((236, 236 - 6 * rng.randrange(0, 9), int32(rng, -20, 260)))
    elif mode == OVERVIEW:
        value["step"] = rng.choice((0, 1, 7, 8, 9, rng.randrange(0, 13), rng.randrange(-4, 20),
                                    s32(rng.getrandbits(32))))
        value["score"] = rng.choice((0, 1, 999999, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF, rng.getrandbits(32),
                                     rng.randrange(1000000)))
        value["offset"] = [int32(rng, 0, 0x200), int32(rng, 0, 0x200)]
        value["salt"] = rng.getrandbits(32)
    return value


# ---------------------------------------------------------------------- the panel modes
def packet_bytes(value: dict) -> int:
    mode = value["mode"]
    if mode == BANNER:
        return banner_lines(value["y"], value["height"]) * LINE_BYTES + FLAT_BYTES + PAGE_BYTES
    if mode == FADE:
        return 2 * SHADED_BYTES + PAGE_BYTES
    return SHADED_BYTES + FLAT_BYTES + PAGE_BYTES


def install_frame(machine: Machine, value: dict) -> int:
    """Map the scratch cursor and table pointer, the primitive buffer with the pattern where the packets
    will go and the sentinel table; returns the cursor."""
    start = PRIMITIVES + value["start"]
    machine.map_zero(SCRATCH, 8)
    machine.map_zero(PRIMITIVES, PRIMITIVES_SIZE)
    machine.store(start, bytes([UNWRITTEN]) * packet_bytes(value))
    machine.put(SCRATCH, start)
    machine.put(SCRATCH + 4, ORDERING_TABLE + SCENE_ENTRY * 4)
    machine.map_zero(ORDERING_TABLE, ORDERING_ENTRIES * 4)
    for entry in range(ORDERING_ENTRIES):
        machine.put(ORDERING_TABLE + entry * 4, sentinel(entry, value["salt"]))
    machine.stub(ADD_PRIM, add_prim)
    return start


def packets(machine: Machine, start: int, problems: list[str]) -> list[tuple[int, list[int]]]:
    """Each packet from `start` to the cursor, (address, body words)."""
    cursor = machine.load(SCRATCH)
    if not start <= cursor <= PRIMITIVES + PRIMITIVES_SIZE:
        problems.append(f"cursor {cursor:#x} outside the primitive buffer")
        return []
    found, address = [], start
    while address < cursor:
        words = machine.load(address + 3, 1)
        end = address + 4 * (1 + words)
        if words == 0 or end > cursor:
            problems.append(f"packet {address:#x} length {words} overruns the cursor {cursor:#x}")
            break
        found.append((address, body_words(machine, address, words, problems)))
        address = end
    return found


def body_words(machine: Machine, address: int, words: int, problems: list[str]) -> list[int]:
    """A packet's words after its tag, each byte the original never writes checked for the pattern and
    read as 0."""
    body = machine.bytes(address + 4, words * 4)
    skipped = UNWRITTEN_BYTES.get(words, ())
    problems.extend(f"packet {address:#x} body byte {offset} written {body[offset]:#x}"
                    for offset in skipped if body[offset] != UNWRITTEN)
    cleared = bytes(0 if offset in skipped else byte for offset, byte in enumerate(body))
    return [int.from_bytes(cleared[index:index + 4], "little") for index in range(0, len(cleared), 4)]


def slots(machine: Machine, found: list[tuple[int, list[int]]], salt: int, problems: list[str]) -> dict[int, int]:
    """Each packet's slot from the table chains, each chain checked for reverse emission order, the entry's
    high byte and its sentinel end."""
    addresses = {address & LOW24: address for address, _ in found}
    slot_of: dict[int, int] = {}
    for entry in range(ORDERING_ENTRIES):
        word = machine.load(ORDERING_TABLE + entry * 4)
        if word & HIGH8 != sentinel(entry, salt) & HIGH8:
            problems.append(f"table entry {entry} high byte {word >> 24:#x}")
        link, previous = word & LOW24, None
        while link in addresses:
            address = addresses.pop(link)
            if previous is not None and address >= previous:
                problems.append(f"slot {entry - SCENE_ENTRY} links {previous:#x} before {address:#x}")
            slot_of[address] = entry - SCENE_ENTRY
            previous = address
            link = machine.load(address) & LOW24
        if link != sentinel(entry, salt) & LOW24:
            problems.append(f"table entry {entry} chain ends at {link:#x}")
    for address in addresses.values():
        problems.append(f"packet {address:#x} is in no table chain")
    return slot_of


def run_packets(machine: Machine, value: dict, entry: int) -> tuple[list, list[int]]:
    mode = value["mode"]
    inputs = [mode]
    if mode in (BANNER, PANEL):
        inputs += [value[key] for key in ("x", "y", "width", "height", "gradient")]
        lines = banner_lines(value["y"], value["height"]) if mode == BANNER else 0
        if not 0 <= value["gradient"] < GRADIENTS or lines > MAX_BANNER_LINES:
            return [[], [REFUSED], []], inputs
    elif mode == FOOTER:
        inputs.append(value["top"])
    start = install_frame(machine, value)
    if mode in (BANNER, PANEL):
        for register, key in zip((4, 5, 6, 7), ("x", "y", "width", "height"), strict=True):
            machine.r[register] = value[key] & MASK
        machine.put(machine.r[29] + STACK_ARGUMENT, value["gradient"])
    elif mode == FOOTER:
        machine.r[4] = value["top"] & MASK
    machine.execute(entry)
    problems: list[str] = []
    found = packets(machine, start, problems)
    slot_of = slots(machine, found, value["salt"], problems)
    events = [item for address, body in found
              for item in (EVENT_PACKET, slot_of.get(address, 99), len(body), *body)]
    cursor = machine.load(SCRATCH)
    allowed = [STACK, (SCRATCH, SCRATCH + 4), (start, cursor),
               (ORDERING_TABLE, ORDERING_TABLE + ORDERING_ENTRIES * 4)]
    problems += store_problems(machine.stores, allowed)
    return [problems, [DRAWN, *events], []], inputs


# ---------------------------------------------------------------------- the camera modes
def install_camera(machine: Machine, words: list[int]) -> None:
    machine.map_zero(SCRATCH, 0x80)
    for index, word in enumerate(words):
        machine.put(CAMERA_BASE + index * 4, word)


def camera_words(machine: Machine) -> list[int]:
    return [machine.load(CAMERA_BASE + index * 4) for index in range(CAMERA_WORDS)]


def run_restore(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    install_camera(machine, value["camera"])
    for index, word in enumerate(value["backup"]):
        machine.put(POSITION_BACKUP + index * 4, word)
    machine.follow.add(MEMCPY)
    machine.execute(entry)
    words = camera_words(machine)
    unchanged = int(words[LOOK_AT:ANGLES] == value["camera"][LOOK_AT:ANGLES])
    values = [*words[POSITION:LOOK_AT], *words[ANGLES:], unchanged]
    names = [*(f"position word {i}" for i in range(4)), *(f"angles word {i}" for i in range(4)), "lookAt kept"]
    inputs = [*value["backup"], *value["camera"]]
    return recorded_observation(machine, values, names, []), [RESTORE, *inputs]


def run_camera(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    install_camera(machine, value["camera"])
    machine.map_zero(MATRIX, MATRIX_BYTES)
    for index in range(MATRIX_BYTES // 4):
        machine.put(MATRIX + index * 4, SENTINEL | index)
    for offset, key in ((0, "pitch"), (2, "yaw"), (4, "roll")):
        machine.put(CAMERA_ANGLES + offset, value[key], 2)
    machine.put(CAMERA_DISTANCE, value["distance"])
    machine.put(CAMERA_HEIGHT, value["height"], 2)
    machine.follow.update((RCOS, RSIN, BUILD_YAW, BUILD_PITCH, MUL_MATRIX, APPLY_MATRIX))
    machine.r[4] = MATRIX
    machine.execute(entry)
    words = camera_words(machine)
    values = [s32(words[POSITION + index]) for index in range(3)]
    values += [s32(words[ANGLES + index]) for index in range(3)]
    values += [machine.load(MATRIX + index * 2, 2, True) for index in range(9)]
    values += [s32(machine.load(MATRIX + 0x14 + index * 4)) for index in range(3)]
    names = [*(f"position {axis}" for axis in "xyz"), *(f"angle {axis}" for axis in "xyz"),
             *(f"m[{i // 3}][{i % 3}]" for i in range(9)), *(f"t[{axis}]" for axis in "xyz")]
    kept = [index for index in (3, *range(LOOK_AT, ANGLES), 11) if words[index] != value["camera"][index]]
    if machine.load(MATRIX + 0x12, 2) != (SENTINEL | 4) >> 16:
        kept.append(-1)
    values.append(int(not kept))
    names.append("pads and lookAt kept")
    inputs = [value[key] for key in ("pitch", "yaw", "roll", "distance", "height")]
    return recorded_observation(machine, values, names, []), [CAMERA, *inputs]


# ---------------------------------------------------------------------- the overview
def c_string(machine: Machine, address: int, limit: int = 64) -> bytes:
    data = bytearray()
    while len(data) < limit:
        byte = machine.load(address + len(data), 1)
        if not byte:
            break
        data.append(byte)
    return bytes(data)


def format_text(pattern: bytes, argument: int) -> bytes:
    """sprintf with %ld or %d of a signed word, the only conversions the stub supports."""
    out, index = bytearray(), 0
    while index < len(pattern):
        if pattern[index:index + 3] == b"%ld":
            out += str(s32(argument)).encode()
            index += 3
        elif pattern[index:index + 2] == b"%d":
            out += str(s32(argument)).encode()
            index += 2
        else:
            out.append(pattern[index])
            index += 1
    return bytes(out)


def run_overview(machine: Machine, value: dict, entry: int) -> tuple[dict, list[int]]:
    calls: list[list[int]] = []
    machine.map_zero(SCRATCH, 8)
    machine.put(SCRATCH + 4, ORDERING_TABLE + SCENE_ENTRY * 4)
    machine.put(SCORE, value["score"])
    marker = matrix_marker(value["salt"])

    def get_offset(m: Machine) -> None:
        calls.append([CALL_GET])
        m.put(m.r[4], value["offset"][0])
        m.put(m.r[4] + 4, value["offset"][1])

    def build_camera(m: Machine) -> None:
        calls.append([CALL_CAMERA])
        for index in range(9):
            m.put(m.r[4] + index * 2, marker[index], 2)
        m.put(m.r[4] + 0x12, 0, 2)
        for index in range(3):
            m.put(m.r[4] + 0x14 + index * 4, marker[9 + index])

    def preview(m: Machine) -> None:
        calls.append([CALL_PREVIEW, *(m.load(m.r[4] + index * 2, 2, True) for index in range(9)),
                      *(s32(m.load(m.r[4] + 0x14 + index * 4)) for index in range(3))])

    def sprintf(m: Machine) -> None:
        text = format_text(c_string(m, m.r[5]), m.r[6])
        m.store(m.r[4], text + b"\0")

    def text(m: Machine) -> None:
        data = c_string(m, m.r[4])
        slot = s32(m.r[7] - (ORDERING_TABLE + SCENE_ENTRY * 4)) // 4
        calls.append([CALL_TEXT, len(data), *data, m.r[5], m.r[6], slot])

    def panel(m: Machine) -> None:
        calls.append([CALL_PANEL, *(s32(m.r[r]) for r in (4, 5, 6, 7)),
                      s32(m.load(m.r[29] + STACK_ARGUMENT))])

    stubs = {
        EDGE_FADE: lambda m: calls.append([CALL_FADE]),
        GET_GEOM_OFFSET: get_offset,
        SET_GEOM_OFFSET: lambda m: calls.append([CALL_SET, s32(m.load(m.r[4])), s32(m.load(m.r[4] + 4))]),
        BUILD_CAMERA: build_camera,
        ACTOR_PREVIEW: preview,
        SPRINTF: sprintf,
        RENDER_TEXT: text,
        GRADIENT_PANEL: panel,
        FOOTER_BACKGROUND: lambda m: calls.append([CALL_FOOTER, s32(m.r[4])]),
    }
    for address, stub in stubs.items():
        machine.stub(address, stub)
    machine.r[4] = value["step"] & MASK
    machine.execute(entry)
    inputs = [value["step"], value["score"], *value["offset"], value["salt"]]
    return recorded_observation(machine, [], [], calls), [OVERVIEW, *inputs]


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    if mode in PACKET_MODES:
        return run_packets(machine, value, entry)
    return {RESTORE: run_restore, CAMERA: run_camera, OVERVIEW: run_overview}[mode](machine, value, entry)


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    stack = STACK
    if value["mode"] == RESTORE:
        return [stack, (CAMERA_BASE, CAMERA_BASE + 0x10), (CAMERA_BASE + 0x20, CAMERA_BASE + 0x30)]
    if value["mode"] == CAMERA:
        return [stack, (CAMERA_BASE, CAMERA_BASE + 0xC), (CAMERA_BASE + 0x20, CAMERA_BASE + 0x2C),
                (MATRIX, MATRIX + 0x12), (MATRIX + 0x14, MATRIX + MATRIX_BYTES)]
    return [stack]


def check(image, value: dict, observation, native: list[int], reference) -> list[str]:
    if value["mode"] in PACKET_MODES:
        problems, expected, _ = observation
        return problems + compare(native, expected)
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    drawn = refused = queued = calls = 0
    for value, observation, _ in results:
        if value["mode"] in PACKET_MODES:
            expected = observation[1]
            if expected[0] == REFUSED:
                refused += 1
                continue
            drawn += 1
            index = 1
            while index < len(expected):
                queued += 1
                index += 3 + expected[index + 2]
        else:
            drawn += 1
            calls += len(observation["calls"])
    return [f"coverage: {drawn} runs ({queued} packets, {calls} calls), {refused} refused"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict, vectors: int = 256) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]) | {
        "INCLUDES": INCLUDES, "VECTORS": vectors}
