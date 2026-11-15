"""F01126 func_800B9BF8 (BATTLE.PRG 4A0A8.c:2110), typed Event::ScriptWaitForScreenTransition."""

from event_screen_commands_common import WAIT_TRANSITION, configure

globals().update(
    configure(
        1126,
        WAIT_TRANSITION,
        0x800B9BF8,
        0x800B9C18,
        {
            "never-holds": {0x800B9C0C: 0x0040102B},
            "returns-phase": {0x800B9C0C: 0x00401021},
        },
    )
)
