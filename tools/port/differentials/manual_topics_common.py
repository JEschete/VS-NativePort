"""Shared original-instruction and native-driver support for the MENUE manual topics and page text."""

from __future__ import annotations

import random

import differential
from differential import Fault, Machine

MODULE = "MENU/MENUE.PRG"
MODULE_BASE = 0x80102800
MODULES = (MODULE,)
DRIVER = "manual_topics_driver.cpp"
# The cores return std::expected.
CXX_STANDARD = "c++latest"
SOURCES = (r"Native\Game\src\Menu\ManualTopics.cpp",)
INCLUDES = (
    r"Native\Engine\include",
    r"Native\Game\include",
)

differential.MODULE_BASES[MODULE] = MODULE_BASE

__all__ = ("CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES", "SOURCES")

MASK = 0xFFFFFFFF

# Original functions, callees and data.
SET_TOPIC_ROWS = 0x80102CD8
GET_SELECTED_TOPIC_ROW = 0x80102DF0
COPY_ENCODED_LINE = 0x80103C9C
SET_MENU_ROWS = 0x801005E0
GET_SELECTED_ROW = 0x801008C8
CLEAR_MENU_EXCEPT = 0x800FA8E0
FLY_OUT_AND_HOIST = 0x800FA92C
SET_TEXT_BOX = 0x800C6BF0
TOPIC_TABLE = 0x80104E54
TOPIC_TABLE_BYTES = 516 * 2
TOP_MENU_STATE = 0x80105230
SELECTED_ROW = 0x80105234
SCROLL_POSITION = 0x80105244
HELP_TEXT = 0x80105254
CURRENT_MANUAL = 0x800F1BC8
CURSOR_MEMORIES = 0x800F4EE8
SETTINGS = 0x80060020
CURSOR_MEMORY_SETTING = SETTINGS + 1

# Inputs. Help text and source lines live only in the overlay, so reading past them faults.
INPUT = 0x80200000
DESTINATION = 0x80300000
STRINGS = 0x80280000

# Driver operations, shared with manual_topics_driver.cpp.
OP_SET_ROWS, OP_SELECT_ROW, OP_UPDATE_MENU, OP_LAST_PAGE, OP_COPY_LINE, OP_COMPOSE_PAGE = 1, 2, 3, 4, 5, 6

# Native statuses: 0, or 1 + the TopicError / PageTextError value.
NO_ROWS, TOO_FEW_STRINGS, SHORT_TOPIC_TABLE, TOPIC_OUTSIDE_TABLE, UNTERMINATED_TOPIC = 1, 2, 3, 4, 5
NEGATIVE_SCROLL, SHORT_HELP_TEXT, LINE_OUTSIDE_TEXT, UNTERMINATED_LINE, PAGE_OVERFLOW = 1, 2, 3, 4, 5
READ_REFUSALS = (SHORT_HELP_TEXT, LINE_OUTSIDE_TEXT, UNTERMINATED_LINE)

TERMINATOR, NEWLINE, CONTROL = 0xE7, 0xE8, 0xEC
PAGE_CAPACITY = 1024
# _copyNextPageText's buffer, at sp + 0x10 under its 0x428-byte frame.
PAGE_BUFFER = differential.DEFAULT_STACK - 0x428 + 0x10


def signed(value: int, bits: int = 32) -> int:
    value &= (1 << bits) - 1
    return value - (1 << bits) if value >> (bits - 1) else value


def imm(word: int, value: int) -> int:
    return (word & 0xFFFF0000) | (value & 0xFFFF)


def opcode(word: int, op: int) -> int:
    return (word & 0x03FFFFFF) | (op << 26)


LH, LB = 0x21, 0x20
NOP = 0

