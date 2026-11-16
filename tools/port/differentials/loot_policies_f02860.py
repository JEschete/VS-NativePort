"""F02860 _displayCurrentLoot (MENUB.PRG 260.c:1192), typed Menu::Loot::RenderLootRows."""

from loot_policies_common import ROWS, configure

globals().update(
    configure(
        2860,
        ROWS,
        0x80108938,
        0x80108E78,
        {
            "gradient-not-saved": {0x801089C4: 0x00000000},
            "visible-rows-7": {0x801089E8: 0x24080007},
            "grip-icon-base-11": {0x80108B70: 0x2463000B},
            "key-item-at-457": {0x80108D18: 0x2C4201C9},
            "slide-step-96": {0x80108D70: 0x00021140},
            "unselectable-inverted": {0x80108D4C: 0x0002102B},
            "scroll-down-at-6": {0x80108DA0: 0x24020006},
            "chosen-state-1": {0x80108DE8: 0x24020001},
            "gradient-index-not-saved": {0x80108E6C: 0x00000000},
        },
    )
)
