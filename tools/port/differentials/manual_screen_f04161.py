"""F04161 vs_menuE_exec (MENUE.PRG 494.c:142), typed Menu::manual::UpdateScreen."""

from manual_screen_common import SCREEN, configure

globals().update(
    configure(
        4161,
        SCREEN,
        0x80102EDC,
        0x801030A4,
        {
            # The step dispatch.
            "first-step-2": {0x80102EF8: 0x2443FFFE},
            "five-steps": {0x80102EFC: 0x2C620005},
            # Open: fade in, wait eight frames, initialize.
            "open-no-fade": {0x80102F24: 0x00000000},
            "open-fade-out": {0x80102F28: 0x24040000},
            "open-delay-7": {0x80102F30: 0x24020007},
            "open-keeps-init": {0x80102F3C: 0x00000000},
            "open-to-wait": {0x80102F40: 0x24020005},
            # Initialize, then hide the field and HUD.
            "init-always": {0x80102F58: 0x00000000},
            "init-not-called": {0x80102F60: 0x00000000},
            "init-not-kept": {0x80102F68: 0x00000000},
            "delay-increments": {0x80102F7C: 0x24420001},
            "opens-uninitialized": {0x80102F90: 0x00000000},
            "field-left-on": {0x80102F9C: 0x24040001},
            "hud-shown": {0x80102FA4: 0x24040000},
            "slide-0x100000": {0x80102FAC: 0x3C030010},
            "initialize-to-show": {0x80102FB4: 0x24020006},
            # Wait for the menu rows.
            "ready-subtracted": {0x80102FD0: 0x00621823},
            # Show the help pages, then restore the HUD and menu.
            "show-no-pages": {0x80102FDC: 0x00000000},
            "show-closes-on-zero": {0x80102FE8: 0x14400028},
            "close-layer-3": {0x80102FF0: 0x2404FFFD},
            "close-brightness-127": {0x80102FF8: 0x2405007F},
            "close-field-off": {0x80103004: 0x24040000},
            "close-slide-kept": {0x80103010: 0x00000000},
            "close-command-1": {0x80103018: 0x24040001},
            "close-clear-39": {0x80103020: 0x24040027},
            "close-to-exit": {0x80103024: 0x24020008},
            # Shut down: fade out.
            "shutdown-fade-in": {0x80103034: 0x24040001},
            "shutdown-to-closed": {0x80103038: 0x24020000},
            # Exit once the fade is done.
            "exit-ignores-fade": {0x80103050: 0x00000000},
            "exit-game-on-1": {0x80103060: 0x24020001},
            "exit-always-game": {0x80103064: 0x00000000},
            "exit-menu-11": {0x80103070: 0x2402000B},
            "exit-keeps-hidden": {0x80103080: 0x00000000},
            "exit-keeps-step": {0x80103088: 0x00000000},
        },
    )
)
