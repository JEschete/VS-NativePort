"""Shared harness for the status screen's actor switching and vitals differentials (MENU4.PRG 120.c:528-843
and 1114-1142, #276).

Each case runs one original under the R3000 interpreter against its typed VS::Menu::Status core
(ActorSwitching.cpp) in status_switching_driver.cpp. Every vector draws the MENU4.PRG globals from
_previousActor (0x801080A4) to _switchCurrentActor's state word (0x801080C0), the HUD's slide offset, the
frame scratch's ordering-table word, the HUD label tables, four argument words, the words
vs_battle_renderValue returns and 15 battle actor slots (present, state flags, kind, HP/MP/RISK, the
analyzed word at stats+0x954, six limb name indices and the accessory id). The stubs record every call as
seven words (kind, four argument registers and two extras: the word stored into a returned sprite, or the
menu row's background width and the stats record its name came from); the driver's hosts record the same.
The driver prints the result, every global, the name row's selected byte and the calls.

Vectors keep to the domain the original reads safely: the slots it dereferences hold actors, and
_getNextValidActor's vectors reach an inspectable actor without a negative remainder. Outside it (covered by
StatusActorSwitchingTests.cpp) the core refuses a negative candidate and a cycle with no inspectable
actor, and the Live adapter refuses missing actors and stats records.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    MODULE_BASES,
    Machine,
    R,
    mode_case,
    recorded_observation,
    s32,
)

MODULE_BASES["MENU/MENU4.PRG"] = 0x80102800
MODULES = ("MENU/MENU4.PRG",)
DRIVER = "status_switching_driver.cpp"
SOURCES = (r"Native\Game\src\Menu\Status\ActorSwitching.cpp",)

INIT, NEXT, SWITCH, UNKNOWN, BAR, VITALS, LIMBS, ROWS = range(8)

GLOBALS = 0x801080A4
PREVIOUS_ACTOR = 0x801080A4
SELECTED_ACTOR = 0x801080A8
DRAW_BACKGROUND_FIRST = 0x801080AC
FADE_SCREEN = 0x801080B0
ANIMATION_STEP = 0x801080B4
VITALS_STATE = 0x801080B8
VITALS_SLIDE = 0x801080B9
VITALS_DELAY = 0x801080BA
SHOWN_ACTOR = 0x801080BB
DELAY_SCREEN_UPDATE = 0x801080BC
SWITCH_PHASE = 0x801080C0
GLOBALS_END = 0x801080C4

BATTLE_ACTORS = 0x800F1928
BATTLE_ACTOR_ENTRIES = 42
HUD_SLIDE_OFFSET = 0x800EB9B0
LABEL_POSITIONS = 0x800EBBEC
LABEL_WIDTHS = 0x800EBBFC
LABEL_TEXTURE_U = 0x800EBC00
LABEL_TABLE_BYTES = 0x28
SCRATCH = 0x1F800000
ORDERING_TABLE = SCRATCH + 4

SLOTS = 15
LIMBS_PER_ACTOR = 6
ACTORS = 0x80500000
ACTOR_STRIDE = 0x100
KIND_OFFSET = 0x1C
STATS_POINTER_OFFSET = 0x3C
STATS = 0x80600000
STATS_STRIDE = 0x1000
VITALS_OFFSET = 0x18
ANALYZED_WORD_OFFSET = 0x954
LIMB_NAME_OFFSET = 0x39E
LIMB_STRIDE = 0xDC
ACCESSORY_OFFSET = 0x338
ANALYZED_BIT = 1 << 17

SPRITES = 0x80700000
SPRITE_STRIDE = 0x20
SPRITE_AREA = 0x1000
MENU_ITEM = 0x80710000
MENU_ITEM_BYTES = 0x40
MENU_SELECTED = MENU_ITEM + 6
VALUE_RETURNS = 8

HUD_ADDRESS = 0x800CB654
FLAGS_ADDRESS = 0x800A0BE0
SOUND_ADDRESS = 0x800C02D8
MENU_ITEM_ADDRESS = 0x800C8E5C
RELEASE_ADDRESS = 0x800F9E0C
LOAD_ADDRESS = 0x800F9A24
SPRITE_ADDRESS = 0x800C0230
LABEL_ADDRESS = 0x800C0224
VALUE_ADDRESS = 0x800C9950
GAUGE_ADDRESS = 0x800C99DC
UNKNOWN_ADDRESS = 0x8010399C
BAR_ADDRESS = 0x80103A6C
LIMBS_ADDRESS = 0x80104514

# The kinds the driver prints for each recorded call.
CALL_HUD, CALL_FLAGS, CALL_SOUND, CALL_MENU_ITEM, CALL_RELEASE, CALL_LOAD, CALL_SPRITE, CALL_LABEL, \
    CALL_VALUE, CALL_GAUGE = range(1, 11)

INSPECTABLE_MASK = 0x01000001
SIGNED_WORDS = (0, 1, -1, 2, 0x7FFF, 0x8000, 0x7FFFFFFF, -0x80000000, 0x10000, -0x10000)
HALVES = (0, 1, -1, 2, 100, 0x7FFF, -0x8000, 999)
STEPS = (-0xC0, -0xA0, -0x9F, -0x80, -0x20, -1, 0, 1, 0x20, 0x80, 0x9F, 0xA0, 0xA1, 0xC0)
HUD_OFFSETS = (0x200000, 0x200000, 0, 0x1FFFFF, 0x200001)


def word(rng: random.Random) -> int:
    return rng.choice((*SIGNED_WORDS, s32(rng.randrange(1 << 32))))


def half(rng: random.Random) -> int:
    return rng.choice((*HALVES, rng.randrange(-0x8000, 0x8000)))


def small_byte(rng: random.Random, low: int, high: int) -> int:
    return rng.choice((rng.randrange(low, high + 1), rng.randrange(low, high + 1), rng.randrange(256)))


def remainder(dividend: int) -> int:
    """C's truncating `% 15` on a 32-bit dividend."""
    dividend = s32(dividend)
    quotient = abs(dividend) // SLOTS
    return dividend - (quotient if dividend >= 0 else -quotient) * SLOTS


