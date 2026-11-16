"""F02637 _loadObjectData (MENU3.PRG 68C4.c:12), typed Menu::Inventory::ReloadEquipmentModels."""

from equipment_rows_common import RELOAD, configure

globals().update(
    configure(
        2637,
        RELOAD,
        0x801090C4,
        0x80109320,
        {
            "shield-releases-weapon": {0x80109148: 0x24040000},
            "shield-stance-11": {0x80109174: 0x2404000B},
            "always-queues-animations": {0x80109188: 0x00000000},
            "two-hand-checks-slot-2": {0x801091A4: 0x8C424590},
            "queued-drawn": {0x801091E0: 0x24050001},
            "idle-animation-ignored": {0x801091FC: 0x00000000},
            "clone-slot-1": {0x8010922C: 0x24040001},
            "keeps-drawn-bit": {0x8010925C: 0x00000000},
            "copy-0x540": {0x8010927C: 0x24060540},
            "flag-0x10": {0x8010929C: 0xA0400010},
            "settled-100": {0x801092DC: 0x24020064},
            "done-returns-1": {0x801092F0: 0x24020001},
        },
    )
)
