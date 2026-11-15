"""F01198 func_800BB450 (BATTLE/BATTLE.PRG 4A0A8.c:2793), typed Event::CommandSetScreenCapture."""

from event_screen_tweens_common import CAPTURE, configure

globals().update(
    configure(
        1198,
        CAPTURE,
        0x800BB450,
        0x800BB474,
        {
            "enable-byte-2": {0x800BB458: 0x90840002},
            "returns-1": {0x800BB468: 0x24020001},
        },
    )
)
