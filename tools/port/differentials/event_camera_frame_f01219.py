"""F01219 vs_battle_applyCameraState (BATTLE/BATTLE.PRG 4A0A8.c:3117), typed Event::ApplyEventCameraState."""

from event_camera_frame_common import APPLY, configure

globals().update(
    configure(
        1219,
        APPLY,
        0x800BBF14,
        0x800BC1CC,
        {
            "world-origin-0x2001": {0x800BBFA8: 0x24022001},
            "shake-gate-0x1A6": {0x800BBFE0: 0x84A201A6},
            "look-at-shake-0x1C6": {0x800BBFF8: 0x24A501C6},
            "keep-roll-changed": {0x800BC0F0: 0xA0400169},
            "backdrop-0x4C6B": {0x800BC1A0: 0x90454C6B},
        },
    )
)
