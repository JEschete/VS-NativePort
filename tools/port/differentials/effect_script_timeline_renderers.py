"""F01526 0x800D4910 (BATTLE.PRG 5BF94.c:7582), typed renderer tick."""

from effect_script_timeline_differential import RENDERERS, configure

globals().update(configure(1526, RENDERERS, 0x800D4910, 0x800D4984, {
    "update-command-3": {0x800D4938: 0x24050003},
    "detach-running": {0x800D494C: 0x10400003},
    "first-only": {0x800D4960: 0x00000000},
    "result-0": {0x800D4978: 0x24020000},
}))
