"""F01392 text-box animation, drawer dispatch and frame-tail orchestration.

Runs the original rsin. Drawing, ordering-table services, timer, overlay and
RNG are external doubles. All box words, buffer publication, callback order,
arguments and store spans are checked. The retired current-box byte is not
native state. Corrupt ordering indices are a separate checked native domain.
"""

from __future__ import annotations

import random

from differential import (
    DEFAULT_STACK,
    DEFAULT_STACK_SIZE,
    printed_problems,
    recorded_observation,
    s32,
)
from text_box_state_common import (
    BOX_COUNT,
    BOX_SIZE,
    BOX_WORDS,
    BOXES,
    box_words,
    halves,
)

ID = "F01392"
ENTRY, END = 0x800CB83C, 0x800CBBCC
DRIVER = "text_box_update_driver.cpp"
SOURCES = (
    r"Native\Game\src\Menu\TextBoxUpdate.cpp",
    r"Native\Game\src\Menu\TextBoxState.cpp",
    r"Native\Engine\src\Core\Math\ScalarMath.cpp",
    r"Native\Engine\src\Core\Math\Distance.cpp",
)
MODULES = ("BATTLE/BATTLE.PRG", "SLUS_010.40")
CXX_STANDARD = "c++latest"
VECTORS, SEED, MUTANT_VECTORS = 1024, 1392, 256
ORIGINAL_SHA256 = "a94fff6505e9df0d218c219b44acac0cf351c5f976fd90c8de490969bca492b5"
TABLE = 0x800F4CD0
TABLE_POINTER = 0x800F51B8
BUFFER = 0x800EB9CE
CURRENT_BOX = 0x800F4E80
TICK = 0x8005E24C
HUD = 0x800F4E90
TIMER = 0x8005046C
SCRATCH = 0x1F800000
SCENE = 0x80700100
TABLE_STRIDE = 34 * 4
MUTANTS = {
    "closed-box-gate": {0x800CB884: 0},
    "frozen-box-gate": {0x800CB894: 0},
    "signed-duration": {0x800CB8A4: 0x82320022},
    "full-tick-speed": {0x800CB8AC: 0x00028003},
    "closing-increment": {0x800CB8B0: 0x02709821},
    "closing-retains-state": {0x800CB8C8: 0},
    "closing-scale-off-by-one": {0x800CB8CC: 0x24420000},
    "opening-increment": {0x800CB8E4: 0x02709823},
    "opening-clamp": {0x800CB8F4: 0x26530000},
    "sine-angle": {0x800CB928: 0x00102240},
    "sine-scale": {0x800CB940: 0x000280C0},
    "linear-scale": {0x800CB950: 0x00101BC0},
    "spring-duration": {0x800CB918: 0x2A420002},
    "spring-offset": {0x800CB994: 0x24505000},
    "overshoot": {0x800CB9A0: 0x34422000},
    "signed-columns": {0x800CB9EC: 0x82230020},
    "column-padding": {0x800CBA04: 0x24420009},
    "signed-lines": {0x800CBA0C: 0x82230021},
    "line-padding": {0x800CBA28: 0x24420003},
    "state-store": {0x800CBA34: 0},
    "frame-x-store": {0x800CBA44: 0},
    "frame-y-store": {0x800CBA50: 0},
    "frame-width-store": {0x800CBA58: 0},
    "frame-height-store": {0x800CBA64: 0},
    "kind-mask": {0x800CBA38: 0x30630007},
    "information-backdrop-call": {0x800CBA98: 0},
    "blend-call": {0x800CBAB4: 0},
    "frame-call": {0x800CBAC4: 0},
    "progressive-call": {0x800CBAD8: 0},
    "scaled-call": {0x800CBAF0: 0},
    "skip-box-zero": {0x800CBB00: 0x24020000},
    "scene-slot": {0x800CBB1C: 0x2484FFEC},
    "chain-end": {0x800CBB24: 0x24A60080},
    "chain-submit-call": {0x800CBB20: 0},
    "buffer-rotation": {0x800CBB34: 0x24020001},
    "clear-count": {0x800CBB50: 0x24050021},
    "clear-call": {0x800CBB70: 0},
    "timer-gate": {0x800CBB80: 0x24020002},
    "timer-call": {0x800CBB8C: 0},
    "overlay-call": {0x800CBB94: 0},
    "random-call": {0x800CBB9C: 0},
    "random-argument": {0x800CBBA0: 0x24040001},
}


