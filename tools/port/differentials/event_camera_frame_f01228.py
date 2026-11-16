"""F01228 func_800BD610 (BATTLE/BATTLE.PRG 4A0A8.c:3663), typed Event::EventCameraCompletionMask."""

from event_camera_frame_common import MASK, configure

globals().update(
    configure(
        1228,
        MASK,
        0x800BD610,
        0x800BD6C4,
        {
            "look-at-mode-0xE": {0x800BD624: 0x3842000E},
            "roll-bit-8": {0x800BD64C: 0x34840008},
            "warp-duration-0x278": {0x800BD68C: 0x84A20278},
            "shake-subu": {0x800BD6AC: 0x00431023},
            "mask-0x7F": {0x800BD6C0: 0x3082007F},
        },
    )
)
