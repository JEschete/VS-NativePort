"""F02836 vs_menu9_loadZud (MENU9.PRG 57BC.c:10), typed Menu::Score::LoadEnemyPreview."""

from encyclopedia_preview_common import LOAD, configure

globals().update(
    configure(
        2836,
        LOAD,
        0x80107FBC,
        0x80108098,
        {
            "pose-step-kept": {0x80108078: 0x00000000},
            "step-advance-two": {0x80108030: 0x24420002},
            "pose-slot-two": {0x80108048: 0x24040002},
            "posed-returns-one": {0x80108070: 0x24020001},
            "weapon-drawn-bit-22": {0x8010804C: 0x3C030040},
        },
    )
)
