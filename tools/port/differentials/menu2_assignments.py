"""F02621: paged rows, actual nested assignment, gradients, input and ownership."""

import menu2_ability_terminal as h

globals().update(h.settings())
ID = "F02621"
ENTRY, END = 0x80102CAC, 0x801034FC
SEED = 2621
MUTANTS = {
    "entry-phase-gate": {0x80102CF0: 0x14400034},
    "entry-row-id": {0x80102CF8: 0x25040015},
    "entry-row-x": {0x80102CFC: 0x2405013F},
    "entry-row-y": {0x80102D04: 0x24C60033},
    "row-width": {0x80102D18: 0x2407007D},
    "entry-row-stride": {0x80102D24: 0x00031980},
    "entry-state": {0x80102D38: 0x24020003},
    "entry-target": {0x80102D44: 0x240200C1},
    "top-scroll-marker": {0x80102D60: 0xA0800005},
    "entry-count-gate": {0x80102D78: 0x10460003},
    "visible-row-limit": {0x80102D8C: 0x24020007},
    "bottom-scroll-gate": {0x80102DAC: 0x14400002},
    "selected-description-offset": {0x80102DF0: 0x24840022},
    "ready-gate": {0x80102E24: 0x144001A7},
    "ready-state": {0x80102E28: 0x24020010},
    "display-cost-store": {0x80102E40: 0},
    "short-list-limit": {0x80102E54: 0x2C42000A},
    "short-kind": {0x80102E8C: 0x24020019},
    "short-circle-icon": {0x80102EAC: 0x24040001},
    "short-icon-x": {0x80102EB0: 0x240500C6},
    "short-triangle-icon": {0x80102ECC: 0x24040001},
    "short-square-icon": {0x80102F48: 0x24040002},
    "long-kind": {0x80102F98: 0x24020019},
    "long-action-offset": {0x80102FB0: 0x02201021},
    "long-icon-count": {0x80103080: 0x2A220007},
    "circle-mask": {0x80103098: 0x30620010},
    "triangle-mask": {0x801030B0: 0x30620080},
    "square-button": {0x801030D8: 0x24050001},
    "selected-clear": {0x801030F4: 0},
    "cancel-mask": {0x80103104: 0x30C20020},
    "free-call": {0x8010311C: 0},
    "text-detach": {0x80103138: 0},
    "memory-highlight": {0x80103144: 0xA0400000},
    "memory-first": {0x80103168: 0xA0600000},
    "up-repeat-mask": {0x80103174: 0x30424000},
    "up-short-wrap": {0x801031AC: 0x2442FFFE},
    "up-press-mask": {0x801031D8: 0x30C24000},
    "up-wrap-highlight": {0x801031E0: 0x24020006},
    "up-wrap-first": {0x801031E8: 0x2462FFF9},
    "up-scroll-row": {0x801031C4: 0x24020002},
    "up-scroll-decrement": {0x80103204: 0x2482FFFE},
    "down-repeat-mask": {0x80103220: 0x30421000},
    "down-short-wrap": {0x80103258: 0xAC824EB8},
    "down-last-page": {0x80103268: 0x2442FFF9},
    "down-press-mask": {0x80103294: 0x30421000},
    "down-wrap-first": {0x801032A8: 0},
    "down-scroll-row": {0x801032B8: 0x24020005},
    "down-scroll-increment": {0x801032C4: 0x24820002},
    "selection-change-gate": {0x801032EC: 0x16C20057},
    "change-sound-call": {0x801032F4: 0},
    "gradient-load": {0x8010337C: 0x90420005},
    "redraw-row-x": {0x801033A8: 0x240500C3},
    "gradient-restore": {0x801033F0: 0xA0800004},
    "redraw-top-scroll": {0x80103404: 0xA0800005},
    "redraw-bottom-scroll": {0x80103440: 0xA0800005},
    "selection-store": {0x80103460: 0xA0400006},
    "cursor-position": {0x80103470: 0x24A50003},
    "cursor-store": {0x80103474: 0},
    "risk-kind": {0x80103480: 0x24020019},
    "risk-action": {0x801034B8: 0x90440001},
}
NATIVE_SOURCE = h.SOURCES[0]
NATIVE_REPLACE = ("constexpr int kRowTargetX = 194;", "constexpr int kRowTargetX = 193;")
vector = h.assignments_vector


def run(machine, value):
    return h.run_assignments(machine, value, ENTRY)


check, describe = h.check, h.describe
