"""F01577 func_800D6538 (BATTLE.PRG 5BF94.c:8255), typed timeline frame hold."""

from timeline_controls_differential import HOLD, configure

globals().update(
    configure(
        1577,
        HOLD,
        0x800D6538,
        0x800D6554,
        {
            "hold-sets-bit-2": {0x800D6548: 0x34420002},
            "hold-reads-track-count": {0x800D6540: 0x90620000},
        },
    )
)
