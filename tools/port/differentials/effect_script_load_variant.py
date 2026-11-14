"""F01530 func_800D4AB4 (BATTLE.PRG 5BF94.c:7625), typed action variant register load."""

from effect_script_opcode_differential import LOAD_VARIANT, configure

globals().update(configure(1530, LOAD_VARIANT, 0x800D4AB4, 0x800D4AEC, {
    "five-variants": {0x800D4AC0: 0x2C420005},
    "loads-register-two": {0x800D4AE0: 0xA0820012},
}))
