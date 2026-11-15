"""F01142 func_800BA2B4 (BATTLE.PRG 4A0A8.c:2274), typed Event::ScriptSetBackgroundParallax."""

from event_room_commands_common import PARALLAX, configure

globals().update(
    configure(
        1142,
        PARALLAX,
        0x800BA2B4,
        0x800BA2E0,
        {
            "first-unsigned": {0x800BA2BC: 0x90820001},
            "second-unsigned": {0x800BA2C0: 0x90850002},
            "first-scale-6": {0x800BA2C4: 0x00022180},
        },
    )
)
