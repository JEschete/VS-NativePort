"""F01574 func_800D6448 (BATTLE.PRG 5BF94.c:8240), typed transform weapon trail."""

from timeline_controls_differential import TRAIL, configure

globals().update(
    configure(
        1574,
        TRAIL,
        0x800D6448,
        0x800D64E4,
        {
            "color-from-texture": {0x800D64B8: 0x30E600FF},
            "bone-reads-actor": {0x800D649C: 0x92450050},
            "transform-stride-0x68": {0x800D6490: 0x000210C0},
            "restore-skipped": {0x800D64C4: 0x00000000},
        },
    )
)
