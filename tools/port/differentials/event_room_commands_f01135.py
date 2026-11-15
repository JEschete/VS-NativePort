"""F01135 func_800B9FC0 (BATTLE.PRG 4A0A8.c:2180), typed Event::ScriptSetEnvironmentParticles."""

from event_room_commands_common import ENVIRONMENT_PARTICLES, configure

globals().update(
    configure(
        1135,
        ENVIRONMENT_PARTICLES,
        0x800B9FC0,
        0x800BA0E4,
        {
            "strength-base-dropped": {0x800BA02C: 0x24A50000},
            "direction-not-sign-extended": {0x800BA08C: 0x00021402},
            "short-wrap-half-turn": {0x800BA0A4: 0x24630800},
            "elapsed-kept": {0x800BA0C4: 0x00000000},
        },
    )
)
