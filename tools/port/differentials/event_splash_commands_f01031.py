"""F01031 func_800B6724 (BATTLE.PRG 4A0A8.c:609), typed Event::ScriptWaitForSplashFile."""

from event_splash_commands_common import WAIT_FILE, configure

globals().update(
    configure(
        1031,
        WAIT_FILE,
        0x800B6724,
        0x800B6744,
        {
            "reads-enabled-bit": {0x800B6738: 0x00021002},
            "returns-two-bits": {0x800B6740: 0x30420003},
            "reads-second-layer": {0x800B6730: 0x8C4203F8},
        },
    )
)
