"""F01416 func_800CD3E4: shaped frame outlines, textured faces and shadows."""

from text_box_frames_common import FRAME, configure

globals().update(configure(1416, FRAME, 0x800CD3E4, 0x800CDCBC, {
    "wrong-base-x-mask": {0x800CD4FC: 0x3222007F},
    "wrong-outline-color": {0x800CD7F0: 0x34A53031},
    "wrong-shadow-offset": {0x800CDB58: 0x3C040003},
}))
