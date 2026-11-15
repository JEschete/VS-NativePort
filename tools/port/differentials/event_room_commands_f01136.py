"""F01136 func_800BA0E4 (BATTLE.PRG 4A0A8.c:2211), typed Event::ScriptSetParticleVariationEnabled."""

from event_room_commands_common import PARTICLE_VARIATION, configure

globals().update(
    configure(
        1136,
        PARTICLE_VARIATION,
        0x800BA0E4,
        0x800BA108,
        {
            "operand-not-inverted": {0x800BA0F4: 0x00000000},
            "inverts-bit-1": {0x800BA0F4: 0x38840002},
            "reads-third-byte": {0x800BA0EC: 0x90840002},
        },
    )
)
