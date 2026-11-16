"""F02700 _getCurrentRoomIndex (MENU5.PRG 4D8.c:1834), typed Menu::map::InitialRoomOf."""

from map_navigation_common import INITIAL, configure

globals().update(
    configure(
        2700,
        INITIAL,
        0x80106570,
        0x80106618,
        {
            "minimap-room-one": {0x8010657C: 0x24450010},
            "zone-ignored": {0x801065A0: 0x00000000},
            "map-ignored": {0x801065B0: 0x00000000},
            "current-not-stored": {0x801065BC: 0x00000000},
            "fallback-current-zero": {0x801065D8: 0x24020000},
            "visited-ignored": {0x801065F0: 0x00000000},
            "fallback-room-one": {0x8010660C: 0x24020001},
        },
    )
)