def vector(rng: random.Random, number: int) -> dict:
    words = box_words(rng, number)
    for index in range(BOX_COUNT):
        offset = index * BOX_WORDS
        duration = rng.choice((0, 1, 2, 3, 4, 8, 127, 128, 255, rng.randrange(256)))
        state = (0, 1, duration, duration + 1, -1, -duration,
                 -duration - 1, 32767, -32768, rng.randrange(-32768, 32768))[
                     (number + index) % 10]
        words[offset] = ((words[offset] & 0x7FFFFFC0) | ((number + index) % 4) << 4
                         | ((number + index) % 16))
        if rng.randrange(7) == 0:
            words[offset] |= 0x80000000
        words[offset + 7] = halves(state, words[offset + 7] >> 16)
        words[offset + 8] = (words[offset + 8] & 0xFF00FFFF) | duration << 16
    return {
        "words": words,
        "tick": rng.choice((0, 1, 2, 3, 4, -1, -2, 0x7FFFFFFF, -0x80000000,
                            s32(rng.getrandbits(32)))),
        "buffer": number % 3,
        "active": rng.randrange(3),
        "hud": rng.choice((0, 1, 1, 2, 127, 128, 255)),
    }


def run(machine, value: dict):
    machine.follow.add(0x80040F28)
    machine.map_zero(BOXES, BOX_COUNT * BOX_SIZE)
    machine.map_zero(TABLE, 3 * TABLE_STRIDE)
    machine.map_zero(SCRATCH, 0x400)
    for index, word in enumerate(value["words"]):
        machine.put(BOXES + index * 4, word)
    machine.put(TICK, value["tick"])
    machine.put(BUFFER, value["buffer"], 1)
    machine.put(HUD, value["hud"], 1)
    machine.put(TABLE_POINTER, TABLE + value["active"] * TABLE_STRIDE)
    machine.put(SCRATCH + 4, SCENE)
    calls: list[list[int]] = []

    def drawer(code, pointer, scale):
        def call(current):
            argument = current.r[4]
            index = (argument - BOXES) // BOX_SIZE if pointer else argument
            calls.append([code, index, s32(current.r[5]) if scale else 0, 0])
        return call

    for address, code, pointer, scale in (
        (0x800C809C, 0, True, True),
        (0x800CD158, 1, False, False),
        (0x800CD3E4, 2, False, False),
        (0x800C7230, 3, True, False),
        (0x800C7BA4, 4, True, True),
    ):
        machine.stub(address, drawer(code, pointer, scale))
    machine.stub(0x8002B514, lambda current: calls.append(
        [5, s32(current.r[4] - SCENE) // 4,
         (current.r[5] - TABLE) // TABLE_STRIDE,
         (current.r[6] - current.r[5]) // 4]))
    machine.stub(0x80028A74, lambda current: calls.append(
        [6, (current.r[4] - TABLE) // TABLE_STRIDE, current.r[5], 0]))
    machine.stub(0x800CB56C, lambda current: calls.append(
        [7, s32(current.r[4] - TIMER), 0, 0]))
    machine.stub(0x800CB708, lambda _current: calls.append([8, 0, 0, 0]))
    machine.stub(0x800CCC8C, lambda current: calls.append([9, current.r[4], 0, 0]))
    machine.execute(ENTRY)
    values = [0, machine.load(BUFFER, 1),
              (machine.load(TABLE_POINTER) - TABLE) // TABLE_STRIDE,
              *(machine.load(BOXES + index * 4) for index in range(BOX_COUNT * BOX_WORDS))]
    names = ["refused", "buffer", "active", *(f"box-word {index}" for index in range(96))]
    inputs = [value["tick"], value["buffer"], value["active"], value["hud"], *value["words"]]
    return recorded_observation(machine, values, names, calls), inputs


def check(_image, _value, observation, native, _reference):
    return printed_problems(observation, native, [
        (DEFAULT_STACK - DEFAULT_STACK_SIZE, DEFAULT_STACK),
        (BOXES, BOXES + BOX_COUNT * BOX_SIZE),
        (CURRENT_BOX, CURRENT_BOX + 1),
        (BUFFER, BUFFER + 1),
        (TABLE_POINTER, TABLE_POINTER + 4),
    ])


def describe(image, results):
    actual = image.sha256(ENTRY, END)
    if actual != ORIGINAL_SHA256:
        raise ValueError(f"F01392 original span changed: {actual}")
    return [
        f"F01392 228 original instructions sha256={actual}",
        f"{len(results)} complete box arrays, buffer publications and callback sequences compared",
        "Original rsin executes; external drawers/ordering/timer/overlay/RNG are doubled.",
        "Only the deliberately retired current-box byte is excluded from native observation.",
    ]
