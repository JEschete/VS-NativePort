"""F02834 _renderEnemyNavigation, MENU9.PRG 238.c:2782."""

from gazette_terminal_common import NAVIGATION, configure

globals().update(configure(2834, NAVIGATION, 0x80107A98, 0x80107C34, {
    "draw-hidden-navigation": {0x80107AB8: 0},
    "normal-clut-transition": {0x80107AC8: 0x241337F9},
    "transition-clut-normal": {0x80107AD0: 0x241337F8},
    "right-text-x-285": {0x80107AE4: 0x34A5011D},
    "right-first-half-bounce": {0x80107B14: 0x2444011F},
    "right-second-half-bounce": {0x80107B24: 0x24440125},
    "left-text-x-33": {0x80107B34: 0x34A50021},
    "left-second-half-bounce": {0x80107B70: 0x24440019},
    "arrow-inset-seven": {0x80107B78: 0x2484FFF9},
    "arrow-width-fifteen": {0x80107B84: 0x2483000F},
    "arrow-bottom-111": {0x80107BBC: 0x2402006F},
    "arrow-page-dim": {0x80107BEC: 0x2402002C},
    "omit-clut": {0x80107BFC: 0},
    "tinted-instead-of-raw": {0x80107C00: 0x34630000},
    "overlay-instead-of-scene-ordering": {0x80107C08: 0x8E240008},
    "omit-primitive-cursor": {0x80107C14: 0},
}))
