"""F02713 renderMapLabels (MENU5.PRG 4D8.c:2450), typed Menu::map::DrawLabels."""

from map_drawing_common import LABELS, configure

globals().update(
    configure(
        2713,
        LABELS,
        0x801082A8,
        0x8010839C,
        {
            "hidden-state-4": {0x801082B4: 0x24020004},
            "hide-ignored": {0x801082D4: 0x00000000},
            "door-pass-area-kind": {0x801082FC: 0x10400007},
            "door-id-is-kind": {0x8010830C: 0x86060004},
            "area-stride-4": {0x80108384: 0x26100004},
        },
    )
)
