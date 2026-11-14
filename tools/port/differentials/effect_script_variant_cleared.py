"""F01525 func_800D48E4 (BATTLE.PRG 5BF94.c:7576), typed branch on a cleared action variant."""

from effect_script_opcode_differential import VARIANT_CLEARED, configure

globals().update(configure(1525, VARIANT_CLEARED, 0x800D48E4, 0x800D4910, {
    "branch-while-set": {0x800D48FC: 0x0004202B},
    "returns-zero": {0x800D4904: 0x24020000},
}))
