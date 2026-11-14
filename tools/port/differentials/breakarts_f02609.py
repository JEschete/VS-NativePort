"""F02609 Menu::BreakArts::DrawNextUnlockPoints, MENU1.PRG 30.c:45."""

from __future__ import annotations

import random

from differential import MODULE_BASES, Machine

MODULE_BASES["MENU/MENU1.PRG"] = 0x80102800

ID = "F02609"
ENTRY, END = 0x80102914, 0x80102A4C
MODULES = ("MENU/MENU1.PRG", "SLUS_010.40")
DRIVER = "breakarts_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\BreakArtsCore.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
VECTORS, SEED = 512, 2609
MUTANT_VECTORS = VECTORS
MUTANTS = {
    "learned-four-return-inverted": {0x8010296C: 0x16220031},
}

ARTS_STATUS = 0x80060040
POINT_REQUIREMENTS = 0x8004FDE4
TO_BCD = 0x800CCC54
RENDER_TEXT = 0x800C6828


def vector(rng: random.Random, number: int) -> list[int]:
    category = [0, 1, 5, 9, 10][number % 5] if number < 40 else rng.randrange(11)
    learned = number % 5 if number < 40 else rng.randrange(5)
    kills = rng.randrange(0, 401)
    requirement = rng.randrange(0, 401)
    return [1, rng.randrange(-128, 129), category, rng.randrange(6),
            learned, kills, requirement]


def run(machine: Machine, vector: list[int]) -> tuple[list[tuple], list[int]]:
    _, x_offset, weapon_category, row, learned, kills, requirement = vector
    category = weapon_category % 10
    machine.put(ARTS_STATUS + category, learned, 1)
    machine.put(ARTS_STATUS + 12 + category * 2, kills, 2)
    machine.put(POINT_REQUIREMENTS + category * 8 + learned * 2, requirement, 2)
    drawings = []

    def to_bcd(m: Machine) -> None:
        value = m.r[4]
        m.r[2] = value % 10 | (value // 10) * 16

    def render_text(m: Machine) -> None:
        text = bytearray()
        for offset in range(32):
            char = machine.load(m.r[4] + offset, 1)
            if char == 0:
                break
            text.append(char)
        else:
            raise ValueError("rendered text is unterminated")
        drawings.append((bytes(text).decode("ascii"), m.r[5]))

    machine.stub(TO_BCD, to_bcd)
    machine.stub(RENDER_TEXT, render_text)
    machine.r[4], machine.r[5], machine.r[6] = x_offset, weapon_category, row
    machine.execute(ENTRY)
    return drawings, vector


def check(image, vector: list[int], observation: list[tuple], native: list[int],
          reference: list[tuple]) -> list[str]:
    expected: list[int] = [len(observation)]
    for text, position in observation:
        encoded = text.encode("ascii")
        expected.extend((position, len(encoded), *encoded))
    return [] if native == expected else [f"unlock text native {native} original {expected}"]
