"""F02638 func_80109320 (MENU3.PRG 68C4.c:87), typed Menu::Inventory::IsTwoHandedWeapon."""

from equipment_rows_common import TWO_HANDED, configure

globals().update(
    configure(
        2638,
        TWO_HANDED,
        0x80109320,
        0x80109344,
        {
            "threshold-4": {0x80109320: 0x28820004},
            "category-9": {0x8010932C: 0x24020009},
            "bit-1": {0x80109334: 0x30820002},
            "eight-one-handed": {0x80109338: 0x24020000},
        },
    )
)
