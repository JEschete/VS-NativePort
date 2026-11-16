"""F02656 MENU4.PRG _initEquipmentScreen (120.c:528), typed Menu::Status::BeginVitalsTransition."""

from status_switching_common import INIT, configure

globals().update(
    configure(
        2656,
        INIT,
        0x80103608,
        0x80103688,
        {
            "hud-hidden-skipped": {0x8010363C: 0x00000000},
            "await-delay-three": {0x80103648: 0x24030003},
            "swap-delay-three": {0x80103668: 0x24020003},
            "hide-as-swap": {0x80103624: 0x24020002},
            "shown-unchanged": {0x80103678: 0x00000000},
        },
    )
)
