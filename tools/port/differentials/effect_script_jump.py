"""F01518 func_800D474C (BATTLE.PRG 5BF94.c:7521), typed effect script jump."""

from effect_script_differential import JUMP, configure

globals().update(configure(1518, JUMP, 0x800D474C, 0x800D4778, {
    "jump-yields": {0x800D476C: 0x00001021},
    "target-not-stored": {0x800D4760: 0},
}))
