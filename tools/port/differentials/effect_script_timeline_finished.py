"""F01542 func_800D4E90 (BATTLE.PRG 5BF94.c:7734), typed timeline-finished branch."""

from effect_script_timeline_differential import FINISHED, configure

globals().update(configure(1542, FINISHED, 0x800D4E90, 0x800D4EC0, {
    "track-count-field": {0x800D4EA4: 0x8C440000},
    "branch-when-active": {0x800D4EAC: 0x0004202B},
    "result-2": {0x800D4EB4: 0x24020002},
}))
