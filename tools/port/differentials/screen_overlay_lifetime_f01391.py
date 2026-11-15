"""F01391 func_800CB7DC (BATTLE/BATTLE.PRG 5BF94.c:3925), typed Effects::ScreenDistortion::ReleaseScreenOverlay."""

from screen_overlay_lifetime_common import RELEASE, configure

globals().update(
    configure(
        1391,
        RELEASE,
        0x800CB7DC,
        0x800CB83C,
        {
            "retires-on-two": {0x800CB80C: 0x24020002},
            "four-pages": {0x800CB818: 0x24050004},
            "owner-7": {0x800CB7FC: 0x24040007},
        },
    )
)
