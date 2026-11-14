"""F02853 _updateLootList (MENUB.PRG 260.c:907), typed loot rows."""

from loot_rows_differential import REFRESH, configure

globals().update(configure(2853, REFRESH, 0x801080F0, 0x8010822C, {
    "claim-state-not-normalized": {0x80108154: 0x00401021},
    "last-row-not-selected": {0x801081A4: 0x24020006},
    "scroll-not-advanced": {0x80108200: 0x00000000},
}))
