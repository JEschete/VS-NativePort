"""F04156 vs_menu9_exec, MENU9.PRG 238.c:269."""

from gazette_terminal_common import SCREEN, configure

globals().update(configure(4156, SCREEN, 0x80102FF8, 0x801031BC, {
    "initial-delay-seven": {0x8010304C: 0x24020007},
    "retain-old-progress": {0x80103058: 0},
    "omit-progress-calculation": {0x8010307C: 0},
    "delay-decrement-two": {0x80103098: 0x2442FFFE},
    "omit-field-disable": {0x801030B4: 0},
    "omit-HUD-hide": {0x801030BC: 0},
    "hidden-HUD-offset-one-unit": {0x801030C8: 0x3C030010},
    "omit-ready-phase-store": {0x801030D4: 0},
    "omit-ready-result-store": {0x801030F0: 0},
    "retain-old-selected-result": {0x80103104: 0},
    "background-priority-minus-three": {0x80103108: 0x2404FFFD},
    "omit-HUD-show": {0x80103114: 0},
    "retain-HUD-offset": {0x80103128: 0},
    "retain-command": {0x8010312C: 0},
    "clear-except-thirty-nine": {0x80103138: 0x24040027},
    "omit-fade-out": {0x80103148: 0},
    "return-menu-eight": {0x80103188: 0x24020008},
    "retain-hide-menu": {0x80103198: 0},
    "retain-outer-phase": {0x801031A0: 0},
}))
