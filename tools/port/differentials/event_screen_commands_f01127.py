"""F01127 func_800B9C18 (BATTLE.PRG 4A0A8.c:2112), typed Event::ScriptTogglePaletteHook."""

from event_screen_commands_common import PALETTE_HOOK, configure

globals().update(
    configure(
        1127,
        PALETTE_HOOK,
        0x800B9C18,
        0x800B9C58,
        {
            "reads-second": {0x800B9C20: 0x90820002},
            "hooks-swapped": {0x800B9C28: 0x14400005},
        },
    )
)
