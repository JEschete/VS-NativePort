"""F02863 _organizeInventory (MENUB.PRG 260.c:1510), typed Menu::Loot::InitOrganizeMenu and UpdateOrganizeMenu."""

from loot_policies_common import ORGANIZE, configure

globals().update(
    configure(
        2863,
        ORGANIZE,
        0x80109444,
        0x80109750,
        {
            "shield-not-disassembled": {0x80109494: 0x24020004},
            "key-item-at-459": {0x801094D0: 0x2C4201CB},
            "action-text-plus-one": {0x80109514: 0x24420005},
            "organize-row-y-a3": {0x80109598: 0x240600A3},
            "cancel-selects": {0x80109644: 0x30420000},
            "selected-not-advanced": {0x80109688: 0x00000000},
            "toggle-without-action": {0x801096A0: 0x00000000},
            "cursor-base-9": {0x8010972C: 0x24A50009},
        },
    )
)
