"""F01587 func_800D6894 (BATTLE.PRG 5BF94.c:8342), typed action-control track dispatch."""

from timeline_controls_differential import DISPATCH, configure

globals().update(
    configure(
        1587,
        DISPATCH,
        0x800D6894,
        0x800D6A18,
        {
            "frame-mask-8-bits": {0x800D68C0: 0x30A200FF},
            "waiting-returns-0": {0x800D68C8: 0x24020000},
            "command-shift-8": {0x800D68CC: 0x00051A02},
            "command-limit-15": {0x800D68D0: 0x2C62000F},
            "operands-swapped": {0x800D68D4: 0x90C50003},
            "advance-skipped": {0x800D6A00: 0x24020001},
        },
    )
)
