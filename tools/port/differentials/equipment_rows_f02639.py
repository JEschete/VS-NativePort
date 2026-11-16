"""F02639 _populateDataSlot (MENU3.PRG 68C4.c:100), typed Menu::Inventory::QueueEquipmentModel."""

from equipment_rows_common import QUEUE, configure

globals().update(
    configure(
        2639,
        QUEUE,
        0x80109344,
        0x801093B8,
        {
            "zero-queued": {0x80109348: 0x00000000},
            "shield-from-95": {0x80109350: 0x2882005F},
            "weapon-texture-239": {0x80109360: 0x240700EF},
            "shield-texture-242": {0x80109368: 0x240700F2},
            "kind-2": {0x8010936C: 0x24020002},
            "slot-from-0x4c": {0x8010938C: 0x8C42004C},
            "sub-kind-index": {0x80109394: 0xA3A60020},
        },
    )
)
