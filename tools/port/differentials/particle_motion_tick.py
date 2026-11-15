"""F01535 func_800D4C18 (BATTLE.PRG 5BF94.c:7658), the effect particle tick."""

from particle_motion_differential import TICK, configure

globals().update(
    configure(
        1535,
        TICK,
        0x800D4C18,
        0x800D4D44,
        {
            "age-step": {0x800D4C64: 0x24420002},
            "tick-event": {0x800D4C88: 0x24A50000},
            "timer-branch": {0x800D4CA8: 0x1040001D},
            "lifetime-branch": {0x800D4CE0: 0x1040000F},
            "end-event": {0x800D4CE8: 0x9202006D},
        },
    )
)
