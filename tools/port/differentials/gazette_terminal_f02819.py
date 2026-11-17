"""F02819 _renderEnemyMenu, MENU9.PRG 238.c:1972."""

from gazette_terminal_common import LIST, configure

globals().update(configure(2819, LIST, 0x80105F00, 0x801061F8, {
    "always-update-information": {0x80105F6C: 0},
    "skip-selected-store": {0x80105FC8: 0},
    "highlight-seven": {0x80105FD0: 0x24020007},
    "negative-highlight-treated-unsigned": {0x80105FD4: 0x96020000},
    "keep-other-selection": {0x80105FE0: 0},
    "highlight-decrements-two": {0x80105FE4: 0x2462FFFE},
    "only-seventy-seven-selections": {0x80105FF0: 0x2A42004D},
    "first-row-y-fifty-five": {0x80106020: 0x24150037},
    "slide-inset-fifteen": {0x80106054: 0x2511FFF1},
    "cursor-raised-seven": {0x80106084: 0x2625FFF9},
    "skip-cursor-animation-store": {0x8010609C: 0},
    "locked-color-two": {0x8010610C: 0x24020002},
    "skip-gradient-store": {0x8010611C: 0},
    "wrong-last-page": {0x80106144: 0x24020045},
    "zero-based-enemy-number": {0x80106168: 0x24C60000},
    "dim-from-second-scroll-bit": {0x80106170: 0x30E70002},
    "only-seven-visible-rows": {0x801061B0: 0x2A420007},
    "leave-framework-row-active": {0x801061C4: 0},
}))
