"""F01578 func_800D6554 (BATTLE.PRG 5BF94.c:8257), typed room object pause."""

from timeline_controls_differential import PAUSE, configure

globals().update(
    configure(
        1578,
        PAUSE,
        0x800D6554,
        0x800D6574,
        {
            "pause-skipped": {0x800D655C: 0x00000000},
        },
    )
)
