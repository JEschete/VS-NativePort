"""F02617: dispatcher states, reward gates, repeat entry and dismissal."""

from __future__ import annotations

import random

import menu2_ability_foundation as h
from differential import Machine

globals().update(h.case_settings())
ID = "F02617"
ENTRY, END = 0x80104578, 0x80104690
DRIVER = "menu2_unlock_notifications_driver.cpp"
SEED = 2617
MUTANTS = {
    "state-count": {0x80104590: 0x2C620005},
    "skip-information-init": {0x801045B8: 0},
    "art-next-state": {0x801045C0: 0x24020004},
    "pending-art-gate": {0x801045D4: 0x1440002A},
    "art-initializer-argument": {0x801045E0: 0x24040000},
    "art-initializer-result": {0x801045E4: 0x10400026},
    "art-wait-state": {0x801045EC: 0x24020001},
    "art-update-argument": {0x801045FC: 0x24040001},
    "art-update-result": {0x80104600: 0x1440001E},
    "ability-next-state": {0x80104610: 0x24020004},
    "pending-ability-gate": {0x80104624: 0x14400016},
    "ability-initializer-argument": {0x80104630: 0x24040000},
    "ability-initializer-result": {0x80104634: 0x10400012},
    "ability-wait-state": {0x8010463C: 0x24020003},
    "ability-update-argument": {0x8010464C: 0x24040001},
    "ability-update-result": {0x80104650: 0x1440000A},
    "dismiss-box": {0x80104664: 0x24040006},
    "dismiss-result": {0x80104668: 0x10400005},
    "dismiss-reset-store": {0x80104670: 0},
    "completion-return": {0x80104678: 0x24020000},
}
NATIVE_REPLACE = ("state = kCheckAbility;\n      if (host.BreakArtPending()",
                  "state = kUpdateAbility;\n      if (host.BreakArtPending()")


def vector(rng: random.Random, number: int) -> list[int]:
    state = (0, 1, 2, 3, 4, 5, 6, 255)[number % 8]
    flags = (0, 1, 128, 255)
    results = (0, 1, -1, 2)
    group = number // 8
    return [state, flags[group % 4], flags[(group // 4) % 4],
            results[(group // 16 + group // 4) % 4],
            results[(group // 64 + group) % 4],
            *(rng.choice(results) for _ in range(3))]


def run(machine: Machine, value: list[int]) -> tuple[dict, list[int]]:
    h.validate(machine)
    state, art_pending, ability_pending, art_init, art_update, ability_init, ability_update, dismiss = value
    machine.put(h.ACTOR, state, 1)
    machine.put(0x800F4E88, art_pending, 1)
    machine.put(0x800F4FDA, ability_pending, 1)
    calls = []

    def art(m: Machine) -> None:
        calls.append([1, m.r[4]])
        m.r[2] = (art_init if m.r[4] else art_update) & 0xFFFFFFFF

    def ability(m: Machine) -> None:
        calls.append([2, m.r[4]])
        m.r[2] = (ability_init if m.r[4] else ability_update) & 0xFFFFFFFF

    def close(m: Machine) -> None:
        calls.append([3, m.r[4]])
        m.r[2] = dismiss & 0xFFFFFFFF

    machine.stub(0x800FFBC8, lambda m: calls.append([0]))
    machine.stub(0x80103C3C, art)
    machine.stub(0x80103E20, ability)
    machine.stub(0x800CD064, close)
    machine.r[4] = h.ACTOR
    machine.execute(ENTRY)
    return h.observation(machine, [machine.r[2], machine.load(h.ACTOR, 1)],
                         calls, [(h.ACTOR, h.ACTOR + 1)], ["result", "phase"]), value


check = h.check
describe = h.describe
