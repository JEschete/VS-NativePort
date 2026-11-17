"""F02830 _leaveBossRushMenu, MENU9.PRG 238.c:2488."""

from gazette_times_common import LEAVE, configure

globals().update(configure(2830, LEAVE, 0x80107090, 0x80107120, {
    "leave-idle-not-sliding": {0x801070A0: 0x24140001},
    "exit-x-319": {0x801070A8: 0x2413013F},
    "cancel-delay-254": {0x801070B0: 0x241200FE},
    "reverse-active-test": {0x801070D0: 0x10400006},
    "target-wrong-axis": {0x801070E8: 0xA453001A},
    "only-seven-trials": {0x801070F4: 0x2A220007},
}))
