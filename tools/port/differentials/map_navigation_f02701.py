"""F02701 _updateRoomIndex (MENU5.PRG 4D8.c:1862), typed Menu::map::SelectAdjacentVisitedRoom."""

from map_navigation_common import ADJACENT, configure

globals().update(
    configure(
        2701,
        ADJACENT,
        0x80106618,
        0x801066E0,
        {
            "direction-inverted": {0x80106640: 0x1C80000A},
            "next-from-previous": {0x80106660: 0x84620020},
            "sound-always": {0x801066C0: 0x00000000},
            "sound-id": {0x801066C4: 0x2404007F},
            "sound-volume": {0x801066CC: 0x2405000C},
        },
    )
)
