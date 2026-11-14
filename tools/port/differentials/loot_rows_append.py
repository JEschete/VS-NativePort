"""F02854 _addLootToList (MENUB.PRG 260.c:950), typed loot rows."""

from loot_rows_differential import APPEND, configure

globals().update(configure(2854, APPEND, 0x8010822C, 0x80108264, {
    "slide-step-kept": {0x80108260: 0x00000000},
    "slot-not-one-based": {0x80108234: 0x00000000},
}))
