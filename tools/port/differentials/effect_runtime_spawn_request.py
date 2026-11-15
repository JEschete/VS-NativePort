"""F01437 func_800CF458 (BATTLE.PRG 5BF94.c:5541), typed secondary effect request spawn."""

from effect_runtime_differential import SPAWN_REQUEST, configure

globals().update(
    configure(
        1437,
        SPAWN_REQUEST,
        0x800CF458,
        0x800CF478,
        {
            "parameter-dropped": {0x800CF464: 0x00003021},
            "request-shifted": {0x800CF464: 0x2484000C},
            "token-7-bits": {0x800CF46C: 0x3042007F},
        },
    )
)
