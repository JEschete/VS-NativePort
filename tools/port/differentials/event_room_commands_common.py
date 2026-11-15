"""Shared harness for the event-script room opcode differentials (BATTLE.PRG 4A0A8.c:1966-2274, #276).

Each case runs one original opcode handler under the R3000 interpreter against its typed
VS::Event core (ScriptRoomCommands.cpp) in event_room_commands_driver.cpp. The original's
instruction lies at every alignment between random neighbour bytes, so a handler reading the
wrong operand, or reading one with the wrong width or signedness, differs. The stubs record the
room callees' argument registers as signed words; the backdrop byte (D_800F4C6A) and the draw mode
word (D_800F19C8) start at random sentinels, and the driver's host stores them as LiveVmHost does.
The tiled background modes also draw the cutscene skip byte (D_800F4C2C) and a word every stubbed
callee returns, so a handler that keys its flow on the wrong skip state or on the callee's raw
result differs.
The environment modes (4A0A8.c:2180-2255) also draw the event weather tween (D_800F4B88), the
muted ambience byte and saved sound (D_800F4C08, D_800F4C4C), whether the room lacks an ambient
section (func_8008E470's 0 or 1) and the previous sound func_800913BC returns; the driver prints
them after the opcode.

Native differences outside the compared domain (covered by EventScriptVmTests.cpp): an instruction
shorter than its opcode's operands is refused without reading them.
"""

from __future__ import annotations

import random
import sys

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    Machine,
    mode_case,
    printed_problems,
    recorded_observation,
    s32,
)

MODULES = ("BATTLE/BATTLE.PRG",)
DRIVER = "event_room_commands_driver.cpp"
SOURCES = (r"Native\Game\src\Event\ScriptRoomCommands.cpp",)

SCRIPT = 0x80500000
SCRIPT_BYTES = 0x40
NEIGHBOURS = 6
BACKDROP = 0x800F4C6A
DRAW_MODE = 0x800F19C8
SKIP_STATE = 0x800F4C2C
WEATHER_TWEEN = 0x800F4B88
WEATHER_TWEEN_BYTES = 0x16
AMBIENT_SUSPENDED = 0x800F4C08
SUSPENDED_SOUND = 0x800F4C4C
GET_SHORT = 0x800BFE00

PLATFORM_ADDRESS = 0x8008E224
LOD_DEPTH_ADDRESS = 0x80098194
SHOW_ADDRESS = 0x80091998
HIDE_ADDRESS = 0x800919D8
SWAP_ADDRESS = 0x80091A1C
COLLISION_ADDRESS = 0x8008B808
DEPTH_BIAS_ADDRESS = 0x80091A78
BACKGROUND_ENABLED_ADDRESS = 0x8008EAF8
BACKGROUND_LOAD_ADDRESS = 0x8008EFCC
BACKGROUND_FINISH_ADDRESS = 0x8008F0FC
BACKGROUND_RELEASE_ADDRESS = 0x8008F234
BACKGROUND_OFFSET_ADDRESS = 0x80091468
FIRST_PERSON_ADDRESS = 0x8007BDD8
PARALLAX_ADDRESS = 0x8008EAE4
WEATHER_ADDRESS = 0x80090C2C
INTENSITY_ADDRESS = 0x80091320
WIND_ADDRESS = 0x8009134C
VARIATION_ADDRESS = 0x80091314
AMBIENT_ABSENT_ADDRESS = 0x8008E470
AMBIENT_SOUND_ADDRESS = 0x800913BC
EFFECT_ACTIVE_ADDRESS = 0x80093764
FINISH_ROTATION_ADDRESS = 0x80093788
START_EFFECT_ADDRESS = 0x8009291C
TOGGLE_EFFECT_ADDRESS = 0x800936F8

(
    GIMMICK,
    DEPTH_CUE,
    LIGHTING,
    SHOW,
    HIDE,
    SWAP,
    COLLISION,
    DEPTH_BIAS,
    BACKGROUND_VISIBLE,
    BACKGROUND_LOAD,
    BACKGROUND_WAIT,
    BACKGROUND_FREE,
    BACKGROUND_OFFSET,
    FIRST_PERSON,
    PARALLAX,
    ENVIRONMENT_PARTICLES,
    PARTICLE_VARIATION,
    MUTE_SOUND,
    ROOM_ANIMATION,
) = range(19)
# The modes whose vectors draw the skip byte and the callees' result (4A0A8.c:2160-2274).
BACKGROUND_MODES = frozenset(range(BACKGROUND_VISIBLE, PARALLAX + 1))
# The modes whose vectors also draw the weather and ambience state (4A0A8.c:2180-2255).
ENVIRONMENT_MODES = frozenset(range(ENVIRONMENT_PARTICLES, ROOM_ANIMATION + 1))

