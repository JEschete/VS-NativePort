"""F01522 0x800D47F4 (BATTLE.PRG 5BF94.c:7546), typed script renderer attach."""

from effect_script_timeline_differential import ATTACH, configure

globals().update(configure(1522, ATTACH, 0x800D47F4, 0x800D487C, {
    "low-byte": {0x800D481C: 0x305000FF},
    "high-shift-8": {0x800D4828: 0x00021200},
    "always-operands": {0x800D4834: 0x10000002},
    "result-0": {0x800D4870: 0x24020000},
}))
