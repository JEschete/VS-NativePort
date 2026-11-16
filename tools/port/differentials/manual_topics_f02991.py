"""F02991 Menu::manual::UpdateTopicMenu, MENUE.PRG _topMenu 494.c:450."""

from __future__ import annotations

import random

from differential import Machine
from manual_topics_common import (
    CURRENT_MANUAL,
    CXX_STANDARD,
    DRIVER,
    GET_SELECTED_TOPIC_ROW,
    INCLUDES,
    MODULES,
    OP_UPDATE_MENU,
    SELECTED_ROW,
    SET_TOPIC_ROWS,
    SOURCES,
    TERMINATOR,
    TOP_MENU_STATE,
    TOPIC_OUTSIDE_TABLE,
    TOPIC_TABLE,
    TOPIC_TABLE_BYTES,
    UNTERMINATED_TOPIC,
    any_byte,
    any_int,
    compare,
    install_menu,
    mutants,
    rows_observation,
    run_original,
    selection_observation,
    signed,
    untouched_rows,
    verify_original_words,
)

__all__ = ("CXX_STANDARD", "DRIVER", "INCLUDES", "MODULES", "SOURCES")

ID = "F02991"
ENTRY, END = 0x8010391C, 0x80103A24
VECTORS, SEED = 512, 2991
MUTANTS = mutants(ID)
TOPIC_STRINGS = 28
# The halfword after the last terminator in the image's table: strings from here are unterminated.
UNTERMINATED_FROM = 489


def vector(rng: random.Random, number: int) -> list[int]:
    """[init, step, selected row, current manual, next manual halfword, framework selected row,
    cursor row, cursor scroll, cursor memory setting, patched entries, (entry, value)...]."""
    init = rng.choice((0, 1))
    step = rng.choice((0, 0, 1, 1, 1, 2, -1, signed(rng.getrandbits(32))))
    current = rng.choice((0, 5, 8, 13, rng.randrange(14), rng.randrange(14), rng.randrange(0x10000)))
    next_row = rng.choice((-1, -1, -2, 0, 3, 13, any_int(rng, -3, 13)))
    patches: list[int] = []
    for _ in range(rng.choice((0, 0, 0, 1, 1, 2))):
        value = rng.choice((rng.randrange(TOPIC_STRINGS, UNTERMINATED_FROM), rng.randrange(UNTERMINATED_FROM, 516),
                            rng.randrange(516, 0x10000)))
        patches.extend((rng.randrange(TOPIC_STRINGS), value))
    return [init, step, any_int(rng, -2, 13), current, rng.randrange(0x10000), next_row, any_byte(rng),
            any_byte(rng), any_byte(rng), len(patches) // 2, *patches]


def run(machine: Machine, vector: list[int]) -> tuple[list[int], list[int]]:
    init, step, selected, current, neighbour, next_row, row, scroll, setting, _count, *patches = vector
    record = install_menu(machine, row, scroll, setting, next_row)
    machine.map_zero(CURRENT_MANUAL & ~0x3F, 0x40)
    machine.put(CURRENT_MANUAL, current, 2)
    machine.put(CURRENT_MANUAL + 2, neighbour, 2)
    machine.put(TOP_MENU_STATE, step)
    machine.put(SELECTED_ROW, selected)
    for index in range(0, len(patches), 2):
        machine.put(TOPIC_TABLE + patches[index] * 2, patches[index + 1], 2)
    machine.follow.update((SET_TOPIC_ROWS, GET_SELECTED_TOPIC_ROW))
    machine.r[4] = init
    faulted = run_original(machine, ENTRY)
    observation = [int(faulted), 0, signed(machine.r[2]), machine.load(TOP_MENU_STATE, 4, True),
                   machine.load(SELECTED_ROW, 4, True), *rows_observation(machine, record),
                   *selection_observation(record)]
    inputs = [OP_UPDATE_MENU, init, step, selected, current, next_row, row, scroll, setting, len(patches), *patches]
    return observation, inputs


def table_problem(image, patches: list[int]) -> int:
    """The native refusal for the patched table, or 0 when every topic string ends in it."""
    table = bytearray(image.data[TOPIC_TABLE - image.base:TOPIC_TABLE - image.base + TOPIC_TABLE_BYTES])
    for index in range(0, len(patches), 2):
        table[patches[index] * 2:patches[index] * 2 + 2] = patches[index + 1].to_bytes(2, "little")
    for topic in range(TOPIC_STRINGS // 2):
        for entry in (topic, TOPIC_STRINGS // 2 + topic):
            start = int.from_bytes(table[entry * 2:entry * 2 + 2], "little") * 2
            if start >= len(table):
                return TOPIC_OUTSIDE_TABLE
            if TERMINATOR not in table[start:]:
                return UNTERMINATED_TOPIC
    return 0


def check(image, vector: list[int], observation: list[int], native: list[int],
          reference: list[int]) -> list[str]:
    init, _step, selected, _current, _neighbour, _next_row, row, scroll, setting, _count, *patches = vector
    if observation[0]:
        return ["the original faulted"]
    problem = table_problem(image, patches)
    if problem and reference[7] == 1:
        # The original lists strings past its table; the native code refuses before advancing.
        return compare(native, [problem, 0, 0, 0 if init else selected, *untouched_rows(row, scroll, setting),
                                0, 0])
    return compare(native, observation[1:])


def describe(image, results: list) -> list[str]:
    verify_original_words(ID, image)
    listed = sum(1 for _, observation, _ in results if observation[7] == 1)
    refused = sum(1 for vector, observation, _ in results
                  if observation[7] == 1 and table_problem(image, vector[10:]))
    polled = sum(1 for vector, _, _ in results if vector[0] == 0 and vector[1] == 1)
    return [f"coverage: {listed} vectors listed the topics ({refused} from refused tables), {polled} polled"]
