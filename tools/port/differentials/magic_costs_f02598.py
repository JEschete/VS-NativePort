"""F02598 Menu::Magic::UpdateTeleportMenu, MENU0.PRG _teleportMenu 15AC.c:22."""

from __future__ import annotations

import random

from differential import Machine
from magic_costs_common import (
    ACTION_RESTRICTIONS,
    CHARACTER,
    CHARACTER_POINTER,
    CHARACTER_STATE,
    CLEAR_MENU_EXCEPT,
    COMPILER,
    COMPILER_OPTIONS,
    CURSOR_MEMORY_SETTING,
    CXX_STANDARD,
    DISABLED_DESCRIPTION_OFFSET,
    DISMISS_INFORMATION_BOX,
    DISPLAY_ACTION_COST,
    DRIVER,
    EVENT_CLEAR_MENU_EXCEPT,
    EVENT_DISMISS_INFORMATION_BOX,
    EVENT_FLY_OUT,
    EVENT_SET_MENU_COMMAND,
    EVENT_SET_MENU_ROWS,
    FLY_OUT,
    GET_CURRENT_ROOM_ID,
    HIGHLIGHTED_ROW,
    INCLUDES,
    LOCATIONS_INDEX,
    MODULES,
    ROWS_IDLE,
    SAVE_POINT_COUNT,
    SAVE_POINT_STATES,
    SELECTED_ROW,
    SET_MENU_COMMAND,
    SET_MENU_ROWS,
    SET_TELEPORT_MP_COST,
    SOURCES,
    TELEPORT_COSTS,
    TELEPORT_CURRENT_MP,
    TELEPORT_CURSOR_ROW,
    TELEPORT_CURSOR_SCROLL,
    TELEPORT_MP_COST,
    TELEPORT_SAVE_POINTS,
    TELEPORT_SELECTED_ROW,
    TELEPORT_STEP,
    TELEPORTATION_STRINGS,
    compare,
    flatten,
    install_cost_text,
    recorder,
    signed,
)

__all__ = ("COMPILER", "COMPILER_OPTIONS", "CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES",
           "SOURCES")

ID = "F02598"
ENTRY, END = 0x80103E30, 0x80104254
VECTORS, SEED = 1024, 2598
MUTANT_VECTORS = VECTORS
MUTANTS = {
    "disabled-state": {0x80103F74: 0x3A430001},
    "current-save-point-cost": {0x80103F98: 0x24020013},
    "restriction-mask": {0x80104000: 0x304200B6},
    "cursor-centered-row": {0x80104090: 0x24030004},
    "selection-offset": {0x801040EC: 0x24440002},
    "back-keeps-header": {0x80104110: 0x24040001},
    "packed-tile-z": {0x801041FC: 0x000423C0},
    "packed-cost": {0x8010420C: 0x00031C80},
}

# Rooms of the original save points, so one row is usually the current save point.
SAVE_POINT_ROOMS = (0x1B, 0x0E, 0x199, 0x1C, 0x28, 0xCD, 0x35, 0x93, 0xCE, 0x106, 0x111, 0x98,
                    0xE0, 0xC8, 0xCA, 0xCC, 0x9B, 0x7C, 0xCF, 0xEC, 0xF1, 0x11D, 0x120, 0x139,
                    0x147, 0x14F, 0xA2, 0xA3, 0xD0, 0xD1, 0x8B, 0xAB, 0x55, 0x63, 0xD2)
STRING_LIMIT = LOCATIONS_INDEX + 2 * SAVE_POINT_COUNT


def vector(rng: random.Random, number: int) -> list[int]:
    """[init, step, selected row, current MP, save points[48], MP costs[48], save-point states[48],
    room id, character MP, restrictions, cursor-memory setting, cursor row, cursor scroll,
    display-action-cost flag, rows idle, selected row result, highlighted row result]."""
    mode = number % 4
    init = rng.choice((1, 1, 2, -1)) if mode == 0 else 0
    step = max(mode - 1, 0)
    save_points = [rng.randrange(SAVE_POINT_COUNT) for _ in range(SAVE_POINT_COUNT)]
    costs = [rng.choice((15, 19, 23, rng.randrange(15, 300), rng.randrange(0x10000)))
             for _ in range(SAVE_POINT_COUNT)]
    if number % 64 == 5:
        states = [0] * SAVE_POINT_COUNT
    else:
        density = rng.choice((1, 3, 7, 15))
        states = [rng.choice((0, 1, 1, 2, 2, 3)) if rng.randrange(16) < density + 1 else 0
                  for _ in range(SAVE_POINT_COUNT)]
        if rng.randrange(2):
            states = [state or 1 for state in states]
    room = rng.choice(SAVE_POINT_ROOMS) if rng.randrange(6) else rng.randrange(0x400)
    selected = rng.choice((-2, -1, 0, 1, rng.randrange(-2, SAVE_POINT_COUNT + 1)))
    selected_result = rng.choice((-1, -1, -1, -2, -3, rng.randrange(SAVE_POINT_COUNT)))
    return [init, step, selected, rng.randrange(-5, 300), *save_points, *costs, *states, room,
            rng.randrange(-0x8000, 0x8000), rng.choice((0, 0, rng.getrandbits(8), 0x48, 0x01)),
            rng.randrange(256), rng.randrange(256), rng.randrange(256), rng.randrange(256),
            int(rng.randrange(8) != 0), selected_result, rng.randrange(SAVE_POINT_COUNT)]


