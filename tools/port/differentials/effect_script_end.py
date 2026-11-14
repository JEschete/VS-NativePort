"""F01520 func_800D47C0 (BATTLE.PRG 5BF94.c:7538), typed effect script end."""

from effect_script_differential import END, configure

globals().update(configure(1520, END, 0x800D47C0, 0x800D47C8, {
    "end-continues": {0x800D47C4: 0x24020001},
    "end-yields": {0x800D47C4: 0x24020000},
}))
