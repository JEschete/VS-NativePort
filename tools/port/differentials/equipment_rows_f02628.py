"""F02628 _processEquipSubMenu (MENU3.PRG 16C.c:111), typed Menu::Inventory::UpdateEquipmentChoices."""

from equipment_rows_common import UPDATE, configure

globals().update(
    configure(
        2628,
        UPDATE,
        0x8010659C,
        0x80106BB4,
        {
            "slide-x-319": {0x801065F0: 0x2405013F},
            "slide-target-170": {0x8010662C: 0x240200AA},
            "slide-state-3": {0x80106628: 0x24040003},
            "outset-bias-99": {0x8010666C: 0x2442FF9D},
            "scrollable-after-7-rows": {0x801066A4: 0x2C420008},
            "idle-not-awaited": {0x80106720: 0x00000000},
            "triangle-always-silent": {0x80106744: 0x10000003},
            "circle-ignores-unselectable": {0x80106784: 0x10000005},
            "cross-code-minus-1": {0x8010680C: 0x2402FFFF},
            "up-scrolls-at-3": {0x80106878: 0x24020003},
            "down-scrolls-at-5": {0x80106910: 0x24020005},
            "scroll-gradient-dropped": {0x80106A48: 0x00000000},
            "more-above-unset": {0x80106AF4: 0x00000000},
            "equipped-shift-18": {0x80106B28: 0x00101483},
            "cursor-lift-13": {0x80106B68: 0x2631FFF3},
            "cursor-x-9c": {0x80106B64: 0x3451009C},
            "first-choice-sound-21": {0x801067C0: 0x24050021},
        },
    )
)
