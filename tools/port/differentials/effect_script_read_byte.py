"""F01549 func_800D5170 (BATTLE.PRG 5BF94.c:7804), typed effect script byte read."""

from effect_script_differential import READ_BYTE, configure

globals().update(configure(1549, READ_BYTE, 0x800D5170, 0x800D5198, {
    "offset-not-advanced": {0x800D517C: 0},
    "reads-next-byte": {0x800D5184: 0x3043FFFF},
}))