def inspectable(slot: dict) -> bool:
    return slot["present"] and slot["kind"] < 5 and slot["flags"] & INSPECTABLE_MASK == 1


def search_ends(value: dict) -> bool:
    """Whether _getNextValidActor reaches an inspectable actor without a negative remainder."""
    current, step = value["arguments"][:2]
    for _ in range(SLOTS):
        current = remainder(current + step)
        if current < 0:
            return False
        if inspectable(value["slots"][current]):
            return True
    return False


def slot_vector(rng: random.Random) -> dict:
    limbs = [rng.choice((0, rng.randrange(1, 256), rng.randrange(1, 256))) for _ in range(LIMBS_PER_ACTOR)]
    if rng.random() < 0.3:
        limbs = [rng.randrange(1, 256) for _ in range(LIMBS_PER_ACTOR)]
    return {
        "present": rng.random() < 0.75,
        "flags": rng.choice((1, 1, 0x01000001, 0, 3, 0x00800001, s32(rng.randrange(1 << 32)) & 0xFFFFFFFF)),
        "kind": rng.choice((0, 1, 4, 5, 6, 0x8000, rng.randrange(0x10000))),
        "vitals": [half(rng) for _ in range(5)],
        "analyzed": rng.choice((0, ANALYZED_BIT, rng.randrange(1 << 32))),
        "limbs": limbs,
        "accessory": rng.choice((0, 0, 1, 0x8000, rng.randrange(0x10000))),
    }


def vector(rng: random.Random, number: int, mode: int) -> dict:
    value = {
        "mode": mode,
        "globals": [rng.randrange(1, SLOTS + 1), rng.randrange(1, SLOTS + 1), word(rng), word(rng),
                    rng.choice((*STEPS, word(rng)))],
        "bytes": [small_byte(rng, 0, 5), small_byte(rng, 0, 5), small_byte(rng, 0, 5), small_byte(rng, 0, 3),
                  rng.randrange(256)],
        "phase": rng.choice((0, 1, 2, 3, 4, 5, 6, 7, rng.randrange(7), rng.randrange(1 << 32))),
        "hud": rng.choice((*HUD_OFFSETS, word(rng))),
        "ordering_table": rng.choice((0x80100000, rng.randrange(1 << 32))),
        "label_positions": [word(rng) for _ in range(3)],
        "label_widths": [rng.randrange(256) for _ in range(3)],
        "label_texture_u": [rng.randrange(256) for _ in range(3)],
        "arguments": [word(rng) for _ in range(4)],
        "values": [word(rng) for _ in range(VALUE_RETURNS)],
        "slots": [slot_vector(rng) for _ in range(SLOTS)],
    }
    arguments = value["arguments"]
    if mode == INIT:
        arguments[0] = rng.choice((0, 0, 1, 15, 0x100, rng.randrange(1, SLOTS + 1), word(rng)))
    elif mode == NEXT:
        while True:
            arguments[0] = rng.choice((rng.randrange(SLOTS), rng.randrange(-20, 40), word(rng)))
            arguments[1] = rng.choice((1, 14, -1, 2, rng.randrange(-30, 30), word(rng)))
            if search_ends(value):
                break
            target = remainder(arguments[0] + arguments[1])
            if target >= 0 and rng.random() < 0.5:
                value["slots"][target].update(present=True, kind=rng.randrange(5),
                                              flags=1 | (rng.randrange(1 << 32) & ~INSPECTABLE_MASK))
            else:
                value["slots"] = [slot_vector(rng) for _ in range(SLOTS)]
    elif mode == SWITCH:
        if rng.random() < 0.4:
            arguments[0] = rng.randrange(1, SLOTS + 1) | rng.choice((0, 0x10, 0x10, rng.randrange(1 << 32) & ~0xF))
        else:
            arguments[0] = 0
        arguments[0] = s32(arguments[0])
        value["globals"][1] = rng.choice((0, 0, rng.randrange(1, SLOTS + 1), word(rng)))
        if arguments[0] & 0xF:
            value["slots"][(arguments[0] & 0xF) - 1]["present"] = True
    elif mode == UNKNOWN:
        arguments[1] = rng.choice((0, 1, word(rng)))
    elif mode == BAR:
        arguments[0] = rng.choice((0, 1, 2, 0x100, 0x101, 0x102, 0x1FF, word(rng)))
        arguments[2] = rng.choice((0, 0, 1, -1, 100, 0x7FFF, word(rng)))
        arguments[1] = rng.choice((0, 1, -1, 100, 0x7FFF, arguments[2], word(rng)))
    elif mode == LIMBS:
        arguments[0] = rng.randrange(SLOTS)
    if mode == VITALS:
        value["bytes"][:3] = [rng.choice((0, 1, 1, 2, 2, 3, 3, 4, 4, rng.randrange(256))),
                              rng.choice((0, 0, 1, 3, 4, 4, 5, rng.randrange(256))),
                              rng.choice((0, 0, 1, 4, rng.randrange(256)))]
        for actor in value["globals"][:2]:
            value["slots"][actor - 1]["present"] = True
    if mode in (LIMBS, ):
        value["slots"][arguments[0]]["present"] = True
    if mode == ROWS:
        value["slots"][value["globals"][1] - 1]["present"] = True
    return value


