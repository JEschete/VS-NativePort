"""F02633 _equipArmorMenu (MENU3.PRG 16C.c:856), typed Menu::Inventory::UpdateArmorMenu."""

from equipment_menus_common import ARMOR, configure

globals().update(
    configure(
        2633,
        ARMOR,
        0x80107B14,
        0x80108010,
        {
            "part-not-stored": {0x80107B4C: 0x00000000},
            "entry-scan-15": {0x80107B9C: 0x2A22000F},
            "index-not-stored": {0x80107BC4: 0x00000000},
            "entry-pages-6": {0x80107BCC: 0x24040006},
            "entry-bars-3": {0x80107BD4: 0x24040003},
            "entry-panel-2": {0x80107BDC: 0x24040002},
            "equipped-shift-18": {0x80107D9C: 0x00101C80},
            "part-category-next": {0x80107E10: 0x25089625},
            "list-shift-18": {0x80107E74: 0x00101C80},
            "silent-choices": {0x80107EC8: 0x24040000},
            "close-bars-3": {0x80107EFC: 0x24040003},
            "back-state-2": {0x80107F18: 0x24020002},
            "old-limb-kept": {0x80107F50: 0x00000000},
            "worn-limb-unset": {0x80107F94: 0x00000000},
            "part-not-zero-based": {0x80107FA0: 0x00000000},
        },
    )
)