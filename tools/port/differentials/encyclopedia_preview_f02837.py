"""F02837 vs_menu9_freeZud (MENU9.PRG 57BC.c:42), typed Menu::Score::FreeEnemyPreview."""

from encyclopedia_preview_common import FREE, configure

globals().update(
    configure(
        2837,
        FREE,
        0x80108098,
        0x801080C8,
        {
            "enemy-slot-two": {0x801080A4: 0x24040002},
            "companion-slot-17": {0x801080AC: 0x24040011},
            "buffer-kept": {0x801080B0: 0x00000000},
        },
    )
)
