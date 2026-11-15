"""F01388 _loadScreff2 (BATTLE/BATTLE.PRG 5BF94.c:3873), typed Effects::ScreenDistortion::LoadScreenOverlay."""

from screen_overlay_lifetime_common import LOAD, configure

globals().update(
    configure(
        1388,
        LOAD,
        0x800CB678,
        0x800CB708,
        {
            "loading-2": {0x800CB69C: 0x24020002},
            "loaded-state-3": {0x800CB6DC: 0x24020003},
            "keeps-loading": {0x800CB6F4: 0x00000000},
            "size-0x800": {0x800CB6A4: 0x24020800},
        },
    )
)
