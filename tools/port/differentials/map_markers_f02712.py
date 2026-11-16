"""F02712 _addMapLabel (MENU5.PRG 4D8.c:2441), typed Menu::map::QueueLabel."""

from map_markers_common import QUEUE, configure

globals().update(
    configure(
        2712,
        QUEUE,
        0x80108274,
        0x801082A8,
        {
            "count-step-2": {0x8010828C: 0x25080002},
            "x-from-y": {0x80108290: 0xA4450000},
            "id-from-kind": {0x8010829C: 0xA4460006},
            "stride-4": {0x80108284: 0x00081080},
        },
    )
)
