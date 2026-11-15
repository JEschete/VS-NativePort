"""F01045 func_800B70E8 (BATTLE.PRG 4A0A8.c:879), typed Event::ScriptTweenSplashLayer."""

from event_splash_commands_common import TWEEN, configure

globals().update(
    configure(
        1045,
        TWEEN,
        0x800B70E8,
        0x800B71C4,
        {
            "selector-reads-sixth": {0x800B70F4: 0x90830006},
            "holds-without-set": {0x800B7118: 0x24020001},
            "ignores-running-tween": {0x800B7130: 0x00000000},
            "x-unsigned": {0x800B7138: 0x90820001},
            "scale-shift-six": {0x800B7170: 0x00021180},
            "brightness-signed": {0x800B7180: 0x80E30003},
            "easing-four-bits": {0x800B71A0: 0x3042000F},
            "elapsed-kept": {0x800B71B0: 0x00000000},
            "running-advances": {0x800B71C0: 0x24020000},
        },
    )
)
