"""F02624: exact threshold text, saturated progress and packed-coordinate wrapping."""

import menu2_ability_terminal as h

globals().update(h.settings())
ID = "F02624"
ENTRY, END = 0x801037E4, 0x801038D4
SEED = 2624
MUTANTS = {
    "kills-field": {0x80103804: 0x94440020},
    "complete-count": {0x8010380C: 0x24020015},
    "complete-gate": {0x80103810: 0x1622002A},
    "threshold-stride": {0x8010381C: 0x00111880},
    "requirement-sign": {0x80103824: 0x84620000},
    "subtract-kills": {0x8010382C: 0x00402021},
    "negative-clamp": {0x80103830: 0x04800002},
    "text-x": {0x80103834: 0x24A300CF},
    "text-y": {0x8010383C: 0x3C020043},
    "unit-suffix": {0x80103854: 0x24020051},
    "digit-mask": {0x80103870: 0x30820007},
    "digit-character": {0x80103874: 0x24420031},
    "decimal-shift": {0x8010387C: 0x000420C3},
    "point-marker": {0x80103890: 0x24020024},
    "label-call": {0x801038A4: 0},
    "value-x": {0x801038B0: 0x26450061},
    "value-color": {0x801038B8: 0x24060001},
}
NATIVE_SOURCE = h.SOURCES[1]
NATIVE_REPLACE = (
    "static_cast<int>(*requirement) - host.TotalKills()",
    "static_cast<int>(*requirement) + host.TotalKills()",
)
vector = h.points_vector


def run(machine, value):
    return h.run_points(machine, value, ENTRY)


check, describe = h.check, h.describe
