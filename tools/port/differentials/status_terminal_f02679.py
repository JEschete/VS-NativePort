"""F02679 wrapped fixed-point camera, unsigned model fields and callback order."""

from status_terminal_common import configure

globals().update(
    configure(
        2679,
        {
            "distance-signed-halfword": (0x8010618C, 0x84520656),
            "height-signed-halfword": (0x80106190, 0x8450063E),
            "slide-subtraction": (0x80106198, 0x02238821),
            "lookat-scale": (0x801061A8, 0x00051243),
            "height-scale": (0x801061C0, 0x00108280),
            "camera-build-call": (0x80106298, 0),
            "delay-byte-store": (0x801062B8, 0),
            "preview-call": (0x801062BC, 0),
            "background-call": (0x801062C4, 0),
            "fade-call": (0x801062CC, 0),
            "limb-panel-call": (0x801062D4, 0),
            "vitals-call": (0x801062DC, 0),
        },
    )
)
