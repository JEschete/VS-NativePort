"""F02608 Menu::BreakArts::SetHpCost, MENU1.PRG 30.c:13."""

from __future__ import annotations

import random

from differential import MODULE_BASES, Machine

MODULE_BASES["MENU/MENU1.PRG"] = 0x80102800

ID = "F02608"
ENTRY, END = 0x80102830, 0x80102914
MODULES = ("MENU/MENU1.PRG", "SLUS_010.40")
DRIVER = "breakarts_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\BreakArtsCore.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
VECTORS, SEED = 512, 2608
MUTANT_VECTORS = VECTORS
MUTANTS = {
    "decimal-loop-skipped": {0x801028C8: 0x1080FFF8},
}

ACTION_DEFINITIONS = 0x8004B9DC
ACTION_DEFINITION_SIZE = 52
ACTION_COST_OFFSET = 3
GET_ACTION_FLAGS = 0x8008A5D0
TO_BCD = 0x800CCC54
SET_ACTION_COST = 0x80100100


def vector(rng: random.Random, number: int) -> list[int]:
    edge_costs = [0, 1, 9, 10, 99, 100, 255]
    cost = edge_costs[number % len(edge_costs)] if number < 64 else rng.randrange(256)
    return [0, rng.randrange(256), cost, rng.getrandbits(16)]


def run(machine: Machine, vector: list[int]) -> tuple[list[tuple], list[int]]:
    _, action_id, cost, flags = vector
    machine.put(
        ACTION_DEFINITIONS + action_id * ACTION_DEFINITION_SIZE + ACTION_COST_OFFSET,
        cost,
        1,
    )
    displays = []

    def get_flags(m: Machine) -> None:
        if m.r[4] != 0 or m.r[5] != action_id:
            raise ValueError(f"unexpected action-flags arguments {m.r[4:6]}")
        m.r[2] = flags

    def to_bcd(m: Machine) -> None:
        value = m.r[4] & 0xFFFFFFFF
        m.r[2] = value % 10 | (value // 10) * 16

    def set_action_cost(m: Machine) -> None:
        text = bytearray()
        for offset in range(32):
            char = machine.load(m.r[5] + offset, 1)
            if char == 0:
                break
            text.append(char)
        else:
            raise ValueError("action-cost text is unterminated")
        displays.append((m.r[4], m.r[6], m.r[7], bytes(text).decode("ascii")))

    machine.stub(GET_ACTION_FLAGS, get_flags)
    machine.stub(TO_BCD, to_bcd)
    machine.stub(SET_ACTION_COST, set_action_cost)
    machine.r[4] = action_id
    machine.execute(ENTRY)
    return displays, vector


def check(image, vector: list[int], observation: list[tuple], native: list[int],
          reference: list[tuple]) -> list[str]:
    expected: list[int] = [len(observation)]
    for index, x, disabled, text in observation:
        encoded = text.encode("ascii")
        expected.extend((index, x, disabled, len(encoded), *encoded))
    return [] if native == expected else [f"cost output native {native} original {expected}"]
