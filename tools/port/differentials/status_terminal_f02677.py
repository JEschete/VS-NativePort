"""F02677 exact left model-slide gradient and framebuffer selection."""

from status_terminal_common import configure

globals().update(
    configure(
        2677,
        {
            "fade-enable-branch": (0x80105EAC, 0x1460002A),
            "gradient-color": (0x80105EBC, 0x34E78081),
            "framebuffer-uv": (0x80105EC4, 0x34C60061),
            "draw-mode-store": (0x80105ED0, 0),
            "gradient-command": (0x80105ED4, 0x3C023D00),
            "top-left-store": (0x80105EE0, 0xACA2000C),
        },
    )
)
