"""F01390 func_800CB79C (BATTLE/BATTLE.PRG 5BF94.c:3916), typed Effects::ScreenDistortion::AcquireScreenOverlay."""

from screen_overlay_lifetime_common import ACQUIRE, configure

globals().update(
    configure(
        1390,
        ACQUIRE,
        0x800CB79C,
        0x800CB7DC,
        {
            "owner-7": {0x800CB7C0: 0x24040007},
            "polls-instead": {0x800CB7C8: 0x24040000},
            "acquires-when-attached": {0x800CB7A8: 0x10400008},
        },
    )
)
