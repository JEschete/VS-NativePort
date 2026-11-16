"""F02629 _populateUiAccessory (MENU3.PRG 16C.c:311), typed Menu::Inventory::SetAccessoryPreviewStats."""

from equipment_rows_common import ACCESSORY, configure

globals().update(
    configure(
        2629,
        ACCESSORY,
        0x80106BB4,
        0x80106C94,
        {
            "column-mask-3": {0x80106C14: 0x30C20003},
            "types-from-classes": {0x80106C3C: 0x94420040},
            "affinities-from-classes": {0x80106C30: 0x94430040},
            "armor-stride-20": {0x80106BDC: 0x00052880},
            "fifteen-columns": {0x80106C48: 0x28C2000F},
            "known-3": {0x80106C64: 0x24070003},
            "subtype-31": {0x80106C70: 0x2403001F},
            "pages-6": {0x80106C68: 0x24040006},
        },
    )
)