def split(vector: list[int]) -> tuple:
    init, step, selected, current_mp = vector[:4]
    save_points = vector[4:4 + SAVE_POINT_COUNT]
    costs = vector[4 + SAVE_POINT_COUNT:4 + 2 * SAVE_POINT_COUNT]
    states = vector[4 + 2 * SAVE_POINT_COUNT:4 + 3 * SAVE_POINT_COUNT]
    return init, step, selected, current_mp, save_points, costs, states, \
        vector[4 + 3 * SAVE_POINT_COUNT:]


def teleportation_strings(machine: Machine) -> list[int]:
    head = [machine.load(TELEPORTATION_STRINGS + index * 2, 2) for index in range(STRING_LIMIT)]
    count = max(*head, DISABLED_DESCRIPTION_OFFSET) + 1
    return [machine.load(TELEPORTATION_STRINGS + index * 2, 2) for index in range(count)]


def string_offset(pointer: int) -> int:
    offset = pointer - TELEPORTATION_STRINGS
    if offset < 0 or offset & 1:
        raise ValueError(f"row string pointer {pointer:#x} is outside the teleportation strings")
    return offset // 2


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    init, step, selected, current_mp, save_points, costs, states, rest = split(vector)
    (room, character_mp, restrictions, setting, cursor_row, cursor_scroll, display_cost,
     rows_idle, selected_result, highlighted) = rest
    machine.put(TELEPORT_STEP, step, 1)
    machine.put(TELEPORT_SELECTED_ROW, selected)
    machine.put(TELEPORT_CURRENT_MP, current_mp)
    for index in range(SAVE_POINT_COUNT):
        machine.put(TELEPORT_SAVE_POINTS + index, save_points[index], 1)
        machine.put(TELEPORT_COSTS + index * 2, costs[index], 2)
        machine.put(SAVE_POINT_STATES + index, states[index], 1)
    machine.put(CHARACTER_POINTER, CHARACTER)
    machine.put(CHARACTER + 0x3C, CHARACTER_STATE)
    machine.put(CHARACTER_STATE + 0x1C, character_mp, 2)
    machine.put(ACTION_RESTRICTIONS, restrictions)
    machine.put(CURSOR_MEMORY_SETTING, setting, 1)
    machine.put(TELEPORT_CURSOR_ROW, cursor_row, 1)
    machine.put(TELEPORT_CURSOR_SCROLL, cursor_scroll, 1)
    machine.put(DISPLAY_ACTION_COST, display_cost, 1)
    strings = teleportation_strings(machine)
    events: list[list[int]] = []

    def set_rows(m: Machine) -> None:
        count, rows, types = signed(m.r[4]), m.r[6], m.r[7]
        event = [EVENT_SET_MENU_ROWS, count, signed(m.r[5]), m.load(CURSOR_MEMORY_SETTING, 1)]
        for index in range(count):
            event.extend((string_offset(m.load(rows + index * 8)),
                          string_offset(m.load(rows + index * 8 + 4)),
                          m.load(types + index * 4, 4, True)))
        events.append(event)

    def returning(value: int):
        def handler(m: Machine) -> None:
            m.r[2] = value & 0xFFFFFFFF
        return handler

    machine.stub(FLY_OUT, recorder(events, EVENT_FLY_OUT, 2))
    machine.stub(ROWS_IDLE, returning(rows_idle))
    machine.stub(GET_CURRENT_ROOM_ID, returning(room))
    machine.stub(SET_MENU_ROWS, set_rows)
    machine.stub(SELECTED_ROW, returning(selected_result))
    machine.stub(HIGHLIGHTED_ROW, returning(highlighted))
    machine.stub(CLEAR_MENU_EXCEPT, recorder(events, EVENT_CLEAR_MENU_EXCEPT, 1))
    machine.stub(DISMISS_INFORMATION_BOX, recorder(events, EVENT_DISMISS_INFORMATION_BOX, 0))
    machine.stub(SET_MENU_COMMAND, recorder(events, EVENT_SET_MENU_COMMAND, 1))
    install_cost_text(machine, events)
    machine.follow.update((TELEPORT_MP_COST, SET_TELEPORT_MP_COST))
    machine.r[4] = init & 0xFFFFFFFF
    machine.execute(ENTRY)
    observation = [
        signed(machine.r[2]),
        machine.load(TELEPORT_STEP, 1),
        machine.load(TELEPORT_SELECTED_ROW, 4, True),
        machine.load(TELEPORT_CURRENT_MP, 4, True),
        *(machine.load(TELEPORT_SAVE_POINTS + index, 1) for index in range(SAVE_POINT_COUNT)),
        *(machine.load(TELEPORT_COSTS + index * 2, 2) for index in range(SAVE_POINT_COUNT)),
        machine.load(TELEPORT_CURSOR_ROW, 1),
        machine.load(TELEPORT_CURSOR_SCROLL, 1),
        machine.load(CURSOR_MEMORY_SETTING, 1),
        machine.load(DISPLAY_ACTION_COST, 1),
        *flatten(events),
    ]
    return observation, [4, *vector, len(strings), *strings]


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    return compare(native, [0, *observation])
