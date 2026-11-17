"""F02824 _setStatText, MENU9.PRG 238.c:2201."""

from gazette_statistics_common import CREATE_STATS, configure

globals().update(configure(2824, CREATE_STATS, 0x80106780, 0x80106808, {
    "statistic-x-fifteen": {0x8010678C: 0x240A000F},
    "highlight-disabled": {0x801067A0: 0x24090000},
    "title-offset-byte-stride": {0x801067BC: 0x00021000},
    "skip-title-store": {0x801067C4: 0},
    "keep-animation": {0x801067D0: 0},
    "skip-number-store": {0x801067D4: 0},
    "description-offset-byte-stride": {0x801067D8: 0x00021000},
    "skip-description-store": {0x801067E0: 0},
    "keep-selection": {0x801067E4: 0},
    "keep-disabled": {0x801067E8: 0},
    "only-22-statistics": {0x801067F4: 0x28620016},
}))
