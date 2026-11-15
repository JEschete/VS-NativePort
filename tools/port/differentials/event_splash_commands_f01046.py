"""F01046 func_800B71C4 (BATTLE.PRG 4A0A8.c:905), typed Event::ScriptFreeSplashImage."""

from event_splash_commands_common import FREE, configure

globals().update(
    configure(
        1046,
        FREE,
        0x800B71C4,
        0x800B71F4,
        {
            "keeps-images": {0x800B71CC: 0x00000000},
            "yields-when-skippable": {0x800B71E0: 0x38420001},
            "returns-finish": {0x800B71E8: 0x00021040},
        },
    )
)
