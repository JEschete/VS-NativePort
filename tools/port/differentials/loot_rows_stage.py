"""F02858 _applyLootList (MENUB.PRG 260.c:1124), typed loot rows."""

from loot_rows_differential import STAGE, configure

globals().update(configure(2858, STAGE, 0x801086F4, 0x801088D4, {
    "blade-row-as-grip": {0x80108754: 0x24040002},
    "second-armor-skipped": {0x801087EC: 0x2A220001},
    "misc-count-ignored": {0x80108880: 0x00000000},
}))
