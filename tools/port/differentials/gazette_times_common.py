"""F02828-F02831, MENU9.PRG 238.c:2400-2628: original instructions versus GazetteTimes.cpp.

The original _strcpy is followed. sprintf/strlen, the menu framework, cursor, compact font and
gradient renderer are boundary doubles. Compare the entire 64-byte format destination, all fields
of nine gazette rows (including the untouched ninth), the first 60 bytes of eight framework rows,
the cursor byte and the ordered calls with complete text bytes, positions, colors and rectangles.

The byte-wide animation covers 0..255, all 24 score bits and both difficulty values are imported,
and presentation offsets and the round increment exercise word wrap. Only selections 0..7 and
terminated labels are original-defined vectors. Native short spans, missing labels, invalid
selections and non-seven-bit round refusals are separately tested, not original-equivalence claims.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MODULE_BASES,
    Fault,
    Machine,
    mode_case,
    recorded_observation,
    s32,
)

MODULE_BASES["MENU/MENU9.PRG"] = 0x80102800
MODULES = ("MENU/MENU9.PRG",)
DRIVER = "gazette_times_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Score\GazetteTimes.cpp",
    r"Native\Engine\src\Core\String\GameText.cpp",
)

FORMAT, CREATE, LEAVE, RENDER = range(4)
CLEAR_COUNT = 0x8010A466
ROWS = 0x8010A230
TITLES = 0x80109598
DESCRIPTIONS = 0x8010960C
MISC = 0x801097DC
CURSOR = 0x801098A0
SELECTED = 0x800F1BF6
SCORES = 0x8005FEC8
COPY = 0x801066F0
SPRINTF = 0x80026EF4
STRLEN = 0x80026E94
GET_ROW = 0x800C8E48
SET_ROW = 0x800C8E5C
SET_INFO = 0x800FFC04
DRAW_CURSOR = 0x800FFCDC
DRAW_TEXT = 0x800C6540
DRAW_PANEL = 0x801051AC
TEXT = 0x80300000
OUTPUT = 0x80302000
MENU_ROWS = 0x80303000
ORDERING = 0x80304000
SCRATCH = 0x1F800000
TEXT_STRIDE = 32
ROW_BYTES = 16
MENU_ROW_BYTES = 64
OBSERVED_MENU_BYTES = 60
OUTPUT_BYTES = 64
ROW_COUNT = 9
TRIALS = 8
PLACES = 3
MASK = 0xFFFFFFFF
CALL_GET, CALL_SET, CALL_INFO, CALL_CURSOR, CALL_TEXT, CALL_PANEL = range(1, 7)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    if mode == FORMAT:
        times = (0, 1, 9, 10, 59, 60, 61, 5998, 5999, 6000, 6001, 0x7FFFFFFF,
                 -1, -9, -10, -59, -60, -61, -6000, -0x80000000)
        rounds = (0, 8, 9, 98, 99, 0x7FFFFFFF, -0x80000000, -2, -1)
        placeholder = [rng.randrange(0xE7) for _ in range(number % 16)] + [0xE7]
        return {
            "mode": mode,
            "clear": 0 if number % 7 == 0 else rng.randrange(1, 0x10000),
            "round": rounds[number % len(rounds)] if number % 2 == 0 else s32(rng.getrandbits(32)),
            "minutes": times[number % len(times)] if number % 3 != 0 else s32(rng.getrandbits(32)),
            "placeholder": placeholder,
        }
    settled = number % 3 == 0
    title_offsets = rng.sample(range(8, 58), TRIALS)
    description_offsets = rng.sample(range(8, 232), TRIALS)
    rows = []
    for index in range(ROW_COUNT):
        delay = 0 if settled else (number + index) % 4
        if number % 5 == 0 and not settled:
            delay = rng.choice((0, 1, 2, 8, 127, 128, 254, 255, rng.randrange(256)))
        rows.append([rng.randrange(3), rng.randrange(256), delay, rng.randrange(3),
                     rng.randrange(-0x8000, 0x8000), rng.randrange(-0x8000, 0x8000)])
    menu = [[rng.randrange(256) for _ in range(OBSERVED_MENU_BYTES)] for _ in range(TRIALS)]
    for index, row in enumerate(menu):
        row[0] = 1 if settled else (number + index) % 4
    times = (0, 0x800000, 0x800000, 0x800001, 0x7FFFFF, 0xFFFFFF, 0x010203,
             0x000909, 0x005963, 0x090A09)
    scores = []
    for index in range(TRIALS * PLACES):
        time = times[(number + index) % len(times)] if number % 2 == 0 else rng.randrange(1 << 24)
        if number % 7 == 0:
            time = 0x800000
        scores.append(time | (rng.randrange(256) << 24))
    return {
        "mode": mode,
        "step": 8 if settled and number % 2 == 0 else rng.choice(
            (0, 1, 7, 8, 9, -1, -0x80000000, 0x7FFFFFFF, s32(rng.getrandbits(32)))),
        "selected": number % TRIALS,
        "cursor": rng.randrange(256),
        "cursor_reply": s32(rng.getrandbits(32)),
        "title_offsets": title_offsets,
        "description_offsets": description_offsets,
        "missing_offsets": rng.sample(range(6, 1646), 2),
        "rows": rows,
        "menu": menu,
        "scores": scores,
    }


def c_string(machine: Machine, address: int) -> bytes:
    result = bytearray()
    for offset in range(128):
        byte = machine.load(address + offset, 1)
        if byte == 0:
            return bytes(result)
        result.append(byte)
    raise Fault("unterminated ASCII string")


def sprintf(machine: Machine) -> None:
    fmt = c_string(machine, machine.r[5]).decode("ascii")
    count = fmt.count("%")
    arguments = [s32(machine.r[6]), s32(machine.r[7]),
                 s32(machine.load(machine.r[29] + 16))]
    result = (fmt % tuple(arguments[:count])).encode("ascii")
    machine.store(machine.r[4], result + b"\0")
    machine.r[2] = len(result)


def install_formatters(machine: Machine) -> None:
    machine.stub(SPRINTF, sprintf)
    machine.stub(STRLEN, lambda m: m.r.__setitem__(2, len(c_string(m, m.r[4]))))


def run_format(machine: Machine, value: dict, entry: int) -> tuple:
    install_formatters(machine)
    machine.follow.add(COPY)
    machine.put(CLEAR_COUNT, value["clear"], 2)
    machine.put(MISC, 6, 2)
    machine.store(MISC + 12, bytes(value["placeholder"]))
    machine.store(OUTPUT, bytes([0xA5] * OUTPUT_BYTES))
    machine.r[4] = OUTPUT
    machine.r[5] = value["round"] & MASK
    machine.r[6] = value["minutes"] & MASK
    machine.execute(entry)
    output = [machine.load(OUTPUT + index, 1) for index in range(OUTPUT_BYTES)]
    inputs = [FORMAT, value["clear"], value["round"], value["minutes"],
              len(value["placeholder"]), *value["placeholder"]]
    return recorded_observation(machine, output, [f"output[{i}]" for i in range(OUTPUT_BYTES)], []), inputs


def setup_rows(machine: Machine, value: dict) -> None:
    for index, offset in enumerate(value["title_offsets"]):
        machine.put(TITLES + index * 2, offset, 2)
    for index, offset in enumerate(value["description_offsets"]):
        machine.put(DESCRIPTIONS + index * 2, offset, 2)
    machine.put(MISC + 8, value["missing_offsets"][0], 2)
    machine.put(MISC + 10, value["missing_offsets"][1], 2)
    for index, row in enumerate(value["rows"]):
        address = ROWS + index * ROW_BYTES
        machine.store(address, bytes(row[:4]))
        machine.put(address + 4, row[4], 2)
        machine.put(address + 6, row[5], 2)
        machine.put(address + 8, TEXT + (16 + index) * TEXT_STRIDE)
        machine.put(address + 12, TEXT + (25 + index) * TEXT_STRIDE)
    for index, row in enumerate(value["menu"]):
        machine.store(MENU_ROWS + index * MENU_ROW_BYTES, bytes(row) + bytes(4))
    for index, score in enumerate(value["scores"]):
        machine.put(SCORES + index * 4, score)
    machine.put(SELECTED, value["selected"], 1)
    machine.put(CURSOR, value["cursor"], 1)
    machine.map_zero(SCRATCH, 16)
    machine.put(SCRATCH + 4, ORDERING)


class Recorder:
    def __init__(self, machine: Machine, value: dict) -> None:
        self.calls: list[list[int]] = []
        self.value = value
        machine.stub(GET_ROW, self.get_row)
        machine.stub(SET_ROW, self.set_row)
        machine.stub(SET_INFO, lambda m: self.calls.append([CALL_INFO, m.r[4]]))
        machine.stub(DRAW_CURSOR, self.cursor)
        machine.stub(DRAW_TEXT, self.text)
        machine.stub(DRAW_PANEL, self.panel)

    def get_row(self, machine: Machine) -> None:
        self.calls.append([CALL_GET, s32(machine.r[4])])
        machine.r[2] = MENU_ROWS + machine.r[4] * MENU_ROW_BYTES

    def set_row(self, machine: Machine) -> None:
        self.calls.append([CALL_SET, *(s32(machine.r[i]) for i in range(4, 8)),
                           s32(machine.load(machine.r[29] + 16)), machine.load(machine.r[29] + 20)])
        machine.r[2] = MENU_ROWS + machine.r[4] * MENU_ROW_BYTES

    def cursor(self, machine: Machine) -> None:
        self.calls.append([CALL_CURSOR, machine.r[4], machine.r[5]])
        machine.r[2] = self.value["cursor_reply"] & MASK

    def text(self, machine: Machine) -> None:
        text = c_string(machine, machine.r[4])
        self.calls.append([CALL_TEXT, machine.r[5], machine.r[6], machine.r[7], len(text), *text])

    def panel(self, machine: Machine) -> None:
        self.calls.append([CALL_PANEL, *(s32(machine.r[i]) for i in range(4, 8)),
                           s32(machine.load(machine.r[29] + 16))])


def run(machine: Machine, value: dict, entry: int) -> tuple:
    if value["mode"] == FORMAT:
        return run_format(machine, value, entry)
    setup_rows(machine, value)
    install_formatters(machine)
    recorder = Recorder(machine, value)
    machine.r[4] = value["step"] & MASK
    machine.execute(entry)
    values = [machine.load(CURSOR, 1)]
    names = ["cursor"]
    for index in range(ROW_COUNT):
        address = ROWS + index * ROW_BYTES
        fields = [(offset, 1) for offset in range(4)] + [(4, 2), (6, 2), (8, 4), (12, 4)]
        values += [machine.load(address + offset, size) for offset, size in fields]
        names += [f"row {index} offset {offset}" for offset, _ in fields]
    for index in range(TRIALS):
        values += [machine.load(MENU_ROWS + index * MENU_ROW_BYTES + offset, 1)
                   for offset in range(OBSERVED_MENU_BYTES)]
        names += [f"menu {index} byte {offset}" for offset in range(OBSERVED_MENU_BYTES)]
    inputs = [value["mode"], value["step"], value["selected"], value["cursor"], value["cursor_reply"],
              *value["title_offsets"], *value["description_offsets"], *value["missing_offsets"],
              *(word for row in value["rows"] for word in row),
              *(byte for row in value["menu"] for byte in row), *value["scores"]]
    return recorded_observation(machine, values, names, recorder.calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    stack = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK + 4)
    if value["mode"] == FORMAT:
        return [stack, (OUTPUT, OUTPUT + OUTPUT_BYTES)]
    return [stack, (ROWS, ROWS + TRIALS * ROW_BYTES), (CURSOR, CURSOR + 1),
            (MENU_ROWS, MENU_ROWS + TRIALS * MENU_ROW_BYTES)]


def describe(image, results) -> list[str]:
    calls = {}
    for _, observation, _ in results:
        for call in observation["calls"]:
            calls[call[0]] = calls.get(call[0], 0) + 1
    return [
        f"complete output/state and ordered calls checked; calls by kind: {calls}",
        "external services doubled; malformed native inputs are separate refusal tests",
    ]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__]) | {
        "VECTORS": 1024,
        "MUTANT_VECTORS": 128,
        "COMPILER": "clang-cl",
        "COMPILER_OPTIONS": ("/W4", "/WX", "/clang:-Wall", "/clang:-Wextra",
                             "/clang:-Wconversion", "/clang:-Wsign-conversion"),
    }
