"""F02676 equipment list construction, animation, selection and nested details."""

from status_terminal_common import configure

globals().update(
    configure(
        2676,
        {
            "weapon-format": (0x80105A98, 0),
            "shield-format": (0x80105ABC, 0),
            "armor-format": (0x80105B60, 0),
            "accessory-format": (0x80105B9C, 0),
            "create-list": (0x80105C04, 0),
            "selected-row": (0x80105C88, 0),
            "comparison-stats": (0x80105CA0, 0),
            "clear-for-selection": (0x80105CA8, 0),
            "begin-vitals": (0x80105D90, 0),
            "remember-cursor": (0x80105DA0, 0),
            "return-row-target": (0x80105E0C, 0x240200B5),
            "cancel-detail-return": (0x80105E48, 0x2402FFFF),
            "returned-selected-store": (0x80105E50, 0),
        },
    )
)
