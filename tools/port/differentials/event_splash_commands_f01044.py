"""F01044 func_800B7014 (BATTLE.PRG 4A0A8.c:857), typed Event::ScriptConfigureSplashLayer."""

from event_splash_commands_common import CONFIGURE, configure

globals().update(
    configure(
        1044,
        CONFIGURE,
        0x800B7014,
        0x800B70E8,
        {
            "skips-present-set": {0x800B7024: 0x1000002B},
            "keeps-tween": {0x800B7050: 0x00000000},
            "routine-three-bits": {0x800B7074: 0x30840007},
            "x-unsigned": {0x800B708C: 0x90C20002},
            "y-not-doubled": {0x800B70A4: 0x00000000},
            "scale-shift-six": {0x800B70B4: 0x00021180},
            "brightness-kept": {0x800B70C4: 0x00000000},
            "activates-layer-zero": {0x800B70D0: 0x00002021},
            "returns-hold": {0x800B70D4: 0x24020001},
        },
    )
)
