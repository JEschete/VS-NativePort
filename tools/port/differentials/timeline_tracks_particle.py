"""F01558 func_800D5700 (BATTLE.PRG 5BF94.c:7920), typed particle emission key timing."""

from timeline_tracks_differential import PARTICLE, configure

globals().update(
    configure(
        1558,
        PARTICLE,
        0x800D5700,
        0x800D5738,
        {
            "parameter-shift-16": {0x800D5720: 0x00021402},
            "parameter-mask-0x1f": {0x800D5724: 0x3042001F},
            "timer-byte-2": {0x800D572C: 0x90620002},
            "parameter-key-stride-8": {0x800D570C: 0x000210C0},
        },
    )
)
