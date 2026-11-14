"""F01544 func_800D4F00 (BATTLE.PRG 5BF94.c:7749), typed particle colour curves."""

from effect_script_particle_differential import CURVES, configure

globals().update(configure(1544, CURVES, 0x800D4F00, 0x800D4FB4, {
    "five-bit-definition": {0x800D4F3C: 0x3224001F},
    "red-shift-5": {0x800D4F64: 0x00118942},
    "green-mask-7": {0x800D4F6C: 0x32030007},
    "blue-shift-3": {0x800D4F90: 0x000528C2},
}))
