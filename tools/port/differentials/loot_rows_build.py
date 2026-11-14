"""F02859 _applyAllLootLists (MENUB.PRG 260.c:1174), typed loot rows."""

from loot_rows_differential import BUILD, configure

globals().update(configure(2859, BUILD, 0x801088D4, 0x80108938, {
    "inventory-not-cleared": {0x801088EC: 0x00000000},
    "stops-after-first-record": {0x8010890C: 0x10000006},
}))