# name: (address, original word, edit) per ID; describe() checks each original word against the image.
EDITS = {
    "F04158": {
        "F04158-last-row-type-8": (0x80102D44, 0x34630004, lambda w: imm(w, 8)),
        "F04158-short-list-below-8": (0x80102D4C, 0x29020009, lambda w: imm(w, 8)),
        "F04158-top-rows-below-9": (0x80102D58, 0x28A20008, lambda w: imm(w, 9)),
        "F04158-row-info-0x103": (0x80102DC4, 0x24050102, lambda w: imm(w, 0x103)),
        "F04158-memory-not-forced": (0x80102DCC, 0xA2020001, lambda w: NOP),
        "F04158-middle-row-5": (0x80102DA0, 0x24030004, lambda w: imm(w, 5)),
        "F04158-memory-not-restored": (0x80102DD0, 0xA2110001, lambda w: NOP),
    },
    "F02989": {
        "F02989-cancel-below-0": (0x80102E04, 0x2A02FFFF, lambda w: imm(w, 0)),
        "F02989-keeps-row-6": (0x80102E14, 0x24040005, lambda w: imm(w, 6)),
        "F02989-hoist-offset-2": (0x80102E30, 0x24050001, lambda w: imm(w, 2)),
        "F02989-undecided-flies-out": (0x80102E20, 0x06000004, lambda w: NOP),
        "F02989-cursor-sign-extended": (0x80102E28, 0x90444EEC, lambda w: opcode(w, LB)),
    },
    "F02991": {
        "F02991-init-ignored": (0x80103920, 0x10800005, lambda w: 0x10000005),
        "F02991-descriptions-shifted": (0x80103960, 0x2409001C, lambda w: imm(w, 0x1E)),
        "F02991-reads-next-manual": (0x801039CC, 0x94451BC8, lambda w: imm(w, 0x1BCA)),
        "F02991-step-not-advanced": (0x801039E4, 0x24420001, lambda w: imm(w, 0)),
        "F02991-selection-not-stored": (0x80103A00, 0xAC435234, lambda w: NOP),
        "F02991-lists-13-rows": (0x801039C8, 0x2404000E, lambda w: imm(w, 0x0D)),
    },
    "F02994": {
        "F02994-count-sign-extended": (0x80103B78, 0x94440010, lambda w: opcode(w, LH)),
        "F02994-rounds-to-11": (0x80103BA8, 0x2482000A, lambda w: imm(w, 0x0B)),
        "F02994-less-9-lines": (0x80103BB0, 0x2484FFF6, lambda w: imm(w, 0xFFF5)),
        "F02994-negative-kept": (0x80103BBC, 0x00002021, lambda w: NOP),
        "F02994-never-rounds": (0x80103BA4, 0x18600002, lambda w: 0x10000002),
    },
    "F02995": {
        "F02995-font-table-7": (0x80103BDC, 0x24020006, lambda w: imm(w, 7)),
        "F02995-eleven-line-page": (0x80103C14, 0x24B2000A, lambda w: imm(w, 0x0B)),
        "F02995-count-sign-extended": (0x80103C18, 0x94430010, lambda w: opcode(w, LH)),
        "F02995-ends-with-0xE6": (0x80103C6C, 0x240200E7, lambda w: imm(w, 0xE6)),
        "F02995-text-box-1": (0x80103C74, 0x00002021, lambda w: 0x24040001),
        "F02995-line-offset-in-bytes": (0x80103C50, 0x00052840, lambda w: NOP),
    },
    "F02996": {
        "F02996-terminator-0xE6": (0x80103CA0, 0x240200E7, lambda w: imm(w, 0xE6)),
        "F02996-control-from-0xED": (0x80103CAC, 0x2C6200EC, lambda w: imm(w, 0xED)),
        "F02996-code-byte-dropped": (0x80103CB8, 0xA0830000, lambda w: NOP),
        "F02996-newline-0xE9": (0x80103CE0, 0x240200E8, lambda w: imm(w, 0xE9)),
        "F02996-returns-newline": (0x80103CEC, 0x24820001, lambda w: imm(w, 0)),
    },
}


def mutants(identifier: str) -> dict:
    """Static patches derived from the recorded words: case discovery must not read disc images (#367)."""
    patches = {}
    for name, (address, original, edit) in EDITS[identifier].items():
        patches[name] = {address: edit(original) & MASK}
    return patches


def verify_original_words(identifier: str, image) -> None:
    stale = [f"{name}: {address:#x} holds {image.read(address):#010x}, expected {original:#010x}"
             for name, (address, original, _edit) in EDITS[identifier].items() if image.read(address) != original]
    if stale:
        raise SystemExit("; ".join(stale))


def compare(native: list[int], expected: list[int]) -> list[str]:
    if native == expected:
        return []
    for index, (left, right) in enumerate(zip(native, expected, strict=False)):
        if left != right:
            return [f"output {index}: native {left} original {right} "
                    f"(native {len(native)} values, original {len(expected)})"]
    return [f"native {len(native)} values, original {len(expected)}"]


def unfaulted(observation: list[int], native: list[int]) -> list[str]:
    """Compare the native output with an observation led by the original's fault flag."""
    if observation[0]:
        return ["the original faulted"]
    return compare(native, observation[1:])


def byte_stores(machine: Machine) -> list[int]:
    return [address for address, size in machine.stores if size == 1]


