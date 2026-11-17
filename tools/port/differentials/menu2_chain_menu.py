"""F02622: canonical learned-chain producer order, empty rows and list lifecycle."""

import menu2_ability_terminal as h

globals().update(h.settings())
ID = "F02622"
ENTRY, END = 0x801034FC, 0x80103670
SEED = 2622
MUTANTS = {
    "initialize-gate": {0x80103500: 0x14800007},
    "hoisted-category": {0x80103508: 0x24040001},
    "hoist-mode": {0x80103510: 0x24050000},
    "phase-reset": {0x8010351C: 0},
    "shortcut-gate": {0x80103550: 0x10400005},
    "ready-gate": {0x80103560: 0x1440003E},
    "order-address": {0x80103570: 0x2458BDCC},
    "action-byte-sign": {0x801035A4: 0x80450000},
    "action-record-stride": {0x801035BC: 0x00021840},
    "unlocked-bit": {0x801035CC: 0x00021382},
    "unlocked-gate": {0x801035D4: 0x1440000D},
    "name-offset": {0x8010357C: 0x25AF001A},
    "description-start": {0x80103590: 0x240A000E},
    "description-index-stride": {0x80103618: 0x254A0004},
    "learned-action-store": {0x80103608: 0xA0600000},
    "producer-count": {0x80103610: 0x28C2000D},
    "menu-kind": {0x80103620: 0x24050019},
    "phase-advance": {0x80103638: 0},
    "closed-gate": {0x80103644: 0x14400006},
    "clear-retained-category": {0x80103650: 0x24040001},
    "closed-return": {0x80103658: 0x24020000},
}
NATIVE_SOURCE = h.SOURCES[1]
NATIVE_REPLACE = (
    "return UpdateLearnedAbilities(AbilityKind::kChain, state, host, initialize);",
    "return UpdateLearnedAbilities(AbilityKind::kDefense, state, host, initialize);",
)
vector = h.list_vector


def run(machine, value):
    return h.run_list(machine, value, ENTRY, 0)


check, describe = h.check, h.describe
