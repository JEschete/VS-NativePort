"""F01538 func_800D4D98 (BATTLE.PRG 5BF94.c:7705), typed action variant clear."""

from effect_script_opcode_differential import CLEAR_VARIANT, configure

globals().update(configure(1538, CLEAR_VARIANT, 0x800D4D98, 0x800D4DA8, {
    "clears-next-byte": {0x800D4D9C: 0xA040522D},
    "returns-two": {0x800D4DA4: 0x24020002},
}))
