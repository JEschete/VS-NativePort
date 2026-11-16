"""F04162 _initMenuState (MENUE.PRG 494.c:219), typed Menu::manual::InitHelpState."""

from manual_screen_common import INIT, configure

globals().update(
    configure(
        4162,
        INIT,
        0x801030A4,
        0x80103110,
        {
            "other-setting": {0x801030A8: 0x90420020},
            "clears-always": {0x801030B0: 0x00000000},
            "never-clears": {0x801030B0: 0x10000005},
            "no-clear-call": {0x801030C0: 0x00000000},
            "clears-0x20": {0x801030C4: 0x24050020},
            "page-0x1C": {0x801030C8: 0x2404001C},
            "two-pages": {0x801030CC: 0x24050002},
            "owner-4": {0x801030D4: 0x24060004},
            "returns-0": {0x801030D8: 0x24020000},
            "step-kept": {0x801030E4: 0x00000000},
            "scroll-kept": {0x801030EC: 0x00000000},
            "lines-kept": {0x801030F4: 0x00000000},
            "text-kept": {0x801030FC: 0x00000000},
            "assets-kept": {0x80103104: 0x00000000},
        },
    )
)
