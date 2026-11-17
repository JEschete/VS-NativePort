"""F02829 _printBossRushMenuRow, MENU9.PRG 238.c:2461."""

from gazette_times_common import CREATE, configure

globals().update(configure(2829, CREATE, 0x80106F9C, 0x80107090, {
    "row-x-193": {0x80106FA8: 0x240E00C1},
    "missing-time-sentinel": {0x80106FF4: 0x3C020081},
    "do-not-mask-difficulty-round": {0x80106FF8: 0x00601821},
    "disabled-flag-zero": {0x80107004: 0xA0A00002},
    "missing-title-entry": {0x80107008: 0x95220006},
    "missing-description-entry": {0x8010701C: 0x95220008},
    "present-disabled": {0x80107034: 0xA0AC0002},
    "keep-selection": {0x8010706C: 0x00000000},
    "no-stagger": {0x80107070: 0xA0AC0001},
    "style-zero": {0x80107074: 0xA0A00000},
    "only-seven-trials": {0x8010707C: 0x28E20007},
}))
