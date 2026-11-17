"""F02614: Break Art eligibility, progression, presentation and acknowledgement."""

from __future__ import annotations

import random

import menu2_ability_foundation as h
from differential import Machine

globals().update(h.case_settings())
ID = "F02614"
ENTRY, END = 0x80103C3C, 0x80103DD8
DRIVER = "menu2_break_art_unlock_driver.cpp"
SEED = 2614
MUTANTS = {
    "initialize-branch": {0x80103C4C: 0x14800049},
    "category-modulus": {0x80103C88: 0x00401021},
    "tier-four": {0x80103C9C: 0x24020003},
    "tier-gate": {0x80103CA0: 0x16220043},
    "requirement-tier-stride": {0x80103CB4: 0x00111880},
    "kills-signed-compare": {0x80103CD0: 0x0062102B},
    "threshold-branch": {0x80103CD4: 0x1040003A},
    "information-header": {0x80103CE0: 0x24040002},
    "learned-increment": {0x80103CE4: 0x26260002},
    "break-art-base": {0x80103CF0: 0x262300B9},
    "category-text-index": {0x80103D10: 0x00101000},
    "message-template": {0x80103D00: 0x24A40026},
    "message-timeout": {0x80103D60: 0x24030077},
    "unlocked-mask": {0x80103D68: 0x34424000},
    "box-done-bit": {0x80103D80: 0x00021742},
    "done-gate": {0x80103D88: 0x1440000C},
    "zero-timeout-gate": {0x80103D98: 0x14600005},
    "pressed-button-gate": {0x80103DA8: 0x14400003},
    "timeout-decrement": {0x80103DAC: 0x2462FFFE},
}
# This anchor is restricted to F02614 by including its following branch.
NATIVE_REPLACE = (
    "state.timeout = kMessageTimeout;\n  } else if (host.InformationBoxDone())",
    "state.timeout = kMessageTimeout - 1;\n  } else if (host.InformationBoxDone())",
)


def vector(rng: random.Random, number: int) -> list[int]:
    initialize = int(number % 8 < 4)
    weapon = (number // 8) % 11
    tier = (number // 11) % 5 if number < 440 else rng.randrange(5)
    requirement = rng.choice((0, 1, 100, 32768, 65535))
    kills = max(0, min(65535, requirement + (-1, 0, 1)[number % 3]))
    timeout = (0, 1, 2, 119, 120, 127, 128, 255)[(number // 8) % 8]
    done = (number // 2) % 2
    buttons = 0 if number % 2 == 0 else rng.choice((1, 2, 128, 255))
    return [initialize, weapon, tier, kills, requirement, timeout, done,
            buttons, rng.getrandbits(32)]


def run(machine: Machine, value: list[int]) -> tuple[dict, list[int]]:
    initialize, weapon, tier, kills, requirement, timeout, done, buttons, seed = value
    h.setup(machine, seed)
    category = weapon % 10
    machine.put(h.ACTORS, h.ACTOR)
    machine.map_zero(h.ACTOR, 0x40)
    machine.put(h.ACTOR + 0x25, weapon, 1)
    machine.put(h.PROGRESS + category, tier, 1)
    machine.put(h.PROGRESS + 12 + category * 2, kills, 2)
    for index in range(4):
        machine.put(h.ART_REQUIREMENTS + category * 8 + index * 2, requirement, 2)
    machine.put(h.BOX_FLAGS, done << 30)
    machine.put(h.INPUT, buttons, 1)
    machine.put(0x801050F0, timeout, 1)
    calls = []
    h.message_calls(machine, calls)
    machine.r[4] = initialize
    machine.execute(ENTRY, limit=2048)
    values = [machine.r[2], machine.load(0x801050F0, 1),
              *(machine.load(h.PROGRESS + index, 1) for index in range(12)),
              *h.flag_values(machine),
              machine.load(h.CONTEXT + 40), machine.load(h.CONTEXT + 44)]
    allowed = [(h.PROGRESS + category, h.PROGRESS + category + 1),
               (h.CONTEXT + 40, h.CONTEXT + 48), (0x801050F0, 0x801050F1),
               *h.flag_spans()]
    return h.observation(machine, values, calls, allowed, ["result", "timeout"]), value


check = h.check
describe = h.describe