def place(machine: Machine, value: dict) -> None:
    machine.map_zero(BATTLE_ACTORS, 4 * BATTLE_ACTOR_ENTRIES)
    machine.map_zero(HUD_SLIDE_OFFSET, 4)
    machine.map_zero(LABEL_POSITIONS, LABEL_TABLE_BYTES)
    machine.map_zero(SCRATCH, 0x10)
    machine.map_zero(ACTORS, SLOTS * ACTOR_STRIDE)
    machine.map_zero(STATS, SLOTS * STATS_STRIDE)
    machine.map_zero(SPRITES, SPRITE_AREA)
    machine.map_zero(MENU_ITEM, MENU_ITEM_BYTES)
    for address, field in zip((PREVIOUS_ACTOR, SELECTED_ACTOR, DRAW_BACKGROUND_FIRST, FADE_SCREEN,
                               ANIMATION_STEP), value["globals"], strict=True):
        machine.put(address, field)
    machine.store(VITALS_STATE, bytes(value["bytes"]))
    machine.put(SWITCH_PHASE, value["phase"])
    machine.put(HUD_SLIDE_OFFSET, value["hud"])
    machine.put(ORDERING_TABLE, value["ordering_table"])
    for index in range(3):
        machine.put(LABEL_POSITIONS + 4 * index, value["label_positions"][index])
        machine.put(LABEL_WIDTHS + index, value["label_widths"][index], 1)
        machine.put(LABEL_TEXTURE_U + index, value["label_texture_u"][index], 1)
    for index, slot in enumerate(value["slots"]):
        actor = ACTORS + index * ACTOR_STRIDE
        stats = STATS + index * STATS_STRIDE
        machine.put(BATTLE_ACTORS + 4 * index, actor if slot["present"] else 0)
        machine.put(actor + KIND_OFFSET, slot["kind"], 2)
        machine.put(actor + STATS_POINTER_OFFSET, stats)
        for field, half_word in enumerate(slot["vitals"]):
            machine.put(stats + VITALS_OFFSET + 2 * field, half_word, 2)
        machine.put(stats + ANALYZED_WORD_OFFSET, slot["analyzed"])
        for limb, name in enumerate(slot["limbs"]):
            machine.put(stats + LIMB_NAME_OFFSET + limb * LIMB_STRIDE, name, 1)
        machine.put(stats + ACCESSORY_OFFSET, slot["accessory"], 2)


def stats_slot(address: int) -> int:
    index, rest = divmod(address - STATS, STATS_STRIDE)
    return index if rest == 0 and 0 <= index < SLOTS else -1


