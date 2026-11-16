"""F02806 _topMenu (MENU9.PRG 238.c:1430), typed Menu::Score::UpdateTopMenu."""

from gazette_rows_common import TOP_MENU, configure

globals().update(
    configure(
        2806,
        TOP_MENU,
        0x80104BD0,
        0x80104CBC,
        {
            "three-choices": {0x80104C50: 0x28E20003},
            "step-kept": {0x80104C7C: 0x24420000},
            "selected-not-stored": {0x80104C98: 0x00000000},
            "initialize-keeps-step": {0x80104BE0: 0x00000000},
            "text-offset-words": {0x80104C40: 0x00021080},
            "pending-returns-0": {0x80104CA8: 0x24020000},
        },
    )
)
