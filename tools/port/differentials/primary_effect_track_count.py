"""F01428 func_800CEF0C (BATTLE.PRG 5BF94.c:5382), typed playable track count."""

from primary_effect_differential import PLAYABLE, configure

globals().update(
    configure(
        1428,
        PLAYABLE,
        0x800CEF0C,
        0x800CEF38,
        {
            "count-phase-4-only": {0x800CEF1C: 0x2C420001},
            "count-reads-started": {0x800CEF2C: 0x8C435228},
        },
    )
)
