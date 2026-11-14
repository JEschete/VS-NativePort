"""F01551 func_800D51D8 (BATTLE.PRG 5BF94.c:7813), typed effect script loop."""

from effect_script_differential import RUN, configure

globals().update(configure(1551, RUN, 0x800D51D8, 0x800D5260, {
    "ended-yields": {0x800D5240: 0x24020001},
    "continue-stops": {0x800D5230: 0},
}))
