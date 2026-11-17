"""F02623: canonical learned-defense producer order and independent lifecycle."""

import menu2_ability_terminal as h

globals().update(h.settings())
ID = "F02623"
ENTRY, END = 0x80103670, 0x801037E4
SEED = 2623
MUTANTS = {
    "initialize-gate": {0x80103674: 0x14800007},
    "hoisted-category": {0x8010367C: 0x24040000},
    "hoist-mode": {0x80103684: 0x00002821},
    "phase-reset": {0x80103690: 0},
    "shortcut-gate": {0x801036C4: 0x10400005},
    "ready-gate": {0x801036D4: 0x1440003E},
    "order-address": {0x801036E4: 0x2458BDBC},
    "action-byte-sign": {0x80103718: 0x80450000},
    "action-record-stride": {0x80103730: 0x00021840},
    "unlocked-bit": {0x80103740: 0x00021382},
    "unlocked-gate": {0x80103748: 0x1440000D},
    "name-offset": {0x801036F0: 0x25AF001A},
    "description-start": {0x80103704: 0x240A002A},
    "description-index-stride": {0x8010378C: 0x254A0004},
    "learned-action-store": {0x8010377C: 0xA0600000},
    "producer-count": {0x80103784: 0x28C2000D},
    "menu-kind": {0x80103794: 0x24050018},
    "phase-advance": {0x801037AC: 0},
    "closed-gate": {0x801037B8: 0x14400006},
    "clear-retained-category": {0x801037C4: 0x24040001},
    "closed-return": {0x801037CC: 0x24020000},
}
NATIVE_SOURCE = h.SOURCES[1]
NATIVE_REPLACE = (
    "return UpdateLearnedAbilities(AbilityKind::kDefense, state, host, initialize);",
    "return UpdateLearnedAbilities(AbilityKind::kChain, state, host, initialize);",
)
vector = h.list_vector


def run(machine, value):
    return h.run_list(machine, value, ENTRY, 1)


check, describe = h.check, h.describe
