"""F02814-F02816/F02824-F02825: original Gazette title/statistics instructions.

All 33 gazette rows, a framework row's first 60 bytes, the cursor byte and ordered
calls are compared. Row-creation responses deliberately retain arbitrary bytes:
the callers must preserve fields they do not assign. Statistics follows the original
integer, percentage, fastest-time and copy instructions; sprintf/strlen, measurement,
framework creation/rendering and cursor services are boundary doubles. Measured text
and complete row text through the terminator are compared, not native pointers.

The number case compares two actual UiTexturedQuad packets, their links, primitive
cursor, untouched UV high halves and surrounding guard words. AddPrim is the shared
SDK boundary double. Values 0..99, bounded pages and terminated strings are the
original-defined domain; native safety refusals have separate Catch2 cases.
Metadata imports neither disc bytes nor an optional disassembler.
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
from gazette_times_common import configure as time_configuration
from gazette_times_common import install_formatters
from manual_illustration_common import add_prim

MODULE_BASES["MENU/MENU9.PRG"] = 0x80102800
MODULES = ("MENU/MENU9.PRG",)
DRIVER = "gazette_statistics_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\Score\GazetteStatistics.cpp",
    r"Native\Game\src\Menu\Score\GazetteTimes.cpp",
    r"Native\Engine\src\Core\String\GameText.cpp",
    r"Native\Engine\src\Render\UiTexturedQuads.cpp",
)

CREATE_TITLES, RENDER_TITLES, DRAW_NUMBER, CREATE_STATS, RENDER_STATS = range(5)
ROWS = 0x8010A230
TITLE_TEXT, TITLE_DESCRIPTIONS = 0x801086E4, 0x801089BC
STAT_TEXT, STAT_DESCRIPTIONS = 0x80108FDC, 0x80109200
MISC = 0x801097DC
TITLE_CURSOR, STAT_CURSOR = 0x80109898, 0x8010989A
MEMORY = 0x800F1BF0
SCORE = 0x8005FEA0
CLEAR_COUNT, CHEST_COUNT, MAP_COUNT = 0x8010A466, 0x8010A468, 0x8010A46A
INTEGER, PERCENT, FASTEST, COPY = 0x80106CEC, 0x80106D5C, 0x80106DE0, 0x801066F0
GET_ROW, SET_ROW, SET_INFO = 0x800C8E48, 0x800C8E5C, 0x800FFC04
DRAW_CURSOR, DRAW_DIGITS = 0x800FFCDC, 0x80105A0C
DRAW_ROW, DRAW_SCROLL, MEASURE = 0x800C9078, 0x80102A7C, 0x800C7010
ADD_PRIM = 0x8002B4D4
TEXT, MENU_ROW = 0x80300000, 0x80302000
PRIMITIVES, ORDERING, SCRATCH = 0x80303000, 0x80304000, 0x1F800000
TEXT_STRIDE, ROW_BYTES, ROW_COUNT, MENU_BYTES = 16, 16, 33, 60
PRIMITIVE_WORDS, GUARD_BYTES = 28, 16
MASK = 0xFFFFFFFF


def word(rng: random.Random, number: int) -> int:
    edges = (0, 1, 51, 52, 53, 360, 361, 362, 5999, 6000, 0x7FFFFFFF,
             0x80000000, 0x80000004, 0xFFFFFFFF)
    return edges[number % len(edges)] if number % 3 else rng.getrandbits(32)


def vector(rng: random.Random, number: int, mode: int) -> dict:
    if mode == DRAW_NUMBER:
        return {
            "mode": mode,
            "x": s32(word(rng, number)),
            "y": s32(word(rng, number + 5)),
            "number": number % 100,
            "dimmed": 0 if number % 2 == 0 else rng.choice((-1, 1, -0x80000000, 256)),
            "ordering": rng.getrandbits(32),
            "primitives": [rng.getrandbits(32) for _ in range(PRIMITIVE_WORDS)],
        }
    statistics = mode in (CREATE_STATS, RENDER_STATS)
    count = 23 if statistics else 32
    page = number % (count - 7)
    selection = (number // (count - 7)) % 8
    if number % 17 == 0:
        selection = 255
    rows = []
    for index in range(ROW_COUNT):
        rows.append([
            0 if (number + index) % 3 == 0 else rng.randrange(1, 256),
            0 if (number + index) % 2 == 0 else rng.randrange(1, 256),
            rng.choice((0, 1, 7, 8, 9, 127, 128, 254, 255, rng.randrange(256))),
            rng.randrange(256), rng.randrange(-32768, 32768),
            rng.randrange(-32768, 32768),
        ])
    if mode == RENDER_TITLES and page + selection < count:
        rows[page + selection][1] = 1 if number % 4 < 2 else 0
    return {
        "mode": mode,
        "step": 8 if number % 3 == 0 else rng.choice(
            (0, 1, 4, 7, 8, 9, -1, -0x80000000, 0x7FFFFFFF, s32(rng.getrandbits(32)))),
        "page": page, "selection": selection,
        "cursor": rng.randrange(256), "cursor_reply": s32(rng.getrandbits(32)),
        "titles": (0, 1, 0x80000000, MASK, rng.getrandbits(32))[number % 5],
        "title_offsets": rng.sample(range(32, 274 if statistics else 364), 32),
        "description_offsets": rng.sample(range(32, 460 if statistics else 784), 32),
        "missing_offsets": rng.sample(range(6, 80), 2),
        "rows": rows,
        "menu": [[rng.randrange(256) for _ in range(MENU_BYTES)] for _ in range(16)],
        "measure": [s32(word(rng, number + index)) for index in range(8)],
        "current": [0 if number % 5 == 0 else rng.randrange(1, 65536),
                    rng.randrange(65536), rng.randrange(65536)],
        "best": [word(rng, number + index) for index in range(4)],
        "chain": rng.randrange(65536),
        "kills": [rng.randrange(65536) for _ in range(6)],
        "weapons": [rng.randrange(65536) for _ in range(10)],
    }


def text(machine: Machine, address: int) -> list[int]:
    result = []
    for index in range(256):
        byte = machine.load(address + index, 1)
        result.append(byte)
        if byte == 0xE7:
            return result
    raise Fault("unterminated game text")


def setup_rows(machine: Machine, value: dict) -> int:
    statistics = value["mode"] in (CREATE_STATS, RENDER_STATS)
    titles, descriptions = ((STAT_TEXT, STAT_DESCRIPTIONS) if statistics
                            else (TITLE_TEXT, TITLE_DESCRIPTIONS))
    for index, offset in enumerate(value["title_offsets"]):
        machine.put(titles + index * 2, offset, 2)
    for index, offset in enumerate(value["description_offsets"]):
        machine.put(descriptions + index * 2, offset, 2)
    for index, offset in enumerate(value["missing_offsets"]):
        machine.put(MISC + index * 2, offset, 2)
        machine.store(MISC + offset * 2, bytes((64 + index, 0xE7)))
    for index in range(132):
        machine.store(TEXT + index * TEXT_STRIDE, bytes((index, 0xE7)))
    for index, row in enumerate(value["rows"]):
        address = ROWS + index * ROW_BYTES
        machine.store(address, bytes(row[:4]))
        machine.put(address + 4, row[4], 2)
        machine.put(address + 6, row[5], 2)
        machine.put(address + 8, TEXT + (66 + index) * TEXT_STRIDE)
        machine.put(address + 12, TEXT + (99 + index) * TEXT_STRIDE)
    memory = MEMORY + (4 if statistics else 0)
    machine.put(memory, value["page"], 1)
    machine.put(memory + 1, value["selection"], 1)
    cursor = STAT_CURSOR if statistics else TITLE_CURSOR
    machine.put(cursor, value["cursor"], 1)
    machine.store(MENU_ROW, bytes(value["menu"][0]))
    machine.put(SCORE, value["titles"])
    for address, item in zip((CLEAR_COUNT, CHEST_COUNT, MAP_COUNT), value["current"], strict=True):
        machine.put(address, item, 2)
    for offset, item in zip((0x8C, 0x90, 0x98, 0x94), value["best"], strict=True):
        machine.put(SCORE + offset, item)
    machine.put(SCORE + 0x88, value["chain"], 2)
    for index, item in enumerate(value["kills"]):
        machine.put(SCORE + 4 + index * 2, item, 2)
    for index, item in enumerate(value["weapons"]):
        machine.put(SCORE + 0x14 + index * 2, item, 2)
    return cursor


class Recorder:
    def __init__(self, machine: Machine, value: dict) -> None:
        self.calls: list[list[int]] = []
        self.value = value
        self.created = 0
        self.measured = 0
        machine.stub(GET_ROW, self.get_row)
        machine.stub(SET_ROW, self.create_row)
        machine.stub(SET_INFO, self.information)
        machine.stub(DRAW_CURSOR, self.cursor)
        machine.stub(DRAW_DIGITS, self.number)
        machine.stub(DRAW_ROW, lambda m: self.draw(m, 6))
        machine.stub(DRAW_SCROLL, lambda m: self.draw(m, 7))
        machine.stub(MEASURE, self.measure)

    def get_row(self, machine: Machine) -> None:
        self.calls.append([1, machine.r[4]])
        machine.r[2] = MENU_ROW

    def create_row(self, machine: Machine) -> None:
        content = text(machine, machine.load(machine.r[29] + 20))
        self.calls.append([2, *(s32(machine.r[i]) for i in range(4, 8)),
                           s32(machine.load(machine.r[29] + 16)), len(content), *content])
        machine.store(MENU_ROW, bytes(self.value["menu"][self.created]))
        self.created += 1
        machine.r[2] = MENU_ROW

    def information(self, machine: Machine) -> None:
        content = text(machine, machine.r[4])
        self.calls.append([3, machine.r[4], len(content), *content])

    def cursor(self, machine: Machine) -> None:
        self.calls.append([4, machine.r[4], machine.r[5]])
        machine.r[2] = self.value["cursor_reply"] & MASK

    def number(self, machine: Machine) -> None:
        self.calls.append([5, *(s32(machine.r[i]) for i in range(4, 8))])

    def draw(self, machine: Machine, kind: int) -> None:
        self.calls.append([kind, *machine.bytes(machine.r[4], MENU_BYTES)])

    def measure(self, machine: Machine) -> None:
        content = text(machine, machine.r[4])
        self.calls.append([8, len(content), *content])
        machine.r[2] = self.value["measure"][self.measured] & MASK
        self.measured += 1


def run_number(machine: Machine, value: dict, entry: int) -> tuple:
    install_formatters(machine)
    machine.stub(ADD_PRIM, add_prim)
    machine.map_zero(SCRATCH, 12)
    machine.put(SCRATCH, PRIMITIVES + GUARD_BYTES)
    machine.put(SCRATCH + 8, ORDERING)
    machine.put(ORDERING, value["ordering"])
    for index, item in enumerate(value["primitives"]):
        machine.put(PRIMITIVES + index * 4, item)
    for register, item in zip(range(4, 8),
                              (value["x"], value["y"], value["number"], value["dimmed"]),
                              strict=True):
        machine.r[register] = item & MASK
    machine.execute(entry)
    values = [machine.load(SCRATCH), machine.load(ORDERING)]
    values += [machine.load(PRIMITIVES + index * 4) for index in range(PRIMITIVE_WORDS)]
    names = ["cursor", "ordering"] + [f"primitive/guard word {i}" for i in range(PRIMITIVE_WORDS)]
    inputs = [DRAW_NUMBER, value["x"], value["y"], value["number"], value["dimmed"],
              value["ordering"], *value["primitives"]]
    return recorded_observation(machine, values, names, []), inputs


def run(machine: Machine, value: dict, entry: int) -> tuple:
    if value["mode"] == DRAW_NUMBER:
        return run_number(machine, value, entry)
    cursor = setup_rows(machine, value)
    install_formatters(machine)
    machine.follow.update((INTEGER, PERCENT, FASTEST, COPY))
    recorder = Recorder(machine, value)
    machine.r[4] = value["step"] & MASK
    machine.execute(entry)
    values, names = [machine.load(cursor, 1)], ["cursor"]
    fields = [(offset, 1) for offset in range(4)] + [(4, 2), (6, 2), (8, 4), (12, 4)]
    for index in range(ROW_COUNT):
        values += [machine.load(ROWS + index * ROW_BYTES + offset, size) for offset, size in fields]
        names += [f"row {index} offset {offset}" for offset, _ in fields]
    values += list(machine.bytes(MENU_ROW, MENU_BYTES))
    names += [f"menu byte {index}" for index in range(MENU_BYTES)]
    inputs = [value["mode"], value["step"], value["page"], value["selection"],
              value["cursor"], value["cursor_reply"], value["titles"],
              *value["title_offsets"], *value["description_offsets"], *value["missing_offsets"],
              *(item for row in value["rows"] for item in row),
              *(item for row in value["menu"] for item in row),
              *value["measure"], *value["current"], *value["best"], value["chain"],
              *value["kills"], *value["weapons"]]
    return recorded_observation(machine, values, names, recorder.calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    stack = (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK + 4)
    if value["mode"] == DRAW_NUMBER:
        return [stack, (SCRATCH, SCRATCH + 4),
                (PRIMITIVES + GUARD_BYTES, PRIMITIVES + GUARD_BYTES + 80)]
    statistics = value["mode"] in (CREATE_STATS, RENDER_STATS)
    cursor = STAT_CURSOR if statistics else TITLE_CURSOR
    return [stack, (ROWS, ROWS + (23 if statistics else 32) * ROW_BYTES),
            (cursor, cursor + 1), (MENU_ROW, MENU_ROW + MENU_BYTES)]


def describe(image, results) -> list[str]:
    calls = {}
    selections = {"unlocked": 0, "locked": 0, "outside": 0}
    for value, observation, _ in results:
        for call in observation["calls"]:
            calls[call[0]] = calls.get(call[0], 0) + 1
        if value["mode"] == RENDER_TITLES:
            selected = value["page"] + value["selection"]
            state = ("outside" if selected >= 32 else
                     "unlocked" if value["rows"][selected][1] else "locked")
            selections[state] += 1
    return [f"full observed state and ordered boundary calls; calls by kind: {calls}",
            f"title selection coverage: {selections}",
            "bounded original domain; native refusal extensions tested separately"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    # Reuse the dependency's proof budget and strict native compiler configuration.
    options = time_configuration(function, mode, entry, end, mutants)
    case = mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
    for setting in ("VECTORS", "MUTANT_VECTORS", "COMPILER", "COMPILER_OPTIONS"):
        case[setting] = options[setting]
    return case
