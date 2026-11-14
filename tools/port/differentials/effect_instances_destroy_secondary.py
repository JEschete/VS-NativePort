"""F01444 func_800CF614 (BATTLE.PRG 5BF94.c:5617), typed secondary effect instance destruction."""

from effect_instances_differential import DESTROY_SECONDARY, configure

globals().update(
    configure(
        1444,
        DESTROY_SECONDARY,
        0x800CF614,
        0x800CF694,
        {
            "first-kept": {0x800CF63C: 0x00000000},
            "middle-kept": {0x800CF648: 0x00000000},
            "particles-kept": {0x800CF668: 0x00000000},
            "token-halfword": {0x800CF670: 0x96040014},
            "token-kept": {0x800CF674: 0x00000000},
            "instance-kept": {0x800CF67C: 0x00000000},
        },
    )
)
