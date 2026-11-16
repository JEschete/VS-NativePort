"""F02657 MENU4.PRG _getNextValidActor (120.c:548), typed Menu::Status::FindNextInspectableActor."""

from status_switching_common import NEXT, configure

globals().update(
    configure(
        2657,
        NEXT,
        0x80103688,
        0x80103744,
        {
            "kind-limit-six": {0x80103708: 0x2C420006},
            "hidden-ignored": {0x80103710: 0x3C030000},
            "flags-of-slot-zero": {0x801036E4: 0x00002021},
            "present-only": {0x80103720: 0x00000000},
        },
    )
)
