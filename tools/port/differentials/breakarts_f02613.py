"""F02613 Menu::BreakArts::Update state transitions, MENU1.PRG 30.c:358."""

from __future__ import annotations

import random

from differential import MODULE_BASES, Machine

MODULE_BASES["MENU/MENU1.PRG"] = 0x80102800

ID = "F02613"
ENTRY, END = 0x80103270, 0x80103478
MODULES = ("MENU/MENU1.PRG", "SLUS_010.40")
DRIVER = "breakarts_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\BreakArtsCore.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
COMPILER = "clang-cl"
CXX_STANDARD = "c++latest"
VECTORS, SEED = 512, 2613
MUTANTS = {
    "main-menu-ready-branch-inverted": {0x801033E4: 0x1440001F},
    "exit-command-changed": {0x801033D8: 0x24040002},
}

READY = 0x800FA9D0
DISMISS_INFORMATION = 0x800FFBA8
SET_COMMAND = 0x800FFA88
INITIALIZE_INFORMATION = 0x800FFBC8
MENU_STATE_POINTER = 0x800F19FC
MENU_STATE = 0x800F1000
CURRENT_MENU = 0x800F51C0
RETURN_STATE = 0x800F51C1
SELECTION_KIND = 0x800F4E9A
SCREEN_STEP = 0x800F1100

PATHS = (
    (0, 0),
    (3, 0),
    (8, 0),
    (8, 1),
    (10, 0),
    (10, 1),
    (9, 0),
    (9, 1),
)


def vector(rng: random.Random, number: int) -> list[int]:
    step, ready = PATHS[number % len(PATHS)]
    return [step, ready, 1, 7]


def _event(code: int, *values: int) -> list[int]:
    return [code, len(values), *values]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    initial_step, ready, equipped, current_menu = vector
    machine.map_zero(MENU_STATE, 0x100)
    machine.map_zero(0x800F4E00, 0x400)
    machine.put(SCREEN_STEP, initial_step, 1)
    machine.put(MENU_STATE_POINTER, MENU_STATE)
    machine.put(MENU_STATE + 0x25, current_menu, 1)
    machine.put(CURRENT_MENU, current_menu, 1)
    events: list[list[int]] = []

    def mainmenu_ready(m: Machine) -> None:
        events.append(_event(4, ready))
        m.r[2] = ready

    def dismiss(m: Machine) -> None:
        events.append(_event(12))

    def set_command(m: Machine) -> None:
        events.append(_event(2, m.r[4]))

    def initialize_information(m: Machine) -> None:
        events.append(_event(3))

    machine.stub(READY, mainmenu_ready)
    machine.stub(DISMISS_INFORMATION, dismiss)
    machine.stub(SET_COMMAND, set_command)
    machine.stub(INITIALIZE_INFORMATION, initialize_information)
    machine.r[4] = SCREEN_STEP
    machine.execute(ENTRY)

    if initial_step == 9 and ready:
        events.extend(
            [
                _event(11, machine.load(SELECTION_KIND, 2, signed=True)),
                _event(8, current_menu),
                _event(10, machine.load(RETURN_STATE, 1)),
                _event(9, machine.load(CURRENT_MENU, 1)),
            ]
        )
    elif initial_step == 10 and ready:
        events.append(_event(9, machine.load(CURRENT_MENU, 1)))

    observation = [
        machine.r[2],
        machine.load(SCREEN_STEP, 1),
        len(events),
        *(value for event in events for value in event),
    ]
    return observation, [5, initial_step, ready, equipped, current_menu]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    return [] if native == observation else [
        f"screen update native {native} original {observation}"
    ]
