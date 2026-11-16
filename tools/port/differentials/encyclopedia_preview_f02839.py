"""F02839 vs_menu9_loadZudFile (MENU9.PRG 592C.c:171), typed Menu::Score::QueueEnemyModelRead."""

from encyclopedia_preview_common import QUEUE_READ, configure

globals().update(
    configure(
        2839,
        QUEUE_READ,
        0x8010812C,
        0x8010823C,
        {
            "table-stride-eight": {0x80108140: 0x001118C0},
            "companion-size-0x3300": {0x80108200: 0x24043300},
            "companion-loop-five": {0x801081E4: 0x28820005},
            "equipped-size-0x2400": {0x80108214: 0x24042400},
        },
    )
)
