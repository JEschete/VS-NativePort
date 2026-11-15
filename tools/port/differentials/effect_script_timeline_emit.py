"""F01534 0x800D4BD0 (BATTLE.PRG 5BF94.c:7651), typed register particle emit."""

from effect_script_timeline_differential import EMIT, configure

globals().update(configure(1534, EMIT, 0x800D4BD0, 0x800D4C18, {
    "next-register": {0x800D4BF4: 0x90450011},
    "variant-1": {0x800D4BF8: 0x24070001},
    "parent-ra": {0x800D4C00: 0xAFBF0010},
    "result-0": {0x800D4C0C: 0x24020000},
}))
