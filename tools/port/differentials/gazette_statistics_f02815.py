"""F02815 _renderTitleMenu, MENU9.PRG 238.c:1756."""

from gazette_statistics_common import RENDER_TITLES, configure

globals().update(configure(2815, RENDER_TITLES, 0x8010579C, 0x80105A0C, {
    "select-page-only": {0x801057F4: 0x00808821},
    "skip-information": {0x80105814: 0},
    "always-send-information": {0x80105808: 0},
    "selected-zero": {0x8010581C: 0xA2400000},
    "highlight-locked": {0x80105828: 0},
    "highlight-seven": {0x801057C8: 0x24130007},
    "keep-unselected-flag": {0x80105838: 0},
    "fade-by-two": {0x80105848: 0x2442FFFE},
    "skip-last-selection": {0x80105858: 0x2A82001F},
    "presentation-times-24": {0x80105874: 0x0040F021},
    "entrance-offset-15": {0x8010589C: 0x27D3FFF1},
    "invert-row-clipping": {0x801058A4: 0x14400002},
    "cursor-at-step-seven": {0x801058BC: 0x24020007},
    "cursor-raise-seven": {0x801058C8: 0x2665FFF9},
    "cursor-inset-eleven": {0x801058D8: 0x2442FFF5},
    "skip-cursor-store": {0x801058E8: 0},
    "row-width-199": {0x80105900: 0x240700C7},
    "locked-font-two": {0x8010591C: 0x24020002},
    "skip-gradient-store": {0x8010592C: 0},
    "no-up-scroll": {0x80105944: 0},
    "last-page-23": {0x8010595C: 0x24020017},
    "no-down-scroll": {0x80105968: 0},
    "number-inset-thirteen": {0x8010597C: 0x2484FFF3},
    "dim-on-down-scroll": {0x80105984: 0x30E70002},
    "skip-title-number": {0x80105980: 0},
    "never-scroll-render": {0x80105990: 0x10000007},
    "only-seven-visible": {0x801059C4: 0x2A820007},
    "leave-row-active": {0x801059D8: 0},
}))
