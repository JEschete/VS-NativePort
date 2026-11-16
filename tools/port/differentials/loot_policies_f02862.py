"""F02862 _discardItems (MENUB.PRG 260.c:1377), typed Menu::Loot::InitLeaveConfirmation and UpdateLeaveConfirmation."""

from loot_policies_common import LEAVE, configure

globals().update(
    configure(
        2862,
        LEAVE,
        0x801090A4,
        0x80109444,
        {
            "empty-list-state-5": {0x801090D8: 0x24020005},
            "entry-ignored": {0x80109104: 0x10000003},
            "key-item-at-459": {0x8010917C: 0x2C4201CB},
            "yes-row-y-93": {0x801091F8: 0x24060093},
            "claimed-message-swapped": {0x80109284: 0x10400002},
            "discard-on-cancel": {0x80109350: 0x30420000},
            "toggle-on-down-only": {0x8010938C: 0x30421000},
            "cursor-base-9": {0x801093BC: 0x24A50009},
            "result-base-2": {0x801093D4: 0x2442FFFE},
        },
    )
)
