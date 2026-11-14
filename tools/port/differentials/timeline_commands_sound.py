"""F01582 func_800D6628 (BATTLE.PRG 5BF94.c:8285), typed emitter reaction sound."""

from timeline_commands_differential import SOUND, configure

globals().update(
    configure(
        1582,
        SOUND,
        0x800D6628,
        0x800D66CC,
        {
            "variant-limit-5": {0x800D6650: 0x2CA20005},
            "repeats-allowed": {0x800D6698: 0x00000000},
            "last-not-stored": {0x800D66A8: 0x00000000},
            "sound-reads-target-actor": {0x800D665C: 0x92240D38},
        },
    )
)
