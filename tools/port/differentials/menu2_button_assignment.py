"""F02620: button toggles, duplicate clearing and post-sound mapping effects."""

import menu2_ability_terminal as h

globals().update(h.settings())
ID = "F02620"
ENTRY, END = 0x80102B5C, 0x80102CAC
SEED = 2620
MUTANTS = {
    "kind-dispatch": {0x80102B70: 0x24020019},
    "chain-byte-sign": {0x80102B90: 0x80500000},
    "chain-button-field": {0x80102BA0: 0x9222000D},
    "chain-toggle-gate": {0x80102BA8: 0x10500005},
    "chain-leave-call": {0x80102BB0: 0},
    "chain-toggle-store": {0x80102BBC: 0xA230000C},
    "chain-set-group": {0x80102BAC: 0x2404007D},
    "chain-set-effect": {0x80102BC4: 0x24050022},
    "chain-duplicate-gate": {0x80102BDC: 0x10500002},
    "chain-duplicate-store": {0x80102BE4: 0},
    "chain-button-count": {0x80102BEC: 0x28620002},
    "chain-new-assignment": {0x80102C08: 0xA040000C},
    "defense-byte-sign": {0x80102C18: 0x80500000},
    "defense-toggle-gate": {0x80102C30: 0x10500005},
    "defense-toggle-store": {0x80102C44: 0xA2300010},
    "defense-set-effect": {0x80102C4C: 0x24050022},
    "defense-duplicate-gate": {0x80102C64: 0x10500002},
    "defense-button-count": {0x80102C74: 0x28620002},
    "defense-new-assignment": {0x80102C8C: 0xA0400010},
}
NATIVE_SOURCE = h.SOURCES[0]
NATIVE_REPLACE = ("mappings[slot] = *action;", "mappings[slot] = 0;")
vector = h.assignment_vector


def run(machine, value):
    return h.run_assignment(machine, value, ENTRY)


check, describe = h.check, h.describe
