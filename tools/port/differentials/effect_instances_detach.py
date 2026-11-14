"""F01446 func_800CF70C (BATTLE.PRG 5BF94.c:5647), typed effect renderer detach."""

from effect_instances_differential import DETACH, configure

globals().update(
    configure(
        1446,
        DETACH,
        0x800CF70C,
        0x800CF7A8,
        {
            "state-kept": {0x800CF734: 0x00000000},
            "first-kept": {0x800CF754: 0x00000000},
            "middle-kept": {0x800CF788: 0x00000000},
            "next-dropped": {0x800CF780: 0x00001021},
            "slot-kept": {0x800CF78C: 0x00000000},
        },
    )
)
