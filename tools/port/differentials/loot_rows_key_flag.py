"""F02866 _setKeyFlag (MENUB.PRG 260.c:2108), typed loot rows."""

from loot_rows_differential import KEY_FLAG, configure

globals().update(configure(2866, KEY_FLAG, 0x8010A24C, 0x8010A280, {
    "first-key-item-flagless": {0x8010A250: 0x288201CB},
    "flag-base-shifted": {0x8010A25C: 0x248401B7},
}))
