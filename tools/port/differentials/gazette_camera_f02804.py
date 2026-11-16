"""F02804 _restoreCamera (MENU9.PRG 238.c:1402), typed Menu::Score::RestoreCamera."""

from gazette_camera_common import RESTORE, configure

globals().update(
    configure(
        2804,
        RESTORE,
        0x80104AF8,
        0x80104B40,
        {
            "position-pad-not-copied": {0x80104B14: 0x2406000C},
            "position-into-lookat": {0x80104B00: 0x34840044},
            "angles-shifted-one-word": {0x80104B1C: 0x34840058},
            "angles-from-position-backup": {0x80104B24: 0x24A5A480},
        },
        vectors=128,
    )
)