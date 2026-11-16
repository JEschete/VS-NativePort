"""F02627 _getSelectedItemRow (MENU3.PRG 16C.c:101), typed Menu::Inventory::SelectedEquipmentRow."""

from equipment_rows_common import SELECTED, configure

globals().update(
    configure(
        2627,
        SELECTED,
        0x80106574,
        0x8010659C,
        {
            "ignores-open-list": {0x80106580: 0x00000000},
            "open-returns-0": {0x80106598: 0x24020000},
            "reads-state-word": {0x80106588: 0x8C429764},
        },
    )
)
