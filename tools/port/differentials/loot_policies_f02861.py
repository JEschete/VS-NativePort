"""F02861 _populateLootItem (MENUB.PRG 260.c:1319), typed Menu::Loot::ShowLootDescription."""

from loot_policies_common import DESCRIPTION, configure

globals().update(
    configure(
        2861,
        DESCRIPTION,
        0x80108E78,
        0x801090A4,
        {
            "backup-skipped": {0x80108EB8: 0x00000000},
            "restore-skipped": {0x80109080: 0x00000000},
            "info-from-row-type": {0x80109064: 0x8FA40018},
            "item-index-from-category": {0x80108EB4: 0x90530000},
            "grip-stride-32": {0x80108F78: 0x00131140},
            "misc-stride-8": {0x80109040: 0x001310C0},
            "category-limit-6": {0x80108EE8: 0x2C620006},
        },
    )
)