def recorder(machine: Machine, value: dict, calls: list[list[int]], sprites: list[tuple[int, int]]) -> None:
    returns = list(value["values"])

    def argument(index: int) -> int:
        return s32(machine.r[R["a0"] + index])

    def record(kind: int, count: int):
        def stub(machine: Machine) -> None:
            calls.append([kind, *(argument(index) for index in range(count)), *([0] * (6 - count))])

        return stub

    def flags(machine: Machine) -> None:
        slot = argument(0)
        calls.append([CALL_FLAGS, slot, 0, 0, 0, 0, 0])
        machine.r[2] = value["slots"][slot]["flags"] & 0xFFFFFFFF if 0 <= slot < SLOTS else 0

    def sprite(kind: int):
        def stub(machine: Machine) -> None:
            address = SPRITES + len(sprites) * SPRITE_STRIDE
            sprites.append((len(calls), address))
            calls.append([kind, *(argument(index) for index in range(4)), 0, 0])
            machine.r[2] = address

        return stub

    def render_value(machine: Machine) -> None:
        calls.append([CALL_VALUE, *(argument(index) for index in range(4)), 0, 0])
        machine.r[2] = (returns.pop(0) if returns else 0) & 0xFFFFFFFF

    def menu_item(machine: Machine) -> None:
        sp = machine.r[R["sp"]]
        calls.append([CALL_MENU_ITEM, *(argument(index) for index in range(4)),
                      machine.load(sp + 0x10, signed=True), stats_slot(machine.load(sp + 0x14))])
        machine.r[2] = MENU_ITEM

    machine.stub(HUD_ADDRESS, record(CALL_HUD, 1))
    machine.stub(FLAGS_ADDRESS, flags)
    machine.stub(SOUND_ADDRESS, record(CALL_SOUND, 0))
    machine.stub(MENU_ITEM_ADDRESS, menu_item)
    machine.stub(RELEASE_ADDRESS, record(CALL_RELEASE, 0))
    machine.stub(LOAD_ADDRESS, record(CALL_LOAD, 1))
    machine.stub(SPRITE_ADDRESS, sprite(CALL_SPRITE))
    machine.stub(LABEL_ADDRESS, sprite(CALL_LABEL))
    machine.stub(VALUE_ADDRESS, render_value)
    machine.stub(GAUGE_ADDRESS, record(CALL_GAUGE, 4))


def run(machine: Machine, value: dict, entry: int) -> tuple:
    mode = value["mode"]
    place(machine, value)
    calls: list[list[int]] = []
    sprites: list[tuple[int, int]] = []
    recorder(machine, value, calls, sprites)
    machine.follow.update((UNKNOWN_ADDRESS, BAR_ADDRESS, LIMBS_ADDRESS))
    for index, argument in enumerate(value["arguments"]):
        machine.r[R["a0"] + index] = argument & 0xFFFFFFFF
    machine.r[2] = 0
    machine.execute(entry)
    for call, address in sprites:
        calls[call][5] = machine.load(address + 0x10, signed=True)

    values = [
        s32(machine.r[2]) if mode in (NEXT, SWITCH, LIMBS, ROWS) else 0,
        *(machine.load(address, signed=True) for address in (PREVIOUS_ACTOR, SELECTED_ACTOR,
                                                             DRAW_BACKGROUND_FIRST, FADE_SCREEN,
                                                             ANIMATION_STEP)),
        *machine.bytes(VITALS_STATE, 5),
        machine.load(SWITCH_PHASE),
        machine.load(MENU_SELECTED, 1),
    ]
    names = ["result", "previous actor", "selected actor", "draw background first", "fade screen",
             "animation step", "vitals state", "vitals slide", "vitals delay", "shown actor",
             "delay screen update", "switch phase", "name row selected"]
    inputs = [
        *value["globals"], *value["bytes"], value["phase"], value["hud"], value["ordering_table"],
        *value["label_positions"], *value["label_widths"], *value["label_texture_u"],
        *value["arguments"], *value["values"],
    ]
    for slot in value["slots"]:
        inputs += [int(slot["present"]), slot["flags"] & 0xFFFFFFFF, slot["kind"], *slot["vitals"],
                   slot["analyzed"] & 0xFFFFFFFF, *slot["limbs"], slot["accessory"]]
    return recorded_observation(machine, values, names, calls), [mode, *inputs]


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    mode = value["mode"]
    if mode in (INIT, VITALS):
        allowed.append((VITALS_STATE, SHOWN_ACTOR + 1))
    if mode == SWITCH:
        allowed += [(SELECTED_ACTOR, GLOBALS_END), (MENU_ITEM, MENU_ITEM + MENU_ITEM_BYTES)]
    if mode in (UNKNOWN, VITALS):
        allowed.append((SPRITES, SPRITES + SPRITE_AREA))
    return allowed


def describe(image, results) -> list[str]:
    called = sum(1 for _, observation, _ in results if observation["calls"])
    returned = sum(1 for _, observation, _ in results if observation["values"][0])
    return [f"{called} of {len(results)} vectors made a recorded call, {returned} returned nonzero"]


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
