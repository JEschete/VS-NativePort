"""F01429 func_800CEF38 (BATTLE.PRG 5BF94.c:5391), typed track requeue."""

from primary_effect_differential import REQUEUE, configure

globals().update(
    configure(
        1429,
        REQUEUE,
        0x800CEF38,
        0x800CEF64,
        {
            "started-incremented": {0x800CEF44: 0x24420001},
            "pending-toggled": {0x800CEF58: 0x00621826},
            "next-track-bit": {0x800CEF4C: 0x24020002},
        },
    )
)
