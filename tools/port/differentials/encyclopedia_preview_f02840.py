"""F02840 vs_menu9_parseZudFile (MENU9.PRG 592C.c:203), typed Menu::Score::LoadEnemyModelParts."""

from encyclopedia_preview_common import PARTS, configure

globals().update(
    configure(
        2840,
        PARTS,
        0x8010823C,
        0x801084E4,
        {
            "loaded-state-three": {0x80108268: 0x24020003},
            "palette-two-bits": {0x801082C4: 0x30420003},
            "companion-offset-0x1800": {0x80108320: 0x24631800},
            "shield-offset-0x1EE0": {0x801083CC: 0x24421EE0},
            "common-model-carried": {0x8010846C: 0x00000000},
            "freed-while-queued": {0x801084A8: 0x10400006},
        },
    )
)
