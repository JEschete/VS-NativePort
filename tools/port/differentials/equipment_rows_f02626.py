"""F02626 _populateMenuRows (MENU3.PRG 16C.c:39), typed Menu::Inventory::CreateEquipmentRows."""

from equipment_rows_common import CREATE, configure

globals().update(
    configure(
        2626,
        CREATE,
        0x80106390,
        0x80106574,
        {
            "name-12-halfwords": {0x80106448: 0x2A02000C},
            "description-45-halfwords": {0x801064B0: 0x2A02002D},
            "chunk-f9": {0x80106490: 0x240200F9},
            "missing-name-unterminated": {0x80106468: 0x00000000},
            "flags-not-stored": {0x80106414: 0x00000000},
            "title-y-19": {0x80106508: 0x24060013},
            "first-row-unselected": {0x80106540: 0x24030000},
            "equipped-shift-18": {0x8010654C: 0x00101483},
            "step-starts-2": {0x801063CC: 0x24020002},
        },
    )
)