# Each mode's opcode byte and length (the BATTLE.PRG size table at 0x800E9C30).
OPCODES = {
    GIMMICK: (0x60, 3),
    DEPTH_CUE: (0x61, 3),
    LIGHTING: (0x62, 2),
    SHOW: (0x64, 2),
    HIDE: (0x65, 2),
    SWAP: (0x66, 3),
    COLLISION: (0x67, 5),
    DEPTH_BIAS: (0x7D, 3),
    BACKGROUND_VISIBLE: (0x73, 2),
    BACKGROUND_LOAD: (0x74, 2),
    BACKGROUND_WAIT: (0x75, 1),
    BACKGROUND_FREE: (0x76, 1),
    BACKGROUND_OFFSET: (0x7B, 5),
    FIRST_PERSON: (0x7C, 2),
    PARALLAX: (0x7E, 4),
    ENVIRONMENT_PARTICLES: (0x77, 5),
    PARTICLE_VARIATION: (0x78, 2),
    MUTE_SOUND: (0x7A, 2),
    ROOM_ANIMATION: (0x79, 3),
}

# The kinds the driver prints for each call, and the argument registers each callee takes.
CALLS = {
    PLATFORM_ADDRESS: (1, 2),
    LOD_DEPTH_ADDRESS: (2, 1),
    SHOW_ADDRESS: (3, 1),
    HIDE_ADDRESS: (4, 1),
    SWAP_ADDRESS: (5, 2),
    COLLISION_ADDRESS: (6, 4),
    DEPTH_BIAS_ADDRESS: (7, 2),
    BACKGROUND_ENABLED_ADDRESS: (8, 1),
    BACKGROUND_LOAD_ADDRESS: (9, 1),
    BACKGROUND_FINISH_ADDRESS: (10, 0),
    BACKGROUND_RELEASE_ADDRESS: (11, 0),
    BACKGROUND_OFFSET_ADDRESS: (12, 2),
    FIRST_PERSON_ADDRESS: (13, 1),
    PARALLAX_ADDRESS: (14, 2),
    WEATHER_ADDRESS: (15, 1),
    INTENSITY_ADDRESS: (16, 1),
    WIND_ADDRESS: (17, 2),
    VARIATION_ADDRESS: (18, 1),
    AMBIENT_ABSENT_ADDRESS: (19, 0),
    AMBIENT_SOUND_ADDRESS: (20, 1),
    EFFECT_ACTIVE_ADDRESS: (21, 1),
    FINISH_ROTATION_ADDRESS: (22, 1),
    START_EFFECT_ADDRESS: (23, 1),
    TOGGLE_EFFECT_ADDRESS: (24, 1),
}
# The callees whose result the environment vectors draw separately: func_8008E470 returns only 0
# or 1 (the host reports a bool), func_800913BC the previous ambient sound.
RESULT_KEYS = {AMBIENT_ABSENT_ADDRESS: "absent", AMBIENT_SOUND_ADDRESS: "previous"}

BOUNDARY_BYTES = (0, 1, 0x7F, 0x80, 0xFF)
# Sign bit and depth mask edges of opcode 0x61's halfword.
DEPTH_CUE_HALVES = (0, 1, 0x7FF, 0x800, 0x7FFF, 0x8000, 0x8001, 0x87FF, 0x8800, 0xFBFF, 0xFFFF)
# Sign edges of opcode 0x7B's halfwords.
OFFSET_HALVES = (0, 1, 0x7FFF, 0x8000, 0x8001, 0xFFFF)
# The skip byte's states (0 none, 1 skippable, 2 skipping) and values no state uses.
SKIP_STATES = (0, 1, 2, 2, 3, 0x82, 0xFF)
# What the stubbed callees return: func_8008F0FC's 0 and 1, then words a raw test would misread.
RESULTS = (0, 0, 1, 1, 2, -1, -0x80000000, 0x100)
# Sign, wrap and half-turn edges of the tween's halfwords.
TWEEN_HALVES = (0, 1, -1, 0x200, 0x7FF, 0x800, 0x801, -0x800, -0x801, 0xF00, 0x7FFF, -0x8000)
# The muted ambience byte: unset, set by the opcode, and other nonzero bytes.
SUSPENDED_BYTES = (0, 0, 1, 1, 0x80, 0xFF)
# The tween's fields in memory order: (offset, size, signed).
TWEEN_FIELDS = ((2, 1, False), (3, 1, False), *((offset, 2, True) for offset in range(4, 0x16, 2)))


