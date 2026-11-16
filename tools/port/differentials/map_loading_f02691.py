"""F02691 _mapSelectionMenu (MENU5.PRG 4D8.c:979), typed Menu::map::UpdateAreaSelection."""

from map_loading_common import SELECT, configure

globals().update(
    configure(
        2691,
        SELECT,
        0x8010451C,
        0x8010467C,
        {
            "reset-step-kept": {0x8010452C: 0x00000000},
            "reset-selected-kept": {0x80104534: 0x00000000},
            "other-step-result": {0x80104558: 0x2402FFFD},
            "scene-zero-listed": {0x80104560: 0x24060000},
            "scene-ids-not-stored": {0x801045C0: 0x00000000},
            "current-row-ignored": {0x801045E4: 0x00000000},
            "scene-limit": {0x801045FC: 0x2842001F},
            "no-visited-result": {0x8010460C: 0x2402FFFD},
            "current-flag-zero": {0x80104624: 0x24050000},
            "step-not-advanced": {0x80104644: 0x00000000},
            "selection-not-stored": {0x80104658: 0x00000000},
            "pending-result": {0x80104668: 0x2402FFFD},
        },
    )
)
