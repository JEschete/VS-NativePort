"""F01222 func_800BC9E0 (BATTLE/BATTLE.PRG 4A0A8.c:3371), typed Event::UpdateEventCameraTracks."""

from event_camera_frame_common import UPDATE, configure

globals().update(
    configure(
        1222,
        UPDATE,
        0x800BC9E0,
        0x800BCA8C,
        {
            "eye-partner-x-0x15C": {0x800BCA10: 0xAC82015C},
            "look-at-partner-x-0xA8": {0x800BCA1C: 0xAC8700A8},
            "camera-roll-0x174": {0x800BCA40: 0x24840174},
            "far-clip-0x18C": {0x800BCA70: 0x2484018C},
        },
    )
)
