"""F01137 func_800BA108 (BATTLE.PRG 4A0A8.c:2217), typed Event::ScriptMuteEnvironmentSound."""

from event_room_commands_common import MUTE_SOUND, configure

globals().update(
    configure(
        1137,
        MUTE_SOUND,
        0x800BA108,
        0x800BA194,
        {
            "mutes-while-muted": {0x800BA12C: 0x00000000},
            "mutes-with-sound-0": {0x800BA150: 0x24040000},
            "saved-sound-dropped": {0x800BA15C: 0x00000000},
            "restore-keeps-muted": {0x800BA17C: 0x00000000},
        },
    )
)
