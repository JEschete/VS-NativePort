"""F01451 func_800CF92C (BATTLE.PRG 5BF94.c:5714), the trail texel clamp."""

from bone_trail_differential import CLAMP, configure

globals().update(
    configure(
        1451,
        CLAMP,
        0x800CF92C,
        0x800CF988,
        {
            "u-low": {0x800CF944: 0x24030000},
            "u-high": {0x800CF950: 0x240300FD},
            "v-high": {0x800CF974: 0x240500FF},
            "v-shift": {0x800CF970: 0x000511C0},
            "u-sign": {0x800CF930: 0x00021402},
        },
    )
)
