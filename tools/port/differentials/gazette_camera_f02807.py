"""F02807 _buildCameraMatrix (MENU9.PRG 238.c:1474), typed Menu::Score::BuildViewerCamera."""

from gazette_camera_common import CAMERA, configure

globals().update(
    configure(
        2807,
        CAMERA,
        0x80104CBC,
        0x80104E90,
        {
            "x-product-shift-11": {0x80104D18: 0x001012C3},
            "z-distance-not-negated": {0x80104D54: 0x00000000},
            "height-offset-not-halved": {0x80104DD0: 0x00000000},
            "yaw-only-matrix": {0x80104E34: 0x00000000},
            "eye-x-not-negated": {0x80104E50: 0x00000000},
            "position-x-shift-11": {0x80104DF0: 0x000212C0},
            "roll-into-yaw-angle": {0x80104E1C: 0xAC670058},
            "x-product-floors": {0x80104D14: 0x00000000},
        },
    )
)