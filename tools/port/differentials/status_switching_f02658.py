"""F02658 MENU4.PRG _switchCurrentActor (120.c:579), typed Menu::Status::AnimateActorSwitch."""

from status_switching_common import SWITCH, configure

globals().update(
    configure(
        2658,
        SWITCH,
        0x80103744,
        0x8010399C,
        {
            "sound-skipped": {0x80103778: 0x00000000},
            "reverse-ignored": {0x8010378C: 0x10000005},
            "slide-step-16": {0x8010384C: 0x24820010},
            "release-skipped": {0x80103858: 0x00000000},
            "forward-from-128": {0x80103888: 0x2402FF80},
            "name-unselected": {0x801037EC: 0x24030000},
            "complete-not-reported": {0x80103984: 0x24020000},
            "reverse-fade-kept": {0x80103908: 0x00000000},
        },
    )
)