def operand_byte(rng: random.Random) -> int:
    return rng.choice((*BOUNDARY_BYTES, rng.randrange(256), rng.randrange(256)))


def vector(rng: random.Random, number: int, mode: int) -> dict:
    opcode, length = OPCODES[mode]
    operands = [operand_byte(rng) for _ in range(length - 1)]
    if mode == DEPTH_CUE:
        half = rng.choice((*DEPTH_CUE_HALVES, rng.randrange(1 << 16), rng.randrange(1 << 16)))
        operands = [half & 0xFF, half >> 8]
    elif mode == BACKGROUND_OFFSET:
        halves = [rng.choice((*OFFSET_HALVES, rng.randrange(1 << 16))) for _ in range(2)]
        operands = [byte for half in halves for byte in (half & 0xFF, half >> 8)]
    value = {
        "mode": mode,
        "alignment": number % 4,
        "bytes": [opcode, *operands],
        "before": [rng.randrange(256) for _ in range(NEIGHBOURS)],
        "after": [rng.randrange(256) for _ in range(NEIGHBOURS)],
        "backdrop": rng.choice((0, 0xFF, 0x5A, rng.randrange(256))),
        "draw_mode": s32(rng.choice((0, 0xFFFFFFFF, 0x12345678, rng.randrange(1 << 32)))),
    }
    # Drawn after the earlier modes' fields so their vectors keep their seeds' sequences.
    if mode in BACKGROUND_MODES:
        value["skip_state"] = rng.choice((*SKIP_STATES, rng.randrange(256)))
        value["result"] = rng.choice((*RESULTS, s32(rng.randrange(1 << 32))))
    if mode in ENVIRONMENT_MODES:
        value["skip_state"] = rng.choice((*SKIP_STATES, rng.randrange(256)))
        value["result"] = rng.choice((*RESULTS, s32(rng.randrange(1 << 32))))
        value["absent"] = rng.randrange(2)
        value["previous"] = rng.choice((-1, 0, 0x7FFF, 0x8000, 0x18000, s32(rng.randrange(1 << 32))))
        value["suspended"] = rng.choice((*SUSPENDED_BYTES, rng.randrange(256)))
        value["saved_sound"] = rng.choice((-1, 0, 0x7FFF, -0x8000, rng.randrange(-0x8000, 0x8000)))
        value["tween_reserved"] = [rng.randrange(256) for _ in range(2)]
        value["tween"] = [rng.randrange(256) if size == 1
                          else rng.choice((*TWEEN_HALVES, rng.randrange(-0x8000, 0x8000)))
                          for _, size, _ in TWEEN_FIELDS]
        if mode == ROOM_ANIMATION:
            # Mostly the start and toggle modes, while skipping, over active and inactive effects.
            value["bytes"][1] = rng.choice((0, 0, 0, 1, 1, 2, 0xFF))
            value["skip_state"] = rng.choice((0, 1, 2, 2, 2, 3, 0xFF))
            value["result"] = rng.choice((0, 0, 0, *RESULTS[2:]))
    return value


def recorder(calls: list[list[int]], value: dict):
    def record(kind: int, count: int, result: int | None):
        def stub(machine: Machine) -> None:
            calls.append([kind, *(s32(machine.r[4 + index]) for index in range(count))])
            if result is not None:
                machine.r[2] = result & 0xFFFFFFFF

        return stub

    return {address: record(kind, count, value.get(RESULT_KEYS.get(address, "result")))
            for address, (kind, count) in CALLS.items()}


