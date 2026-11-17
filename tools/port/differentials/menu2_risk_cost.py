"""F02618: unsigned-byte Risk text, decimal boundaries and untouched buffer bytes."""

from __future__ import annotations

import random

import menu2_ability_foundation as h
from differential import Fault, Machine

globals().update(h.case_settings())
ID = "F02618"
ENTRY, END = 0x80102864, 0x80102928
DRIVER = "menu2_risk_cost_driver.cpp"
SEED = 2618
MUTANTS = {
    "label-slot": {0x80102870: 0x24040000},
    "label-position": {0x8010287C: 0x24060009},
    "action-record-stride": {0x801028A4: 0x00021040},
    "cost-field": {0x801028B4: 0x90440004},
    "missing-terminator": {0x801028C4: 0},
    "decimal-helper-call": {0x801028C8: 0x0C033316},
    "digit-mask": {0x801028D8: 0x30820007},
    "digit-character": {0x801028DC: 0x24420031},
    "decimal-shift": {0x801028E0: 0x000420C3},
    "decimal-loop": {0x801028E4: 0x1080FFF8},
    "cost-marker": {0x801028FC: 0x24020024},
    "value-slot": {0x80102900: 0x24040001},
    "value-position": {0x80102904: 0x24060047},
    "disabled-value": {0x80102908: 0x24070001},
}
NATIVE_REPLACE = ("remaining /= kDecimalBase;", "remaining /= (kDecimalBase + 1);")
BUFFER = 0x801050B0


def vector(rng: random.Random, number: int) -> list[int]:
    return [rng.randrange(256), number % 256, rng.randrange(256)]


def run(machine: Machine, value: list[int]) -> tuple[dict, list[int]]:
    action, cost, seed = value
    h.setup(machine, seed)
    machine.put(h.ACTIONS + action * h.ACTION_STRIDE + 3, cost, 1)
    for index in range(16):
        machine.put(BUFFER + index, seed + index * 13, 1)
    calls = []

    def decimal(m: Machine) -> None:
        value = m.r[4]
        m.r[2] = (value % 10) | ((value // 10) * 16)

    def display(m: Machine) -> None:
        text = []
        for index in range(128):
            byte = m.load(m.r[5] + index, 1)
            if byte == 0:
                calls.append([10, m.r[4], m.r[6], m.r[7], len(text), *text])
                return
            text.append(byte)
        raise Fault("unterminated action cost")

    machine.stub(0x800CCC54, decimal)
    machine.stub(0x80100100, display)
    machine.r[4] = action
    machine.execute(ENTRY, limit=2048)
    values = [machine.load(BUFFER + index, 1) for index in range(16)]
    return h.observation(machine, values, calls, [(BUFFER, BUFFER + 16)]), value


check = h.check
describe = h.describe
