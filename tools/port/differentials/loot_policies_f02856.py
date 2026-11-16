"""F02856 _displayInventoryCapacities (MENUB.PRG 260.c:1015), typed Menu::Loot::RenderBagCapacities."""

from loot_policies_common import CAPACITIES, configure

globals().update(
    configure(
        2856,
        CAPACITIES,
        0x8010837C,
        0x801086DC,
        {
            "preview-row-ignored": {0x801083C4: 0x10000018},
            "header-x-191": {0x801084EC: 0x250500BF},
            "roomy-color-dropped": {0x80108530: 0x00000000},
            "overflow-check-skipped": {0x80108550: 0x10000004},
            "all-ignores-controls": {0x8010862C: 0x30420000},
            "end-button-square": {0x80108694: 0x24040001},
            "six-categories": {0x80108584: 0x2A220006},
        },
    )
)
