"""F02635 _equipMenu (MENU3.PRG 16C.c:1054), typed Menu::Inventory::UpdateEquipmentMenu."""

from equipment_menus_common import TOP, configure

globals().update(
    configure(
        2635,
        TOP,
        0x80108088,
        0x80108518,
        {
            "entry-clear-row-39": {0x801080AC: 0x24040027},
            "entry-icon-2": {0x801080B8: 0x24040002},
            "title-x-319": {0x80108140: 0x2405013F},
            "title-state-1": {0x80108168: 0x24020001},
            "title-target-181": {0x80108170: 0x240200B5},
            "none-index-19": {0x801081B8: 0x24040026},
            "accessory-row-6": {0x80108258: 0x24140006},
            "armor-limb-offset-2": {0x80108284: 0x24420002},
            "types-shift-8": {0x80108300: 0x00021200},
            "row-info-9135": {0x80108320: 0x34A59135},
            "square-mask-40": {0x8010834C: 0x30420040},
            "quit-icon-kept": {0x8010838C: 0x00000000},
            "transition-row-9": {0x8010839C: 0x24440009},
            "sub-state-5": {0x801083A4: 0x24020005},
            "quit-waits": {0x801083BC: 0x00000000},
            "render-state-2": {0x801083F8: 0x24020002},
            "continue-row-10": {0x80108404: 0x2484000A},
            "quit-state-0": {0x80108458: 0x24020000},
            "icon-on-top-1": {0x80108478: 0x24020001},
            "hint-width-139": {0x801084B8: 0x2407008B},
            "hint-index-13": {0x801084C4: 0x2442000D},
            "hint-not-selected": {0x801084F0: 0x00000000},
        },
    )
)