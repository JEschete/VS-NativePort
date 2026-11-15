"""F01047 func_800B71F4 (BATTLE.PRG 4A0A8.c:911), typed Event::ScriptWaitForSplashTween."""

from event_splash_commands_common import WAIT_TWEEN, configure

globals().update(
    configure(
        1047,
        WAIT_TWEEN,
        0x800B71F4,
        0x800B7218,
        {
            "reads-second-elapsed": {0x800B7200: 0x90440016},
            "reads-fourth-record": {0x800B7204: 0x9042003F},
            "ignores-third-layer": {0x800B720C: 0x00601025},
            "returns-durations": {0x800B7214: 0x00401021},
        },
    )
)
