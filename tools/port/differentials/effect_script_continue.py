"""F01539 func_800D4DA8 (BATTLE.PRG 5BF94.c:7711), typed effect script no-operation."""

from effect_script_differential import CONTINUE, configure

globals().update(configure(1539, CONTINUE, 0x800D4DA8, 0x800D4DB0, {
    "continue-ends": {0x800D4DAC: 0x24020002},
    "continue-yields": {0x800D4DAC: 0x24020000},
}))