# ---------------------------------------------------------------------------------------------- topic menu
class MenuRecord:
    """The framework calls the original made and the remembered cursor and setting."""

    def __init__(self) -> None:
        self.creations = 0
        self.setting_while_creating = 0
        self.count = 0
        self.info = 0
        self.offsets: list[int] = []
        self.types: list[int] = []
        self.next_row = -1
        self.cleared: list[int] = []
        self.fly_outs: list[int] = []


def install_menu(machine: Machine, row: int, scroll: int, setting: int, next_row: int = -1) -> MenuRecord:
    record = MenuRecord()
    record.next_row = next_row
    machine.map_zero(CURSOR_MEMORIES, 0x40)
    machine.map_zero(SETTINGS & ~0x3F, 0x40)
    machine.put(CURSOR_MEMORIES + 4, row, 1)
    machine.put(CURSOR_MEMORIES + 5, scroll, 1)
    machine.put(CURSOR_MEMORY_SETTING, setting, 1)

    def set_menu_rows(m: Machine) -> None:
        count = signed(m.r[4])
        record.creations += 1
        record.setting_while_creating = m.load(CURSOR_MEMORY_SETTING, 1)
        record.count = count
        record.info = signed(m.r[5])
        rows = count if 0 < count <= 64 else 0
        record.offsets = [(m.load(m.r[6] + index * 4) - TOPIC_TABLE) & MASK for index in range(rows * 2)]
        record.types = [m.load(m.r[7] + index * 4, 4, True) for index in range(rows)]

    def get_selected_row(m: Machine) -> None:
        m.r[2] = record.next_row & MASK

    def clear_menu_except(m: Machine) -> None:
        record.cleared.append(signed(m.r[4]))

    def fly_out(m: Machine) -> None:
        record.fly_outs.extend((signed(m.r[4]), signed(m.r[5])))

    machine.stub(SET_MENU_ROWS, set_menu_rows)
    machine.stub(GET_SELECTED_ROW, get_selected_row)
    machine.stub(CLEAR_MENU_EXCEPT, clear_menu_except)
    machine.stub(FLY_OUT_AND_HOIST, fly_out)
    return record


def rows_observation(machine: Machine, record: MenuRecord) -> list[int]:
    """The driver's Host::PrintRows fields."""
    return [machine.load(CURSOR_MEMORIES + 4, 1), machine.load(CURSOR_MEMORIES + 5, 1), record.creations,
            record.setting_while_creating, machine.load(CURSOR_MEMORY_SETTING, 1), record.count, record.info,
            len(record.offsets), *record.offsets, len(record.types), *record.types]


def selection_observation(record: MenuRecord) -> list[int]:
    """The driver's Host::PrintSelection fields."""
    return [len(record.cleared), *record.cleared, len(record.fly_outs), *record.fly_outs]


def untouched_rows(row: int, scroll: int, setting: int) -> list[int]:
    return [row, scroll, 0, 0, setting, 0, 0, 0, 0]


def any_byte(rng: random.Random) -> int:
    return rng.choice((rng.randrange(256), rng.randrange(0x80, 256), 0, 0xFF))


def any_int(rng: random.Random, low: int, high: int) -> int:
    """Mostly in [low, high], sometimes any word."""
    return rng.randrange(low, high + 1) if rng.randrange(8) else signed(rng.getrandbits(32))


# ---------------------------------------------------------------------------------------------- page text
def encoded_line(rng: random.Random, length: int, terminated: bool = True) -> list[int]:
    """Glyphs, one-byte codes and two-byte codes (whose operand may be a terminator)."""
    line: list[int] = []
    while len(line) < length:
        kind = rng.randrange(10)
        if kind < 6:
            line.append(rng.randrange(0, TERMINATOR))
        elif kind < 8:
            line.append(rng.randrange(NEWLINE, CONTROL))
        else:
            line.extend((rng.randrange(CONTROL, 256), rng.choice((TERMINATOR, rng.randrange(256)))))
    return line + [TERMINATOR] if terminated else line


def copied_length(line: list[int]) -> int:
    """The bytes _vsStrcpy writes for a terminated `line`, newline included (vector sizing only)."""
    index = written = 0
    while index < len(line) and line[index] != TERMINATOR:
        step = 2 if line[index] >= CONTROL else 1
        index += step
        written += step
    return written + 1


def run_original(machine: Machine, entry: int) -> bool:
    """Execute `entry`; True when the original faulted (read past its inputs or ran away)."""
    try:
        machine.execute(entry)
    except Fault:
        return True
    return False
