"""F02667 MENU4.PRG _getEquipmentListRowCount (120.c:1132), typed Menu::Status::EquipmentRowCount."""

from status_switching_common import ROWS, configure

globals().update(
    configure(
        2667,
        ROWS,
        0x8010455C,
        0x801045B8,
        {
            "three-fixed-rows": {0x80104594: 0x24420003},
            "accessory-ignored": {0x801045A8: 0x00001821},
            "accessory-low-byte": {0x801045A0: 0x90630338},
        },
    )
)
