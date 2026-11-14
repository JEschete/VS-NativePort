"""F01523 func_800D487C (BATTLE.PRG 5BF94.c:7563), typed branch once the renderers finish."""

from effect_script_opcode_differential import RENDERERS, configure

globals().update(configure(1523, RENDERERS, 0x800D487C, 0x800D48A4, {
    "branch-while-attached": {0x800D4890: 0x0004202B},
    "reads-timeline": {0x800D4888: 0x8CA40D4C},
}))
