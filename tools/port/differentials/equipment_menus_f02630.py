"""F02630 _equipAccessoryMenu (MENU3.PRG 16C.c:343), typed Menu::Inventory::UpdateAccessoryMenu."""

from equipment_menus_common import ACCESSORY, configure

globals().update(
    configure(
        2630,
        ACCESSORY,
        0x80106C94,
        0x80107148,
        {
            "entry-limb-6": {0x80106CDC: 0x24040006},
            "count-category-3": {0x80106DB4: 0x24040003},
            "has-scan-limb-6": {0x80106DCC: 0x24050006},
            "none-index-19": {0x80106E5C: 0x24030026},
            "none-row-type-2": {0x80106E88: 0x24020002},
            "equipped-shift-18": {0x80106F04: 0x00101C80},
            "spare-category-6": {0x80106F74: 0x24020006},
            "spare-any-limb": {0x80106F88: 0x00000000},
            "list-shift-18": {0x80106FC4: 0x00101C80},
            "list-state-2": {0x80106FF8: 0x24020002},
            "silent-choices": {0x80107018: 0x24040000},
            "clear-row-39": {0x80107034: 0x24040027},
            "fly-out-row-0": {0x8010703C: 0x24040000},
            "pages-hide-7": {0x80107044: 0x24040007},
            "panel-close-1": {0x8010704C: 0x24040001},
            "back-state-2": {0x80107060: 0x24020002},
            "old-limb-kept": {0x80107098: 0x00000000},
            "worn-limb-6": {0x801070CC: 0x24020006},
            "load-state-3": {0x801070E0: 0x24020003},
            "idle-ignored": {0x801070FC: 0x00000000},
        },
    )
)
