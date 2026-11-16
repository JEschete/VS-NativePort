"""F02706 func_80107630 (MENU5.PRG 4D8.c:2180), typed Menu::map::DrawDoorStateMarker."""

from map_markers_common import DOOR_STATE, configure

globals().update(
    configure(
        2706,
        DOOR_STATE,
        0x80107630,
        0x801079B8,
        {
            "variant-always-0": {0x8010768C: 0x30460000},
            "name-not-decremented": {0x8010764C: 0x24D60000},
            "name-mask-ff": {0x801077BC: 0x32D600FF},
            "icon-clut-37f5": {0x801077D0: 0x240237F5},
            "caption-clut-37fd": {0x80107944: 0x240237FD},
            "caption-y-from-width": {0x80107804: 0x86240004},
            "label-kind-1": {0x80107974: 0x24060001},
        },
    )
)
