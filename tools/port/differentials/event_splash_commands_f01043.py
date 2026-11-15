"""F01043 vs_battle_script_loadGim (BATTLE.PRG 4A0A8.c:851), typed Event::ScriptLoadSplashImage."""

from event_splash_commands_common import LOAD, configure

globals().update(
    configure(
        1043,
        LOAD,
        0x800B6FD8,
        0x800B7014,
        {
            "id-reads-second": {0x800B6FEC: 0x26040002},
            "id-unsigned": {0x800B6FFC: 0x00022402},
            "page-signed": {0x800B6FF4: 0x82050003},
            "returns-hold": {0x800B7008: 0x24020001},
        },
    )
)
