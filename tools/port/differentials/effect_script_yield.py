"""F01517 func_800D4720 (BATTLE.PRG 5BF94.c:7515), typed effect script yield."""

from effect_script_differential import YIELD, configure

globals().update(configure(1517, YIELD, 0x800D4720, 0x800D474C, {
    "yield-continues": {0x800D4740: 0x24020001},
    "target-not-stored": {0x800D4734: 0},
}))
