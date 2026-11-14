"""F01531 func_800D4AEC (BATTLE.PRG 5BF94.c:7633), typed script register write."""

from effect_script_opcode_differential import SET_REGISTER, configure

globals().update(configure(1531, SET_REGISTER, 0x800D4AEC, 0x800D4B34, {
    "next-register": {0x800D4B18: 0xA2220011},
    "two-registers": {0x800D4B10: 0x32100001},
}))
