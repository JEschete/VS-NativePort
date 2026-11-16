"""F02707 _renderDoorName (MENU5.PRG 4D8.c:2227), typed Menu::map::DrawDoorLabel."""

from map_markers_common import DOOR_LABEL, configure

globals().update(
    configure(
        2707,
        DOOR_LABEL,
        0x801079B8,
        0x80107A9C,
        {
            "pivot-187": {0x801079C8: 0x240200BB},
            "pivot-fallback-187": {0x801079F0: 0x241100BB},
            "pivot-test-inverted": {0x801079E8: 0x06200002},
            "leader-drop-9": {0x80107A08: 0x24E70009},
            "row-line-end-195": {0x80107A14: 0x240600C3},
            "background-8": {0x80107A5C: 0x24030008},
            "row-step-8": {0x80107A6C: 0x2463FFF8},
            "state-byte-1": {0x80107A80: 0xA0400001},
        },
    )
)
