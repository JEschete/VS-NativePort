"""F01521 func_800D47C8 (BATTLE.PRG 5BF94.c:7540), typed effect script selector."""

from effect_script_opcode_differential import SELECTOR_MODE, configure

globals().update(configure(1521, SELECTOR_MODE, 0x800D47C8, 0x800D47F4, {
    "selector-seven-bits": {0x800D47E0: 0x3042007F},
    "returns-two": {0x800D47E8: 0x24020002},
}))
