"""F01427 func_800CEEBC (BATTLE.PRG 5BF94.c:5373), typed track-start check."""

from primary_effect_differential import CAN_START, configure

globals().update(
    configure(
        1427,
        CAN_START,
        0x800CEEBC,
        0x800CEF0C,
        {
            "phase-6-plays": {0x800CEECC: 0x2C420003},
            "started-swapped": {0x800CEEEC: 0x0082102A},
            "pending-ignored": {0x800CEF00: 0x24050001},
        },
    )
)
