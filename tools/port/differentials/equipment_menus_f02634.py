"""F02634 _dispatchEquipMenu (MENU3.PRG 16C.c:1028), typed Menu::Inventory::DispatchEquipmentMenu."""

from equipment_menus_common import DISPATCH, configure

globals().update(
    configure(
        2634,
        DISPATCH,
        0x80108010,
        0x80108088,
        {
            "row-0-reselects": {0x80108014: 0x00000000},
            "accessory-row-9": {0x8010801C: 0x24020009},
            "armor-from-row-4": {0x80108024: 0x28820004},
            "accessory-handler-2": {0x8010802C: 0x24020002},
            "armor-part-offset-1": {0x80108040: 0x2484FFFF},
            "armor-handler-from-row": {0x80108048: 0xA04496A3},
        },
    )
)