"""F02855 _itemIncludesItemsInCategory (MENUB.PRG 260.c:968), typed loot rows."""

from loot_rows_differential import COUNT_CATEGORY, configure

globals().update(configure(2855, COUNT_CATEGORY, 0x80108264, 0x8010837C, {
    "weapon-parts-uncounted": {0x80108294: 0x24020000},
    "empty-sockets-counted": {0x80108304: 0x24020001},
    "shield-sockets-shifted": {0x80108358: 0x9042002D},
}))
