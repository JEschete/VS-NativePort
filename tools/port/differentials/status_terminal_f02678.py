"""F02678 exact three-packet background and ordering-head choice."""

from status_terminal_common import configure

globals().update(
    configure(
        2678,
        {
            "gradient-bottom-y": (0x80105FCC, 0),
            "chain-address-mask-low": (0x80105FA0, 0),
            "gradient-tag-word-count": (0x80106068, 0),
            "framebuffer-index-load": (0x801060D4, 0),
            "last-xy-store": (0x80106124, 0),
            "final-chain-store": (0x8010612C, 0),
            "final-cursor-size": (0x80106130, 0x27020034),
        },
    )
)
