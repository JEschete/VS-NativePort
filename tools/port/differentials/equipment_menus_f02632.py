"""F02632 _equipShieldMenu (MENU3.PRG 16C.c:683), typed Menu::Inventory::UpdateShieldMenu."""

from equipment_menus_common import SHIELD, configure

globals().update(
    configure(
        2632,
        SHIELD,
        0x80107620,
        0x80107B14,
        {
            "entry-bars-1": {0x801076D0: 0x24040001},
            "count-category-4": {0x80107738: 0x24040004},
            "list-shift-18": {0x80107934: 0x00101C80},
            "choice-sounds": {0x80107988: 0x24040001},
            "close-bars-3": {0x801079BC: 0x24040003},
            "shield-not-unequipped": {0x801079E0: 0x00000000},
            "none-model-kept": {0x801079F8: 0x00000000},
            "shield-not-marked": {0x80107A2C: 0x00000000},
            "model-offset-7": {0x80107A34: 0x92230007},
            "material-offset-29": {0x80107A44: 0x92230029},
            "unarmed-held-ignored": {0x80107A50: 0x00000000},
            "hands-index-shifted": {0x80107A68: 0x90620000},
            "two-handed-ignored": {0x80107A70: 0x10000003},
            "model-category-1": {0x80107AAC: 0x24040001},
            "reload-busy-ignored": {0x80107AB0: 0x00000000},
        },
    )
)