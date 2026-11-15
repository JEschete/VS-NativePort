"""F00120 func_80073484 (BATTLE.PRG 146C.c:4638), typed Effects::EndAshleyStandaloneEffect."""

from action_callbacks_common import END, configure

globals().update(
    configure(
        120,
        END,
        0x80073484,
        0x800734C0,
        {
            "exploration-skipped": {0x8007348C: 0x00000000},
            "tick-3": {0x80073498: 0x24020003},
            "hud-hidden": {0x8007349C: 0x24040001},
            "minimap-hidden": {0x800734AC: 0x00002021},
        },
    )
)
