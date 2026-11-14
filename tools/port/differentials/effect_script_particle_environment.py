"""F01536 func_800D4D44 (BATTLE.PRG 5BF94.c:7691), typed particle environment refresh."""

from effect_script_timeline_differential import ENVIRONMENT, configure

globals().update(configure(1536, ENVIRONMENT, 0x800D4D44, 0x800D4D64, {
    "no-refresh": {0x800D4D4C: 0x00000000},
    "result-2": {0x800D4D58: 0x24020002},
}))
