"""F01559 func_800D5738 (BATTLE.PRG 5BF94.c:7927), typed camera shake key timer."""

from timeline_tracks_differential import SHAKE, configure

globals().update(
    configure(
        1559,
        SHAKE,
        0x800D5738,
        0x800D5780,
        {
            "curve-shift-5": {0x800D5760: 0x00042142},
            "curve-mask-0x3f": {0x800D5768: 0x3084003F},
            "parameters-word-0": {0x800D5758: 0x8C440000},
            "timer-to-curve-frame": {0x800D576C: 0xA6020008},
        },
    )
)