def run(machine: Machine, value: dict, entry: int) -> tuple:
    machine.map_zero(SCRIPT, SCRIPT_BYTES)
    address = SCRIPT + NEIGHBOURS + value["alignment"]
    machine.store(address - NEIGHBOURS, bytes(value["before"]))
    machine.store(address, bytes(value["bytes"]))
    machine.store(address + len(value["bytes"]), bytes(value["after"]))
    machine.put(BACKDROP, value["backdrop"], 1)
    machine.put(DRAW_MODE, value["draw_mode"], 4)
    if "skip_state" in value:
        machine.put(SKIP_STATE, value["skip_state"], 1)
    tween = value.get("tween", [0] * len(TWEEN_FIELDS))
    machine.store(WEATHER_TWEEN, bytes(value.get("tween_reserved", [0, 0])))
    for (offset, size, _), field in zip(TWEEN_FIELDS, tween, strict=True):
        machine.put(WEATHER_TWEEN + offset, field & ((1 << (8 * size)) - 1), size)
    machine.put(AMBIENT_SUSPENDED, value.get("suspended", 0), 1)
    machine.put(SUSPENDED_SOUND, value.get("saved_sound", 0) & 0xFFFF, 2)
    calls: list[list[int]] = []
    for callee, stub in recorder(calls, value).items():
        machine.stub(callee, stub)
    machine.follow.add(GET_SHORT)
    machine.r[4] = address
    machine.execute(entry)

    values = [
        s32(machine.r[2]),
        0,
        machine.load(BACKDROP, 1),
        machine.load(DRAW_MODE, 4, signed=True),
        *(machine.load(WEATHER_TWEEN + offset, size, signed=signed) for offset, size, signed in TWEEN_FIELDS),
        machine.load(AMBIENT_SUSPENDED, 1),
        machine.load(SUSPENDED_SOUND, 2, signed=True),
    ]
    names = ["flow", "refusals", "backdrop", "draw mode",
             *(f"tween +{offset:#x}" for offset, _, _ in TWEEN_FIELDS), "ambient suspended", "saved sound"]
    inputs = [
        value["mode"],
        value["backdrop"],
        value["draw_mode"],
        value.get("skip_state", 0),
        value.get("result", 0),
        value.get("absent", 0),
        value.get("previous", 0),
        value.get("suspended", 0),
        value.get("saved_sound", 0),
        *tween,
        len(value["bytes"]),
        *value["bytes"],
    ]
    return recorded_observation(machine, values, names, calls), inputs


def allowed_stores(value: dict) -> list[tuple[int, int]]:
    allowed = [(DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK)]
    if value["mode"] == DEPTH_CUE:
        allowed.append((BACKDROP, BACKDROP + 1))
    elif value["mode"] == LIGHTING:
        allowed.append((DRAW_MODE, DRAW_MODE + 4))
    elif value["mode"] == ENVIRONMENT_PARTICLES:
        allowed.append((WEATHER_TWEEN + 2, WEATHER_TWEEN + WEATHER_TWEEN_BYTES))
    elif value["mode"] == MUTE_SOUND:
        allowed += [(AMBIENT_SUSPENDED, AMBIENT_SUSPENDED + 1), (SUSPENDED_SOUND, SUSPENDED_SOUND + 2)]
    return allowed


def check(image, value: dict, observation: dict, native: list[int], reference) -> list[str]:
    return printed_problems(observation, native, allowed_stores(value))


def describe(image, results) -> list[str]:
    if not results:
        return []
    called = sum(1 for _, observation, _ in results if observation["calls"])
    lines = [f"{called} of {len(results)} vectors made a room call"]
    mode = results[0][0]["mode"]
    if mode == DEPTH_CUE:
        kept = sum(1 for value, _, _ in results if value["bytes"][2] & 0x80)
        lines.append(f"{kept} of {len(results)} vectors set the depth cue sign bit")
    elif mode == LIGHTING:
        changed = sum(1 for value, observation, _ in results if observation["values"][3] != value["draw_mode"])
        lines.append(f"{changed} of {len(results)} vectors changed the draw mode word")
    elif mode in (BACKGROUND_WAIT, BACKGROUND_FREE):
        flows = sorted({observation["values"][0] for _, observation, _ in results})
        held = sum(1 for _, observation, _ in results if observation["values"][0])
        lines.append(f"{held} of {len(results)} vectors held or yielded (flows {flows})")
    elif mode == ENVIRONMENT_PARTICLES:
        tweened = sum(1 for value, _, _ in results if value["bytes"][4])
        lines.append(f"{tweened} of {len(results)} vectors started a tween")
    elif mode == MUTE_SOUND:
        changed = sum(1 for value, observation, _ in results
                      if observation["values"][4 + len(TWEEN_FIELDS)] != value["suspended"])
        lines.append(f"{changed} of {len(results)} vectors changed the muted ambience byte")
    elif mode == ROOM_ANIMATION:
        finished = sum(1 for _, observation, _ in results if any(call[0] == 22 for call in observation["calls"]))
        lines.append(f"{finished} of {len(results)} vectors finished a door rotation")
    return lines


def configure(function: int, mode: int, entry: int, end: int, mutants: dict) -> dict:
    """Case-module globals for one original compared in `mode`."""
    return mode_case(function, mode, entry, end, mutants, harness=sys.modules[__name__])
