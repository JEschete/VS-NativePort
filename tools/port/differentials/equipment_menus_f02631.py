"""F02631 _equipWeaponMenu (MENU3.PRG 16C.c:503), typed Menu::Inventory::UpdateWeaponMenu."""

from equipment_menus_common import WEAPON, configure

globals().update(
    configure(
        2631,
        WEAPON,
        0x80107148,
        0x80107620,
        {
            "entry-pages-6": {0x801071E8: 0x24040006},
            "entry-bars-1": {0x801071F0: 0x24040001},
            "equipped-shift-18": {0x80107394: 0x00101C80},
            "list-shift-18": {0x80107420: 0x00101C80},
            "choice-sounds": {0x80107474: 0x24040001},
            "close-bars-3": {0x801074A8: 0x24040003},
            "back-state-2": {0x801074C0: 0x24020002},
            "unarmed-category-9": {0x801074E8: 0x24020009},
            "weapon-not-marked": {0x8010751C: 0x00000000},
            "model-from-category": {0x80107544: 0x90440003},
            "material-offset-29": {0x80107550: 0x90430029},
            "hands-index-shifted": {0x8010756C: 0x90620000},
            "two-handed-ignored": {0x80107574: 0x10000003},
            "reload-busy-ignored": {0x801075BC: 0x00000000},
        },
    )
)