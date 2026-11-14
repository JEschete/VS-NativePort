"""F01524 func_800D48A4 (BATTLE.PRG 5BF94.c:7569), typed immediate timeline load."""

from effect_script_timeline_differential import IMMEDIATE, configure

globals().update(configure(1524, IMMEDIATE, 0x800D48A4, 0x800D48E4, {
    "timeline-zero": {0x800D48BC: 0x00002821},
    "a2-one": {0x800D48C4: 0x24060001},
    "loop-0xB8": {0x800D48C8: 0x240200B8},
    "result-2": {0x800D48D8: 0x24020002},
}))
