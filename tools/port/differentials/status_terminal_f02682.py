"""F02682 status module lifecycle, navigation, subcontrollers and exact exit."""

from status_terminal_common import configure

globals().update(
    configure(
        2682,
        {
            "initial-item-load": (0x80106558, 0),
            "field-suspend": (0x80106578, 0),
            "background-priority": (0x80106584, 0),
            "initial-projection": (0x801065EC, 0),
            "information-initialize": (0x801066B8, 0),
            "inspect-cursor": (0x80106B70, 0),
            "effects-leave-sound": (0x80106DD0, 0),
            "defense-sound": (0x801070A0, 0),
            "defense-information": (0x801070DC, 0),
            "restore-projection": (0x80107188, 0),
            "release-preview": (0x8010723C, 0),
            "menu-unload": (0x801072B4, 0),
            "battle-unload": (0x80107300, 0),
            "frame-call": (0x8010734C, 0),
            "prompt-animation-store": (0x80107390, 0),
            "prompt-background": (0x801073CC, 0),
            "prompt-label": (0x80107414, 0),
        },
    )
)
