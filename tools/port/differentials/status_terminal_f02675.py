"""F02675 equipment detail state machine, byte phases and word returns."""

from status_terminal_common import configure

globals().update(
    configure(
        2675,
        {
            "entry-hide": (0x80104FE0, 0),
            "entry-hoist": (0x80104FEC, 0),
            "entry-command": (0x80104FF4, 0),
            "initialize-detail": (0x80105094, 0),
            "weapon-stats": (0x801050C4, 0),
            "enter-weapon-part": (0x801051FC, 0),
            "cycle-change-sound": (0x80105320, 0),
            "cycle-information": (0x801054E8, 0),
            "navigation-selected": (0x80105674, 0),
            "cursor-byte-store": (0x801058E0, 0),
            "triangle-word-return": (0x801058FC, 0x00001021),
            "cross-command": (0x8010591C, 0),
            "tab-navigation": (0x80105944, 0),
        },
    )
)
