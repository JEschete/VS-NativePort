"""F01519 0x800D4778 (BATTLE.PRG 5BF94.c:7527), typed child script spawn."""

from effect_script_timeline_differential import SPAWN, configure

globals().update(configure(1519, SPAWN, 0x800D4778, 0x800D47C0, {
    "offset-byte": {0x800D4798: 0x304200FF},
    "source-past-context": {0x800D479C: 0x26100D20},
    "result-0": {0x800D47B4: 0x24020000},
}))
