"""F01550 func_800D5198 (BATTLE.PRG 5BF94.c:7806), typed effect script halfword read."""

from effect_script_differential import READ_HALFWORD, configure

globals().update(configure(1550, READ_HALFWORD, 0x800D5198, 0x800D51D8, {
    "high-byte-unshifted": {0x800D51C0: 0},
    "low-byte-dropped": {0x800D51C4: 0x00021025},
}))
