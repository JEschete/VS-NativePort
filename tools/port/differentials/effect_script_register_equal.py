"""F01532 func_800D4B34 (BATTLE.PRG 5BF94.c:7639), typed branch on a script register."""

from effect_script_opcode_differential import REGISTER_EQUAL, configure

globals().update(configure(1532, REGISTER_EQUAL, 0x800D4B34, 0x800D4B90, {
    "low-byte-only": {0x800D4B60: 0x304200FF},
    "branch-unless-equal": {0x800D4B74: 0x0004202B},
}))
