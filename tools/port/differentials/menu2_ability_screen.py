"""F02625: screen dispatch, count-dependent text, nested effects and point animation."""

import menu2_ability_terminal as h

globals().update(h.settings())
ID = "F02625"
ENTRY, END = 0x801038D4, 0x80103C3C
SEED = 2625
MUTANTS = {
    "phase-range": {0x801038F8: 0x2C620006},
    "initial-animation-reset": {0x80103928: 0},
    "initial-point-step": {0x8010392C: 0x24020009},
    "initial-ready-gate": {0x80103938: 0x1440009E},
    "information-init-call": {0x80103940: 0},
    "shortcut-gate": {0x80103954: 0x14400018},
    "shortcut-y": {0x8010395C: 0x24060023},
    "shortcut-width": {0x80103960: 0x2407008B},
    "shortcut-background": {0x80103968: 0x24020007},
    "shortcut-type": {0x8010396C: 0x2490FFF8},
    "shortcut-row": {0x80103990: 0x24840004},
    "shortcut-selection": {0x801039AC: 0xA0400006},
    "category-ready-gate": {0x801039C0: 0x1440007B},
    "chain-availability": {0x801039E8: 0x10400006},
    "chain-unavailable-help": {0x801039F4: 0x2603006C},
    "chain-name-byte-quirk": {0x801039FC: 0x34420000},
    "defense-availability": {0x80103A18: 0x10400007},
    "defense-disabled-flag": {0x80103A2C: 0x34420000},
    "cursor-memory-gate": {0x80103A48: 0x14620002},
    "category-count": {0x80103A54: 0x24040001},
    "category-row-info": {0x80103A58: 0x24050116},
    "cursor-memory-restore": {0x80103A6C: 0},
    "select-type-state": {0x80103A68: 0x24020004},
    "selection-animation": {0x80103A84: 0xA22050C0},
    "selection-increment": {0x80103A88: 0x24500002},
    "selection-animation-stop": {0x80103A98: 0},
    "chain-state": {0x80103A9C: 0x24020007},
    "chain-initialize": {0x80103AA8: 0x24040000},
    "defense-initialize": {0x80103AC8: 0x24040000},
    "cancel-clear": {0x80103AD8: 0x24040002},
    "cancel-battle-code": {0x80103ADC: 0x2402FFFF},
    "chain-close-gate": {0x80103AF8: 0x1440002D},
    "chain-shortcut-exit": {0x80103B08: 0x1040000B},
    "defense-close-gate": {0x80103B20: 0x14400023},
    "defense-shortcut-exit": {0x80103B30: 0x14400006},
    "main-exit-command": {0x80103B60: 0x24040001},
    "main-exit-ready": {0x80103B6C: 0x14400010},
    "main-exit-state": {0x80103B78: 0},
    "battle-exit-menu": {0x80103BA0: 0x24030002},
    "battle-exit-state": {0x80103BAC: 0},
    "animation-direction": {0x80103BBC: 0x1440000E},
    "animation-byte-sign": {0x80103BD4: 0x80440000},
    "entry-animation-decrement": {0x80103BEC: 0x2442FFFE},
    "exit-animation-limit": {0x80103C04: 0x2C620009},
    "exit-animation-increment": {0x80103C0C: 0x24620002},
    "exit-animation-distance": {0x80103C1C: 0x00042100},
}
NATIVE_SOURCE = h.SOURCES[1]
NATIVE_REPLACE = (
    "constexpr int kPointsExitDistance = 32;",
    "constexpr int kPointsExitDistance = 31;",
)
vector = h.screen_vector


def run(machine, value):
    return h.run_screen(machine, value, ENTRY)


check, describe = h.check, h.describe
