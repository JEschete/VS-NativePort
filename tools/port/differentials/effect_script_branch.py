"""F01516 func_800D46DC (BATTLE.PRG 5BF94.c:7506), typed effect script branch."""

from effect_script_differential import BRANCH, configure

globals().update(configure(1516, BRANCH, 0x800D46DC, 0x800D4720, {
    "branch-inverted": {0x800D46E8: 0x14800005},
    "skip-three": {0x800D4708: 0x24420003},
}))
