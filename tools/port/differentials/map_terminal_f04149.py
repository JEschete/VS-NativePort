"""F04149 vs_menu5_exec, MENU5.PRG 4D8.c:240; typed map::UpdateScreen."""

from map_terminal_common import SCREEN, configure

globals().update(configure(4149, SCREEN, 0x801030F4, 0x80103418, {
    "backup-size": {0x80103140: 0x3404B400},
    "no-current-row-reset": {0x80103150: 0},
    "capture-width": {0x8010318C: 0x2402005F},
    "open-delay": {0x801031F0: 0x24020007},
    "scene-loaded-not-stored": {0x8010321C: 0},
    "hud-slide": {0x80103260: 0x3C030010},
    "no-ready-transition": {0x80103288: 0},
    "close-delay": {0x801032D8: 0x24020009},
    "clear-width": {0x80103330: 0x2408001F},
    "clut-count": {0x8010338C: 0x240700FF},
    "alternate-destination": {0x801033D0: 0x24020005},
    "completion-zero": {0x801033E8: 0x24020000},
}))
